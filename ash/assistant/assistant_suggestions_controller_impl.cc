// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_suggestions_controller_impl.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/assistant/util/resource_util.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_suggestion.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

using assistant::AssistantSuggestion;
using assistant::AssistantSuggestionType;
using assistant::prefs::AssistantOnboardingMode;

// Conversation starters -------------------------------------------------------

constexpr int kMaxNumOfConversationStarters = 3;

}  // namespace

// AssistantSuggestionsControllerImpl ------------------------------------------

AssistantSuggestionsControllerImpl::AssistantSuggestionsControllerImpl() {
  UpdateConversationStarters();
  assistant_controller_observation_.Observe(AssistantController::Get());
}

AssistantSuggestionsControllerImpl::~AssistantSuggestionsControllerImpl() =
    default;

const AssistantSuggestionsModel* AssistantSuggestionsControllerImpl::GetModel()
    const {
  return &model_;
}

void AssistantSuggestionsControllerImpl::OnAssistantControllerConstructed() {
  AssistantUiController::Get()->GetModel()->AddObserver(this);
  AssistantState::Get()->AddObserver(this);
}

void AssistantSuggestionsControllerImpl::OnAssistantControllerDestroying() {
  AssistantState::Get()->RemoveObserver(this);
  AssistantUiController::Get()->GetModel()->RemoveObserver(this);
}

void AssistantSuggestionsControllerImpl::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    std::optional<AssistantEntryPoint> entry_point,
    std::optional<AssistantExitPoint> exit_point) {
  // When Assistant is finishing a session, we update our cache of conversation
  // starters so that they're fresh for the next launch.
  if (assistant::util::IsFinishingSession(new_visibility))
    UpdateConversationStarters();
}

void AssistantSuggestionsControllerImpl::OnAssistantContextEnabled(
    bool enabled) {
  // We currently assume that the context setting is not being modified while
  // Assistant UI is visible.
  DCHECK_NE(AssistantVisibility::kVisible,
            AssistantUiController::Get()->GetModel()->visibility());
  UpdateConversationStarters();
}

void AssistantSuggestionsControllerImpl::OnAssistantOnboardingModeChanged(
    AssistantOnboardingMode onboarding_mode) {
  UpdateOnboardingSuggestions();
}

void AssistantSuggestionsControllerImpl::UpdateConversationStarters() {
  std::vector<AssistantSuggestion> conversation_starters;

  // Adds a conversation starter for the given |message_id| and |action_url|.
  auto AddConversationStarter = [&conversation_starters](
                                    int message_id, GURL action_url = GURL()) {
    AssistantSuggestion starter;
    starter.id = base::UnguessableToken::Create();
    starter.type = AssistantSuggestionType::kConversationStarter;
    starter.text = l10n_util::GetStringUTF8(message_id);
    starter.action_url = action_url;
    conversation_starters.push_back(std::move(starter));
  };

  // Always show the "What can you do?" conversation starter.
  AddConversationStarter(IDS_ASH_ASSISTANT_CHIP_WHAT_CAN_YOU_DO);

  // The rest of the conversation starters will be shuffled...
  std::vector<int> shuffled_message_ids;

  shuffled_message_ids.push_back(IDS_ASH_ASSISTANT_CHIP_IM_BORED);
  shuffled_message_ids.push_back(IDS_ASH_ASSISTANT_CHIP_OPEN_FILES);
  shuffled_message_ids.push_back(IDS_ASH_ASSISTANT_CHIP_PLAY_MUSIC);
  shuffled_message_ids.push_back(IDS_ASH_ASSISTANT_CHIP_SEND_AN_EMAIL);
  shuffled_message_ids.push_back(IDS_ASH_ASSISTANT_CHIP_SET_A_REMINDER);
  shuffled_message_ids.push_back(IDS_ASH_ASSISTANT_CHIP_TELL_ME_A_JOKE);
  shuffled_message_ids.push_back(IDS_ASH_ASSISTANT_CHIP_WHATS_ON_MY_CALENDAR);
  shuffled_message_ids.push_back(IDS_ASH_ASSISTANT_CHIP_WHATS_THE_WEATHER);

  base::RandomShuffle(shuffled_message_ids.begin(), shuffled_message_ids.end());

  // ...and added until we have no more than |kMaxNumOfConversationStarters|.
  for (int i = 0;
       conversation_starters.size() < kMaxNumOfConversationStarters &&
       i < static_cast<int>(shuffled_message_ids.size());
       ++i) {
    AddConversationStarter(shuffled_message_ids[i]);
  }

  model_.SetConversationStarters(std::move(conversation_starters));
}

void AssistantSuggestionsControllerImpl::UpdateOnboardingSuggestions() {
  auto CreateIconResourceLink = [](int message_id) {
    switch (message_id) {
      case IDS_ASH_ASSISTANT_ONBOARDING_SUGGESTION_CONVERSION:
        return assistant::util::CreateIconResourceLink(
            assistant::util::IconName::kConversionPath);
      case IDS_ASH_ASSISTANT_ONBOARDING_SUGGESTION_KNOWLEDGE:
        return assistant::util::CreateIconResourceLink(
            assistant::util::IconName::kPersonPinCircle);
      case IDS_ASH_ASSISTANT_ONBOARDING_SUGGESTION_KNOWLEDGE_EDU:
        return assistant::util::CreateIconResourceLink(
            assistant::util::IconName::kStraighten);
      case IDS_ASH_ASSISTANT_ONBOARDING_SUGGESTION_LANGUAGE:
        return assistant::util::CreateIconResourceLink(
            assistant::util::IconName::kTranslate);
      case IDS_ASH_ASSISTANT_ONBOARDING_SUGGESTION_MATH:
        return assistant::util::CreateIconResourceLink(
            assistant::util::IconName::kCalculate);
      case IDS_ASH_ASSISTANT_ONBOARDING_SUGGESTION_PERSONALITY:
        return assistant::util::CreateIconResourceLink(
            assistant::util::IconName::kSentimentVerySatisfied);
      case IDS_ASH_ASSISTANT_ONBOARDING_SUGGESTION_PRODUCTIVITY:
        return assistant::util::CreateIconResourceLink(
            assistant::util::IconName::kTimer);
      case IDS_ASH_ASSISTANT_ONBOARDING_SUGGESTION_TECHNICAL:
        return assistant::util::CreateIconResourceLink(
            assistant::util::IconName::kScreenshot);
      default:
        NOTREACHED();
    }
  };

  std::vector<AssistantSuggestion> onboarding_suggestions;

  using assistant::AssistantBetterOnboardingType;
  auto AddSuggestion = [&CreateIconResourceLink, &onboarding_suggestions](
                           int message_id, AssistantBetterOnboardingType type) {
    onboarding_suggestions.emplace_back();
    auto& suggestion = onboarding_suggestions.back();
    suggestion.id = base::UnguessableToken::Create();
    suggestion.type = AssistantSuggestionType::kBetterOnboarding;
    suggestion.better_onboarding_type = type;
    suggestion.text = l10n_util::GetStringUTF8(message_id);
    suggestion.icon_url = CreateIconResourceLink(message_id);
    suggestion.action_url = GURL();
  };

  switch (AssistantState::Get()->onboarding_mode().value_or(
      AssistantOnboardingMode::kDefault)) {
    case AssistantOnboardingMode::kEducation:
      AddSuggestion(IDS_ASH_ASSISTANT_ONBOARDING_SUGGESTION_MATH,
                    AssistantBetterOnboardingType::kMath);
      AddSuggestion(IDS_ASH_ASSISTANT_ONBOARDING_SUGGESTION_KNOWLEDGE_EDU,
                    AssistantBetterOnboardingType::kKnowledgeEdu);
      break;
    case AssistantOnboardingMode::kDefault:
      AddSuggestion(IDS_ASH_ASSISTANT_ONBOARDING_SUGGESTION_CONVERSION,
                    AssistantBetterOnboardingType::kConversion);
      AddSuggestion(IDS_ASH_ASSISTANT_ONBOARDING_SUGGESTION_KNOWLEDGE,
                    AssistantBetterOnboardingType::kKnowledge);
      break;
  }

  AddSuggestion(IDS_ASH_ASSISTANT_ONBOARDING_SUGGESTION_PRODUCTIVITY,
                AssistantBetterOnboardingType::kProductivity);
  AddSuggestion(IDS_ASH_ASSISTANT_ONBOARDING_SUGGESTION_PERSONALITY,
                AssistantBetterOnboardingType::kPersonality);
  AddSuggestion(IDS_ASH_ASSISTANT_ONBOARDING_SUGGESTION_LANGUAGE,
                AssistantBetterOnboardingType::kLanguage);
  AddSuggestion(IDS_ASH_ASSISTANT_ONBOARDING_SUGGESTION_TECHNICAL,
                AssistantBetterOnboardingType::kTechnical);

  model_.SetOnboardingSuggestions(std::move(onboarding_suggestions));
}

}  // namespace ash

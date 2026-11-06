// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/aba_search_journey_cue_target.h"

#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"
#include "chrome/browser/contextual_tasks/search_journey_tab_feature.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"

namespace contextual_tasks {

namespace {

constexpr int kIconSize = 16;

}  // namespace

// static
void AbaSearchJourneyCueTarget::Register(tabs::TabInterface& tab) {
  auto* controller = contextual_cueing::ContextualCueingController::From(&tab);
  CHECK(controller);
  controller->RegisterCueTarget(
      contextual_cueing::CueTargetType::kAbaSearchJourney,
      std::make_unique<AbaSearchJourneyCueTarget>(tab));
}

AbaSearchJourneyCueTarget::AbaSearchJourneyCueTarget(tabs::TabInterface& tab)
    : tab_(tab),
      did_activate_subscription_(tab.RegisterDidActivate(
          base::BindRepeating(&AbaSearchJourneyCueTarget::OnTabDidActivate,
                              base::Unretained(this)))) {}

AbaSearchJourneyCueTarget::~AbaSearchJourneyCueTarget() = default;

contextual_cueing::CueTargetType AbaSearchJourneyCueTarget::GetType() const {
  return contextual_cueing::CueTargetType::kAbaSearchJourney;
}

bool AbaSearchJourneyCueTarget::RequiresModelExecution() const {
  return false;
}

bool AbaSearchJourneyCueTarget::IsEligible() const {
  SearchJourneyTabFeature* feature = SearchJourneyTabFeature::From(&tab_.get());
  return feature && feature->ShouldShowCue();
}

void AbaSearchJourneyCueTarget::CheckEligibility(
    base::WeakPtr<content::WebContents> web_contents,
    contextual_cueing::CueIntrusiveness intrusiveness,
    EligibilityCallback callback) {
  const bool eligible = web_contents && IsEligible();
  ContentGenerator generator;
  if (eligible) {
    generator = base::BindOnce(&AbaSearchJourneyCueTarget::GenerateContent,
                               weak_ptr_factory_.GetWeakPtr());
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), eligible, std::move(generator)));
}

bool AbaSearchJourneyCueTarget::IsPageEligible(
    const page_content_annotations::PageContentAnnotationsResult& result,
    content::WebContents* active_web_contents) const {
  // This target doesn't use PageContentAnnotationsResult.
  return false;
}

void AbaSearchJourneyCueTarget::OnChipShown() {
  OnCueShown(/*is_anchored_message=*/false);
}

void AbaSearchJourneyCueTarget::OnAnchoredMessageShown(
    page_actions::PageActionPriorityCategory priority) {
  OnCueShown(/*is_anchored_message=*/true);
}

void AbaSearchJourneyCueTarget::OnAnchoredMessageClicked(
    contextual_cueing::CueActionData data) {
  if (SearchJourneyTabFeature* feature =
          SearchJourneyTabFeature::From(&tab_.get())) {
    feature->RestoreJourneyInSidePanel();
  }
}

void AbaSearchJourneyCueTarget::OnEditPrompt(
    contextual_cueing::CueActionData data) {
  NOTREACHED();
}

ui::ImageModel AbaSearchJourneyCueTarget::GetAnchoredMessageIcon() const {
  return ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                            ? omnibox::kSearchSparkIcon
                                            : omnibox::kSearchSparkOldIcon,
                                        ui::kColorSysOnSurface, kIconSize);
}

ui::ImageModel AbaSearchJourneyCueTarget::GetOmniboxChipIcon() const {
  // `kDockToLeftOldIcon` is the old version of `kDockToRightIcon`; their names
  // are mirrored.
  return ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? kDockToRightIcon : kDockToLeftOldIcon,
      ui::kColorSysOnSurface, kIconSize);
}

contextual_cueing::CueActionData
AbaSearchJourneyCueTarget::CueActionDataFromResponse(
    const optimization_guide::proto::ContextualCue& cue,
    std::vector<tabs::TabHandle> tabs_to_show) const {
  return std::monostate();
}

optimization_guide::proto::ContextualCueingSurface
AbaSearchJourneyCueTarget::GetSurface() const {
  return optimization_guide::proto::CONTEXTUAL_CUEING_SURFACE_UNSPECIFIED;
}

void AbaSearchJourneyCueTarget::OnTabDidActivate(tabs::TabInterface* tab) {
  // Cues are only evaluated when a page loads, so evaluate this one when the
  // user returns to Tab A.
  if (!IsEligible()) {
    return;
  }
  auto* controller =
      contextual_cueing::ContextualCueingController::From(&tab_.get());
  CHECK(controller);
  controller->EvaluateCueForTarget(GetType());
}

void AbaSearchJourneyCueTarget::OnCueShown(bool is_anchored_message) {
  if (SearchJourneyTabFeature* feature =
          SearchJourneyTabFeature::From(&tab_.get())) {
    feature->OnCueShown(is_anchored_message);
  }
}

void AbaSearchJourneyCueTarget::GenerateContent(GenerateCallback callback) {
  // The journey may have expired since the eligibility check.
  if (!IsEligible()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  optimization_guide::proto::ContextualCue cue;
  cue.set_suggested_cuj(contextual_cueing::GetName(GetType()));
  auto* anchored_message_cue = cue.mutable_anchored_message_cue();
  anchored_message_cue->set_action_text(
      l10n_util::GetStringUTF8(IDS_CONTEXTUAL_TASKS_SEARCH_JOURNEY_CUE_ACTION));
  anchored_message_cue->set_anchored_message_text(l10n_util::GetStringUTF8(
      IDS_CONTEXTUAL_TASKS_SEARCH_JOURNEY_CUE_ANCHORED_MESSAGE));
  std::move(callback).Run(std::move(cue));
}

}  // namespace contextual_tasks

// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/grammar_manager.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/input_method/assistive_window_properties.h"
#include "chrome/browser/chromeos/input_method/ui/suggestion_details.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/ime_input_context_handler_interface.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace chromeos {
namespace {

using text_utils::FindCurrentSentence;
using text_utils::FindLastSentence;
using text_utils::Sentence;

constexpr base::TimeDelta kCheckDelay = base::TimeDelta::FromMilliseconds(500);

void RecordGrammarAction(GrammarActions action) {
  base::UmaHistogramEnumeration("InputMethod.Assistive.Grammar.Actions",
                                action);
}

bool IsValidSentence(const std::u16string& text, const Sentence& sentence) {
  uint32_t start = sentence.original_range.start();
  uint32_t end = sentence.original_range.end();
  if (start >= text.size() || end > text.size())
    return false;

  return FindCurrentSentence(text, start) == sentence;
}

}  // namespace

GrammarManager::GrammarManager(
    Profile* profile,
    std::unique_ptr<GrammarServiceClient> grammar_client,
    SuggestionHandlerInterface* suggestion_handler)
    : profile_(profile),
      grammar_client_(std::move(grammar_client)),
      suggestion_handler_(suggestion_handler),
      current_fragment_(gfx::Range(), std::string()),
      suggestion_button_(ui::ime::AssistiveWindowButton{
          .id = ui::ime::ButtonId::kSuggestion,
          .window_type = ui::ime::AssistiveWindowType::kGrammarSuggestion,
      }),
      ignore_button_(ui::ime::AssistiveWindowButton{
          .id = ui::ime::ButtonId::kIgnoreSuggestion,
          .window_type = ui::ime::AssistiveWindowType::kGrammarSuggestion,
      }) {}

GrammarManager::~GrammarManager() = default;

bool GrammarManager::IsOnDeviceGrammarEnabled() {
  return base::FeatureList::IsEnabled(
      chromeos::features::kOnDeviceGrammarCheck);
}

void GrammarManager::OnFocus(int context_id) {
  if (context_id != context_id_) {
    last_text_ = u"";
    last_sentence_ = Sentence();
  }
  context_id_ = context_id;
}

bool GrammarManager::OnKeyEvent(const ui::KeyEvent& event) {
  if (!suggestion_shown_ || event.type() != ui::ET_KEY_PRESSED)
    return false;

  if (event.code() == ui::DomCode::ESCAPE) {
    DismissSuggestion();
    return true;
  }
  if (event.code() == ui::DomCode::TAB) {
    if (highlighted_button_ == ui::ime::ButtonId::kSuggestion) {
      highlighted_button_ = ui::ime::ButtonId::kIgnoreSuggestion;
      SetButtonHighlighted(ignore_button_);
    } else {
      highlighted_button_ = ui::ime::ButtonId::kSuggestion;
      SetButtonHighlighted(suggestion_button_);
    }
    return true;
  }
  if (event.code() == ui::DomCode::ENTER) {
    switch (highlighted_button_) {
      case ui::ime::ButtonId::kSuggestion:
        AcceptSuggestion();
        return true;
      case ui::ime::ButtonId::kIgnoreSuggestion:
        IgnoreSuggestion();
        return true;
      default:
        break;
    }
  }
  return false;
}

void GrammarManager::OnSurroundingTextChanged(const std::u16string& text,
                                              int cursor_pos,
                                              int anchor_pos) {
  if (suggestion_shown_)
    DismissSuggestion();

  if (text != last_text_) {
    last_text_ = text;

    // Grammar check is cpu consuming, so we only send request to ml service
    // when the user has finished a sentence or stopped typing for some time.
    Sentence last_sentence = FindLastSentence(text, cursor_pos);
    if (last_sentence_ != last_sentence) {
      last_sentence_ = last_sentence;
      Check(last_sentence);
    }

    delay_timer_.Start(
        FROM_HERE, kCheckDelay,
        base::BindOnce(&GrammarManager::Check, base::Unretained(this),
                       FindCurrentSentence(text, cursor_pos)));
    return;
  }

  // Do not show the suggestion when the user is selecting a range of text, so
  // that we will not show conflict with the system copy/paste popup.
  if (cursor_pos != anchor_pos)
    return;

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return;

  absl::optional<ui::GrammarFragment> grammar_fragment_opt =
      input_context->GetGrammarFragment(gfx::Range(cursor_pos));

  if (grammar_fragment_opt) {
    if (current_fragment_ != grammar_fragment_opt.value()) {
      current_fragment_ = grammar_fragment_opt.value();
      RecordGrammarAction(GrammarActions::kWindowShown);
    }
    std::string error;
    AssistiveWindowProperties properties;
    properties.type = ui::ime::AssistiveWindowType::kGrammarSuggestion;
    properties.candidates = {base::UTF8ToUTF16(current_fragment_.suggestion)};
    properties.visible = true;
    suggestion_handler_->SetAssistiveWindowProperties(context_id_, properties,
                                                      &error);
    if (!error.empty()) {
      LOG(ERROR) << "Fail to show suggestion. " << error;
    }
    highlighted_button_ = ui::ime::ButtonId::kNone;
    suggestion_shown_ = true;
  }
}

void GrammarManager::Check(const Sentence& sentence) {
  if (!IsValidSentence(last_text_, sentence))
    return;

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return;

  input_context->ClearGrammarFragments(sentence.original_range);

  grammar_client_->RequestTextCheck(
      profile_, sentence.text,
      base::BindOnce(&GrammarManager::OnGrammarCheckDone,
                     base::Unretained(this), sentence));
}

void GrammarManager::OnGrammarCheckDone(
    const Sentence& sentence,
    bool success,
    const std::vector<ui::GrammarFragment>& results) const {
  if (!success || !IsValidSentence(last_text_, sentence) || results.empty())
    return;

  std::vector<ui::GrammarFragment> corrected_results;
  for (const ui::GrammarFragment& fragment : results) {
    corrected_results.emplace_back(
        gfx::Range(fragment.range.start() + sentence.original_range.start(),
                   fragment.range.end() + sentence.original_range.start()),
        fragment.suggestion);
  }

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return;

  input_context->AddGrammarFragments(corrected_results);
  RecordGrammarAction(GrammarActions::kUnderlined);
}

void GrammarManager::DismissSuggestion() {
  std::string error;
  suggestion_handler_->DismissSuggestion(context_id_, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Failed to dismiss suggestion. " << error;
    return;
  }
  suggestion_shown_ = false;
}

void GrammarManager::AcceptSuggestion() {
  if (!suggestion_shown_)
    return;

  DismissSuggestion();

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context) {
    LOG(ERROR) << "Failed to commit grammar suggestion.";
  }

  // NOTE: GetSurroundingTextInfo() could return a stale cache that no
  // longer reflects reality, due to async-ness between IMF and
  // TextInputClient.
  // TODO(crbug/1194424): Work around the issue or fix
  // GetSurroundingTextInfo().
  const ui::SurroundingTextInfo surrounding_text =
      input_context->GetSurroundingTextInfo();

  // Delete the incorrect grammar fragment.
  input_context->DeleteSurroundingText(
      -static_cast<int>(surrounding_text.selection_range.start() -
                        current_fragment_.range.start()),
      current_fragment_.range.length() -
          surrounding_text.selection_range.length());
  // Insert the suggestion and put cursor after it.
  input_context->CommitText(
      base::UTF8ToUTF16(current_fragment_.suggestion),
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);

  RecordGrammarAction(GrammarActions::kAccepted);
}

void GrammarManager::IgnoreSuggestion() {
  if (!suggestion_shown_)
    return;

  DismissSuggestion();

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return;

  input_context->ClearGrammarFragments(current_fragment_.range);

  RecordGrammarAction(GrammarActions::kIgnored);
}

void GrammarManager::SetButtonHighlighted(
    const ui::ime::AssistiveWindowButton& button) {
  std::string error;
  suggestion_handler_->SetButtonHighlighted(context_id_, button, true, &error);
  if (!error.empty()) {
    LOG(ERROR) << "Failed to set button highlighted. " << error;
  }
}

}  // namespace chromeos

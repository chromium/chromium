// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/grammar_manager.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/input_method/ui/suggestion_details.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/ime_input_context_handler_interface.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace chromeos {
namespace {

constexpr base::TimeDelta kCheckDelay = base::TimeDelta::FromMilliseconds(500);

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
          .index = 0,
      }) {}

GrammarManager::~GrammarManager() = default;

bool GrammarManager::IsOnDeviceGrammarEnabled() {
  return base::FeatureList::IsEnabled(
      chromeos::features::kOnDeviceGrammarCheck);
}

void GrammarManager::OnFocus(int context_id) {
  if (context_id != context_id_) {
    last_text_ = u"";
  }
  context_id_ = context_id;
}

bool GrammarManager::OnKeyEvent(const ui::KeyEvent& event) {
  if (!suggestion_shown_)
    return false;

  if (event.code() == ui::DomCode::ESCAPE) {
    DismissSuggestion();
    return true;
  }
  if (event.code() == ui::DomCode::TAB) {
    if (!suggestion_highlighted_) {
      suggestion_highlighted_ = true;
      SetButtonHighlighted(suggestion_button_);
    }
    return true;
  }
  if (event.code() == ui::DomCode::ENTER && suggestion_highlighted_) {
    AcceptSuggestion();
    return true;
  }
  return false;
}

void GrammarManager::OnSurroundingTextChanged(const std::u16string& text,
                                              int cursor_pos,
                                              int anchor_pos) {
  if (text != last_text_) {
    if (suggestion_shown_) {
      DismissSuggestion();
    }
    // Grammar check is cpu consuming, so we only send request to ml service
    // when the user has stopped typing for some time.
    delay_timer_.Start(
        FROM_HERE, kCheckDelay,
        base::BindOnce(&GrammarManager::Check, base::Unretained(this), text));

    last_text_ = text;
    return;
  }

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return;

  gfx::Range cursor_range = cursor_pos <= anchor_pos
                                ? gfx::Range(cursor_pos, anchor_pos)
                                : gfx::Range(anchor_pos, cursor_pos);
  absl::optional<ui::GrammarFragment> grammar_fragment_opt =
      input_context->GetGrammarFragment(cursor_range);

  if (grammar_fragment_opt) {
    current_fragment_ = grammar_fragment_opt.value();
    std::string error;
    ui::ime::SuggestionDetails details{
        .text = base::UTF8ToUTF16(current_fragment_.suggestion),
        .confirmed_length = 0,
    };
    suggestion_handler_->SetSuggestion(context_id_, details, &error);
    if (!error.empty()) {
      LOG(ERROR) << "Fail to show suggestion. " << error;
    }
    suggestion_highlighted_ = false;
    suggestion_shown_ = true;
  } else if (suggestion_shown_) {
    DismissSuggestion();
  }
}

void GrammarManager::Check(const std::u16string& text) {
  if (text != last_text_) {
    return;
  }

  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return;

  input_context->ClearGrammarFragments(gfx::Range(0, text.size()));

  grammar_client_->RequestTextCheck(
      profile_, text,
      base::BindOnce(&GrammarManager::OnGrammarCheckDone,
                     base::Unretained(this), text));
}

void GrammarManager::OnGrammarCheckDone(
    const std::u16string& text,
    bool success,
    const std::vector<ui::GrammarFragment>& results) const {
  if (!success || text != last_text_ || results.empty())
    return;
  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return;

  input_context->AddGrammarFragments(results);
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

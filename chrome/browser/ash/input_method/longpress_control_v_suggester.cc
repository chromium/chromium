// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/longpress_control_v_suggester.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/input_method/longpress_suggester.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chrome/browser/ash/input_method/suggestion_handler_interface.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/text_input_target.h"
#include "ui/gfx/range/range.h"

namespace ash::input_method {

LongpressControlVSuggester::LongpressControlVSuggester(
    SuggestionHandlerInterface* suggestion_handler)
    : LongpressSuggester(suggestion_handler) {}

LongpressControlVSuggester::~LongpressControlVSuggester() = default;

void LongpressControlVSuggester::CachePastedTextStart() {
  pasted_text_start_.reset();

  TextInputTarget* input_context = IMEBridge::Get()->GetInputContextHandler();
  if (!input_context) {
    return;
  }

  pasted_text_start_ =
      input_context->GetSurroundingTextInfo().selection_range.GetMin();
}

SuggestionStatus LongpressControlVSuggester::HandleKeyEvent(
    const ui::KeyEvent& event) {
  // The clipboard history controller handles the mouse and key events that
  // allow users to select an item to paste.
  return SuggestionStatus::kNotHandled;
}

bool LongpressControlVSuggester::TrySuggestWithSurroundingText(
    const std::u16string& text,
    const gfx::Range selection_range) {
  // Pastes cause the surrounding text to change. Continue "suggesting" after
  // such changes so that `this` remains the current suggester.
  return true;
}

bool LongpressControlVSuggester::AcceptSuggestion(size_t index) {
  if (!focused_context_id_.has_value()) {
    LOG(ERROR) << "suggest: Accepted long-press Ctrl+V suggestion but had no "
                  "context to replace originally pasted content.";
    Reset();
    return true;
  }

  if (auto* input_context = IMEBridge::Get()->GetInputContextHandler();
      input_context != nullptr && pasted_text_start_.has_value()) {
    size_t pasted_text_end =
        input_context->GetSurroundingTextInfo().selection_range.GetMin();
    DCHECK_GE(pasted_text_end, *pasted_text_start_);

    std::string error;
    suggestion_handler_->AcceptSuggestionCandidate(
        *focused_context_id_, /*candidate=*/u"",
        /*delete_previous_utf16_len=*/pasted_text_end - *pasted_text_start_,
        /*use_replace_surrounding_text=*/
        base::FeatureList::IsEnabled(
            features::kDiacriticsUseReplaceSurroundingText),
        &error);
    if (!error.empty()) {
      LOG(ERROR) << "suggest: Accepted long-press Ctrl+V suggestion without "
                    "replacing originally pasted content: "
                 << error;
    }
  } else {
    LOG(ERROR) << "suggest: Accepted long-press Ctrl+V suggestion but could "
                  "not attempt to replace originally pasted content.";
  }

  Reset();
  return true;
}

void LongpressControlVSuggester::DismissSuggestion() {
  Reset();
}

AssistiveType LongpressControlVSuggester::GetProposeActionType() {
  return AssistiveType::kLongpressControlV;
}

void LongpressControlVSuggester::Reset() {
  pasted_text_start_.reset();
}

}  // namespace ash::input_method

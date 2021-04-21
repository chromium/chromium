// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/multi_word_suggester.h"

#include "chromeos/services/ime/public/cpp/suggestions.h"

namespace chromeos {
namespace {

using TextSuggestion = ::chromeos::ime::TextSuggestion;

}  // namespace

MultiWordSuggester::~MultiWordSuggester() = default;

void MultiWordSuggester::OnFocus(int context_id) {}

void MultiWordSuggester::OnBlur() {}

void MultiWordSuggester::OnExternalSuggestionsUpdated(
    const std::vector<TextSuggestion>& suggestions) {
  // TODO(crbug/1146266): Take any suggestions here and display them.
}

SuggestionStatus MultiWordSuggester::HandleKeyEvent(const ui::KeyEvent& event) {
  return SuggestionStatus::kNotHandled;
}

bool MultiWordSuggester::Suggest(const std::u16string& text) {
  return false;
}

bool MultiWordSuggester::AcceptSuggestion(size_t index) {
  return false;
}

void MultiWordSuggester::DismissSuggestion() {}

AssistiveType MultiWordSuggester::GetProposeActionType() {
  return AssistiveType::kMultiWordPrediction;
}

bool MultiWordSuggester::HasSuggestions() {
  return false;
}

std::vector<TextSuggestion> MultiWordSuggester::GetSuggestions() {
  return {};
}

}  // namespace chromeos

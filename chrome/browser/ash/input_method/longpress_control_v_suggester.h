// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_LONGPRESS_CONTROL_V_SUGGESTER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_LONGPRESS_CONTROL_V_SUGGESTER_H_

#include <string>

#include "chrome/browser/ash/input_method/longpress_suggester.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "ui/events/event.h"
#include "ui/gfx/range/range.h"

namespace ash::input_method {

class SuggestionHandlerInterface;

class LongpressControlVSuggester : public LongpressSuggester {
 public:
  explicit LongpressControlVSuggester(
      SuggestionHandlerInterface* suggestion_handler);
  ~LongpressControlVSuggester() override;

  // Suggester overrides:
  SuggestionStatus HandleKeyEvent(const ui::KeyEvent& event) override;
  bool TrySuggestWithSurroundingText(const std::u16string& text,
                                     gfx::Range selection_range) override;
  bool AcceptSuggestion(size_t index) override;
  void DismissSuggestion() override;
  AssistiveType GetProposeActionType() override;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_LONGPRESS_CONTROL_V_SUGGESTER_H_

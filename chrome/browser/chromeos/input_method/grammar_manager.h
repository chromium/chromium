// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_GRAMMAR_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_GRAMMAR_MANAGER_H_

#include <string>

#include "base/timer/timer.h"
#include "ui/events/event.h"

namespace chromeos {

// Sends grammar check requests to ml service and upon receiving the grammar
// suggestions, shows UI to indiciate the suggestions and allows users to
// accept or dismiss the suggestions.
class GrammarManager {
 public:
  GrammarManager();
  GrammarManager(const GrammarManager&) = delete;
  GrammarManager& operator=(const GrammarManager&) = delete;
  ~GrammarManager();

  // Check if the chromium flag for on-device grammar check is enabled.
  bool IsOnDeviceGrammarEnabled();

  // Indicates a new text field is focused, used to save context ID.
  void OnFocus(int context_id);

  // This class intercepts keystrokes when the grammar suggestion pop up is
  // displayed. Returns whether the keypress has been handled.
  bool OnKeyEvent(const ui::KeyEvent& event);

  // Sends grammar check request to ml service or display existing grammar
  // suggestion based on the surrounding text changes and cursor changes.
  void OnSurroundingTextChanged(const std::u16string& text,
                                int cursor_pos,
                                int anchor_pos);

 private:
  void Check(const std::u16string& text);

  int context_id_ = 0;
  std::u16string last_text_;
  base::OneShotTimer delay_timer_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_GRAMMAR_MANAGER_H_

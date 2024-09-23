// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTH_VIEWS_AUTH_TEXTFIELD_TIMER_H_
#define ASH_AUTH_VIEWS_AUTH_TEXTFIELD_TIMER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/auth/views/auth_textfield.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"

namespace ash {

class ASH_EXPORT AuthTextfieldTimer : public AuthTextfield::Observer {
 public:
  void OnContentsChanged(const std::u16string& new_contents) override;
  void OnTextVisibleChanged(bool visible) override;

  explicit AuthTextfieldTimer(AuthTextfield* auth_textfield);
  AuthTextfieldTimer(const AuthTextfieldTimer&) = delete;
  AuthTextfieldTimer& operator=(const AuthTextfieldTimer&) = delete;
  ~AuthTextfieldTimer() override;

 private:
  void ClearTextTimerElapsed();
  void HideTextTimerElapsed();

  raw_ptr<AuthTextfield> auth_textfield_;

  // Clears the text field after a time without action if the display
  // password button is visible.
  base::RetainingOneShotTimer clear_text_timer_;

  // Hides the text after a short delay if the text is shown.
  base::RetainingOneShotTimer hide_text_timer_;
};

}  // namespace ash

#endif  // ASH_AUTH_VIEWS_AUTH_TEXTFIELD_TIMER_H_

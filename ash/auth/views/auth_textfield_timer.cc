// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/auth_textfield_timer.h"

#include <string>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/auth/views/auth_textfield.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/timer/timer.h"

namespace ash {

namespace {

// Delay after which the text gets cleared if nothing has been typed. It is
// only running if the display password button is shown, as there is no
// potential security threat otherwise.
constexpr base::TimeDelta kClearTextAfterDelay = base::Seconds(30);

// Delay after which the password gets back to hidden state, for security.
constexpr base::TimeDelta kHideTextAfterDelay = base::Seconds(5);

}  // namespace

AuthTextfieldTimer::AuthTextfieldTimer(AuthTextfield* auth_textfield)
    : auth_textfield_(auth_textfield),
      clear_text_timer_(
          FROM_HERE,
          kClearTextAfterDelay,
          base::BindRepeating(&AuthTextfieldTimer::ClearTextTimerElapsed,
                              base::Unretained(this))),
      hide_text_timer_(
          FROM_HERE,
          kHideTextAfterDelay,
          base::BindRepeating(&AuthTextfieldTimer::HideTextTimerElapsed,
                              base::Unretained(this))) {
  auth_textfield_->AddObserver(this);
}

AuthTextfieldTimer::~AuthTextfieldTimer() {
  auth_textfield_->RemoveObserver(this);
  auth_textfield_ = nullptr;
}

void AuthTextfieldTimer::OnContentsChanged(const std::u16string& new_contents) {
  if (new_contents.empty()) {
    hide_text_timer_.Stop();
    clear_text_timer_.Stop();
    return;
  }
  if (auth_textfield_->IsTextVisible()) {
    hide_text_timer_.Reset();
  } else {
    hide_text_timer_.Stop();
  }
  clear_text_timer_.Reset();
}

void AuthTextfieldTimer::OnTextVisibleChanged(bool visible) {
  if (visible) {
    hide_text_timer_.Reset();
  } else {
    hide_text_timer_.Stop();
  }
}

void AuthTextfieldTimer::ClearTextTimerElapsed() {
  auth_textfield_->Reset();
}

void AuthTextfieldTimer::HideTextTimerElapsed() {
  // The password is not hidden if ChromeVox is enabled.
  if (Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    return;
  }
  auth_textfield_->SetTextVisible(false);
}

}  // namespace ash

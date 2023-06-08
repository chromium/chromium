// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_DIALOG_H_
#define ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_DIALOG_H_

#include "ash/ash_export.h"
#include "ash/style/system_dialog_delegate_view.h"
#include "base/functional/callback_forward.h"

namespace ash {

// A singleton dialog view where a user can choose to start the Welcome Tour
// tutorial. Used if and only if the Welcome Tour feature is enabled.
class ASH_EXPORT WelcomeTourDialog : public SystemDialogDelegateView {
 public:
  METADATA_HEADER(WelcomeTourDialog);

  // Creates and shows the Welcome Tour dialog at the center of the primary
  // display. `start_tutorial_callback` is the callback that runs to start the
  // Welcome Tour tutorial.
  static void CreateAndShow(base::OnceClosure start_tutorial_callback);

  // Returns a pointer to the `WelcomeTourDialog` instance. Returns `nullptr` if
  // the instance does not exist.
  static WelcomeTourDialog* Get();

  WelcomeTourDialog(const WelcomeTourDialog&) = delete;
  WelcomeTourDialog& operator=(const WelcomeTourDialog&) = delete;
  ~WelcomeTourDialog() override;

 private:
  explicit WelcomeTourDialog(base::OnceClosure start_tutorial_callback);
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_DIALOG_H_

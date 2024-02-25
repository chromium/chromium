// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_DIALOG_H_
#define ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_DIALOG_H_

#include "ash/ash_export.h"
#include "ash/style/system_dialog_delegate_view.h"
#include "base/functional/callback_forward.h"

namespace ash {

// A singleton dialog view which serves as a part of the Welcome Tour. From this
// dialog, a user can choose to accept or cancel the Welcome Tour tutorial. Used
// if and only if the Welcome Tour feature is enabled.
class ASH_EXPORT WelcomeTourDialog : public SystemDialogDelegateView {
  METADATA_HEADER(WelcomeTourDialog, SystemDialogDelegateView)

 public:
  // Creates and shows the Welcome Tour dialog at the center of the primary
  // display. `accept_callback` is the callback that runs when the accept button
  // is clicked. `cancel_callback` is the callback that runs when the cancel
  // button is clicked. `close_callback` is the callback that runs when a user
  // closes the dialog without clicking the accept button or the cancel button.
  static void CreateAndShow(base::OnceClosure accept_callback,
                            base::OnceClosure cancel_callback,
                            base::OnceClosure close_callback);

  // Returns a pointer to the `WelcomeTourDialog` instance. Returns `nullptr` if
  // the instance does not exist.
  static WelcomeTourDialog* Get();

  WelcomeTourDialog(const WelcomeTourDialog&) = delete;
  WelcomeTourDialog& operator=(const WelcomeTourDialog&) = delete;
  ~WelcomeTourDialog() override;

 private:
  WelcomeTourDialog(base::OnceClosure accept_callback,
                    base::OnceClosure cancel_callback,
                    base::OnceClosure close_callback);
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_DIALOG_H_

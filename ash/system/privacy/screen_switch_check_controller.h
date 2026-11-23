// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_SCREEN_SWITCH_CHECK_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_SCREEN_SWITCH_CHECK_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/privacy/screen_security_observer.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Controller of a dialog that confirms the user wants to stop screen share/cast
// on user profile switching.
class ScreenSwitchCheckController : public ScreenSecurityObserver,
                                    public SessionObserver {
 public:
  ScreenSwitchCheckController();

  ScreenSwitchCheckController(const ScreenSwitchCheckController&) = delete;
  ScreenSwitchCheckController& operator=(const ScreenSwitchCheckController&) =
      delete;

  ~ScreenSwitchCheckController() override;

  void set_skip_cancel_dialog_for_testing(bool skip) {
    skip_cancel_dialog_for_testing_ = skip;
  }

  // Determines if it's ok to switch away from the currently active user. Screen
  // casting may block this (or at least throw up a confirmation dialog). Calls
  // |callback| with the result.
  void CanSwitchAwayFromActiveUser(base::OnceCallback<void(bool)> callback);

 private:
  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // ScreenSecurityObserver:
  void OnScreenAccessStart(base::OnceClosure stop_callback,
                           const base::RepeatingClosure& source_callback,
                           const std::u16string& access_app_name) override;
  void OnScreenAccessStop() override;
  void OnRemotingScreenShareStart(base::OnceClosure stop_callback) override;
  void OnRemotingScreenShareStop() override;

  // Removes `widget` from `dialog_widgets_`, which destroys it.
  void OnDialogWidgetClosed(views::Widget* widget,
                            views::Widget::ClosedReason reason);

  bool is_screen_accessed_ = false;
  bool is_remoting_share_ = false;
  bool skip_cancel_dialog_for_testing_ = false;
  std::vector<std::unique_ptr<views::Widget>> dialog_widgets_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_SCREEN_SWITCH_CHECK_CONTROLLER_H_

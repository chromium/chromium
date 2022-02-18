// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_GUEST_TOS_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_GUEST_TOS_SCREEN_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/guest_tos_screen_handler.h"

namespace ash {

// Controller for the Guest ToS screen.
class GuestTosScreen : public BaseScreen {
 public:
  enum class Result {
    ACCEPT,
    BACK,
    CANCEL,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  explicit GuestTosScreen(GuestTosScreenView* view,
                          const ScreenExitCallback& exit_callback);
  GuestTosScreen(const GuestTosScreen&) = delete;
  GuestTosScreen& operator=(const GuestTosScreen&) = delete;
  ~GuestTosScreen() override;

  void OnViewDestroyed(GuestTosScreenView* view);

  void OnAccept(bool enable_usage_stats);

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

  // Callback to make when the guest OOBE prefs are done writing.
  void OnOobeGuestPrefWriteDone();

  GuestTosScreenView* view_ = nullptr;
  ScreenExitCallback exit_callback_;

  base::WeakPtrFactory<GuestTosScreen> weak_ptr_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::GuestTosScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_GUEST_TOS_SCREEN_H_

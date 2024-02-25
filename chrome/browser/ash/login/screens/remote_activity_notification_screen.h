// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_REMOTE_ACTIVITY_NOTIFICATION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_REMOTE_ACTIVITY_NOTIFICATION_SCREEN_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ui/webui/ash/login/remote_activity_notification_screen_handler.h"

namespace ash {

// Representation independent class that controls remote activity notification.
class RemoteActivityNotificationScreen : public BaseScreen {
 public:
  using ScreenExitCallback = base::RepeatingCallback<void()>;

  explicit RemoteActivityNotificationScreen(
      base::WeakPtr<RemoteActivityNotificationView> view,
      const ScreenExitCallback& exit_callback);
  RemoteActivityNotificationScreen(const RemoteActivityNotificationScreen&) =
      delete;
  RemoteActivityNotificationScreen& operator=(
      const RemoteActivityNotificationScreen&) = delete;
  ~RemoteActivityNotificationScreen() override;

 protected:
  ScreenExitCallback exit_callback();

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  base::WeakPtr<RemoteActivityNotificationView> view_;
  ScreenExitCallback exit_callback_;

  base::WeakPtrFactory<RemoteActivityNotificationScreen> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_REMOTE_ACTIVITY_NOTIFICATION_SCREEN_H_

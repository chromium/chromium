// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_SCREEN_SECURITY_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_SCREEN_SECURITY_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/shell_observer.h"
#include "ash/system/privacy/screen_security_observer.h"
#include "base/memory/weak_ptr.h"

namespace ash {

extern ASH_EXPORT const char kScreenAccessNotificationId[];
extern ASH_EXPORT const char kRemotingScreenShareNotificationId[];
extern ASH_EXPORT const char kNotifierScreenAccess[];
extern ASH_EXPORT const char kNotifierRemotingScreenShare[];

// Controller class to manage screen security notifications.
class ASH_EXPORT ScreenSecurityController : public ScreenSecurityObserver,
                                            public ShellObserver {
 public:
  ScreenSecurityController();

  ScreenSecurityController(const ScreenSecurityController&) = delete;
  ScreenSecurityController& operator=(const ScreenSecurityController&) = delete;

  ~ScreenSecurityController() override;

  // Stop all sharing/accessing sessions by calling all the callbacks in
  // `screen_access_stop_callbacks_` or `remoting_share_stop_callbacks_`,
  // depending on `is_screen_access` argument. Also removes any privacy
  // notifications if exist.
  void StopAllSessions(bool is_screen_access);

 private:
  // Creates the screen security notification. If
  // `is_screen_access_notification`, the notification is created for screen
  // access. Otherwise it is for remoting screen share.
  void CreateNotification(const std::u16string& message,
                          bool is_screen_access_notification);

  // Changes the source of current capture session by bringing up the picker
  // again, only if there is only one screen capture session.
  void ChangeSource();

  // ScreenSecurityObserver:
  void OnScreenAccessStart(base::OnceClosure stop_callback,
                           const base::RepeatingClosure& source_callback,
                           const std::u16string& access_app_name) override;
  void OnScreenAccessStop() override;
  void OnRemotingScreenShareStart(base::OnceClosure stop_callback) override;
  void OnRemotingScreenShareStop() override;

  // ShellObserver:
  void OnCastingSessionStartedOrStopped(bool started) override;

  bool is_casting_ = false;

  // There can be multiple cast sessions at the same time. If the user hits the
  // stop button, stops all sessions since there is not a good UI to distinguish
  // between the different sessions.
  std::vector<base::OnceClosure> screen_access_stop_callbacks_;
  std::vector<base::OnceClosure> remoting_share_stop_callbacks_;
  base::RepeatingClosure change_source_callback_;

  base::WeakPtrFactory<ScreenSecurityController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_SCREEN_SECURITY_CONTROLLER_H_

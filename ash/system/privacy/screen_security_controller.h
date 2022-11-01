// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_SCREEN_SECURITY_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_SCREEN_SECURITY_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/shell_observer.h"
#include "ash/system/privacy/screen_capture_observer.h"
#include "ash/system/privacy/screen_share_observer.h"
#include "base/memory/weak_ptr.h"

namespace ash {

extern ASH_EXPORT const char kScreenCaptureNotificationId[];
extern ASH_EXPORT const char kScreenShareNotificationId[];
extern ASH_EXPORT const char kNotifierScreenCapture[];
extern ASH_EXPORT const char kNotifierScreenShare[];

// Controller class to manage screen security notifications.
class ASH_EXPORT ScreenSecurityController : public ScreenCaptureObserver,
                                            public ScreenShareObserver,
                                            public ShellObserver {
 public:
  ScreenSecurityController();

  ScreenSecurityController(const ScreenSecurityController&) = delete;
  ScreenSecurityController& operator=(const ScreenSecurityController&) = delete;

  ~ScreenSecurityController() override;

 private:
  void CreateNotification(const std::u16string& message, bool is_capture);
  // Remove the notification and call all the callbacks in
  // |capture_stop_callbacks_| or |share_stop_callbacks_|, depending on
  // |is_capture| argument.
  void StopAllSessions(bool is_capture);
  // Change the source of current capture session by bringing up the picker
  // again, only if there is only one screen capture session.
  void ChangeSource();

  // ScreenCaptureObserver:
  void OnScreenCaptureStart(
      base::OnceClosure stop_callback,
      const base::RepeatingClosure& source_callback,
      const std::u16string& screen_capture_status) override;
  void OnScreenCaptureStop() override;

  // ScreenShareObserver:
  void OnScreenShareStart(base::OnceClosure stop_callback,
                          const std::u16string& helper_name) override;
  void OnScreenShareStop() override;

  // ShellObserver:
  void OnCastingSessionStartedOrStopped(bool started) override;

  bool is_casting_ = false;

  // There can be multiple cast sessions at the same time. If the user hits the
  // stop button, stop all sessions since there is not a good UI to distinguish
  // between the different sessions.
  std::vector<base::OnceClosure> capture_stop_callbacks_;
  std::vector<base::OnceClosure> share_stop_callbacks_;
  base::RepeatingClosure change_source_callback_;

  base::WeakPtrFactory<ScreenSecurityController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_SCREEN_SECURITY_CONTROLLER_H_

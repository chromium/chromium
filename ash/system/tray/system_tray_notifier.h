// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_NOTIFIER_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_NOTIFIER_H_

#include <string>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list.h"

namespace ash {

class IMEObserver;
class NetworkObserver;
class ScreenCaptureObserver;
class ScreenShareObserver;
class SystemTrayObserver;
class VirtualKeyboardObserver;

namespace mojom {
enum class UpdateSeverity;
}

// Observer and notification manager for the ash system tray.
class ASH_EXPORT SystemTrayNotifier {
 public:
  SystemTrayNotifier();

  SystemTrayNotifier(const SystemTrayNotifier&) = delete;
  SystemTrayNotifier& operator=(const SystemTrayNotifier&) = delete;

  ~SystemTrayNotifier();

  // Input methods.
  void AddIMEObserver(IMEObserver* observer);
  void RemoveIMEObserver(IMEObserver* observer);
  void NotifyRefreshIME();
  void NotifyRefreshIMEMenu(bool is_active);

  // Network.
  void AddNetworkObserver(NetworkObserver* observer);
  void RemoveNetworkObserver(NetworkObserver* observer);
  void NotifyRequestToggleWifi();

  // Screen capture.
  void AddScreenCaptureObserver(ScreenCaptureObserver* observer);
  void RemoveScreenCaptureObserver(ScreenCaptureObserver* observer);
  void NotifyScreenCaptureStart(base::RepeatingClosure stop_callback,
                                base::RepeatingClosure source_callback,
                                const std::u16string& sharing_app_name);
  void NotifyScreenCaptureStop();

  // Screen share.
  void AddScreenShareObserver(ScreenShareObserver* observer);
  void RemoveScreenShareObserver(ScreenShareObserver* observer);
  void NotifyScreenShareStart(base::RepeatingClosure stop_callback,
                              const std::u16string& helper_name);
  void NotifyScreenShareStop();

  // System tray focus.
  void AddSystemTrayObserver(SystemTrayObserver* observer);
  void RemoveSystemTrayObserver(SystemTrayObserver* observer);
  void NotifyFocusOut(bool reverse);
  void NotifySystemTrayBubbleShown();

  // Virtual keyboard.
  void AddVirtualKeyboardObserver(VirtualKeyboardObserver* observer);
  void RemoveVirtualKeyboardObserver(VirtualKeyboardObserver* observer);
  void NotifyVirtualKeyboardSuppressionChanged(bool suppressed);

 private:
  base::ObserverList<IMEObserver>::Unchecked ime_observers_;
  base::ObserverList<NetworkObserver>::Unchecked network_observers_;
  base::ObserverList<ScreenCaptureObserver>::Unchecked
      screen_capture_observers_;
  base::ObserverList<ScreenShareObserver>::Unchecked screen_share_observers_;
  base::ObserverList<SystemTrayObserver>::Unchecked system_tray_observers_;
  base::ObserverList<VirtualKeyboardObserver>::Unchecked
      virtual_keyboard_observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_NOTIFIER_H_

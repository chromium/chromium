// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_SCREEN_STATE_RECEIVER_H_
#define BASE_ANDROID_SCREEN_STATE_RECEIVER_H_

#include "base/base_export.h"
#include "base/observer_list_types.h"

namespace base::android {

class BASE_EXPORT ScreenStateReceiver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when the device goes to sleep and becomes non-interactive.
    // This is triggered by various system scenarios, including:
    // - Pressing the physical/virtual power button.
    // - Screen idle timeout.
    // - Entering Always-On Display (AOD): The system state transitions to
    //   DOZING, which is considered Non-Interactive and triggers the broadcast.
    // - Closing hardware mechanisms (laptop lid, folding a foldable device)
    // - Programmatic forced lock: For example, Device Admin API calls, or
    //   double-tap on the home screen to sleep.
    // - HDMI CEC command: A connected external display sends a Standby command.
    // - Inattentive sleep: The system forces sleep after detecting that the
    //   user hasn't looked at the screen for an extended period.
    // - Sleep button or accessibility actions.
    virtual void OnScreenOff() {}
    virtual void OnScreenOn() {}
  };

  static void AddObserver(Observer* observer);
  static void RemoveObserver(Observer* observer);
};

}  // namespace base::android

#endif  // BASE_ANDROID_SCREEN_STATE_RECEIVER_H_

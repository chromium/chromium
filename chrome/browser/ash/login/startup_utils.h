// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_STARTUP_UTILS_H_
#define CHROME_BROWSER_ASH_LOGIN_STARTUP_UTILS_H_

#include <string>

#include "base/functional/callback_forward.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Time;
class TimeDelta;
}  // namespace base

namespace ash {

// Static utility methods used at startup time to get/change bits of device
// state.
class StartupUtils {
 public:
  // Returns true if EULA has been accepted.
  static bool IsEulaAccepted(const PrefService& local_state);

  // Returns OOBE completion status, i.e. whether the OOBE wizard should be run
  // on next boot.  This is NOT what causes the .oobe_completed flag file to be
  // written.
  static bool IsOobeCompleted(const PrefService& local_state);

  // Marks EULA status as accepted.
  static void MarkEulaAccepted(PrefService& local_state);

  // Marks OOBE process as completed.
  static void MarkOobeCompleted(PrefService& local_state);

  // Stores the next pending OOBE screen in case it will need to be resumed.
  static void SaveOobePendingScreen(PrefService& local_state,
                                    const std::string& screen);

  // Stores the next OOBE screen after updating and rebooting to be resumed.
  static void SaveScreenAfterConsumerUpdate(PrefService& local_state,
                                            const std::string& screen);

  // Returns the time the OOBE flag file was created.
  static base::Time GetTimeOfOobeFlagFileCreation();

  // Returns the time since the OOBE flag file was created.
  static base::TimeDelta GetTimeSinceOobeFlagFileCreation();

  // Returns device registration completion status, i.e. second part of OOBE.
  // It is triggered by enrolling the device, but also by logging in as a
  // consumer owner or by logging in as guest.  This state change is announced
  // to the system by writing the .oobe_completed flag file.
  static bool IsDeviceRegistered(PrefService& local_state);

  // clear specific oobe preference from Local state.
  static void ClearSpecificOobePrefs();

  // Marks device registered. i.e. second part of OOBE is completed.
  static void MarkDeviceRegistered(base::OnceClosure done_callback);

  // Mark a device as requiring enrollment recovery.
  static void MarkEnrollmentRecoveryRequired();

  static void DisableHIDDetectionScreenForTests();

  // If `local_state` is nullptr, it's taken from g_browser_process.
  // TODO(crbug.com/393260347): Remove the g_browser_process dependency.
  static bool IsHIDDetectionScreenDisabledForTests(
      PrefService* local_state = nullptr);

  // Returns initial locale from local settings.
  static std::string GetInitialLocale();

  // Sets initial locale in local settings.
  static void SetInitialLocale(const std::string& locale);

  // Registers OOBE local state preferences .
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Registers OOBE and login related preferences that are associated with a
  // profile.
  static void RegisterOobeProfilePrefs(PrefRegistrySimple* registry);

  // Returns whether the device is owned by a consumer or has been enterprise
  // enrolled.
  static bool IsDeviceOwned();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_STARTUP_UTILS_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SCREEN_EXIT_CODE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SCREEN_EXIT_CODE_H_

#include <string>

namespace chromeos {

// Each login screen or a view shown within login wizard view is itself a
// state. Upon exit each view returns one of the results by calling
// the BaseScreenDelegate::OnExit() method. Depending on the result and the
// current view or state the login wizard decides what is the next view to show.
//
// There must be an exit code for each way to exit the screen for each screen.
//
// Numeric ids are provided to facilitate interpretation of log files only,
// they are subject to change without notice.
enum class ScreenExitCode {
  // "Continue" was pressed on welcome screen.
  WELCOME_CONTINUED = 0,
  HID_DETECTION_COMPLETED = 1,
  // Connection failed while trying to load a WebPageScreen.
  CONNECTION_FAILED = 2,
  UPDATE_INSTALLED = 3,
  // This exit code means EITHER that there was no update, OR that there
  // was an update, but that it was not a "critical" update. "Critical" updates
  // are those that have a deadline and require the device to reboot.
  UPDATE_NOUPDATE = 4,
  UPDATE_ERROR_CHECKING_FOR_UPDATE = 5,
  UPDATE_ERROR_UPDATING = 6,
  USER_IMAGE_SELECTED = 7,
  EULA_ACCEPTED = 8,
  EULA_BACK = 9,
  ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED = 10,
  ENTERPRISE_ENROLLMENT_COMPLETED = 11,
  ENTERPRISE_ENROLLMENT_BACK = 12,
  RESET_CANCELED = 13,
  KIOSK_AUTOLAUNCH_CANCELED = 14,
  KIOSK_AUTOLAUNCH_CONFIRMED = 15,
  KIOSK_ENABLE_COMPLETED = 16,
  TERMS_OF_SERVICE_DECLINED = 17,
  TERMS_OF_SERVICE_ACCEPTED = 18,
  WRONG_HWID_WARNING_SKIPPED = 19,
  CONTROLLER_PAIRING_FINISHED = 20,
  ENABLE_DEBUGGING_FINISHED = 21,
  ENABLE_DEBUGGING_CANCELED = 22,
  ARC_TERMS_OF_SERVICE_SKIPPED = 23,
  ARC_TERMS_OF_SERVICE_ACCEPTED = 24,
  UPDATE_ERROR_UPDATING_CRITICAL_UPDATE = 25,
  ENCRYPTION_MIGRATION_FINISHED = 26,
  ENCRYPTION_MIGRATION_SKIPPED = 27,
  VOICE_INTERACTION_VALUE_PROP_SKIPPED = 28,
  VOICE_INTERACTION_VALUE_PROP_ACCEPTED = 29,
  WAIT_FOR_CONTAINER_READY_FINISHED = 30,
  WAIT_FOR_CONTAINER_READY_ERROR = 31,
  SYNC_CONSENT_FINISHED = 32,
  DEMO_MODE_SETUP_FINISHED = 33,
  DEMO_MODE_SETUP_CANCELED = 34,
  RECOMMEND_APPS_SKIPPED = 35,
  RECOMMEND_APPS_SELECTED = 36,
  DEMO_MODE_PREFERENCES_CONTINUED = 37,
  DEMO_MODE_PREFERENCES_CANCELED = 38,
  APP_DOWNLOADING_FINISHED = 39,
  ARC_TERMS_OF_SERVICE_BACK = 40,
  DISCOVER_FINISHED = 41,
  NETWORK_BACK = 42,
  NETWORK_CONNECTED = 43,
  NETWORK_OFFLINE_DEMO_SETUP = 44,
  FINGERPRINT_SETUP_FINISHED = 45,
  MARKETING_OPT_IN_FINISHED = 46,
  ASSISTANT_OPTIN_FLOW_FINISHED = 47,
  MULTIDEVICE_SETUP_FINISHED = 48,
  UPDATE_REJECT_OVER_CELLULAR = 49,
  EXIT_CODES_COUNT  // not a real code, must be the last
};

std::string ExitCodeToString(ScreenExitCode code);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SCREEN_EXIT_CODE_H_

// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_SCREEN_H_

#include <string>

namespace chromeos {

// TODO(jdufault): Rename to LoginScreen or similar since this is not directly
// tied to Oobe. See crbug.com/678740.

// Different screens in the Oobe. If you update this enum, *make sure* to
// update kScreenNames in the cc file as well.
enum class OobeScreen : unsigned int {
  SCREEN_OOBE_HID_DETECTION = 0,
  SCREEN_OOBE_WELCOME,
  SCREEN_OOBE_NETWORK,
  SCREEN_OOBE_EULA,
  SCREEN_OOBE_UPDATE,
  SCREEN_OOBE_ENABLE_DEBUGGING,
  SCREEN_OOBE_ENROLLMENT,
  SCREEN_OOBE_RESET,
  SCREEN_GAIA_SIGNIN,
  SCREEN_ACCOUNT_PICKER,
  SCREEN_KIOSK_AUTOLAUNCH,
  SCREEN_KIOSK_ENABLE,
  SCREEN_ERROR_MESSAGE,
  SCREEN_USER_IMAGE_PICKER,
  SCREEN_TPM_ERROR,
  SCREEN_PASSWORD_CHANGED,
  SCREEN_CREATE_SUPERVISED_USER_FLOW_DEPRECATED,
  SCREEN_TERMS_OF_SERVICE,
  SCREEN_ARC_TERMS_OF_SERVICE,
  SCREEN_WRONG_HWID,
  SCREEN_AUTO_ENROLLMENT_CHECK,
  SCREEN_APP_LAUNCH_SPLASH,
  SCREEN_ARC_KIOSK_SPLASH,
  SCREEN_CONFIRM_PASSWORD,
  SCREEN_FATAL_ERROR,
  SCREEN_OOBE_CONTROLLER_PAIRING,
  SCREEN_OOBE_HOST_PAIRING,
  SCREEN_DEVICE_DISABLED,
  SCREEN_UNRECOVERABLE_CRYPTOHOME_ERROR,
  SCREEN_USER_SELECTION,
  SCREEN_ACTIVE_DIRECTORY_PASSWORD_CHANGE,
  SCREEN_ENCRYPTION_MIGRATION,
  SCREEN_VOICE_INTERACTION_VALUE_PROP,
  SCREEN_WAIT_FOR_CONTAINER_READY,
  SCREEN_UPDATE_REQUIRED,
  SCREEN_ASSISTANT_OPTIN_FLOW,

  // Special "first screen" that initiates login flow.
  SCREEN_SPECIAL_LOGIN,
  // Special "first screen" that initiates full OOBE flow.
  SCREEN_SPECIAL_OOBE,
  // Special test value that commands not to create any window yet.
  SCREEN_TEST_NO_WINDOW,

  SCREEN_SYNC_CONSENT,
  SCREEN_FINGERPRINT_SETUP,
  SCREEN_OOBE_DEMO_SETUP,
  SCREEN_OOBE_DEMO_PREFERENCES,

  SCREEN_RECOMMEND_APPS,
  SCREEN_APP_DOWNLOADING,
  SCREEN_DISCOVER,

  SCREEN_MARKETING_OPT_IN,
  SCREEN_MULTIDEVICE_SETUP,

  SCREEN_UNKNOWN  // This must always be the last element.
};

// Returns the JS name for the given screen.
std::string GetOobeScreenName(OobeScreen screen);

// Converts the JS name for the given sreen into a Screen instance.
OobeScreen GetOobeScreenFromName(const std::string& name);

// Returns true if a command line argument requests |screen| to always be shown.
bool ForceShowOobeScreen(OobeScreen screen);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_OOBE_SCREEN_H_

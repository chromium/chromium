// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/screen_exit_code.h"

#include "base/logging.h"

namespace chromeos {

std::string ExitCodeToString(ScreenExitCode code) {
  switch (code) {
    case ScreenExitCode::WELCOME_CONTINUED:
      return "WELCOME_CONTINUED";
    case ScreenExitCode::HID_DETECTION_COMPLETED:
      return "HID_DETECTION_COMPLETED";
    case ScreenExitCode::CONNECTION_FAILED:
      return "CONNECTION_FAILED";
    case ScreenExitCode::UPDATE_INSTALLED:
      return "UPDATE_INSTALLED";
    case ScreenExitCode::UPDATE_NOUPDATE:
      return "UPDATE_NOUPDATE";
    case ScreenExitCode::UPDATE_ERROR_CHECKING_FOR_UPDATE:
      return "UPDATE_ERROR_CHECKING_FOR_UPDATE";
    case ScreenExitCode::UPDATE_ERROR_UPDATING:
      return "UPDATE_ERROR_UPDATING";
    case ScreenExitCode::USER_IMAGE_SELECTED:
      return "USER_IMAGE_SELECTED";
    case ScreenExitCode::EULA_ACCEPTED:
      return "EULA_ACCEPTED";
    case ScreenExitCode::EULA_BACK:
      return "EULA_BACK";
    case ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED:
      return "ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED";
    case ScreenExitCode::ENTERPRISE_ENROLLMENT_COMPLETED:
      return "ENTERPRISE_ENROLLMENT_COMPLETED";
    case ScreenExitCode::ENTERPRISE_ENROLLMENT_BACK:
      return "ENTERPRISE_ENROLLMENT_BACK";
    case ScreenExitCode::RESET_CANCELED:
      return "RESET_CANCELED";
    case ScreenExitCode::KIOSK_AUTOLAUNCH_CANCELED:
      return "KIOSK_AUTOLAUNCH_CANCELED";
    case ScreenExitCode::KIOSK_AUTOLAUNCH_CONFIRMED:
      return "KIOSK_AUTOLAUNCH_CONFIRMED";
    case ScreenExitCode::KIOSK_ENABLE_COMPLETED:
      return "KIOSK_ENABLE_COMPLETED";
    case ScreenExitCode::TERMS_OF_SERVICE_DECLINED:
      return "TERMS_OF_SERVICE_DECLINED";
    case ScreenExitCode::TERMS_OF_SERVICE_ACCEPTED:
      return "TERMS_OF_SERVICE_ACCEPTED";
    case ScreenExitCode::WRONG_HWID_WARNING_SKIPPED:
      return "WRONG_HWID_WARNING_SKIPPED";
    case ScreenExitCode::CONTROLLER_PAIRING_FINISHED:
      return "CONTROLLER_PAIRING_FINISHED";
    case ScreenExitCode::ENABLE_DEBUGGING_FINISHED:
      return "ENABLE_DEBUGGING_FINISHED";
    case ScreenExitCode::ENABLE_DEBUGGING_CANCELED:
      return "ENABLE_DEBUGGING_CANCELED";
    case ScreenExitCode::ARC_TERMS_OF_SERVICE_SKIPPED:
      return "ARC_TERMS_OF_SERVICE_SKIPPED";
    case ScreenExitCode::ARC_TERMS_OF_SERVICE_ACCEPTED:
      return "ARC_TERMS_OF_SERVICE_ACCEPTED";
    case ScreenExitCode::UPDATE_ERROR_UPDATING_CRITICAL_UPDATE:
      return "UPDATE_ERROR_UPDATING_CRITICAL_UPDATE";
    case ScreenExitCode::VOICE_INTERACTION_VALUE_PROP_SKIPPED:
      return "VOICE_INTERACTION_VALUE_PROP_SKIPPED";
    case ScreenExitCode::VOICE_INTERACTION_VALUE_PROP_ACCEPTED:
      return "VOICE_INTERACTION_VALUE_PROP_ACCEPTED";
    case ScreenExitCode::WAIT_FOR_CONTAINER_READY_FINISHED:
      return "WAIT_FOR_CONTAINER_READY_FINISHED";
    case ScreenExitCode::WAIT_FOR_CONTAINER_READY_ERROR:
      return "WAIT_FOR_CONTAINER_READY_ERROR";
    case ScreenExitCode::SYNC_CONSENT_FINISHED:
      return "SYNC_CONSENT_FINISHED";
    case ScreenExitCode::DEMO_MODE_SETUP_FINISHED:
      return "DEMO_MODE_SETUP_FINISHED";
    case ScreenExitCode::DEMO_MODE_SETUP_CANCELED:
      return "DEMO_MODE_SETUP_CANCELED";
    case ScreenExitCode::RECOMMEND_APPS_SKIPPED:
      return "RECOMMEND_APPS_SKIPPED";
    case ScreenExitCode::RECOMMEND_APPS_SELECTED:
      return "RECOMMEND_APPS_SELECTED";
    case ScreenExitCode::DEMO_MODE_PREFERENCES_CONTINUED:
      return "DEMO_MODE_PREFERENCES_CONTINUED";
    case ScreenExitCode::DEMO_MODE_PREFERENCES_CANCELED:
      return "DEMO_MODE_PREFERENCES_CANCELED";
    case ScreenExitCode::APP_DOWNLOADING_FINISHED:
      return "APP_DOWNLOADING_FINISHED";
    case ScreenExitCode::ARC_TERMS_OF_SERVICE_BACK:
      return "ARC_TERMS_OF_SERVICE_BACK";
    case ScreenExitCode::DISCOVER_FINISHED:
      return "DISCOVER_FINISHED";
    case ScreenExitCode::NETWORK_BACK:
      return "NETWORK_BACK";
    case ScreenExitCode::NETWORK_CONNECTED:
      return "NETWORK_CONNECTED";
    case ScreenExitCode::NETWORK_OFFLINE_DEMO_SETUP:
      return "NETWORK_OFFLINE_DEMO_SETUP";
    case ScreenExitCode::FINGERPRINT_SETUP_FINISHED:
      return "FINGERPRINT_SETUP_FINISHED";
    case ScreenExitCode::MARKETING_OPT_IN_FINISHED:
      return "MARKETING_OPT_IN_FINISHED";
    case ScreenExitCode::ASSISTANT_OPTIN_FLOW_FINISHED:
      return "ASSISTANT_OPTIN_FLOW_FINISHED";
    case ScreenExitCode::MULTIDEVICE_SETUP_FINISHED:
      return "MULTIDEVICE_SETUP_FINISHED";
    case ScreenExitCode::EXIT_CODES_COUNT:
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace chromeos

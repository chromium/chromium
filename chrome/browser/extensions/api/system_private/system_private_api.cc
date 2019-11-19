// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_private/system_private_api.h"

#include <memory>
#include <utility>

#include "base/stl_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/system_private.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "google_apis/google_api_keys.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/update_engine_client.h"
#else
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#endif

namespace {

// Maps prefs::kIncognitoModeAvailability values (0 = enabled, ...)
// to strings exposed to extensions.
const char* const kIncognitoModeAvailabilityStrings[] = {
  "enabled",
  "disabled",
  "forced"
};

// Property keys.
const char kDownloadProgressKey[] = "downloadProgress";
const char kStateKey[] = "state";

// System update states.
const char kNotAvailableState[] = "NotAvailable";
const char kNeedRestartState[] = "NeedRestart";

#if defined(OS_CHROMEOS)
const char kUpdatingState[] = "Updating";
#endif  // defined(OS_CHROMEOS)

}  // namespace

namespace extensions {

namespace system_private = api::system_private;

ExtensionFunction::ResponseAction
SystemPrivateGetIncognitoModeAvailabilityFunction::Run() {
  PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  int value = prefs->GetInteger(prefs::kIncognitoModeAvailability);
  EXTENSION_FUNCTION_VALIDATE(
      value >= 0 &&
      value < static_cast<int>(base::size(kIncognitoModeAvailabilityStrings)));
  return RespondNow(OneArgument(
      std::make_unique<base::Value>(kIncognitoModeAvailabilityStrings[value])));
}

ExtensionFunction::ResponseAction SystemPrivateGetUpdateStatusFunction::Run() {
  std::string state;
  double download_progress = 0;
#if defined(OS_CHROMEOS)
  // With UpdateEngineClient, we can provide more detailed information about
  // system updates on ChromeOS.
  const update_engine::StatusResult status = chromeos::DBusThreadManager::Get()
                                                 ->GetUpdateEngineClient()
                                                 ->GetLastStatus();
  // |download_progress| is set to 1 after download finishes
  // (i.e. verify, finalize and need-reboot phase) to indicate the progress
  // even though |status.download_progress| is 0 in these phases.
  switch (status.current_operation()) {
    case update_engine::Operation::ERROR:
    case update_engine::Operation::DISABLED:
      state = kNotAvailableState;
      break;
    case update_engine::Operation::IDLE:
      state = kNotAvailableState;
      break;
    case update_engine::Operation::CHECKING_FOR_UPDATE:
      state = kNotAvailableState;
      break;
    case update_engine::Operation::UPDATE_AVAILABLE:
      state = kUpdatingState;
      break;
    case update_engine::Operation::DOWNLOADING:
      state = kUpdatingState;
      download_progress = status.progress();
      break;
    case update_engine::Operation::VERIFYING:
      state = kUpdatingState;
      download_progress = 1;
      break;
    case update_engine::Operation::FINALIZING:
      state = kUpdatingState;
      download_progress = 1;
      break;
    case update_engine::Operation::UPDATED_NEED_REBOOT:
      state = kNeedRestartState;
      download_progress = 1;
      break;
    case update_engine::Operation::REPORTING_ERROR_EVENT:
    case update_engine::Operation::ATTEMPTING_ROLLBACK:
    case update_engine::Operation::NEED_PERMISSION_TO_UPDATE:
      state = kNotAvailableState;
      break;
    default:
      NOTREACHED();
  }
#else
  if (UpgradeDetector::GetInstance()->notify_upgrade()) {
    state = kNeedRestartState;
    download_progress = 1;
  } else {
    state = kNotAvailableState;
  }
#endif

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString(kStateKey, state);
  dict->SetDouble(kDownloadProgressKey, download_progress);
  return RespondNow(OneArgument(std::move(dict)));
}

ExtensionFunction::ResponseAction SystemPrivateGetApiKeyFunction::Run() {
  return RespondNow(
      OneArgument(std::make_unique<base::Value>(google_apis::GetAPIKey())));
}

}  // namespace extensions

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_switches.h"

namespace ash {
namespace switches {

// If set, the CrOSActionRecorder will be enabled which will record some user
// actions on device.
const char kEnableCrOSActionRecorder[] = "enable-cros-action-recorder";
// Copy user action data to download directory.
const char kCrOSActionRecorderCopyToDownloadDir[] = "copy-to-download-dir";
// Disable cros action logging.
const char kCrOSActionRecorderDisabled[] = "disable-and-delete-previous-log";
// Log user actions with action name hashed.
const char kCrOSActionRecorderWithHash[] = "log-with-hash";
// Log user actions with action name unhashed.
const char kCrOSActionRecorderWithoutHash[] = "log-without-hash";
// Disable structured metrics logging of cros actions.
const char kCrOSActionRecorderStructuredDisabled[] =
    "structured-metrics-disabled";

}  // namespace switches
}  // namespace ash

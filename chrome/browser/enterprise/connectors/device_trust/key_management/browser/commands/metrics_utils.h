// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_METRICS_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_METRICS_UTILS_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif  // BUILDFLAG(IS_WIN)

namespace enterprise_connectors {

// Superset of errors that can occur in key rotation commands on all platforms.
// Do not change ordering. If adding a new value, also update
// DTKeyRotationCommandError in enums.xml.
enum class KeyRotationCommandError {
  kUnknown = 0,
  kTimeout = 1,
  kClassNotRegistered = 2,
  kNoInterface = 3,
  kUpdaterConcurrency = 4,
  kUserInstallation = 5,
  kMissingManagementService = 6,
  kProcessInvalid = 7,
  kMaxValue = kProcessInvalid,
};

// Logs the known key rotation command `error`.
void LogKeyRotationCommandError(KeyRotationCommandError error);

// Logs the given `exit_code` as result of a key rotation command. Handles both
// positive and negative values.
void LogKeyRotationExitCode(int exit_code);

#if BUILDFLAG(IS_WIN)
// Logs the unexpected `result` code from trying to communicate with the
// installer.
void LogUnexpectedHresult(HRESULT result);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_BROWSER_COMMANDS_METRICS_UTILS_H_

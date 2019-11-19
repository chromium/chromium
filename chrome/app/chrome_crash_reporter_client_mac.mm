// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_crash_reporter_client.h"

#include <CoreFoundation/CoreFoundation.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/policy_constants.h"
#include "components/version_info/version_info.h"

bool ChromeCrashReporterClient::ReportingIsEnforcedByPolicy(
    bool* breakpad_enabled) {
  base::ScopedCFTypeRef<CFStringRef> key(
      base::SysUTF8ToCFStringRef(policy::key::kMetricsReportingEnabled));
  Boolean key_valid;
  Boolean metrics_reporting_enabled = CFPreferencesGetAppBooleanValue(key,
      kCFPreferencesCurrentApplication, &key_valid);
  if (key_valid &&
      CFPreferencesAppValueIsForced(key, kCFPreferencesCurrentApplication)) {
    *breakpad_enabled = metrics_reporting_enabled;
    return true;
  }
  return false;
}

bool ChromeCrashReporterClient::ShouldMonitorCrashHandlerExpensively() {
  // This mechanism dedicates a process to be crashpad_handler's own
  // crashpad_handler. In Google Chrome, scale back on this in the more stable
  // channels. Other builds are of more of a developmental nature, so always
  // enable the additional crash reporting.
  double probability;
  switch (chrome::GetChannel()) {
    case version_info::Channel::STABLE:
      probability = 0.01;
      break;

    case version_info::Channel::BETA:
      probability = 0.1;
      break;

    case version_info::Channel::DEV:
      probability = 0.25;
      break;

    default:
      return true;
  }

  return base::RandDouble() < probability;
}

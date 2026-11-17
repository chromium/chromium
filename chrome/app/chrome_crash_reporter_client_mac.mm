// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_crash_reporter_client.h"

#include <CoreFoundation/CoreFoundation.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/policy_constants.h"

bool ChromeCrashReporterClient::ReportingIsEnforcedByPolicy(
    bool* breakpad_enabled) {
  base::apple::ScopedCFTypeRef<CFStringRef> key =
      base::SysUTF8ToCFStringRef(policy::key::kMetricsReportingEnabled);
  Boolean key_valid;
  Boolean metrics_reporting_enabled = CFPreferencesGetAppBooleanValue(
      key.get(), kCFPreferencesCurrentApplication, &key_valid);
  if (key_valid && CFPreferencesAppValueIsForced(
                       key.get(), kCFPreferencesCurrentApplication)) {
    *breakpad_enabled = metrics_reporting_enabled;
    return true;
  }
  return false;
}

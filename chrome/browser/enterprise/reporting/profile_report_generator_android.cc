// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/profile_report_generator_android.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise_reporting {

ProfileReportGeneratorAndroid::ProfileReportGeneratorAndroid() = default;

ProfileReportGeneratorAndroid::~ProfileReportGeneratorAndroid() = default;

void ProfileReportGeneratorAndroid::GetExtensionInfo(
    enterprise_management::ChromeUserProfileInfo* report) {
  // Extensions aren't supported on Android.
}

void ProfileReportGeneratorAndroid::GetExtensionRequest(
    enterprise_management::ChromeUserProfileInfo* report) {
  // Extensions aren't supported on Android.
}

}  // namespace enterprise_reporting

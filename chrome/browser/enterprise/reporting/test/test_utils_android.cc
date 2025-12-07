// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/test/test_utils.h"

#include "base/android/device_info.h"
#include "base/run_loop.h"
#include "components/device_signals/core/browser/browser_utils.h"
#include "components/device_signals/core/common/platform_utils.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using safe_browsing::HasHarmfulAppsResultStatus;
using safe_browsing::SafeBrowsingApiHandlerBridge;
using safe_browsing::VerifyAppsEnabledResult;

namespace enterprise_reporting {

namespace em = enterprise_management;

namespace {

void OnIsVerifyAppsEnabledDone(const em::OSReport& os_report,
                               base::OnceClosure done_closure,
                               VerifyAppsEnabledResult result) {
  EXPECT_EQ(os_report.verified_apps_enabled(),
            (result == VerifyAppsEnabledResult::SUCCESS_ENABLED ||
             result == VerifyAppsEnabledResult::SUCCESS_ALREADY_ENABLED));
  std::move(done_closure).Run();
}

void VerifyIsVerifyAppsEnabled(const em::OSReport& os_report,
                               base::OnceClosure done_closure) {
  SafeBrowsingApiHandlerBridge::GetInstance().StartIsVerifyAppsEnabled(
      base::BindOnce(&OnIsVerifyAppsEnabledDone, std::ref(os_report),
                     std::move(done_closure)));
}

void OnHasHarmfulAppsDone(const em::OSReport& os_report,
                          base::OnceClosure done_closure,
                          HasHarmfulAppsResultStatus result,
                          int num_of_apps,
                          int status_code) {
  EXPECT_EQ(
      os_report.has_potentially_harmful_apps(),
      (result == HasHarmfulAppsResultStatus::SUCCESS && num_of_apps != 0));

  std::move(done_closure).Run();
}

void VerifyHasHarmfulApps(const em::OSReport& os_report,
                          base::OnceClosure done_closure) {
  SafeBrowsingApiHandlerBridge::GetInstance().StartHasPotentiallyHarmfulApps(
      base::BindOnce(&OnHasHarmfulAppsDone, std::ref(os_report),
                     std::move(done_closure)));
}

}  // namespace

void SetFakeSignalsValues() {
  SafeBrowsingApiHandlerBridge::GetInstance()
      .SetVerifyAppsEnableResultForTesting(
          safe_browsing::VerifyAppsEnabledResult::SUCCESS_ENABLED);
  SafeBrowsingApiHandlerBridge::GetInstance().SetHarmfulAppsResultForTesting(
      safe_browsing::HasHarmfulAppsResultStatus::SUCCESS,
      /*num_of_apps=*/0,
      /*status_code=*/0);
}

void VerifyDeviceIdentifier(
    em::BrowserDeviceIdentifier& browser_device_identifier,
    bool can_collect_pii_signals) {
  EXPECT_EQ(browser_device_identifier.computer_name(),
            can_collect_pii_signals ? base::android::device_info::device_name()
                                    : std::string());
}

void VerifyOsReportSignals(const em::OSReport& os_report,
                           bool expect_signals_override_value,
                           bool can_collect_pii_signals) {
  EXPECT_EQ(os_report.name(), policy::GetOSPlatform());
  EXPECT_EQ(os_report.arch(), policy::GetOSArchitecture());
  if (expect_signals_override_value) {
    EXPECT_EQ(os_report.version(), device_signals::GetOsVersion());
    EXPECT_EQ(os_report.security_patch_ms(),
              device_signals::GetSecurityPatchLevelEpoch());
    base::RunLoop run_loop;
    VerifyIsVerifyAppsEnabled(
        std::ref(os_report),
        base::BindOnce(&VerifyHasHarmfulApps, std::ref(os_report),
                       run_loop.QuitClosure()));
    run_loop.Run();
  } else {
    EXPECT_EQ(os_report.version(), policy::GetOSVersion());

    // Signals report only fields should not be written
    ASSERT_FALSE(os_report.has_device_enrollment_domain());
    ASSERT_FALSE(os_report.has_screen_lock_secured());

    EXPECT_EQ(0, os_report.mac_addresses_size());
  }
}

}  // namespace enterprise_reporting

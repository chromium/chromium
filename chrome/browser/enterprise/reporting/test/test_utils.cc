// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/test/test_utils.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_policy_blocklist_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/device_signals/core/browser/browser_utils.h"
#include "components/device_signals/core/common/platform_utils.h"
#include "components/enterprise/browser/reporting/report_util.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {
namespace em = enterprise_management;

void CheckReportMatchSignal(std::string_view report_value,
                            std::optional<std::string> signal_value) {
  if (signal_value == std::nullopt) {
    ASSERT_TRUE(report_value.empty());
    return;
  }

  EXPECT_EQ(signal_value.value(), report_value);
}

void VerifyProfileSignalsReport(
    const em::ProfileSignalsReport& profile_signals_report,
    Profile* profile) {
  EXPECT_EQ(profile_signals_report.built_in_dns_client_enabled(),
            g_browser_process->local_state()->GetBoolean(
                prefs::kBuiltInDnsClientEnabled));
  EXPECT_EQ(profile_signals_report.chrome_remote_desktop_app_blocked(),
            device_signals::GetChromeRemoteDesktopAppBlocked(
                ChromePolicyBlocklistServiceFactory::GetForProfile(profile)));
  EXPECT_EQ(profile_signals_report.password_protection_warning_trigger(),
            TranslatePasswordProtectionTrigger(
                device_signals::GetPasswordProtectionWarningTrigger(
                    profile->GetPrefs())));
  CheckReportMatchSignal(
      profile_signals_report.profile_enrollment_domain(),
      device_signals::TryGetEnrollmentDomain(profile->GetCloudPolicyManager()));
  EXPECT_EQ(
      profile_signals_report.safe_browsing_protection_level(),
      TranslateSafeBrowsingLevel(
          device_signals::GetSafeBrowsingProtectionLevel(profile->GetPrefs())));
  EXPECT_EQ(profile_signals_report.site_isolation_enabled(),
            device_signals::GetSiteIsolationEnabled());
}

}  // namespace enterprise_reporting

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_TEST_TEST_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_TEST_TEST_UTILS_H_

#include <optional>
#include <string>
#include <string_view>

#include "build/buildflag.h"
#include "components/policy/proto/device_management_backend.pb.h"

class Profile;

namespace enterprise_reporting {

// Helper function to deal with when a signal with std::nullopt gets
// converted into an empty string in the report.
void CheckReportMatchSignal(std::string_view report_value,
                            std::optional<std::string> signal_value);

// Helper function to set up mock values for some signals.
void SetFakeSignalsValues();

// Helper function to verify that the signals within the
// `BrowserDeviceIdentifier` section of a report matches the expected values.
void VerifyDeviceIdentifier(
    enterprise_management::BrowserDeviceIdentifier& browser_device_identifier,
    bool can_collect_pii_signals);

// Helper function to verify that the signals within the `OSReport` section of a
// report matches the expected values.
void VerifyOsReportSignals(const enterprise_management::OSReport& os_report,
                           bool expect_signals_override_value,
                           bool can_collect_pii_signals);

// Helper function to verify that the signals within the `ProfileSignalsReport`
// section of a report matches the expected values.
void VerifyProfileSignalsReport(
    const enterprise_management::ProfileSignalsReport& profile_signals_report,
    Profile* profile);

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_TEST_TEST_UTILS_H_

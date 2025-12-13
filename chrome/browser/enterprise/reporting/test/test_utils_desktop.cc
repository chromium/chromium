// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/enterprise/reporting/test/test_utils.h"
#include "components/device_signals/core/browser/browser_utils.h"
#include "components/device_signals/core/common/platform_utils.h"
#include "components/enterprise/browser/reporting/report_util.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

namespace em = enterprise_management;

namespace {

void VerifyDeviceIdentifierAsyncSignal(
    em::BrowserDeviceIdentifier& browser_device_identifier,
    bool can_collect_pii_signals,
    base::OnceCallback<void()> done_closure) {
  EXPECT_EQ(browser_device_identifier.serial_number(),
            can_collect_pii_signals ? device_signals::GetSerialNumber()
                                    : std::string());

  std::move(done_closure).Run();
}

void VerifyOsReportAsyncSignal(const em::OSReport& os_report,
                               base::OnceCallback<void()> done_closure) {
  EXPECT_EQ(os_report.disk_encryption(),
            TranslateSettingValue(device_signals::GetDiskEncrypted()));
  EXPECT_EQ(os_report.os_firewall(),
            TranslateSettingValue(device_signals::GetOSFirewall()));
  std::move(done_closure).Run();
}

}  // namespace

void SetFakeSignalsValues() {}

void VerifyDeviceIdentifier(
    em::BrowserDeviceIdentifier& browser_device_identifier,
    bool can_collect_pii_signals) {
  EXPECT_EQ(browser_device_identifier.computer_name(),
            can_collect_pii_signals ? policy::GetDeviceName() : std::string());

  EXPECT_EQ(
      browser_device_identifier.host_name(),
      can_collect_pii_signals ? device_signals::GetHostName() : std::string());

  base::RunLoop run_loop;

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&VerifyDeviceIdentifierAsyncSignal,
                     std::ref(browser_device_identifier),
                     can_collect_pii_signals, run_loop.QuitClosure()));
  run_loop.Run();
}

void VerifyOsReportSignals(const em::OSReport& os_report,
                           bool expect_signals_override_value,
                           bool can_collect_pii_signals) {
  EXPECT_EQ(os_report.name(), policy::GetOSPlatform());
  EXPECT_EQ(os_report.arch(), policy::GetOSArchitecture());
  if (expect_signals_override_value) {
    EXPECT_EQ(os_report.version(), device_signals::GetOsVersion());

    EXPECT_EQ(os_report.screen_lock_secured(),
              TranslateSettingValue(device_signals::GetScreenlockSecured()));
#if BUILDFLAG(IS_WIN)
    EXPECT_EQ(os_report.secure_boot_mode(),
              TranslateSettingValue(device_signals::GetSecureBootEnabled()));
    CheckReportMatchSignal(os_report.windows_machine_domain(),
                           device_signals::GetWindowsMachineDomain());
    CheckReportMatchSignal(os_report.windows_user_domain(),
                           device_signals::GetWindowsUserDomain());
    CheckReportMatchSignal(os_report.machine_guid(),
                           can_collect_pii_signals
                               ? device_signals::GetMachineGuid()
                               : std::nullopt);
#endif  // BUILDFLAG(IS_WIN)
    base::RunLoop run_loop;
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&VerifyOsReportAsyncSignal, std::ref(os_report),
                       run_loop.QuitClosure()));
    run_loop.Run();
  } else {
    EXPECT_EQ(os_report.version(), policy::GetOSVersion());

    // Signals report only fields should not be written
    ASSERT_FALSE(os_report.has_device_enrollment_domain());
    ASSERT_FALSE(os_report.has_screen_lock_secured());

    EXPECT_EQ(0, os_report.mac_addresses_size());
#if BUILDFLAG(IS_WIN)
    EXPECT_EQ(0, os_report.antivirus_info_size());
    EXPECT_EQ(0, os_report.hotfixes_size());
#endif  // BUILDFLAG(IS_WIN)
  }
}

}  // namespace enterprise_reporting

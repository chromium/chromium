// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/signals/decorators/browser/browser_signals_decorator.h"

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/enterprise/signals/device_info_fetcher.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

namespace {

constexpr char kFakeEnrollmentDomain[] = "fake.domain.google.com";
constexpr char kLatencyHistogram[] =
    "Enterprise.DeviceTrust.SignalsDecorator.Latency.Browser";

constexpr int32_t kDisabledSetting = 1;
constexpr int32_t kEnabledSetting = 2;

base::Value::List GetExpectedMacAddresses() {
  base::Value::List mac_addresses;
  mac_addresses.Append("00:00:00:00:00:00");
  return mac_addresses;
}

}  // namespace

class BrowserSignalsDecoratorTest : public testing::Test {
 protected:
  void SetUp() override {
    enterprise_signals::DeviceInfoFetcher::SetForceStubForTesting(
        /*should_force=*/true);
    decorator_.emplace(&mock_cloud_policy_store_);
  }

  void TearDown() override {
    enterprise_signals::DeviceInfoFetcher::SetForceStubForTesting(
        /*should_force=*/false);
  }

  void SetFakePolicyData() {
    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_managed_by(kFakeEnrollmentDomain);
    mock_cloud_policy_store_.set_policy_data_for_testing(
        std::move(policy_data));
  }

  void ValidateStaticSignals(const base::Value::Dict& signals) {
    const auto* serial_number =
        signals.FindString(device_signals::names::kSerialNumber);
    ASSERT_TRUE(serial_number);
    EXPECT_EQ(*serial_number, "twirlchange");

    auto screen_lock_secured =
        signals.FindInt(device_signals::names::kScreenLockSecured);
    ASSERT_TRUE(screen_lock_secured);
    EXPECT_EQ(screen_lock_secured.value(), kEnabledSetting);

    auto disk_encrypted =
        signals.FindInt(device_signals::names::kDiskEncrypted);
    ASSERT_TRUE(disk_encrypted);
    EXPECT_EQ(disk_encrypted.value(), kDisabledSetting);

    const auto* device_host_name =
        signals.FindString(device_signals::names::kDeviceHostName);
    ASSERT_TRUE(device_host_name);
    EXPECT_EQ(*device_host_name, "midnightshift");

    const auto* mac_addresses =
        signals.FindList(device_signals::names::kMacAddresses);
    ASSERT_TRUE(mac_addresses);
    EXPECT_EQ(*mac_addresses, GetExpectedMacAddresses());

    const auto* windows_machine_domain =
        signals.FindString(device_signals::names::kWindowsMachineDomain);
    ASSERT_TRUE(windows_machine_domain);
    EXPECT_EQ(*windows_machine_domain, "MACHINE_DOMAIN");

    const auto* windows_user_domain =
        signals.FindString(device_signals::names::kWindowsUserDomain);
    ASSERT_TRUE(windows_user_domain);
    EXPECT_EQ(*windows_user_domain, "USER_DOMAIN");

    auto secure_boot_enabled =
        signals.FindInt(device_signals::names::kSecureBootEnabled);
    ASSERT_TRUE(secure_boot_enabled);
    EXPECT_EQ(secure_boot_enabled.value(), kEnabledSetting);
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  policy::MockCloudPolicyStore mock_cloud_policy_store_;
  absl::optional<BrowserSignalsDecorator> decorator_;
};

TEST_F(BrowserSignalsDecoratorTest, Decorate_WithPolicyData) {
  SetFakePolicyData();

  base::RunLoop run_loop;

  base::Value::Dict signals;
  decorator_->Decorate(signals, run_loop.QuitClosure());

  run_loop.Run();

  ValidateStaticSignals(signals);

  EXPECT_EQ(
      kFakeEnrollmentDomain,
      *signals.FindString(device_signals::names::kDeviceEnrollmentDomain));

  histogram_tester_.ExpectTotalCount(kLatencyHistogram, 1);
}

TEST_F(BrowserSignalsDecoratorTest, Decorate_WithoutPolicyData) {
  base::RunLoop run_loop;

  base::Value::Dict signals;
  decorator_->Decorate(signals, run_loop.QuitClosure());

  run_loop.Run();

  ValidateStaticSignals(signals);
  EXPECT_FALSE(
      signals.contains(device_signals::names::kDeviceEnrollmentDomain));
}

}  // namespace enterprise_connectors

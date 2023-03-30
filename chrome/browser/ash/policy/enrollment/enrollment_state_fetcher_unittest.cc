// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/enrollment_state_fetcher.h"
#include <algorithm>
#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_test_support.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/system_clock/fake_system_clock_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

std::unique_ptr<EnrollmentStateFetcher::RlweClient> CreateRlweClientForTesting(
    const psm::testing::RlweTestCase& test_case,
    const private_membership::rlwe::RlwePlaintextId& plaintext_id) {
  auto client =
      private_membership::rlwe::PrivateMembershipRlweClient::CreateForTesting(
          private_membership::rlwe::RlweUseCase::CROS_DEVICE_STATE,
          {plaintext_id}, test_case.ec_cipher_key(), test_case.seed());
  return std::move(client.value());
}

}  // namespace

class EnrollmentStateFetcherTest : public testing::Test {
 public:
  void SetUp() override {
    psm_test_case_ = psm::testing::LoadTestCase(/*is_member=*/true);
    fake_dm_service_ =
        std::make_unique<FakeDeviceManagementService>(&job_creation_handler_);
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kEnterpriseEnableUnifiedStateDetermination,
        AutoEnrollmentTypeChecker::kUnifiedStateDeterminationAlways);
    system_clock_.SetNetworkSynchronized(true);

    DeviceCloudPolicyManagerAsh::RegisterPrefs(local_state_.registry());
    EnrollmentStateFetcher::RegisterPrefs(local_state_.registry());

    ash::system::StatisticsProvider::SetTestProvider(&statistics_provider_);
    statistics_provider_.SetMachineStatistic(
        ash::system::kSerialNumberKeyForTest, "fake-serial-number");
    statistics_provider_.SetMachineStatistic(ash::system::kRlzBrandCodeKey,
                                             "fake-brand-code");
  }

  AutoEnrollmentState FetchEnrollmentState() {
    base::test::TestFuture<AutoEnrollmentState> future;
    auto fetcher = EnrollmentStateFetcher::Create(
        future.GetCallback(), &local_state_,
        base::BindRepeating(&CreateRlweClientForTesting, psm_test_case_),
        fake_dm_service_.get(), shared_url_loader_factory_, &system_clock_);
    fetcher->Start();
    return future.Get();
  }

 protected:
  base::test::ScopedCommandLine command_line_;
  TestingPrefServiceSimple local_state_;
  ash::FakeSystemClockClient system_clock_;
  ash::system::FakeStatisticsProvider statistics_provider_;
  ash::ScopedStubInstallAttributes install_attributes_;
  psm::testing::RlweTestCase psm_test_case_;

  // Fake URL loader factories.
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  // Fake DMService.
  base::test::SingleThreadTaskEnvironment task_environment_;
  testing::StrictMock<MockJobCreationHandler> job_creation_handler_;
  std::unique_ptr<FakeDeviceManagementService> fake_dm_service_;
};

TEST_F(EnrollmentStateFetcherTest, RegisterPrefs) {
  TestingPrefServiceSimple local_state;
  auto* registry = local_state.registry();

  DeviceCloudPolicyManagerAsh::RegisterPrefs(registry);
  EnrollmentStateFetcher::RegisterPrefs(registry);

  const base::Value* value;
  auto defaults = registry->defaults();
  ASSERT_TRUE(defaults->GetValue(prefs::kServerBackedDeviceState, &value));
  EXPECT_TRUE(value->is_dict());
  EXPECT_TRUE(value->GetDict().empty());
  ASSERT_TRUE(defaults->GetValue(prefs::kEnrollmentPsmResult, &value));
  EXPECT_EQ(value->GetInt(), -1);
  ASSERT_TRUE(
      defaults->GetValue(prefs::kEnrollmentPsmDeterminationTime, &value));
  EXPECT_EQ(value->GetString(), "0");
}

TEST_F(EnrollmentStateFetcherTest, DisabledViaSwitches) {
  command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kEnterpriseEnableUnifiedStateDetermination,
      AutoEnrollmentTypeChecker::kUnifiedStateDeterminationNever);

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kNoEnrollment);
}

TEST_F(EnrollmentStateFetcherTest, SystemClockNotSyncronized) {
  system_clock_.DisableService();

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kConnectionError);
}

TEST_F(EnrollmentStateFetcherTest, EmbargoDateNotPassed) {
  base::Time::Exploded exploded;
  base::Time embargo_date = base::Time::Now() + base::Days(7);
  embargo_date.UTCExplode(&exploded);
  statistics_provider_.SetMachineStatistic(
      ash::system::kRlzEmbargoEndDateKey,
      base::StringPrintf("%04d-%02d-%02d", exploded.year, exploded.month,
                         exploded.day_of_month));

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kNoEnrollment);
}

TEST_F(EnrollmentStateFetcherTest, RlzBrandCodeMissing) {
  statistics_provider_.ClearMachineStatistic(ash::system::kRlzBrandCodeKey);

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kNoEnrollment);
}

TEST_F(EnrollmentStateFetcherTest, SerialNumberMissing) {
  statistics_provider_.ClearMachineStatistic(
      ash::system::kSerialNumberKeyForTest);

  AutoEnrollmentState state = FetchEnrollmentState();

  EXPECT_EQ(state, AutoEnrollmentState::kNoEnrollment);
}

// TODO(b/265923216): Write test for missing state keys.
// TODO(b/265923216): Write test for failed OPRF request.
// TODO(b/265923216): Write test for failed Query request.
// TODO(b/265923216): Write test for failed state request.
// TODO(b/265923216): Write test for successful state fetch.

}  // namespace policy

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_mode_resources_remover.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_mode_test_helper.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace chromeos {

namespace {

// Key for the pref in local state that tracks accumulated device usage time in
// seconds.
constexpr char kAccumulatedUsagePref[] =
    "demo_mode_resources_remover.accumulated_device_usage_s";

// Used as a callback to DemoModeResourcesRemover::AttemptRemoval - it records
// the result of the attempt to |result_out|.
void RecordRemovalResult(
    base::Optional<DemoModeResourcesRemover::RemovalResult>* result_out,
    DemoModeResourcesRemover::RemovalResult result) {
  *result_out = result;
}

}  // namespace

class DemoModeResourcesRemoverTest : public testing::Test {
 public:
  DemoModeResourcesRemoverTest() = default;
  ~DemoModeResourcesRemoverTest() override = default;

  void SetUp() override {
    install_attributes_ = std::make_unique<ScopedStubInstallAttributes>(
        CreateInstallAttributes());

    CryptohomeClient::InitializeFake();

    demo_mode_test_helper_ = std::make_unique<DemoModeTestHelper>();
    demo_resources_path_ =
        demo_mode_test_helper_->GetPreinstalledDemoResourcesPath();

    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<FakeChromeUserManager>());

    DemoModeResourcesRemover::RegisterLocalStatePrefs(local_state_.registry());
  }

  void TearDown() override {
    demo_mode_test_helper_.reset();
    CryptohomeClient::Shutdown();
  }

 protected:
  virtual std::unique_ptr<StubInstallAttributes> CreateInstallAttributes() {
    return StubInstallAttributes::CreateConsumerOwned();
  }

  bool CreateDemoModeResources() {
    if (!base::CreateDirectory(demo_resources_path_))
      return false;

    const std::string manifest = R"({
        "name": "demo-mode-resources",
        "version": "0.0.1",
        "min_env_version": "1.0"
    })";
    if (base::WriteFile(demo_resources_path_.AppendASCII("manifest.json"),
                        manifest.data(),
                        manifest.size()) != static_cast<int>(manifest.size())) {
      return false;
    }

    const std::string image = "fake image content";
    if (base::WriteFile(demo_resources_path_.AppendASCII("image.squash"),
                        image.data(),
                        image.size()) != static_cast<int>(image.size())) {
      return false;
    }

    return true;
  }

  bool DemoModeResourcesExist() {
    return base::DirectoryExists(demo_resources_path_);
  }

  enum class TestUserType {
    kRegular,
    kRegularSecond,
    kGuest,
    kPublicAccount,
    kKiosk,
    kDerelictDemoKiosk
  };

  void AddAndLogInUser(TestUserType type, DemoModeResourcesRemover* remover) {
    FakeChromeUserManager* user_manager =
        static_cast<FakeChromeUserManager*>(user_manager::UserManager::Get());
    user_manager::User* user = nullptr;
    switch (type) {
      case TestUserType::kRegular:
        user =
            user_manager->AddUser(AccountId::FromUserEmail("fake_user@test"));
        break;
      case TestUserType::kRegularSecond:
        user =
            user_manager->AddUser(AccountId::FromUserEmail("fake_user_1@test"));
        break;
      case TestUserType::kGuest:
        user = user_manager->AddGuestUser();
        break;
      case TestUserType::kPublicAccount:
        user = user_manager->AddPublicAccountUser(
            AccountId::FromUserEmail("fake_user@test"));
        break;
      case TestUserType::kKiosk:
        user = user_manager->AddKioskAppUser(
            AccountId::FromUserEmail("fake_user@test"));
        break;
      case TestUserType::kDerelictDemoKiosk:
        user = user_manager->AddKioskAppUser(user_manager::DemoAccountId());
        break;
    }

    ASSERT_TRUE(user);

    user_manager->LoginUser(user->GetAccountId());
    user_manager->SwitchActiveUser(user->GetAccountId());
    remover->ActiveUserChanged(user);
  }

  void AdvanceTestTime(const base::TimeDelta& time) {
    test_clock_.Advance(time);
    // TODO(tbarzic): Add support for injecting a test tick clock to
    // ui::ActivityDetector so activity_detector_ time gets updated by
    // test_clock_, too.
    activity_detector_.set_now_for_test(test_clock_.NowTicks());
  }

  TestingPrefServiceSimple local_state_;
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<DemoModeTestHelper> demo_mode_test_helper_;

  ui::UserActivityDetector activity_detector_;
  // Tick clock that can be used for tests - not used by default, but tests can
  // inject it into DemoModeResourcesRemover using OverrideTimeForTesting().
  base::SimpleTestTickClock test_clock_;

 private:
  std::unique_ptr<ScopedStubInstallAttributes> install_attributes_;

  base::FilePath demo_resources_path_;

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  DISALLOW_COPY_AND_ASSIGN(DemoModeResourcesRemoverTest);
};

class ManagedDemoModeResourcesRemoverTest
    : public DemoModeResourcesRemoverTest {
 public:
  ManagedDemoModeResourcesRemoverTest() = default;
  ~ManagedDemoModeResourcesRemoverTest() override = default;

  std::unique_ptr<StubInstallAttributes> CreateInstallAttributes() override {
    return StubInstallAttributes::CreateCloudManaged("test-domain",
                                                     "FAKE_DEVICE_ID");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagedDemoModeResourcesRemoverTest);
};

class DemoModeResourcesRemoverInLegacyDemoRetailModeTest
    : public DemoModeResourcesRemoverTest {
 public:
  DemoModeResourcesRemoverInLegacyDemoRetailModeTest() = default;
  ~DemoModeResourcesRemoverInLegacyDemoRetailModeTest() override = default;

  std::unique_ptr<StubInstallAttributes> CreateInstallAttributes() override {
    return StubInstallAttributes::CreateCloudManaged("us-retailmode.com",
                                                     "FAKE_DEVICE_ID");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DemoModeResourcesRemoverInLegacyDemoRetailModeTest);
};

TEST(LegacyDemoRetailModeDomainMatching, Matching) {
  EXPECT_TRUE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "us-retailmode.com"));
  EXPECT_TRUE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "us2-retailmode.com"));
  EXPECT_TRUE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "hr-retailmode.com"));
  EXPECT_TRUE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "uk-retailmode.com"));
  EXPECT_FALSE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "u1-retailmode.com"));
  EXPECT_FALSE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "uss-retailmode.com"));
  EXPECT_FALSE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "us4-retailmode.com"));
  EXPECT_FALSE(
      DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain("us-retailmode"));
  EXPECT_FALSE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "us-retailmode.com.foo"));
  EXPECT_FALSE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(""));
  EXPECT_FALSE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "fake-domain.com"));
  EXPECT_FALSE(DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      "us-us-retailmode.com"));
  EXPECT_FALSE(
      DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain("us.com"));
}

TEST_F(DemoModeResourcesRemoverTest, LowDiskSpace) {
  ASSERT_TRUE(CreateDemoModeResources());

  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());
  EXPECT_EQ(DemoModeResourcesRemover::Get(), remover.get());

  FakeCryptohomeClient::Get()->NotifyLowDiskSpace(1024 * 1024 * 1024);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverTest, LowDiskSpaceInDemoSession) {
  ASSERT_TRUE(CreateDemoModeResources());
  demo_mode_test_helper_->InitializeSession();

  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  EXPECT_FALSE(remover.get());
  EXPECT_FALSE(DemoModeResourcesRemover::Get());

  FakeCryptohomeClient::Get()->NotifyLowDiskSpace(1024 * 1024 * 1024);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverTest, NotCreatedAfterResourcesRemoved) {
  ASSERT_TRUE(CreateDemoModeResources());

  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());
  EXPECT_EQ(DemoModeResourcesRemover::Get(), remover.get());

  FakeCryptohomeClient::Get()->NotifyLowDiskSpace(1024 * 1024 * 1024);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(DemoModeResourcesExist());

  // Reset the resources remover - subsequent attempts to create the remover
  // instance should return nullptr.
  remover.reset();
  EXPECT_FALSE(DemoModeResourcesRemover::CreateIfNeeded(&local_state_));
}

TEST_F(DemoModeResourcesRemoverTest, AttemptRemoval) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());
  EXPECT_EQ(DemoModeResourcesRemover::Get(), remover.get());

  base::Optional<DemoModeResourcesRemover::RemovalResult> result;
  remover->AttemptRemoval(
      DemoModeResourcesRemover::RemovalReason::kEnterpriseEnrolled,
      base::BindOnce(&RecordRemovalResult, &result));

  task_environment_.RunUntilIdle();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(DemoModeResourcesRemover::RemovalResult::kSuccess, result.value());
  EXPECT_FALSE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverTest, AttemptRemovalResourcesNonExistent) {
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());
  EXPECT_EQ(DemoModeResourcesRemover::Get(), remover.get());

  base::Optional<DemoModeResourcesRemover::RemovalResult> result;
  remover->AttemptRemoval(
      DemoModeResourcesRemover::RemovalReason::kLowDiskSpace,
      base::BindOnce(&RecordRemovalResult, &result));

  task_environment_.RunUntilIdle();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(DemoModeResourcesRemover::RemovalResult::kNotFound, result.value());
}

TEST_F(DemoModeResourcesRemoverTest, AttemptRemovalInDemoSession) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  demo_mode_test_helper_->InitializeSession();

  base::Optional<DemoModeResourcesRemover::RemovalResult> result;
  remover->AttemptRemoval(
      DemoModeResourcesRemover::RemovalReason::kLowDiskSpace,
      base::BindOnce(&RecordRemovalResult, &result));

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(DemoModeResourcesRemover::RemovalResult::kNotAllowed,
            result.value());
  EXPECT_TRUE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverTest, ConcurrentRemovalAttempts) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());
  EXPECT_EQ(DemoModeResourcesRemover::Get(), remover.get());

  base::Optional<DemoModeResourcesRemover::RemovalResult> result_1;
  remover->AttemptRemoval(
      DemoModeResourcesRemover::RemovalReason::kLowDiskSpace,
      base::BindOnce(&RecordRemovalResult, &result_1));

  base::Optional<DemoModeResourcesRemover::RemovalResult> result_2;
  remover->AttemptRemoval(
      DemoModeResourcesRemover::RemovalReason::kLowDiskSpace,
      base::BindOnce(&RecordRemovalResult, &result_2));

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(DemoModeResourcesExist());
  ASSERT_TRUE(result_1.has_value());
  EXPECT_EQ(DemoModeResourcesRemover::RemovalResult::kSuccess,
            result_1.value());

  ASSERT_TRUE(result_2.has_value());
  EXPECT_EQ(DemoModeResourcesRemover::RemovalResult::kSuccess,
            result_2.value());
}

TEST_F(DemoModeResourcesRemoverTest, RepeatedRemovalAttempt) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());
  remover->AttemptRemoval(
      DemoModeResourcesRemover::RemovalReason::kLowDiskSpace,
      DemoModeResourcesRemover::RemovalCallback());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(DemoModeResourcesExist());

  base::Optional<DemoModeResourcesRemover::RemovalResult> result;
  remover->AttemptRemoval(
      DemoModeResourcesRemover::RemovalReason::kLowDiskSpace,
      base::BindOnce(&RecordRemovalResult, &result));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(DemoModeResourcesRemover::RemovalResult::kAlreadyRemoved,
            result.value());
}

TEST_F(DemoModeResourcesRemoverTest, NoRemovalOnLogin) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AddAndLogInUser(TestUserType::kRegular, remover.get());

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverTest, RemoveAfterActiveUse) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AdvanceTestTime(base::TimeDelta::FromMinutes(1));

  remover->OverrideTimeForTesting(
      &test_clock_,
      DemoModeResourcesRemover::UsageAccumulationConfig(
          base::TimeDelta::FromSeconds(3) /*resources_removal_threshold*/,
          base::TimeDelta::FromSeconds(1) /*update_interval*/,
          base::TimeDelta::FromSeconds(9) /*idle_threshold*/));

  AddAndLogInUser(TestUserType::kRegular, remover.get());
  activity_detector_.HandleExternalUserActivity();

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());

  // Advance time so it's longer than removal threshold, but under the idle
  // threshold (so it's not disregarded as idle time).
  AdvanceTestTime(base::TimeDelta::FromSeconds(4));
  activity_detector_.HandleExternalUserActivity();

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverTest, IgnoreUsageBeforeLogin) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AdvanceTestTime(base::TimeDelta::FromMinutes(1));

  remover->OverrideTimeForTesting(
      &test_clock_,
      DemoModeResourcesRemover::UsageAccumulationConfig(
          base::TimeDelta::FromSeconds(3) /*resources_removal_threshold*/,
          base::TimeDelta::FromSeconds(1) /*update_interval*/,
          base::TimeDelta::FromSeconds(9) /*idle_threshold*/));

  activity_detector_.HandleExternalUserActivity();

  // Advance time so it's longer than removal threshold, but under the idle
  // threshold (so it's not disregarded as idle time).
  AdvanceTestTime(base::TimeDelta::FromSeconds(4));
  activity_detector_.HandleExternalUserActivity();

  AddAndLogInUser(TestUserType::kRegular, remover.get());

  // The total usage was over the removal threshold, but it happened before
  // login - the resources should still be around.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverTest, RemoveAfterActiveUse_AccumulateActivity) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AdvanceTestTime(base::TimeDelta::FromMinutes(1));

  remover->OverrideTimeForTesting(
      &test_clock_,
      DemoModeResourcesRemover::UsageAccumulationConfig(
          base::TimeDelta::FromSeconds(3) /*resources_removal_threshold*/,
          base::TimeDelta::FromSeconds(1) /*update_interval*/,
          base::TimeDelta::FromSeconds(9) /*idle_threshold*/));

  AddAndLogInUser(TestUserType::kRegular, remover.get());
  activity_detector_.HandleExternalUserActivity();

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());

  // Over update interval, but under removal threshold.
  AdvanceTestTime(base::TimeDelta::FromSeconds(2));
  activity_detector_.HandleExternalUserActivity();

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());

  // This should get accumulated time over removal threshold.
  AdvanceTestTime(base::TimeDelta::FromSeconds(2));
  activity_detector_.HandleExternalUserActivity();

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverTest, DoNotAccumulateIdleTimeUsage) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AdvanceTestTime(base::TimeDelta::FromMinutes(1));

  remover->OverrideTimeForTesting(
      &test_clock_,
      DemoModeResourcesRemover::UsageAccumulationConfig(
          base::TimeDelta::FromSeconds(8) /*resources_removal_threshold*/,
          base::TimeDelta::FromSeconds(3) /*update_interval*/,
          base::TimeDelta::FromSeconds(4) /*idle_threshold*/));

  AddAndLogInUser(TestUserType::kRegular, remover.get());
  activity_detector_.HandleExternalUserActivity();

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());

  // Advance to the time just under removal threshold in small increments
  // (within the idle threshold)
  AdvanceTestTime(base::TimeDelta::FromSeconds(3));
  activity_detector_.HandleExternalUserActivity();
  AdvanceTestTime(base::TimeDelta::FromSeconds(3));
  activity_detector_.HandleExternalUserActivity();

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());

  // Simulate longer idle period.
  AdvanceTestTime(base::TimeDelta::FromSeconds(10));
  activity_detector_.HandleExternalUserActivity();

  // The resources should be still be here, as usage amount should not have been
  // incremented.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());

  // Advance time little bit more, so it's over the removal threshold (and over
  // the update interval).
  AdvanceTestTime(base::TimeDelta::FromSeconds(3));
  activity_detector_.HandleExternalUserActivity();

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverTest, ReportUsageBeforeIdlePeriod) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AdvanceTestTime(base::TimeDelta::FromMinutes(1));

  remover->OverrideTimeForTesting(
      &test_clock_,
      DemoModeResourcesRemover::UsageAccumulationConfig(
          base::TimeDelta::FromSeconds(12) /*resources_removal_threshold*/,
          base::TimeDelta::FromSeconds(7) /*update_interval*/,
          base::TimeDelta::FromSeconds(5) /*idle_threshold*/));

  AddAndLogInUser(TestUserType::kRegular, remover.get());
  activity_detector_.HandleExternalUserActivity();

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());

  // Advance to the time just under removal threshold in small increments
  // (within the idle threshold), that are under the update interval combined.
  // This will leave unrecorded usage before the idle period.
  AdvanceTestTime(base::TimeDelta::FromSeconds(3));
  activity_detector_.HandleExternalUserActivity();
  AdvanceTestTime(base::TimeDelta::FromSeconds(3));
  activity_detector_.HandleExternalUserActivity();

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());

  // Simulate longer idle period.
  AdvanceTestTime(base::TimeDelta::FromSeconds(10));
  activity_detector_.HandleExternalUserActivity();

  // The resources should be still be here, as usage amount should not have been
  // incremented.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());

  // Advance time cummulatively over the update period.
  AdvanceTestTime(base::TimeDelta::FromSeconds(4));
  activity_detector_.HandleExternalUserActivity();
  AdvanceTestTime(base::TimeDelta::FromSeconds(3));
  activity_detector_.HandleExternalUserActivity();

  // When combined the accumulated active usage was above the removal threshold.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverTest, RemovalThresholdReachedBeforeIdlePeriod) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AdvanceTestTime(base::TimeDelta::FromMinutes(1));

  remover->OverrideTimeForTesting(
      &test_clock_,
      DemoModeResourcesRemover::UsageAccumulationConfig(
          base::TimeDelta::FromSeconds(9) /*resources_removal_threshold*/,
          base::TimeDelta::FromSeconds(5) /*update_interval*/,
          base::TimeDelta::FromSeconds(7) /*idle_threshold*/));

  AddAndLogInUser(TestUserType::kRegular, remover.get());
  activity_detector_.HandleExternalUserActivity();

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());

  // Advance to the time just under removal threshold in small increments, but
  // with total over the update interval.
  AdvanceTestTime(base::TimeDelta::FromSeconds(3));
  activity_detector_.HandleExternalUserActivity();
  AdvanceTestTime(base::TimeDelta::FromSeconds(3));
  activity_detector_.HandleExternalUserActivity();

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());

  // Advance time so total is over the remova threshold, but in increment under
  // the update interval.
  AdvanceTestTime(base::TimeDelta::FromSeconds(3));
  activity_detector_.HandleExternalUserActivity();

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());

  // Simulate longer idle period.
  AdvanceTestTime(base::TimeDelta::FromSeconds(10));
  activity_detector_.HandleExternalUserActivity();

  // Activity after the idle period ended should have flushed previous pending
  // usage, and the resources should have been removed.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverTest, UpdateInterval) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AdvanceTestTime(base::TimeDelta::FromMinutes(1));

  remover->OverrideTimeForTesting(
      &test_clock_,
      DemoModeResourcesRemover::UsageAccumulationConfig(
          base::TimeDelta::FromSeconds(3) /*resources_removal_threshold*/,
          base::TimeDelta::FromSeconds(1) /*update_interval*/,
          base::TimeDelta::FromSeconds(9) /*idle_threshold*/));

  AddAndLogInUser(TestUserType::kRegular, remover.get());

  // Test that local state is not updated on each detected user activity.
  AdvanceTestTime(base::TimeDelta::FromMilliseconds(300));
  activity_detector_.HandleExternalUserActivity();
  EXPECT_EQ(0, local_state_.GetInteger(kAccumulatedUsagePref));

  AdvanceTestTime(base::TimeDelta::FromMilliseconds(300));
  activity_detector_.HandleExternalUserActivity();
  EXPECT_EQ(0, local_state_.GetInteger(kAccumulatedUsagePref));

  AdvanceTestTime(base::TimeDelta::FromMilliseconds(300));
  activity_detector_.HandleExternalUserActivity();
  EXPECT_EQ(0, local_state_.GetInteger(kAccumulatedUsagePref));

  AdvanceTestTime(base::TimeDelta::FromMilliseconds(300));
  activity_detector_.HandleExternalUserActivity();
  EXPECT_EQ(1, local_state_.GetInteger(kAccumulatedUsagePref));
}
TEST_F(DemoModeResourcesRemoverTest,
       RemoveAfterActiveUse_AccumulateActivityOverRestarts) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AdvanceTestTime(base::TimeDelta::FromMinutes(1));

  remover->OverrideTimeForTesting(
      &test_clock_,
      DemoModeResourcesRemover::UsageAccumulationConfig(
          base::TimeDelta::FromSeconds(3) /*resources_removal_threshold*/,
          base::TimeDelta::FromSeconds(1) /*update_interval*/,
          base::TimeDelta::FromSeconds(9) /*idle_threshold*/));

  AddAndLogInUser(TestUserType::kRegular, remover.get());
  activity_detector_.HandleExternalUserActivity();

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());

  // Over update interval, but under removal threshold.
  AdvanceTestTime(base::TimeDelta::FromSeconds(2));
  activity_detector_.HandleExternalUserActivity();

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());

  remover.reset();
  remover = DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());
  remover->OverrideTimeForTesting(
      &test_clock_,
      DemoModeResourcesRemover::UsageAccumulationConfig(
          base::TimeDelta::FromSeconds(3) /*resources_removal_threshold*/,
          base::TimeDelta::FromSeconds(1) /*update_interval*/,
          base::TimeDelta::FromSeconds(9) /*idle_threshold*/));
  AddAndLogInUser(TestUserType::kRegularSecond, remover.get());

  // This should get accumulated time over removal threshold.
  AdvanceTestTime(base::TimeDelta::FromSeconds(2));
  activity_detector_.HandleExternalUserActivity();

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverTest,
       RemoveAfterActiveUse_RecordLeftoverUsageOnShutdown) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AdvanceTestTime(base::TimeDelta::FromMinutes(1));

  remover->OverrideTimeForTesting(
      &test_clock_,
      DemoModeResourcesRemover::UsageAccumulationConfig(
          base::TimeDelta::FromSeconds(4) /*resources_removal_threshold*/,
          base::TimeDelta::FromSeconds(2) /*update_interval*/,
          base::TimeDelta::FromSeconds(9) /*idle_threshold*/));

  AddAndLogInUser(TestUserType::kRegular, remover.get());
  activity_detector_.HandleExternalUserActivity();

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());

  // Over update interval, but under removal threshold.
  AdvanceTestTime(base::TimeDelta::FromSeconds(3));
  activity_detector_.HandleExternalUserActivity();

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());

  // This is under update interval, but should get accumulated time over
  // removal threshold.
  AdvanceTestTime(base::TimeDelta::FromSeconds(1));
  activity_detector_.HandleExternalUserActivity();

  remover.reset();

  // Session restart on with usage already over threshold - expect resources
  // removal.
  remover = DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  remover->OverrideTimeForTesting(
      &test_clock_,
      DemoModeResourcesRemover::UsageAccumulationConfig(
          base::TimeDelta::FromSeconds(4) /*resources_removal_threshold*/,
          base::TimeDelta::FromSeconds(2) /*update_interval*/,
          base::TimeDelta::FromSeconds(9) /*idle_threshold*/));
  AddAndLogInUser(TestUserType::kRegular, remover.get());

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(DemoModeResourcesExist());
}

// Tests the kiosk app incarnation of demo mode.
TEST_F(DemoModeResourcesRemoverTest, NoRemovalInKioskDemoMode) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AddAndLogInUser(TestUserType::kDerelictDemoKiosk, remover.get());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverInLegacyDemoRetailModeTest,
       NoRemovalInKioskDemoModeWithUserActivity) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AdvanceTestTime(base::TimeDelta::FromMinutes(1));

  remover->OverrideTimeForTesting(
      &test_clock_,
      DemoModeResourcesRemover::UsageAccumulationConfig(
          base::TimeDelta::FromSeconds(4) /*resources_removal_threshold*/,
          base::TimeDelta::FromSeconds(2) /*update_interval*/,
          base::TimeDelta::FromSeconds(9) /*idle_threshold*/));

  AddAndLogInUser(TestUserType::kDerelictDemoKiosk, remover.get());

  AdvanceTestTime(base::TimeDelta::FromSeconds(5));

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());
}

TEST_F(ManagedDemoModeResourcesRemoverTest, RemoveOnRegularLogin) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AddAndLogInUser(TestUserType::kRegular, remover.get());

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(DemoModeResourcesExist());
}

TEST_F(ManagedDemoModeResourcesRemoverTest, NoRemovalGuestLogin) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AddAndLogInUser(TestUserType::kGuest, remover.get());

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(DemoModeResourcesExist());
}

TEST_F(ManagedDemoModeResourcesRemoverTest, RemoveOnLowDiskInGuest) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AddAndLogInUser(TestUserType::kGuest, remover.get());
  FakeCryptohomeClient::Get()->NotifyLowDiskSpace(1024 * 1024 * 1024);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(DemoModeResourcesExist());
}

TEST_F(ManagedDemoModeResourcesRemoverTest, RemoveOnPublicSessionLogin) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AddAndLogInUser(TestUserType::kPublicAccount, remover.get());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(DemoModeResourcesExist());
}

TEST_F(ManagedDemoModeResourcesRemoverTest, RemoveInKioskSession) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AddAndLogInUser(TestUserType::kKiosk, remover.get());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverInLegacyDemoRetailModeTest, NoRemovalOnLogin) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AddAndLogInUser(TestUserType::kPublicAccount, remover.get());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverInLegacyDemoRetailModeTest,
       RemoveOnLowDiskSpace) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AddAndLogInUser(TestUserType::kPublicAccount, remover.get());
  FakeCryptohomeClient::Get()->NotifyLowDiskSpace(1024 * 1024 * 1024);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(DemoModeResourcesExist());
}

TEST_F(DemoModeResourcesRemoverInLegacyDemoRetailModeTest,
       ActiveUsageShouldNotTriggerRemoval) {
  ASSERT_TRUE(CreateDemoModeResources());
  std::unique_ptr<DemoModeResourcesRemover> remover =
      DemoModeResourcesRemover::CreateIfNeeded(&local_state_);
  ASSERT_TRUE(remover.get());

  AdvanceTestTime(base::TimeDelta::FromMinutes(1));

  remover->OverrideTimeForTesting(
      &test_clock_,
      DemoModeResourcesRemover::UsageAccumulationConfig(
          base::TimeDelta::FromSeconds(4) /*resources_removal_threshold*/,
          base::TimeDelta::FromSeconds(2) /*update_interval*/,
          base::TimeDelta::FromSeconds(9) /*idle_threshold*/));

  AddAndLogInUser(TestUserType::kPublicAccount, remover.get());

  AdvanceTestTime(base::TimeDelta::FromSeconds(5));

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(DemoModeResourcesExist());
}

}  // namespace chromeos

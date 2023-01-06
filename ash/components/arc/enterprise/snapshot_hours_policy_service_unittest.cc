// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/enterprise/snapshot_hours_policy_service.h"

#include <memory>
#include <string>

#include "ash/components/arc/arc_prefs.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace arc {
namespace data_snapshotd {

namespace {

constexpr char kPublicAccountEmail[] = "public-session-account@localhost";
constexpr char kUserAccountEmail[] = "regular-user-account@localhost";

// DeviceArcDataSnapshotHours policy with one correct interval.
constexpr char kJsonPolicy[] =
    "{"
    "\"intervals\": ["
    "{"
    "\"start\": {"
    "\"day_of_week\": \"MONDAY\","
    "\"time\": 1284000"
    "},"
    "\"end\": {"
    "\"day_of_week\": \"MONDAY\","
    "\"time\": 21720000"
    "}"
    "}"
    "],"
    "\"timezone\": \"GMT\""
    "}";

// DeviceArcDataSnapshotHours correct policy with missing timezone.
constexpr char kJsonPolicyNoTimezone[] =
    "{"
    "\"intervals\": ["
    "{"
    "\"start\": {"
    "\"day_of_week\": \"MONDAY\","
    "\"time\": 1284000"
    "},"
    "\"end\": {"
    "\"day_of_week\": \"MONDAY\","
    "\"time\": 21720000"
    "}"
    "}"
    "]"
    "}";

// DeviceArcDataSnapshotHours incorrect policy with incorrect intervals.
constexpr char kJsonPolicyIncorrectIntervals[] =
    "{"
    "\"intervals\": ["
    "{"
    "\"start\": {"
    "\"day_of_week\": \"MONDAY\","
    "\"time\": 1284000"
    "},"
    "\"end\": {"
    "\"day_of_week\": \"UNSPECIFIED\","
    "\"time\": 21720000"
    "}"
    "}"
    "],"
    "\"timezone\": \"GMT\""
    "}";

// DeviceArcDataSnapshotHours incorrect policy with missing intervals.
constexpr char kJsonPolicyNoIntervals[] =
    "{"
    "\"timezone\": \"GMT\""
    "}";

// DeviceArcDataSnapshotHours incorrect policy with empty intervals.
constexpr char kJsonPolicyEmptyIntervals[] =
    "{"
    "\"intervals\": ["
    "],"
    "\"timezone\": \"GMT\""
    "}";

// DeviceArcDataSnapshotHours correct policy with UNSET timezone.
constexpr char kJsonPolicyUnsetTimezone[] =
    "{"
    "\"intervals\": ["
    "{"
    "\"start\": {"
    "\"day_of_week\": \"MONDAY\","
    "\"time\": 1284000"
    "},"
    "\"end\": {"
    "\"day_of_week\": \"MONDAY\","
    "\"time\": 21720000"
    "}"
    "}"
    "],"
    "\"timezone\": \"UNSET\""
    "}";

class FakeObserver : public SnapshotHoursPolicyService::Observer {
 public:
  FakeObserver() = default;
  FakeObserver(const FakeObserver&) = delete;
  FakeObserver& operator=(const FakeObserver&) = delete;
  ~FakeObserver() override = default;

  void OnSnapshotsDisabled() override { disabled_calls_num_++; }

  void OnSnapshotsEnabled() override { enabled_calls_num_++; }

  void OnSnapshotUpdateEndTimeChanged() override { changed_calls_num_++; }

  int disabled_calls_num() const { return disabled_calls_num_; }
  int enabled_calls_num() const { return enabled_calls_num_; }
  int changed_calls_num() const { return changed_calls_num_; }

 private:
  int disabled_calls_num_ = 0;
  int enabled_calls_num_ = 0;
  int changed_calls_num_ = 0;
};

}  // namespace

// Tests SnapshotHoursPolicyService class instance.
class SnapshotHoursPolicyServiceTest
    : public testing::TestWithParam<std::string> {
 protected:
  SnapshotHoursPolicyServiceTest() = default;

  void SetUp() override {
    arc::prefs::RegisterLocalStatePrefs(local_state_.registry());

    fake_user_manager_ = new user_manager::FakeUserManager();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager_));
    fake_user_manager_->set_local_state(local_state());

    policy_service_ =
        std::make_unique<SnapshotHoursPolicyService>(local_state());
    observer_ = std::make_unique<FakeObserver>();
    policy_service()->AddObserver(observer_.get());
  }

  void TearDown() override {
    policy_service()->RemoveObserver(observer_.get());
    policy_service_.reset();
    local_state_.ClearPref(prefs::kArcSnapshotHours);
  }

  const std::string& policy() const { return GetParam(); }

  // Ensure the feature disabled.
  void EnsureSnapshotDisabled(int disabled_calls_num = 0) {
    EXPECT_FALSE(policy_service()->is_snapshot_enabled());
    EXPECT_FALSE(policy_service()->get_timer_for_testing()->IsRunning());
    EXPECT_TRUE(policy_service()->snapshot_update_end_time().is_null());
    EXPECT_EQ(observer_->disabled_calls_num(), disabled_calls_num);
  }

  // Ensure the feature enabled.
  void EnsureSnapshotEnabled(int enabled_calls_num = 1) {
    EXPECT_TRUE(policy_service()->is_snapshot_enabled());
    EXPECT_TRUE(policy_service()->get_timer_for_testing()->IsRunning());

    EXPECT_EQ(policy_service()->get_intervals_for_testing().size(), 1u);
    EXPECT_EQ(observer_->enabled_calls_num(), enabled_calls_num);
  }

  // Enable feature and check.
  void EnableSnapshot(int enabled_calls_num = 1,
                      const std::string& policyJson = kJsonPolicy) {
    auto account_id = AccountId::FromUserEmail(kPublicAccountEmail);
    EXPECT_TRUE(fake_user_manager_->AddPublicAccountUser(account_id));
    policy_service()->LocalStateChanged(user_manager());

    absl::optional<base::Value> policy = base::JSONReader::Read(policyJson);
    EXPECT_TRUE(policy.has_value());
    local_state()->Set(arc::prefs::kArcSnapshotHours, policy.value());

    EnsureSnapshotEnabled(enabled_calls_num);
  }

  // Fire the next timer.
  void FastForwardToTimer() {
    if (policy_service()->snapshot_update_end_time().is_null()) {
      task_environment_.FastForwardBy(
          policy_service()->get_timer_for_testing()->desired_run_time() -
          base::Time::Now());
      task_environment_.RunUntilIdle();
    }
    EXPECT_FALSE(policy_service()->snapshot_update_end_time().is_null());
  }

  void LoginAsPublicSession() {
    auto account_id = AccountId::FromUserEmail(kPublicAccountEmail);
    const auto* user = fake_user_manager_->AddPublicAccountUser(account_id);
    user_manager()->UserLoggedIn(account_id, user->username_hash(), false,
                                 false);
    user_manager()->SwitchActiveUser(account_id);
    EXPECT_EQ(user_manager()->GetActiveUser()->GetAccountId(), account_id);
    policy_service()->LocalStateChanged(user_manager());
  }

  void RemovePublicSession() {
    auto account_id = AccountId::FromUserEmail(kPublicAccountEmail);
    user_manager()->RemoveUserFromList(account_id);
    policy_service()->LocalStateChanged(user_manager());
  }

  void LoginAsRegularUser() {
    auto account_id = AccountId::FromUserEmail(kUserAccountEmail);
    const auto* user = user_manager()->AddUser(account_id);
    user_manager()->UserLoggedIn(account_id, user->username_hash(), false,
                                 false);
    user_manager()->SwitchActiveUser(account_id);
    EXPECT_EQ(user_manager()->GetActiveUser()->GetAccountId(), account_id);
    policy_service()->LocalStateChanged(user_manager());
  }

  SnapshotHoursPolicyService* policy_service() { return policy_service_.get(); }
  TestingPrefServiceSimple* local_state() { return &local_state_; }
  FakeObserver* observer() { return observer_.get(); }
  user_manager::FakeUserManager* user_manager() { return fake_user_manager_; }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<FakeObserver> observer_;
  std::unique_ptr<SnapshotHoursPolicyService> policy_service_;
  TestingPrefServiceSimple local_state_;
  user_manager::FakeUserManager* fake_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

class SnapshotHoursPolicyServiceDisabledTest
    : public SnapshotHoursPolicyServiceTest {};

// Test that the feature is disabled by default.
TEST_F(SnapshotHoursPolicyServiceTest, Disabled) {
  EnsureSnapshotDisabled();
}

// Test that the feature is disabled if MGS is not configured.
TEST_F(SnapshotHoursPolicyServiceTest, MgsIsNotConfigured) {
  EnableSnapshot();

  RemovePublicSession();

  EnsureSnapshotDisabled(1 /* disabled_calls_num */);
}

TEST_F(SnapshotHoursPolicyServiceTest, DoubleDisable) {
  EnableSnapshot();

  {
    absl::optional<base::Value> policy_value =
        base::JSONReader::Read(kJsonPolicyEmptyIntervals);
    EXPECT_TRUE(policy_value.has_value());
    local_state()->Set(arc::prefs::kArcSnapshotHours, policy_value.value());

    EnsureSnapshotDisabled(1 /* disabled_calls_num */);
  }

  {
    // User a different JSON to ensure the policy value is updated.
    absl::optional<base::Value> policy_value =
        base::JSONReader::Read(kJsonPolicyNoIntervals);
    EXPECT_TRUE(policy_value.has_value());
    local_state()->Set(arc::prefs::kArcSnapshotHours, policy_value.value());

    // Do not notify the second time.
    EnsureSnapshotDisabled(1 /* disabled_calls_num */);
  }
}

TEST_F(SnapshotHoursPolicyServiceTest, DoubleEnable) {
  EnableSnapshot();
  // Do not notify the second time.
  EnableSnapshot();
}

// Test that once the feature enabled, the time is outside interval.
TEST_F(SnapshotHoursPolicyServiceTest, OutsideInterval) {
  EnableSnapshot();
  EXPECT_TRUE(policy_service()->snapshot_update_end_time().is_null());
  EXPECT_EQ(observer()->changed_calls_num(), 0);

  FastForwardToTimer();
  EXPECT_EQ(observer()->changed_calls_num(), 1);
  EXPECT_FALSE(policy_service()->snapshot_update_end_time().is_null());
}

// Test that once the feature enabled, the time is outside interval.
TEST_F(SnapshotHoursPolicyServiceTest, InsideInterval) {
  EnableSnapshot();
  EXPECT_TRUE(policy_service()->snapshot_update_end_time().is_null());
  EXPECT_EQ(observer()->changed_calls_num(), 0);

  FastForwardToTimer();
  EXPECT_EQ(observer()->changed_calls_num(), 1);
  EXPECT_FALSE(policy_service()->snapshot_update_end_time().is_null());

  // Disable snapshots.
  {
    absl::optional<base::Value> policy_value =
        base::JSONReader::Read(kJsonPolicyNoIntervals);
    EXPECT_TRUE(policy_value.has_value());
    local_state()->Set(arc::prefs::kArcSnapshotHours, policy_value.value());

    EnsureSnapshotDisabled(1 /* disabled_calls_num */);
    EXPECT_EQ(observer()->changed_calls_num(), 2);
  }

  EnableSnapshot(2 /* enabled_calls_num */);
  EXPECT_EQ(observer()->changed_calls_num(), 3);
  EXPECT_FALSE(policy_service()->snapshot_update_end_time().is_null());
}

// Test that if ARC is disabled by user policy (not for public session
// account), it does not disable the feature.
TEST_F(SnapshotHoursPolicyServiceTest, DisableByUserPolicyForUser) {
  EnableSnapshot();

  LoginAsRegularUser();
  TestingPrefServiceSimple profile_prefs;
  arc::prefs::RegisterProfilePrefs(profile_prefs.registry());
  profile_prefs.SetBoolean(arc::prefs::kArcEnabled, false);
  policy_service()->StartObservingPrimaryProfilePrefs(&profile_prefs);
  EnsureSnapshotEnabled();
  policy_service()->StopObservingPrimaryProfilePrefs();
}

// Test that if ARC is disabled for public session account, it disables
// the feature.
TEST_F(SnapshotHoursPolicyServiceTest, DisableByUserPolicyForMGS) {
  LoginAsPublicSession();
  EnableSnapshot();

  TestingPrefServiceSimple profile_prefs;
  arc::prefs::RegisterProfilePrefs(profile_prefs.registry());
  profile_prefs.SetBoolean(arc::prefs::kArcEnabled, false);
  policy_service()->StartObservingPrimaryProfilePrefs(&profile_prefs);
  EnsureSnapshotDisabled(1 /* disabled_calls_num */);
  policy_service()->StopObservingPrimaryProfilePrefs();
  EnsureSnapshotEnabled(2 /* enabled_calls_num */);
}

TEST_P(SnapshotHoursPolicyServiceTest, OneIntervalEnabled) {
  EnableSnapshot(1 /* enabled_calls_num */, policy());
}

TEST_P(SnapshotHoursPolicyServiceDisabledTest, DisabledByPolicy) {
  EnableSnapshot();

  absl::optional<base::Value> policy_value = base::JSONReader::Read(policy());
  EXPECT_TRUE(policy_value.has_value());
  local_state()->Set(arc::prefs::kArcSnapshotHours, policy_value.value());

  EnsureSnapshotDisabled(1 /* disabled_calls_num */);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SnapshotHoursPolicyServiceDisabledTest,
    testing::Values(kJsonPolicyIncorrectIntervals,
                    kJsonPolicyNoIntervals,
                    kJsonPolicyEmptyIntervals));
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SnapshotHoursPolicyServiceTest,
    testing::Values(kJsonPolicyNoTimezone,
                    kJsonPolicy,
                    kJsonPolicyUnsetTimezone));

}  // namespace data_snapshotd
}  // namespace arc

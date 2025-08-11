// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/sample_map.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/invalidation/test_support/fake_invalidation_listener.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_invalidation_scope.h"
#include "components/policy/core/common/cloud/policy_invalidation_util.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::WithArg;

namespace em = enterprise_management;

namespace policy {

namespace {

// Fake policy values which are alternated to cause the store to report a
// changed policy.
constexpr char kPolicyValueA[] = "policyValueA";
constexpr char kPolicyValueB[] = "policyValueB";

constexpr char kDeviceLocalAccountId[] = "test_account";

}  // namespace

class CloudPolicyInvalidatorTestBase : public testing::Test {
 protected:
  CloudPolicyInvalidatorTestBase();
  ~CloudPolicyInvalidatorTestBase() override;

  // Starts the invalidator which will be tested.
  // `highest_handled_invalidation_version` is the highest invalidation version
  // that was handled already before this invalidator was created.
  void StartInvalidator(int64_t highest_handled_invalidation_version = 0);
  // Destroys the invalidator.
  void DestroyInvalidator();

  const CloudPolicyInvalidator* invalidator() const {
    return invalidator_.get();
  }

  // Connects the cloud policy core.
  void ConnectCore();

  // Starts the refresh scheduler.
  void StartRefreshScheduler();

  // Disconnects the cloud policy core.
  void DisconnectCore();

  // Simulates storing a new policy to the policy store.
  // `policy_changed` determines whether a policy value different from the
  // current value will be stored.
  // `time` determines the timestamp the store will report.
  void StorePolicy(int64_t invalidation_version,
                   bool policy_changed,
                   const base::Time& time);
  void StorePolicy(int64_t invalidation_version, bool policy_changed) {
    StorePolicy(invalidation_version, policy_changed, Now() - base::Minutes(5));
  }
  void StorePolicy(int64_t invalidation_version) {
    StorePolicy(invalidation_version, false);
  }
  void StorePolicy() { StorePolicy(0); }

  // Disables the invalidation service. It is disabled by default.
  void DisableInvalidationListener();

  // Enables the invalidation service. It is disabled by default.
  void EnableInvalidationListener();

  // Causes the invalidation service to fire an invalidation.
  invalidation::DirectInvalidation FireInvalidation(int64_t version,
                                                    const std::string& payload);

  // Returns true if the invalidation info of the `core_`'s client matches the
  // passed invalidation's version and payload.
  bool ClientInvalidationInfoMatches(
      const invalidation::DirectInvalidation& invalidation);

  // Returns true if the invalidation info of the `core_`'s client is unset.
  bool ClientInvalidationInfoIsUnset();

  // Checks that the policy was not refreshed due to an invalidation.
  bool CheckPolicyNotRefreshed();

  // Checks that the policy was refreshed due to an invalidation with the given
  // base delay.
  bool CheckPolicyRefreshed(base::TimeDelta delay = base::TimeDelta());

  // Returns the invalidations enabled state set by the invalidator on the
  // refresh scheduler.
  bool InvalidationsEnabled();

  // Determines if the invalidator has registered as an observer with the
  // invalidation service.
  bool IsInvalidatorRegistered();

  // Advance the test clock.
  void AdvanceClock(base::TimeDelta delta);

  // Get the current time on the test clock.
  base::Time Now();

  // Translate a version number into an appropriate invalidation version (which
  // is based on the current time).
  int64_t V(int version);

  // Get an invalidation version for the given time.
  int64_t GetVersion(base::Time time);

  // Get the invalidation scope that the `invalidator_` is responsible for.
  virtual PolicyInvalidationScope GetPolicyInvalidationScope() const;
  std::string GetPolicyInvalidationType() const;

 private:
  // Checks that the policy was refreshed the given number of times.
  bool CheckPolicyRefreshCount(int count);

  base::test::SingleThreadTaskEnvironment task_environment_;

  // Objects the invalidator depends on.
  MockCloudPolicyStore store_;
  CloudPolicyCore core_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SimpleTestClock clock_;

  invalidation::FakeInvalidationListener invalidation_listener_;

  // The currently used policy value.
  std::string policy_value_cur_;

  // The invalidator which will be tested.
  std::unique_ptr<CloudPolicyInvalidator> invalidator_;
};

CloudPolicyInvalidatorTestBase::CloudPolicyInvalidatorTestBase()
    : core_(dm_protocol::GetChromeUserPolicyType(),
            std::string(),
            &store_,
            task_environment_.GetMainThreadTaskRunner(),
            network::TestNetworkConnectionTracker::CreateGetter()),
      task_runner_(new base::TestSimpleTaskRunner()),
      policy_value_cur_(kPolicyValueA) {
  clock_.SetNow(base::Time::UnixEpoch() + base::Seconds(987654321));
}

CloudPolicyInvalidatorTestBase::~CloudPolicyInvalidatorTestBase() {
  core_.Disconnect();
}

void CloudPolicyInvalidatorTestBase::StartInvalidator(
    int64_t highest_handled_invalidation_version) {
  invalidator_ = std::make_unique<CloudPolicyInvalidator>(
      GetPolicyInvalidationScope(), &invalidation_listener_, &core_,
      task_runner_, &clock_, highest_handled_invalidation_version,
      kDeviceLocalAccountId);
}

void CloudPolicyInvalidatorTestBase::DestroyInvalidator() {
  invalidator_.reset();
}

void CloudPolicyInvalidatorTestBase::ConnectCore() {
  std::unique_ptr<MockCloudPolicyClient> client =
      std::make_unique<MockCloudPolicyClient>();
  client->SetDMToken("dm");
  core_.Connect(std::move(client));
}

void CloudPolicyInvalidatorTestBase::StartRefreshScheduler() {
  core_.StartRefreshScheduler();
}

void CloudPolicyInvalidatorTestBase::DisconnectCore() {
  core_.Disconnect();
}

void CloudPolicyInvalidatorTestBase::StorePolicy(int64_t invalidation_version,
                                                 bool policy_changed,
                                                 const base::Time& time) {
  auto data = std::make_unique<em::PolicyData>();
  data->set_timestamp(time.InMillisecondsSinceUnixEpoch());
  // Swap the policy value if a policy change is desired.
  if (policy_changed) {
    policy_value_cur_ =
        policy_value_cur_ == kPolicyValueA ? kPolicyValueB : kPolicyValueA;
  }
  data->set_policy_value(policy_value_cur_);
  store_.invalidation_version_ = invalidation_version;
  store_.set_policy_data_for_testing(std::move(data));
  base::Value::Dict policies;
  policies.Set(key::kMaxInvalidationFetchDelay,
               CloudPolicyInvalidator::kMaxFetchDelayMin);
  store_.policy_map_.LoadFrom(policies, POLICY_LEVEL_MANDATORY,
                              POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD);
  store_.NotifyStoreLoaded();
}

void CloudPolicyInvalidatorTestBase::DisableInvalidationListener() {
  invalidation_listener_.Shutdown();
}

void CloudPolicyInvalidatorTestBase::EnableInvalidationListener() {
  invalidation_listener_.Start();
}

invalidation::DirectInvalidation
CloudPolicyInvalidatorTestBase::FireInvalidation(int64_t version,
                                                 const std::string& payload) {
  invalidation::DirectInvalidation invalidation(GetPolicyInvalidationType(),
                                                version, payload);
  invalidation_listener_.FireInvalidation(invalidation);
  return invalidation;
}

bool CloudPolicyInvalidatorTestBase::ClientInvalidationInfoIsUnset() {
  MockCloudPolicyClient* client =
      static_cast<MockCloudPolicyClient*>(core_.client());
  return client->invalidation_version_ == 0 &&
         client->invalidation_payload_.empty();
}

bool CloudPolicyInvalidatorTestBase::ClientInvalidationInfoMatches(
    const invalidation::DirectInvalidation& invalidation) {
  MockCloudPolicyClient* client =
      static_cast<MockCloudPolicyClient*>(core_.client());
  return invalidation.version() == client->invalidation_version_ &&
         invalidation.payload() == client->invalidation_payload_;
}

bool CloudPolicyInvalidatorTestBase::CheckPolicyNotRefreshed() {
  return CheckPolicyRefreshCount(0);
}

bool CloudPolicyInvalidatorTestBase::InvalidationsEnabled() {
  return core_.refresh_scheduler()->invalidations_available();
}

bool CloudPolicyInvalidatorTestBase::IsInvalidatorRegistered() {
  return invalidator_ && invalidation_listener_.HasObserver(invalidator_.get());
}

void CloudPolicyInvalidatorTestBase::AdvanceClock(base::TimeDelta delta) {
  clock_.Advance(delta);
}

base::Time CloudPolicyInvalidatorTestBase::Now() {
  return clock_.Now();
}

int64_t CloudPolicyInvalidatorTestBase::V(int version) {
  return GetVersion(Now()) + version;
}

int64_t CloudPolicyInvalidatorTestBase::GetVersion(base::Time time) {
  return (time - base::Time::UnixEpoch()).InMicroseconds();
}

PolicyInvalidationScope
CloudPolicyInvalidatorTestBase::GetPolicyInvalidationScope() const {
  return PolicyInvalidationScope::kUser;
}

std::string CloudPolicyInvalidatorTestBase::GetPolicyInvalidationType() const {
  switch (GetPolicyInvalidationScope()) {
    case PolicyInvalidationScope::kUser:
      return "USER_POLICY_FETCH";
    case PolicyInvalidationScope::kDevice:
      return "DEVICE_POLICY_FETCH";
    case PolicyInvalidationScope::kDeviceLocalAccount:
      return "PUBLIC_ACCOUNT_POLICY_FETCH-test_account";
    case PolicyInvalidationScope::kCBCM:
      return "BROWSER_POLICY_FETCH";
  }
}

bool CloudPolicyInvalidatorTestBase::CheckPolicyRefreshed(
    base::TimeDelta delay) {
  const auto* delay_policy_value = store_.policy_map().GetValue(
      key::kMaxInvalidationFetchDelay, base::Value::Type::INTEGER);
  const base::TimeDelta max_delay =
      delay +
      (delay_policy_value
           ? base::Milliseconds(delay_policy_value->GetInt())
           : base::Milliseconds(CloudPolicyInvalidator::kMaxFetchDelayMax));

  if (!task_runner_->HasPendingTask()) {
    return false;
  }
  base::TimeDelta actual_delay = task_runner_->FinalPendingTaskDelay();
  EXPECT_GE(actual_delay, delay);
  EXPECT_LE(actual_delay, max_delay);

  return CheckPolicyRefreshCount(1);
}

bool CloudPolicyInvalidatorTestBase::CheckPolicyRefreshCount(int count) {
  MockCloudPolicyClient* client =
      static_cast<MockCloudPolicyClient*>(core_.client());
  if (!client) {
    task_runner_->RunUntilIdle();
    return count == 0;
  }

  // Clear any non-invalidation refreshes which may be pending.
  EXPECT_CALL(*client, FetchPolicy(testing::_)).Times(testing::AnyNumber());
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(client);

  // Run the invalidator tasks then check for invalidation refreshes.
  EXPECT_CALL(*client, FetchPolicy(PolicyFetchReason::kInvalidation))
      .Times(count);
  task_runner_->RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  return testing::Mock::VerifyAndClearExpectations(client);
}

class CloudPolicyInvalidatorTest : public CloudPolicyInvalidatorTestBase {};

TEST_F(CloudPolicyInvalidatorTest, DoesNotRegisterWhenCoreIsNotConnected) {
  StartInvalidator();
  EnableInvalidationListener();

  FireInvalidation(V(1), "test");

  EXPECT_FALSE(IsInvalidatorRegistered());
  EXPECT_TRUE(CheckPolicyNotRefreshed());
}

TEST_F(CloudPolicyInvalidatorTest,
       DoesNotRegisterWhenRefreshSchedulerNotStarted) {
  StartInvalidator();
  EnableInvalidationListener();
  ConnectCore();

  FireInvalidation(V(1), "test");

  EXPECT_FALSE(IsInvalidatorRegistered());
  EXPECT_TRUE(CheckPolicyNotRefreshed());
}

TEST_F(CloudPolicyInvalidatorTest, RegistersWhenCoreIsReady) {
  StartInvalidator();
  EnableInvalidationListener();
  ConnectCore();
  StartRefreshScheduler();

  FireInvalidation(V(1), "test");

  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_TRUE(InvalidationsEnabled());
  EXPECT_TRUE(CheckPolicyRefreshed());
}

TEST_F(CloudPolicyInvalidatorTest,
       UnregistersWhenCoreDisconnectsAndRegistersWhenConnected) {
  StartInvalidator();
  EnableInvalidationListener();
  ConnectCore();
  StartRefreshScheduler();

  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_TRUE(InvalidationsEnabled());

  DisconnectCore();
  EXPECT_FALSE(IsInvalidatorRegistered());

  ConnectCore();
  EXPECT_FALSE(IsInvalidatorRegistered());

  StartRefreshScheduler();
  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_TRUE(InvalidationsEnabled());
}

TEST_F(CloudPolicyInvalidatorTest,
       UpdatesInvalidationStatusWhenInvalidationListenerStarts) {
  StartInvalidator();
  ConnectCore();
  StartRefreshScheduler();

  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_FALSE(InvalidationsEnabled());

  EnableInvalidationListener();

  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_TRUE(InvalidationsEnabled());

  DisableInvalidationListener();

  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_FALSE(InvalidationsEnabled());
}

TEST_F(CloudPolicyInvalidatorTest, HandlesInvalidation) {
  StorePolicy();
  StartInvalidator();
  ConnectCore();
  StartRefreshScheduler();
  EnableInvalidationListener();

  const invalidation::DirectInvalidation inv =
      FireInvalidation(V(12), "test_payload");

  EXPECT_TRUE(ClientInvalidationInfoMatches(inv));
  EXPECT_EQ(0, invalidator()->highest_handled_invalidation_version());

  StorePolicy(V(12));
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(V(12), invalidator()->highest_handled_invalidation_version());
}

TEST_F(CloudPolicyInvalidatorTest, HandlesInvalidationBeforePolicyLoaded) {
  StartInvalidator();
  ConnectCore();
  StartRefreshScheduler();
  EnableInvalidationListener();

  const invalidation::DirectInvalidation inv =
      FireInvalidation(V(12), "test_payload");

  EXPECT_TRUE(ClientInvalidationInfoMatches(inv));
  EXPECT_TRUE(CheckPolicyRefreshed());
  EXPECT_EQ(0, invalidator()->highest_handled_invalidation_version());

  StorePolicy(V(12));

  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(V(12), invalidator()->highest_handled_invalidation_version());
}

TEST_F(CloudPolicyInvalidatorTest, HandlesMultipleInvalidations) {
  StorePolicy();
  StartInvalidator();
  ConnectCore();
  StartRefreshScheduler();
  EnableInvalidationListener();

  // Fire invalidations out of order.
  const invalidation::DirectInvalidation inv2 = FireInvalidation(V(2), "test1");
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv2));
  const invalidation::DirectInvalidation inv1 = FireInvalidation(V(1), "test2");
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv2));
  const invalidation::DirectInvalidation inv3 = FireInvalidation(V(3), "test3");
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv3));

  // Make sure the policy is refreshed once.
  EXPECT_TRUE(CheckPolicyRefreshed());
  EXPECT_EQ(0, invalidator()->highest_handled_invalidation_version());

  // Make sure that the invalidation data is only removed from the client after
  // the store is loaded with the latest version.
  StorePolicy(V(1));
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv3));
  EXPECT_EQ(V(1), invalidator()->highest_handled_invalidation_version());

  StorePolicy(V(2));
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv3));
  EXPECT_EQ(V(2), invalidator()->highest_handled_invalidation_version());

  StorePolicy(V(3));
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(V(3), invalidator()->highest_handled_invalidation_version());
}

TEST_F(CloudPolicyInvalidatorTest, IgnoresOldInvalidations) {
  StorePolicy();
  ConnectCore();
  StartRefreshScheduler();
  EnableInvalidationListener();
  StartInvalidator(/*highest_handled_invalidation_version=*/V(2));

  // Check that an invalidation whose version is lower than the highest handled
  // so far is acknowledged but ignored otherwise.
  const invalidation::DirectInvalidation inv1 = FireInvalidation(V(1), "test1");
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(V(2), invalidator()->highest_handled_invalidation_version());

  // Check that an invalidation whose version matches the highest handled so far
  // is acknowledged but ignored otherwise.
  const invalidation::DirectInvalidation inv2 = FireInvalidation(V(2), "test2");
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(V(2), invalidator()->highest_handled_invalidation_version());

  // Check that an invalidation whose version is higher than the highest handled
  // so far is handled, causing a policy refresh.
  const invalidation::DirectInvalidation inv3 = FireInvalidation(V(3), "test3");
  EXPECT_TRUE(CheckPolicyRefreshed());
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv3));

  StorePolicy(V(3));
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(V(3), invalidator()->highest_handled_invalidation_version());
}

TEST_F(CloudPolicyInvalidatorTest, NoticesRegularPolicyRefresh) {
  StorePolicy();
  ConnectCore();
  StartRefreshScheduler();
  EnableInvalidationListener();
  StartInvalidator();

  EXPECT_EQ(0, invalidator()->highest_handled_invalidation_version());

  StorePolicy(V(2));

  EXPECT_EQ(V(2), invalidator()->highest_handled_invalidation_version());
}

class CloudPolicyInvalidatorOwnerNameTest
    : public CloudPolicyInvalidatorTestBase {
 protected:
  PolicyInvalidationScope GetPolicyInvalidationScope() const override {
    return scope_;
  }

  PolicyInvalidationScope scope_;
};

TEST_F(CloudPolicyInvalidatorOwnerNameTest, GetTypeForUserScope) {
  scope_ = PolicyInvalidationScope::kUser;
  StartInvalidator();
  ASSERT_TRUE(invalidator());
  EXPECT_EQ("USER_POLICY_FETCH", invalidator()->GetType());
}

TEST_F(CloudPolicyInvalidatorOwnerNameTest, GetTypeForDeviceScope) {
  scope_ = PolicyInvalidationScope::kDevice;
  StartInvalidator();
  ASSERT_TRUE(invalidator());
  EXPECT_EQ("DEVICE_POLICY_FETCH", invalidator()->GetType());
}

TEST_F(CloudPolicyInvalidatorOwnerNameTest, GetTypeForDeviceLocalAccountScope) {
  scope_ = PolicyInvalidationScope::kDeviceLocalAccount;
  StartInvalidator();
  ASSERT_TRUE(invalidator());
  EXPECT_EQ("PUBLIC_ACCOUNT_POLICY_FETCH-test_account",
            invalidator()->GetType());
}

TEST_F(CloudPolicyInvalidatorOwnerNameTest, GetTypeForCbcmScope) {
  scope_ = PolicyInvalidationScope::kCBCM;
  StartInvalidator();
  ASSERT_TRUE(invalidator());
  EXPECT_EQ("BROWSER_POLICY_FETCH", invalidator()->GetType());
}

class CloudPolicyInvalidatorUserTypedTest
    : public CloudPolicyInvalidatorTestBase,
      public testing::WithParamInterface<PolicyInvalidationScope> {
 public:
  CloudPolicyInvalidatorUserTypedTest(
      const CloudPolicyInvalidatorUserTypedTest&) = delete;
  CloudPolicyInvalidatorUserTypedTest& operator=(
      const CloudPolicyInvalidatorUserTypedTest&) = delete;

 protected:
  CloudPolicyInvalidatorUserTypedTest();

  base::HistogramBase::Count32 GetCount(MetricPolicyRefresh metric);
  base::HistogramBase::Count32 GetInvalidationCount(
      PolicyInvalidationType type);

 private:
  // CloudPolicyInvalidatorTest:
  PolicyInvalidationScope GetPolicyInvalidationScope() const override;

  base::HistogramTester histogram_tester_;
};

CloudPolicyInvalidatorUserTypedTest::CloudPolicyInvalidatorUserTypedTest() {
  ConnectCore();
  StartRefreshScheduler();
}

base::HistogramBase::Count32 CloudPolicyInvalidatorUserTypedTest::GetCount(
    MetricPolicyRefresh metric) {
  const char* metric_name = CloudPolicyInvalidator::GetPolicyRefreshMetricName(
      GetPolicyInvalidationScope());
  return histogram_tester_.GetHistogramSamplesSinceCreation(metric_name)
      ->GetCount(metric);
}

base::HistogramBase::Count32
CloudPolicyInvalidatorUserTypedTest::GetInvalidationCount(
    PolicyInvalidationType type) {
  const char* metric_name =
      CloudPolicyInvalidator::GetPolicyInvalidationMetricName(
          GetPolicyInvalidationScope());
  return histogram_tester_.GetHistogramSamplesSinceCreation(metric_name)
      ->GetCount(type);
}

PolicyInvalidationScope
CloudPolicyInvalidatorUserTypedTest::GetPolicyInvalidationScope() const {
  return GetParam();
}

TEST_P(CloudPolicyInvalidatorUserTypedTest,
       RefreshMetricsInvalidationsDisabled) {
  StartInvalidator();
  DisableInvalidationListener();

  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);

  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_CHANGED));
  EXPECT_EQ(2, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_CHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_UNCHANGED));

  EXPECT_EQ(0, invalidator()->highest_handled_invalidation_version());
}

TEST_P(CloudPolicyInvalidatorUserTypedTest, RefreshMetricsNoInvalidations) {
  // Store loads should be differentiated depending on whether the invalidation
  // service was enabled or not.
  StorePolicy();
  StartInvalidator();
  EnableInvalidationListener();

  // Initially, invalidations have not been enabled past the grace period, so
  // invalidations are OFF.
  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);
  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));

  // If the clock advances less than the grace period, invalidations are OFF.
  AdvanceClock(base::Seconds(1));
  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);
  EXPECT_EQ(2, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(2, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));

  // After the grace period elapses, invalidations are ON.
  AdvanceClock(base::Seconds(CloudPolicyInvalidator::kInvalidationGracePeriod));
  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);
  EXPECT_EQ(3, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_CHANGED));

  // After the invalidation service is disabled, invalidations are OFF.
  DisableInvalidationListener();
  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);
  EXPECT_EQ(4, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(3, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));

  // Enabling the invalidation service results in a new grace period, so
  // invalidations are OFF.
  EnableInvalidationListener();
  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);
  EXPECT_EQ(5, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(4, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));

  // After the grace period elapses, invalidations are ON.
  AdvanceClock(base::Seconds(CloudPolicyInvalidator::kInvalidationGracePeriod));
  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);

  EXPECT_EQ(2, GetCount(METRIC_POLICY_REFRESH_CHANGED));
  EXPECT_EQ(4, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));
  EXPECT_EQ(6, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_CHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_UNCHANGED));

  EXPECT_EQ(0, invalidator()->highest_handled_invalidation_version());
}

TEST_P(CloudPolicyInvalidatorUserTypedTest, RefreshMetricsInvalidation) {
  StartInvalidator();
  EnableInvalidationListener();

  StorePolicy();
  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));

  AdvanceClock(base::Seconds(CloudPolicyInvalidator::kInvalidationGracePeriod));
  FireInvalidation(V(5), "test");

  StorePolicy(0, /*policy_changed=*/false);
  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));

  StorePolicy(0, /*policy_changed=*/true);
  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_CHANGED));
  EXPECT_EQ(0, invalidator()->highest_handled_invalidation_version());

  StorePolicy(V(5), true /* policy_changed */);
  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_CHANGED));
  EXPECT_EQ(V(5), invalidator()->highest_handled_invalidation_version());

  // Store loads after the invalidation is complete are not counted as
  // invalidated.
  StorePolicy(V(5), /*policy_changed=*/false);
  StorePolicy(V(6), /*policy_changed=*/true);
  StorePolicy(V(6), /*policy_changed=*/false);
  StorePolicy(V(7), /*policy_changed=*/true);
  StorePolicy(V(7), /*policy_changed=*/false);
  StorePolicy(V(8), /*policy_changed=*/true);
  StorePolicy(V(8), /*policy_changed=*/false);

  EXPECT_EQ(4, GetCount(METRIC_POLICY_REFRESH_CHANGED));
  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));
  EXPECT_EQ(5, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_CHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_UNCHANGED));

  EXPECT_EQ(V(8), invalidator()->highest_handled_invalidation_version());
}

TEST_P(CloudPolicyInvalidatorUserTypedTest, ExpiredInvalidations) {
  StorePolicy(0, false, Now());
  StartInvalidator();
  EnableInvalidationListener();

  // Invalidations fired before the last fetch time (adjusted by max time delta)
  // should be ignored (and count as expired).
  base::Time time = Now() - (invalidation_timeouts::kMaxInvalidationTimeDelta +
                             base::Seconds(300));
  invalidation::DirectInvalidation inv =
      FireInvalidation(GetVersion(time), "test");
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  ASSERT_TRUE(CheckPolicyNotRefreshed());

  inv = FireInvalidation(GetVersion(time), "");  // no payload
  ASSERT_TRUE(CheckPolicyNotRefreshed());

  time += base::Minutes(5) - base::Seconds(1);
  inv = FireInvalidation(GetVersion(time), "test");
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  ASSERT_TRUE(CheckPolicyNotRefreshed());

  // Invalidations fired after the last fetch should not be ignored.
  time += base::Seconds(1);
  inv = FireInvalidation(GetVersion(time), "");  // no payload
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  ASSERT_TRUE(CheckPolicyRefreshed(
      base::Minutes(CloudPolicyInvalidator::kMissingPayloadDelay)));

  time += base::Minutes(10);
  inv = FireInvalidation(GetVersion(time), "test");
  ASSERT_TRUE(ClientInvalidationInfoMatches(inv));
  ASSERT_TRUE(CheckPolicyRefreshed());

  time += base::Minutes(10);
  inv = FireInvalidation(GetVersion(time), "test");
  ASSERT_TRUE(ClientInvalidationInfoMatches(inv));
  ASSERT_TRUE(CheckPolicyRefreshed());

  time += base::Minutes(10);
  inv = FireInvalidation(GetVersion(time), "test");
  ASSERT_TRUE(ClientInvalidationInfoMatches(inv));
  ASSERT_TRUE(CheckPolicyRefreshed());

  // Verify that received invalidations metrics are correct.
  EXPECT_EQ(1, GetInvalidationCount(POLICY_INVALIDATION_TYPE_NO_PAYLOAD));
  EXPECT_EQ(3, GetInvalidationCount(POLICY_INVALIDATION_TYPE_NORMAL));
  EXPECT_EQ(1,
            GetInvalidationCount(POLICY_INVALIDATION_TYPE_NO_PAYLOAD_EXPIRED));
  EXPECT_EQ(2, GetInvalidationCount(POLICY_INVALIDATION_TYPE_EXPIRED));

  EXPECT_EQ(0, invalidator()->highest_handled_invalidation_version());
}

INSTANTIATE_TEST_SUITE_P(
    CloudPolicyInvalidatorUserTypedTestInstance,
    CloudPolicyInvalidatorUserTypedTest,
    testing::Values(PolicyInvalidationScope::kUser,
                    PolicyInvalidationScope::kDevice,
                    PolicyInvalidationScope::kDeviceLocalAccount));

}  // namespace policy

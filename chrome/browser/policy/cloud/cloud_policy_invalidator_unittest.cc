// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

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
  void StartInvalidator();

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

  // Returns the number of policy refreshes that have occurred since the last
  // call to this method and resets the counter to 0.
  int GetPolicyRefreshCountAndReset();

  // Returns the invalidations enabled state set by the invalidator on the
  // refresh scheduler.
  bool InvalidationsEnabled();

  // Determines if the invalidator has registered as an observer with the
  // invalidation service.
  bool IsInvalidatorRegistered();

  // Advance the test clock by the given `delta`.
  void FastForwardBy(base::TimeDelta delta);
  // Advance the test clock to trigger invalidation handling.
  void FastForwardByInvalidationDelay();

  // Get the current time on the test clock.
  base::Time Now();

  // Translate a version number into an appropriate invalidation version (which
  // is based on the start time).
  int64_t V(int version);

  // Get an invalidation version for the given time.
  int64_t GetVersion(base::Time time);

  // Get the invalidation scope that the `invalidator_` is responsible for.
  virtual PolicyInvalidationScope GetPolicyInvalidationScope() const;
  std::string GetPolicyInvalidationType() const;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  const base::Time start_time{task_environment_.GetMockClock()->Now()};

  // Objects the invalidator depends on.
  testing::NiceMock<MockCloudPolicyStore> store_;
  CloudPolicyCore core_{dm_protocol::GetChromeUserPolicyType(), std::string(),
                        &store_, task_environment_.GetMainThreadTaskRunner(),
                        network::TestNetworkConnectionTracker::CreateGetter()};
  int policy_refresh_count_ = 0;

  invalidation::FakeInvalidationListener invalidation_listener_;

  // The currently used policy value.
  std::string policy_value_cur_{kPolicyValueA};

  // The invalidator which will be tested.
  std::unique_ptr<CloudPolicyInvalidator> invalidator_;
};

CloudPolicyInvalidatorTestBase::CloudPolicyInvalidatorTestBase() = default;

CloudPolicyInvalidatorTestBase::~CloudPolicyInvalidatorTestBase() {
  core_.Disconnect();
}

void CloudPolicyInvalidatorTestBase::StartInvalidator() {
  invalidator_ = std::make_unique<CloudPolicyInvalidator>(
      GetPolicyInvalidationScope(), &invalidation_listener_, &core_,
      task_environment_.GetMainThreadTaskRunner(),
      task_environment_.GetMockClock(), kDeviceLocalAccountId);
}

void CloudPolicyInvalidatorTestBase::ConnectCore() {
  std::unique_ptr<testing::NiceMock<MockCloudPolicyClient>> client =
      std::make_unique<testing::NiceMock<MockCloudPolicyClient>>();

  ON_CALL(*client, FetchPolicy(PolicyFetchReason::kInvalidation))
      .WillByDefault([this]() { ++policy_refresh_count_; });

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
               static_cast<int>(
                   CloudPolicyInvalidator::kMaxFetchDelayMin.InMilliseconds()));
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
  if (!client) {
    return false;
  }

  return client->invalidation_version_ == 0 &&
         client->invalidation_payload_.empty();
}

int CloudPolicyInvalidatorTestBase::GetPolicyRefreshCountAndReset() {
  return std::exchange(policy_refresh_count_, 0);
}

bool CloudPolicyInvalidatorTestBase::ClientInvalidationInfoMatches(
    const invalidation::DirectInvalidation& invalidation) {
  MockCloudPolicyClient* client =
      static_cast<MockCloudPolicyClient*>(core_.client());
  if (!client) {
    return false;
  }

  return invalidation.version() == client->invalidation_version_ &&
         invalidation.payload() == client->invalidation_payload_;
}

bool CloudPolicyInvalidatorTestBase::InvalidationsEnabled() {
  return core_.refresh_scheduler()->invalidations_available();
}

bool CloudPolicyInvalidatorTestBase::IsInvalidatorRegistered() {
  return invalidator_ && invalidation_listener_.HasObserver(invalidator_.get());
}

void CloudPolicyInvalidatorTestBase::FastForwardBy(base::TimeDelta delta) {
  task_environment_.FastForwardBy(delta);
}

void CloudPolicyInvalidatorTestBase::FastForwardByInvalidationDelay() {
  const auto* delay_policy_value = store_.policy_map().GetValue(
      key::kMaxInvalidationFetchDelay, base::Value::Type::INTEGER);
  const base::TimeDelta max_delay =
      delay_policy_value ? base::Milliseconds(delay_policy_value->GetInt())
                         : CloudPolicyInvalidator::kMaxFetchDelayMax;
  FastForwardBy(max_delay);
}

base::Time CloudPolicyInvalidatorTestBase::Now() {
  return task_environment_.GetMockClock()->Now();
}

int64_t CloudPolicyInvalidatorTestBase::V(int version) {
  return GetVersion(start_time) + version;
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

class CloudPolicyInvalidatorTest : public CloudPolicyInvalidatorTestBase {};

TEST_F(CloudPolicyInvalidatorTest, DoesNotRegisterWhenCoreIsNotConnected) {
  StartInvalidator();
  EnableInvalidationListener();

  FireInvalidation(V(1), "test");
  FastForwardByInvalidationDelay();

  EXPECT_FALSE(IsInvalidatorRegistered());
  EXPECT_EQ(GetPolicyRefreshCountAndReset(), 0);
}

TEST_F(CloudPolicyInvalidatorTest,
       DoesNotRegisterWhenRefreshSchedulerNotStarted) {
  StartInvalidator();
  EnableInvalidationListener();
  ConnectCore();

  FireInvalidation(V(1), "test");
  FastForwardByInvalidationDelay();

  EXPECT_FALSE(IsInvalidatorRegistered());
  EXPECT_EQ(GetPolicyRefreshCountAndReset(), 0);
}

TEST_F(CloudPolicyInvalidatorTest, RegistersWhenCoreIsReady) {
  StartInvalidator();
  EnableInvalidationListener();
  ConnectCore();
  StartRefreshScheduler();

  FireInvalidation(V(1), "test");
  FastForwardByInvalidationDelay();

  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_TRUE(InvalidationsEnabled());
  EXPECT_EQ(GetPolicyRefreshCountAndReset(), 1);
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

  // Verify that invalidation is not yet handled as we did not pass random
  // delay.
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(GetPolicyRefreshCountAndReset(), 0);

  FastForwardByInvalidationDelay();

  // Verify that the invalidation is handled after the delay.
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv));
  EXPECT_EQ(GetPolicyRefreshCountAndReset(), 1);
  EXPECT_EQ(0, invalidator()->highest_handled_invalidation_version());

  StorePolicy(V(12));
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(V(12), invalidator()->highest_handled_invalidation_version());
}

TEST_F(CloudPolicyInvalidatorTest, HandlesInvalidationWithoutPayload) {
  StorePolicy();
  StartInvalidator();
  ConnectCore();
  StartRefreshScheduler();
  EnableInvalidationListener();

  // Fire an invalidation and check that it triggered a policy refresh only
  // after a random delay.
  const invalidation::DirectInvalidation inv = FireInvalidation(V(12), "");

  // Verify that invalidation is not yet handled as we did not pass random
  // delay.
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(GetPolicyRefreshCountAndReset(), 0);
  EXPECT_EQ(0, invalidator()->highest_handled_invalidation_version());

  FastForwardByInvalidationDelay();

  EXPECT_TRUE(ClientInvalidationInfoMatches(inv));
  EXPECT_EQ(GetPolicyRefreshCountAndReset(), 1);
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
  FastForwardByInvalidationDelay();

  EXPECT_TRUE(ClientInvalidationInfoMatches(inv));
  EXPECT_EQ(GetPolicyRefreshCountAndReset(), 1);
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
  const invalidation::DirectInvalidation inv1 = FireInvalidation(V(1), "test2");
  const invalidation::DirectInvalidation inv3 = FireInvalidation(V(3), "test3");
  FastForwardByInvalidationDelay();

  // Make sure the policy is refreshed once.
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv3));
  EXPECT_EQ(GetPolicyRefreshCountAndReset(), 1);
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
  StartInvalidator();

  const invalidation::DirectInvalidation inv0 = FireInvalidation(V(2), "test2");
  FastForwardByInvalidationDelay();
  EXPECT_EQ(GetPolicyRefreshCountAndReset(), 1);
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv0));
  StorePolicy(V(2));
  EXPECT_EQ(V(2), invalidator()->highest_handled_invalidation_version());

  // Check that an invalidation whose version is lower than the highest handled
  // so far is acknowledged but ignored otherwise.
  const invalidation::DirectInvalidation inv1 = FireInvalidation(V(1), "test1");
  FastForwardByInvalidationDelay();

  EXPECT_EQ(GetPolicyRefreshCountAndReset(), 0);
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(V(2), invalidator()->highest_handled_invalidation_version());

  // Check that an invalidation whose version matches the highest handled so far
  // is acknowledged but ignored otherwise.
  const invalidation::DirectInvalidation inv2 = FireInvalidation(V(2), "test2");
  FastForwardByInvalidationDelay();

  EXPECT_EQ(GetPolicyRefreshCountAndReset(), 0);
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(V(2), invalidator()->highest_handled_invalidation_version());

  // Check that an invalidation whose version is higher than the highest handled
  // so far is handled, causing a policy refresh.
  const invalidation::DirectInvalidation inv3 = FireInvalidation(V(3), "test3");
  FastForwardByInvalidationDelay();

  EXPECT_EQ(GetPolicyRefreshCountAndReset(), 1);
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
  FastForwardBy(base::Seconds(1));
  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);
  EXPECT_EQ(2, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(2, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));

  // After the grace period elapses, invalidations are ON.
  FastForwardBy(CloudPolicyInvalidator::kInvalidationGracePeriod);
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
  FastForwardBy(CloudPolicyInvalidator::kInvalidationGracePeriod);
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

  FastForwardBy(CloudPolicyInvalidator::kInvalidationGracePeriod);
  FireInvalidation(V(5), "test");
  FastForwardByInvalidationDelay();

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
  FastForwardBy(base::Hours(1));
  const auto policy_store_timestamp = Now();
  StorePolicy(0, false, policy_store_timestamp);
  StartInvalidator();
  EnableInvalidationListener();

  // Invalidations fired before the last fetch time (adjusted by max time delta)
  // should be ignored (and count as expired).
  const base::Time expired_invalidation_timestamp =
      policy_store_timestamp -
      invalidation_timeouts::kMaxInvalidationTimeDelta - base::Seconds(1);

  // Fire expired invalidation with a payload.
  invalidation::DirectInvalidation inv =
      FireInvalidation(GetVersion(expired_invalidation_timestamp), "test");
  FastForwardByInvalidationDelay();
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(GetPolicyRefreshCountAndReset(), 0);

  // Fire expired invalidation without a payload.
  inv = FireInvalidation(GetVersion(expired_invalidation_timestamp), "");
  FastForwardByInvalidationDelay();
  EXPECT_EQ(GetPolicyRefreshCountAndReset(), 0);

  // Invalidations fired after the last fetch should not be ignored.
  const base::Time non_expired_invalidaiton_timestamp =
      expired_invalidation_timestamp + base::Seconds(1);

  // Fire a fine invalidation without a payload.
  inv = FireInvalidation(GetVersion(non_expired_invalidaiton_timestamp), "");
  FastForwardByInvalidationDelay();
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv));
  EXPECT_EQ(GetPolicyRefreshCountAndReset(), 1);

  // Fire three fine invalidations with a payload.
  inv = FireInvalidation(
      GetVersion(non_expired_invalidaiton_timestamp + base::Minutes(10)),
      "test");
  FastForwardByInvalidationDelay();
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv));
  EXPECT_EQ(GetPolicyRefreshCountAndReset(), 1);

  inv = FireInvalidation(
      GetVersion(non_expired_invalidaiton_timestamp + base::Minutes(20)),
      "test");
  FastForwardByInvalidationDelay();
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv));
  EXPECT_EQ(GetPolicyRefreshCountAndReset(), 1);

  inv = FireInvalidation(
      GetVersion(non_expired_invalidaiton_timestamp + base::Minutes(30)),
      "test");
  FastForwardByInvalidationDelay();
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv));
  EXPECT_EQ(GetPolicyRefreshCountAndReset(), 1);

  // Verify that received invalidations metrics are correct.
  EXPECT_EQ(1, GetInvalidationCount(POLICY_INVALIDATION_TYPE_NO_PAYLOAD));
  EXPECT_EQ(3, GetInvalidationCount(POLICY_INVALIDATION_TYPE_NORMAL));
  EXPECT_EQ(1,
            GetInvalidationCount(POLICY_INVALIDATION_TYPE_NO_PAYLOAD_EXPIRED));
  EXPECT_EQ(1, GetInvalidationCount(POLICY_INVALIDATION_TYPE_EXPIRED));

  // New policies never stored, verify invalidation handling did not finished.
  EXPECT_EQ(0, invalidator()->highest_handled_invalidation_version());
}

INSTANTIATE_TEST_SUITE_P(
    CloudPolicyInvalidatorUserTypedTestInstance,
    CloudPolicyInvalidatorUserTypedTest,
    testing::Values(PolicyInvalidationScope::kUser,
                    PolicyInvalidationScope::kDevice,
                    PolicyInvalidationScope::kDeviceLocalAccount));

}  // namespace policy

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
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
#include "build/build_config.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_invalidator.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/invalidation/impl/invalidator_registrar_with_memory.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
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

namespace em = enterprise_management;

namespace policy {

class CloudPolicyInvalidatorTestBase : public testing::Test {
 protected:
  // Policy objects which can be used in tests.
  enum PolicyObject {
    POLICY_OBJECT_NONE,
    POLICY_OBJECT_A,
    POLICY_OBJECT_B
  };

  CloudPolicyInvalidatorTestBase();

  void TearDown() override;

  // Starts the invalidator which will be tested.
  // |initialize| determines if the invalidator should be initialized.
  // |start_refresh_scheduler| determines if the refresh scheduler should start.
  // |highest_handled_invalidation_version| is the highest invalidation version
  // that was handled already before this invalidator was created.
  void StartInvalidator(bool initialize,
                        bool start_refresh_scheduler,
                        int64_t highest_handled_invalidation_version);
  void StartInvalidator() {
    StartInvalidator(true, /* initialize */
                     true, /* start_refresh_scheduler */
                     0     /* highest_handled_invalidation_version */);
  }

  const CloudPolicyInvalidator* invalidator() const {
    return invalidator_.get();
  }

  // Calls Initialize on the invalidator.
  void InitializeInvalidator();

  // Calls Shutdown on the invalidator. Test must call DestroyInvalidator
  // afterwards to prevent Shutdown from being called twice.
  void ShutdownInvalidator();

  // Destroys the invalidator.
  void DestroyInvalidator();

  // Connects the cloud policy core.
  void ConnectCore();

  // Starts the refresh scheduler.
  void StartRefreshScheduler();

  // Disconnects the cloud policy core.
  void DisconnectCore();

  // Simulates storing a new policy to the policy store.
  // |object| determines which policy object the store will report the
  // invalidator should register for. May be POLICY_OBJECT_NONE for no object.
  // |invalidation_version| determines what invalidation the store will report.
  // |policy_changed| determines whether a policy value different from the
  // current value will be stored.
  // |time| determines the timestamp the store will report.
  void StorePolicy(PolicyObject object,
                   int64_t invalidation_version,
                   bool policy_changed,
                   const base::Time& time);
  void StorePolicy(PolicyObject object,
                   int64_t invalidation_version,
                   bool policy_changed) {
    StorePolicy(object, invalidation_version, policy_changed,
                Now() - base::Minutes(5));
  }
  void StorePolicy(PolicyObject object, int64_t invalidation_version) {
    StorePolicy(object, invalidation_version, false);
  }
  void StorePolicy(PolicyObject object) {
    StorePolicy(object, 0);
  }

  // Disables the invalidation service. It is enabled by default.
  void DisableInvalidationService();

  // Enables the invalidation service. It is enabled by default.
  void EnableInvalidationService();

  // Causes the invalidation service to fire an invalidation.
  invalidation::Invalidation FireInvalidation(PolicyObject object,
                                              int64_t version,
                                              const std::string& payload);

  // Checks the expected value of the currently set invalidation info.
  bool CheckInvalidationInfo(int64_t version, const std::string& payload);

  // Checks that the policy was not refreshed due to an invalidation.
  bool CheckPolicyNotRefreshed();

  // Checks that the policy was refreshed due to an invalidation with the given
  // base delay.
  bool CheckPolicyRefreshed(base::TimeDelta delay = base::TimeDelta());

  bool IsUnsent(const invalidation::Invalidation& invalidation);

  // Returns the invalidations enabled state set by the invalidator on the
  // refresh scheduler.
  bool InvalidationsEnabled();

  // Determines if the invalidation with the given ack handle has been
  // acknowledged.
  bool IsInvalidationAcknowledged(
      const invalidation::Invalidation& invalidation);

  // Determines if the invalidator has registered for an object with the
  // invalidation service.
  bool IsInvalidatorRegistered();

  // Returns the highest invalidation version that was handled already according
  // to the |invalidator_|.
  int64_t GetHighestHandledInvalidationVersion() const;

  // Advance the test clock.
  void AdvanceClock(base::TimeDelta delta);

  // Get the current time on the test clock.
  base::Time Now();

  // Translate a version number into an appropriate invalidation version (which
  // is based on the current time).
  int64_t V(int version);

  // Get an invalidation version for the given time.
  int64_t GetVersion(base::Time time);

  // Get the invalidation scope that the |invalidator_| is responsible for.
  virtual PolicyInvalidationScope GetPolicyInvalidationScope() const;

 private:
  // Checks that the policy was refreshed the given number of times.
  bool CheckPolicyRefreshCount(int count);

  // Returns the invalidation topic corresponding to the given policy object.
  const invalidation::Topic& GetPolicyTopic(PolicyObject object) const;

  base::test::SingleThreadTaskEnvironment task_environment_;

  // Objects the invalidator depends on.
  invalidation::FakeInvalidationService invalidation_service_;
  MockCloudPolicyStore store_;
  CloudPolicyCore core_;
  raw_ptr<MockCloudPolicyClient, DanglingUntriaged> client_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SimpleTestClock clock_;

  // The invalidator which will be tested.
  std::unique_ptr<CloudPolicyInvalidator> invalidator_;

  // Topics for the test policy objects.
  invalidation::Topic topic_a_;
  invalidation::Topic topic_b_;

  // Fake policy values which are alternated to cause the store to report a
  // changed policy.
  const char* policy_value_a_;
  const char* policy_value_b_;

  // The currently used policy value.
  const char* policy_value_cur_;

  const char* account_id_;
};

CloudPolicyInvalidatorTestBase::CloudPolicyInvalidatorTestBase()
    : core_(dm_protocol::kChromeUserPolicyType,
            std::string(),
            &store_,
            task_environment_.GetMainThreadTaskRunner(),
            network::TestNetworkConnectionTracker::CreateGetter()),
      client_(nullptr),
      task_runner_(new base::TestSimpleTaskRunner()),
      topic_a_("asdf"),
      topic_b_("zxcv"),
      policy_value_a_("asdf"),
      policy_value_b_("zxcv"),
      policy_value_cur_(policy_value_a_),
      account_id_("test_account") {
  clock_.SetNow(base::Time::UnixEpoch() + base::Seconds(987654321));
}

void CloudPolicyInvalidatorTestBase::TearDown() {
  if (invalidator_)
    invalidator_->Shutdown();
  core_.Disconnect();
}

void CloudPolicyInvalidatorTestBase::StartInvalidator(
    bool initialize,
    bool start_refresh_scheduler,
    int64_t highest_handled_invalidation_version) {
  invalidator_ = std::make_unique<CloudPolicyInvalidator>(
      GetPolicyInvalidationScope(), &core_, task_runner_, &clock_,
      highest_handled_invalidation_version, account_id_);
  if (start_refresh_scheduler) {
    ConnectCore();
    StartRefreshScheduler();
  }
  if (initialize)
    InitializeInvalidator();
}

void CloudPolicyInvalidatorTestBase::InitializeInvalidator() {
  invalidator_->Initialize(&invalidation_service_);
}

void CloudPolicyInvalidatorTestBase::ShutdownInvalidator() {
  invalidator_->Shutdown();
}

void CloudPolicyInvalidatorTestBase::DestroyInvalidator() {
  invalidator_.reset();
}

void CloudPolicyInvalidatorTestBase::ConnectCore() {
  client_ = new MockCloudPolicyClient();
  client_->SetDMToken("dm");
  core_.Connect(std::unique_ptr<CloudPolicyClient>(client_));
}

void CloudPolicyInvalidatorTestBase::StartRefreshScheduler() {
  core_.StartRefreshScheduler();
}

void CloudPolicyInvalidatorTestBase::DisconnectCore() {
  client_ = nullptr;
  core_.Disconnect();
}

void CloudPolicyInvalidatorTestBase::StorePolicy(PolicyObject object,
                                                 int64_t invalidation_version,
                                                 bool policy_changed,
                                                 const base::Time& time) {
  auto data = std::make_unique<em::PolicyData>();
  if (object != POLICY_OBJECT_NONE) {
    // CloudPolicyInvalidator expects the topic to subscribe in this field.
    data->set_policy_invalidation_topic(GetPolicyTopic(object));
  }
  data->set_timestamp(time.InMillisecondsSinceUnixEpoch());
  // Swap the policy value if a policy change is desired.
  if (policy_changed)
    policy_value_cur_ = policy_value_cur_ == policy_value_a_ ?
        policy_value_b_ : policy_value_a_;
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

void CloudPolicyInvalidatorTestBase::DisableInvalidationService() {
  invalidation_service_.SetInvalidatorState(
      invalidation::TRANSIENT_INVALIDATION_ERROR);
}

void CloudPolicyInvalidatorTestBase::EnableInvalidationService() {
  invalidation_service_.SetInvalidatorState(
      invalidation::INVALIDATIONS_ENABLED);
}

invalidation::Invalidation CloudPolicyInvalidatorTestBase::FireInvalidation(
    PolicyObject object,
    int64_t version,
    const std::string& payload) {
  invalidation::Invalidation invalidation =
      invalidation::Invalidation(GetPolicyTopic(object), version, payload);
  invalidation_service_.EmitInvalidationForTest(invalidation);
  return invalidation;
}

bool CloudPolicyInvalidatorTestBase::CheckInvalidationInfo(
    int64_t version,
    const std::string& payload) {
  MockCloudPolicyClient* client =
      static_cast<MockCloudPolicyClient*>(core_.client());
  return version == client->invalidation_version_ &&
      payload == client->invalidation_payload_;
}

bool CloudPolicyInvalidatorTestBase::CheckPolicyNotRefreshed() {
  return CheckPolicyRefreshCount(0);
}

bool CloudPolicyInvalidatorTestBase::IsUnsent(
    const invalidation::Invalidation& invalidation) {
  return invalidation_service_.GetFakeAckHandler()->IsUnsent(invalidation);
}

bool CloudPolicyInvalidatorTestBase::InvalidationsEnabled() {
  return core_.refresh_scheduler()->invalidations_available();
}

bool CloudPolicyInvalidatorTestBase::IsInvalidationAcknowledged(
    const invalidation::Invalidation& invalidation) {
  // The acknowledgement task is run through a WeakHandle that posts back to our
  // own thread.  We need to run any posted tasks before we can check
  // acknowledgement status.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsUnsent(invalidation));
  return !invalidation_service_.GetFakeAckHandler()->IsUnacked(invalidation);
}

bool CloudPolicyInvalidatorTestBase::IsInvalidatorRegistered() {
  return !invalidation_service_.invalidator_registrar()
              .GetRegisteredTopics(invalidator_.get())
              .empty();
}

int64_t CloudPolicyInvalidatorTestBase::GetHighestHandledInvalidationVersion()
    const {
  return invalidator_->highest_handled_invalidation_version();
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

bool CloudPolicyInvalidatorTestBase::CheckPolicyRefreshed(
    base::TimeDelta delay) {
  base::TimeDelta max_delay =
      delay + base::Milliseconds(CloudPolicyInvalidator::kMaxFetchDelayMin);

  if (!task_runner_->HasPendingTask())
    return false;
  base::TimeDelta actual_delay = task_runner_->FinalPendingTaskDelay();
  EXPECT_GE(actual_delay, delay);
  EXPECT_LE(actual_delay, max_delay);

  return CheckPolicyRefreshCount(1);
}

bool CloudPolicyInvalidatorTestBase::CheckPolicyRefreshCount(int count) {
  if (!client_) {
    task_runner_->RunUntilIdle();
    return count == 0;
  }

  // Clear any non-invalidation refreshes which may be pending.
  EXPECT_CALL(*client_, FetchPolicy(testing::_)).Times(testing::AnyNumber());
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(client_);

  // Run the invalidator tasks then check for invalidation refreshes.
  EXPECT_CALL(*client_, FetchPolicy(PolicyFetchReason::kInvalidation))
      .Times(count);
  task_runner_->RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  return testing::Mock::VerifyAndClearExpectations(client_);
}

const invalidation::Topic& CloudPolicyInvalidatorTestBase::GetPolicyTopic(
    PolicyObject object) const {
  EXPECT_TRUE(object == POLICY_OBJECT_A || object == POLICY_OBJECT_B);
  return object == POLICY_OBJECT_A ? topic_a_ : topic_b_;
}

class CloudPolicyInvalidatorTest : public CloudPolicyInvalidatorTestBase {};

TEST_F(CloudPolicyInvalidatorTest, Uninitialized) {
  // No invalidations should be processed if the invalidator is not initialized.
  StartInvalidator(false, /* initialize */
                   true,  /* start_refresh_scheduler */
                   0      /* highest_handled_invalidation_version*/);
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_FALSE(IsInvalidatorRegistered());
  EXPECT_TRUE(IsUnsent(FireInvalidation(POLICY_OBJECT_A, V(1), "test")));
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, RefreshSchedulerNotStarted) {
  // No invalidations should be processed if the refresh scheduler is not
  // started.
  StartInvalidator(true,  /* initialize */
                   false, /* start_refresh_scheduler */
                   0      /* highest_handled_invalidation_version*/);
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_FALSE(IsInvalidatorRegistered());
  EXPECT_TRUE(IsUnsent(FireInvalidation(POLICY_OBJECT_A, V(1), "test")));
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, DisconnectCoreThenInitialize) {
  // No invalidations should be processed if the core is disconnected before
  // initialization.
  StartInvalidator(false, /* initialize */
                   true,  /* start_refresh_scheduler */
                   0      /* highest_handled_invalidation_version*/);
  DisconnectCore();
  InitializeInvalidator();
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_FALSE(IsInvalidatorRegistered());
  EXPECT_TRUE(IsUnsent(FireInvalidation(POLICY_OBJECT_A, V(1), "test")));
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, InitializeThenStartRefreshScheduler) {
  // Make sure registration occurs and invalidations are processed when
  // Initialize is called before starting the refresh scheduler.
  // Note that the reverse case (start refresh scheduler then initialize) is
  // the default behavior for the test fixture, so will be tested in most other
  // tests.
  StartInvalidator(true,  /* initialize */
                   false, /* start_refresh_scheduler */
                   0      /* highest_handled_invalidation_version*/);
  ConnectCore();
  StartRefreshScheduler();
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_TRUE(IsInvalidatorRegistered());
  FireInvalidation(POLICY_OBJECT_A, V(1), "test");
  EXPECT_TRUE(CheckPolicyRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, RegisterOnStoreLoaded) {
  // No registration when store is not loaded.
  StartInvalidator();
  EXPECT_FALSE(IsInvalidatorRegistered());
  EXPECT_FALSE(InvalidationsEnabled());
  EXPECT_TRUE(IsUnsent(FireInvalidation(POLICY_OBJECT_A, V(1), "test")));
  EXPECT_TRUE(IsUnsent(FireInvalidation(POLICY_OBJECT_B, V(2), "test")));
  EXPECT_TRUE(CheckPolicyNotRefreshed());

  // No registration when store is loaded with no invalidation object id.
  StorePolicy(POLICY_OBJECT_NONE);
  EXPECT_FALSE(IsInvalidatorRegistered());
  EXPECT_FALSE(InvalidationsEnabled());
  EXPECT_TRUE(IsUnsent(FireInvalidation(POLICY_OBJECT_A, V(3), "test")));
  EXPECT_TRUE(IsUnsent(FireInvalidation(POLICY_OBJECT_B, V(4), "test")));
  EXPECT_TRUE(CheckPolicyNotRefreshed());

  // Check registration when store is loaded for object A.
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_TRUE(InvalidationsEnabled());
  FireInvalidation(POLICY_OBJECT_A, V(5), "test");
  EXPECT_TRUE(CheckPolicyRefreshed());
  EXPECT_TRUE(IsUnsent(FireInvalidation(POLICY_OBJECT_B, V(6), "test")));
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, ChangeRegistration) {
  // Register for object A.
  StartInvalidator();
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_TRUE(InvalidationsEnabled());
  FireInvalidation(POLICY_OBJECT_A, V(1), "test");
  EXPECT_TRUE(CheckPolicyRefreshed());
  EXPECT_TRUE(IsUnsent(FireInvalidation(POLICY_OBJECT_B, V(2), "test")));
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  invalidation::Invalidation inv =
      FireInvalidation(POLICY_OBJECT_A, V(3), "test");

  // Check re-registration for object B. Make sure the pending invalidation for
  // object A is acknowledged without making the callback.
  StorePolicy(POLICY_OBJECT_B);
  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_TRUE(InvalidationsEnabled());
  EXPECT_TRUE(IsInvalidationAcknowledged(inv));
  EXPECT_TRUE(CheckPolicyNotRefreshed());

  // Make sure future invalidations for object A are ignored and for object B
  // are processed.
  EXPECT_TRUE(IsUnsent(FireInvalidation(POLICY_OBJECT_A, V(4), "test")));
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  FireInvalidation(POLICY_OBJECT_B, V(5), "test");
  EXPECT_TRUE(CheckPolicyRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, UnregisterOnStoreLoaded) {
  // Register for object A.
  StartInvalidator();
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_TRUE(InvalidationsEnabled());
  FireInvalidation(POLICY_OBJECT_A, V(1), "test");
  EXPECT_TRUE(CheckPolicyRefreshed());

  // Check unregistration when store is loaded with no invalidation object id.
  invalidation::Invalidation inv =
      FireInvalidation(POLICY_OBJECT_A, V(2), "test");
  EXPECT_FALSE(IsInvalidationAcknowledged(inv));
  StorePolicy(POLICY_OBJECT_NONE);
  EXPECT_FALSE(IsInvalidatorRegistered());
  EXPECT_TRUE(IsInvalidationAcknowledged(inv));
  EXPECT_FALSE(InvalidationsEnabled());
  EXPECT_TRUE(IsUnsent(FireInvalidation(POLICY_OBJECT_A, V(3), "test")));
  EXPECT_TRUE(IsUnsent(FireInvalidation(POLICY_OBJECT_B, V(4), "test")));
  EXPECT_TRUE(CheckPolicyNotRefreshed());

  // Check re-registration for object B.
  StorePolicy(POLICY_OBJECT_B);
  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_TRUE(InvalidationsEnabled());
  FireInvalidation(POLICY_OBJECT_B, V(5), "test");
  EXPECT_TRUE(CheckPolicyRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, HandleInvalidation) {
  // Register and fire invalidation
  StorePolicy(POLICY_OBJECT_A);
  StartInvalidator();
  EXPECT_TRUE(InvalidationsEnabled());
  invalidation::Invalidation inv =
      FireInvalidation(POLICY_OBJECT_A, V(12), "test_payload");

  // Make sure client info is set as soon as the invalidation is received.
  EXPECT_TRUE(CheckInvalidationInfo(V(12), "test_payload"));
  EXPECT_TRUE(CheckPolicyRefreshed());

  // Make sure invalidation is not acknowledged until the store is loaded.
  EXPECT_FALSE(IsInvalidationAcknowledged(inv));
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
  EXPECT_TRUE(CheckInvalidationInfo(V(12), "test_payload"));
  StorePolicy(POLICY_OBJECT_A, V(12));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv));
  EXPECT_TRUE(CheckInvalidationInfo(0, std::string()));
  EXPECT_EQ(V(12), GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, HandleMultipleInvalidations) {
  // Generate multiple invalidations.
  StorePolicy(POLICY_OBJECT_A);
  StartInvalidator();
  invalidation::Invalidation inv1 =
      FireInvalidation(POLICY_OBJECT_A, V(1), "test1");
  EXPECT_TRUE(CheckInvalidationInfo(V(1), "test1"));
  invalidation::Invalidation inv2 =
      FireInvalidation(POLICY_OBJECT_A, V(2), "test2");
  EXPECT_TRUE(CheckInvalidationInfo(V(2), "test2"));
  invalidation::Invalidation inv3 =
      FireInvalidation(POLICY_OBJECT_A, V(3), "test3");
  EXPECT_TRUE(CheckInvalidationInfo(V(3), "test3"));

  // Make sure the replaced invalidations are acknowledged.
  EXPECT_TRUE(IsInvalidationAcknowledged(inv1));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv2));

  // Make sure the policy is refreshed once.
  EXPECT_TRUE(CheckPolicyRefreshed());

  // Make sure that the last invalidation is only acknowledged after the store
  // is loaded with the latest version.
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
  StorePolicy(POLICY_OBJECT_A, V(1));
  EXPECT_FALSE(IsInvalidationAcknowledged(inv3));
  EXPECT_EQ(V(1), GetHighestHandledInvalidationVersion());
  StorePolicy(POLICY_OBJECT_A, V(2));
  EXPECT_FALSE(IsInvalidationAcknowledged(inv3));
  EXPECT_EQ(V(2), GetHighestHandledInvalidationVersion());
  StorePolicy(POLICY_OBJECT_A, V(3));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv3));
  EXPECT_EQ(V(3), GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest,
       InitialHighestHandledInvalidationVersionNonZero) {
  StorePolicy(POLICY_OBJECT_A);
  StartInvalidator(true, /* initialize */
                   true, /* start_refresh_scheduler */
                   V(2)  /* highest_handled_invalidation_version*/);

  // Check that an invalidation whose version is lower than the highest handled
  // so far is acknowledged but ignored otherwise.
  invalidation::Invalidation inv1 =
      FireInvalidation(POLICY_OBJECT_A, V(1), "test1");
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_TRUE(CheckInvalidationInfo(0, std::string()));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv1));
  EXPECT_EQ(V(2), GetHighestHandledInvalidationVersion());

  // Check that an invalidation whose version matches the highest handled so far
  // is acknowledged but ignored otherwise.
  invalidation::Invalidation inv2 =
      FireInvalidation(POLICY_OBJECT_A, V(2), "test2");
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_TRUE(CheckInvalidationInfo(0, std::string()));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv2));
  EXPECT_EQ(V(2), GetHighestHandledInvalidationVersion());

  // Check that an invalidation whose version is higher than the highest handled
  // so far is handled, causing a policy refresh.
  invalidation::Invalidation inv3 =
      FireInvalidation(POLICY_OBJECT_A, V(3), "test3");
  EXPECT_TRUE(CheckPolicyRefreshed());
  EXPECT_TRUE(CheckInvalidationInfo(V(3), "test3"));
  StorePolicy(POLICY_OBJECT_A, V(3));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv3));
  EXPECT_EQ(V(3), GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, AcknowledgeBeforeRefresh) {
  // Generate an invalidation.
  StorePolicy(POLICY_OBJECT_A);
  StartInvalidator();
  invalidation::Invalidation inv =
      FireInvalidation(POLICY_OBJECT_A, V(3), "test");

  // Ensure that the policy is not refreshed and the invalidation is
  // acknowledged if the store is loaded with the latest version before the
  // refresh can occur.
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
  StorePolicy(POLICY_OBJECT_A, V(3));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv));
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_EQ(V(3), GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, NoCallbackAfterShutdown) {
  // Generate an invalidation.
  StorePolicy(POLICY_OBJECT_A);
  StartInvalidator();
  invalidation::Invalidation inv =
      FireInvalidation(POLICY_OBJECT_A, V(3), "test");

  // Ensure that the policy refresh is not made after the invalidator is shut
  // down.
  ShutdownInvalidator();
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
  DestroyInvalidator();
}

TEST_F(CloudPolicyInvalidatorTest, StateChanged) {
  // Test invalidation service state changes while not registered.
  StartInvalidator();
  DisableInvalidationService();
  EnableInvalidationService();
  EXPECT_FALSE(InvalidationsEnabled());

  // Test invalidation service state changes while registered.
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_TRUE(InvalidationsEnabled());
  DisableInvalidationService();
  EXPECT_FALSE(InvalidationsEnabled());
  DisableInvalidationService();
  EXPECT_FALSE(InvalidationsEnabled());
  EnableInvalidationService();
  EXPECT_TRUE(InvalidationsEnabled());
  EnableInvalidationService();
  EXPECT_TRUE(InvalidationsEnabled());

  // Test registration changes with invalidation service enabled.
  StorePolicy(POLICY_OBJECT_NONE);
  EXPECT_FALSE(InvalidationsEnabled());
  StorePolicy(POLICY_OBJECT_NONE);
  EXPECT_FALSE(InvalidationsEnabled());
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_TRUE(InvalidationsEnabled());
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_TRUE(InvalidationsEnabled());

  // Test registration changes with invalidation service disabled.
  DisableInvalidationService();
  EXPECT_FALSE(InvalidationsEnabled());
  StorePolicy(POLICY_OBJECT_NONE);
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_FALSE(InvalidationsEnabled());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, Disconnect) {
  // Generate an invalidation.
  StorePolicy(POLICY_OBJECT_A);
  StartInvalidator();
  invalidation::Invalidation inv =
      FireInvalidation(POLICY_OBJECT_A, V(1), "test");
  EXPECT_TRUE(InvalidationsEnabled());

  // Ensure that the policy is not refreshed after disconnecting the core, but
  // a call to indicate that invalidations are disabled is made.
  DisconnectCore();
  EXPECT_TRUE(CheckPolicyNotRefreshed());

  // Ensure that invalidation service events do not cause refreshes while the
  // invalidator is stopped.
  EXPECT_TRUE(IsUnsent(FireInvalidation(POLICY_OBJECT_A, V(2), "test")));
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  DisableInvalidationService();
  EnableInvalidationService();

  // Connect and disconnect without starting the refresh scheduler.
  ConnectCore();
  EXPECT_TRUE(IsUnsent(FireInvalidation(POLICY_OBJECT_A, V(3), "test")));
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  DisconnectCore();
  EXPECT_TRUE(IsUnsent(FireInvalidation(POLICY_OBJECT_A, V(4), "test")));
  EXPECT_TRUE(CheckPolicyNotRefreshed());

  // Ensure that the invalidator returns to normal after reconnecting.
  ConnectCore();
  StartRefreshScheduler();
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_TRUE(InvalidationsEnabled());
  FireInvalidation(POLICY_OBJECT_A, V(5), "test");
  EXPECT_TRUE(CheckInvalidationInfo(V(5), "test"));
  EXPECT_TRUE(CheckPolicyRefreshed());
  DisableInvalidationService();
  EXPECT_FALSE(InvalidationsEnabled());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

class CloudPolicyInvalidatorOwnerNameTest
    : public CloudPolicyInvalidatorTestBase {
 protected:
  PolicyInvalidationScope GetPolicyInvalidationScope() const override {
    return scope_;
  }

  PolicyInvalidationScope scope_;
};

TEST_F(CloudPolicyInvalidatorOwnerNameTest, GetOwnerNameForUserScope) {
  scope_ = PolicyInvalidationScope::kUser;
  StartInvalidator(false, /* initialize */
                   false, /* start_refresh_scheduler */
                   0 /* highest_handled_invalidation_version*/);
  ASSERT_TRUE(invalidator());
  EXPECT_EQ("CloudPolicy.User", invalidator()->GetOwnerName());
}

TEST_F(CloudPolicyInvalidatorOwnerNameTest, GetOwnerNameForDeviceScope) {
  scope_ = PolicyInvalidationScope::kDevice;
  StartInvalidator(false, /* initialize */
                   false, /* start_refresh_scheduler */
                   0 /* highest_handled_invalidation_version*/);
  ASSERT_TRUE(invalidator());
  EXPECT_EQ("CloudPolicy.Device", invalidator()->GetOwnerName());
}

TEST_F(CloudPolicyInvalidatorOwnerNameTest,
       GetOwnerNameForDeviceLocalAccountScope) {
  scope_ = PolicyInvalidationScope::kDeviceLocalAccount;
  StartInvalidator(false, /* initialize */
                   false, /* start_refresh_scheduler */
                   0 /* highest_handled_invalidation_version*/);
  ASSERT_TRUE(invalidator());
  EXPECT_EQ("CloudPolicy.DeviceLocalAccount.test_account",
            invalidator()->GetOwnerName());
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
  CloudPolicyInvalidatorUserTypedTest() = default;

  base::HistogramBase::Count GetCount(MetricPolicyRefresh metric);
  base::HistogramBase::Count GetInvalidationCount(PolicyInvalidationType type);

 private:
  // CloudPolicyInvalidatorTest:
  PolicyInvalidationScope GetPolicyInvalidationScope() const override;

  base::HistogramTester histogram_tester_;
};

base::HistogramBase::Count CloudPolicyInvalidatorUserTypedTest::GetCount(
    MetricPolicyRefresh metric) {
  const char* metric_name = CloudPolicyInvalidator::GetPolicyRefreshMetricName(
      GetPolicyInvalidationScope());
  return histogram_tester_.GetHistogramSamplesSinceCreation(metric_name)
      ->GetCount(metric);
}

base::HistogramBase::Count
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

TEST_P(CloudPolicyInvalidatorUserTypedTest, RefreshMetricsUnregistered) {
  // Store loads occurring before invalidation registration are not counted.
  StartInvalidator();
  StorePolicy(POLICY_OBJECT_NONE, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_NONE, 0, true /* policy_changed */);
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_CHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_CHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_UNCHANGED));

  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_P(CloudPolicyInvalidatorUserTypedTest, RefreshMetricsNoInvalidations) {
  // Store loads occurring while registered should be differentiated depending
  // on whether the invalidation service was enabled or not.
  StorePolicy(POLICY_OBJECT_A);
  StartInvalidator();

  // Initially, invalidations have not been enabled past the grace period, so
  // invalidations are OFF.
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, true /* policy_changed */);
  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));

  // If the clock advances less than the grace period, invalidations are OFF.
  AdvanceClock(base::Seconds(1));
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, true /* policy_changed */);
  EXPECT_EQ(2, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));

  // After the grace period elapses, invalidations are ON.
  AdvanceClock(base::Seconds(CloudPolicyInvalidator::kInvalidationGracePeriod));
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, true /* policy_changed */);
  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_CHANGED));

  // After the invalidation service is disabled, invalidations are OFF.
  DisableInvalidationService();
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, true /* policy_changed */);
  EXPECT_EQ(3, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));

  // Enabling the invalidation service results in a new grace period, so
  // invalidations are OFF.
  EnableInvalidationService();
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, true /* policy_changed */);
  EXPECT_EQ(4, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));

  // After the grace period elapses, invalidations are ON.
  AdvanceClock(base::Seconds(CloudPolicyInvalidator::kInvalidationGracePeriod));
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, true /* policy_changed */);

  EXPECT_EQ(2, GetCount(METRIC_POLICY_REFRESH_CHANGED));
  EXPECT_EQ(4, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));
  EXPECT_EQ(6, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_CHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_UNCHANGED));

  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_P(CloudPolicyInvalidatorUserTypedTest, RefreshMetricsInvalidation) {
  // Store loads after an invalidation are not counted as invalidated.
  StartInvalidator();
  StorePolicy(POLICY_OBJECT_A);
  AdvanceClock(base::Seconds(CloudPolicyInvalidator::kInvalidationGracePeriod));
  FireInvalidation(POLICY_OBJECT_A, V(5), "test");
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, true /* policy_changed */);
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
  StorePolicy(POLICY_OBJECT_A, V(5), true /* policy_changed */);
  EXPECT_EQ(V(5), GetHighestHandledInvalidationVersion());

  // Store loads after the invalidation is complete are not counted as
  // invalidated.
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, true /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, true /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, true /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);

  EXPECT_EQ(4, GetCount(METRIC_POLICY_REFRESH_CHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));
  EXPECT_EQ(5, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_CHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_UNCHANGED));

  EXPECT_EQ(V(5), GetHighestHandledInvalidationVersion());
}

TEST_P(CloudPolicyInvalidatorUserTypedTest, ExpiredInvalidations) {
  StorePolicy(POLICY_OBJECT_A, 0, false, Now());
  StartInvalidator();

  // Invalidations fired before the last fetch time (adjusted by max time delta)
  // should be ignored (and count as expired).
  base::Time time = Now() - (invalidation_timeouts::kMaxInvalidationTimeDelta +
                             base::Seconds(300));
  invalidation::Invalidation inv =
      FireInvalidation(POLICY_OBJECT_A, GetVersion(time), "test");
  ASSERT_TRUE(IsInvalidationAcknowledged(inv));
  ASSERT_TRUE(CheckPolicyNotRefreshed());

  inv = FireInvalidation(POLICY_OBJECT_A, GetVersion(time), "");  // no payload
  ASSERT_TRUE(IsInvalidationAcknowledged(inv));
  ASSERT_TRUE(CheckPolicyNotRefreshed());

  time += base::Minutes(5) - base::Seconds(1);
  inv = FireInvalidation(POLICY_OBJECT_A, GetVersion(time), "test");
  ASSERT_TRUE(IsInvalidationAcknowledged(inv));
  ASSERT_TRUE(CheckPolicyNotRefreshed());

  // Invalidations fired after the last fetch should not be ignored.
  time += base::Seconds(1);
  inv = FireInvalidation(POLICY_OBJECT_A, GetVersion(time), "");  // no payload
  ASSERT_FALSE(IsInvalidationAcknowledged(inv));
  ASSERT_TRUE(CheckPolicyRefreshed(
      base::Minutes(CloudPolicyInvalidator::kMissingPayloadDelay)));

  time += base::Minutes(10);
  inv = FireInvalidation(POLICY_OBJECT_A, GetVersion(time), "test");
  ASSERT_FALSE(IsInvalidationAcknowledged(inv));
  ASSERT_TRUE(CheckPolicyRefreshed());

  time += base::Minutes(10);
  inv = FireInvalidation(POLICY_OBJECT_A, GetVersion(time), "test");
  ASSERT_FALSE(IsInvalidationAcknowledged(inv));
  ASSERT_TRUE(CheckPolicyRefreshed());

  time += base::Minutes(10);
  inv = FireInvalidation(POLICY_OBJECT_A, GetVersion(time), "test");
  ASSERT_FALSE(IsInvalidationAcknowledged(inv));
  ASSERT_TRUE(CheckPolicyRefreshed());

  // Verify that received invalidations metrics are correct.
  EXPECT_EQ(1, GetInvalidationCount(POLICY_INVALIDATION_TYPE_NO_PAYLOAD));
  EXPECT_EQ(3, GetInvalidationCount(POLICY_INVALIDATION_TYPE_NORMAL));
  EXPECT_EQ(1,
            GetInvalidationCount(POLICY_INVALIDATION_TYPE_NO_PAYLOAD_EXPIRED));
  EXPECT_EQ(2, GetInvalidationCount(POLICY_INVALIDATION_TYPE_EXPIRED));

  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

INSTANTIATE_TEST_SUITE_P(
    CloudPolicyInvalidatorUserTypedTestInstance,
    CloudPolicyInvalidatorUserTypedTest,
    testing::Values(PolicyInvalidationScope::kUser,
                    PolicyInvalidationScope::kDevice,
                    PolicyInvalidationScope::kDeviceLocalAccount));

}  // namespace policy

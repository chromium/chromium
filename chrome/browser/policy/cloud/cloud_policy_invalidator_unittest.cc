// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/sample_map.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_invalidator.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

class CloudPolicyInvalidatorTest : public testing::Test {
 protected:
  // Policy objects which can be used in tests.
  enum PolicyObject {
    POLICY_OBJECT_NONE,
    POLICY_OBJECT_A,
    POLICY_OBJECT_B
  };

  CloudPolicyInvalidatorTest();

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
    StorePolicy(object,
                invalidation_version,
                policy_changed,
                Now() - base::TimeDelta::FromMinutes(5));
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
  syncer::Invalidation FireInvalidation(PolicyObject object,
                                        int64_t version,
                                        const std::string& payload);

  // Causes the invalidation service to fire an invalidation with unknown
  // version.
  syncer::Invalidation FireUnknownVersionInvalidation(PolicyObject object);

  // Checks the expected value of the currently set invalidation info.
  bool CheckInvalidationInfo(int64_t version, const std::string& payload);

  // Checks that the policy was not refreshed due to an invalidation.
  bool CheckPolicyNotRefreshed();

  // Checks that the policy was refreshed due to an invalidation within an
  // appropriate timeframe depending on whether the invalidation had unknown
  // version.
  bool CheckPolicyRefreshed();
  bool CheckPolicyRefreshedWithUnknownVersion();

  bool IsUnsent(const syncer::Invalidation& invalidation);

  // Returns the invalidations enabled state set by the invalidator on the
  // refresh scheduler.
  bool InvalidationsEnabled();

  // Determines if the invalidation with the given ack handle has been
  // acknowledged.
  bool IsInvalidationAcknowledged(const syncer::Invalidation& invalidation);

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

  // Get the policy type that the |invalidator_| is responsible for.
  virtual em::DeviceRegisterRequest::Type GetPolicyType() const;

 private:
  // Checks that the policy was refreshed due to an invalidation with the given
  // base delay.
  bool CheckPolicyRefreshed(base::TimeDelta delay);

  // Checks that the policy was refreshed the given number of times.
  bool CheckPolicyRefreshCount(int count);

  // Returns the object id of the given policy object.
  const invalidation::ObjectId& GetPolicyObjectId(PolicyObject object) const;

  base::MessageLoop loop_;

  // Objects the invalidator depends on.
  invalidation::FakeInvalidationService invalidation_service_;
  MockCloudPolicyStore store_;
  CloudPolicyCore core_;
  MockCloudPolicyClient* client_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SimpleTestClock clock_;

  // The invalidator which will be tested.
  std::unique_ptr<CloudPolicyInvalidator> invalidator_;

  // Object ids for the test policy objects.
  invalidation::ObjectId object_id_a_;
  invalidation::ObjectId object_id_b_;

  // Fake policy values which are alternated to cause the store to report a
  // changed policy.
  const char* policy_value_a_;
  const char* policy_value_b_;

  // The currently used policy value.
  const char* policy_value_cur_;
};

CloudPolicyInvalidatorTest::CloudPolicyInvalidatorTest()
    : core_(dm_protocol::kChromeUserPolicyType,
            std::string(),
            &store_,
            loop_.task_runner(),
            network::TestNetworkConnectionTracker::CreateGetter()),
      client_(nullptr),
      task_runner_(new base::TestSimpleTaskRunner()),
      object_id_a_(135, "asdf"),
      object_id_b_(246, "zxcv"),
      policy_value_a_("asdf"),
      policy_value_b_("zxcv"),
      policy_value_cur_(policy_value_a_) {
  clock_.SetNow(base::Time::UnixEpoch() +
                base::TimeDelta::FromSeconds(987654321));
}

void CloudPolicyInvalidatorTest::TearDown() {
  if (invalidator_)
    invalidator_->Shutdown();
  core_.Disconnect();
}

void CloudPolicyInvalidatorTest::StartInvalidator(
    bool initialize,
    bool start_refresh_scheduler,
    int64_t highest_handled_invalidation_version) {
  invalidator_.reset(
      new CloudPolicyInvalidator(GetPolicyType(), &core_, task_runner_, &clock_,
                                 highest_handled_invalidation_version));
  if (start_refresh_scheduler) {
    ConnectCore();
    StartRefreshScheduler();
  }
  if (initialize)
    InitializeInvalidator();
}

void CloudPolicyInvalidatorTest::InitializeInvalidator() {
  invalidator_->Initialize(&invalidation_service_);
}

void CloudPolicyInvalidatorTest::ShutdownInvalidator() {
  invalidator_->Shutdown();
}

void CloudPolicyInvalidatorTest::DestroyInvalidator() {
  invalidator_.reset();
}

void CloudPolicyInvalidatorTest::ConnectCore() {
  client_ = new MockCloudPolicyClient();
  client_->SetDMToken("dm");
  core_.Connect(std::unique_ptr<CloudPolicyClient>(client_));
}

void CloudPolicyInvalidatorTest::StartRefreshScheduler() {
  core_.StartRefreshScheduler();
}

void CloudPolicyInvalidatorTest::DisconnectCore() {
  client_ = nullptr;
  core_.Disconnect();
}

void CloudPolicyInvalidatorTest::StorePolicy(PolicyObject object,
                                             int64_t invalidation_version,
                                             bool policy_changed,
                                             const base::Time& time) {
  em::PolicyData* data = new em::PolicyData();
  if (object != POLICY_OBJECT_NONE) {
    data->set_invalidation_source(GetPolicyObjectId(object).source());
    data->set_invalidation_name(GetPolicyObjectId(object).name());
  }
  data->set_timestamp(time.ToJavaTime());
  // Swap the policy value if a policy change is desired.
  if (policy_changed)
    policy_value_cur_ = policy_value_cur_ == policy_value_a_ ?
        policy_value_b_ : policy_value_a_;
  data->set_policy_value(policy_value_cur_);
  store_.invalidation_version_ = invalidation_version;
  store_.policy_.reset(data);
  base::DictionaryValue policies;
  policies.SetInteger(
      key::kMaxInvalidationFetchDelay,
      CloudPolicyInvalidator::kMaxFetchDelayMin);
  store_.policy_map_.LoadFrom(
      &policies,
      POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_MACHINE,
      POLICY_SOURCE_CLOUD);
  store_.NotifyStoreLoaded();
}

void CloudPolicyInvalidatorTest::DisableInvalidationService() {
  invalidation_service_.SetInvalidatorState(
      syncer::TRANSIENT_INVALIDATION_ERROR);
}

void CloudPolicyInvalidatorTest::EnableInvalidationService() {
  invalidation_service_.SetInvalidatorState(syncer::INVALIDATIONS_ENABLED);
}

syncer::Invalidation CloudPolicyInvalidatorTest::FireInvalidation(
    PolicyObject object,
    int64_t version,
    const std::string& payload) {
  syncer::Invalidation invalidation = syncer::Invalidation::Init(
      GetPolicyObjectId(object),
      version,
      payload);
  invalidation_service_.EmitInvalidationForTest(invalidation);
  return invalidation;
}

syncer::Invalidation CloudPolicyInvalidatorTest::FireUnknownVersionInvalidation(
    PolicyObject object) {
  syncer::Invalidation invalidation = syncer::Invalidation::InitUnknownVersion(
      GetPolicyObjectId(object));
  invalidation_service_.EmitInvalidationForTest(invalidation);
  return invalidation;
}

bool CloudPolicyInvalidatorTest::CheckInvalidationInfo(
    int64_t version,
    const std::string& payload) {
  MockCloudPolicyClient* client =
      static_cast<MockCloudPolicyClient*>(core_.client());
  return version == client->invalidation_version_ &&
      payload == client->invalidation_payload_;
}

bool CloudPolicyInvalidatorTest::CheckPolicyNotRefreshed() {
  return CheckPolicyRefreshCount(0);
}

bool CloudPolicyInvalidatorTest::CheckPolicyRefreshed() {
  return CheckPolicyRefreshed(base::TimeDelta());
}

bool CloudPolicyInvalidatorTest::IsUnsent(
    const syncer::Invalidation& invalidation) {
  return invalidation_service_.GetMockAckHandler()->IsUnsent(invalidation);
}

bool CloudPolicyInvalidatorTest::CheckPolicyRefreshedWithUnknownVersion() {
  return CheckPolicyRefreshed(base::TimeDelta::FromMinutes(
        CloudPolicyInvalidator::kMissingPayloadDelay));
}

bool CloudPolicyInvalidatorTest::InvalidationsEnabled() {
  return core_.refresh_scheduler()->invalidations_available();
}

bool CloudPolicyInvalidatorTest::IsInvalidationAcknowledged(
    const syncer::Invalidation& invalidation) {
  // The acknowledgement task is run through a WeakHandle that posts back to our
  // own thread.  We need to run any posted tasks before we can check
  // acknowledgement status.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsUnsent(invalidation));
  return !invalidation_service_.GetMockAckHandler()->IsUnacked(invalidation);
}

bool CloudPolicyInvalidatorTest::IsInvalidatorRegistered() {
  return !invalidation_service_.invalidator_registrar()
      .GetRegisteredIds(invalidator_.get()).empty();
}

int64_t CloudPolicyInvalidatorTest::GetHighestHandledInvalidationVersion()
    const {
  return invalidator_->highest_handled_invalidation_version();
}

void CloudPolicyInvalidatorTest::AdvanceClock(base::TimeDelta delta) {
  clock_.Advance(delta);
}

base::Time CloudPolicyInvalidatorTest::Now() {
  return clock_.Now();
}

int64_t CloudPolicyInvalidatorTest::V(int version) {
  return GetVersion(Now()) + version;
}

int64_t CloudPolicyInvalidatorTest::GetVersion(base::Time time) {
  return (time - base::Time::UnixEpoch()).InMicroseconds();
}

em::DeviceRegisterRequest::Type
CloudPolicyInvalidatorTest::GetPolicyType() const {
  return UserCloudPolicyInvalidator::GetPolicyType();
}

bool CloudPolicyInvalidatorTest::CheckPolicyRefreshed(base::TimeDelta delay) {
  base::TimeDelta max_delay = delay + base::TimeDelta::FromMilliseconds(
      CloudPolicyInvalidator::kMaxFetchDelayMin);

  if (!task_runner_->HasPendingTask())
    return false;
  base::TimeDelta actual_delay = task_runner_->FinalPendingTaskDelay();
  EXPECT_GE(actual_delay, delay);
  EXPECT_LE(actual_delay, max_delay);

  return CheckPolicyRefreshCount(1);
}

bool CloudPolicyInvalidatorTest::CheckPolicyRefreshCount(int count) {
  if (!client_) {
    task_runner_->RunUntilIdle();
    return count == 0;
  }

  // Clear any non-invalidation refreshes which may be pending.
  EXPECT_CALL(*client_, FetchPolicy()).Times(testing::AnyNumber());
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(client_);

  // Run the invalidator tasks then check for invalidation refreshes.
  EXPECT_CALL(*client_, FetchPolicy()).Times(count);
  task_runner_->RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  return testing::Mock::VerifyAndClearExpectations(client_);
}

const invalidation::ObjectId& CloudPolicyInvalidatorTest::GetPolicyObjectId(
    PolicyObject object) const {
  EXPECT_TRUE(object == POLICY_OBJECT_A || object == POLICY_OBJECT_B);
  return object == POLICY_OBJECT_A ? object_id_a_ : object_id_b_;
}

TEST_F(CloudPolicyInvalidatorTest, Uninitialized) {
  // No invalidations should be processed if the invalidator is not initialized.
  StartInvalidator(false, /* initialize */
                   true,  /* start_refresh_scheduler */
                   0      /* highest_handled_invalidation_version*/);
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_FALSE(IsInvalidatorRegistered());
  EXPECT_TRUE(IsUnsent(FireUnknownVersionInvalidation(POLICY_OBJECT_A)));
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
  EXPECT_TRUE(IsUnsent(FireUnknownVersionInvalidation(POLICY_OBJECT_A)));
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
  EXPECT_TRUE(IsUnsent(FireUnknownVersionInvalidation(POLICY_OBJECT_A)));
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
  FireUnknownVersionInvalidation(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckPolicyRefreshedWithUnknownVersion());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, RegisterOnStoreLoaded) {
  // No registration when store is not loaded.
  StartInvalidator();
  EXPECT_FALSE(IsInvalidatorRegistered());
  EXPECT_FALSE(InvalidationsEnabled());
  EXPECT_TRUE(IsUnsent(FireUnknownVersionInvalidation(POLICY_OBJECT_A)));
  EXPECT_TRUE(IsUnsent(FireUnknownVersionInvalidation(POLICY_OBJECT_B)));
  EXPECT_TRUE(CheckPolicyNotRefreshed());

  // No registration when store is loaded with no invalidation object id.
  StorePolicy(POLICY_OBJECT_NONE);
  EXPECT_FALSE(IsInvalidatorRegistered());
  EXPECT_FALSE(InvalidationsEnabled());
  EXPECT_TRUE(IsUnsent(FireUnknownVersionInvalidation(POLICY_OBJECT_A)));
  EXPECT_TRUE(IsUnsent(FireUnknownVersionInvalidation(POLICY_OBJECT_B)));
  EXPECT_TRUE(CheckPolicyNotRefreshed());

  // Check registration when store is loaded for object A.
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_TRUE(InvalidationsEnabled());
  FireUnknownVersionInvalidation(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckPolicyRefreshedWithUnknownVersion());
  EXPECT_TRUE(IsUnsent(FireUnknownVersionInvalidation(POLICY_OBJECT_B)));
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, ChangeRegistration) {
  // Register for object A.
  StartInvalidator();
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_TRUE(InvalidationsEnabled());
  FireUnknownVersionInvalidation(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckPolicyRefreshedWithUnknownVersion());
  EXPECT_TRUE(IsUnsent(FireUnknownVersionInvalidation(POLICY_OBJECT_B)));
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  syncer::Invalidation inv = FireUnknownVersionInvalidation(POLICY_OBJECT_A);

  // Check re-registration for object B. Make sure the pending invalidation for
  // object A is acknowledged without making the callback.
  StorePolicy(POLICY_OBJECT_B);
  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_TRUE(InvalidationsEnabled());
  EXPECT_TRUE(IsInvalidationAcknowledged(inv));
  EXPECT_TRUE(CheckPolicyNotRefreshed());

  // Make sure future invalidations for object A are ignored and for object B
  // are processed.
  EXPECT_TRUE(IsUnsent(FireUnknownVersionInvalidation(POLICY_OBJECT_A)));
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  FireUnknownVersionInvalidation(POLICY_OBJECT_B);
  EXPECT_TRUE(CheckPolicyRefreshedWithUnknownVersion());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, UnregisterOnStoreLoaded) {
  // Register for object A.
  StartInvalidator();
  StorePolicy(POLICY_OBJECT_A);
  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_TRUE(InvalidationsEnabled());
  FireUnknownVersionInvalidation(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckPolicyRefreshedWithUnknownVersion());

  // Check unregistration when store is loaded with no invalidation object id.
  syncer::Invalidation inv = FireUnknownVersionInvalidation(POLICY_OBJECT_A);
  EXPECT_FALSE(IsInvalidationAcknowledged(inv));
  StorePolicy(POLICY_OBJECT_NONE);
  EXPECT_FALSE(IsInvalidatorRegistered());
  EXPECT_TRUE(IsInvalidationAcknowledged(inv));
  EXPECT_FALSE(InvalidationsEnabled());
  EXPECT_TRUE(IsUnsent(FireUnknownVersionInvalidation(POLICY_OBJECT_A)));
  EXPECT_TRUE(IsUnsent(FireUnknownVersionInvalidation(POLICY_OBJECT_B)));
  EXPECT_TRUE(CheckPolicyNotRefreshed());

  // Check re-registration for object B.
  StorePolicy(POLICY_OBJECT_B);
  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_TRUE(InvalidationsEnabled());
  FireUnknownVersionInvalidation(POLICY_OBJECT_B);
  EXPECT_TRUE(CheckPolicyRefreshedWithUnknownVersion());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, HandleInvalidation) {
  // Register and fire invalidation
  StorePolicy(POLICY_OBJECT_A);
  StartInvalidator();
  EXPECT_TRUE(InvalidationsEnabled());
  syncer::Invalidation inv =
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

TEST_F(CloudPolicyInvalidatorTest, HandleInvalidationWithUnknownVersion) {
  // Register and fire invalidation with unknown version.
  StorePolicy(POLICY_OBJECT_A);
  StartInvalidator();
  syncer::Invalidation inv = FireUnknownVersionInvalidation(POLICY_OBJECT_A);

  // Make sure client info is not set until after the invalidation callback is
  // made.
  EXPECT_TRUE(CheckInvalidationInfo(0, std::string()));
  EXPECT_TRUE(CheckPolicyRefreshedWithUnknownVersion());
  EXPECT_TRUE(CheckInvalidationInfo(-1, std::string()));

  // Make sure invalidation is not acknowledged until the store is loaded.
  EXPECT_FALSE(IsInvalidationAcknowledged(inv));
  StorePolicy(POLICY_OBJECT_A, -1);
  EXPECT_TRUE(IsInvalidationAcknowledged(inv));
  EXPECT_TRUE(CheckInvalidationInfo(0, std::string()));
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, HandleMultipleInvalidations) {
  // Generate multiple invalidations.
  StorePolicy(POLICY_OBJECT_A);
  StartInvalidator();
  syncer::Invalidation inv1 = FireInvalidation(POLICY_OBJECT_A, V(1), "test1");
  EXPECT_TRUE(CheckInvalidationInfo(V(1), "test1"));
  syncer::Invalidation inv2 = FireInvalidation(POLICY_OBJECT_A, V(2), "test2");
  EXPECT_TRUE(CheckInvalidationInfo(V(2), "test2"));
  syncer::Invalidation inv3 = FireInvalidation(POLICY_OBJECT_A, V(3), "test3");
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
       HandleMultipleInvalidationsWithUnknownVersion) {
  // Validate that multiple invalidations with unknown version each generate
  // unique invalidation version numbers.
  StorePolicy(POLICY_OBJECT_A);
  StartInvalidator();
  syncer::Invalidation inv1 = FireUnknownVersionInvalidation(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckInvalidationInfo(0, std::string()));
  EXPECT_TRUE(CheckPolicyRefreshedWithUnknownVersion());
  EXPECT_TRUE(CheckInvalidationInfo(-1, std::string()));
  syncer::Invalidation inv2 = FireUnknownVersionInvalidation(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckInvalidationInfo(0, std::string()));
  EXPECT_TRUE(CheckPolicyRefreshedWithUnknownVersion());
  EXPECT_TRUE(CheckInvalidationInfo(-2, std::string()));
  syncer::Invalidation inv3 = FireUnknownVersionInvalidation(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckInvalidationInfo(0, std::string()));
  EXPECT_TRUE(CheckPolicyRefreshedWithUnknownVersion());
  EXPECT_TRUE(CheckInvalidationInfo(-3, std::string()));

  // Make sure the replaced invalidations are acknowledged.
  EXPECT_TRUE(IsInvalidationAcknowledged(inv1));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv2));

  // Make sure that the last invalidation is only acknowledged after the store
  // is loaded with the last unknown version.
  StorePolicy(POLICY_OBJECT_A, -1);
  EXPECT_FALSE(IsInvalidationAcknowledged(inv3));
  StorePolicy(POLICY_OBJECT_A, -2);
  EXPECT_FALSE(IsInvalidationAcknowledged(inv3));
  StorePolicy(POLICY_OBJECT_A, -3);
  EXPECT_TRUE(IsInvalidationAcknowledged(inv3));
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest,
       InitialHighestHandledInvalidationVersionNonZero) {
  StorePolicy(POLICY_OBJECT_A);
  StartInvalidator(true, /* initialize */
                   true, /* start_refresh_scheduler */
                   V(2)  /* highest_handled_invalidation_version*/);

  // Check that an invalidation whose version is lower than the highest handled
  // so far is acknowledged but ignored otherwise.
  syncer::Invalidation inv1 = FireInvalidation(POLICY_OBJECT_A, V(1), "test1");
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_TRUE(CheckInvalidationInfo(0, std::string()));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv1));
  EXPECT_EQ(V(2), GetHighestHandledInvalidationVersion());

  // Check that an invalidation with an unknown version is handled.
  syncer::Invalidation inv = FireUnknownVersionInvalidation(POLICY_OBJECT_A);
  EXPECT_TRUE(CheckPolicyRefreshedWithUnknownVersion());
  EXPECT_TRUE(CheckInvalidationInfo(-1, std::string()));
  StorePolicy(POLICY_OBJECT_A, -1);
  EXPECT_TRUE(IsInvalidationAcknowledged(inv));
  EXPECT_EQ(V(2), GetHighestHandledInvalidationVersion());

  // Check that an invalidation whose version matches the highest handled so far
  // is acknowledged but ignored otherwise.
  syncer::Invalidation inv2 = FireInvalidation(POLICY_OBJECT_A, V(2), "test2");
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_TRUE(CheckInvalidationInfo(0, std::string()));
  EXPECT_TRUE(IsInvalidationAcknowledged(inv2));
  EXPECT_EQ(V(2), GetHighestHandledInvalidationVersion());

  // Check that an invalidation whose version is higher than the highest handled
  // so far is handled, causing a policy refresh.
  syncer::Invalidation inv3 = FireInvalidation(POLICY_OBJECT_A, V(3), "test3");
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
  syncer::Invalidation inv = FireInvalidation(POLICY_OBJECT_A, V(3), "test");

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
  syncer::Invalidation inv = FireInvalidation(POLICY_OBJECT_A, V(3), "test");

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
  syncer::Invalidation inv = FireInvalidation(POLICY_OBJECT_A, V(1), "test");
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

class CloudPolicyInvalidatorUserTypedTest
    : public CloudPolicyInvalidatorTest,
      public testing::WithParamInterface<em::DeviceRegisterRequest::Type>  {
 protected:
  CloudPolicyInvalidatorUserTypedTest();
  virtual ~CloudPolicyInvalidatorUserTypedTest();

  // CloudPolicyInvalidatorTest:
  void SetUp() override;

  // Get the current count for the given metric.
  base::HistogramBase::Count GetCount(MetricPolicyRefresh metric);
  base::HistogramBase::Count GetInvalidationCount(PolicyInvalidationType type);

 private:
  // CloudPolicyInvalidatorTest:
  em::DeviceRegisterRequest::Type GetPolicyType() const override;

  // Get histogram samples for the given histogram.
  std::unique_ptr<base::HistogramSamples> GetHistogramSamples(
      const std::string& name) const;

  // Stores starting histogram counts for kMetricPolicyRefresh.
  std::unique_ptr<base::HistogramSamples> refresh_samples_;

  // Stores starting histogram counts for kMetricPolicyInvalidations.
  std::unique_ptr<base::HistogramSamples> invalidations_samples_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyInvalidatorUserTypedTest);
};

CloudPolicyInvalidatorUserTypedTest::CloudPolicyInvalidatorUserTypedTest() {
}

CloudPolicyInvalidatorUserTypedTest::~CloudPolicyInvalidatorUserTypedTest() {
}

void CloudPolicyInvalidatorUserTypedTest::SetUp() {
  refresh_samples_ = GetHistogramSamples(
      GetPolicyType() == em::DeviceRegisterRequest::DEVICE ?
          kMetricDevicePolicyRefresh : kMetricUserPolicyRefresh);
  invalidations_samples_ = GetHistogramSamples(
      GetPolicyType() == em::DeviceRegisterRequest::DEVICE ?
          kMetricDevicePolicyInvalidations : kMetricUserPolicyInvalidations);
}

base::HistogramBase::Count CloudPolicyInvalidatorUserTypedTest::GetCount(
    MetricPolicyRefresh metric) {
  return GetHistogramSamples(
      GetPolicyType() == em::DeviceRegisterRequest::DEVICE ?
          kMetricDevicePolicyRefresh : kMetricUserPolicyRefresh)->
              GetCount(metric) - refresh_samples_->GetCount(metric);
}

base::HistogramBase::Count
CloudPolicyInvalidatorUserTypedTest::GetInvalidationCount(
    PolicyInvalidationType type) {
  return GetHistogramSamples(
      GetPolicyType() == em::DeviceRegisterRequest::DEVICE ?
          kMetricDevicePolicyInvalidations : kMetricUserPolicyInvalidations)->
          GetCount(type) - invalidations_samples_->GetCount(type);
}

em::DeviceRegisterRequest::Type
CloudPolicyInvalidatorUserTypedTest::GetPolicyType() const {
  return GetParam();
}

std::unique_ptr<base::HistogramSamples>
CloudPolicyInvalidatorUserTypedTest::GetHistogramSamples(
    const std::string& name) const {
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(name);
  if (!histogram)
    return std::unique_ptr<base::HistogramSamples>(new base::SampleMap());
  return histogram->SnapshotSamples();
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
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  StorePolicy(POLICY_OBJECT_A, 0, false /* policy_changed */);
  StorePolicy(POLICY_OBJECT_A, 0, true /* policy_changed */);
  EXPECT_EQ(2, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));

  // After the grace period elapses, invalidations are ON.
  AdvanceClock(base::TimeDelta::FromSeconds(
      CloudPolicyInvalidator::kInvalidationGracePeriod));
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
  AdvanceClock(base::TimeDelta::FromSeconds(
      CloudPolicyInvalidator::kInvalidationGracePeriod));
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
  // Store loads after an invalidation are counted as invalidated, even if
  // the loads do not result in the invalidation being acknowledged.
  StartInvalidator();
  StorePolicy(POLICY_OBJECT_A);
  AdvanceClock(base::TimeDelta::FromSeconds(
      CloudPolicyInvalidator::kInvalidationGracePeriod));
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

  EXPECT_EQ(3, GetCount(METRIC_POLICY_REFRESH_CHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));
  EXPECT_EQ(4, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(2, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_CHANGED));
  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_UNCHANGED));
  EXPECT_EQ(V(5), GetHighestHandledInvalidationVersion());
}

TEST_P(CloudPolicyInvalidatorUserTypedTest, ExpiredInvalidations) {
  StorePolicy(POLICY_OBJECT_A, 0, false, Now());
  StartInvalidator();

  // Invalidations fired before the last fetch time (adjusted by max time delta)
  // should be ignored.
  base::Time time = Now() - base::TimeDelta::FromSeconds(
      CloudPolicyInvalidator::kMaxInvalidationTimeDelta + 300);
  syncer::Invalidation inv =
      FireInvalidation(POLICY_OBJECT_A, GetVersion(time), "test");
  ASSERT_TRUE(IsInvalidationAcknowledged(inv));
  ASSERT_TRUE(CheckPolicyNotRefreshed());

  time += base::TimeDelta::FromMinutes(5) - base::TimeDelta::FromSeconds(1);
  inv = FireInvalidation(POLICY_OBJECT_A, GetVersion(time), "test");
  ASSERT_TRUE(IsInvalidationAcknowledged(inv));
  ASSERT_TRUE(CheckPolicyNotRefreshed());

  // Invalidations fired after the last fetch should not be ignored.
  time += base::TimeDelta::FromSeconds(1);
  inv = FireInvalidation(POLICY_OBJECT_A, GetVersion(time), "test");
  ASSERT_FALSE(IsInvalidationAcknowledged(inv));
  ASSERT_TRUE(CheckPolicyRefreshed());

  time += base::TimeDelta::FromMinutes(10);
  inv = FireInvalidation(POLICY_OBJECT_A, GetVersion(time), "test");
  ASSERT_FALSE(IsInvalidationAcknowledged(inv));
  ASSERT_TRUE(CheckPolicyRefreshed());

  time += base::TimeDelta::FromMinutes(10);
  inv = FireInvalidation(POLICY_OBJECT_A, GetVersion(time), "test");
  ASSERT_FALSE(IsInvalidationAcknowledged(inv));
  ASSERT_TRUE(CheckPolicyRefreshed());

  // Unknown version invalidations fired just after the last fetch time should
  // be ignored.
  inv = FireUnknownVersionInvalidation(POLICY_OBJECT_A);
  ASSERT_TRUE(IsInvalidationAcknowledged(inv));
  ASSERT_TRUE(CheckPolicyNotRefreshed());

  AdvanceClock(base::TimeDelta::FromSeconds(
      CloudPolicyInvalidator::kUnknownVersionIgnorePeriod - 1));
  inv = FireUnknownVersionInvalidation(POLICY_OBJECT_A);
  ASSERT_TRUE(IsInvalidationAcknowledged(inv));
  ASSERT_TRUE(CheckPolicyNotRefreshed());

  // Unknown version invalidations fired past the ignore period should not be
  // ignored.
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  inv = FireUnknownVersionInvalidation(POLICY_OBJECT_A);
  ASSERT_FALSE(IsInvalidationAcknowledged(inv));
  ASSERT_TRUE(CheckPolicyRefreshedWithUnknownVersion());

  // Verify that received invalidations metrics are correct.
  EXPECT_EQ(1, GetInvalidationCount(POLICY_INVALIDATION_TYPE_NO_PAYLOAD));
  EXPECT_EQ(3, GetInvalidationCount(POLICY_INVALIDATION_TYPE_NORMAL));
  EXPECT_EQ(2,
            GetInvalidationCount(POLICY_INVALIDATION_TYPE_NO_PAYLOAD_EXPIRED));
  EXPECT_EQ(2, GetInvalidationCount(POLICY_INVALIDATION_TYPE_EXPIRED));
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

#if defined(OS_CHROMEOS)
INSTANTIATE_TEST_CASE_P(
    CloudPolicyInvalidatorUserTypedTestInstance,
    CloudPolicyInvalidatorUserTypedTest,
    testing::Values(em::DeviceRegisterRequest::USER,
                    em::DeviceRegisterRequest::DEVICE));
#elif defined(OS_ANDROID)
INSTANTIATE_TEST_CASE_P(
    CloudPolicyInvalidatorUserTypedTestInstance,
    CloudPolicyInvalidatorUserTypedTest,
    testing::Values(em::DeviceRegisterRequest::ANDROID_BROWSER));
#else
INSTANTIATE_TEST_CASE_P(
    CloudPolicyInvalidatorUserTypedTestInstance,
    CloudPolicyInvalidatorUserTypedTest,
    testing::Values(em::DeviceRegisterRequest::BROWSER));
#endif

}  // namespace policy

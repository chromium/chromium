// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <optional>
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
#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_invalidator.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/invalidation/impl/invalidator_registrar_with_memory.h"
#include "components/invalidation/invalidation_listener_impl.h"
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

using ::testing::_;
using ::testing::WithArg;

namespace em = enterprise_management;

namespace policy {

namespace {
// Invalidation topics which can be used in tests.
const invalidation::Topic kTopicA = "topic_a";
const invalidation::Topic kTopicB = "topic_b";

constexpr char kFakeSenderId[] = "fake_sender_id";
constexpr char kTestLogPrefix[] = "test";
constexpr char kFakeRegistrationToken[] = "fake_registration_token";
}  // namespace

class FakeRegistrationTokenHandler
    : public invalidation::RegistrationTokenHandler {
 public:
  ~FakeRegistrationTokenHandler() override = default;

  void OnRegistrationTokenReceived(const std::string& registration_token,
                                   base::Time token_end_of_live) override {
    registration_token_ = registration_token;
    token_end_of_live_ = token_end_of_live;
  }

  std::string get_registration_token() { return registration_token_; }

 private:
  std::string registration_token_;
  base::Time token_end_of_live_;
};

class MockInstanceIDDriver : public instance_id::InstanceIDDriver {
 public:
  MockInstanceIDDriver() : InstanceIDDriver(/*gcm_driver=*/nullptr) {}
  ~MockInstanceIDDriver() override = default;

  MOCK_METHOD(instance_id::InstanceID*,
              GetInstanceID,
              (const std::string& app_id),
              (override));
  MOCK_METHOD(void, RemoveInstanceID, (const std::string& app_id), (override));
  MOCK_METHOD(bool,
              ExistsInstanceID,
              (const std::string& app_id),
              (const override));
};

class MockInstanceID : public instance_id::InstanceID {
 public:
  MockInstanceID() : InstanceID("app_id", /*gcm_driver=*/nullptr) {}
  ~MockInstanceID() override = default;
  MOCK_METHOD(void, GetID, (GetIDCallback callback), (override));
  MOCK_METHOD(void,
              GetCreationTime,
              (GetCreationTimeCallback callback),
              (override));
  MOCK_METHOD(void,
              GetToken,
              (const std::string& authorized_entity,
               const std::string& scope,
               base::TimeDelta time_to_live,
               std::set<Flags> flags,
               GetTokenCallback callback),
              (override));
  MOCK_METHOD(void,
              ValidateToken,
              (const std::string& authorized_entity,
               const std::string& scope,
               const std::string& token,
               ValidateTokenCallback callback),
              (override));

 protected:
  MOCK_METHOD(void,
              DeleteTokenImpl,
              (const std::string& authorized_entity,
               const std::string& scope,
               DeleteTokenCallback callback),
              (override));
  MOCK_METHOD(void, DeleteIDImpl, (DeleteIDCallback callback), (override));
};

class CloudPolicyInvalidatorWithListenerTestBase : public testing::Test {
 protected:
  CloudPolicyInvalidatorWithListenerTestBase();

  void SetUp() override {
    ON_CALL(mock_instance_id_driver_,
            GetInstanceID(invalidation::InvalidationListener::kFmAppId))
        .WillByDefault(Return(&mock_instance_id_));
  }

  void TearDown() override;

  // Starts the invalidator which will be tested.
  // `initialize` determines if the invalidator should be initialized.
  // `start_refresh_scheduler` determines if the refresh scheduler should start.
  // `highest_handled_invalidation_version` is the highest invalidation version
  // that was handled already before this invalidator was created.
  void StartInvalidator(bool initialize,
                        bool start_refresh_scheduler,
                        int64_t highest_handled_invalidation_version);
  void StartInvalidator() {
    StartInvalidator(/*initialize=*/true,
                     /*start_refresh_scheduler=*/true,
                     /*highest_handled_invalidation_version=*/0);
  }

  void SetRegistrationTokenFetchState(const std::string& registration_token,
                                      instance_id::InstanceID::Result result) {
    ON_CALL(mock_instance_id_, GetToken(/*authorized_entity=*/kFakeSenderId,
                                        /*scope=*/instance_id::kGCMScope,
                                        /*time_to_live=*/
                                        invalidation::InvalidationListenerImpl::
                                            kRegistrationTokenTimeToLive,
                                        /*flags=*/_, /*callback=*/_))
        .WillByDefault(
            // Call the callback with `registration_token` and `result` as
            // arguments.
            WithArg<4>([registration_token,
                        result](MockInstanceID::GetTokenCallback callback) {
              std::move(callback).Run(registration_token, result);
            }));
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

  // Disables the invalidation service. It is enabled by default.
  void DisableInvalidationService();

  // Enables the invalidation service. It is enabled by default.
  void EnableInvalidationService();

  // Start the listener instance.
  void StartInvalidationListener();

  // Causes the invalidation service to fire an invalidation.
  invalidation::Invalidation FireInvalidation(const invalidation::Topic& topic,
                                              int64_t version,
                                              const std::string& payload);

  // Returns true if the invalidation info of the `core_`'s client matches the
  // passed invalidation's version and payload.
  bool ClientInvalidationInfoMatches(
      const invalidation::Invalidation& invalidation);

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

  // Determines if the invalidator has registered for a topic with the
  // invalidation service.
  // This implicitly also checks that that invalidator is registered as an
  // observer (otherwise, it could not have registered topics).
  bool IsInvalidatorRegistered(const invalidation::Topic& topic);

  // Returns the highest invalidation version that was handled already according
  // to the `invalidator_`.
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

  // Get the invalidation scope that the `invalidator_` is responsible for.
  virtual PolicyInvalidationScope GetPolicyInvalidationScope() const;

 private:
  // Checks that the policy was refreshed the given number of times.
  bool CheckPolicyRefreshCount(int count);

  base::test::SingleThreadTaskEnvironment task_environment_;

  // Objects the invalidator depends on.
  MockCloudPolicyStore store_;
  CloudPolicyCore core_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SimpleTestClock clock_;

  // The invalidator which will be tested.
  std::unique_ptr<CloudPolicyInvalidator> invalidator_;

  // Topics for the test policy objects.
  invalidation::Topic topic_a_;
  invalidation::Topic topic_b_;

  // Fake policy values which are alternated to cause the store to report a
  // changed policy.
  const std::string policy_value_a_;
  const std::string policy_value_b_;

  // The currently used policy value.
  std::string policy_value_cur_;

  const char* account_id_ = "test_account";

  std::unique_ptr<invalidation::InvalidationListener> invalidation_listener_;
  testing::NiceMock<MockInstanceIDDriver> mock_instance_id_driver_;
  testing::NiceMock<MockInstanceID> mock_instance_id_;
  gcm::FakeGCMDriver gcmDriver;
  FakeRegistrationTokenHandler fake_token_handler_;
};

CloudPolicyInvalidatorWithListenerTestBase::
    CloudPolicyInvalidatorWithListenerTestBase()
    : core_(dm_protocol::kChromeUserPolicyType,
            std::string(),
            &store_,
            task_environment_.GetMainThreadTaskRunner(),
            network::TestNetworkConnectionTracker::CreateGetter()),
      task_runner_(new base::TestSimpleTaskRunner()),
      policy_value_a_(kTopicA),
      policy_value_b_(kTopicB),
      policy_value_cur_(policy_value_a_) {
  clock_.SetNow(base::Time::UnixEpoch() + base::Seconds(987654321));
}

void CloudPolicyInvalidatorWithListenerTestBase::TearDown() {
  if (invalidator_) {
    invalidator_->Shutdown();
  }
  core_.Disconnect();
}

void CloudPolicyInvalidatorWithListenerTestBase::StartInvalidator(
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
  if (initialize) {
    InitializeInvalidator();
    if (start_refresh_scheduler) {
      invalidator_->OnExpectationChanged(
          invalidation::InvalidationsExpected::kYes);
    }
  }
}

void CloudPolicyInvalidatorWithListenerTestBase::InitializeInvalidator() {
  invalidation_listener_ = invalidation::InvalidationListener::Create(
      &gcmDriver, &mock_instance_id_driver_, kFakeSenderId, kTestLogPrefix);
  invalidator_->Initialize(invalidation_listener_.get());
}

void CloudPolicyInvalidatorWithListenerTestBase::ShutdownInvalidator() {
  invalidator_->Shutdown();
}

void CloudPolicyInvalidatorWithListenerTestBase::DestroyInvalidator() {
  invalidator_.reset();
}

void CloudPolicyInvalidatorWithListenerTestBase::ConnectCore() {
  std::unique_ptr<MockCloudPolicyClient> client =
      std::make_unique<MockCloudPolicyClient>();
  client->SetDMToken("dm");
  core_.Connect(std::move(client));
}

void CloudPolicyInvalidatorWithListenerTestBase::StartRefreshScheduler() {
  core_.StartRefreshScheduler();
}

void CloudPolicyInvalidatorWithListenerTestBase::DisconnectCore() {
  core_.Disconnect();
}

void CloudPolicyInvalidatorWithListenerTestBase::StorePolicy(
    int64_t invalidation_version,
    bool policy_changed,
    const base::Time& time) {
  auto data = std::make_unique<em::PolicyData>();
  data->set_timestamp(time.InMillisecondsSinceUnixEpoch());
  // Swap the policy value if a policy change is desired.
  if (policy_changed) {
    policy_value_cur_ = policy_value_cur_ == policy_value_a_ ? policy_value_b_
                                                             : policy_value_a_;
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

void CloudPolicyInvalidatorWithListenerTestBase::DisableInvalidationService() {
  invalidation_listener_->SetRegistrationUploadStatus(
      invalidation::InvalidationListener::RegistrationTokenUploadStatus::
          kFailed);
}

void CloudPolicyInvalidatorWithListenerTestBase::EnableInvalidationService() {
  invalidation_listener_->SetRegistrationUploadStatus(
      invalidation::InvalidationListener::RegistrationTokenUploadStatus::
          kSucceeded);
}

void CloudPolicyInvalidatorWithListenerTestBase::StartInvalidationListener() {
  SetRegistrationTokenFetchState(kFakeRegistrationToken,
                                 instance_id::InstanceID::SUCCESS);
  invalidation_listener_->Start(&fake_token_handler_);
}

invalidation::Invalidation
CloudPolicyInvalidatorWithListenerTestBase::FireInvalidation(
    const invalidation::Topic& topic,
    int64_t version,
    const std::string& payload) {
  invalidation::DirectInvalidation invalidation(topic, version, payload);
  invalidator_->OnInvalidationReceived(invalidation);
  return invalidation;
}

bool CloudPolicyInvalidatorWithListenerTestBase::
    ClientInvalidationInfoIsUnset() {
  MockCloudPolicyClient* client =
      static_cast<MockCloudPolicyClient*>(core_.client());
  return client->invalidation_version_ == 0 &&
         client->invalidation_payload_.empty();
}

bool CloudPolicyInvalidatorWithListenerTestBase::ClientInvalidationInfoMatches(
    const invalidation::Invalidation& invalidation) {
  MockCloudPolicyClient* client =
      static_cast<MockCloudPolicyClient*>(core_.client());
  return invalidation.version() == client->invalidation_version_ &&
         invalidation.payload() == client->invalidation_payload_;
}

bool CloudPolicyInvalidatorWithListenerTestBase::CheckPolicyNotRefreshed() {
  return CheckPolicyRefreshCount(0);
}

bool CloudPolicyInvalidatorWithListenerTestBase::InvalidationsEnabled() {
  return core_.refresh_scheduler()->invalidations_available();
}

bool CloudPolicyInvalidatorWithListenerTestBase::IsInvalidatorRegistered() {
  return invalidation_listener_ &&
         invalidation_listener_->HasObserver(invalidator_.get());
}

int64_t CloudPolicyInvalidatorWithListenerTestBase::
    GetHighestHandledInvalidationVersion() const {
  return invalidator_->highest_handled_invalidation_version();
}

void CloudPolicyInvalidatorWithListenerTestBase::AdvanceClock(
    base::TimeDelta delta) {
  clock_.Advance(delta);
}

base::Time CloudPolicyInvalidatorWithListenerTestBase::Now() {
  return clock_.Now();
}

int64_t CloudPolicyInvalidatorWithListenerTestBase::V(int version) {
  return GetVersion(Now()) + version;
}

int64_t CloudPolicyInvalidatorWithListenerTestBase::GetVersion(
    base::Time time) {
  return (time - base::Time::UnixEpoch()).InMicroseconds();
}

PolicyInvalidationScope
CloudPolicyInvalidatorWithListenerTestBase::GetPolicyInvalidationScope() const {
  return PolicyInvalidationScope::kUser;
}

bool CloudPolicyInvalidatorWithListenerTestBase::CheckPolicyRefreshed(
    base::TimeDelta delay) {
  base::TimeDelta max_delay =
      delay + base::Milliseconds(CloudPolicyInvalidator::kMaxFetchDelayMin);

  if (!task_runner_->HasPendingTask()) {
    return false;
  }
  base::TimeDelta actual_delay = task_runner_->FinalPendingTaskDelay();
  EXPECT_GE(actual_delay, delay);
  EXPECT_LE(actual_delay, max_delay);

  return CheckPolicyRefreshCount(1);
}

bool CloudPolicyInvalidatorWithListenerTestBase::CheckPolicyRefreshCount(
    int count) {
  MockCloudPolicyClient* client = (MockCloudPolicyClient*)core_.client();
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

class CloudPolicyInvalidatorWithListenerTest
    : public CloudPolicyInvalidatorWithListenerTestBase {};

TEST_F(CloudPolicyInvalidatorWithListenerTest, Uninitialized) {
  // No invalidations should be processed if the invalidator is not initialized.
  StartInvalidator(/*initialize=*/false,
                   /*start_refresh_scheduler=*/true,
                   /*highest_handled_invalidation_version=*/0);
  StorePolicy();
  EXPECT_FALSE(IsInvalidatorRegistered());
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorWithListenerTest, RefreshSchedulerNotStarted) {
  // No invalidations should be processed if the refresh scheduler is not
  // started.
  StartInvalidator(/*initialize=*/true,
                   /*start_refresh_scheduler=*/false,
                   /*highest_handled_invalidation_version=*/0);
  StartInvalidationListener();
  StorePolicy();
  EXPECT_FALSE(IsInvalidatorRegistered());
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorWithListenerTest, DisconnectCoreThenInitialize) {
  // No invalidations should be processed if the core is disconnected before
  // initialization.
  StartInvalidator(/*initialize=*/false,
                   /*start_refresh_scheduler=*/true,
                   /*highest_handled_invalidation_version=*/0);
  DisconnectCore();
  InitializeInvalidator();
  StorePolicy();
  EXPECT_FALSE(IsInvalidatorRegistered());
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorWithListenerTest,
       InitializeThenStartRefreshScheduler) {
  // Make sure registration occurs and invalidations are processed when
  // Initialize is called before starting the refresh scheduler.
  // Note that the reverse case (start refresh scheduler then initialize) is
  // the default behavior for the test fixture, so will be tested in most other
  // tests.
  StartInvalidator(/*initialize=*/true,
                   /*start_refresh_scheduler=*/false,
                   /*highest_handled_invalidation_version=*/0);
  ConnectCore();
  StartRefreshScheduler();
  StorePolicy();
  FireInvalidation(kTopicA, V(1), "test");
  EXPECT_TRUE(CheckPolicyRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorWithListenerTest, RegisterOnStoreLoaded) {
  // No registration when store is not loaded.
  StartInvalidator();
  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_TRUE(CheckPolicyNotRefreshed());

  StorePolicy();
  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_TRUE(CheckPolicyNotRefreshed());

  // Check registration when store is loaded.
  StartInvalidationListener();
  StorePolicy();
  FireInvalidation(kTopicA, V(5), "test");
  EXPECT_TRUE(CheckPolicyRefreshed());
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorWithListenerTest, UnregisterOnStoreLoaded) {
  StartInvalidator();
  StorePolicy();
  EXPECT_TRUE(InvalidationsEnabled());
  FireInvalidation(kTopicA, V(1), "test");
  EXPECT_TRUE(CheckPolicyRefreshed());

  invalidation::Invalidation inv = FireInvalidation(kTopicA, V(2), "test");
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv));
  StorePolicy();
  EXPECT_TRUE(IsInvalidatorRegistered());
  FireInvalidation(kTopicB, V(5), "test");
  EXPECT_TRUE(CheckPolicyRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorWithListenerTest, HandleInvalidation) {
  // Register and fire invalidation
  StorePolicy();
  StartInvalidator();
  EXPECT_TRUE(InvalidationsEnabled());
  const invalidation::Invalidation inv =
      FireInvalidation(kTopicA, V(12), "test_payload");

  // Make sure client info is set as soon as the invalidation is received.
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv));
  EXPECT_TRUE(CheckPolicyRefreshed());

  // Make sure invalidation data is not removed from the client until the store
  // is loaded.
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv));
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv));
  StorePolicy(V(12));
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(V(12), GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorWithListenerTest, HandleMultipleInvalidations) {
  // Generate multiple invalidations.
  StorePolicy();
  StartInvalidator();
  const invalidation::Invalidation inv1 =
      FireInvalidation(kTopicA, V(1), "test1");
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv1));
  const invalidation::Invalidation inv2 =
      FireInvalidation(kTopicA, V(2), "test2");
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv2));
  const invalidation::Invalidation inv3 =
      FireInvalidation(kTopicA, V(3), "test3");
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv3));

  // Make sure the policy is refreshed once.
  EXPECT_TRUE(CheckPolicyRefreshed());

  // Make sure that the invalidation data is only removed from the client after
  // the store is loaded with the latest version.
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
  StorePolicy(V(1));
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv3));
  EXPECT_EQ(V(1), GetHighestHandledInvalidationVersion());
  StorePolicy(V(2));
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv3));
  EXPECT_EQ(V(2), GetHighestHandledInvalidationVersion());
  StorePolicy(V(3));
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(V(3), GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorWithListenerTest,
       InitialHighestHandledInvalidationVersionNonZero) {
  StorePolicy();
  StartInvalidator(/*initialize=*/true,
                   /*start_refresh_scheduler=*/true,
                   /*highest_handled_invalidation_version=*/V(2));

  // Check that an invalidation whose version is lower than the highest handled
  // so far is acknowledged but ignored otherwise.
  const invalidation::Invalidation inv1 =
      FireInvalidation(kTopicA, V(1), "test1");
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(V(2), GetHighestHandledInvalidationVersion());

  // Check that an invalidation whose version matches the highest handled so far
  // is acknowledged but ignored otherwise.
  const invalidation::Invalidation inv2 =
      FireInvalidation(kTopicA, V(2), "test2");
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(V(2), GetHighestHandledInvalidationVersion());

  // Check that an invalidation whose version is higher than the highest handled
  // so far is handled, causing a policy refresh.
  const invalidation::Invalidation inv3 =
      FireInvalidation(kTopicA, V(3), "test3");
  EXPECT_TRUE(CheckPolicyRefreshed());
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv3));
  StorePolicy(V(3));
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(V(3), GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorWithListenerTest, StoreLoadedBeforeRefresh) {
  // Generate an invalidation.
  StorePolicy();
  StartInvalidator();
  const invalidation::Invalidation inv =
      FireInvalidation(kTopicA, V(3), "test");

  // Ensure that the policy is not refreshed and the invalidation is
  // data is removed from the client if the store is loaded with the latest
  // version before the refresh can occur.
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
  StorePolicy(V(3));
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_EQ(V(3), GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorWithListenerTest, NoCallbackAfterShutdown) {
  // Generate an invalidation.
  StorePolicy();
  StartInvalidator();
  invalidation::Invalidation inv = FireInvalidation(kTopicA, V(3), "test");

  // Ensure that the policy refresh is not made after the invalidator is shut
  // down.
  ShutdownInvalidator();
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
  DestroyInvalidator();
}

TEST_F(CloudPolicyInvalidatorWithListenerTest, StateChanged) {
  // Test invalidation service state changes while not registered.
  StartInvalidator();
  DisableInvalidationService();
  EnableInvalidationService();
  EXPECT_FALSE(InvalidationsEnabled());
  // Test invalidation service state changes while registered.
  StartInvalidationListener();
  StorePolicy();
  EXPECT_TRUE(InvalidationsEnabled());

  DisableInvalidationService();
  EXPECT_FALSE(InvalidationsEnabled());

  DisableInvalidationService();
  EXPECT_FALSE(InvalidationsEnabled());
  EnableInvalidationService();
  EXPECT_TRUE(InvalidationsEnabled());
  EnableInvalidationService();
  EXPECT_TRUE(InvalidationsEnabled());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

class CloudPolicyInvalidatorWithListenerOwnerNameTest
    : public CloudPolicyInvalidatorWithListenerTestBase {
 protected:
  PolicyInvalidationScope GetPolicyInvalidationScope() const override {
    return scope_;
  }

  PolicyInvalidationScope scope_;
};

TEST_F(CloudPolicyInvalidatorWithListenerOwnerNameTest,
       GetOwnerNameForUserScope) {
  scope_ = PolicyInvalidationScope::kUser;
  StartInvalidator(/*initialize=*/false,
                   /*start_refresh_scheduler=*/false,
                   /*highest_handled_invalidation_version=*/0);
  ASSERT_TRUE(invalidator());
  EXPECT_EQ("CloudPolicy.User", invalidator()->GetOwnerName());
}

TEST_F(CloudPolicyInvalidatorWithListenerOwnerNameTest,
       GetOwnerNameForDeviceScope) {
  scope_ = PolicyInvalidationScope::kDevice;
  StartInvalidator(/*initialize=*/false,
                   /*start_refresh_scheduler=*/false,
                   /*highest_handled_invalidation_version=*/0);
  ASSERT_TRUE(invalidator());
  EXPECT_EQ("CloudPolicy.Device", invalidator()->GetOwnerName());
}

TEST_F(CloudPolicyInvalidatorWithListenerOwnerNameTest,
       GetOwnerNameForDeviceLocalAccountScope) {
  scope_ = PolicyInvalidationScope::kDeviceLocalAccount;
  StartInvalidator(/*initialize=*/false,
                   /*start_refresh_scheduler=*/false,
                   /*highest_handled_invalidation_version=*/0);
  ASSERT_TRUE(invalidator());
  EXPECT_EQ("CloudPolicy.DeviceLocalAccount.test_account",
            invalidator()->GetOwnerName());
}

class CloudPolicyInvalidatorWithListenerUserTypedTest
    : public CloudPolicyInvalidatorWithListenerTestBase,
      public testing::WithParamInterface<PolicyInvalidationScope> {
 public:
  CloudPolicyInvalidatorWithListenerUserTypedTest(
      const CloudPolicyInvalidatorWithListenerUserTypedTest&) = delete;
  CloudPolicyInvalidatorWithListenerUserTypedTest& operator=(
      const CloudPolicyInvalidatorWithListenerUserTypedTest&) = delete;

 protected:
  CloudPolicyInvalidatorWithListenerUserTypedTest() = default;

  base::HistogramBase::Count GetCount(MetricPolicyRefresh metric);
  base::HistogramBase::Count GetInvalidationCount(PolicyInvalidationType type);

 private:
  // CloudPolicyInvalidatorWithListenerTest:
  PolicyInvalidationScope GetPolicyInvalidationScope() const override;

  base::HistogramTester histogram_tester_;
};

base::HistogramBase::Count
CloudPolicyInvalidatorWithListenerUserTypedTest::GetCount(
    MetricPolicyRefresh metric) {
  const char* metric_name = CloudPolicyInvalidator::GetPolicyRefreshMetricName(
      GetPolicyInvalidationScope());
  return histogram_tester_.GetHistogramSamplesSinceCreation(metric_name)
      ->GetCount(metric);
}

base::HistogramBase::Count
CloudPolicyInvalidatorWithListenerUserTypedTest::GetInvalidationCount(
    PolicyInvalidationType type) {
  const char* metric_name =
      CloudPolicyInvalidator::GetPolicyInvalidationMetricName(
          GetPolicyInvalidationScope());
  return histogram_tester_.GetHistogramSamplesSinceCreation(metric_name)
      ->GetCount(type);
}

PolicyInvalidationScope
CloudPolicyInvalidatorWithListenerUserTypedTest::GetPolicyInvalidationScope()
    const {
  return GetParam();
}

TEST_P(CloudPolicyInvalidatorWithListenerUserTypedTest,
       RefreshMetricsUnregistered) {
  // Store loads occurring before invalidation registration are not counted.
  StartInvalidator();
  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_CHANGED));
  EXPECT_EQ(2, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_CHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_UNCHANGED));

  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_P(CloudPolicyInvalidatorWithListenerUserTypedTest,
       RefreshMetricsNoInvalidations) {
  // Store loads occurring while registered should be differentiated depending
  // on whether the invalidation service was enabled or not.
  StorePolicy();
  StartInvalidator();

  // Initially, invalidations have not been enabled past the grace period, so
  // invalidations are OFF.
  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);
  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));

  // If the clock advances less than the grace period, invalidations are OFF.
  AdvanceClock(base::Seconds(1));
  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);
  EXPECT_EQ(2, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));

  // After the grace period elapses, invalidations are ON.
  AdvanceClock(base::Seconds(CloudPolicyInvalidator::kInvalidationGracePeriod));
  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);
  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_CHANGED));

  // After the invalidation service is disabled, invalidations are OFF.
  DisableInvalidationService();
  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);
  EXPECT_EQ(3, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));

  // Enabling the invalidation service results in a new grace period, so
  // invalidations are OFF.
  EnableInvalidationService();
  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);
  EXPECT_EQ(4, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));

  // After the grace period elapses, invalidations are ON.
  AdvanceClock(base::Seconds(CloudPolicyInvalidator::kInvalidationGracePeriod));
  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);

  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_CHANGED));
  EXPECT_EQ(5, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));
  EXPECT_EQ(6, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_CHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_UNCHANGED));

  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_P(CloudPolicyInvalidatorWithListenerUserTypedTest,
       RefreshMetricsInvalidation) {
  // Store loads after an invalidation are not counted as invalidated.
  StartInvalidator();
  StorePolicy();
  AdvanceClock(base::Seconds(CloudPolicyInvalidator::kInvalidationGracePeriod));
  FireInvalidation(kTopicA, V(5), "test");
  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
  StorePolicy(V(5), true /* policy_changed */);
  EXPECT_EQ(V(5), GetHighestHandledInvalidationVersion());

  // Store loads after the invalidation is complete are not counted as
  // invalidated.
  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);
  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);
  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);
  StorePolicy(0, /*policy_changed=*/false);

  EXPECT_EQ(4, GetCount(METRIC_POLICY_REFRESH_CHANGED));
  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));
  EXPECT_EQ(5, GetCount(METRIC_POLICY_REFRESH_UNCHANGED));
  EXPECT_EQ(1, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_CHANGED));
  EXPECT_EQ(0, GetCount(METRIC_POLICY_REFRESH_INVALIDATED_UNCHANGED));

  EXPECT_EQ(V(5), GetHighestHandledInvalidationVersion());
}

TEST_P(CloudPolicyInvalidatorWithListenerUserTypedTest, ExpiredInvalidations) {
  StorePolicy(0, false, Now());
  StartInvalidator();

  // Invalidations fired before the last fetch time (adjusted by max time delta)
  // should be ignored (and count as expired).
  base::Time time = Now() - (invalidation_timeouts::kMaxInvalidationTimeDelta +
                             base::Seconds(300));
  invalidation::Invalidation inv =
      FireInvalidation(kTopicA, GetVersion(time), "test");
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  ASSERT_TRUE(CheckPolicyNotRefreshed());

  inv = FireInvalidation(kTopicA, GetVersion(time), "");  // no payload
  ASSERT_TRUE(CheckPolicyNotRefreshed());

  time += base::Minutes(5) - base::Seconds(1);
  inv = FireInvalidation(kTopicA, GetVersion(time), "test");
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  ASSERT_TRUE(CheckPolicyNotRefreshed());

  // Invalidations fired after the last fetch should not be ignored.
  time += base::Seconds(1);
  inv = FireInvalidation(kTopicA, GetVersion(time), "");  // no payload
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  ASSERT_TRUE(CheckPolicyRefreshed(
      base::Minutes(CloudPolicyInvalidator::kMissingPayloadDelay)));

  time += base::Minutes(10);
  inv = FireInvalidation(kTopicA, GetVersion(time), "test");
  ASSERT_TRUE(ClientInvalidationInfoMatches(inv));
  ASSERT_TRUE(CheckPolicyRefreshed());

  time += base::Minutes(10);
  inv = FireInvalidation(kTopicA, GetVersion(time), "test");
  ASSERT_TRUE(ClientInvalidationInfoMatches(inv));
  ASSERT_TRUE(CheckPolicyRefreshed());

  time += base::Minutes(10);
  inv = FireInvalidation(kTopicA, GetVersion(time), "test");
  ASSERT_TRUE(ClientInvalidationInfoMatches(inv));
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
    CloudPolicyInvalidatorWithListenerUserTypedTestInstance,
    CloudPolicyInvalidatorWithListenerUserTypedTest,
    testing::Values(PolicyInvalidationScope::kUser,
                    PolicyInvalidationScope::kDevice,
                    PolicyInvalidationScope::kDeviceLocalAccount));

}  // namespace policy

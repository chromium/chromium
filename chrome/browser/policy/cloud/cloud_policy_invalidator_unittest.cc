// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"

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
#include "chrome/browser/policy/cloud/user_cloud_policy_invalidator.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/invalidation/invalidation_listener.h"
#include "components/invalidation/invalidation_listener_impl.h"
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

// Fake policy values which are alternated to cause the store to report a
// changed policy.
constexpr char kPolicyValueA[] = "policyValueA";
constexpr char kPolicyValueB[] = "policyValueB";

constexpr int64_t kFakeProjectNumber = 1234567890;
constexpr char kFakeProjectNumberStr[] = "1234567890";
constexpr char kTestLogPrefix[] = "test";
constexpr char kFakeRegistrationToken[] = "fake_registration_token";

std::string GetGcmAppId(auto project_number) {
  return base::StrCat(
      {invalidation::InvalidationListener::kFmAppId, "-", project_number});
}
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

class CloudPolicyInvalidatorTestBase : public testing::Test {
 protected:
  CloudPolicyInvalidatorTestBase();

  void SetUp() override {
    ON_CALL(mock_instance_id_driver_,
            GetInstanceID(GetGcmAppId(kFakeProjectNumberStr)))
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
    ON_CALL(mock_instance_id_,
            GetToken(/*authorized_entity=*/kFakeProjectNumberStr,
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
  void DisableInvalidationListener();

  // Enables the invalidation service. It is enabled by default.
  void EnableInvalidationListener();

  // Start the listener instance.
  void StartInvalidationListener();

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

  // The currently used policy value.
  std::string policy_value_cur_;

  const char* account_id_ = "test_account";

  std::unique_ptr<invalidation::InvalidationListener> invalidation_listener_;
  testing::NiceMock<MockInstanceIDDriver> mock_instance_id_driver_;
  testing::NiceMock<MockInstanceID> mock_instance_id_;
  gcm::FakeGCMDriver gcmDriver;
  FakeRegistrationTokenHandler fake_token_handler_;
};

CloudPolicyInvalidatorTestBase::CloudPolicyInvalidatorTestBase()
    : core_(dm_protocol::kChromeUserPolicyType,
            std::string(),
            &store_,
            task_environment_.GetMainThreadTaskRunner(),
            network::TestNetworkConnectionTracker::CreateGetter()),
      task_runner_(new base::TestSimpleTaskRunner()),
      policy_value_cur_(kPolicyValueA) {
  clock_.SetNow(base::Time::UnixEpoch() + base::Seconds(987654321));
}

void CloudPolicyInvalidatorTestBase::TearDown() {
  if (invalidator_) {
    invalidator_->Shutdown();
  }
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
  if (initialize) {
    InitializeInvalidator();
    if (start_refresh_scheduler) {
      invalidator_->OnExpectationChanged(
          invalidation::InvalidationsExpected::kYes);
    }
  }
}

void CloudPolicyInvalidatorTestBase::InitializeInvalidator() {
  invalidation_listener_ = invalidation::InvalidationListener::Create(
      &gcmDriver, &mock_instance_id_driver_, kFakeProjectNumber,
      kTestLogPrefix);
  invalidator_->Initialize(invalidation_listener_.get());
}

void CloudPolicyInvalidatorTestBase::ShutdownInvalidator() {
  invalidator_->Shutdown();
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
  invalidation_listener_->SetRegistrationUploadStatus(
      invalidation::InvalidationListener::RegistrationTokenUploadStatus::
          kFailed);
}

void CloudPolicyInvalidatorTestBase::EnableInvalidationListener() {
  invalidation_listener_->SetRegistrationUploadStatus(
      invalidation::InvalidationListener::RegistrationTokenUploadStatus::
          kSucceeded);
}

void CloudPolicyInvalidatorTestBase::StartInvalidationListener() {
  SetRegistrationTokenFetchState(kFakeRegistrationToken,
                                 instance_id::InstanceID::SUCCESS);
  invalidation_listener_->Start(&fake_token_handler_);
}

invalidation::DirectInvalidation
CloudPolicyInvalidatorTestBase::FireInvalidation(int64_t version,
                                                 const std::string& payload) {
  const invalidation::DirectInvalidation invalidation("USER_POLICY_FETCH",
                                                      version, payload);
  invalidator_->OnInvalidationReceived(invalidation);
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
  return invalidation_listener_ &&
         invalidation_listener_->HasObserver(invalidator_.get());
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

TEST_F(CloudPolicyInvalidatorTest, Uninitialized) {
  // No invalidations should be processed if the invalidator is not initialized.
  StartInvalidator(/*initialize=*/false,
                   /*start_refresh_scheduler=*/true,
                   /*highest_handled_invalidation_version=*/0);
  StorePolicy();
  EXPECT_FALSE(IsInvalidatorRegistered());
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, RefreshSchedulerNotStarted) {
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

TEST_F(CloudPolicyInvalidatorTest, DisconnectCoreThenInitialize) {
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

TEST_F(CloudPolicyInvalidatorTest, DisconnectCoreThenIgnoreInvalidations) {
  StartInvalidator();
  StorePolicy();

  // Disconnect core, change invalidations state and fire invalidation. The
  // invalidations should be enabled but ignored.
  DisconnectCore();
  DisableInvalidationListener();
  EnableInvalidationListener();
  FireInvalidation(V(1), "test");

  EXPECT_TRUE(IsInvalidatorRegistered());
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, InitializeThenStartRefreshScheduler) {
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
  FireInvalidation(V(1), "test");
  EXPECT_TRUE(CheckPolicyRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, RegisterOnStoreLoaded) {
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
  FireInvalidation(V(5), "test");
  EXPECT_TRUE(CheckPolicyRefreshed());
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, UnregisterOnStoreLoaded) {
  StartInvalidator();
  StorePolicy();
  EXPECT_TRUE(InvalidationsEnabled());
  FireInvalidation(V(1), "test");
  EXPECT_TRUE(CheckPolicyRefreshed());

  const invalidation::DirectInvalidation inv = FireInvalidation(V(2), "test");
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv));
  StorePolicy();
  EXPECT_TRUE(IsInvalidatorRegistered());
  FireInvalidation(V(5), "test");
  EXPECT_TRUE(CheckPolicyRefreshed());
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, HandleInvalidation) {
  // Register and fire invalidation
  StorePolicy();
  StartInvalidator();
  EXPECT_TRUE(InvalidationsEnabled());
  const invalidation::DirectInvalidation inv =
      FireInvalidation(V(12), "test_payload");

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

TEST_F(CloudPolicyInvalidatorTest, HandleInvalidationBeforePolicyLoaded) {
  // Register and fire invalidation
  StartInvalidator();
  EXPECT_TRUE(InvalidationsEnabled());
  const invalidation::DirectInvalidation inv =
      FireInvalidation(V(12), "test_payload");

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

TEST_F(CloudPolicyInvalidatorTest, HandleMultipleInvalidations) {
  // Generate multiple invalidations.
  StorePolicy();
  StartInvalidator();
  const invalidation::DirectInvalidation inv1 = FireInvalidation(V(1), "test1");
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv1));
  const invalidation::DirectInvalidation inv2 = FireInvalidation(V(2), "test2");
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv2));
  const invalidation::DirectInvalidation inv3 = FireInvalidation(V(3), "test3");
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

TEST_F(CloudPolicyInvalidatorTest,
       InitialHighestHandledInvalidationVersionNonZero) {
  StorePolicy();
  StartInvalidator(/*initialize=*/true,
                   /*start_refresh_scheduler=*/true,
                   /*highest_handled_invalidation_version=*/V(2));

  // Check that an invalidation whose version is lower than the highest handled
  // so far is acknowledged but ignored otherwise.
  const invalidation::DirectInvalidation inv1 = FireInvalidation(V(1), "test1");
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(V(2), GetHighestHandledInvalidationVersion());

  // Check that an invalidation whose version matches the highest handled so far
  // is acknowledged but ignored otherwise.
  const invalidation::DirectInvalidation inv2 = FireInvalidation(V(2), "test2");
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(V(2), GetHighestHandledInvalidationVersion());

  // Check that an invalidation whose version is higher than the highest handled
  // so far is handled, causing a policy refresh.
  const invalidation::DirectInvalidation inv3 = FireInvalidation(V(3), "test3");
  EXPECT_TRUE(CheckPolicyRefreshed());
  EXPECT_TRUE(ClientInvalidationInfoMatches(inv3));
  StorePolicy(V(3));
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_EQ(V(3), GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, StoreLoadedBeforeRefresh) {
  // Generate an invalidation.
  StorePolicy();
  StartInvalidator();
  const invalidation::DirectInvalidation inv = FireInvalidation(V(3), "test");

  // Ensure that the policy is not refreshed and the invalidation is
  // data is removed from the client if the store is loaded with the latest
  // version before the refresh can occur.
  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
  StorePolicy(V(3));
  EXPECT_TRUE(ClientInvalidationInfoIsUnset());
  EXPECT_TRUE(CheckPolicyNotRefreshed());
  EXPECT_EQ(V(3), GetHighestHandledInvalidationVersion());
}

TEST_F(CloudPolicyInvalidatorTest, NoCallbackAfterShutdown) {
  // Generate an invalidation.
  StorePolicy();
  StartInvalidator();
  const invalidation::DirectInvalidation inv = FireInvalidation(V(3), "test");

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
  DisableInvalidationListener();
  EnableInvalidationListener();
  EXPECT_FALSE(InvalidationsEnabled());
  // Test invalidation service state changes while registered.
  StartInvalidationListener();
  StorePolicy();
  EXPECT_TRUE(InvalidationsEnabled());

  DisableInvalidationListener();
  EXPECT_FALSE(InvalidationsEnabled());

  DisableInvalidationListener();
  EXPECT_FALSE(InvalidationsEnabled());
  EnableInvalidationListener();
  EXPECT_TRUE(InvalidationsEnabled());
  EnableInvalidationListener();
  EXPECT_TRUE(InvalidationsEnabled());
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

TEST_F(CloudPolicyInvalidatorOwnerNameTest, GetTypeForUserScope) {
  scope_ = PolicyInvalidationScope::kUser;
  StartInvalidator(/*initialize=*/false,
                   /*start_refresh_scheduler=*/false,
                   /*highest_handled_invalidation_version=*/0);
  ASSERT_TRUE(invalidator());
  EXPECT_EQ("USER_POLICY_FETCH", invalidator()->GetType());
}

TEST_F(CloudPolicyInvalidatorOwnerNameTest, GetTypeForDeviceScope) {
  scope_ = PolicyInvalidationScope::kDevice;
  StartInvalidator(/*initialize=*/false,
                   /*start_refresh_scheduler=*/false,
                   /*highest_handled_invalidation_version=*/0);
  ASSERT_TRUE(invalidator());
  EXPECT_EQ("DEVICE_POLICY_FETCH", invalidator()->GetType());
}

TEST_F(CloudPolicyInvalidatorOwnerNameTest, GetTypeForDeviceLocalAccountScope) {
  scope_ = PolicyInvalidationScope::kDeviceLocalAccount;
  StartInvalidator(/*initialize=*/false,
                   /*start_refresh_scheduler=*/false,
                   /*highest_handled_invalidation_version=*/0);
  ASSERT_TRUE(invalidator());
  EXPECT_EQ("PUBLIC_ACCOUNT_POLICY_FETCH-test_account",
            invalidator()->GetType());
}

TEST_F(CloudPolicyInvalidatorOwnerNameTest, GetTypeForCbcmScope) {
  scope_ = PolicyInvalidationScope::kCBCM;
  StartInvalidator(/*initialize=*/false,
                   /*start_refresh_scheduler=*/false,
                   /*highest_handled_invalidation_version=*/0);
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
  CloudPolicyInvalidatorUserTypedTest() = default;

  base::HistogramBase::Count32 GetCount(MetricPolicyRefresh metric);
  base::HistogramBase::Count32 GetInvalidationCount(
      PolicyInvalidationType type);

 private:
  // CloudPolicyInvalidatorTest:
  PolicyInvalidationScope GetPolicyInvalidationScope() const override;

  base::HistogramTester histogram_tester_;
};

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

TEST_P(CloudPolicyInvalidatorUserTypedTest, RefreshMetricsUnregistered) {
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

TEST_P(CloudPolicyInvalidatorUserTypedTest, RefreshMetricsNoInvalidations) {
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
  DisableInvalidationListener();
  StorePolicy(0, /*policy_changed=*/false);
  StorePolicy(0, /*policy_changed=*/true);
  EXPECT_EQ(3, GetCount(METRIC_POLICY_REFRESH_CHANGED_NO_INVALIDATIONS));

  // Enabling the invalidation service results in a new grace period, so
  // invalidations are OFF.
  EnableInvalidationListener();
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

TEST_P(CloudPolicyInvalidatorUserTypedTest, RefreshMetricsInvalidation) {
  // Store loads after an invalidation are not counted as invalidated.
  StartInvalidator();
  StorePolicy();
  AdvanceClock(base::Seconds(CloudPolicyInvalidator::kInvalidationGracePeriod));
  FireInvalidation(V(5), "test");
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

TEST_P(CloudPolicyInvalidatorUserTypedTest, ExpiredInvalidations) {
  StorePolicy(0, false, Now());
  StartInvalidator();

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

  EXPECT_EQ(0, GetHighestHandledInvalidationVersion());
}

INSTANTIATE_TEST_SUITE_P(
    CloudPolicyInvalidatorUserTypedTestInstance,
    CloudPolicyInvalidatorUserTypedTest,
    testing::Values(PolicyInvalidationScope::kUser,
                    PolicyInvalidationScope::kDevice,
                    PolicyInvalidationScope::kDeviceLocalAccount));

}  // namespace policy

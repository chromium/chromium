// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_scheduler.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_test_helpers.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_worker.h"
#include "chrome/browser/ash/cert_provisioning/mock_cert_provisioning_client.h"
#include "chrome/browser/ash/cert_provisioning/mock_cert_provisioning_worker.h"
#include "chrome/browser/ash/platform_keys/mock_platform_keys_service.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/attestation/fake_attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using base::Time;
using base::test::ParseJson;
using testing::_;
using testing::AtLeast;
using testing::ByMove;
using testing::Exactly;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;
using testing::StrictMock;

namespace ash {
namespace cert_provisioning {
namespace {

constexpr char kWifiServiceGuid[] = "wifi_guid";
constexpr char kCertProfileId[] = "cert_profile_id_1";
constexpr char kCertProfileName[] = "Certificate Profile 1";
constexpr char kCertProfileVersion[] = "cert_profile_version_1";
constexpr char kCertProvId[] = "111";
constexpr base::TimeDelta kCertProfileRenewalPeriod = base::Seconds(0);

void VerifyDeleteKeysByPrefixCalledOnce(CertScope cert_scope) {
  const std::vector<::attestation::DeleteKeysRequest> delete_keys_history =
      AttestationClient::Get()->GetTestInterface()->delete_keys_history();
  // Use `ASSERT_EQ()` so the checks that follows don't crash.
  ASSERT_EQ(delete_keys_history.size(), 1U);
  EXPECT_EQ(delete_keys_history[0].username().empty(),
            cert_scope != CertScope::kUser);
  EXPECT_EQ(delete_keys_history[0].key_label_match(), kKeyNamePrefix);
  EXPECT_EQ(delete_keys_history[0].match_behavior(),
            ::attestation::DeleteKeysRequest::MATCH_BEHAVIOR_PREFIX);
}

void ExpectDeleteKeysByPrefixNeverCalled() {
  const std::vector<::attestation::DeleteKeysRequest> delete_keys_history =
      AttestationClient::Get()->GetTestInterface()->delete_keys_history();
  EXPECT_TRUE(delete_keys_history.empty());
}

//=============== TestCertProvisioningSchedulerObserver ========================

class TestCertProvisioningSchedulerObserver {
 public:
  TestCertProvisioningSchedulerObserver() = default;
  ~TestCertProvisioningSchedulerObserver() = default;

  TestCertProvisioningSchedulerObserver(
      const TestCertProvisioningSchedulerObserver& other) = delete;
  TestCertProvisioningSchedulerObserver& operator=(
      const TestCertProvisioningSchedulerObserver& other) = delete;

  base::RepeatingClosure GetCallback() {
    return base::BindRepeating(
        &TestCertProvisioningSchedulerObserver::OnVisibleStateChanged,
        base::Unretained(this));
  }

  void OnVisibleStateChanged() {
    ++notifications_received_;
    run_loop_->Quit();
  }

  // Waits for one call to happen (since construction or since the previous
  // WaitForOneCall has returned).
  void WaitForOneCall() {
    run_loop_->Run();
    // Create a new RunLoop so it can already be terminated when the next
    // OnVisibleStateChanged() call comes in.
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  // Returns the number of received calls since the last use of this method.
  size_t ReadAndResetCallCount() {
    size_t result = notifications_received_;
    notifications_received_ = 0;
    return result;
  }

 private:
  size_t notifications_received_ = 0;
  std::unique_ptr<base::RunLoop> run_loop_ = std::make_unique<base::RunLoop>();
};

//=================== CertProvisioningSchedulerTest ============================

class CertProvisioningSchedulerTest : public testing::Test {
 public:
  CertProvisioningSchedulerTest()
      : network_state_test_helper_(/*use_default_devices_and_services=*/false) {
    Init();
  }
  CertProvisioningSchedulerTest(const CertProvisioningSchedulerTest&) = delete;
  CertProvisioningSchedulerTest& operator=(
      const CertProvisioningSchedulerTest&) = delete;
  ~CertProvisioningSchedulerTest() override = default;

 protected:
  void Init() {
    RegisterProfilePrefs(pref_service_.registry());
    RegisterLocalStatePrefs(pref_service_.registry());

    certificate_helper_ =
        std::make_unique<CertificateHelperForTesting>(&platform_keys_service_);

    AddOnlineWifiNetwork();
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void SetUp() override {
    AttestationClient::InitializeFake();
    CertProvisioningWorkerFactory::SetFactoryForTesting(&mock_factory_);
  }

  void TearDown() override {
    CertProvisioningWorkerFactory::SetFactoryForTesting(nullptr);
    AttestationClient::Shutdown();
  }

  void AddOnlineWifiNetwork() {
    ASSERT_TRUE(wifi_network_service_path_.empty());

    std::stringstream ss;
    ss << "{"
       << "  \"GUID\": \"" << kWifiServiceGuid << "\","
       << "  \"Type\": \"" << shill::kTypeWifi << "\","
       << "  \"State\": \"" << shill::kStateOnline << "\""
       << "}";

    wifi_network_service_path_ =
        network_state_test_helper_.ConfigureService(ss.str());
  }

  // |network_state| is a shill network state, e.g. "shill::kStateIdle".
  void SetWifiNetworkState(std::string network_state) {
    network_state_test_helper_.SetServiceProperty(wifi_network_service_path_,
                                                  shill::kStateProperty,
                                                  base::Value(network_state));
    base::RunLoop().RunUntilIdle();
  }

  Profile* GetProfile() { return profile_helper_for_testing_.GetProfile(); }

  std::unique_ptr<MockCertProvisioningInvalidatorFactory>
  MakeFakeInvalidationFactory() {
    auto result = std::make_unique<MockCertProvisioningInvalidatorFactory>();
    // Return nullptr from every Create. The result will be passed to mock
    // worker, so it does not need to be non-null.
    result->ExpectCreateReturnNull();
    return result;
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ProfileHelperForTesting profile_helper_for_testing_;
  platform_keys::MockPlatformKeysService platform_keys_service_;
  std::unique_ptr<CertificateHelperForTesting> certificate_helper_;
  TestingPrefServiceSimple pref_service_;
  // Only expected creations are allowed.
  StrictMock<MockCertProvisioningWorkerFactory> mock_factory_;
  NetworkStateTestHelper network_state_test_helper_;
  std::string wifi_network_service_path_;
};

TEST_F(CertProvisioningSchedulerTest, Success) {
  const CertScope kCertScope = CertScope::kUser;

  auto mock_invalidation_factory_obj =
      std::make_unique<MockCertProvisioningInvalidatorFactory>();
  MockCertProvisioningInvalidatorFactory* mock_invalidation_factory =
      mock_invalidation_factory_obj.get();

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_,
      std::make_unique<MockCertProvisioningClient>(), &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      std::move(mock_invalidation_factory_obj));

  // The policy is empty, so no workers should be created yet.
  FastForwardBy(base::Seconds(1));
  EXPECT_EQ(scheduler.GetWorkers().size(), 0U);

  EXPECT_CALL(*mock_invalidation_factory, Create)
      .Times(1)
      .WillOnce(
          Return(ByMove(nullptr)));  // nullptr is good enough for mock worker.

  // From CertProvisioningSchedulerImpl::CleanVaKeysIfIdle.
  VerifyDeleteKeysByPrefixCalledOnce(kCertScope);

  // One worker will be created on prefs update.
  CertProfile cert_profile(kCertProfileId, kCertProfileName,
                           kCertProfileVersion, KeyType::kRsa,
                           /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
                           ProtocolVersion::kStatic);
  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1),
                          /*is_waiting=*/false, cert_profile,
                          /*failure_message=*/"");

  // Add 1 certificate profile to the policy (the values are the same as
  // in |cert_profile|).
  base::Value config = ParseJson(
      R"([{"name": "Certificate Profile 1",
           "cert_profile_id":"cert_profile_id_1",
           "policy_version":"cert_profile_version_1",
           "key_algorithm":"rsa"}])");
  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), config);

  EXPECT_EQ(scheduler.GetWorkers().size(), 1U);

  // Emulate callback from the worker.
  scheduler.OnProfileFinished(cert_profile, kCertProvId,
                              CertProvisioningWorkerState::kSucceeded);

  // Finished worker should be deleted.
  EXPECT_EQ(scheduler.GetWorkers().size(), 0U);
  EXPECT_TRUE(scheduler.GetFailedCertProfileIds().empty());

  certificate_helper_->AddCert(kCertScope, kCertProfileId);

  // Check one more time that scheduler doesn't create new workers for
  // finished certificate profiles (the factory will fail on an attempt to
  // do so).
  scheduler.UpdateAllWorkers();

  FastForwardBy(base::Seconds(100));
}

TEST_F(CertProvisioningSchedulerTest, WorkerFailed) {
  const CertScope kCertScope = CertScope::kDevice;

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_,
      std::make_unique<MockCertProvisioningClient>(), &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  // The policy is empty, so no workers should be created yet.
  FastForwardBy(base::Seconds(1));
  EXPECT_EQ(scheduler.GetWorkers().size(), 0U);

  // From CertProvisioningScheduler::CleanVaKeysIfIdle.
  VerifyDeleteKeysByPrefixCalledOnce(kCertScope);

  // One worker will be created on prefs update.
  CertProfile cert_profile(kCertProfileId, kCertProfileName,
                           kCertProfileVersion, KeyType::kRsa,
                           /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
                           ProtocolVersion::kStatic);
  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1),
                          /*is_waiting=*/false, cert_profile,
                          /*failure_message=*/"reason for failure");

  // Add 1 certificate profile to the policy (the values are the same as
  // in |cert_profile|).
  base::Value config = ParseJson(
      R"([{"name": "Certificate Profile 1",
           "cert_profile_id":"cert_profile_id_1",
           "policy_version":"cert_profile_version_1",
           "key_algorithm":"rsa"}])");
  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), config);

  // Now 1 worker should be created.
  EXPECT_EQ(scheduler.GetWorkers().size(), 1U);

  // Emulate callback from the worker.
  scheduler.OnProfileFinished(cert_profile, kCertProvId,
                              CertProvisioningWorkerState::kFailed);

  // The failure message in the FailedWorkerInfo object should match the
  // failure message passed to the mock worker
  EXPECT_EQ(scheduler.GetFailedCertProfileIds().size(), 1U);
  EXPECT_EQ(
      scheduler.GetFailedCertProfileIds().at(kCertProfileId).failure_message,
      "reason for failure");

  // Failed worker should be deleted, failed profile ID is saved, no new
  // workers should be created.
  EXPECT_EQ(scheduler.GetWorkers().size(), 0U);
  EXPECT_TRUE(
      base::Contains(scheduler.GetFailedCertProfileIds(), kCertProfileId));

  certificate_helper_->AddCert(kCertScope, kCertProfileId);

  // Check one more time that scheduler doesn't create new workers for failed
  // certificate profiles (the factory will fail on an attempt to do so).
  scheduler.UpdateAllWorkers();
}

TEST_F(CertProvisioningSchedulerTest, InitialAndDailyUpdates) {
  const CertScope kCertScope = CertScope::kUser;

  CertProfile cert_profile(kCertProfileId, kCertProfileName,
                           kCertProfileVersion, KeyType::kRsa,
                           /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
                           ProtocolVersion::kStatic);

  // Add 1 certificate profile to the policy (the values are the same as
  // in |cert_profile|).
  base::Value config = ParseJson(
      R"([{"name": "Certificate Profile 1",
           "cert_profile_id":"cert_profile_id_1",
           "policy_version":"cert_profile_version_1",
           "key_algorithm":"rsa"}])");
  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), config);

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_,
      std::make_unique<MockCertProvisioningClient>(), &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  // Now one worker should be created.
  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                          cert_profile, /*failure_message=*/"");
  FastForwardBy(base::Seconds(1));
  ASSERT_EQ(scheduler.GetWorkers().size(), 1U);

  // Emulate callback from the worker.
  scheduler.OnProfileFinished(cert_profile, kCertProvId,
                              CertProvisioningWorkerState::kFailed);

  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);
  EXPECT_TRUE(
      base::Contains(scheduler.GetFailedCertProfileIds(), kCertProfileId));

  // No workers should be created yet.
  FastForwardBy(base::Hours(20));
  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);

  // From CertProvisioningSchedulerImpl::CleanVaKeysIfIdle.
  VerifyDeleteKeysByPrefixCalledOnce(kCertScope);

  // Now list of failed profiles should be cleared that will cause a new attempt
  // to provision certificate.
  MockCertProvisioningWorker* worker2 =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
  worker2->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                           cert_profile, /*failure_message=*/"");
  FastForwardBy(base::Hours(5));
  ASSERT_EQ(scheduler.GetWorkers().size(), 1U);

  // Emulate callback from the worker.
  scheduler.OnProfileFinished(cert_profile, kCertProvId,
                              CertProvisioningWorkerState::kSucceeded);

  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);
  EXPECT_TRUE(scheduler.GetFailedCertProfileIds().empty());
}

TEST_F(CertProvisioningSchedulerTest, MultipleWorkers) {
  const CertScope kCertScope = CertScope::kDevice;

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_,
      std::make_unique<MockCertProvisioningClient>(), &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  // The policy is empty, so no workers should be created yet.
  FastForwardBy(base::Seconds(1));
  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);

  // From CertProvisioningScheduler::CleanVaKeysIfIdle.
  VerifyDeleteKeysByPrefixCalledOnce(kCertScope);

  // New workers will be created on prefs update.
  const char kCertProfileId0[] = "cert_profile_id_0";
  const char kCertProfileName0[] = "Certificate Profile 0";
  const char kCertProfileVersion0[] = "cert_profile_version_0";
  CertProfile cert_profile0(kCertProfileId0, kCertProfileName0,
                            kCertProfileVersion0, KeyType::kRsa,
                            /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
                            ProtocolVersion::kStatic);
  const char kCertProfileId1[] = "cert_profile_id_1";
  const char kCertProfileName1[] = "Certificate Profile 1";
  const char kCertProfileVersion1[] = "cert_profile_version_1";
  CertProfile cert_profile1(kCertProfileId1, kCertProfileName1,
                            kCertProfileVersion1, KeyType::kRsa,
                            /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
                            ProtocolVersion::kStatic);
  const char kCertProfileId2[] = "cert_profile_id_2";
  const char kCertProfileName2[] = "Certificate Profile 2";
  const char kCertProfileVersion2[] = "cert_profile_version_2";
  CertProfile cert_profile2(kCertProfileId2, kCertProfileName2,
                            kCertProfileVersion2, KeyType::kRsa,
                            /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
                            ProtocolVersion::kStatic);
  MockCertProvisioningWorker* worker0 =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile0);
  worker0->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                           cert_profile0, /*failure_message=*/"");
  MockCertProvisioningWorker* worker1 =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile1);
  worker1->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                           cert_profile1, /*failure_message=*/"");
  MockCertProvisioningWorker* worker2 =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile2);
  worker2->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                           cert_profile2, /*failure_message=*/"");

  // Add 3 certificate profiles to the policy (the values are the same as
  // in |cert_profile|-s)
  base::Value config = ParseJson(
      R"([{
           "name": "Certificate Profile 0",
           "cert_profile_id":"cert_profile_id_0",
           "policy_version":"cert_profile_version_0",
           "key_algorithm":"rsa"
          },
          {
           "name": "Certificate Profile 1",
           "cert_profile_id":"cert_profile_id_1",
           "policy_version":"cert_profile_version_1",
           "key_algorithm":"rsa"
          },
          {
           "name": "Certificate Profile 2",
           "cert_profile_id":"cert_profile_id_2",
           "policy_version":"cert_profile_version_2",
           "key_algorithm":"rsa"
          }])");
  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), config);

  // Now one worker for each profile should be created.
  ASSERT_EQ(scheduler.GetWorkers().size(), 3U);

  // worker0 successfully finished. Should be just deleted.
  scheduler.OnProfileFinished(cert_profile0, kCertProvId,
                              CertProvisioningWorkerState::kSucceeded);

  // worker1 is waiting. Should be continued.
  worker1->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/true,
                           cert_profile1, /*failure_message=*/"");

  // worker2 failed. Should be deleted and the profile id should be saved.
  scheduler.OnProfileFinished(cert_profile2, kCertProvId,
                              CertProvisioningWorkerState::kFailed);

  EXPECT_EQ(scheduler.GetWorkers().size(), 1U);
  EXPECT_TRUE(
      base::Contains(scheduler.GetFailedCertProfileIds(), kCertProfileId2));

  certificate_helper_->AddCert(kCertScope, kCertProfileId0);

  // Make scheduler check workers state.
  scheduler.UpdateAllWorkers();

  EXPECT_EQ(scheduler.GetWorkers().size(), 1U);
  EXPECT_TRUE(
      base::Contains(scheduler.GetFailedCertProfileIds(), kCertProfileId2));

  // Check one more time that scheduler doesn't create new workers for failed
  // certificate profiles (the factory will fail on an attempt to do so).
  scheduler.UpdateAllWorkers();
  EXPECT_EQ(scheduler.GetWorkers().size(), 1U);
}

TEST_F(CertProvisioningSchedulerTest, RemoveCertWithoutPolicy) {
  const CertScope kCertScope = CertScope::kDevice;

  certificate_helper_->AddCert(kCertScope, kCertProfileId);

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_,
      std::make_unique<MockCertProvisioningClient>(), &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  EXPECT_CALL(
      platform_keys_service_,
      RemoveCertificate(GetPlatformKeysTokenId(kCertScope),
                        /*certificate=*/certificate_helper_->GetCerts().back(),
                        /*callback=*/_))
      .Times(1);

  FastForwardBy(base::Seconds(1));
}

TEST_F(CertProvisioningSchedulerTest, DeserializeWorkers) {
  const CertScope kCertScope = CertScope::kUser;

  CertProfile cert_profile(kCertProfileId, kCertProfileName,
                           kCertProfileVersion, KeyType::kRsa,
                           /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
                           ProtocolVersion::kStatic);

  // Add 1 certificate profile to the policy (the values are the same as
  // in |cert_profile|).
  base::Value cert_profiles = ParseJson(
      R"([{"name": "Certificate Profile 1",
           "cert_profile_id":"cert_profile_id_1",
           "policy_version": "cert_profile_version_1",
           "key_algorithm":"rsa"}])");
  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), cert_profiles);
  // Add 1 serialized worker for the profile.
  base::Value saved_worker = ParseJson(
      R"({
          "cert_profile": {
            "policy_version": "cert_profile_version_1",
            "profile_id": "cert_profile_1"
          },
          "cert_scope": 0,
          "invalidation_topic": "",
          "public_key": "fake_public_key_1",
          "state": 1
        })");
  base::Value::Dict all_saved_workers;
  all_saved_workers.Set("cert_profile_1", saved_worker.Clone());

  pref_service_.SetDict(GetPrefNameForSerialization(kCertScope),
                        std::move(all_saved_workers));

  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectDeserializeReturnMock(kCertScope, saved_worker);
  // is_waiting==true should be set by Serializer so Scheduler knows that the
  // worker has to be continued manually.
  worker->SetExpectations(/*do_step_times=*/AtLeast(1),
                          /*is_waiting=*/true, cert_profile,
                          /*failure_message=*/"");

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_,
      std::make_unique<MockCertProvisioningClient>(), &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  // Now one worker should be created.
  FastForwardBy(base::Seconds(1));
  ASSERT_EQ(scheduler.GetWorkers().size(), 1U);
}

TEST_F(CertProvisioningSchedulerTest, InconsistentDataErrorHandling) {
  const CertScope kCertScope = CertScope::kDevice;

  const char kCertProfileVersion1[] = "cert_profile_version_1";
  const char kCertProfileVersion2[] = "cert_profile_version_2";

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_,
      std::make_unique<MockCertProvisioningClient>(), &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  // The policy is empty, so no workers should be created yet.
  FastForwardBy(base::Seconds(1));
  EXPECT_EQ(scheduler.GetWorkers().size(), 0U);

  // From CertProvisioningScheduler::CleanVaKeysIfIdle.
  VerifyDeleteKeysByPrefixCalledOnce(kCertScope);

  CertProfile cert_profile_v1(kCertProfileId, kCertProfileName,
                              kCertProfileVersion1, KeyType::kRsa,
                              /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
                              ProtocolVersion::kStatic);

  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile_v1);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                          cert_profile_v1, /*failure_message=*/"");

  // Add 1 certificate profile to the policy (the values are the same as
  // in |cert_profile_v1|).
  base::Value config = ParseJson(
      R"([{"name": "Certificate Profile 1",
           "cert_profile_id":"cert_profile_id_1",
           "policy_version":"cert_profile_version_1",
           "key_algorithm":"rsa"}])");
  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), config);

  // Now 1 worker should be created.
  EXPECT_EQ(scheduler.GetWorkers().size(), 1U);

  // Emulate callback from the worker.
  scheduler.OnProfileFinished(
      cert_profile_v1, kCertProvId,
      CertProvisioningWorkerState::kInconsistentDataError);

  // Failed worker should be deleted, failed profile ID should not be saved, no
  // new workers should be created.
  EXPECT_EQ(scheduler.GetWorkers().size(), 0U);
  EXPECT_TRUE(scheduler.GetFailedCertProfileIds().empty());

  // Add a new worker to the factory.
  worker = mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile_v1);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                          cert_profile_v1, /*failure_message=*/"");

  // After some delay a new worker should be created to try again.
  FastForwardBy(base::Seconds(31));
  EXPECT_EQ(scheduler.GetWorkers().size(), 1U);

  // Emulate callback from the worker.
  scheduler.OnProfileFinished(
      cert_profile_v1, kCertProvId,
      CertProvisioningWorkerState::kInconsistentDataError);

  // Failed worker should be deleted, failed profile ID should not be saved, no
  // new workers should be created.
  EXPECT_EQ(scheduler.GetWorkers().size(), 0U);
  EXPECT_TRUE(scheduler.GetFailedCertProfileIds().empty());

  // Add a new worker to the factory.
  CertProfile cert_profile_v2(kCertProfileId, kCertProfileName,
                              kCertProfileVersion2, KeyType::kRsa,
                              /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
                              ProtocolVersion::kStatic);
  worker = mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile_v2);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                          cert_profile_v2, /*failure_message=*/"");

  // On policy update a new worker should be created to try again.
  config = ParseJson(
      R"([{"name": "Certificate Profile 1",
           "cert_profile_id":"cert_profile_id_1",
           "policy_version":"cert_profile_version_2",
           "key_algorithm":"rsa"}])");
  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), config);
  EXPECT_EQ(scheduler.GetWorkers().size(), 1U);

  // If another update happens, workers with matching policy versions should not
  // be deleted.
  scheduler.UpdateAllWorkers();
  EXPECT_EQ(scheduler.GetWorkers().size(), 1U);

  // On policy update if existing profile has changed its policy_version,
  // scheduler should recreate the worker for it.
  config = ParseJson(
      R"([{"name": "Certificate Profile 1",
           "cert_profile_id":"cert_profile_id_1",
           "policy_version":"cert_profile_version_3",
           "key_algorithm":"rsa"}])");

  // On policy change scheduler should detect mismatch in policy versions and
  // stop the worker.
  EXPECT_CALL(*worker,
              Stop(CertProvisioningWorkerState::kInconsistentDataError));

  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), config);

  // Emulate that after some time the worker reports back to scheduler.
  FastForwardBy(base::Seconds(10));
  scheduler.OnProfileFinished(
      cert_profile_v1, kCertProvId,
      CertProvisioningWorkerState::kInconsistentDataError);
  EXPECT_EQ(scheduler.GetWorkers().size(), 0U);
}

TEST_F(CertProvisioningSchedulerTest, RetryAfterNoInternetConnection) {
  const CertScope kCertScope = CertScope::kDevice;
  SetWifiNetworkState(shill::kStateIdle);

  CertProfile cert_profile(kCertProfileId, kCertProfileName,
                           kCertProfileVersion, KeyType::kRsa,
                           /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
                           ProtocolVersion::kStatic);
  // Add 1 certificate profile to the policy (the values are the same as
  // in |cert_profile|).
  base::Value config = ParseJson(
      R"([{"name": "Certificate Profile 1",
           "cert_profile_id":"cert_profile_id_1",
           "policy_version":"cert_profile_version_1",
           "key_algorithm":"rsa"}])");
  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), config);

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_,
      std::make_unique<MockCertProvisioningClient>(), &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  FastForwardBy(base::Hours(72));
  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);

  // From CertProvisioningScheduler::CleanVaKeysIfIdle.
  VerifyDeleteKeysByPrefixCalledOnce(kCertScope);

  // Add a new worker to the factory.
  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                          cert_profile, /*failure_message=*/"");

  SetWifiNetworkState(shill::kStateOnline);

  ASSERT_EQ(scheduler.GetWorkers().size(), 1U);
}

TEST_F(CertProvisioningSchedulerTest, DeleteWorkerWithoutPolicy) {
  const CertScope kCertScope = CertScope::kDevice;

  CertProfile cert_profile(kCertProfileId, kCertProfileName,
                           kCertProfileVersion, KeyType::kRsa,
                           /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
                           ProtocolVersion::kStatic);
  // Add 1 certificate profile to the policy (the values are the same as
  // in |cert_profile|).
  base::Value config = ParseJson(
      R"([{"name": "Certificate Profile 1",
           "cert_profile_id":"cert_profile_id_1",
           "policy_version":"cert_profile_version_1",
           "key_algorithm":"rsa"}])");

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_,
      std::make_unique<MockCertProvisioningClient>(), &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  // Add a new worker to the factory.
  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                          cert_profile, /*failure_message=*/"");

  // Prefs update will be ignored because initialization task has not finished
  // yet.
  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), config);
  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);

  FastForwardBy(base::Seconds(1));
  EXPECT_EQ(scheduler.GetWorkers().size(), 1U);

  EXPECT_CALL(*worker, Stop(CertProvisioningWorkerState::kCanceled));

  config = ParseJson("[]");
  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), config);

  FastForwardBy(base::Seconds(1));
  // Emulate callback from the worker.
  scheduler.OnProfileFinished(cert_profile, kCertProvId,
                              CertProvisioningWorkerState::kCanceled);

  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);

  // From CertProvisioningScheduler::CleanVaKeysIfIdle.
  VerifyDeleteKeysByPrefixCalledOnce(kCertScope);
}

TEST_F(CertProvisioningSchedulerTest, DeleteVaKeysOnIdle) {
  const CertScope kCertScope = CertScope::kDevice;

  {
    CertProvisioningSchedulerImpl scheduler(
        kCertScope, GetProfile(), &pref_service_,
        std::make_unique<MockCertProvisioningClient>(), &platform_keys_service_,
        network_state_test_helper_.network_state_handler(),
        MakeFakeInvalidationFactory());

    FastForwardBy(base::Seconds(1));

    // From CertProvisioningScheduler::CleanVaKeysIfIdle.
    VerifyDeleteKeysByPrefixCalledOnce(kCertScope);
  }

  AttestationClient::Get()->GetTestInterface()->ClearDeleteKeysHistory();

  {
    CertProfile cert_profile(kCertProfileId, kCertProfileName,
                             kCertProfileVersion, KeyType::kRsa,
                             /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
                             ProtocolVersion::kStatic);

    // Add 1 serialized worker for the profile (the values are the same as
    // in |cert_profile|).
    base::Value saved_worker = ParseJson(
        R"({
          "cert_profile": {
            "policy_version": "cert_profile_version_1",
            "profile_id": "cert_profile_1"
          },
          "cert_scope": 0,
          "invalidation_topic": "",
          "public_key": "fake_public_key_1",
          "state": 1
        })");
    base::Value::Dict all_saved_workers;
    all_saved_workers.Set("cert_profile_1", saved_worker.Clone());

    pref_service_.SetDict(GetPrefNameForSerialization(kCertScope),
                          std::move(all_saved_workers));

    MockCertProvisioningWorker* worker =
        mock_factory_.ExpectDeserializeReturnMock(kCertScope, saved_worker);
    // This worker should be deleted approximately right after creation, hence
    // no calls for DoStep.
    worker->SetExpectations(/*do_step_times=*/Exactly(0),
                            /*is_waiting=*/true, cert_profile,
                            /*failure_message=*/"");

    CertProvisioningSchedulerImpl scheduler(
        kCertScope, GetProfile(), &pref_service_,
        std::make_unique<MockCertProvisioningClient>(), &platform_keys_service_,
        network_state_test_helper_.network_state_handler(),
        MakeFakeInvalidationFactory());

    FastForwardBy(base::Seconds(1));

    ExpectDeleteKeysByPrefixNeverCalled();
  }
}

TEST_F(CertProvisioningSchedulerTest, UpdateOneWorker) {
  const CertScope kCertScope = CertScope::kUser;

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_,
      std::make_unique<MockCertProvisioningClient>(), &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  CertProfile cert_profile(kCertProfileId, kCertProfileName,
                           kCertProfileVersion, KeyType::kRsa,
                           /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
                           ProtocolVersion::kStatic);

  FastForwardBy(base::Seconds(1));

  // From CertProvisioningScheduler::CleanVaKeysIfIdle.
  VerifyDeleteKeysByPrefixCalledOnce(kCertScope);

  // There is no policies yet, |kCertProfileId| will not be found.
  scheduler.UpdateOneWorker(kCertProfileId);
  FastForwardBy(base::Seconds(1));
  ASSERT_TRUE(scheduler.GetWorkers().empty());

  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
  worker->SetExpectations(/*do_step_times=*/Exactly(1),
                          /*is_waiting=*/false, cert_profile,
                          /*failure_message=*/"");

  // Add 1 certificate profile to the policy (the values are the same as
  // in |cert_profile|).
  base::Value config = ParseJson(
      R"([{"name": "Certificate Profile 1",
           "cert_profile_id":"cert_profile_id_1",
           "policy_version":"cert_profile_version_1",
           "key_algorithm":"rsa"}])");
  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), config);
  FastForwardBy(base::Seconds(1));

  // If worker is waiting, it should be continued.
  {
    worker->SetExpectations(/*do_step_times=*/Exactly(1),
                            /*is_waiting=*/true, cert_profile,
                            /*failure_message=*/"");

    scheduler.UpdateOneWorker(kCertProfileId);
    FastForwardBy(base::Seconds(1));
    ASSERT_EQ(scheduler.GetWorkers().size(), 1U);
  }

  // If worker is not waiting, it should not be continued.
  {
    worker->SetExpectations(/*do_step_times=*/Exactly(0),
                            /*is_waiting=*/false, cert_profile,
                            /*failure_message=*/"");

    scheduler.UpdateOneWorker(kCertProfileId);
    FastForwardBy(base::Seconds(1));
    ASSERT_EQ(scheduler.GetWorkers().size(), 1U);
  }

  // If there is no intenet connection, the worker should not be continued
  // until it is restored.
  {
    SetWifiNetworkState(shill::kStateIdle);

    worker->SetExpectations(/*do_step_times=*/Exactly(0),
                            /*is_waiting=*/true, cert_profile,
                            /*failure_message=*/"");

    scheduler.UpdateOneWorker(kCertProfileId);
    FastForwardBy(base::Seconds(1));
    ASSERT_EQ(scheduler.GetWorkers().size(), 1U);

    worker->SetExpectations(/*do_step_times=*/Exactly(1),
                            /*is_waiting=*/true, cert_profile,
                            /*failure_message=*/"");

    SetWifiNetworkState(shill::kStateOnline);
  }

  // Emulate callback from the worker.
  scheduler.OnProfileFinished(cert_profile, kCertProvId,
                              CertProvisioningWorkerState::kSucceeded);
  FastForwardBy(base::Seconds(1));
  ASSERT_TRUE(scheduler.GetWorkers().empty());

  certificate_helper_->AddCert(kCertScope, kCertProfileId);

  {
    // If a certificate already exists, a new worker should not be created.
    scheduler.UpdateOneWorker(kCertProfileId);
    FastForwardBy(base::Seconds(1));
    ASSERT_TRUE(scheduler.GetWorkers().empty());
  }
}

TEST_F(CertProvisioningSchedulerTest, CertRenewal) {
  const CertScope kCertScope = CertScope::kUser;
  // 1 day == 86400 seconds.
  const base::TimeDelta kRenewalPeriod = base::Days(1);

  CertProfile cert_profile(
      kCertProfileId, kCertProfileName, kCertProfileVersion, KeyType::kRsa,
      /*is_va_enabled=*/true, kRenewalPeriod, ProtocolVersion::kStatic);

  const Time t1 = Time::Now() - base::Days(1);
  const Time t2 = Time::Now() + base::Days(7);
  certificate_helper_->AddCert(kCertScope, kCertProfileId,
                               chromeos::platform_keys::Status::kSuccess,
                               /*not_valid_before=*/t1, /*not_valid_after=*/t2);

  // Add 1 certificate profile to the policy (the values are the same as
  // in |cert_profile|).
  base::Value config = ParseJson(
      R"([{"name": "Certificate Profile 1",
           "cert_profile_id":"cert_profile_id_1",
           "policy_version":"cert_profile_version_1",
           "key_algorithm":"rsa",
           "renewal_period_seconds": 86400}])");
  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), config);

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_,
      std::make_unique<MockCertProvisioningClient>(), &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  // The certificate already exists, nothing should happen on scheduler
  // creation.
  FastForwardBy(base::Seconds(1));
  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);

  // From CertProvisioningScheduler::CleanVaKeysIfIdle.
  VerifyDeleteKeysByPrefixCalledOnce(kCertScope);

  // Also nothing should happen in the next ~6 days.
  FastForwardBy(base::Days(5) + base::Hours(23));
  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);

  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1),
                          /*is_waiting=*/false, cert_profile,
                          /*failure_message=*/"");

  // One day (according to the policy) before the certificate expires, scheduler
  // should create a new worker to provision a replacement.
  FastForwardBy(base::Hours(1));
  ASSERT_EQ(scheduler.GetWorkers().size(), 1U);
}

TEST_F(CertProvisioningSchedulerTest, PlatformKeysServiceShutDown) {
  CertScope kCertScope = CertScope::kDevice;

  platform_keys::PlatformKeysServiceObserver* observer = nullptr;
  EXPECT_CALL(platform_keys_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&observer));
  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_,
      std::make_unique<MockCertProvisioningClient>(), &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  ASSERT_TRUE(observer);

  // Add 1 certificate profile to the policy.
  base::Value config = ParseJson(
      R"([{"name": "Certificate Profile 1",
           "cert_profile_id":"cert_profile_id_1",
           "policy_version":"cert_profile_version_1",
           "key_algorithm":"rsa" }])");
  pref_service_.Set(prefs::kRequiredClientCertificateForDevice, config);

  // Same as in the policy.
  CertProfile cert_profile{kCertProfileId,          kCertProfileName,
                           kCertProfileVersion,     KeyType::kRsa,
                           /*is_va_enabled=*/true,  kCertProfileRenewalPeriod,
                           ProtocolVersion::kStatic};

  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                          cert_profile, /*failure_message=*/"");
  scheduler.UpdateAllWorkers();

  // Now 1 worker should be created.
  EXPECT_EQ(scheduler.GetWorkers().size(), 1U);

  // PlatformKeysService notifies that it is shutting down.
  EXPECT_CALL(platform_keys_service_, RemoveObserver(observer));
  observer->OnPlatformKeysServiceShutDown();

  // The worker should be deleted.
  EXPECT_EQ(scheduler.GetWorkers().size(), 0U);

  // Check one more time that scheduler doesn't create new workers after
  // PlatformKeysService has been shut down (the factory will fail on an attempt
  // to do so).
  scheduler.UpdateAllWorkers();
}

TEST_F(CertProvisioningSchedulerTest, StateChangeNotifications) {
  const CertScope kCertScope = CertScope::kDevice;

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_,
      std::make_unique<MockCertProvisioningClient>(), &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  TestCertProvisioningSchedulerObserver observer;
  auto subscription = scheduler.AddObserver(observer.GetCallback());

  // The policy is empty, so no workers should be created yet.
  FastForwardBy(base::Seconds(1));
  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);

  // From CertProvisioningScheduler::CleanVaKeysIfIdle.
  VerifyDeleteKeysByPrefixCalledOnce(kCertScope);

  // Two new workers will be created on prefs update.
  // Expect a state change notification for this.
  const char kCertProfileId0[] = "cert_profile_id_0";
  const char kCertProfileName0[] = "Certificate Profile 0";
  const char kCertProfileVersion0[] = "cert_profile_version_0";
  CertProfile cert_profile0(kCertProfileId0, kCertProfileName0,
                            kCertProfileVersion0, KeyType::kRsa,
                            /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
                            ProtocolVersion::kStatic);
  const char kCertProfileId1[] = "cert_profile_id_1";
  const char kCertProfileName1[] = "Certificate Profile 1";
  const char kCertProfileVersion1[] = "cert_profile_version_1";
  CertProfile cert_profile1(kCertProfileId1, kCertProfileName1,
                            kCertProfileVersion1, KeyType::kRsa,
                            /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
                            ProtocolVersion::kStatic);

  MockCertProvisioningWorker* worker0 =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile0);
  worker0->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                           cert_profile0, /*failure_message=*/"");
  MockCertProvisioningWorker* worker1 =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile1);
  worker1->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                           cert_profile1, /*failure_message=*/"");

  // Add 2 certificate profiles to the policy (the values are the same as
  // in |cert_profile|-s)
  base::Value config = ParseJson(
      R"([{
           "name": "Certificate Profile 0",
           "cert_profile_id":"cert_profile_id_0",
           "policy_version":"cert_profile_version_0",
           "key_algorithm":"rsa"
          },
          {
           "name": "Certificate Profile 1",
           "cert_profile_id":"cert_profile_id_1",
           "policy_version":"cert_profile_version_1",
           "key_algorithm":"rsa"
          }])");
  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), config);
  observer.WaitForOneCall();

  // Now one worker for each profile should be created.
  ASSERT_EQ(scheduler.GetWorkers().size(), 2U);

  // Emulate a worker reporting a state change.
  // A state change event should be fired by the scheduler for that.
  scheduler.OnVisibleStateChanged();
  observer.WaitForOneCall();

  // Emulate a worker reporting a state changeand successfully finishing.
  // Should be just deleted, and state change event should be
  // fired for that.
  scheduler.OnVisibleStateChanged();
  scheduler.OnProfileFinished(cert_profile0, kCertProvId,
                              CertProvisioningWorkerState::kSucceeded);
  observer.WaitForOneCall();

  // worker1 failed. Should be deleted and the profile id should be saved, and a
  // state change event should be fired for that.
  scheduler.OnProfileFinished(cert_profile1, kCertProvId,
                              CertProvisioningWorkerState::kFailed);
  observer.WaitForOneCall();

  EXPECT_EQ(scheduler.GetWorkers().size(), 0U);
  EXPECT_TRUE(
      base::Contains(scheduler.GetFailedCertProfileIds(), kCertProfileId1));
}

TEST_F(CertProvisioningSchedulerTest, HoldBackNotifications) {
  CertProvisioningSchedulerImpl scheduler(
      CertScope::kDevice, GetProfile(), &pref_service_,
      std::make_unique<MockCertProvisioningClient>(), &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  TestCertProvisioningSchedulerObserver observer;
  auto subscription = scheduler.AddObserver(observer.GetCallback());

  // Ensure initial 0.
  EXPECT_EQ(0u, observer.ReadAndResetCallCount());

  // A single event produces a single notification.
  {
    scheduler.OnVisibleStateChanged();
    // Here and below this is needed to let an async notification task to be
    // executed.
    FastForwardBy(base::Seconds(0));
    EXPECT_EQ(1u, observer.ReadAndResetCallCount());
    FastForwardBy(base::Seconds(0));
    EXPECT_EQ(0u, observer.ReadAndResetCallCount());
    FastForwardBy(base::Days(1));
    EXPECT_EQ(0u, observer.ReadAndResetCallCount());
  }

  // Multiple synchronous events produce a single notification.
  {
    scheduler.OnVisibleStateChanged();
    scheduler.OnVisibleStateChanged();
    scheduler.OnVisibleStateChanged();
    // Here and below this is needed to let an async notification task to be
    // executed.
    FastForwardBy(base::Seconds(0));
    EXPECT_EQ(1u, observer.ReadAndResetCallCount());
    FastForwardBy(base::Seconds(0));
    EXPECT_EQ(0u, observer.ReadAndResetCallCount());
    FastForwardBy(base::Days(1));
    EXPECT_EQ(0u, observer.ReadAndResetCallCount());
  }

  // Multiple asynchronous events within a short period of time produce a single
  // notification immediately and one more after the internal timer is released.
  {
    scheduler.OnVisibleStateChanged();
    FastForwardBy(base::Seconds(0));
    scheduler.OnVisibleStateChanged();
    FastForwardBy(base::Seconds(0));
    scheduler.OnVisibleStateChanged();
    // Here and below this is needed to let an async notification task to be
    // executed.
    FastForwardBy(base::Seconds(0));
    EXPECT_EQ(1u, observer.ReadAndResetCallCount());
    FastForwardBy(base::Seconds(0));
    EXPECT_EQ(0u, observer.ReadAndResetCallCount());
    FastForwardBy(base::Milliseconds(350));
    EXPECT_EQ(1u, observer.ReadAndResetCallCount());
    FastForwardBy(base::Days(1));
    EXPECT_EQ(0u, observer.ReadAndResetCallCount());
  }

  // N asynchronous events far enough apart produce N notifications.
  {
    scheduler.OnVisibleStateChanged();
    FastForwardBy(base::Milliseconds(350));
    scheduler.OnVisibleStateChanged();
    FastForwardBy(base::Milliseconds(350));
    scheduler.OnVisibleStateChanged();
    // Here and below this is needed to let an async notification task to be
    // executed.
    FastForwardBy(base::Seconds(0));
    EXPECT_EQ(3u, observer.ReadAndResetCallCount());
    FastForwardBy(base::Seconds(0));
    EXPECT_EQ(0u, observer.ReadAndResetCallCount());
    FastForwardBy(base::Days(1));
    EXPECT_EQ(0u, observer.ReadAndResetCallCount());
  }
}

TEST_F(CertProvisioningSchedulerTest, ResetOneWorker) {
  const CertScope kCertScope = CertScope::kUser;

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_,
      std::make_unique<MockCertProvisioningClient>(), &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  CertProfile cert_profile(kCertProfileId, kCertProfileName,
                           kCertProfileVersion, KeyType::kRsa,
                           /*is_va_enabled=*/true, kCertProfileRenewalPeriod,
                           ProtocolVersion::kStatic);

  FastForwardBy(base::Seconds(1));

  // From CertProvisioningScheduler::CleanVaKeysIfIdle.
  VerifyDeleteKeysByPrefixCalledOnce(kCertScope);

  // There is no policies yet, |kCertProfileId| will not be found.
  scheduler.UpdateOneWorker(kCertProfileId);
  FastForwardBy(base::Seconds(1));
  ASSERT_TRUE(scheduler.GetWorkers().empty());

  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
  worker->SetExpectations(/*do_step_times=*/Exactly(1),
                          /*is_waiting=*/false, cert_profile,
                          /*failure_message=*/"");

  // Add 1 certificate profile to the policy (the values are the same as
  // in |cert_profile|).
  base::Value config = ParseJson(
      R"([{"name": "Certificate Profile 1",
           "cert_profile_id":"cert_profile_id_1",
           "policy_version":"cert_profile_version_1",
           "key_algorithm":"rsa"}])");
  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), config);
  FastForwardBy(base::Seconds(1));

  {
    worker->ResetExpected();
    scheduler.ResetOneWorker(kCertProfileId);
    ASSERT_EQ(scheduler.GetWorkers().size(), 1U);
    MockCertProvisioningWorker* second_worker =
        mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
    second_worker->SetExpectations(Exactly(1), false, cert_profile, "");
    scheduler.OnProfileFinished(cert_profile, kCertProvId,
                                CertProvisioningWorkerState::kCanceled);
    ASSERT_EQ(scheduler.GetWorkers().size(), 1U);
  }
}
}  // namespace
}  // namespace cert_provisioning
}  // namespace ash

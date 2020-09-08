// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_scheduler.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_test_helpers.h"
#include "chrome/browser/chromeos/cert_provisioning/cert_provisioning_worker.h"
#include "chrome/browser/chromeos/cert_provisioning/mock_cert_provisioning_worker.h"
#include "chrome/browser/chromeos/platform_keys/mock_platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/common/pref_names.h"
#include "chromeos/network/network_state_test_helper.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
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

namespace chromeos {
namespace cert_provisioning {
namespace {

constexpr char kWifiServiceGuid[] = "wifi_guid";
constexpr char kCertProfileId[] = "cert_profile_id_1";
constexpr char kCertProfileVersion[] = "cert_profile_version_1";
constexpr TimeDelta kCertProfileRenewalPeriod = TimeDelta::FromSeconds(0);

//=============== TestCertProvisioningSchedulerObserver ========================

class TestCertProvisioningSchedulerObserver
    : public CertProvisioningSchedulerObserver {
 public:
  TestCertProvisioningSchedulerObserver() = default;
  ~TestCertProvisioningSchedulerObserver() override = default;

  TestCertProvisioningSchedulerObserver(
      const TestCertProvisioningSchedulerObserver& other) = delete;
  TestCertProvisioningSchedulerObserver& operator=(
      const TestCertProvisioningSchedulerObserver& other) = delete;

  // CertProvisioningSchedulerObserver:
  void OnVisibleStateChanged() override { run_loop_->Quit(); }

  // Waits for one call to happen (since construction or since the previous
  // WaitForOneCall has returned).
  void WaitForOneCall() {
    run_loop_->Run();
    // Create a new RunLoop so it can already be terminated when the next
    // OnVisibleStateChanged() call comes in.
    run_loop_ = std::make_unique<base::RunLoop>();
  }

 private:
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

  void FastForwardBy(TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void SetUp() override {
    CertProvisioningWorkerFactory::SetFactoryForTesting(&mock_factory_);
  }

  void TearDown() override {
    CertProvisioningWorkerFactory::SetFactoryForTesting(nullptr);
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
  StrictMock<SpyingFakeCryptohomeClient> fake_cryptohome_client_;
  TestingPrefServiceSimple pref_service_;
  policy::MockCloudPolicyClient cloud_policy_client_;
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
      kCertScope, GetProfile(), &pref_service_, &cloud_policy_client_,
      &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      std::move(mock_invalidation_factory_obj));

  // From CertProvisioningSchedulerImpl::CleanVaKeysIfIdle.
  EXPECT_CALL(fake_cryptohome_client_,
              OnTpmAttestationDeleteKeysByPrefix(
                  attestation::AttestationKeyType::KEY_USER, kKeyNamePrefix))
      .Times(1);

  // The policy is empty, so no workers should be created yet.
  FastForwardBy(TimeDelta::FromSeconds(1));
  EXPECT_EQ(scheduler.GetWorkers().size(), 0U);

  EXPECT_CALL(*mock_invalidation_factory, Create)
      .Times(1)
      .WillOnce(
          Return(ByMove(nullptr)));  // nullptr is good enough for mock worker.

  // One worker will be created on prefs update.
  CertProfile cert_profile(kCertProfileId, kCertProfileVersion,
                           /*is_va_enabled=*/true, kCertProfileRenewalPeriod);
  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1),
                          /*is_waiting=*/false, cert_profile);

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
  scheduler.OnProfileFinished(cert_profile,
                              CertProvisioningWorkerState::kSucceeded);

  // Finished worker should be deleted.
  EXPECT_EQ(scheduler.GetWorkers().size(), 0U);
  EXPECT_TRUE(scheduler.GetFailedCertProfileIds().empty());

  certificate_helper_->AddCert(kCertScope, kCertProfileId);

  // Check one more time that scheduler doesn't create new workers for
  // finished certificate profiles (the factory will fail on an attempt to
  // do so).
  scheduler.UpdateAllCerts();

  FastForwardBy(TimeDelta::FromSeconds(100));
}

TEST_F(CertProvisioningSchedulerTest, WorkerFailed) {
  const CertScope kCertScope = CertScope::kDevice;

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_, &cloud_policy_client_,
      &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  // From CertProvisioningSchedulerImpl::CleanVaKeysIfIdle.
  EXPECT_CALL(fake_cryptohome_client_,
              OnTpmAttestationDeleteKeysByPrefix(
                  attestation::AttestationKeyType::KEY_DEVICE, kKeyNamePrefix))
      .Times(1);

  // The policy is empty, so no workers should be created yet.
  FastForwardBy(TimeDelta::FromSeconds(1));
  EXPECT_EQ(scheduler.GetWorkers().size(), 0U);

  // One worker will be created on prefs update.
  CertProfile cert_profile(kCertProfileId, kCertProfileVersion,
                           /*is_va_enabled=*/true, kCertProfileRenewalPeriod);
  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1),
                          /*is_waiting=*/false, cert_profile);

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
  scheduler.OnProfileFinished(cert_profile,
                              CertProvisioningWorkerState::kFailed);

  // Failed worker should be deleted, failed profile ID is saved, no new
  // workers should be created.
  EXPECT_EQ(scheduler.GetWorkers().size(), 0U);
  EXPECT_TRUE(
      base::Contains(scheduler.GetFailedCertProfileIds(), kCertProfileId));

  certificate_helper_->AddCert(kCertScope, kCertProfileId);

  // Check one more time that scheduler doesn't create new workers for failed
  // certificate profiles (the factory will fail on an attempt to do so).
  scheduler.UpdateAllCerts();
}

TEST_F(CertProvisioningSchedulerTest, InitialAndDailyUpdates) {
  const CertScope kCertScope = CertScope::kUser;

  CertProfile cert_profile(kCertProfileId, kCertProfileVersion,
                           /*is_va_enabled=*/true, kCertProfileRenewalPeriod);

  // Add 1 certificate profile to the policy (the values are the same as
  // in |cert_profile|).
  base::Value config = ParseJson(
      R"([{"name": "Certificate Profile 1",
           "cert_profile_id":"cert_profile_id_1",
           "policy_version":"cert_profile_version_1",
           "key_algorithm":"rsa"}])");
  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), config);

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_, &cloud_policy_client_,
      &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  // From CertProvisioningSchedulerImpl::CleanVaKeysIfIdle.
  EXPECT_CALL(fake_cryptohome_client_,
              OnTpmAttestationDeleteKeysByPrefix(
                  attestation::AttestationKeyType::KEY_USER, kKeyNamePrefix))
      .Times(1);

  // Now one worker should be created.
  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                          cert_profile);
  FastForwardBy(TimeDelta::FromSeconds(1));
  ASSERT_EQ(scheduler.GetWorkers().size(), 1U);

  // Emulate callback from the worker.
  scheduler.OnProfileFinished(cert_profile,
                              CertProvisioningWorkerState::kFailed);

  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);
  EXPECT_TRUE(
      base::Contains(scheduler.GetFailedCertProfileIds(), kCertProfileId));

  // No workers should be created yet.
  FastForwardBy(TimeDelta::FromHours(20));
  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);

  // Now list of failed profiles should be cleared that will cause a new attempt
  // to provision certificate.
  MockCertProvisioningWorker* worker2 =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
  worker2->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                           cert_profile);
  FastForwardBy(TimeDelta::FromHours(5));
  ASSERT_EQ(scheduler.GetWorkers().size(), 1U);

  // Emulate callback from the worker.
  scheduler.OnProfileFinished(cert_profile,
                              CertProvisioningWorkerState::kSucceeded);

  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);
  EXPECT_TRUE(scheduler.GetFailedCertProfileIds().empty());
}

TEST_F(CertProvisioningSchedulerTest, MultipleWorkers) {
  const CertScope kCertScope = CertScope::kDevice;

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_, &cloud_policy_client_,
      &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  // From CertProvisioningSchedulerImpl::CleanVaKeysIfIdle.
  EXPECT_CALL(fake_cryptohome_client_,
              OnTpmAttestationDeleteKeysByPrefix(
                  attestation::AttestationKeyType::KEY_DEVICE, kKeyNamePrefix))
      .Times(1);

  // The policy is empty, so no workers should be created yet.
  FastForwardBy(TimeDelta::FromSeconds(1));
  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);

  // New workers will be created on prefs update.
  const char kCertProfileId0[] = "cert_profile_id_0";
  const char kCertProfileVersion0[] = "cert_profile_version_0";
  CertProfile cert_profile0(kCertProfileId0, kCertProfileVersion0,
                            /*is_va_enabled=*/true, kCertProfileRenewalPeriod);
  const char kCertProfileId1[] = "cert_profile_id_1";
  const char kCertProfileVersion1[] = "cert_profile_version_1";
  CertProfile cert_profile1(kCertProfileId1, kCertProfileVersion1,
                            /*is_va_enabled=*/true, kCertProfileRenewalPeriod);
  const char kCertProfileId2[] = "cert_profile_id_2";
  const char kCertProfileVersion2[] = "cert_profile_version_2";
  CertProfile cert_profile2(kCertProfileId2, kCertProfileVersion2,
                            /*is_va_enabled=*/true, kCertProfileRenewalPeriod);
  MockCertProvisioningWorker* worker0 =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile0);
  worker0->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                           cert_profile0);
  MockCertProvisioningWorker* worker1 =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile1);
  worker1->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                           cert_profile1);
  MockCertProvisioningWorker* worker2 =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile2);
  worker2->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                           cert_profile2);

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
  scheduler.OnProfileFinished(cert_profile0,
                              CertProvisioningWorkerState::kSucceeded);

  // worker1 is waiting. Should be continued.
  worker1->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/true,
                           cert_profile1);

  // worker2 failed. Should be deleted and the profile id should be saved.
  scheduler.OnProfileFinished(cert_profile2,
                              CertProvisioningWorkerState::kFailed);

  EXPECT_EQ(scheduler.GetWorkers().size(), 1U);
  EXPECT_TRUE(
      base::Contains(scheduler.GetFailedCertProfileIds(), kCertProfileId2));

  certificate_helper_->AddCert(kCertScope, kCertProfileId0);

  // Make scheduler check workers state.
  scheduler.UpdateAllCerts();

  EXPECT_EQ(scheduler.GetWorkers().size(), 1U);
  EXPECT_TRUE(
      base::Contains(scheduler.GetFailedCertProfileIds(), kCertProfileId2));

  // Check one more time that scheduler doesn't create new workers for failed
  // certificate profiles (the factory will fail on an attempt to do so).
  scheduler.UpdateAllCerts();
  EXPECT_EQ(scheduler.GetWorkers().size(), 1U);
}

TEST_F(CertProvisioningSchedulerTest, RemoveCertWithoutPolicy) {
  const CertScope kCertScope = CertScope::kDevice;

  certificate_helper_->AddCert(kCertScope, kCertProfileId);

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_, &cloud_policy_client_,
      &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  EXPECT_CALL(
      platform_keys_service_,
      RemoveCertificate(GetPlatformKeysTokenId(kCertScope),
                        /*certificate=*/certificate_helper_->GetCerts().back(),
                        /*callback=*/_))
      .Times(1);

  FastForwardBy(TimeDelta::FromSeconds(1));
}

TEST_F(CertProvisioningSchedulerTest, DeserializeWorkers) {
  const CertScope kCertScope = CertScope::kUser;

  CertProfile cert_profile(kCertProfileId, kCertProfileVersion,
                           /*is_va_enabled=*/true, kCertProfileRenewalPeriod);

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
  base::Value all_saved_workers(base::Value::Type::DICTIONARY);
  all_saved_workers.SetKey("cert_profile_1", saved_worker.Clone());

  pref_service_.Set(GetPrefNameForSerialization(kCertScope), all_saved_workers);

  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectDeserializeReturnMock(kCertScope, saved_worker);
  // is_waiting==true should be set by Serializer so Scheduler knows that the
  // worker has to be continued manually.
  worker->SetExpectations(/*do_step_times=*/AtLeast(1),
                          /*is_waiting=*/true, cert_profile);

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_, &cloud_policy_client_,
      &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  // Now one worker should be created.
  FastForwardBy(TimeDelta::FromSeconds(1));
  ASSERT_EQ(scheduler.GetWorkers().size(), 1U);
}

TEST_F(CertProvisioningSchedulerTest, InconsistentDataErrorHandling) {
  const CertScope kCertScope = CertScope::kDevice;

  const char kCertProfileVersion1[] = "cert_profile_version_1";
  const char kCertProfileVersion2[] = "cert_profile_version_2";

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_, &cloud_policy_client_,
      &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  // From CertProvisioningSchedulerImpl::CleanVaKeysIfIdle.
  EXPECT_CALL(fake_cryptohome_client_,
              OnTpmAttestationDeleteKeysByPrefix(
                  attestation::AttestationKeyType::KEY_DEVICE, kKeyNamePrefix))
      .Times(1);

  // The policy is empty, so no workers should be created yet.
  FastForwardBy(TimeDelta::FromSeconds(1));
  EXPECT_EQ(scheduler.GetWorkers().size(), 0U);

  CertProfile cert_profile_v1(kCertProfileId, kCertProfileVersion1,
                              /*is_va_enabled=*/true,
                              kCertProfileRenewalPeriod);

  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile_v1);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                          cert_profile_v1);

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
      cert_profile_v1, CertProvisioningWorkerState::kInconsistentDataError);

  // Failed worker should be deleted, failed profile ID should not be saved, no
  // new workers should be created.
  EXPECT_EQ(scheduler.GetWorkers().size(), 0U);
  EXPECT_TRUE(scheduler.GetFailedCertProfileIds().empty());

  // Add a new worker to the factory.
  worker = mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile_v1);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                          cert_profile_v1);

  // After some delay a new worker should be created to try again.
  FastForwardBy(TimeDelta::FromSeconds(31));
  EXPECT_EQ(scheduler.GetWorkers().size(), 1U);

  // Emulate callback from the worker.
  scheduler.OnProfileFinished(
      cert_profile_v1, CertProvisioningWorkerState::kInconsistentDataError);

  // Failed worker should be deleted, failed profile ID should not be saved, no
  // new workers should be created.
  EXPECT_EQ(scheduler.GetWorkers().size(), 0U);
  EXPECT_TRUE(scheduler.GetFailedCertProfileIds().empty());

  // Add a new worker to the factory.
  CertProfile cert_profile_v2(kCertProfileId, kCertProfileVersion2,
                              /*is_va_enabled=*/true,
                              kCertProfileRenewalPeriod);
  worker = mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile_v2);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                          cert_profile_v2);

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
  scheduler.UpdateAllCerts();
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
  FastForwardBy(TimeDelta::FromSeconds(10));
  scheduler.OnProfileFinished(
      cert_profile_v1, CertProvisioningWorkerState::kInconsistentDataError);
  EXPECT_EQ(scheduler.GetWorkers().size(), 0U);
}

TEST_F(CertProvisioningSchedulerTest, RetryAfterNoInternetConnection) {
  const CertScope kCertScope = CertScope::kDevice;
  SetWifiNetworkState(shill::kStateIdle);

  CertProfile cert_profile(kCertProfileId, kCertProfileVersion,
                           /*is_va_enabled=*/true, kCertProfileRenewalPeriod);
  // Add 1 certificate profile to the policy (the values are the same as
  // in |cert_profile|).
  base::Value config = ParseJson(
      R"([{"name": "Certificate Profile 1",
           "cert_profile_id":"cert_profile_id_1",
           "policy_version":"cert_profile_version_1",
           "key_algorithm":"rsa"}])");
  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), config);

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_, &cloud_policy_client_,
      &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  // From CertProvisioningSchedulerImpl::CleanVaKeysIfIdle.
  EXPECT_CALL(fake_cryptohome_client_,
              OnTpmAttestationDeleteKeysByPrefix(
                  attestation::AttestationKeyType::KEY_DEVICE, kKeyNamePrefix))
      .Times(1);

  FastForwardBy(TimeDelta::FromHours(72));
  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);

  // Add a new worker to the factory.
  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                          cert_profile);

  SetWifiNetworkState(shill::kStateOnline);

  ASSERT_EQ(scheduler.GetWorkers().size(), 1U);
}

TEST_F(CertProvisioningSchedulerTest, DeleteWorkerWithoutPolicy) {
  const CertScope kCertScope = CertScope::kDevice;

  CertProfile cert_profile(kCertProfileId, kCertProfileVersion,
                           /*is_va_enabled=*/true, kCertProfileRenewalPeriod);
  // Add 1 certificate profile to the policy (the values are the same as
  // in |cert_profile|).
  base::Value config = ParseJson(
      R"([{"name": "Certificate Profile 1",
           "cert_profile_id":"cert_profile_id_1",
           "policy_version":"cert_profile_version_1",
           "key_algorithm":"rsa"}])");

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_, &cloud_policy_client_,
      &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  // From CertProvisioningSchedulerImpl::CleanVaKeysIfIdle.
  EXPECT_CALL(fake_cryptohome_client_,
              OnTpmAttestationDeleteKeysByPrefix(
                  attestation::AttestationKeyType::KEY_DEVICE, kKeyNamePrefix))
      .Times(1);

  // Add a new worker to the factory.
  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                          cert_profile);

  // Prefs update will be ignored because initialization task has not finished
  // yet.
  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), config);
  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);

  FastForwardBy(TimeDelta::FromSeconds(1));
  EXPECT_EQ(scheduler.GetWorkers().size(), 1U);

  EXPECT_CALL(*worker, Stop(CertProvisioningWorkerState::kCanceled));

  config = ParseJson("[]");
  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), config);

  FastForwardBy(TimeDelta::FromSeconds(1));
  // Emulate callback from the worker.
  scheduler.OnProfileFinished(cert_profile,
                              CertProvisioningWorkerState::kCanceled);

  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);
}

TEST_F(CertProvisioningSchedulerTest, DeleteVaKeysOnIdle) {
  const CertScope kCertScope = CertScope::kDevice;

  {
    CertProvisioningSchedulerImpl scheduler(
        kCertScope, GetProfile(), &pref_service_, &cloud_policy_client_,
        &platform_keys_service_,
        network_state_test_helper_.network_state_handler(),
        MakeFakeInvalidationFactory());

    // From CertProvisioningSchedulerImpl::CleanVaKeysIfIdle.
    EXPECT_CALL(
        fake_cryptohome_client_,
        OnTpmAttestationDeleteKeysByPrefix(
            attestation::AttestationKeyType::KEY_DEVICE, kKeyNamePrefix))
        .Times(1);

    FastForwardBy(TimeDelta::FromSeconds(1));
  }

  {
    CertProfile cert_profile(kCertProfileId, kCertProfileVersion,
                             /*is_va_enabled=*/true, kCertProfileRenewalPeriod);

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
    base::Value all_saved_workers(base::Value::Type::DICTIONARY);
    all_saved_workers.SetKey("cert_profile_1", saved_worker.Clone());

    pref_service_.Set(GetPrefNameForSerialization(kCertScope),
                      all_saved_workers);

    MockCertProvisioningWorker* worker =
        mock_factory_.ExpectDeserializeReturnMock(kCertScope, saved_worker);
    // This worker should be deleted approximately right after creation, hence
    // no calls for DoStep.
    worker->SetExpectations(/*do_step_times=*/Exactly(0),
                            /*is_waiting=*/true, cert_profile);

    CertProvisioningSchedulerImpl scheduler(
        kCertScope, GetProfile(), &pref_service_, &cloud_policy_client_,
        &platform_keys_service_,
        network_state_test_helper_.network_state_handler(),
        MakeFakeInvalidationFactory());

    EXPECT_CALL(fake_cryptohome_client_, OnTpmAttestationDeleteKeysByPrefix)
        .Times(0);

    FastForwardBy(TimeDelta::FromSeconds(1));
  }
}

TEST_F(CertProvisioningSchedulerTest, UpdateOneCert) {
  const CertScope kCertScope = CertScope::kUser;

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_, &cloud_policy_client_,
      &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  CertProfile cert_profile(kCertProfileId, kCertProfileVersion,
                           /*is_va_enabled=*/true, kCertProfileRenewalPeriod);

  // From CertProvisioningSchedulerImpl::CleanVaKeysIfIdle.
  EXPECT_CALL(fake_cryptohome_client_, OnTpmAttestationDeleteKeysByPrefix);
  FastForwardBy(TimeDelta::FromSeconds(1));

  // There is no policies yet, |kCertProfileId| will not be found.
  scheduler.UpdateOneCert(kCertProfileId);
  FastForwardBy(TimeDelta::FromSeconds(1));
  ASSERT_TRUE(scheduler.GetWorkers().empty());

  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
  worker->SetExpectations(/*do_step_times=*/Exactly(1),
                          /*is_waiting=*/false, cert_profile);

  // Add 1 certificate profile to the policy (the values are the same as
  // in |cert_profile|).
  base::Value config = ParseJson(
      R"([{"name": "Certificate Profile 1",
           "cert_profile_id":"cert_profile_id_1",
           "policy_version":"cert_profile_version_1",
           "key_algorithm":"rsa"}])");
  pref_service_.Set(GetPrefNameForCertProfiles(kCertScope), config);
  FastForwardBy(TimeDelta::FromSeconds(1));

  // If worker is waiting, it should be continued.
  {
    worker->SetExpectations(/*do_step_times=*/Exactly(1),
                            /*is_waiting=*/true, cert_profile);

    scheduler.UpdateOneCert(kCertProfileId);
    FastForwardBy(TimeDelta::FromSeconds(1));
    ASSERT_EQ(scheduler.GetWorkers().size(), 1U);
  }

  // If worker is not waiting, it should not be continued.
  {
    worker->SetExpectations(/*do_step_times=*/Exactly(0),
                            /*is_waiting=*/false, cert_profile);

    scheduler.UpdateOneCert(kCertProfileId);
    FastForwardBy(TimeDelta::FromSeconds(1));
    ASSERT_EQ(scheduler.GetWorkers().size(), 1U);
  }

  // If there is no intenet connection, the worker should not be continued
  // until it is restored.
  {
    SetWifiNetworkState(shill::kStateIdle);

    worker->SetExpectations(/*do_step_times=*/Exactly(0),
                            /*is_waiting=*/true, cert_profile);

    scheduler.UpdateOneCert(kCertProfileId);
    FastForwardBy(TimeDelta::FromSeconds(1));
    ASSERT_EQ(scheduler.GetWorkers().size(), 1U);

    worker->SetExpectations(/*do_step_times=*/Exactly(1),
                            /*is_waiting=*/true, cert_profile);

    SetWifiNetworkState(shill::kStateOnline);
  }

  // Emulate callback from the worker.
  scheduler.OnProfileFinished(cert_profile,
                              CertProvisioningWorkerState::kSucceeded);
  FastForwardBy(TimeDelta::FromSeconds(1));
  ASSERT_TRUE(scheduler.GetWorkers().empty());

  certificate_helper_->AddCert(kCertScope, kCertProfileId);

  {
    // If a certificate already exists, a new worker should not be created.
    scheduler.UpdateOneCert(kCertProfileId);
    FastForwardBy(TimeDelta::FromSeconds(1));
    ASSERT_TRUE(scheduler.GetWorkers().empty());
  }
}

TEST_F(CertProvisioningSchedulerTest, CertRenewal) {
  const CertScope kCertScope = CertScope::kUser;
  // 1 day == 86400 seconds.
  const TimeDelta kRenewalPeriod = TimeDelta::FromDays(1);

  CertProfile cert_profile(kCertProfileId, kCertProfileVersion,
                           /*is_va_enabled=*/true, kRenewalPeriod);

  const Time t1 = Time::Now() - TimeDelta::FromDays(1);
  const Time t2 = Time::Now() + TimeDelta::FromDays(7);
  certificate_helper_->AddCert(kCertScope, kCertProfileId,
                               platform_keys::Status::kSuccess,
                               /*nat_valid_before=*/t1, /*not_valid_after=*/t2);

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
      kCertScope, GetProfile(), &pref_service_, &cloud_policy_client_,
      &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  // From CertProvisioningScheduler::CleanVaKeysIfIdle.
  EXPECT_CALL(fake_cryptohome_client_,
              OnTpmAttestationDeleteKeysByPrefix(
                  attestation::AttestationKeyType::KEY_USER, kKeyNamePrefix))
      .Times(1);

  // The certificate already exists, nothing should happen on scheduler
  // creation.
  FastForwardBy(TimeDelta::FromSeconds(1));
  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);

  // Also nothing should happen in the next ~6 days.
  FastForwardBy(TimeDelta::FromDays(5) + TimeDelta::FromHours(23));
  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);

  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1),
                          /*is_waiting=*/false, cert_profile);

  // One day (according to the policy) before the certificate expires, scheduler
  // should create a new worker to provision a replacement.
  FastForwardBy(TimeDelta::FromHours(1));
  ASSERT_EQ(scheduler.GetWorkers().size(), 1U);
}

TEST_F(CertProvisioningSchedulerTest, PlatformKeysServiceShutDown) {
  CertScope kCertScope = CertScope::kDevice;

  platform_keys::PlatformKeysServiceObserver* observer = nullptr;
  EXPECT_CALL(platform_keys_service_, AddObserver(_))
      .WillOnce(SaveArg<0>(&observer));
  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_, &cloud_policy_client_,
      &platform_keys_service_,
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
  const char kCertProfileId[] = "cert_profile_id_1";
  const char kCertProfileVersion[] = "cert_profile_version_1";
  CertProfile cert_profile{kCertProfileId, kCertProfileVersion,
                           /*is_va_enabled=*/true, kCertProfileRenewalPeriod};

  MockCertProvisioningWorker* worker =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile);
  worker->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                          cert_profile);
  scheduler.UpdateAllCerts();

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
  scheduler.UpdateAllCerts();
}

TEST_F(CertProvisioningSchedulerTest, StateChangeNotifications) {
  const CertScope kCertScope = CertScope::kDevice;

  CertProvisioningSchedulerImpl scheduler(
      kCertScope, GetProfile(), &pref_service_, &cloud_policy_client_,
      &platform_keys_service_,
      network_state_test_helper_.network_state_handler(),
      MakeFakeInvalidationFactory());

  TestCertProvisioningSchedulerObserver observer;
  scheduler.AddObserver(&observer);

  // From CertProvisioningSchedulerImpl::CleanVaKeysIfIdle.
  EXPECT_CALL(fake_cryptohome_client_,
              OnTpmAttestationDeleteKeysByPrefix(
                  attestation::AttestationKeyType::KEY_DEVICE, kKeyNamePrefix))
      .Times(1);

  // The policy is empty, so no workers should be created yet.
  FastForwardBy(TimeDelta::FromSeconds(1));
  ASSERT_EQ(scheduler.GetWorkers().size(), 0U);

  // Two new workers will be created on prefs update.
  // Expect a state change notification for this.
  const char kCertProfileId0[] = "cert_profile_id_0";
  const char kCertProfileVersion0[] = "cert_profile_version_0";
  CertProfile cert_profile0(kCertProfileId0, kCertProfileVersion0,
                            /*is_va_enabled=*/true, kCertProfileRenewalPeriod);
  const char kCertProfileId1[] = "cert_profile_id_1";
  const char kCertProfileVersion1[] = "cert_profile_version_1";
  CertProfile cert_profile1(kCertProfileId1, kCertProfileVersion1,
                            /*is_va_enabled=*/true, kCertProfileRenewalPeriod);

  MockCertProvisioningWorker* worker0 =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile0);
  worker0->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                           cert_profile0);
  MockCertProvisioningWorker* worker1 =
      mock_factory_.ExpectCreateReturnMock(kCertScope, cert_profile1);
  worker1->SetExpectations(/*do_step_times=*/AtLeast(1), /*is_waiting=*/false,
                           cert_profile1);

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
  scheduler.OnProfileFinished(cert_profile0,
                              CertProvisioningWorkerState::kSucceeded);
  observer.WaitForOneCall();

  // worker1 failed. Should be deleted and the profile id should be saved, and a
  // state change event should be fired for that.
  scheduler.OnProfileFinished(cert_profile1,
                              CertProvisioningWorkerState::kFailed);
  observer.WaitForOneCall();

  EXPECT_EQ(scheduler.GetWorkers().size(), 0U);
  EXPECT_TRUE(
      base::Contains(scheduler.GetFailedCertProfileIds(), kCertProfileId1));

  scheduler.RemoveObserver(&observer);
}

}  // namespace
}  // namespace cert_provisioning
}  // namespace chromeos

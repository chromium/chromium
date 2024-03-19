// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/device_settings_service.h"

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ash/ownership/owner_key_loader.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/net/fake_nss_service.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using ::testing::Mock;

namespace ash {

namespace {

class MockDeviceSettingsObserver : public DeviceSettingsService::Observer {
 public:
  ~MockDeviceSettingsObserver() override {}

  MOCK_METHOD0(OwnershipStatusChanged, void());
  MOCK_METHOD0(DeviceSettingsUpdated, void());
  MOCK_METHOD0(OnDeviceSettingsServiceShutdown, void());
};

}  // namespace

class DeviceSettingsServiceTest : public DeviceSettingsTestBase {
 public:
  DeviceSettingsServiceTest(const DeviceSettingsServiceTest&) = delete;
  DeviceSettingsServiceTest& operator=(const DeviceSettingsServiceTest&) =
      delete;

  void SetOperationCompleted() { operation_completed_ = true; }

  void SetOwnershipStatus(
      DeviceSettingsService::OwnershipStatus ownership_status) {
    ownership_status_ = ownership_status;
  }

  void OnIsOwner(bool is_owner) {
    is_owner_ = is_owner;
    is_owner_set_ = true;
  }

 protected:
  DeviceSettingsServiceTest()
      : operation_completed_(false),
        is_owner_(true),
        is_owner_set_(false),
        ownership_status_(
            DeviceSettingsService::OwnershipStatus::kOwnershipUnknown) {}
  ~DeviceSettingsServiceTest() override = default;

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();

    // Disable owner key migration.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{kStoreOwnerKeyInPrivateSlot},
        /*disabled_features=*/{kMigrateOwnerKeyToPrivateSlot});

    device_policy_->payload()
        .mutable_device_policy_refresh_rate()
        ->set_device_policy_refresh_rate(120);
    ReloadDevicePolicy();
  }

  void CheckPolicy() {
    ASSERT_TRUE(device_settings_service_->policy_data());
    EXPECT_EQ(device_policy_->policy_data().SerializeAsString(),
              device_settings_service_->policy_data()->SerializeAsString());
    ASSERT_TRUE(device_settings_service_->device_settings());
    EXPECT_EQ(device_policy_->payload().SerializeAsString(),
              device_settings_service_->device_settings()->SerializeAsString());
  }

  StubInstallAttributes* GetInstallAttributes() {
    return static_cast<StubInstallAttributes*>(InstallAttributes::Get());
  }

  base::test::ScopedFeatureList feature_list_;
  bool operation_completed_;
  bool is_owner_;
  bool is_owner_set_;
  DeviceSettingsService::OwnershipStatus ownership_status_;
  base::HistogramTester histogram_tester_;
};

TEST_F(DeviceSettingsServiceTest, LoadNoKey) {
  owner_key_util_->Clear();
  ReloadDeviceSettings();

  EXPECT_EQ(DeviceSettingsService::STORE_KEY_UNAVAILABLE,
            device_settings_service_->status());
  EXPECT_FALSE(device_settings_service_->policy_data());
  EXPECT_FALSE(device_settings_service_->device_settings());
}

TEST_F(DeviceSettingsServiceTest, LoadNoPolicy) {
  session_manager_client_.set_device_policy(std::string());
  ReloadDeviceSettings();

  EXPECT_EQ(DeviceSettingsService::STORE_NO_POLICY,
            device_settings_service_->status());
  EXPECT_FALSE(device_settings_service_->policy_data());
  EXPECT_FALSE(device_settings_service_->device_settings());
}

TEST_F(DeviceSettingsServiceTest, LoadValidationError) {
  device_policy_->policy().set_policy_data_signature("bad");
  session_manager_client_.set_device_policy(device_policy_->GetBlob());
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  ReloadDeviceSettings();

  EXPECT_EQ(DeviceSettingsService::STORE_VALIDATION_ERROR,
            device_settings_service_->status());
  EXPECT_FALSE(device_settings_service_->policy_data());
  EXPECT_FALSE(device_settings_service_->device_settings());
}

TEST_F(DeviceSettingsServiceTest, LoadValidationErrorFutureTimestamp) {
  base::Time timestamp(base::Time::NowFromSystemTime() + base::Days(5000));
  device_policy_->policy_data().set_timestamp(
      (timestamp - base::Time::UnixEpoch()).InMilliseconds());
  device_policy_->Build();
  session_manager_client_.set_device_policy(device_policy_->GetBlob());
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  ReloadDeviceSettings();

  // Loading a cached device policy with a timestamp in the future should work,
  // since this may be due to a broken clock on the client device.
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_->status());
  CheckPolicy();
}

TEST_F(DeviceSettingsServiceTest, LoadSuccess) {
  ReloadDeviceSettings();

  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_->status());
  CheckPolicy();
}

TEST_F(DeviceSettingsServiceTest, LoadAfterSessionStopping) {
  SetSessionStopping();
  device_settings_service_->LoadImmediately();
  EXPECT_FALSE(device_settings_service_->policy_data());
  EXPECT_FALSE(device_settings_service_->device_settings());
}

TEST_F(DeviceSettingsServiceTest, StoreFailure) {
  owner_key_util_->Clear();
  session_manager_client_.set_device_policy(std::string());
  ReloadDeviceSettings();
  EXPECT_EQ(DeviceSettingsService::STORE_KEY_UNAVAILABLE,
            device_settings_service_->status());

  session_manager_client_.ForceStorePolicyFailure(true);
  device_settings_service_->Store(
      device_policy_->GetCopy(),
      base::BindOnce(&DeviceSettingsServiceTest::SetOperationCompleted,
                     base::Unretained(this)));
  FlushDeviceSettings();
  EXPECT_TRUE(operation_completed_);
  EXPECT_EQ(DeviceSettingsService::STORE_OPERATION_FAILED,
            device_settings_service_->status());
}

TEST_F(DeviceSettingsServiceTest, StoreSuccess) {
  owner_key_util_->Clear();
  session_manager_client_.set_device_policy(std::string());
  ReloadDeviceSettings();
  EXPECT_EQ(DeviceSettingsService::STORE_KEY_UNAVAILABLE,
            device_settings_service_->status());

  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  InitOwner(AccountId::FromUserEmail(device_policy_->policy_data().username()),
            /*tpm_is_ready=*/false);
  device_settings_service_->Store(
      device_policy_->GetCopy(),
      base::BindOnce(&DeviceSettingsServiceTest::SetOperationCompleted,
                     base::Unretained(this)));
  FlushDeviceSettings();
  EXPECT_TRUE(operation_completed_);
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_->status());
  CheckPolicy();
}

TEST_F(DeviceSettingsServiceTest, StoreRotation) {
  ReloadDeviceSettings();
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_->status());

  device_policy_->payload()
      .mutable_device_policy_refresh_rate()
      ->set_device_policy_refresh_rate(300);
  device_policy_->SetDefaultNewSigningKey();
  device_policy_->Build();
  device_settings_service_->Store(device_policy_->GetCopy(),
                                  base::OnceClosure());
  FlushDeviceSettings();
  owner_key_util_->SetPublicKeyFromPrivateKey(
      *device_policy_->GetNewSigningKey());
  device_settings_service_->OwnerKeySet(true);
  FlushDeviceSettings();
  EXPECT_EQ(DeviceSettingsService::STORE_SUCCESS,
            device_settings_service_->status());
  CheckPolicy();

  // Check the new key has been loaded.
  EXPECT_EQ(device_policy_->GetPublicNewSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
}

TEST_F(DeviceSettingsServiceTest, OwnershipStatus) {
  owner_key_util_->Clear();

  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  EXPECT_FALSE(device_settings_service_->GetPublicKey().get());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipUnknown,
            device_settings_service_->GetOwnershipStatus());

  device_settings_service_->GetOwnershipStatusAsync(base::BindOnce(
      &DeviceSettingsServiceTest::SetOwnershipStatus, base::Unretained(this)));
  FlushDeviceSettings();
  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  EXPECT_TRUE(device_settings_service_->GetPublicKey()->is_empty());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipNone,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipNone,
            ownership_status_);

  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  ReloadDeviceSettings();
  device_settings_service_->GetOwnershipStatusAsync(base::BindOnce(
      &DeviceSettingsServiceTest::SetOwnershipStatus, base::Unretained(this)));
  FlushDeviceSettings();
  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_FALSE(device_settings_service_->GetPublicKey()->is_empty());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            ownership_status_);

  owner_key_util_->ImportPrivateKeyAndSetPublicKey(
      device_policy_->GetSigningKey());
  InitOwner(AccountId::FromUserEmail(device_policy_->policy_data().username()),
            true);
  device_settings_service_->GetOwnershipStatusAsync(base::BindOnce(
      &DeviceSettingsServiceTest::SetOwnershipStatus, base::Unretained(this)));
  FlushDeviceSettings();
  EXPECT_TRUE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_FALSE(device_settings_service_->GetPublicKey()->is_empty());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            ownership_status_);
}

TEST_F(DeviceSettingsServiceTest, OnTPMTokenReadyForNonOwner) {
  owner_key_util_->Clear();

  TestingProfile::Builder profile_builder;
  profile_builder.SetProfileName("non@owner.com");
  std::unique_ptr<TestingProfile> non_owner_profile = profile_builder.Build();

  FakeNssService::InitializeForBrowserContext(non_owner_profile.get(),
                                              /*enable_system_slot=*/false);

  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  EXPECT_FALSE(device_settings_service_->GetPublicKey().get());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipUnknown,
            device_settings_service_->GetOwnershipStatus());

  const std::string& user_id = device_policy_->policy_data().username();
  InitOwner(AccountId::FromUserEmail(user_id), false);
  OwnerSettingsServiceAsh* service =
      OwnerSettingsServiceAshFactory::GetForBrowserContext(
          non_owner_profile.get());
  ASSERT_TRUE(service);
  service->IsOwnerAsync(base::BindOnce(&DeviceSettingsServiceTest::OnIsOwner,
                                       base::Unretained(this)));

  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  ReloadDeviceSettings();

  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_FALSE(device_settings_service_->GetPublicKey()->is_empty());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_FALSE(is_owner_set_);

  service->OnTPMTokenReady();
  FlushDeviceSettings();

  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_FALSE(device_settings_service_->GetPublicKey()->is_empty());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_TRUE(is_owner_set_);
  EXPECT_FALSE(is_owner_);
}

TEST_F(DeviceSettingsServiceTest, OwnerPrivateKeyInTPMToken) {
  owner_key_util_->Clear();

  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  EXPECT_FALSE(device_settings_service_->GetPublicKey().get());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipUnknown,
            device_settings_service_->GetOwnershipStatus());

  const std::string& user_id = device_policy_->policy_data().username();
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  InitOwner(AccountId::FromUserEmail(user_id), false);
  OwnerSettingsServiceAsh* service =
      OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_.get());
  ASSERT_TRUE(service);
  ReloadDeviceSettings();

  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_FALSE(device_settings_service_->GetPublicKey()->is_empty());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            device_settings_service_->GetOwnershipStatus());

  owner_key_util_->ImportPrivateKeyAndSetPublicKey(
      device_policy_->GetSigningKey());
  service->OnTPMTokenReady();
  FlushDeviceSettings();

  EXPECT_TRUE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_FALSE(device_settings_service_->GetPublicKey()->is_empty());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
}

TEST_F(DeviceSettingsServiceTest, OnTPMTokenReadyForOwner) {
  owner_key_util_->Clear();

  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  EXPECT_FALSE(device_settings_service_->GetPublicKey().get());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipUnknown,
            device_settings_service_->GetOwnershipStatus());

  const std::string& user_id = device_policy_->policy_data().username();
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  InitOwner(AccountId::FromUserEmail(user_id), false);
  OwnerSettingsServiceAsh* service =
      OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_.get());
  ASSERT_TRUE(service);
  service->IsOwnerAsync(base::BindOnce(&DeviceSettingsServiceTest::OnIsOwner,
                                       base::Unretained(this)));
  ReloadDeviceSettings();

  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_FALSE(device_settings_service_->GetPublicKey()->is_empty());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_FALSE(is_owner_set_);

  owner_key_util_->ImportPrivateKeyAndSetPublicKey(
      device_policy_->GetSigningKey());
  service->OnTPMTokenReady();
  FlushDeviceSettings();

  EXPECT_TRUE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_FALSE(device_settings_service_->GetPublicKey()->is_empty());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_TRUE(is_owner_set_);
  EXPECT_TRUE(is_owner_);
}

TEST_F(DeviceSettingsServiceTest, IsCurrentUserOwnerAsyncWithLoadedCerts) {
  owner_key_util_->Clear();

  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  EXPECT_FALSE(device_settings_service_->GetPublicKey().get());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipUnknown,
            device_settings_service_->GetOwnershipStatus());

  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  owner_key_util_->ImportPrivateKeyAndSetPublicKey(
      device_policy_->GetSigningKey());

  InitOwner(AccountId::FromUserEmail(device_policy_->policy_data().username()),
            true);
  ReloadDeviceSettings();
  FlushDeviceSettings();

  EXPECT_TRUE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_FALSE(device_settings_service_->GetPublicKey()->is_empty());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_FALSE(is_owner_set_);

  OwnerSettingsServiceAsh* service =
      OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_.get());
  ASSERT_TRUE(service);
  service->IsOwnerAsync(base::BindOnce(&DeviceSettingsServiceTest::OnIsOwner,
                                       base::Unretained(this)));
  // The callback should be called immediately.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_FALSE(device_settings_service_->GetPublicKey()->is_empty());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_TRUE(is_owner_set_);
  EXPECT_TRUE(is_owner_);
}

TEST_F(DeviceSettingsServiceTest, Observer) {
  owner_key_util_->Clear();
  MockDeviceSettingsObserver observer_;
  device_settings_service_->AddObserver(&observer_);

  EXPECT_CALL(observer_, OwnershipStatusChanged()).Times(1);
  EXPECT_CALL(observer_, DeviceSettingsUpdated()).Times(1);
  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OwnershipStatusChanged()).Times(1);
  EXPECT_CALL(observer_, DeviceSettingsUpdated()).Times(1);
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  InitOwner(AccountId::FromUserEmail(device_policy_->policy_data().username()),
            /*tpm_is_ready=*/false);
  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OwnershipStatusChanged()).Times(0);
  EXPECT_CALL(observer_, DeviceSettingsUpdated()).Times(1);
  device_settings_service_->Store(device_policy_->GetCopy(),
                                  base::OnceClosure());
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OwnershipStatusChanged()).Times(0);
  EXPECT_CALL(observer_, DeviceSettingsUpdated()).Times(1);
  device_settings_service_->PropertyChangeComplete(true);
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&observer_);

  device_settings_service_->RemoveObserver(&observer_);
}

// Test that DeviceSettingsService defers load operations until after
// OwnerSettingsService finishes loading the private key and invokes
// DeviceSettingsService::InitOwner to set the owner info.
// See http://crbug.com/706820 for more details.
TEST_F(DeviceSettingsServiceTest, LoadDeferredDuringOwnershipEstablishment) {
  owner_key_util_->Clear();

  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  EXPECT_FALSE(device_settings_service_->GetPublicKey().get());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipUnknown,
            device_settings_service_->GetOwnershipStatus());

  // Mark ownership establishment is running.
  device_settings_service_->MarkWillEstablishConsumerOwnership();

  const std::string& user_id = device_policy_->policy_data().username();
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  InitOwner(AccountId::FromUserEmail(user_id), false);
  OwnerSettingsServiceAsh* service =
      OwnerSettingsServiceAshFactory::GetForBrowserContext(profile_.get());
  ASSERT_TRUE(service);
  service->IsOwnerAsync(base::BindOnce(&DeviceSettingsServiceTest::OnIsOwner,
                                       base::Unretained(this)));
  ReloadDeviceSettings();

  // No load operation should happen until OwnerSettingsService loads the
  // private key.
  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_FALSE(device_settings_service_->GetPublicKey().get());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipUnknown,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_FALSE(is_owner_set_);

  // Load the private key and trigger a reload. Load operations should finish.
  owner_key_util_->ImportPrivateKeyAndSetPublicKey(
      device_policy_->GetSigningKey());
  service->OnTPMTokenReady();
  FlushDeviceSettings();

  // Verify owner key is loaded and ownership status is updated.
  EXPECT_TRUE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_FALSE(device_settings_service_->GetPublicKey()->is_empty());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_TRUE(is_owner_set_);
  EXPECT_TRUE(is_owner_);
}

// Check that when LoadIfNotPresent function is called first time the policy
// refresh happens and when LoadIfNotPresent is called the second time, even if
// the policy has changed, the new data is not loaded.
TEST_F(DeviceSettingsServiceTest, LoadIfNotPresentDoesntRefresh) {
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  owner_key_util_->ImportPrivateKeyAndSetPublicKey(
      device_policy_->GetSigningKey());

  InitOwner(AccountId::FromUserEmail(device_policy_->policy_data().username()),
            true);
  ReloadDeviceSettings();
  device_settings_service_->LoadIfNotPresent();

  EXPECT_TRUE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_FALSE(device_settings_service_->GetPublicKey()->is_empty());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_EQ(DeviceSettingsService::OwnershipStatus::kOwnershipTaken,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_FALSE(is_owner_set_);
  EXPECT_FALSE(
      device_settings_service_->device_settings()->has_guest_mode_enabled());

  device_policy_->payload()
      .mutable_guest_mode_enabled()
      ->set_guest_mode_enabled(true);
  ReloadDeviceSettings();
  device_settings_service_->LoadIfNotPresent();
  EXPECT_FALSE(
      device_settings_service_->device_settings()->has_guest_mode_enabled());
  device_settings_service_->Load();
  EXPECT_TRUE(device_settings_service_->device_settings()
                  ->guest_mode_enabled()
                  .guest_mode_enabled());
}

TEST_F(DeviceSettingsServiceTest, CheckHistogramMismatchDeviceIdEnterprise) {
  StubInstallAttributes* attrs = GetInstallAttributes();
  // The policy builder assigns by default "device-id" value to the device_id
  // in the policy. Here we set a different device_id in the install attributes
  // to check that a mismatch is triggered.
  attrs->SetCloudManaged("example.com", "fake_device_id");
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  owner_key_util_->ImportPrivateKeyAndSetPublicKey(
      device_policy_->GetSigningKey());

  InitOwner(AccountId::FromUserEmail(device_policy_->policy_data().username()),
            true);
  ReloadDeviceSettings();
  device_settings_service_->LoadIfNotPresent();
  histogram_tester_.ExpectBucketCount(
      "Enterprise.DevicePolicyDeviceIdValidity2.OldEnrollmentEnterprise",
      policy::PolicyDeviceIdValidity::kInvalid, /*nrSamples=*/1);
}

TEST_F(DeviceSettingsServiceTest, CheckHistogramGoodDeviceIdEnterprise) {
  StubInstallAttributes* attrs = GetInstallAttributes();
  // The policy builder assigns by default "device-id" value to the device_id
  // in the policy which matches the value we assign here.
  attrs->SetCloudManaged("example.com", "device-id");
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  owner_key_util_->ImportPrivateKeyAndSetPublicKey(
      device_policy_->GetSigningKey());

  InitOwner(AccountId::FromUserEmail(device_policy_->policy_data().username()),
            true);
  ReloadDeviceSettings();
  device_settings_service_->LoadIfNotPresent();
  histogram_tester_.ExpectBucketCount(
      "Enterprise.DevicePolicyDeviceIdValidity2.OldEnrollmentEnterprise",
      policy::PolicyDeviceIdValidity::kValid, /*nrSamples=*/1);
}

TEST_F(DeviceSettingsServiceTest, CheckHistogramMismatchDeviceIdDemoMode) {
  StubInstallAttributes* attrs = GetInstallAttributes();
  attrs->SetDemoMode();
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  owner_key_util_->ImportPrivateKeyAndSetPublicKey(
      device_policy_->GetSigningKey());

  InitOwner(AccountId::FromUserEmail(device_policy_->policy_data().username()),
            true);
  ReloadDeviceSettings();
  device_settings_service_->LoadIfNotPresent();
  // The policy builder assigns by default "device-id" value to the device_id
  // in the policy, while the function SetDemoMode in install attributes set
  // the value for device_id of "demo-device-id". So, we expect a mismatch
  // to be triggered here.
  histogram_tester_.ExpectBucketCount(
      "Enterprise.DevicePolicyDeviceIdValidity2.OldEnrollmentDemo",
      policy::PolicyDeviceIdValidity::kInvalid, /*nrSamples=*/1);
}

TEST_F(DeviceSettingsServiceTest, CheckHistogramGoodDeviceIdDemoMode) {
  // The function SetDemoMode in install attributes set the value
  // "demo-device-id" for device_id, so we update the device_id in the policy
  // blob to match the same value.
  device_policy_->policy_data().set_device_id("demo-device-id");
  ReloadDevicePolicy();

  StubInstallAttributes* attrs = GetInstallAttributes();
  attrs->SetDemoMode();
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  owner_key_util_->ImportPrivateKeyAndSetPublicKey(
      device_policy_->GetSigningKey());

  InitOwner(AccountId::FromUserEmail(device_policy_->policy_data().username()),
            true);
  ReloadDeviceSettings();
  device_settings_service_->LoadIfNotPresent();
  histogram_tester_.ExpectBucketCount(
      "Enterprise.DevicePolicyDeviceIdValidity2.OldEnrollmentDemo",
      policy::PolicyDeviceIdValidity::kValid, /*nrSamples=*/1);
}

}  // namespace ash

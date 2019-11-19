// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_settings_service.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using ::testing::Mock;

namespace chromeos {

namespace {

class MockDeviceSettingsObserver : public DeviceSettingsService::Observer {
 public:
  virtual ~MockDeviceSettingsObserver() {}

  MOCK_METHOD0(OwnershipStatusChanged, void());
  MOCK_METHOD0(DeviceSettingsUpdated, void());
  MOCK_METHOD0(OnDeviceSettingsServiceShutdown, void());
};

}  // namespace

class DeviceSettingsServiceTest : public DeviceSettingsTestBase {
 public:
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
        ownership_status_(DeviceSettingsService::OWNERSHIP_UNKNOWN) {}
  ~DeviceSettingsServiceTest() override = default;

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();
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

  bool operation_completed_;
  bool is_owner_;
  bool is_owner_set_;
  DeviceSettingsService::OwnershipStatus ownership_status_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceSettingsServiceTest);
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
  base::Time timestamp(base::Time::NowFromSystemTime() +
                       base::TimeDelta::FromDays(5000));
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

TEST_F(DeviceSettingsServiceTest, StoreFailure) {
  owner_key_util_->Clear();
  session_manager_client_.set_device_policy(std::string());
  ReloadDeviceSettings();
  EXPECT_EQ(DeviceSettingsService::STORE_KEY_UNAVAILABLE,
            device_settings_service_->status());

  session_manager_client_.ForceStorePolicyFailure(true);
  device_settings_service_->Store(
      device_policy_->GetCopy(),
      base::Bind(&DeviceSettingsServiceTest::SetOperationCompleted,
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
            true);
  device_settings_service_->Store(
      device_policy_->GetCopy(),
      base::Bind(&DeviceSettingsServiceTest::SetOperationCompleted,
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
  device_settings_service_->Store(device_policy_->GetCopy(), base::Closure());
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
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_UNKNOWN,
            device_settings_service_->GetOwnershipStatus());

  device_settings_service_->GetOwnershipStatusAsync(base::Bind(
      &DeviceSettingsServiceTest::SetOwnershipStatus, base::Unretained(this)));
  FlushDeviceSettings();
  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  EXPECT_FALSE(device_settings_service_->GetPublicKey()->is_loaded());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_NONE,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_NONE, ownership_status_);

  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  ReloadDeviceSettings();
  device_settings_service_->GetOwnershipStatusAsync(base::Bind(
      &DeviceSettingsServiceTest::SetOwnershipStatus, base::Unretained(this)));
  FlushDeviceSettings();
  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_TRUE(device_settings_service_->GetPublicKey()->is_loaded());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN, ownership_status_);

  owner_key_util_->SetPrivateKey(device_policy_->GetSigningKey());
  InitOwner(AccountId::FromUserEmail(device_policy_->policy_data().username()),
            true);
  device_settings_service_->GetOwnershipStatusAsync(base::Bind(
      &DeviceSettingsServiceTest::SetOwnershipStatus, base::Unretained(this)));
  FlushDeviceSettings();
  EXPECT_TRUE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_TRUE(device_settings_service_->GetPublicKey()->is_loaded());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN, ownership_status_);
}

TEST_F(DeviceSettingsServiceTest, OnTPMTokenReadyForNonOwner) {
  owner_key_util_->Clear();

  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  EXPECT_FALSE(device_settings_service_->GetPublicKey().get());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_UNKNOWN,
            device_settings_service_->GetOwnershipStatus());

  const std::string& user_id = device_policy_->policy_data().username();
  InitOwner(AccountId::FromUserEmail(user_id), false);
  OwnerSettingsServiceChromeOS* service =
      OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(profile_.get());
  ASSERT_TRUE(service);
  service->IsOwnerAsync(base::Bind(&DeviceSettingsServiceTest::OnIsOwner,
                                   base::Unretained(this)));

  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  ReloadDeviceSettings();

  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_TRUE(device_settings_service_->GetPublicKey()->is_loaded());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_FALSE(is_owner_set_);

  service->OnTPMTokenReady(true /* is ready */);
  FlushDeviceSettings();

  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_TRUE(device_settings_service_->GetPublicKey()->is_loaded());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_TRUE(is_owner_set_);
  EXPECT_FALSE(is_owner_);
}

TEST_F(DeviceSettingsServiceTest, OwnerPrivateKeyInTPMToken) {
  owner_key_util_->Clear();

  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  EXPECT_FALSE(device_settings_service_->GetPublicKey().get());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_UNKNOWN,
            device_settings_service_->GetOwnershipStatus());

  const std::string& user_id = device_policy_->policy_data().username();
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  InitOwner(AccountId::FromUserEmail(user_id), false);
  OwnerSettingsServiceChromeOS* service =
      OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(profile_.get());
  ASSERT_TRUE(service);
  ReloadDeviceSettings();

  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_TRUE(device_settings_service_->GetPublicKey()->is_loaded());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN,
            device_settings_service_->GetOwnershipStatus());

  owner_key_util_->SetPrivateKey(device_policy_->GetSigningKey());
  service->OnTPMTokenReady(true /* is ready */);
  FlushDeviceSettings();

  EXPECT_TRUE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_TRUE(device_settings_service_->GetPublicKey()->is_loaded());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
}

TEST_F(DeviceSettingsServiceTest, OnTPMTokenReadyForOwner) {
  owner_key_util_->Clear();

  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  EXPECT_FALSE(device_settings_service_->GetPublicKey().get());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_UNKNOWN,
            device_settings_service_->GetOwnershipStatus());

  const std::string& user_id = device_policy_->policy_data().username();
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  InitOwner(AccountId::FromUserEmail(user_id), false);
  OwnerSettingsServiceChromeOS* service =
      OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(profile_.get());
  ASSERT_TRUE(service);
  service->IsOwnerAsync(base::Bind(&DeviceSettingsServiceTest::OnIsOwner,
                                   base::Unretained(this)));
  ReloadDeviceSettings();

  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_TRUE(device_settings_service_->GetPublicKey()->is_loaded());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_FALSE(is_owner_set_);

  owner_key_util_->SetPrivateKey(device_policy_->GetSigningKey());
  service->OnTPMTokenReady(true /* is ready */);
  FlushDeviceSettings();

  EXPECT_TRUE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_TRUE(device_settings_service_->GetPublicKey()->is_loaded());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_TRUE(is_owner_set_);
  EXPECT_TRUE(is_owner_);
}

TEST_F(DeviceSettingsServiceTest, IsCurrentUserOwnerAsyncWithLoadedCerts) {
  owner_key_util_->Clear();

  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  EXPECT_FALSE(device_settings_service_->GetPublicKey().get());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_UNKNOWN,
            device_settings_service_->GetOwnershipStatus());

  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  owner_key_util_->SetPrivateKey(device_policy_->GetSigningKey());

  InitOwner(AccountId::FromUserEmail(device_policy_->policy_data().username()),
            true);
  ReloadDeviceSettings();
  FlushDeviceSettings();

  EXPECT_TRUE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_TRUE(device_settings_service_->GetPublicKey()->is_loaded());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_FALSE(is_owner_set_);

  OwnerSettingsServiceChromeOS* service =
      OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(profile_.get());
  ASSERT_TRUE(service);
  service->IsOwnerAsync(base::Bind(&DeviceSettingsServiceTest::OnIsOwner,
                                   base::Unretained(this)));
  // The callback should be called immediately.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_TRUE(device_settings_service_->GetPublicKey()->is_loaded());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN,
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
            true);
  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OwnershipStatusChanged()).Times(0);
  EXPECT_CALL(observer_, DeviceSettingsUpdated()).Times(1);
  device_settings_service_->Store(device_policy_->GetCopy(), base::Closure());
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
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_UNKNOWN,
            device_settings_service_->GetOwnershipStatus());

  // Mark ownership establishment is running.
  device_settings_service_->MarkWillEstablishConsumerOwnership();

  const std::string& user_id = device_policy_->policy_data().username();
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  InitOwner(AccountId::FromUserEmail(user_id), false);
  OwnerSettingsServiceChromeOS* service =
      OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(profile_.get());
  ASSERT_TRUE(service);
  service->IsOwnerAsync(base::Bind(&DeviceSettingsServiceTest::OnIsOwner,
                                   base::Unretained(this)));
  ReloadDeviceSettings();

  // No load operation should happen until OwnerSettingsService loads the
  // private key.
  EXPECT_FALSE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_FALSE(device_settings_service_->GetPublicKey().get());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_UNKNOWN,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_FALSE(is_owner_set_);

  // Load the private key and trigger a reload. Load operations should finish.
  owner_key_util_->SetPrivateKey(device_policy_->GetSigningKey());
  service->OnTPMTokenReady(true /* is ready */);
  FlushDeviceSettings();

  // Verify owner key is loaded and ownership status is updated.
  EXPECT_TRUE(device_settings_service_->HasPrivateOwnerKey());
  ASSERT_TRUE(device_settings_service_->GetPublicKey().get());
  ASSERT_TRUE(device_settings_service_->GetPublicKey()->is_loaded());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            device_settings_service_->GetPublicKey()->as_string());
  EXPECT_EQ(DeviceSettingsService::OWNERSHIP_TAKEN,
            device_settings_service_->GetOwnershipStatus());
  EXPECT_TRUE(is_owner_set_);
  EXPECT_TRUE(is_owner_);
}

}  // namespace chromeos

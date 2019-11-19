// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"

#include <stdint.h>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/dbus/cryptohome/tpm_util.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/test_utils.h"
#include "crypto/rsa_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

void CopyLockResult(base::RunLoop* loop,
                    chromeos::InstallAttributes::LockResult* out,
                    chromeos::InstallAttributes::LockResult result) {
  *out = result;
  loop->Quit();
}

}  // namespace

class DeviceCloudPolicyStoreChromeOSTest
    : public chromeos::DeviceSettingsTestBase {
 protected:
  DeviceCloudPolicyStoreChromeOSTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  ~DeviceCloudPolicyStoreChromeOSTest() override = default;

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();

    install_attributes_ = std::make_unique<chromeos::InstallAttributes>(
        chromeos::FakeCryptohomeClient::Get());
    store_ = std::make_unique<DeviceCloudPolicyStoreChromeOS>(
        device_settings_service_.get(), install_attributes_.get(),
        base::ThreadTaskRunnerHandle::Get());
    store_->AddObserver(&observer_);

    base::RunLoop loop;
    chromeos::InstallAttributes::LockResult result;
    install_attributes_->LockDevice(
        DEVICE_MODE_ENTERPRISE,
        PolicyBuilder::kFakeDomain,
        std::string(),  // realm
        PolicyBuilder::kFakeDeviceId,
        base::Bind(&CopyLockResult, &loop, &result));
    loop.Run();
    ASSERT_EQ(chromeos::InstallAttributes::LOCK_SUCCESS, result);
  }

  void TearDown() override {
    store_->RemoveObserver(&observer_);
    store_.reset();
    install_attributes_.reset();
    DeviceSettingsTestBase::TearDown();
  }

  void ExpectFailure(CloudPolicyStore::Status expected_status) {
    EXPECT_EQ(expected_status, store_->status());
    EXPECT_TRUE(store_->is_initialized());
    EXPECT_FALSE(store_->has_policy());
    EXPECT_FALSE(store_->is_managed());
    EXPECT_EQ(std::string(), store_->policy_signature_public_key());
  }

  void ExpectSuccess() {
    EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
    EXPECT_TRUE(store_->is_initialized());
    EXPECT_TRUE(store_->has_policy());
    EXPECT_TRUE(store_->is_managed());
    EXPECT_TRUE(store_->policy());
    base::Value expected(false);
    EXPECT_EQ(expected, *store_->policy_map().GetValue(
                            key::kDeviceMetricsReportingEnabled));
    EXPECT_FALSE(store_->policy_signature_public_key().empty());
  }

  void PrepareExistingPolicy() {
    store_->Load();
    FlushDeviceSettings();
    ExpectSuccess();

    device_policy_->UnsetNewSigningKey();
    device_policy_->Build();
  }

  void PrepareNewSigningKey() {
    device_policy_->SetDefaultNewSigningKey();
    device_policy_->Build();
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_->GetNewSigningKey());
  }

  void ResetToNonEnterprise() {
    store_->RemoveObserver(&observer_);
    store_.reset();
    chromeos::tpm_util::InstallAttributesSet("enterprise.owned", std::string());
    install_attributes_.reset(
        new chromeos::InstallAttributes(chromeos::FakeCryptohomeClient::Get()));
    store_.reset(new DeviceCloudPolicyStoreChromeOS(
        device_settings_service_.get(), install_attributes_.get(),
        base::ThreadTaskRunnerHandle::Get()));
    store_->AddObserver(&observer_);
  }

  ScopedTestingLocalState local_state_;
  std::unique_ptr<chromeos::InstallAttributes> install_attributes_;

  std::unique_ptr<DeviceCloudPolicyStoreChromeOS> store_;
  MockCloudPolicyStoreObserver observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceCloudPolicyStoreChromeOSTest);
};

TEST_F(DeviceCloudPolicyStoreChromeOSTest, LoadNoKey) {
  owner_key_util_->Clear();
  store_->Load();
  FlushDeviceSettings();
  ExpectFailure(CloudPolicyStore::STATUS_BAD_STATE);
}

TEST_F(DeviceCloudPolicyStoreChromeOSTest, LoadNoPolicy) {
  session_manager_client_.set_device_policy(std::string());
  store_->Load();
  FlushDeviceSettings();
  ExpectFailure(CloudPolicyStore::STATUS_LOAD_ERROR);
}

TEST_F(DeviceCloudPolicyStoreChromeOSTest, LoadNotEnterprise) {
  ResetToNonEnterprise();
  store_->Load();
  FlushDeviceSettings();
  ExpectFailure(CloudPolicyStore::STATUS_BAD_STATE);
}

TEST_F(DeviceCloudPolicyStoreChromeOSTest, LoadSuccess) {
  store_->Load();
  FlushDeviceSettings();
  ExpectSuccess();
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreChromeOSTest, StoreSuccess) {
  PrepareExistingPolicy();
  store_->Store(device_policy_->policy());
  FlushDeviceSettings();
  ExpectSuccess();
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreChromeOSTest, StoreNoSignature) {
  PrepareExistingPolicy();
  device_policy_->policy().clear_policy_data_signature();
  store_->Store(device_policy_->policy());
  FlushDeviceSettings();
  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE,
            store_->validation_status());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreChromeOSTest, StoreBadSignature) {
  PrepareExistingPolicy();
  device_policy_->policy().set_policy_data_signature("invalid");
  store_->Store(device_policy_->policy());
  FlushDeviceSettings();
  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE,
            store_->validation_status());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreChromeOSTest, StoreKeyRotation) {
  PrepareExistingPolicy();
  device_policy_->SetDefaultNewSigningKey();
  device_policy_->Build();
  store_->Store(device_policy_->policy());
  content::RunAllTasksUntilIdle();
  owner_key_util_->SetPublicKeyFromPrivateKey(
      *device_policy_->GetNewSigningKey());
  ReloadDeviceSettings();
  ExpectSuccess();
  EXPECT_EQ(device_policy_->GetPublicNewSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreChromeOSTest,
       StoreKeyRotationVerificationFailure) {
  PrepareExistingPolicy();
  device_policy_->SetDefaultNewSigningKey();
  device_policy_->Build();
  *device_policy_->policy()
       .mutable_new_public_key_verification_signature_deprecated() = "garbage";
  store_->Store(device_policy_->policy());
  FlushDeviceSettings();
  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE,
            store_->validation_status());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreChromeOSTest,
       StoreKeyRotationMissingSignatureFailure) {
  PrepareExistingPolicy();
  device_policy_->SetDefaultNewSigningKey();
  device_policy_->Build();
  device_policy_->policy()
      .clear_new_public_key_verification_signature_deprecated();
  store_->Store(device_policy_->policy());
  FlushDeviceSettings();
  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE,
            store_->validation_status());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreChromeOSTest, StoreValueValidationError) {
  PrepareExistingPolicy();

  std::string onc_policy = chromeos::onc::test_utils::ReadTestData(
      "toplevel_with_unknown_fields.onc");
  device_policy_->payload()
      .mutable_open_network_configuration()
      ->set_open_network_configuration(onc_policy);
  device_policy_->Build();

  EXPECT_CALL(observer_, OnStoreLoaded(store_.get()));

  store_->Store(device_policy_->policy());
  FlushDeviceSettings();
  const CloudPolicyValidatorBase::ValidationResult* validation_result =
      store_->validation_result();
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_OK, validation_result->status);
  EXPECT_EQ(3u, validation_result->value_validation_issues.size());
  EXPECT_EQ(device_policy_->policy_data().policy_token(),
            validation_result->policy_token);
  EXPECT_EQ(device_policy_->policy().policy_data_signature(),
            validation_result->policy_data_signature);
}

TEST_F(DeviceCloudPolicyStoreChromeOSTest, InstallInitialPolicySuccess) {
  PrepareNewSigningKey();
  store_->InstallInitialPolicy(device_policy_->policy());
  FlushDeviceSettings();
  ExpectSuccess();
  EXPECT_EQ(device_policy_->GetPublicNewSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreChromeOSTest, InstallInitialPolicyNoSignature) {
  PrepareNewSigningKey();
  device_policy_->policy().clear_policy_data_signature();
  store_->InstallInitialPolicy(device_policy_->policy());
  FlushDeviceSettings();
  ExpectFailure(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_INITIAL_SIGNATURE,
            store_->validation_status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreChromeOSTest,
       InstallInitialPolicyVerificationFailure) {
  PrepareNewSigningKey();
  *device_policy_->policy()
       .mutable_new_public_key_verification_signature_deprecated() = "garbage";
  store_->InstallInitialPolicy(device_policy_->policy());
  FlushDeviceSettings();
  ExpectFailure(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE,
            store_->validation_status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreChromeOSTest,
       InstallInitialPolicyMissingSignatureFailure) {
  PrepareNewSigningKey();
  device_policy_->policy()
      .clear_new_public_key_verification_signature_deprecated();
  store_->InstallInitialPolicy(device_policy_->policy());
  FlushDeviceSettings();
  ExpectFailure(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE,
            store_->validation_status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreChromeOSTest, InstallInitialPolicyNoKey) {
  PrepareNewSigningKey();
  device_policy_->policy().clear_new_public_key();
  store_->InstallInitialPolicy(device_policy_->policy());
  FlushDeviceSettings();
  ExpectFailure(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_INITIAL_SIGNATURE,
            store_->validation_status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreChromeOSTest, InstallInitialPolicyNotEnterprise) {
  PrepareNewSigningKey();
  ResetToNonEnterprise();
  store_->InstallInitialPolicy(device_policy_->policy());
  FlushDeviceSettings();
  ExpectFailure(CloudPolicyStore::STATUS_BAD_STATE);
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

}  // namespace policy

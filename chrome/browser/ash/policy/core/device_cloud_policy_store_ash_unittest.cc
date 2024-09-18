// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/device_management/fake_install_attributes_client.h"
#include "chromeos/ash/components/dbus/device_management/install_attributes_util.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/components/onc/onc_test_utils.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/policy_switches.h"
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
                    ash::InstallAttributes::LockResult* out,
                    ash::InstallAttributes::LockResult result) {
  *out = result;
  loop->Quit();
}

}  // namespace

class DeviceCloudPolicyStoreAshTest : public ash::DeviceSettingsTestBase {
 public:
  DeviceCloudPolicyStoreAshTest(const DeviceCloudPolicyStoreAshTest&) = delete;
  DeviceCloudPolicyStoreAshTest& operator=(
      const DeviceCloudPolicyStoreAshTest&) = delete;

 protected:
  DeviceCloudPolicyStoreAshTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  ~DeviceCloudPolicyStoreAshTest() override = default;

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();

    // This will change the verification key to be used by the
    // CloudPolicyValidator. It will allow for the policy provided by the
    // PolicyBuilder to pass the signature validation.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(
        switches::kPolicyVerificationKey,
        PolicyBuilder::GetEncodedPolicyVerificationKey());

    ash::InstallAttributesClient::InitializeFake();
    install_attributes_ = std::make_unique<ash::InstallAttributes>(
        ash::InstallAttributesClient::Get());
    store_ = std::make_unique<DeviceCloudPolicyStoreAsh>(
        device_settings_service_.get(), install_attributes_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    store_->AddObserver(&observer_);

    base::RunLoop loop;
    ash::InstallAttributes::LockResult result;
    install_attributes_->LockDevice(
        DEVICE_MODE_ENTERPRISE, PolicyBuilder::kFakeDomain,
        std::string(),  // realm
        PolicyBuilder::kFakeDeviceId,
        base::BindOnce(&CopyLockResult, &loop, &result));
    loop.Run();
    ASSERT_EQ(ash::InstallAttributes::LOCK_SUCCESS, result);
  }

  void TearDown() override {
    store_->RemoveObserver(&observer_);
    store_.reset();
    install_attributes_.reset();
    ash::InstallAttributesClient::Shutdown();
    DeviceSettingsTestBase::TearDown();
  }

  void ExpectFailure(CloudPolicyStore::Status expected_status) {
    EXPECT_EQ(expected_status, store_->status());
    EXPECT_TRUE(store_->is_initialized());
    EXPECT_EQ(!install_attributes_->IsEnterpriseManaged(),
              store_->first_policies_loaded());
    EXPECT_FALSE(store_->has_policy());
    EXPECT_FALSE(store_->is_managed());
    EXPECT_EQ(std::string(), store_->policy_signature_public_key());
  }

  void ExpectSuccess() {
    EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
    EXPECT_TRUE(store_->is_initialized());
    EXPECT_TRUE(store_->first_policies_loaded());
    EXPECT_TRUE(store_->has_policy());
    EXPECT_TRUE(store_->is_managed());
    EXPECT_TRUE(store_->policy());
    base::Value expected(false);
    EXPECT_EQ(expected, *store_->policy_map().GetValue(
                            key::kDeviceMetricsReportingEnabled,
                            base::Value::Type::BOOLEAN));
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
    ash::install_attributes_util::InstallAttributesSet("enterprise.owned",
                                                       std::string());
    install_attributes_ = std::make_unique<ash::InstallAttributes>(
        ash::FakeInstallAttributesClient::Get());
    store_ = std::make_unique<DeviceCloudPolicyStoreAsh>(
        device_settings_service_.get(), install_attributes_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    store_->AddObserver(&observer_);
  }

  ScopedTestingLocalState local_state_;
  std::unique_ptr<ash::InstallAttributes> install_attributes_;

  std::unique_ptr<DeviceCloudPolicyStoreAsh> store_;
  MockCloudPolicyStoreObserver observer_;
};

TEST_F(DeviceCloudPolicyStoreAshTest, LoadNoKey) {
  owner_key_util_->Clear();
  store_->Load();
  FlushDeviceSettings();
  ExpectFailure(CloudPolicyStore::STATUS_BAD_STATE);
}

TEST_F(DeviceCloudPolicyStoreAshTest, LoadNoPolicy) {
  session_manager_client_.set_device_policy(std::string());
  store_->Load();
  FlushDeviceSettings();
  ExpectFailure(CloudPolicyStore::STATUS_LOAD_ERROR);
}

TEST_F(DeviceCloudPolicyStoreAshTest, LoadNotEnterprise) {
  ResetToNonEnterprise();
  store_->Load();
  FlushDeviceSettings();
  ExpectFailure(CloudPolicyStore::STATUS_BAD_STATE);
}

TEST_F(DeviceCloudPolicyStoreAshTest, LoadSuccess) {
  store_->Load();
  FlushDeviceSettings();
  ExpectSuccess();
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreAshTest, StoreSuccess) {
  PrepareExistingPolicy();
  store_->Store(device_policy_->policy());
  FlushDeviceSettings();
  ExpectSuccess();
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreAshTest, StoreNoSignature) {
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

TEST_F(DeviceCloudPolicyStoreAshTest, StoreBadSignature) {
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

TEST_F(DeviceCloudPolicyStoreAshTest, StoreKeyRotation) {
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

TEST_F(DeviceCloudPolicyStoreAshTest, StoreKeyRotationVerificationFailure) {
  PrepareExistingPolicy();
  device_policy_->SetDefaultNewSigningKey();
  device_policy_->Build();
  *device_policy_->policy()
       .mutable_new_public_key_verification_signature_deprecated() = "garbage";
  *device_policy_->policy()
       .mutable_new_public_key_verification_data_signature() = "garbage";
  store_->Store(device_policy_->policy());
  FlushDeviceSettings();
  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE,
            store_->validation_status());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreAshTest, StoreKeyRotationMissingSignatureFailure) {
  PrepareExistingPolicy();
  device_policy_->SetDefaultNewSigningKey();
  device_policy_->Build();
  device_policy_->policy()
      .clear_new_public_key_verification_signature_deprecated();
  device_policy_->policy().clear_new_public_key_verification_data_signature();
  store_->Store(device_policy_->policy());
  FlushDeviceSettings();
  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE,
            store_->validation_status());
  EXPECT_EQ(device_policy_->GetPublicSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreAshTest, StoreValueValidationError) {
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

TEST_F(DeviceCloudPolicyStoreAshTest, StorePolicyBadDomain) {
  PrepareExistingPolicy();

  device_policy_->policy_data().mutable_username()->assign(
      "user@bad_domain.com");
  device_policy_->Build();

  store_->Store(device_policy_->policy());
  FlushDeviceSettings();
  const CloudPolicyValidatorBase::ValidationResult* validation_result =
      store_->validation_result();
  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_USER,
            validation_result->status);
}

TEST_F(DeviceCloudPolicyStoreAshTest, StoreDeviceIdValidationEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kDeviceIdValidation);
  local_state_.Get()->SetManagedPref(prefs::kEnrollmentVersionOS,
                                     base::Value("128"));
  PrepareExistingPolicy();

  // Set the device_id created by the policy generator. Expected to be valid.
  device_policy_->policy_data().mutable_device_id()->assign(
      PolicyBuilder::kFakeDeviceId);
  device_policy_->Build();

  store_->Store(device_policy_->policy());
  FlushDeviceSettings();
  const CloudPolicyValidatorBase::ValidationResult* validation_result =
      store_->validation_result();
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, store_->status());
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_OK, validation_result->status);
}

TEST_F(DeviceCloudPolicyStoreAshTest, StoreDeviceIdValidationEnabledError) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kDeviceIdValidation);
  local_state_.Get()->SetManagedPref(prefs::kEnrollmentVersionOS,
                                     base::Value("128"));
  PrepareExistingPolicy();

  device_policy_->policy_data().mutable_device_id()->assign("bad-device-id");
  device_policy_->Build();

  store_->Store(device_policy_->policy());
  FlushDeviceSettings();
  const CloudPolicyValidatorBase::ValidationResult* validation_result =
      store_->validation_result();
  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR, store_->status());
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_DEVICE_ID,
            validation_result->status);
}

TEST_F(DeviceCloudPolicyStoreAshTest, InstallInitialPolicySuccess) {
  PrepareNewSigningKey();
  store_->InstallInitialPolicy(device_policy_->policy());
  FlushDeviceSettings();
  ExpectSuccess();
  EXPECT_EQ(device_policy_->GetPublicNewSigningKeyAsString(),
            store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreAshTest, InstallInitialPolicyNoSignature) {
  PrepareNewSigningKey();
  device_policy_->policy().clear_policy_data_signature();
  store_->InstallInitialPolicy(device_policy_->policy());
  FlushDeviceSettings();
  ExpectFailure(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_INITIAL_SIGNATURE,
            store_->validation_status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreAshTest, InstallInitialPolicyVerificationFailure) {
  PrepareNewSigningKey();
  *device_policy_->policy()
       .mutable_new_public_key_verification_signature_deprecated() = "garbage";
  *device_policy_->policy()
       .mutable_new_public_key_verification_data_signature() = "garbage";
  store_->InstallInitialPolicy(device_policy_->policy());
  FlushDeviceSettings();
  ExpectFailure(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE,
            store_->validation_status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreAshTest,
       InstallInitialPolicyMissingSignatureFailure) {
  PrepareNewSigningKey();
  device_policy_->policy()
      .clear_new_public_key_verification_signature_deprecated();
  device_policy_->policy().clear_new_public_key_verification_data_signature();
  store_->InstallInitialPolicy(device_policy_->policy());
  FlushDeviceSettings();
  ExpectFailure(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_KEY_VERIFICATION_SIGNATURE,
            store_->validation_status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreAshTest, InstallInitialPolicyNoKey) {
  PrepareNewSigningKey();
  device_policy_->policy().clear_new_public_key();
  store_->InstallInitialPolicy(device_policy_->policy());
  FlushDeviceSettings();
  ExpectFailure(CloudPolicyStore::STATUS_VALIDATION_ERROR);
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_BAD_INITIAL_SIGNATURE,
            store_->validation_status());
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreAshTest, InstallInitialPolicyNotEnterprise) {
  PrepareNewSigningKey();
  ResetToNonEnterprise();
  store_->InstallInitialPolicy(device_policy_->policy());
  FlushDeviceSettings();
  ExpectFailure(CloudPolicyStore::STATUS_BAD_STATE);
  EXPECT_EQ(std::string(), store_->policy_signature_public_key());
}

TEST_F(DeviceCloudPolicyStoreAshTest, InstallInitialPolicyBadDomain) {
  PrepareNewSigningKey();
  device_policy_->policy_data().set_username("bad_owner@bad_domain.com");
  device_policy_->Build();
  store_->InstallInitialPolicy(device_policy_->policy());
  FlushDeviceSettings();
  ExpectFailure(CloudPolicyStore::STATUS_VALIDATION_ERROR);
}

TEST_F(DeviceCloudPolicyStoreAshTest, InstallInitialPolicyBadDeviceId) {
  PrepareNewSigningKey();
  device_policy_->policy_data().set_device_id("bad_device_id");
  device_policy_->Build();
  store_->InstallInitialPolicy(device_policy_->policy());
  FlushDeviceSettings();
  ExpectFailure(CloudPolicyStore::STATUS_VALIDATION_ERROR);
}

TEST_F(DeviceCloudPolicyStoreAshTest, StoreDeviceBlockDevmodeAllowed) {
  PrepareExistingPolicy();
  device_policy_->payload().mutable_system_settings()->set_block_devmode(true);
  store_->Store(device_policy_->policy());
  FlushDeviceSettings();
  ExpectSuccess();
}

TEST_F(DeviceCloudPolicyStoreAshTest, StoreDeviceBlockDevmodeDisallowed) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kDisallowPolicyBlockDevMode);
  PrepareExistingPolicy();
  device_policy_->payload().mutable_system_settings()->set_block_devmode(true);
  device_policy_->Build();
  store_->Store(device_policy_->policy());
  FlushDeviceSettings();
  EXPECT_EQ(store_->status(), CloudPolicyStore::STATUS_BAD_STATE);
}

}  // namespace policy

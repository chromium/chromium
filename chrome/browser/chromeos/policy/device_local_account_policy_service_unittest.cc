// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_provider.h"
#include "chrome/browser/chromeos/policy/fake_affiliated_invalidation_service_provider.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AnyNumber;
using testing::AtLeast;
using testing::Mock;
using testing::SaveArg;
using testing::_;

namespace em = enterprise_management;

namespace policy {

namespace {

const char kAccount1[] = "account1@localhost";
const char kAccount2[] = "account2@localhost";
const char kAccount3[] = "account3@localhost";

const char kExtensionID[] = "kbmnembihfiondgfjekmnmcbddelicoi";
const char kExtensionVersion[] = "1.0.0.0";
const char kExtensionCRXPath[] = "extensions/hosted_app.crx";
const char kUpdateURL[] = "https://clients2.google.com/service/update2/crx";

}  // namespace

class MockDeviceLocalAccountPolicyServiceObserver
    : public DeviceLocalAccountPolicyService::Observer {
 public:
  MOCK_METHOD1(OnPolicyUpdated, void(const std::string&));
  MOCK_METHOD0(OnDeviceLocalAccountsChanged, void(void));
};

class DeviceLocalAccountPolicyServiceTestBase
    : public chromeos::DeviceSettingsTestBase {
 public:
  DeviceLocalAccountPolicyServiceTestBase();
  ~DeviceLocalAccountPolicyServiceTestBase() override;

  void CreatePolicyService();

  void InstallDeviceLocalAccountPolicy(const std::string& account_id);
  void AddDeviceLocalAccountToPolicy(const std::string& account_id);
  virtual void InstallDevicePolicy();

  const std::string account_1_user_id_;
  const std::string account_2_user_id_;

  PolicyMap expected_policy_map_;
  UserPolicyBuilder device_local_account_policy_;
  std::unique_ptr<chromeos::CrosSettings> cros_settings_;
  scoped_refptr<base::TestSimpleTaskRunner> extension_cache_task_runner_;
  MockDeviceManagementService mock_device_management_service_;
  FakeAffiliatedInvalidationServiceProvider
      affiliated_invalidation_service_provider_;
  std::unique_ptr<DeviceLocalAccountPolicyService> service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyServiceTestBase);
};

class DeviceLocalAccountPolicyServiceTest
    : public DeviceLocalAccountPolicyServiceTestBase {
 public:
  MOCK_METHOD1(OnRefreshDone, void(bool));

 protected:
  DeviceLocalAccountPolicyServiceTest();
  ~DeviceLocalAccountPolicyServiceTest();

  void InstallDevicePolicy() override;

  MockDeviceLocalAccountPolicyServiceObserver service_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyServiceTest);
};

DeviceLocalAccountPolicyServiceTestBase::
    DeviceLocalAccountPolicyServiceTestBase()
    : account_1_user_id_(GenerateDeviceLocalAccountUserId(
          kAccount1,
          DeviceLocalAccount::TYPE_PUBLIC_SESSION)),
      account_2_user_id_(GenerateDeviceLocalAccountUserId(
          kAccount2,
          DeviceLocalAccount::TYPE_PUBLIC_SESSION)),
      cros_settings_(std::make_unique<chromeos::CrosSettings>(
          &device_settings_service_,
          TestingBrowserProcess::GetGlobal()->local_state())),
      extension_cache_task_runner_(new base::TestSimpleTaskRunner) {
  expected_policy_map_.Set(key::kSearchSuggestEnabled, POLICY_LEVEL_MANDATORY,
                           POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                           std::make_unique<base::Value>(true), nullptr);

  device_local_account_policy_.payload()
      .mutable_searchsuggestenabled()
      ->set_value(true);
  device_local_account_policy_.policy_data().set_policy_type(
      dm_protocol::kChromePublicAccountPolicyType);
}

DeviceLocalAccountPolicyServiceTestBase::
    ~DeviceLocalAccountPolicyServiceTestBase() {
  service_->Shutdown();
  service_.reset();
  extension_cache_task_runner_->RunUntilIdle();
  chromeos::DeviceSettingsTestBase::TearDown();
}

void DeviceLocalAccountPolicyServiceTestBase::CreatePolicyService() {
  service_.reset(new DeviceLocalAccountPolicyService(
      &session_manager_client_, &device_settings_service_, cros_settings_.get(),
      &affiliated_invalidation_service_provider_,
      base::ThreadTaskRunnerHandle::Get(), extension_cache_task_runner_,
      base::ThreadTaskRunnerHandle::Get(),
      /*url_loader_factory=*/nullptr));
}

void DeviceLocalAccountPolicyServiceTestBase::
    InstallDeviceLocalAccountPolicy(const std::string& account_id) {
  device_local_account_policy_.policy_data().set_settings_entity_id(account_id);
  device_local_account_policy_.policy_data().set_username(account_id);
  device_local_account_policy_.Build();
  session_manager_client_.set_device_local_account_policy(
      account_id, device_local_account_policy_.GetBlob());
}

void DeviceLocalAccountPolicyServiceTestBase::AddDeviceLocalAccountToPolicy(
    const std::string& account_id) {
  em::DeviceLocalAccountInfoProto* account =
      device_policy_.payload().mutable_device_local_accounts()->add_account();
  account->set_account_id(account_id);
  account->set_type(
      em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);
}

void DeviceLocalAccountPolicyServiceTestBase::InstallDevicePolicy() {
  device_policy_.Build();
  session_manager_client_.set_device_policy(device_policy_.GetBlob());
  ReloadDeviceSettings();
}

DeviceLocalAccountPolicyServiceTest::DeviceLocalAccountPolicyServiceTest() {
  CreatePolicyService();
  service_->AddObserver(&service_observer_);
}

DeviceLocalAccountPolicyServiceTest::~DeviceLocalAccountPolicyServiceTest() {
  service_->RemoveObserver(&service_observer_);
}

void DeviceLocalAccountPolicyServiceTest::InstallDevicePolicy() {
  EXPECT_CALL(service_observer_, OnDeviceLocalAccountsChanged());
  DeviceLocalAccountPolicyServiceTestBase::InstallDevicePolicy();
  Mock::VerifyAndClearExpectations(&service_observer_);
}

TEST_F(DeviceLocalAccountPolicyServiceTest, NoAccounts) {
  EXPECT_FALSE(service_->GetBrokerForUser(account_1_user_id_));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, GetBroker) {
  InstallDeviceLocalAccountPolicy(kAccount1);
  AddDeviceLocalAccountToPolicy(kAccount1);
  EXPECT_CALL(service_observer_, OnPolicyUpdated(account_1_user_id_));
  InstallDevicePolicy();

  DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForUser(account_1_user_id_);
  ASSERT_TRUE(broker);
  EXPECT_EQ(account_1_user_id_, broker->user_id());
  ASSERT_TRUE(broker->core()->store());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, broker->core()->store()->status());
  EXPECT_FALSE(broker->core()->client());
  EXPECT_FALSE(broker->core()->store()->policy_map().empty());
  EXPECT_FALSE(broker->HasInvalidatorForTest());
}

TEST_F(DeviceLocalAccountPolicyServiceTest, LoadNoPolicy) {
  AddDeviceLocalAccountToPolicy(kAccount1);
  EXPECT_CALL(service_observer_, OnPolicyUpdated(account_1_user_id_));
  InstallDevicePolicy();

  DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForUser(account_1_user_id_);
  ASSERT_TRUE(broker);
  EXPECT_EQ(account_1_user_id_, broker->user_id());
  ASSERT_TRUE(broker->core()->store());
  EXPECT_EQ(CloudPolicyStore::STATUS_LOAD_ERROR,
            broker->core()->store()->status());
  EXPECT_TRUE(broker->core()->store()->policy_map().empty());
  EXPECT_FALSE(broker->HasInvalidatorForTest());
  EXPECT_FALSE(service_->IsPolicyAvailableForUser(account_1_user_id_));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, LoadValidationFailure) {
  device_local_account_policy_.policy_data().set_policy_type(
      dm_protocol::kChromeUserPolicyType);
  InstallDeviceLocalAccountPolicy(kAccount1);
  AddDeviceLocalAccountToPolicy(kAccount1);
  EXPECT_CALL(service_observer_, OnPolicyUpdated(account_1_user_id_));
  InstallDevicePolicy();

  DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForUser(account_1_user_id_);
  ASSERT_TRUE(broker);
  EXPECT_EQ(account_1_user_id_, broker->user_id());
  ASSERT_TRUE(broker->core()->store());
  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR,
            broker->core()->store()->status());
  EXPECT_TRUE(broker->core()->store()->policy_map().empty());
  EXPECT_FALSE(broker->HasInvalidatorForTest());
  EXPECT_FALSE(service_->IsPolicyAvailableForUser(account_1_user_id_));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, LoadPolicy) {
  InstallDeviceLocalAccountPolicy(kAccount1);
  AddDeviceLocalAccountToPolicy(kAccount1);
  EXPECT_CALL(service_observer_, OnPolicyUpdated(account_1_user_id_));
  InstallDevicePolicy();

  DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForUser(account_1_user_id_);
  ASSERT_TRUE(broker);
  EXPECT_EQ(account_1_user_id_, broker->user_id());
  ASSERT_TRUE(broker->core()->store());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, broker->core()->store()->status());
  ASSERT_TRUE(broker->core()->store()->policy());
  EXPECT_EQ(device_local_account_policy_.policy_data().SerializeAsString(),
            broker->core()->store()->policy()->SerializeAsString());
  EXPECT_TRUE(expected_policy_map_.Equals(
      broker->core()->store()->policy_map()));
  EXPECT_FALSE(broker->HasInvalidatorForTest());
  EXPECT_TRUE(service_->IsPolicyAvailableForUser(account_1_user_id_));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, StoreValidationFailure) {
  AddDeviceLocalAccountToPolicy(kAccount1);
  EXPECT_CALL(service_observer_, OnPolicyUpdated(account_1_user_id_));
  InstallDevicePolicy();
  Mock::VerifyAndClearExpectations(&service_observer_);

  DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForUser(account_1_user_id_);
  ASSERT_TRUE(broker);
  EXPECT_EQ(account_1_user_id_, broker->user_id());
  ASSERT_TRUE(broker->core()->store());

  device_local_account_policy_.policy_data().set_policy_type(
      dm_protocol::kChromeUserPolicyType);
  device_local_account_policy_.Build();
  broker->core()->store()->Store(device_local_account_policy_.policy());
  EXPECT_CALL(service_observer_, OnPolicyUpdated(account_1_user_id_));
  FlushDeviceSettings();

  EXPECT_EQ(CloudPolicyStore::STATUS_VALIDATION_ERROR,
            broker->core()->store()->status());
  EXPECT_EQ(CloudPolicyValidatorBase::VALIDATION_WRONG_POLICY_TYPE,
            broker->core()->store()->validation_status());
  EXPECT_FALSE(broker->HasInvalidatorForTest());
  EXPECT_FALSE(service_->IsPolicyAvailableForUser(account_1_user_id_));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, StorePolicy) {
  AddDeviceLocalAccountToPolicy(kAccount1);
  EXPECT_CALL(service_observer_, OnPolicyUpdated(account_1_user_id_));
  InstallDevicePolicy();
  Mock::VerifyAndClearExpectations(&service_observer_);

  DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForUser(account_1_user_id_);
  ASSERT_TRUE(broker);
  EXPECT_EQ(account_1_user_id_, broker->user_id());
  ASSERT_TRUE(broker->core()->store());

  device_local_account_policy_.policy_data().set_settings_entity_id(kAccount1);
  device_local_account_policy_.policy_data().set_username(kAccount1);
  device_local_account_policy_.Build();
  broker->core()->store()->Store(device_local_account_policy_.policy());
  EXPECT_CALL(service_observer_, OnPolicyUpdated(account_1_user_id_));
  FlushDeviceSettings();

  EXPECT_EQ(CloudPolicyStore::STATUS_OK, broker->core()->store()->status());
  ASSERT_TRUE(broker->core()->store()->policy());
  EXPECT_EQ(device_local_account_policy_.policy_data().SerializeAsString(),
            broker->core()->store()->policy()->SerializeAsString());
  EXPECT_TRUE(expected_policy_map_.Equals(
      broker->core()->store()->policy_map()));
  EXPECT_FALSE(broker->HasInvalidatorForTest());
  EXPECT_TRUE(service_->IsPolicyAvailableForUser(account_1_user_id_));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, DevicePolicyChange) {
  InstallDeviceLocalAccountPolicy(kAccount1);
  AddDeviceLocalAccountToPolicy(kAccount1);
  EXPECT_CALL(service_observer_, OnPolicyUpdated(account_1_user_id_));
  InstallDevicePolicy();

  device_policy_.payload().mutable_device_local_accounts()->clear_account();
  InstallDevicePolicy();

  EXPECT_FALSE(service_->GetBrokerForUser(account_1_user_id_));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, DuplicateAccounts) {
  InstallDeviceLocalAccountPolicy(kAccount1);
  AddDeviceLocalAccountToPolicy(kAccount1);
  EXPECT_CALL(service_observer_, OnPolicyUpdated(account_1_user_id_));
  InstallDevicePolicy();
  Mock::VerifyAndClearExpectations(&service_observer_);

  // Add a second entry with a duplicate account name to device policy.
  AddDeviceLocalAccountToPolicy(kAccount1);
  InstallDevicePolicy();

  // Make sure the broker is accessible and policy got loaded.
  DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForUser(account_1_user_id_);
  ASSERT_TRUE(broker);
  EXPECT_EQ(account_1_user_id_, broker->user_id());
  ASSERT_TRUE(broker->core()->store());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK, broker->core()->store()->status());
  ASSERT_TRUE(broker->core()->store()->policy());
  EXPECT_EQ(device_local_account_policy_.policy_data().SerializeAsString(),
            broker->core()->store()->policy()->SerializeAsString());
  EXPECT_TRUE(expected_policy_map_.Equals(
      broker->core()->store()->policy_map()));
  EXPECT_FALSE(broker->HasInvalidatorForTest());
  EXPECT_TRUE(service_->IsPolicyAvailableForUser(account_1_user_id_));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, FetchPolicy) {
  InstallDeviceLocalAccountPolicy(kAccount1);
  AddDeviceLocalAccountToPolicy(kAccount1);
  EXPECT_CALL(service_observer_, OnPolicyUpdated(account_1_user_id_));
  InstallDevicePolicy();

  DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForUser(account_1_user_id_);
  ASSERT_TRUE(broker);

  service_->Connect(&mock_device_management_service_);
  EXPECT_TRUE(broker->core()->client());

  em::DeviceManagementRequest request;
  em::DeviceManagementResponse response;
  response.mutable_policy_response()->add_response()->CopyFrom(
      device_local_account_policy_.policy());
  EXPECT_CALL(mock_device_management_service_,
              CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH, _))
      .WillOnce(mock_device_management_service_.SucceedJob(response));
  EXPECT_CALL(
      mock_device_management_service_,
      StartJob(dm_protocol::kValueRequestPolicy, std::string(), std::string(),
               device_policy_.policy_data().request_token(), std::string(),
               device_policy_.policy_data().device_id(), _))
      .WillOnce(SaveArg<6>(&request));
  // This will be called twice, because the ComponentCloudPolicyService will
  // also become ready after flushing all the pending tasks.
  EXPECT_CALL(service_observer_, OnPolicyUpdated(account_1_user_id_)).Times(2);
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&service_observer_);
  Mock::VerifyAndClearExpectations(&mock_device_management_service_);
  EXPECT_TRUE(request.has_policy_request());
  ASSERT_EQ(2, request.policy_request().request_size());

  const em::PolicyFetchRequest* public_account =
      &request.policy_request().request(0);
  const em::PolicyFetchRequest* extensions =
      &request.policy_request().request(1);
  // The order is not guarateed.
  if (extensions->policy_type() ==
      dm_protocol::kChromePublicAccountPolicyType) {
    std::swap(public_account, extensions);
  }

  EXPECT_EQ(dm_protocol::kChromePublicAccountPolicyType,
            public_account->policy_type());
  EXPECT_EQ(kAccount1, public_account->settings_entity_id());

  EXPECT_EQ(dm_protocol::kChromeExtensionPolicyType,
            extensions->policy_type());
  EXPECT_FALSE(extensions->has_settings_entity_id());

  ASSERT_TRUE(broker->core()->store());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK,
            broker->core()->store()->status());
  ASSERT_TRUE(broker->core()->store()->policy());
  EXPECT_EQ(device_local_account_policy_.policy_data().SerializeAsString(),
            broker->core()->store()->policy()->SerializeAsString());
  EXPECT_TRUE(expected_policy_map_.Equals(
      broker->core()->store()->policy_map()));
  EXPECT_TRUE(broker->HasInvalidatorForTest());
  EXPECT_TRUE(service_->IsPolicyAvailableForUser(account_1_user_id_));
}

TEST_F(DeviceLocalAccountPolicyServiceTest, RefreshPolicy) {
  InstallDeviceLocalAccountPolicy(kAccount1);
  AddDeviceLocalAccountToPolicy(kAccount1);
  EXPECT_CALL(service_observer_, OnPolicyUpdated(account_1_user_id_));
  InstallDevicePolicy();

  DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForUser(account_1_user_id_);
  ASSERT_TRUE(broker);

  service_->Connect(&mock_device_management_service_);
  ASSERT_TRUE(broker->core()->service());

  em::DeviceManagementResponse response;
  response.mutable_policy_response()->add_response()->CopyFrom(
      device_local_account_policy_.policy());
  EXPECT_CALL(mock_device_management_service_, CreateJob(_, _))
      .WillOnce(mock_device_management_service_.SucceedJob(response));
  EXPECT_CALL(mock_device_management_service_, StartJob(_, _, _, _, _, _, _));
  EXPECT_CALL(*this, OnRefreshDone(true)).Times(1);
  // This will be called twice, because the ComponentCloudPolicyService will
  // also become ready after flushing all the pending tasks.
  EXPECT_CALL(service_observer_, OnPolicyUpdated(account_1_user_id_)).Times(2);
  broker->core()->service()->RefreshPolicy(
      base::Bind(&DeviceLocalAccountPolicyServiceTest::OnRefreshDone,
                 base::Unretained(this)));
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&service_observer_);
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(&mock_device_management_service_);

  ASSERT_TRUE(broker->core()->store());
  EXPECT_EQ(CloudPolicyStore::STATUS_OK,
            broker->core()->store()->status());
  EXPECT_TRUE(expected_policy_map_.Equals(
      broker->core()->store()->policy_map()));
  EXPECT_TRUE(broker->HasInvalidatorForTest());
  EXPECT_TRUE(service_->IsPolicyAvailableForUser(account_1_user_id_));
}

class DeviceLocalAccountPolicyExtensionCacheTest
    : public DeviceLocalAccountPolicyServiceTestBase {
 protected:
  DeviceLocalAccountPolicyExtensionCacheTest();

  void SetUp() override;

  base::FilePath GetCacheDirectoryForAccountID(const std::string& account_id);

  base::ScopedTempDir cache_root_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_root_dir_override_;

  base::FilePath cache_dir_1_;
  base::FilePath cache_dir_2_;
  base::FilePath cache_dir_3_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyExtensionCacheTest);
};

DeviceLocalAccountPolicyExtensionCacheTest::
    DeviceLocalAccountPolicyExtensionCacheTest() {
}

void DeviceLocalAccountPolicyExtensionCacheTest::SetUp() {
  DeviceLocalAccountPolicyServiceTestBase::SetUp();
  ASSERT_TRUE(cache_root_dir_.CreateUniqueTempDir());
  cache_root_dir_override_.reset(new base::ScopedPathOverride(
      chromeos::DIR_DEVICE_LOCAL_ACCOUNT_EXTENSIONS,
      cache_root_dir_.GetPath()));

  cache_dir_1_ = GetCacheDirectoryForAccountID(kAccount1);
  cache_dir_2_ = GetCacheDirectoryForAccountID(kAccount2);
  cache_dir_3_ = GetCacheDirectoryForAccountID(kAccount3);

  em::StringList* forcelist = device_local_account_policy_.payload()
      .mutable_extensioninstallforcelist()->mutable_value();
  forcelist->add_entries(base::StringPrintf("%s;%s", kExtensionID, kUpdateURL));
}

base::FilePath DeviceLocalAccountPolicyExtensionCacheTest::
    GetCacheDirectoryForAccountID(const std::string& account_id) {
  return cache_root_dir_.GetPath().Append(
      base::HexEncode(account_id.c_str(), account_id.size()));
}

// Verifies that during startup, orphaned cache directories are deleted,
// cache directories belonging to an existing account are preserved and missing
// cache directories are created. Also verifies that when startup is complete,
// the caches for all existing accounts are running.
TEST_F(DeviceLocalAccountPolicyExtensionCacheTest, Startup) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  const base::FilePath source_crx_file =
      test_data_dir.Append(kExtensionCRXPath);
  const std::string target_crx_file_name =
      base::StringPrintf("%s-%s.crx", kExtensionID, kExtensionVersion);

  // Create and pre-populate a cache directory for account 1.
  EXPECT_TRUE(base::CreateDirectory(cache_dir_1_));
  EXPECT_TRUE(CopyFile(source_crx_file,
                       cache_dir_1_.Append(target_crx_file_name)));

  // Create and pre-populate a cache directory for account 3.
  EXPECT_TRUE(base::CreateDirectory(cache_dir_3_));
  EXPECT_TRUE(CopyFile(source_crx_file,
                       cache_dir_3_.Append(target_crx_file_name)));

  // Add accounts 1 and 2 to device policy.
  InstallDeviceLocalAccountPolicy(kAccount1);
  InstallDeviceLocalAccountPolicy(kAccount2);
  AddDeviceLocalAccountToPolicy(kAccount1);
  AddDeviceLocalAccountToPolicy(kAccount2);
  InstallDevicePolicy();

  // Create the DeviceLocalAccountPolicyService, allowing it to finish the
  // deletion of orphaned cache directories.
  CreatePolicyService();
  FlushDeviceSettings();
  extension_cache_task_runner_->RunUntilIdle();

  // Verify that the cache directory for account 1 and its contents still exist.
  EXPECT_TRUE(base::DirectoryExists(cache_dir_1_));
  EXPECT_TRUE(ContentsEqual(source_crx_file,
                            cache_dir_1_.Append(target_crx_file_name)));

  // Verify that a cache directory for account 2 was created.
  EXPECT_TRUE(base::DirectoryExists(cache_dir_2_));

  // Verify that the cache directory for account 3 was deleted.
  EXPECT_FALSE(base::DirectoryExists(cache_dir_3_));

  // Verify that the cache for account 1 has been started.
  DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForUser(account_1_user_id_);
  ASSERT_TRUE(broker);
  EXPECT_TRUE(broker->extension_loader()->IsCacheRunning());

  // Verify that the cache for account 2 has been started.
  broker = service_->GetBrokerForUser(account_2_user_id_);
  ASSERT_TRUE(broker);
  EXPECT_TRUE(broker->extension_loader()->IsCacheRunning());
}

// Verifies that while the deletion of orphaned cache directories is in
// progress, the caches for accounts which existed before the deletion started
// are running but caches for newly added accounts are not started.
TEST_F(DeviceLocalAccountPolicyExtensionCacheTest, RaceAgainstOrphanDeletion) {
  // Add account 1 to device policy.
  InstallDeviceLocalAccountPolicy(kAccount1);
  AddDeviceLocalAccountToPolicy(kAccount1);
  InstallDevicePolicy();

  // Create the DeviceLocalAccountPolicyService, triggering the deletion of
  // orphaned cache directories.
  CreatePolicyService();
  FlushDeviceSettings();

  // Verify that the cache for account 1 has been started as it is unaffected by
  // the orphan deletion.
  DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForUser(account_1_user_id_);
  ASSERT_TRUE(broker);
  EXPECT_TRUE(broker->extension_loader()->IsCacheRunning());

  // Add account 2 to device policy.
  InstallDeviceLocalAccountPolicy(kAccount2);
  AddDeviceLocalAccountToPolicy(kAccount2);
  InstallDevicePolicy();

  // Verify that the cache for account 2 has not been started yet as the orphan
  // deletion is still in progress.
  broker = service_->GetBrokerForUser(account_2_user_id_);
  ASSERT_TRUE(broker);
  EXPECT_FALSE(broker->extension_loader()->IsCacheRunning());

  // Allow the orphan deletion to finish.
  extension_cache_task_runner_->RunUntilIdle();
  base::RunLoop().RunUntilIdle();

  // Verify that the cache for account 2 has been started.
  EXPECT_TRUE(broker->extension_loader()->IsCacheRunning());
}

// Verifies that while the shutdown of a cache is in progress, no new cache is
// started if an account with the same ID is re-added.
TEST_F(DeviceLocalAccountPolicyExtensionCacheTest, RaceAgainstCacheShutdown) {
  // Add account 1 to device policy.
  InstallDeviceLocalAccountPolicy(kAccount1);
  AddDeviceLocalAccountToPolicy(kAccount1);
  InstallDevicePolicy();

  // Create the DeviceLocalAccountPolicyService, allowing it to finish the
  // deletion of orphaned cache directories.
  CreatePolicyService();
  FlushDeviceSettings();
  extension_cache_task_runner_->RunUntilIdle();

  // Remove account 1 from device policy, triggering a shutdown of its cache.
  device_policy_.payload().mutable_device_local_accounts()->clear_account();
  InstallDevicePolicy();

  // Re-add account 1 to device policy.
  AddDeviceLocalAccountToPolicy(kAccount1);
  InstallDevicePolicy();

  // Verify that the cache for account 1 has not been started yet as the
  // shutdown of a previous cache for this account ID is still in progress.
  DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForUser(account_1_user_id_);
  ASSERT_TRUE(broker);
  EXPECT_FALSE(broker->extension_loader()->IsCacheRunning());

  // Allow the cache shutdown to finish.
  extension_cache_task_runner_->RunUntilIdle();
  base::RunLoop().RunUntilIdle();

  // Verify that the cache directory for account 1 still exists.
  EXPECT_TRUE(base::DirectoryExists(cache_dir_1_));

  // Verify that the cache for account 1 has been started, reusing the existing
  // cache directory.
  EXPECT_TRUE(broker->extension_loader()->IsCacheRunning());
}

// Verifies that while the deletion of an obsolete cache directory is in
// progress, no new cache is started if an account with the same ID is re-added.
TEST_F(DeviceLocalAccountPolicyExtensionCacheTest,
       RaceAgainstObsoleteDeletion) {
  // Add account 1 to device policy.
  InstallDeviceLocalAccountPolicy(kAccount1);
  AddDeviceLocalAccountToPolicy(kAccount1);
  InstallDevicePolicy();

  // Create the DeviceLocalAccountPolicyService, allowing it to finish the
  // deletion of orphaned cache directories.
  CreatePolicyService();
  FlushDeviceSettings();
  extension_cache_task_runner_->RunUntilIdle();

  // Remove account 1 from device policy, allowing the shutdown of its cache to
  // finish and the deletion of its now obsolete cache directory to begin.
  device_policy_.payload().mutable_device_local_accounts()->clear_account();
  InstallDevicePolicy();
  extension_cache_task_runner_->RunUntilIdle();
  base::RunLoop().RunUntilIdle();

  // Re-add account 1 to device policy.
  AddDeviceLocalAccountToPolicy(kAccount1);
  InstallDevicePolicy();

  // Verify that the cache for account 1 has not been started yet as the
  // deletion of the cache directory for this account ID is still in progress.
  DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForUser(account_1_user_id_);
  ASSERT_TRUE(broker);
  EXPECT_FALSE(broker->extension_loader()->IsCacheRunning());

  // Allow the deletion to finish.
  extension_cache_task_runner_->RunUntilIdle();
  base::RunLoop().RunUntilIdle();

  // Verify that the cache directory for account 1 was deleted.
  EXPECT_FALSE(base::DirectoryExists(cache_dir_1_));

  // Verify that the cache for account 1 has been started.
  EXPECT_TRUE(broker->extension_loader()->IsCacheRunning());
}

// Verifies that when an account is added and no deletion of cache directories
// affecting this account is in progress, its cache is started immediately.
TEST_F(DeviceLocalAccountPolicyExtensionCacheTest, AddAccount) {
  // Create the DeviceLocalAccountPolicyService, allowing it to finish the
  // deletion of orphaned cache directories.
  InstallDevicePolicy();
  CreatePolicyService();
  FlushDeviceSettings();
  extension_cache_task_runner_->RunUntilIdle();

  // Add account 1 to device policy.
  InstallDeviceLocalAccountPolicy(kAccount1);
  AddDeviceLocalAccountToPolicy(kAccount1);
  InstallDevicePolicy();

  // Verify that the cache for account 1 has been started.
  DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForUser(account_1_user_id_);
  ASSERT_TRUE(broker);
  EXPECT_TRUE(broker->extension_loader()->IsCacheRunning());
}

// Verifies that when an account is removed, its cache directory is deleted.
TEST_F(DeviceLocalAccountPolicyExtensionCacheTest, RemoveAccount) {
  // Add account 1 to device policy.
  InstallDeviceLocalAccountPolicy(kAccount1);
  AddDeviceLocalAccountToPolicy(kAccount1);
  InstallDevicePolicy();

  // Create the DeviceLocalAccountPolicyService, allowing it to finish the
  // deletion of orphaned cache directories.
  CreatePolicyService();
  FlushDeviceSettings();
  extension_cache_task_runner_->RunUntilIdle();

  // Verify that a cache directory has been created for account 1.
  EXPECT_TRUE(base::DirectoryExists(cache_dir_1_));

  // Remove account 1 from device policy, allowing the deletion of its now
  // obsolete cache directory to finish.
  device_policy_.payload().mutable_device_local_accounts()->clear_account();
  InstallDevicePolicy();
  extension_cache_task_runner_->RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  extension_cache_task_runner_->RunUntilIdle();

  // Verify that the cache directory for account 1 was deleted.
  EXPECT_FALSE(base::DirectoryExists(cache_dir_1_));
}

class DeviceLocalAccountPolicyProviderTest
    : public DeviceLocalAccountPolicyServiceTestBase {
 protected:
  DeviceLocalAccountPolicyProviderTest();

  void SetUp() override;
  void TearDown() override;

  SchemaRegistry schema_registry_;
  std::unique_ptr<DeviceLocalAccountPolicyProvider> provider_;
  MockConfigurationPolicyObserver provider_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyProviderTest);
};

DeviceLocalAccountPolicyProviderTest::DeviceLocalAccountPolicyProviderTest()
    : DeviceLocalAccountPolicyServiceTestBase() {}

void DeviceLocalAccountPolicyProviderTest::SetUp() {
  DeviceLocalAccountPolicyServiceTestBase::SetUp();
  CreatePolicyService();
  provider_ = DeviceLocalAccountPolicyProvider::Create(
      GenerateDeviceLocalAccountUserId(kAccount1,
                                       DeviceLocalAccount::TYPE_PUBLIC_SESSION),
      service_.get(), false /*force_immediate_load*/);
  provider_->Init(&schema_registry_);
  provider_->AddObserver(&provider_observer_);

  // Values implicitly enforced for public accounts.
  expected_policy_map_.Set(
      key::kLidCloseAction, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
      POLICY_SOURCE_PUBLIC_SESSION_OVERRIDE,
      std::make_unique<base::Value>(
          chromeos::PowerPolicyController::ACTION_STOP_SESSION),
      nullptr);
  expected_policy_map_.Set(key::kShelfAutoHideBehavior, POLICY_LEVEL_MANDATORY,
                           POLICY_SCOPE_MACHINE,
                           POLICY_SOURCE_PUBLIC_SESSION_OVERRIDE,
                           std::make_unique<base::Value>("Never"), nullptr);
  expected_policy_map_.Set(key::kShowLogoutButtonInTray, POLICY_LEVEL_MANDATORY,
                           POLICY_SCOPE_MACHINE,
                           POLICY_SOURCE_PUBLIC_SESSION_OVERRIDE,
                           std::make_unique<base::Value>(true), nullptr);

  // Policy defaults (for policies not set by admin).
  SetEnterpriseUsersDefaults(&expected_policy_map_);
}

void DeviceLocalAccountPolicyProviderTest::TearDown() {
  provider_->RemoveObserver(&provider_observer_);
  provider_->Shutdown();
  provider_.reset();
  DeviceLocalAccountPolicyServiceTestBase::TearDown();
}

TEST_F(DeviceLocalAccountPolicyProviderTest, Initialization) {
  EXPECT_FALSE(provider_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Policy change should complete initialization.
  InstallDeviceLocalAccountPolicy(kAccount1);
  AddDeviceLocalAccountToPolicy(kAccount1);
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(provider_.get()))
      .Times(AtLeast(1));
  InstallDevicePolicy();
  Mock::VerifyAndClearExpectations(&provider_observer_);

  EXPECT_TRUE(provider_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // The account disappearing should *not* flip the initialization flag back.
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(provider_.get()))
      .Times(AnyNumber());
  device_policy_.payload().mutable_device_local_accounts()->clear_account();
  InstallDevicePolicy();
  Mock::VerifyAndClearExpectations(&provider_observer_);

  EXPECT_TRUE(provider_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

TEST_F(DeviceLocalAccountPolicyProviderTest, Policy) {
  // Policy should load successfully.
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(provider_.get()))
      .Times(AtLeast(1));
  InstallDeviceLocalAccountPolicy(kAccount1);
  AddDeviceLocalAccountToPolicy(kAccount1);
  InstallDevicePolicy();
  Mock::VerifyAndClearExpectations(&provider_observer_);

  PolicyBundle expected_policy_bundle;
  expected_policy_bundle.Get(PolicyNamespace(
      POLICY_DOMAIN_CHROME, std::string())).CopyFrom(expected_policy_map_);
  EXPECT_TRUE(expected_policy_bundle.Equals(provider_->policies()));

  // Make sure the Dinosaur game is disabled by default. This ensures the
  // default policies have been set in Public Sessions.
  bool allow_dinosaur_game = true;
  auto* policy_value =
      provider_->policies()
          .Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
          .GetValue(key::kAllowDinosaurEasterEgg);
  EXPECT_TRUE(policy_value && policy_value->GetAsBoolean(&allow_dinosaur_game));
  EXPECT_FALSE(allow_dinosaur_game);

  // Policy change should be reported.
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(provider_.get()))
      .Times(AtLeast(1));
  device_local_account_policy_.payload()
      .mutable_searchsuggestenabled()
      ->set_value(false);
  InstallDeviceLocalAccountPolicy(kAccount1);
  DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForUser(account_1_user_id_);
  ASSERT_TRUE(broker);
  broker->core()->store()->Load();
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&provider_observer_);

  expected_policy_bundle
      .Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(key::kSearchSuggestEnabled, POLICY_LEVEL_MANDATORY,
           POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
           std::make_unique<base::Value>(false), nullptr);
  EXPECT_TRUE(expected_policy_bundle.Equals(provider_->policies()));

  // Any values set for the |ShelfAutoHideBehavior|, |ShowLogoutButtonInTray|
  // and |ExtensionAllowedTypes| policies should be overridden.
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(provider_.get()))
      .Times(AtLeast(1));
  device_local_account_policy_.payload().mutable_shelfautohidebehavior()->
      set_value("Always");
  device_local_account_policy_.payload().mutable_showlogoutbuttonintray()->
      set_value(false);
  InstallDeviceLocalAccountPolicy(kAccount1);
  broker->core()->store()->Load();
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&provider_observer_);
  EXPECT_TRUE(expected_policy_bundle.Equals(provider_->policies()));

  // Account disappears, policy should stay in effect.
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(provider_.get()))
      .Times(AnyNumber());
  device_policy_.payload().mutable_device_local_accounts()->clear_account();
  InstallDevicePolicy();
  Mock::VerifyAndClearExpectations(&provider_observer_);

  EXPECT_TRUE(expected_policy_bundle.Equals(provider_->policies()));
}

TEST_F(DeviceLocalAccountPolicyProviderTest, RefreshPolicies) {
  // If there's no device policy, the refresh completes immediately.
  EXPECT_FALSE(service_->GetBrokerForUser(account_1_user_id_));
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(provider_.get()))
      .Times(AtLeast(1));
  provider_->RefreshPolicies();
  Mock::VerifyAndClearExpectations(&provider_observer_);

  // Make device settings appear.
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(provider_.get()))
      .Times(AnyNumber());
  AddDeviceLocalAccountToPolicy(kAccount1);
  InstallDevicePolicy();
  EXPECT_TRUE(service_->GetBrokerForUser(account_1_user_id_));

  // If there's no cloud connection, refreshes are still immediate.
  DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForUser(account_1_user_id_);
  ASSERT_TRUE(broker);
  EXPECT_FALSE(broker->core()->client());
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(provider_.get()))
      .Times(AtLeast(1));
  provider_->RefreshPolicies();
  Mock::VerifyAndClearExpectations(&provider_observer_);

  // Bring up the cloud connection. The refresh scheduler may fire refreshes at
  // this point which are not relevant for the test.
  EXPECT_CALL(mock_device_management_service_, CreateJob(_, _))
      .WillRepeatedly(
          mock_device_management_service_.FailJob(DM_STATUS_REQUEST_FAILED));
  EXPECT_CALL(mock_device_management_service_, StartJob(_, _, _, _, _, _, _))
      .Times(AnyNumber());
  service_->Connect(&mock_device_management_service_);
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&mock_device_management_service_);

  // No callbacks until the refresh completes.
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(_)).Times(0);
  MockDeviceManagementJob* request_job;
  EXPECT_CALL(mock_device_management_service_, CreateJob(_, _))
      .WillOnce(mock_device_management_service_.CreateAsyncJob(&request_job));
  EXPECT_CALL(mock_device_management_service_, StartJob(_, _, _, _, _, _, _));
  provider_->RefreshPolicies();
  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(&provider_observer_);
  Mock::VerifyAndClearExpectations(&mock_device_management_service_);
  EXPECT_TRUE(provider_->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // When the response comes in, it should propagate and fire the notification.
  EXPECT_CALL(provider_observer_, OnUpdatePolicy(provider_.get()))
      .Times(AtLeast(1));
  ASSERT_TRUE(request_job);
  em::DeviceManagementResponse response;
  device_local_account_policy_.Build();
  response.mutable_policy_response()->add_response()->CopyFrom(
      device_local_account_policy_.policy());
  request_job->SendResponse(DM_STATUS_SUCCESS, response);
  FlushDeviceSettings();
  Mock::VerifyAndClearExpectations(&provider_observer_);
}

class DeviceLocalAccountPolicyProviderLoadImmediateTest
    : public DeviceLocalAccountPolicyServiceTestBase {
 protected:
  DeviceLocalAccountPolicyProviderLoadImmediateTest();

  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<DeviceLocalAccountPolicyProvider> provider_;
  MockDeviceLocalAccountPolicyServiceObserver service_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyProviderLoadImmediateTest);
};

DeviceLocalAccountPolicyProviderLoadImmediateTest::
    DeviceLocalAccountPolicyProviderLoadImmediateTest() {}

void DeviceLocalAccountPolicyProviderLoadImmediateTest::SetUp() {
  DeviceLocalAccountPolicyServiceTestBase::SetUp();
  CreatePolicyService();
  service_->AddObserver(&service_observer_);
}

void DeviceLocalAccountPolicyProviderLoadImmediateTest::TearDown() {
  service_->RemoveObserver(&service_observer_);
  provider_->Shutdown();
  provider_.reset();
  DeviceLocalAccountPolicyServiceTestBase::TearDown();
}

TEST_F(DeviceLocalAccountPolicyProviderLoadImmediateTest, Initialization) {
  InstallDeviceLocalAccountPolicy(kAccount1);
  AddDeviceLocalAccountToPolicy(kAccount1);
  EXPECT_CALL(service_observer_, OnPolicyUpdated(account_1_user_id_))
      .Times(AtLeast(2));
  EXPECT_CALL(service_observer_, OnDeviceLocalAccountsChanged());
  InstallDevicePolicy();

  provider_ = DeviceLocalAccountPolicyProvider::Create(
      GenerateDeviceLocalAccountUserId(kAccount1,
                                       DeviceLocalAccount::TYPE_PUBLIC_SESSION),
      service_.get(), true /*force_immediate_load*/);

  EXPECT_TRUE(provider_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
}

}  // namespace policy

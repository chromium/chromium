// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/cloud_external_data_policy_observer.h"

#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base_test_util.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_external_data_manager.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_provider.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/policy/fake_affiliated_invalidation_service_provider.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_utils.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace em = enterprise_management;

using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::_;

namespace policy {

namespace {

const char kDeviceLocalAccount[] = "device_local_account@localhost";

const char kRegularUserID[] = "user@example.com";

const char kAvatar1URL[] = "http://localhost/avatar1.jpg";
const char kAvatar2URL[] = "http://localhost/avatar2.jpg";

void ConstructAvatarPolicy(const std::string& file_name,
                           const std::string& url,
                           std::string* policy_data,
                           std::string* policy) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  ASSERT_TRUE(base::ReadFileToString(
      test_data_dir.Append("chromeos").Append(file_name),
      policy_data));
  base::JSONWriter::Write(
      *test::ConstructExternalDataReference(url, *policy_data), policy);
}

}  // namespace

class CloudExternalDataPolicyObserverTest
    : public chromeos::DeviceSettingsTestBase,
      public CloudExternalDataPolicyObserver::Delegate {
 public:
  typedef std::pair<std::string, std::string> FetchedCall;

  CloudExternalDataPolicyObserverTest();
  ~CloudExternalDataPolicyObserverTest() override;

  // chromeos::DeviceSettingsTestBase:
  void SetUp() override;
  void TearDown() override;

  // CloudExternalDataPolicyObserver::Delegate:
  void OnExternalDataSet(const std::string& policy,
                         const std::string& user_id) override;
  void OnExternalDataCleared(const std::string& policy,
                             const std::string& user_id) override;
  void OnExternalDataFetched(const std::string& policy,
                             const std::string& user_id,
                             std::unique_ptr<std::string> data) override;

  void CreateObserver();
  void RemoveObserver();

  void ClearObservations();

  void SetDeviceLocalAccountAvatarPolicy(const std::string& account_id,
                                         const std::string& value);

  void AddDeviceLocalAccount(const std::string& account_id);
  void RemoveDeviceLocalAccount(const std::string& account_id);

  DeviceLocalAccountPolicyBroker* GetBrokerForDeviceLocalAccountUser();

  void RefreshDeviceLocalAccountPolicy(DeviceLocalAccountPolicyBroker* broker);

  void LogInAsDeviceLocalAccount(const AccountId& account_id);

  void SetRegularUserAvatarPolicy(const std::string& value);

  void LogInAsRegularUser();

  const std::string device_local_account_user_id_;

  std::string avatar_policy_1_data_;
  std::string avatar_policy_2_data_;
  std::string avatar_policy_1_;
  std::string avatar_policy_2_;

  std::unique_ptr<chromeos::CrosSettings> cros_settings_;
  std::unique_ptr<DeviceLocalAccountPolicyService>
      device_local_account_policy_service_;
  FakeAffiliatedInvalidationServiceProvider
      affiliated_invalidation_service_provider_;
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  std::unique_ptr<DeviceLocalAccountPolicyProvider>
      device_local_account_policy_provider_;

  MockCloudExternalDataManager external_data_manager_;
  MockConfigurationPolicyProvider user_policy_provider_;

  std::unique_ptr<TestingProfile> profile_;

  std::unique_ptr<CloudExternalDataPolicyObserver> observer_;

  std::vector<std::string> set_calls_;
  std::vector<std::string> cleared_calls_;
  std::vector<FetchedCall> fetched_calls_;

  ExternalDataFetcher::FetchCallback fetch_callback_;

  TestingProfileManager profile_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CloudExternalDataPolicyObserverTest);
};

CloudExternalDataPolicyObserverTest::CloudExternalDataPolicyObserverTest()
    : device_local_account_user_id_(GenerateDeviceLocalAccountUserId(
          kDeviceLocalAccount,
          DeviceLocalAccount::TYPE_PUBLIC_SESSION)),
      profile_manager_(TestingBrowserProcess::GetGlobal()) {}

CloudExternalDataPolicyObserverTest::~CloudExternalDataPolicyObserverTest() {
}

void CloudExternalDataPolicyObserverTest::SetUp() {
  chromeos::DeviceSettingsTestBase::SetUp();

  ASSERT_TRUE(profile_manager_.SetUp());
  shared_url_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &url_loader_factory_);
  cros_settings_ = std::make_unique<chromeos::CrosSettings>(
      &device_settings_service_,
      TestingBrowserProcess::GetGlobal()->local_state());
  device_local_account_policy_service_.reset(
      new DeviceLocalAccountPolicyService(
          &session_manager_client_, &device_settings_service_,
          cros_settings_.get(), &affiliated_invalidation_service_provider_,
          base::ThreadTaskRunnerHandle::Get(),
          base::ThreadTaskRunnerHandle::Get(),
          base::ThreadTaskRunnerHandle::Get(), shared_url_loader_factory_));

  EXPECT_CALL(user_policy_provider_, IsInitializationComplete(_))
      .WillRepeatedly(Return(true));
  user_policy_provider_.Init();

  ConstructAvatarPolicy("avatar1.jpg",
                        kAvatar1URL,
                        &avatar_policy_1_data_,
                        &avatar_policy_1_);
  ConstructAvatarPolicy("avatar2.jpg",
                        kAvatar2URL,
                        &avatar_policy_2_data_,
                        &avatar_policy_2_);
}

void CloudExternalDataPolicyObserverTest::TearDown() {
  observer_.reset();
  user_policy_provider_.Shutdown();
  profile_.reset();
  if (device_local_account_policy_provider_) {
    device_local_account_policy_provider_->Shutdown();
    device_local_account_policy_provider_.reset();
  }
  device_local_account_policy_service_->Shutdown();
  device_local_account_policy_service_.reset();
  cros_settings_.reset();
  chromeos::DeviceSettingsTestBase::TearDown();
}


void CloudExternalDataPolicyObserverTest::OnExternalDataSet(
    const std::string& policy,
    const std::string& user_id) {
  EXPECT_EQ(key::kUserAvatarImage, policy);
  set_calls_.push_back(user_id);
}

void CloudExternalDataPolicyObserverTest::OnExternalDataCleared(
    const std::string& policy,
    const std::string& user_id) {
  EXPECT_EQ(key::kUserAvatarImage, policy);
  cleared_calls_.push_back(user_id);
}

void CloudExternalDataPolicyObserverTest::OnExternalDataFetched(
    const std::string& policy,
    const std::string& user_id,
    std::unique_ptr<std::string> data) {
  EXPECT_EQ(key::kUserAvatarImage, policy);
  fetched_calls_.push_back(make_pair(user_id, std::string()));
  fetched_calls_.back().second.swap(*data);
}

void CloudExternalDataPolicyObserverTest::CreateObserver() {
  observer_.reset(new CloudExternalDataPolicyObserver(
      cros_settings_.get(), device_local_account_policy_service_.get(),
      key::kUserAvatarImage, this));
  observer_->Init();
}

void CloudExternalDataPolicyObserverTest::RemoveObserver() {
  observer_.reset();
}

void CloudExternalDataPolicyObserverTest::ClearObservations() {
  set_calls_.clear();
  cleared_calls_.clear();
  fetched_calls_.clear();
}

void CloudExternalDataPolicyObserverTest::SetDeviceLocalAccountAvatarPolicy(
    const std::string& account_id,
    const std::string& value) {
  UserPolicyBuilder builder;
  builder.policy_data().set_policy_type(
      dm_protocol::kChromePublicAccountPolicyType);
  builder.policy_data().set_settings_entity_id(account_id);
  builder.policy_data().set_username(account_id);
  if (!value.empty())
    builder.payload().mutable_useravatarimage()->set_value(value);
  builder.Build();
  session_manager_client_.set_device_local_account_policy(account_id,
                                                          builder.GetBlob());
}

void CloudExternalDataPolicyObserverTest::AddDeviceLocalAccount(
    const std::string& account_id) {
  em::DeviceLocalAccountInfoProto* account =
      device_policy_.payload().mutable_device_local_accounts()->add_account();
  account->set_account_id(account_id);
  account->set_type(
      em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);
  device_policy_.Build();
  session_manager_client_.set_device_policy(device_policy_.GetBlob());
  ReloadDeviceSettings();
}

void CloudExternalDataPolicyObserverTest::RemoveDeviceLocalAccount(
    const std::string& account_id) {
  em::DeviceLocalAccountsProto* accounts =
      device_policy_.payload().mutable_device_local_accounts();
  std::vector<std::string> account_ids;
  for (int i = 0; i < accounts->account_size(); ++i) {
    if (accounts->account(i).account_id() != account_id)
      account_ids.push_back(accounts->account(i).account_id());
  }
  accounts->clear_account();
  for (std::vector<std::string>::const_iterator it = account_ids.begin();
       it != account_ids.end(); ++it) {
    em::DeviceLocalAccountInfoProto* account = accounts->add_account();
    account->set_account_id(*it);
    account->set_type(
        em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);
  }
  device_policy_.Build();
  session_manager_client_.set_device_policy(device_policy_.GetBlob());
  ReloadDeviceSettings();
}

DeviceLocalAccountPolicyBroker*
    CloudExternalDataPolicyObserverTest::GetBrokerForDeviceLocalAccountUser() {
  return device_local_account_policy_service_->GetBrokerForUser(
      device_local_account_user_id_);
}

void CloudExternalDataPolicyObserverTest::RefreshDeviceLocalAccountPolicy(
    DeviceLocalAccountPolicyBroker* broker) {
  broker->core()->store()->Load();
  content::RunAllTasksUntilIdle();
}

void CloudExternalDataPolicyObserverTest::LogInAsDeviceLocalAccount(
    const AccountId& account_id) {
  user_manager_->AddUser(account_id);

  device_local_account_policy_provider_.reset(
      new DeviceLocalAccountPolicyProvider(
          account_id.GetUserEmail(), device_local_account_policy_service_.get(),
          std::unique_ptr<PolicyMap>()));

  PolicyServiceImpl::Providers providers;
  providers.push_back(device_local_account_policy_provider_.get());
  TestingProfile::Builder builder;
  std::unique_ptr<PolicyServiceImpl> policy_service =
      std::make_unique<PolicyServiceImpl>(std::move(providers));
  builder.SetPolicyService(std::move(policy_service));
  builder.SetPath(chromeos::ProfileHelper::Get()->GetProfilePathByUserIdHash(
      chromeos::ProfileHelper::GetUserIdHashByUserIdForTesting(
          account_id.GetUserEmail())));

  profile_ = builder.Build();
  profile_->set_profile_name(account_id.GetUserEmail());

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
      content::NotificationService::AllSources(),
      content::Details<Profile>(profile_.get()));
}

void CloudExternalDataPolicyObserverTest::SetRegularUserAvatarPolicy(
    const std::string& value) {
  PolicyMap policy_map;
  if (!value.empty()) {
    policy_map.Set(key::kUserAvatarImage, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                   std::make_unique<base::Value>(value),
                   external_data_manager_.CreateExternalDataFetcher(
                       key::kUserAvatarImage));
  }
  user_policy_provider_.UpdateChromePolicy(policy_map);
}

void CloudExternalDataPolicyObserverTest::LogInAsRegularUser() {
  user_manager_->AddUser(AccountId::FromUserEmail(kRegularUserID));

  PolicyServiceImpl::Providers providers;
  providers.push_back(&user_policy_provider_);
  TestingProfile::Builder builder;
  std::unique_ptr<PolicyServiceImpl> policy_service =
      std::make_unique<PolicyServiceImpl>(std::move(providers));
  builder.SetPolicyService(std::move(policy_service));
  builder.SetPath(chromeos::ProfileHelper::Get()->GetProfilePathByUserIdHash(
      chromeos::ProfileHelper::GetUserIdHashByUserIdForTesting(
          kRegularUserID)));

  profile_ = builder.Build();
  profile_->set_profile_name(kRegularUserID);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
      content::NotificationService::AllSources(),
      content::Details<Profile>(profile_.get()));
}

// Verifies that when an external data reference is set for a device-local
// account, a corresponding notification is emitted and a fetch is started.
// Further verifies that when the fetch succeeds, a notification containing the
// external data is emitted.
TEST_F(CloudExternalDataPolicyObserverTest,
       ExistingDeviceLocalAccountFetchSuccess) {
  SetDeviceLocalAccountAvatarPolicy(kDeviceLocalAccount, avatar_policy_1_);
  AddDeviceLocalAccount(kDeviceLocalAccount);

  DeviceLocalAccountPolicyBroker* broker = GetBrokerForDeviceLocalAccountUser();
  ASSERT_TRUE(broker);
  broker->external_data_manager()->Connect(shared_url_loader_factory_);
  base::RunLoop().RunUntilIdle();

  CreateObserver();

  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  EXPECT_EQ(1u, set_calls_.size());
  EXPECT_EQ(device_local_account_user_id_, set_calls_.front());
  ClearObservations();

  EXPECT_TRUE(url_loader_factory_.IsPending(kAvatar1URL));
  url_loader_factory_.AddResponse(kAvatar1URL, avatar_policy_1_data_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_EQ(1u, fetched_calls_.size());
  EXPECT_EQ(device_local_account_user_id_, fetched_calls_.front().first);
  EXPECT_EQ(avatar_policy_1_data_, fetched_calls_.front().second);
  ClearObservations();

  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// Verifies that when an external data reference is set for a device-local
// account, a corresponding notification is emitted and a fetch is started.
// Further verifies that when the fetch fails, no notification is emitted.
TEST_F(CloudExternalDataPolicyObserverTest,
       ExistingDeviceLocalAccountFetchFailure) {
  SetDeviceLocalAccountAvatarPolicy(kDeviceLocalAccount, avatar_policy_1_);
  AddDeviceLocalAccount(kDeviceLocalAccount);

  DeviceLocalAccountPolicyBroker* broker = GetBrokerForDeviceLocalAccountUser();
  ASSERT_TRUE(broker);
  broker->external_data_manager()->Connect(shared_url_loader_factory_);
  base::RunLoop().RunUntilIdle();

  CreateObserver();

  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  EXPECT_EQ(1u, set_calls_.size());
  EXPECT_EQ(device_local_account_user_id_, set_calls_.front());
  ClearObservations();

  EXPECT_TRUE(url_loader_factory_.IsPending(kAvatar1URL));
  url_loader_factory_.AddResponse(kAvatar1URL, std::string(),
                                  net::HTTP_BAD_REQUEST);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  ClearObservations();

  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// Verifies that when the external data reference for a device-local account is
// initially not set, no notifications are emitted. Further verifies that when
// the external data reference is then cleared (which is a no-op), again, no
// notifications are emitted.
TEST_F(CloudExternalDataPolicyObserverTest,
       ExistingDeviceLocalAccountClearUnset) {
  AddDeviceLocalAccount(kDeviceLocalAccount);

  DeviceLocalAccountPolicyBroker* broker = GetBrokerForDeviceLocalAccountUser();
  ASSERT_TRUE(broker);
  broker->external_data_manager()->Connect(shared_url_loader_factory_);
  base::RunLoop().RunUntilIdle();

  CreateObserver();

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  ClearObservations();

  EXPECT_EQ(0, url_loader_factory_.NumPending());

  SetDeviceLocalAccountAvatarPolicy(kDeviceLocalAccount, "");
  RefreshDeviceLocalAccountPolicy(broker);

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  ClearObservations();

  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// Verifies that when the external data reference for a device-local account is
// initially set, a corresponding notification is emitted and a fetch is
// started. Further verifies that when the external data reference is then
// cleared, a corresponding notification is emitted and the fetch is canceled.
TEST_F(CloudExternalDataPolicyObserverTest,
       ExistingDeviceLocalAccountClearSet) {
  SetDeviceLocalAccountAvatarPolicy(kDeviceLocalAccount, avatar_policy_1_);
  AddDeviceLocalAccount(kDeviceLocalAccount);

  DeviceLocalAccountPolicyBroker* broker = GetBrokerForDeviceLocalAccountUser();
  ASSERT_TRUE(broker);
  broker->external_data_manager()->Connect(shared_url_loader_factory_);
  base::RunLoop().RunUntilIdle();

  CreateObserver();

  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  EXPECT_EQ(1u, set_calls_.size());
  EXPECT_EQ(device_local_account_user_id_, set_calls_.front());
  ClearObservations();

  EXPECT_TRUE(url_loader_factory_.IsPending(kAvatar1URL));

  SetDeviceLocalAccountAvatarPolicy(kDeviceLocalAccount, "");
  RefreshDeviceLocalAccountPolicy(broker);

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  EXPECT_EQ(1u, cleared_calls_.size());
  EXPECT_EQ(device_local_account_user_id_, cleared_calls_.front());
  ClearObservations();

  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// Verifies that when the external data reference for a device-local account is
// initially not set, no notifications are emitted. Further verifies that when
// the external data reference is then set, a corresponding notification is
// emitted and a fetch is started. Also verifies that when the fetch eventually
// succeeds, a notification containing the external data is emitted.
TEST_F(CloudExternalDataPolicyObserverTest,
       ExistingDeviceLocalAccountSetUnset) {
  AddDeviceLocalAccount(kDeviceLocalAccount);

  DeviceLocalAccountPolicyBroker* broker = GetBrokerForDeviceLocalAccountUser();
  ASSERT_TRUE(broker);
  broker->external_data_manager()->Connect(shared_url_loader_factory_);
  base::RunLoop().RunUntilIdle();

  CreateObserver();

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  ClearObservations();

  SetDeviceLocalAccountAvatarPolicy(kDeviceLocalAccount, avatar_policy_1_);
  RefreshDeviceLocalAccountPolicy(broker);

  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  EXPECT_EQ(1u, set_calls_.size());
  EXPECT_EQ(device_local_account_user_id_, set_calls_.front());
  ClearObservations();

  EXPECT_TRUE(url_loader_factory_.IsPending(kAvatar1URL));
  url_loader_factory_.AddResponse(kAvatar1URL, avatar_policy_1_data_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_EQ(1u, fetched_calls_.size());
  EXPECT_EQ(device_local_account_user_id_, fetched_calls_.front().first);
  EXPECT_EQ(avatar_policy_1_data_, fetched_calls_.front().second);
  ClearObservations();

  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// Verifies that when the external data reference for a device-local account is
// initially set, a corresponding notification is emitted and a fetch is
// started. Further verifies that when the external data reference is then
// updated, a corresponding notification is emitted and the fetch is restarted.
// Also verifies that when the fetch eventually succeeds, a notification
// containing the external data is emitted.
TEST_F(CloudExternalDataPolicyObserverTest, ExistingDeviceLocalAccountSetSet) {
  SetDeviceLocalAccountAvatarPolicy(kDeviceLocalAccount, avatar_policy_1_);
  AddDeviceLocalAccount(kDeviceLocalAccount);

  DeviceLocalAccountPolicyBroker* broker = GetBrokerForDeviceLocalAccountUser();
  ASSERT_TRUE(broker);
  broker->external_data_manager()->Connect(shared_url_loader_factory_);
  base::RunLoop().RunUntilIdle();

  CreateObserver();

  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  EXPECT_EQ(1u, set_calls_.size());
  EXPECT_EQ(device_local_account_user_id_, set_calls_.front());
  ClearObservations();

  EXPECT_TRUE(url_loader_factory_.IsPending(kAvatar1URL));

  SetDeviceLocalAccountAvatarPolicy(kDeviceLocalAccount, avatar_policy_2_);
  RefreshDeviceLocalAccountPolicy(broker);

  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  EXPECT_EQ(1u, set_calls_.size());
  EXPECT_EQ(device_local_account_user_id_, set_calls_.front());
  ClearObservations();

  EXPECT_EQ(1, url_loader_factory_.NumPending());
  EXPECT_TRUE(url_loader_factory_.IsPending(kAvatar2URL));
  url_loader_factory_.AddResponse(kAvatar2URL, avatar_policy_2_data_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_EQ(1u, fetched_calls_.size());
  EXPECT_EQ(device_local_account_user_id_, fetched_calls_.front().first);
  EXPECT_EQ(avatar_policy_2_data_, fetched_calls_.front().second);
  ClearObservations();

  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// Verifies that when the external data reference for a device-local account is
// initially not set, a 'cleared' notification is emitted during login into the
// account. Further verifies that when the external data reference is then set,
// a corresponding notification is emitted only once and a fetch is started.
// Also verifies that when the fetch eventually succeeds, a notification
// containing the external data is emitted, again, only once.
TEST_F(CloudExternalDataPolicyObserverTest,
       ExistingDeviceLocalAccountSetAfterLogin) {
  AddDeviceLocalAccount(kDeviceLocalAccount);

  DeviceLocalAccountPolicyBroker* broker = GetBrokerForDeviceLocalAccountUser();
  ASSERT_TRUE(broker);
  broker->external_data_manager()->Connect(shared_url_loader_factory_);
  base::RunLoop().RunUntilIdle();

  CreateObserver();

  LogInAsDeviceLocalAccount(AccountId::FromUserEmail(kDeviceLocalAccount));

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_EQ(1u, cleared_calls_.size());
  EXPECT_TRUE(fetched_calls_.empty());
  ClearObservations();

  SetDeviceLocalAccountAvatarPolicy(kDeviceLocalAccount, avatar_policy_1_);
  RefreshDeviceLocalAccountPolicy(broker);

  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  EXPECT_EQ(1u, set_calls_.size());
  EXPECT_EQ(device_local_account_user_id_, set_calls_.front());
  ClearObservations();

  EXPECT_TRUE(url_loader_factory_.IsPending(kAvatar1URL));
  url_loader_factory_.AddResponse(kAvatar1URL, avatar_policy_1_data_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_EQ(1u, fetched_calls_.size());
  EXPECT_EQ(device_local_account_user_id_, fetched_calls_.front().first);
  EXPECT_EQ(avatar_policy_1_data_, fetched_calls_.front().second);
  ClearObservations();

  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// Verifies that when the external data reference for a device-local account is
// initially not set, no notifications are emitted. Further verifies that when
// the device-local account is then removed, again, no notifications are sent.
TEST_F(CloudExternalDataPolicyObserverTest,
       ExistingDeviceLocalAccountRemoveAccountUnset) {
  AddDeviceLocalAccount(kDeviceLocalAccount);

  DeviceLocalAccountPolicyBroker* broker = GetBrokerForDeviceLocalAccountUser();
  ASSERT_TRUE(broker);
  broker->external_data_manager()->Connect(shared_url_loader_factory_);
  base::RunLoop().RunUntilIdle();

  CreateObserver();

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  ClearObservations();

  EXPECT_EQ(0, url_loader_factory_.NumPending());

  RemoveDeviceLocalAccount(kDeviceLocalAccount);

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  ClearObservations();

  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// Verifies that when the external data reference for a device-local account is
// initially set, a corresponding notification is emitted and a fetch is
// started. Further verifies that when the device-local account is then removed,
// a notification indicating that the external data reference has been cleared
// is emitted and the fetch is canceled.
TEST_F(CloudExternalDataPolicyObserverTest,
       ExistingDeviceLocalAccountRemoveAccountSet) {
  SetDeviceLocalAccountAvatarPolicy(kDeviceLocalAccount, avatar_policy_1_);
  AddDeviceLocalAccount(kDeviceLocalAccount);

  DeviceLocalAccountPolicyBroker* broker = GetBrokerForDeviceLocalAccountUser();
  ASSERT_TRUE(broker);
  broker->external_data_manager()->Connect(shared_url_loader_factory_);
  base::RunLoop().RunUntilIdle();

  CreateObserver();

  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  EXPECT_EQ(1u, set_calls_.size());
  EXPECT_EQ(device_local_account_user_id_, set_calls_.front());
  ClearObservations();

  EXPECT_TRUE(url_loader_factory_.IsPending(kAvatar1URL));

  RemoveDeviceLocalAccount(kDeviceLocalAccount);

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  EXPECT_EQ(1u, cleared_calls_.size());
  EXPECT_EQ(device_local_account_user_id_, cleared_calls_.front());
  ClearObservations();

  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// Verifies that when an external data reference is set for a regular user and
// the user logs in, a corresponding notification is emitted and a fetch is
// started. Further verifies that when the fetch succeeds, a notification
// containing the external data is emitted.
TEST_F(CloudExternalDataPolicyObserverTest, RegularUserFetchSuccess) {
  SetRegularUserAvatarPolicy(avatar_policy_1_);

  CreateObserver();

  EXPECT_CALL(external_data_manager_, Fetch(key::kUserAvatarImage, _))
      .Times(1)
      .WillOnce(SaveArg<1>(&fetch_callback_));

  LogInAsRegularUser();

  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  EXPECT_EQ(1u, set_calls_.size());
  EXPECT_EQ(kRegularUserID, set_calls_.front());
  ClearObservations();

  Mock::VerifyAndClear(&external_data_manager_);
  EXPECT_CALL(external_data_manager_, Fetch(key::kUserAvatarImage, _)).Times(0);

  fetch_callback_.Run(base::WrapUnique(new std::string(avatar_policy_1_data_)));

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_EQ(1u, fetched_calls_.size());
  EXPECT_EQ(kRegularUserID, fetched_calls_.front().first);
  EXPECT_EQ(avatar_policy_1_data_, fetched_calls_.front().second);
  ClearObservations();
}

// Verifies that when the external data reference for a regular user is not set
// while the user is logging in, a 'cleared' notifications is emitted. Further
// verifies that when the external data reference is then cleared (which is a
// no-op), no notifications are emitted.
TEST_F(CloudExternalDataPolicyObserverTest, RegularUserClearUnset) {
  CreateObserver();

  EXPECT_CALL(external_data_manager_, Fetch(key::kUserAvatarImage, _)).Times(0);

  LogInAsRegularUser();

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_EQ(1u, cleared_calls_.size());
  EXPECT_TRUE(fetched_calls_.empty());
  ClearObservations();

  Mock::VerifyAndClear(&external_data_manager_);
  EXPECT_CALL(external_data_manager_, Fetch(key::kUserAvatarImage, _)).Times(0);

  SetRegularUserAvatarPolicy("");

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  ClearObservations();
}

// Verifies that when the external data reference for a regular user is set
// while the user is logging in, a corresponding notification is emitted and a
// fetch is started. Further verifies that when the external data reference is
// then cleared, a corresponding notification is emitted and no new fetch is
// started.
TEST_F(CloudExternalDataPolicyObserverTest, RegularUserClearSet) {
  SetRegularUserAvatarPolicy(avatar_policy_1_);

  CreateObserver();

  EXPECT_CALL(external_data_manager_, Fetch(key::kUserAvatarImage, _))
      .Times(1)
      .WillOnce(SaveArg<1>(&fetch_callback_));

  LogInAsRegularUser();

  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  EXPECT_EQ(1u, set_calls_.size());
  EXPECT_EQ(kRegularUserID, set_calls_.front());
  ClearObservations();

  Mock::VerifyAndClear(&external_data_manager_);
  EXPECT_CALL(external_data_manager_, Fetch(key::kUserAvatarImage, _)).Times(0);

  SetRegularUserAvatarPolicy("");

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  EXPECT_EQ(1u, cleared_calls_.size());
  EXPECT_EQ(kRegularUserID, cleared_calls_.front());
  ClearObservations();
}

// Verifies that when the external data reference for a regular user is not set
// while the user is logging in, a 'cleared' notifications is emitted. Further
// verifies that when the external data reference is then set, a corresponding
// notification is emitted and a fetch is started. Also verifies that when the
// fetch eventually succeeds, a notification containing the external data is
// emitted.
TEST_F(CloudExternalDataPolicyObserverTest, RegularUserSetUnset) {
  CreateObserver();

  EXPECT_CALL(external_data_manager_, Fetch(key::kUserAvatarImage, _)).Times(0);

  LogInAsRegularUser();

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_EQ(1u, cleared_calls_.size());
  EXPECT_TRUE(fetched_calls_.empty());
  ClearObservations();

  Mock::VerifyAndClear(&external_data_manager_);
  EXPECT_CALL(external_data_manager_, Fetch(key::kUserAvatarImage, _))
      .Times(1)
      .WillOnce(SaveArg<1>(&fetch_callback_));

  SetRegularUserAvatarPolicy(avatar_policy_1_);

  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  EXPECT_EQ(1u, set_calls_.size());
  EXPECT_EQ(kRegularUserID, set_calls_.front());
  ClearObservations();

  Mock::VerifyAndClear(&external_data_manager_);
  EXPECT_CALL(external_data_manager_, Fetch(key::kUserAvatarImage, _)).Times(0);

  fetch_callback_.Run(base::WrapUnique(new std::string(avatar_policy_1_data_)));

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_EQ(1u, fetched_calls_.size());
  EXPECT_EQ(kRegularUserID, fetched_calls_.front().first);
  EXPECT_EQ(avatar_policy_1_data_, fetched_calls_.front().second);
  ClearObservations();
}

// Verifies that when the external data reference for a regular user is set
// while the user is logging in, a corresponding notification is emitted and a
// fetch is started. Further verifies that when the external data reference is
// then updated, a corresponding notification is emitted and the fetch is
// restarted. Also verifies that when the fetch eventually succeeds, a
// notification containing the external data is emitted.
TEST_F(CloudExternalDataPolicyObserverTest, RegularUserSetSet) {
  SetRegularUserAvatarPolicy(avatar_policy_1_);

  CreateObserver();

  EXPECT_CALL(external_data_manager_, Fetch(key::kUserAvatarImage, _))
      .Times(1)
      .WillOnce(SaveArg<1>(&fetch_callback_));

  LogInAsRegularUser();

  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  EXPECT_EQ(1u, set_calls_.size());
  EXPECT_EQ(kRegularUserID, set_calls_.front());
  ClearObservations();

  Mock::VerifyAndClear(&external_data_manager_);
  EXPECT_CALL(external_data_manager_, Fetch(key::kUserAvatarImage, _))
      .Times(1)
      .WillOnce(SaveArg<1>(&fetch_callback_));

  SetRegularUserAvatarPolicy(avatar_policy_2_);

  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  EXPECT_EQ(1u, set_calls_.size());
  EXPECT_EQ(kRegularUserID, set_calls_.front());
  ClearObservations();

  Mock::VerifyAndClear(&external_data_manager_);
  EXPECT_CALL(external_data_manager_, Fetch(key::kUserAvatarImage, _)).Times(0);

  fetch_callback_.Run(base::WrapUnique(new std::string(avatar_policy_2_data_)));

  EXPECT_TRUE(set_calls_.empty());
  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_EQ(1u, fetched_calls_.size());
  EXPECT_EQ(kRegularUserID, fetched_calls_.front().first);
  EXPECT_EQ(avatar_policy_2_data_, fetched_calls_.front().second);
  ClearObservations();
}

// Tests that if external data reference for a regular user was cleared when
// the user logged out, the notification will still be emitted when the user
// logs back in.
TEST_F(CloudExternalDataPolicyObserverTest, RegularUserLogoutTest) {
  SetRegularUserAvatarPolicy(avatar_policy_1_);
  CreateObserver();

  EXPECT_CALL(external_data_manager_, Fetch(key::kUserAvatarImage, _))
      .Times(1)
      .WillOnce(SaveArg<1>(&fetch_callback_));

  LogInAsRegularUser();

  EXPECT_TRUE(cleared_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  EXPECT_EQ(1u, set_calls_.size());
  EXPECT_EQ(kRegularUserID, set_calls_.front());
  ClearObservations();

  // Now simulate log out the user. Simply reset the external data policy
  // observer.
  RemoveObserver();

  Mock::VerifyAndClear(&external_data_manager_);
  EXPECT_CALL(external_data_manager_, Fetch(key::kUserAvatarImage, _)).Times(0);

  SetRegularUserAvatarPolicy("");

  // Now simulate log back the user.
  CreateObserver();
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
      content::NotificationService::AllSources(),
      content::Details<Profile>(profile_.get()));

  // Test that clear notification is emitted.
  EXPECT_TRUE(set_calls_.empty());
  EXPECT_TRUE(fetched_calls_.empty());
  EXPECT_EQ(1u, cleared_calls_.size());
  EXPECT_EQ(kRegularUserID, cleared_calls_.front());
  ClearObservations();
}

}  // namespace policy

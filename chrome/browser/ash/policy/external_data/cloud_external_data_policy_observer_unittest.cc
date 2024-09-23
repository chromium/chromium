// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/cloud_external_data_policy_observer.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_provider.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/external_data/cloud_external_data_manager_base_test_util.h"
#include "chrome/browser/ash/policy/external_data/device_local_account_external_data_manager.h"
#include "chrome/browser/ash/policy/invalidation/fake_affiliated_invalidation_service_provider.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings_holder.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_utils.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace em = enterprise_management;

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;

namespace policy {

namespace {

const char kDeviceLocalAccount[] = "device_local_account@localhost";

const char kRegularUserID[] = "user@example.com";

const char kAvatar1URL[] = "http://localhost/avatar1.jpg";
const char kAvatar2URL[] = "http://localhost/avatar2.jpg";
const char kInvalidAvatarURL[] = "http//localhost/avatar1.jpg";

void ConstructAvatarPolicy(const std::string& file_name,
                           const std::string& url,
                           std::string* policy_data,
                           std::string* policy) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  ASSERT_TRUE(base::ReadFileToString(
      test_data_dir.Append("chromeos").Append("avatars").Append(file_name),
      policy_data));
  base::JSONWriter::Write(
      test::ConstructExternalDataReference(url, *policy_data), policy);
}

class TestDelegate : public CloudExternalDataPolicyObserver::Delegate {
 public:
  using FetchedCall = std::pair<std::string, std::string>;

  void ClearObservations() {
    set_calls_.clear();
    cleared_calls_.clear();
    fetched_calls_.clear();
  }

  const std::vector<std::string>& set_calls() const { return set_calls_; }
  const std::vector<std::string>& cleared_calls() const {
    return cleared_calls_;
  }
  const std::vector<FetchedCall>& fetched_calls() const {
    return fetched_calls_;
  }

  // CloudExternalDataPolicyObserver::Delegate:
  void OnExternalDataSet(const std::string& policy,
                         const std::string& user_id) override {
    EXPECT_EQ(key::kUserAvatarImage, policy);
    set_calls_.push_back(user_id);
  }

  void OnExternalDataCleared(const std::string& policy,
                             const std::string& user_id) override {
    EXPECT_EQ(key::kUserAvatarImage, policy);
    cleared_calls_.push_back(user_id);
  }
  void OnExternalDataFetched(const std::string& policy,
                             const std::string& user_id,
                             std::unique_ptr<std::string> data,
                             const base::FilePath& file_path) override {
    EXPECT_EQ(key::kUserAvatarImage, policy);
    fetched_calls_.emplace_back(user_id, std::move(*data));
  }
  void RemoveForAccountId(const AccountId& account_id) override {
    NOTIMPLEMENTED();
  }

 private:
  std::vector<std::string> set_calls_;
  std::vector<std::string> cleared_calls_;
  std::vector<FetchedCall> fetched_calls_;
};

}  // namespace

class CloudExternalDataPolicyObserverTest : public ash::DeviceSettingsTestBase {
 public:
  CloudExternalDataPolicyObserverTest();

  CloudExternalDataPolicyObserverTest(
      const CloudExternalDataPolicyObserverTest&) = delete;
  CloudExternalDataPolicyObserverTest& operator=(
      const CloudExternalDataPolicyObserverTest&) = delete;

  ~CloudExternalDataPolicyObserverTest() override;

  // ash::DeviceSettingsTestBase:
  void SetUp() override;
  void TearDown() override;

  void CreateObserver();
  void RemoveObserver();

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
  std::string invalid_avatar_policy_data_;
  std::string avatar_policy_1_;
  std::string avatar_policy_2_;
  std::string invalid_avatar_policy_;

  std::unique_ptr<ash::CrosSettingsHolder> cros_settings_holder_;
  std::unique_ptr<DeviceLocalAccountPolicyService>
      device_local_account_policy_service_;
  FakeAffiliatedInvalidationServiceProvider
      affiliated_invalidation_service_provider_;
  network::TestURLLoaderFactory url_loader_factory_;
  std::unique_ptr<ash::ScopedStubInstallAttributes> install_attributes_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  std::unique_ptr<DeviceLocalAccountPolicyProvider>
      device_local_account_policy_provider_;

  MockCloudExternalDataManager external_data_manager_;
  testing::NiceMock<MockConfigurationPolicyProvider> user_policy_provider_;

  std::unique_ptr<TestingProfile> profile_;

  std::unique_ptr<CloudExternalDataPolicyObserver> observer_;
  raw_ptr<TestDelegate> delegate_ = nullptr;

  ExternalDataFetcher::FetchCallback fetch_callback_;

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      user_manager_{std::make_unique<ash::FakeChromeUserManager>()};

  TestingProfileManager profile_manager_;
  session_manager::SessionManager session_manager_;
};

CloudExternalDataPolicyObserverTest::CloudExternalDataPolicyObserverTest()
    : DeviceSettingsTestBase(/*profile_creation_enabled=*/false),
      device_local_account_user_id_(GenerateDeviceLocalAccountUserId(
          kDeviceLocalAccount,
          DeviceLocalAccountType::kPublicSession)),
      profile_manager_(TestingBrowserProcess::GetGlobal()) {}

CloudExternalDataPolicyObserverTest::~CloudExternalDataPolicyObserverTest() {}

void CloudExternalDataPolicyObserverTest::SetUp() {
  ash::DeviceSettingsTestBase::SetUp();

  ASSERT_TRUE(profile_manager_.SetUp());
  install_attributes_ = std::make_unique<ash::ScopedStubInstallAttributes>();
  shared_url_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &url_loader_factory_);
  cros_settings_holder_ = std::make_unique<ash::CrosSettingsHolder>(
      device_settings_service_.get(),
      TestingBrowserProcess::GetGlobal()->local_state());
  device_local_account_policy_service_ =
      std::make_unique<DeviceLocalAccountPolicyService>(
          &session_manager_client_, device_settings_service_.get(),
          ash::CrosSettings::Get(), &affiliated_invalidation_service_provider_,
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          shared_url_loader_factory_);

  user_policy_provider_.SetDefaultReturns(
      /*is_initialization_complete_return=*/true,
      /*is_first_policy_load_complete_return=*/true);
  user_policy_provider_.Init();

  ConstructAvatarPolicy("avatar1.jpg", kAvatar1URL, &avatar_policy_1_data_,
                        &avatar_policy_1_);
  ConstructAvatarPolicy("avatar2.jpg", kAvatar2URL, &avatar_policy_2_data_,
                        &avatar_policy_2_);
  ConstructAvatarPolicy("avatar1.jpg", kInvalidAvatarURL,
                        &invalid_avatar_policy_data_, &invalid_avatar_policy_);
}

void CloudExternalDataPolicyObserverTest::TearDown() {
  delegate_ = nullptr;
  observer_.reset();
  user_policy_provider_.Shutdown();
  profile_.reset();
  if (device_local_account_policy_provider_) {
    device_local_account_policy_provider_->Shutdown();
    device_local_account_policy_provider_.reset();
  }
  device_local_account_policy_service_->Shutdown();
  device_local_account_policy_service_.reset();
  cros_settings_holder_.reset();
  install_attributes_.reset();
  ash::DeviceSettingsTestBase::TearDown();
}

void CloudExternalDataPolicyObserverTest::CreateObserver() {
  auto delegate = std::make_unique<TestDelegate>();
  delegate_ = delegate.get();
  observer_ = std::make_unique<CloudExternalDataPolicyObserver>(
      ash::CrosSettings::Get(), device_local_account_policy_service_.get(),
      key::kUserAvatarImage, user_manager_.Get(), std::move(delegate));
  observer_->Init();
}

void CloudExternalDataPolicyObserverTest::RemoveObserver() {
  delegate_ = nullptr;
  observer_.reset();
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
      device_policy_->payload().mutable_device_local_accounts()->add_account();
  account->set_account_id(account_id);
  account->set_type(
      em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);
  device_policy_->Build();
  session_manager_client_.set_device_policy(device_policy_->GetBlob());
  ReloadDeviceSettings();
}

void CloudExternalDataPolicyObserverTest::RemoveDeviceLocalAccount(
    const std::string& account_id) {
  em::DeviceLocalAccountsProto* accounts =
      device_policy_->payload().mutable_device_local_accounts();
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
  device_policy_->Build();
  session_manager_client_.set_device_policy(device_policy_->GetBlob());
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
  user_manager::User* user = user_manager_->AddUser(account_id);

  device_local_account_policy_provider_ =
      std::make_unique<DeviceLocalAccountPolicyProvider>(
          account_id.GetUserEmail(), device_local_account_policy_service_.get(),
          DeviceLocalAccountType::kPublicSession);

  PolicyServiceImpl::Providers providers;
  providers.push_back(device_local_account_policy_provider_.get());
  TestingProfile::Builder builder;
  std::unique_ptr<PolicyServiceImpl> policy_service =
      std::make_unique<PolicyServiceImpl>(std::move(providers));
  builder.SetPolicyService(std::move(policy_service));
  builder.SetPath(ash::ProfileHelper::Get()->GetProfilePathByUserIdHash(
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id)));

  profile_ = builder.Build();
  profile_->set_profile_name(account_id.GetUserEmail());

  ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                               profile_.get());
  session_manager_.NotifyUserProfileLoaded(account_id);
}

void CloudExternalDataPolicyObserverTest::SetRegularUserAvatarPolicy(
    const std::string& value) {
  PolicyMap policy_map;
  if (!value.empty()) {
    auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
        value, base::JSON_ALLOW_TRAILING_COMMAS);
    ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;
    ASSERT_TRUE(parsed_json->is_dict());

    policy_map.Set(key::kUserAvatarImage, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                   (*parsed_json).Clone(),
                   external_data_manager_.CreateExternalDataFetcher(
                       key::kUserAvatarImage));
  }
  user_policy_provider_.UpdateChromePolicy(policy_map);
}

void CloudExternalDataPolicyObserverTest::LogInAsRegularUser() {
  AccountId account_id = AccountId::FromUserEmail(kRegularUserID);
  user_manager::User* user = user_manager_->AddUser(account_id);

  PolicyServiceImpl::Providers providers;
  providers.push_back(&user_policy_provider_);
  TestingProfile::Builder builder;
  std::unique_ptr<PolicyServiceImpl> policy_service =
      std::make_unique<PolicyServiceImpl>(std::move(providers));
  builder.SetPolicyService(std::move(policy_service));
  builder.SetPath(ash::ProfileHelper::Get()->GetProfilePathByUserIdHash(
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id)));

  profile_ = builder.Build();
  profile_->set_profile_name(kRegularUserID);

  ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                               profile_.get());
  session_manager_.NotifyUserProfileLoaded(account_id);
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

  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  EXPECT_EQ(1u, delegate_->set_calls().size());
  EXPECT_EQ(device_local_account_user_id_, delegate_->set_calls().front());
  delegate_->ClearObservations();

  EXPECT_TRUE(url_loader_factory_.IsPending(kAvatar1URL));
  url_loader_factory_.AddResponse(kAvatar1URL, avatar_policy_1_data_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_EQ(1u, delegate_->fetched_calls().size());
  EXPECT_EQ(device_local_account_user_id_,
            delegate_->fetched_calls().front().first);
  EXPECT_EQ(avatar_policy_1_data_, delegate_->fetched_calls().front().second);
  delegate_->ClearObservations();

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

  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  EXPECT_EQ(1u, delegate_->set_calls().size());
  EXPECT_EQ(device_local_account_user_id_, delegate_->set_calls().front());
  delegate_->ClearObservations();

  EXPECT_TRUE(url_loader_factory_.IsPending(kAvatar1URL));
  url_loader_factory_.AddResponse(kAvatar1URL, std::string(),
                                  net::HTTP_BAD_REQUEST);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  delegate_->ClearObservations();

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

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  delegate_->ClearObservations();

  EXPECT_EQ(0, url_loader_factory_.NumPending());

  SetDeviceLocalAccountAvatarPolicy(kDeviceLocalAccount, "");
  RefreshDeviceLocalAccountPolicy(broker);

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  delegate_->ClearObservations();

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

  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  EXPECT_EQ(1u, delegate_->set_calls().size());
  EXPECT_EQ(device_local_account_user_id_, delegate_->set_calls().front());
  delegate_->ClearObservations();

  EXPECT_TRUE(url_loader_factory_.IsPending(kAvatar1URL));

  SetDeviceLocalAccountAvatarPolicy(kDeviceLocalAccount, "");
  RefreshDeviceLocalAccountPolicy(broker);

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  EXPECT_EQ(1u, delegate_->cleared_calls().size());
  EXPECT_EQ(device_local_account_user_id_, delegate_->cleared_calls().front());
  delegate_->ClearObservations();

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

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  delegate_->ClearObservations();

  SetDeviceLocalAccountAvatarPolicy(kDeviceLocalAccount, avatar_policy_1_);
  RefreshDeviceLocalAccountPolicy(broker);

  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  EXPECT_EQ(1u, delegate_->set_calls().size());
  EXPECT_EQ(device_local_account_user_id_, delegate_->set_calls().front());
  delegate_->ClearObservations();

  EXPECT_TRUE(url_loader_factory_.IsPending(kAvatar1URL));
  url_loader_factory_.AddResponse(kAvatar1URL, avatar_policy_1_data_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_EQ(1u, delegate_->fetched_calls().size());
  EXPECT_EQ(device_local_account_user_id_,
            delegate_->fetched_calls().front().first);
  EXPECT_EQ(avatar_policy_1_data_, delegate_->fetched_calls().front().second);
  delegate_->ClearObservations();

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

  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  EXPECT_EQ(1u, delegate_->set_calls().size());
  EXPECT_EQ(device_local_account_user_id_, delegate_->set_calls().front());
  delegate_->ClearObservations();

  EXPECT_TRUE(url_loader_factory_.IsPending(kAvatar1URL));

  SetDeviceLocalAccountAvatarPolicy(kDeviceLocalAccount, avatar_policy_2_);
  RefreshDeviceLocalAccountPolicy(broker);

  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  EXPECT_EQ(1u, delegate_->set_calls().size());
  EXPECT_EQ(device_local_account_user_id_, delegate_->set_calls().front());
  delegate_->ClearObservations();

  EXPECT_EQ(1, url_loader_factory_.NumPending());
  EXPECT_TRUE(url_loader_factory_.IsPending(kAvatar2URL));
  url_loader_factory_.AddResponse(kAvatar2URL, avatar_policy_2_data_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_EQ(1u, delegate_->fetched_calls().size());
  EXPECT_EQ(device_local_account_user_id_,
            delegate_->fetched_calls().front().first);
  EXPECT_EQ(avatar_policy_2_data_, delegate_->fetched_calls().front().second);
  delegate_->ClearObservations();

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

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_EQ(1u, delegate_->cleared_calls().size());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  delegate_->ClearObservations();

  SetDeviceLocalAccountAvatarPolicy(kDeviceLocalAccount, avatar_policy_1_);
  RefreshDeviceLocalAccountPolicy(broker);

  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  EXPECT_EQ(1u, delegate_->set_calls().size());
  EXPECT_EQ(device_local_account_user_id_, delegate_->set_calls().front());
  delegate_->ClearObservations();

  EXPECT_TRUE(url_loader_factory_.IsPending(kAvatar1URL));
  url_loader_factory_.AddResponse(kAvatar1URL, avatar_policy_1_data_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_EQ(1u, delegate_->fetched_calls().size());
  EXPECT_EQ(device_local_account_user_id_,
            delegate_->fetched_calls().front().first);
  EXPECT_EQ(avatar_policy_1_data_, delegate_->fetched_calls().front().second);
  delegate_->ClearObservations();

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

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  delegate_->ClearObservations();

  EXPECT_EQ(0, url_loader_factory_.NumPending());

  RemoveDeviceLocalAccount(kDeviceLocalAccount);

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  delegate_->ClearObservations();

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

  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  EXPECT_EQ(1u, delegate_->set_calls().size());
  EXPECT_EQ(device_local_account_user_id_, delegate_->set_calls().front());
  delegate_->ClearObservations();

  EXPECT_TRUE(url_loader_factory_.IsPending(kAvatar1URL));

  RemoveDeviceLocalAccount(kDeviceLocalAccount);

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  EXPECT_EQ(1u, delegate_->cleared_calls().size());
  EXPECT_EQ(device_local_account_user_id_, delegate_->cleared_calls().front());
  delegate_->ClearObservations();

  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// Verifies that when an external data reference is set for a regular user and
// the user logs in, a corresponding notification is emitted and a fetch is
// started. Further verifies that when the fetch succeeds, a notification
// containing the external data is emitted.
TEST_F(CloudExternalDataPolicyObserverTest, RegularUserFetchSuccess) {
  SetRegularUserAvatarPolicy(avatar_policy_1_);

  CreateObserver();

  EXPECT_CALL(external_data_manager_,
              Fetch(key::kUserAvatarImage, std::string(), _))
      .Times(1)
      .WillOnce([&](const std::string& policy, const std::string& field_name,
                    ExternalDataFetcher::FetchCallback callback) {
        fetch_callback_ = std::move(callback);
      });

  LogInAsRegularUser();

  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  EXPECT_EQ(1u, delegate_->set_calls().size());
  EXPECT_EQ(kRegularUserID, delegate_->set_calls().front());
  delegate_->ClearObservations();

  Mock::VerifyAndClear(&external_data_manager_);
  EXPECT_CALL(external_data_manager_,
              Fetch(key::kUserAvatarImage, std::string(), _))
      .Times(0);

  std::move(fetch_callback_)
      .Run(std::make_unique<std::string>(avatar_policy_1_data_),
           base::FilePath());

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_EQ(1u, delegate_->fetched_calls().size());
  EXPECT_EQ(kRegularUserID, delegate_->fetched_calls().front().first);
  EXPECT_EQ(avatar_policy_1_data_, delegate_->fetched_calls().front().second);
  delegate_->ClearObservations();
}

// Verifies that when the external data reference for a regular user is not set
// while the user is logging in, a 'cleared' notifications is emitted. Further
// verifies that when the external data reference is then cleared (which is a
// no-op), no notifications are emitted.
TEST_F(CloudExternalDataPolicyObserverTest, RegularUserClearUnset) {
  CreateObserver();

  EXPECT_CALL(external_data_manager_,
              Fetch(key::kUserAvatarImage, std::string(), _))
      .Times(0);

  LogInAsRegularUser();

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_EQ(1u, delegate_->cleared_calls().size());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  delegate_->ClearObservations();

  Mock::VerifyAndClear(&external_data_manager_);
  EXPECT_CALL(external_data_manager_,
              Fetch(key::kUserAvatarImage, std::string(), _))
      .Times(0);

  SetRegularUserAvatarPolicy("");

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  delegate_->ClearObservations();
}

// Verifies that when the external data reference for a regular user is set
// while the user is logging in, a corresponding notification is emitted and a
// fetch is started. Further verifies that when the external data reference is
// then cleared, a corresponding notification is emitted and no new fetch is
// started.
TEST_F(CloudExternalDataPolicyObserverTest, RegularUserClearSet) {
  SetRegularUserAvatarPolicy(avatar_policy_1_);

  CreateObserver();

  EXPECT_CALL(external_data_manager_,
              Fetch(key::kUserAvatarImage, std::string(), _))
      .Times(1);

  LogInAsRegularUser();

  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  EXPECT_EQ(1u, delegate_->set_calls().size());
  EXPECT_EQ(kRegularUserID, delegate_->set_calls().front());
  delegate_->ClearObservations();

  Mock::VerifyAndClear(&external_data_manager_);
  EXPECT_CALL(external_data_manager_,
              Fetch(key::kUserAvatarImage, std::string(), _))
      .Times(0);

  SetRegularUserAvatarPolicy("");

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  EXPECT_EQ(1u, delegate_->cleared_calls().size());
  EXPECT_EQ(kRegularUserID, delegate_->cleared_calls().front());
  delegate_->ClearObservations();
}

// Verifies that when the external data reference for a regular user is not set
// while the user is logging in, a 'cleared' notifications is emitted. Further
// verifies that when the external data reference is then set, a corresponding
// notification is emitted and a fetch is started. Also verifies that when the
// fetch eventually succeeds, a notification containing the external data is
// emitted.
TEST_F(CloudExternalDataPolicyObserverTest, RegularUserSetUnset) {
  CreateObserver();

  EXPECT_CALL(external_data_manager_,
              Fetch(key::kUserAvatarImage, std::string(), _))
      .Times(0);

  LogInAsRegularUser();

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_EQ(1u, delegate_->cleared_calls().size());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  delegate_->ClearObservations();

  Mock::VerifyAndClear(&external_data_manager_);
  EXPECT_CALL(external_data_manager_,
              Fetch(key::kUserAvatarImage, std::string(), _))
      .Times(1)
      .WillOnce([&](const std::string& policy, const std::string& field_name,
                    ExternalDataFetcher::FetchCallback callback) {
        fetch_callback_ = std::move(callback);
      });

  SetRegularUserAvatarPolicy(avatar_policy_1_);

  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  EXPECT_EQ(1u, delegate_->set_calls().size());
  EXPECT_EQ(kRegularUserID, delegate_->set_calls().front());
  delegate_->ClearObservations();

  Mock::VerifyAndClear(&external_data_manager_);
  EXPECT_CALL(external_data_manager_,
              Fetch(key::kUserAvatarImage, std::string(), _))
      .Times(0);

  std::move(fetch_callback_)
      .Run(std::make_unique<std::string>(avatar_policy_1_data_),
           base::FilePath());

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_EQ(1u, delegate_->fetched_calls().size());
  EXPECT_EQ(kRegularUserID, delegate_->fetched_calls().front().first);
  EXPECT_EQ(avatar_policy_1_data_, delegate_->fetched_calls().front().second);
  delegate_->ClearObservations();
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

  EXPECT_CALL(external_data_manager_,
              Fetch(key::kUserAvatarImage, std::string(), _))
      .Times(1);

  LogInAsRegularUser();

  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  EXPECT_EQ(1u, delegate_->set_calls().size());
  EXPECT_EQ(kRegularUserID, delegate_->set_calls().front());
  delegate_->ClearObservations();

  Mock::VerifyAndClear(&external_data_manager_);
  EXPECT_CALL(external_data_manager_,
              Fetch(key::kUserAvatarImage, std::string(), _))
      .Times(1)
      .WillOnce([&](const std::string& policy, const std::string& field_name,
                    ExternalDataFetcher::FetchCallback callback) {
        fetch_callback_ = std::move(callback);
      });

  SetRegularUserAvatarPolicy(avatar_policy_2_);

  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  EXPECT_EQ(1u, delegate_->set_calls().size());
  EXPECT_EQ(kRegularUserID, delegate_->set_calls().front());
  delegate_->ClearObservations();

  Mock::VerifyAndClear(&external_data_manager_);
  EXPECT_CALL(external_data_manager_,
              Fetch(key::kUserAvatarImage, std::string(), _))
      .Times(0);

  std::move(fetch_callback_)
      .Run(std::make_unique<std::string>(avatar_policy_2_data_),
           base::FilePath());

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_EQ(1u, delegate_->fetched_calls().size());
  EXPECT_EQ(kRegularUserID, delegate_->fetched_calls().front().first);
  EXPECT_EQ(avatar_policy_2_data_, delegate_->fetched_calls().front().second);
  delegate_->ClearObservations();
}

// Tests that if external data reference for a regular user was cleared when
// the user logged out, the notification will still be emitted when the user
// logs back in.
TEST_F(CloudExternalDataPolicyObserverTest, RegularUserLogoutTest) {
  SetRegularUserAvatarPolicy(avatar_policy_1_);
  CreateObserver();

  EXPECT_CALL(external_data_manager_,
              Fetch(key::kUserAvatarImage, std::string(), _))
      .Times(1);

  LogInAsRegularUser();

  EXPECT_TRUE(delegate_->cleared_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  EXPECT_EQ(1u, delegate_->set_calls().size());
  EXPECT_EQ(kRegularUserID, delegate_->set_calls().front());
  delegate_->ClearObservations();

  // Now simulate log out the user. Simply reset the external data policy
  // observer.
  RemoveObserver();

  Mock::VerifyAndClear(&external_data_manager_);
  EXPECT_CALL(external_data_manager_,
              Fetch(key::kUserAvatarImage, std::string(), _))
      .Times(0);

  SetRegularUserAvatarPolicy("");

  // Now simulate log back the user.
  CreateObserver();
  session_manager_.NotifyUserProfileLoaded(
      AccountId::FromUserEmail(kRegularUserID));

  // Test that clear notification is emitted.
  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  EXPECT_EQ(1u, delegate_->cleared_calls().size());
  EXPECT_EQ(kRegularUserID, delegate_->cleared_calls().front());
  delegate_->ClearObservations();
}

// Tests that if an invalid policy (a policy for which
// ExternalDataPolicyHandler::CheckPolicySettings() returns false) is passed
// through to the policy map, only a 'cleared' notifications is emitted.
TEST_F(CloudExternalDataPolicyObserverTest, RegularUserInvalidPolicyTest) {
  SetRegularUserAvatarPolicy(invalid_avatar_policy_);

  CreateObserver();

  EXPECT_CALL(external_data_manager_,
              Fetch(key::kUserAvatarImage, std::string(), _))
      .Times(0);

  LogInAsRegularUser();

  EXPECT_TRUE(delegate_->set_calls().empty());
  EXPECT_TRUE(delegate_->fetched_calls().empty());
  EXPECT_EQ(1u, delegate_->cleared_calls().size());
  EXPECT_EQ(kRegularUserID, delegate_->cleared_calls().front());
  delegate_->ClearObservations();
}

}  // namespace policy

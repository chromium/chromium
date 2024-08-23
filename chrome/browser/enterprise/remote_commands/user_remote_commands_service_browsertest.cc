// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/remote_commands/user_remote_commands_service.h"

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/remote_commands/user_remote_commands_service_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/cloud/cloud_policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/invalidation/invalidation_factory.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/invalidation/test_support/fake_invalidation_listener.h"
#include "components/policy/core/browser/cloud/user_policy_signin_service_base.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/remote_commands_result_waiter.h"
#include "components/policy/test_support/remote_commands_state.h"
#include "components/policy/test_support/signature_provider.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;

namespace em = enterprise_management;

namespace enterprise_commands {

namespace {

constexpr char kTestUser[] = "test@example.com";
constexpr int kCommandId = 1;

struct FeaturesTestParam {
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
};

std::variant<std::unique_ptr<invalidation::InvalidationService>,
             std::unique_ptr<invalidation::InvalidationListener>>
CreateInvalidationServiceForSenderId(std::string fcm_sender_id,
                                     std::string /*project_id*/,
                                     std::string /*log_prefix*/) {
  if (base::FeatureList::IsEnabled(
          invalidation::kInvalidationsWithDirectMessages)) {
    return std::make_unique<invalidation::FakeInvalidationListener>();
  }
  return std::make_unique<invalidation::FakeInvalidationService>();
}

std::unique_ptr<KeyedService> BuildFakeProfileInvalidationProvider(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<invalidation::ProfileInvalidationProvider>(
      std::make_unique<invalidation::ProfileIdentityProvider>(
          IdentityManagerFactory::GetForProfile(profile)),
      base::BindRepeating(&CreateInvalidationServiceForSenderId));
}

}  // namespace

class UserRemoteCommandsServiceTest
    : public PlatformBrowserTest,
      public testing::WithParamInterface<FeaturesTestParam> {
 public:
  UserRemoteCommandsServiceTest() {
    scoped_feature_list_.InitWithFeatures(GetParam().enabled_features,
                                          GetParam().disabled_features);
  }
  ~UserRemoteCommandsServiceTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    PlatformBrowserTest::SetUpOnMainThread();

    test_server_ = std::make_unique<policy::EmbeddedPolicyTestServer>();
    ASSERT_TRUE(test_server_->Start());

    test_server_->policy_storage()->add_managed_user("*");
    test_server_->policy_storage()->set_policy_user(kTestUser);
    test_server_->policy_storage()
        ->signature_provider()
        ->SetUniversalSigningKeys();

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                    test_server_->GetServiceURL().spec());
    policy::ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting();
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    PlatformBrowserTest::CreatedBrowserMainParts(browser_main_parts);
    invalidation::ProfileInvalidationProviderFactory::GetInstance()
        ->RegisterTestingFactory(
            base::BindRepeating(&BuildFakeProfileInvalidationProvider));
  }

  // Mock a signed-in user. This is used by the UserCloudPolicyStore to pass
  // the username to the UserCloudPolicyValidator.
  void CreateIdentityTestEnv() {
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
    identity_test_env_->MakePrimaryAccountAvailable(
        kTestUser, signin::ConsentLevel::kSync);
  }

  policy::UserCloudPolicyManager* InitCloudPolicyManager() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    base::FilePath dest_path =
        g_browser_process->profile_manager()->user_data_dir();
    profile_ = Profile::CreateProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")),
        /*delegate=*/nullptr, Profile::CreateMode::kSynchronous);
#else
    profile_ = chrome_test_utils::GetProfile(this);
#endif
    policy::UserCloudPolicyManager* policy_manager =
        profile()->GetUserCloudPolicyManager();
    policy_manager->Connect(
        g_browser_process->local_state(),
        std::make_unique<policy::CloudPolicyClient>(
            g_browser_process->browser_policy_connector()
                ->device_management_service(),
            g_browser_process->shared_url_loader_factory()));

    return policy_manager;
  }

  // Register the user with fake DM Server.
  void RegisterUser(policy::CloudPolicyClient* client) {
    base::test::TestFuture<void> registered_signal;
    policy::MockCloudPolicyClientObserver observer;
    EXPECT_CALL(observer, OnRegistrationStateChanged(_))
        .WillOnce(InvokeWithoutArgs(
            [&registered_signal]() { registered_signal.SetValue(); }));
    client->AddObserver(&observer);

    ASSERT_FALSE(client->is_registered());
    policy::CloudPolicyClient::RegistrationParameters parameters(
        em::DeviceRegisterRequest::BROWSER,
        em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
    client->Register(parameters, std::string(), "oauth_token_unused");
    EXPECT_TRUE(registered_signal.Wait());

    ::testing::Mock::VerifyAndClearExpectations(&observer);

    client->RemoveObserver(&observer);
    EXPECT_TRUE(client->is_registered());
  }

  // Remote command needs to verify the command with signing key and client id
  // that is received with policy fetch. Here as we skipped the policy fetch
  // in `SetUpOnMainThread`, those information are set directly.
  void SetUpFakePolicyData(policy::UserCloudPolicyManager* policy_manager) {
    auto* store = policy_manager->core()->store();
    auto fake_policy_data = std::make_unique<em::PolicyData>();
    fake_policy_data->set_device_id(
        policy_manager->core()->client()->client_id());
    store->set_policy_data_for_testing(std::move(fake_policy_data));
    store->set_policy_signature_public_key_for_testing(
        test_server_->policy_storage()
            ->signature_provider()
            ->GetCurrentKey()
            ->public_key());
  }

  void SetUpOnMainThread() override {
    policy::BrowserPolicyConnector* connector =
        g_browser_process->browser_policy_connector();
    connector->ScheduleServiceInitialization(0);

    CreateIdentityTestEnv();

    policy::UserCloudPolicyManager* policy_manager = InitCloudPolicyManager();

    // Prevent auto policy fetch after register as we don't need to test that.
    policy_manager->core()->client()->RemoveObserver(
        policy_manager->core()->refresh_scheduler());

    RegisterUser(policy_manager->core()->client());
    SetUpFakePolicyData(policy_manager);
  }

  void TearDownOnMainThread() override {
    identity_test_env_.reset();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    profile_.reset();
#else
    profile_ = nullptr;
#endif
  }

  invalidation::FakeInvalidationService* GetInvalidationServiceForSenderId(
      std::string sender_id) {
    auto* profile_invalidation_provider_factory =
        static_cast<invalidation::ProfileInvalidationProvider*>(
            invalidation::ProfileInvalidationProviderFactory::GetInstance()
                ->GetForProfile(profile()));
    auto invalidation_service_or_listener =
        profile_invalidation_provider_factory->GetInvalidationServiceOrListener(
            std::move(sender_id),
            /*project_id=*/"");
    CHECK(std::holds_alternative<invalidation::InvalidationService*>(
        invalidation_service_or_listener));
    return static_cast<invalidation::FakeInvalidationService*>(
        std::get<invalidation::InvalidationService*>(
            invalidation_service_or_listener));
  }

  void AddPendingRemoteCommand(const em::RemoteCommand& command) {
    test_server_->remote_commands_state()->AddPendingRemoteCommand(command);
  }

  em::RemoteCommandResult WaitForResult(int command_id) {
    return policy::RemoteCommandsResultWaiter(
               test_server_->remote_commands_state(), command_id)
        .WaitAndGetResult();
  }

  Profile* profile() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    return profile_.get();
#else
    return profile_;
#endif
  }

 private:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // For Lacros use non-main profile in these tests.
  std::unique_ptr<Profile> profile_;
#else
  raw_ptr<Profile> profile_;
#endif

  std::unique_ptr<policy::EmbeddedPolicyTestServer> test_server_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(UserRemoteCommandsServiceTest, Success) {
  em::RemoteCommand command;
  command.set_type(em::RemoteCommand_Type_BROWSER_CLEAR_BROWSING_DATA);
  command.set_command_id(kCommandId);
  command.set_payload("{}");
  AddPendingRemoteCommand(command);

  // Initial the `RemoteCommandService`. In real life, this part is handled
  // by UserPolicySigninService which triggered by a real signin process.
  // The initialization will automatically pull pending commands in the end.
  auto* remote_command_service =
      enterprise_commands::UserRemoteCommandsServiceFactory::GetForProfile(
          profile());
  remote_command_service->Init();

  em::RemoteCommandResult result = WaitForResult(kCommandId);

  EXPECT_EQ(em::RemoteCommandResult_ResultType_RESULT_SUCCESS, result.result());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    UserRemoteCommandsServiceTest,
    testing::Values(FeaturesTestParam{},
                    FeaturesTestParam{
                        .enabled_features = {
                            invalidation::kInvalidationsWithDirectMessages}}));

}  // namespace enterprise_commands

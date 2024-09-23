// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/cloud/cloud_policy_test_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/invalidation_factory.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/test_support/fake_invalidation_listener.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/cloud/user_policy_signin_service_base.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_settings.pb.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/dbus/constants/dbus_paths.h"  // nogncheck
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#else
#include "chrome/browser/net/system_network_context_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#endif  // !BUILDFLAG(IS_ANDROID)

using testing::_;
using testing::AnyNumber;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Return;

namespace content {
class BrowserContext;
}

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kPolicyInvalidationTopic[] = "test_policy_topic";
constexpr char kPolicyInvalidationType[] = "USER_POLICY_FETCH";

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
  Profile* profile = static_cast<Profile*>(context);
  return std::make_unique<invalidation::ProfileInvalidationProvider>(
      std::make_unique<invalidation::ProfileIdentityProvider>(
          IdentityManagerFactory::GetForProfile(profile)),
      base::BindRepeating(&CreateInvalidationServiceForSenderId));
}

const char* GetTestUser() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return user_manager::kStubUserEmail;
#else
  return "user@example.com";
#endif
}

em::CloudPolicySettings GetTestPolicy(const char* homepage) {
  em::CloudPolicySettings settings;

  em::BooleanPolicyProto* saving_browser_history_disabled =
      settings.mutable_savingbrowserhistorydisabled();
  saving_browser_history_disabled->mutable_policy_options()->set_mode(
      em::PolicyOptions::MANDATORY);
  saving_browser_history_disabled->set_value(true);

  em::IntegerPolicyProto* default_popups_setting =
      settings.mutable_defaultpopupssetting();
  default_popups_setting->mutable_policy_options()->set_mode(
      em::PolicyOptions::MANDATORY);
  default_popups_setting->set_value(4);

  em::StringListPolicyProto* url_blocklist = settings.mutable_urlblocklist();
  url_blocklist->mutable_policy_options()->set_mode(
      em::PolicyOptions::MANDATORY);
  url_blocklist->mutable_value()->add_entries("dev.chromium.org");
  url_blocklist->mutable_value()->add_entries("youtube.com");

  em::StringPolicyProto* default_search_provider_name =
      settings.mutable_defaultsearchprovidername();
  default_search_provider_name->mutable_policy_options()->set_mode(
      em::PolicyOptions::MANDATORY);
  default_search_provider_name->set_value("MyDefaultSearchEngine");

  em::StringPolicyProto* homepage_location =
      settings.mutable_homepagelocation();
  homepage_location->mutable_policy_options()->set_mode(
      em::PolicyOptions::RECOMMENDED);
  homepage_location->set_value(homepage);

  return settings;
}

void GetExpectedTestPolicy(PolicyMap* expected, const char* homepage) {
  GetExpectedDefaultPolicy(expected);

  expected->Set(key::kSavingBrowserHistoryDisabled, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
                nullptr);
  expected->Set(key::kDefaultPopupsSetting, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(4),
                nullptr);
  base::Value::List list;
  list.Append("dev.chromium.org");
  list.Append("youtube.com");
  expected->Set(key::kURLBlocklist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                POLICY_SOURCE_CLOUD, base::Value(std::move(list)), nullptr);
  expected->Set(key::kDefaultSearchProviderName, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                base::Value("MyDefaultSearchEngine"), nullptr);
  expected->Set(key::kHomepageLocation, POLICY_LEVEL_RECOMMENDED,
                POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(homepage),
                nullptr);
}

}  // namespace

// Tests the cloud policy stack(s).
class CloudPolicyTest : public PlatformBrowserTest,
                        public PolicyService::Observer,
                        public testing::WithParamInterface<FeaturesTestParam> {
 protected:
  CloudPolicyTest() {
    scoped_feature_list_.InitWithFeatures(GetParam().enabled_features,
                                          GetParam().disabled_features);
  }
  ~CloudPolicyTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    PlatformBrowserTest::SetUpOnMainThread();

    test_server_ = std::make_unique<EmbeddedPolicyTestServer>();
    ASSERT_TRUE(test_server_->Start());

    ASSERT_NO_FATAL_FAILURE(
        SetServerPolicy(em::CloudPolicySettings(), 1, std::string()));

    std::string url = test_server_->GetServiceURL().spec();

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kDeviceManagementUrl, url);
    ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting();
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    PlatformBrowserTest::CreatedBrowserMainParts(browser_main_parts);
    invalidation::ProfileInvalidationProviderFactory::GetInstance()
        ->RegisterTestingFactory(
            base::BindRepeating(&BuildFakeProfileInvalidationProvider));
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(PolicyServiceIsEmpty(g_browser_process->policy_service()))
        << "Pre-existing policies in this machine will make this test fail.";

    BrowserPolicyConnector* connector =
        g_browser_process->browser_policy_connector();
    connector->ScheduleServiceInitialization(0);

#if BUILDFLAG(IS_CHROMEOS_ASH)
    UserCloudPolicyManagerAsh* policy_manager =
        chrome_test_utils::GetProfile(this)->GetUserCloudPolicyManagerAsh();
    ASSERT_TRUE(policy_manager);
#else
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    base::FilePath dest_path =
        g_browser_process->profile_manager()->user_data_dir();
    profile_ = Profile::CreateProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")), nullptr,
        Profile::CreateMode::kSynchronous);
    Profile* profile = profile_.get();
#else
    Profile* profile = chrome_test_utils::GetProfile(this);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    // Mock a signed-in user. This is used by the UserCloudPolicyStore to pass
    // the username to the UserCloudPolicyValidator.
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
    identity_test_env_->MakePrimaryAccountAvailable(
        GetTestUser(), signin::ConsentLevel::kSync);

    UserCloudPolicyManager* policy_manager =
        profile->GetUserCloudPolicyManager();
    ASSERT_TRUE(policy_manager);
    policy_manager->Connect(
        g_browser_process->local_state(),
        std::make_unique<CloudPolicyClient>(
            connector->device_management_service(),
            g_browser_process->shared_url_loader_factory()));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    ASSERT_TRUE(policy_manager->core()->client());

    // The registration below will trigger a policy refresh (see
    // CloudPolicyRefreshScheduler::OnRegistrationStateChanged). When the tests
    // below call RefreshPolicies(), the first policy request will be cancelled
    // (see CloudPolicyClient::FetchPolicy which will reset
    // |policy_fetch_request_job_|). When the URLLoader implementation sees the
    // SimpleURLLoader going away, it'll cancel it's request as well. This race
    // sometimes causes errors in the Python policy server (|test_server_|).
    // Work around this by removing the refresh scheduler as an observer
    // temporarily.
    policy_manager->core()->client()->RemoveObserver(
        policy_manager->core()->refresh_scheduler());

    base::RunLoop run_loop;
    MockCloudPolicyClientObserver observer;
    EXPECT_CALL(observer, OnRegistrationStateChanged(_))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    policy_manager->core()->client()->AddObserver(&observer);

    // Give a bogus OAuth token to the |policy_manager|. This should make its
    // CloudPolicyClient fetch the DMToken.
    ASSERT_FALSE(policy_manager->core()->client()->is_registered());
    CloudPolicyClient::RegistrationParameters parameters(
#if BUILDFLAG(IS_CHROMEOS_ASH)
        em::DeviceRegisterRequest::USER,
#else
        em::DeviceRegisterRequest::BROWSER,
#endif
        em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
    policy_manager->core()->client()->Register(
        parameters, std::string() /* client_id */,
        "oauth_token_unused" /* oauth_token */);
    run_loop.Run();
    Mock::VerifyAndClearExpectations(&observer);
    policy_manager->core()->client()->RemoveObserver(&observer);
    EXPECT_TRUE(policy_manager->core()->client()->is_registered());

    // Readd the refresh scheduler as an observer now that the first policy
    // fetch finished.
    policy_manager->core()->client()->AddObserver(
        policy_manager->core()->refresh_scheduler());

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Get the path to the user policy key file.
    base::FilePath user_policy_key_dir;
    ASSERT_TRUE(base::PathService::Get(
        chromeos::dbus_paths::DIR_USER_POLICY_KEYS, &user_policy_key_dir));
    std::string sanitized_username =
        ash::UserDataAuthClient::GetStubSanitizedUsername(
            cryptohome::CreateAccountIdentifierFromAccountId(
                AccountId::FromUserEmail(GetTestUser())));
    user_policy_key_file_ = user_policy_key_dir.AppendASCII(sanitized_username)
                                .AppendASCII("policy.pub");
#else
    user_policy_key_file_ =
        profile->GetPath().AppendASCII("Policy").AppendASCII("Signing Key");
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  void TearDownOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    profile_.reset();
#endif
    identity_test_env_.reset();
  }

  Profile* profile() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    return profile_.get();
#else
    return chrome_test_utils::GetProfile(this);
#endif
  }

  PolicyService* GetPolicyService() {
    return profile()->GetProfilePolicyConnector()->policy_service();
  }

  void FirePolicyInvalidation() {
    const base::TimeDelta now =
        base::Time::NowFromSystemTime() - base::Time::UnixEpoch();

    std::visit(
        base::Overloaded{
            [now](invalidation::InvalidationService* service) {
              static_cast<invalidation::FakeInvalidationService*>(service)
                  ->EmitInvalidationForTest(invalidation::Invalidation(
                      kPolicyInvalidationTopic, now.InMicroseconds(),
                      "payload"));
            },
            [now](invalidation::InvalidationListener* listener) {
              static_cast<invalidation::FakeInvalidationListener*>(listener)
                  ->FireInvalidation(invalidation::DirectInvalidation(
                      kPolicyInvalidationType, now.InMicroseconds(),
                      "payload"));
            }},
        // Provider caches invalidation service and listener for sender id and
        // project id. To send an invalidation to the policy invalidator, it
        // must be sent to the correct project id.
        invalidation::ProfileInvalidationProviderFactory::GetInstance()
            ->GetForProfile(profile())
            ->GetInvalidationServiceOrListener(
                kPolicyFCMInvalidationSenderID,
                invalidation::InvalidationListener::kProjectNumberEnterprise));
  }

  void SetServerPolicy(const em::CloudPolicySettings& settings,
                       int key_version,
                       const std::string& policy_invalidation_topic) {
    test_server_->policy_storage()->SetPolicyPayload(
        dm_protocol::kChromeUserPolicyType, settings.SerializeAsString());

    test_server_->policy_storage()->add_managed_user("*");
    test_server_->policy_storage()->set_policy_user(GetTestUser());
    test_server_->policy_storage()
        ->signature_provider()
        ->set_current_key_version(key_version);
    test_server_->policy_storage()->set_policy_invalidation_topic(
        policy_invalidation_topic);
  }

  void OnPolicyUpdated(const PolicyNamespace& ns,
                       const PolicyMap& previous,
                       const PolicyMap& current) override {
    if (!on_policy_updated_.is_null()) {
      std::move(on_policy_updated_).Run();
    }
  }

  void OnPolicyServiceInitialized(PolicyDomain domain) override {}

  void FlushNonChromeOSStoreIOTasks() {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    base::RunLoop run_loop;
    profile()
        ->GetUserCloudPolicyManager()
        ->user_store()
        ->background_task_runner()
        ->PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                          base::Milliseconds(0));
    run_loop.Run();
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  }

  std::unique_ptr<EmbeddedPolicyTestServer> test_server_;
  base::FilePath user_policy_key_file_;
  base::OnceClosure on_policy_updated_;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // For Lacros use non-main profile in these tests.
  std::unique_ptr<Profile> profile_;
#endif

  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(CloudPolicyTest, FetchPolicy) {
  PolicyService* policy_service = GetPolicyService();
  {
    base::RunLoop run_loop;
    // This does the initial fetch and stores the initial key.
    policy_service->RefreshPolicies(run_loop.QuitClosure(),
                                    PolicyFetchReason::kTest);
    run_loop.Run();
  }

  PolicyMap default_policy;
  GetExpectedDefaultPolicy(&default_policy);
  EXPECT_TRUE(default_policy.Equals(policy_service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))));

  ASSERT_NO_FATAL_FAILURE(SetServerPolicy(GetTestPolicy("google.com"), 1,
                                          kPolicyInvalidationTopic));
  PolicyMap expected;
  GetExpectedTestPolicy(&expected, "google.com");
  {
    base::RunLoop run_loop;
    // This fetches the new policies, using the same key.
    policy_service->RefreshPolicies(run_loop.QuitClosure(),
                                    PolicyFetchReason::kTest);
    run_loop.Run();
  }
  EXPECT_TRUE(expected.Equals(policy_service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// ENTERPRISE_DEFAULT policies only are supported on Chrome OS currently.
IN_PROC_BROWSER_TEST_P(CloudPolicyTest, EnsureDefaultPoliciesSet) {
  PolicyService* policy_service = GetPolicyService();
  {
    base::RunLoop run_loop;
    // This does the initial fetch and stores the initial key.
    policy_service->RefreshPolicies(run_loop.QuitClosure(),
                                    PolicyFetchReason::kTest);
    run_loop.Run();
  }

  PolicyMap default_policy;
  GetExpectedDefaultPolicy(&default_policy);
  // Make sure the expected policy has at least one of the policies we're
  // expecting.
  EXPECT_TRUE(default_policy.GetValue(key::kEasyUnlockAllowed,
                                      base::Value::Type::BOOLEAN));

  // Now make sure that these default policies are actually getting injected.
  EXPECT_TRUE(default_policy.Equals(policy_service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))));
}
#endif

// crbug.com/1230268 not working on Lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_InvalidatePolicy DISABLED_InvalidatePolicy
#else
#define MAYBE_InvalidatePolicy InvalidatePolicy
#endif
IN_PROC_BROWSER_TEST_P(CloudPolicyTest, MAYBE_InvalidatePolicy) {
  PolicyService* policy_service = GetPolicyService();
  policy_service->AddObserver(POLICY_DOMAIN_CHROME, this);

  // Perform the initial fetch.
  ASSERT_NO_FATAL_FAILURE(SetServerPolicy(GetTestPolicy("google.com"), 1,
                                          kPolicyInvalidationTopic));
  {
    base::RunLoop run_loop;
    policy_service->RefreshPolicies(run_loop.QuitClosure(),
                                    PolicyFetchReason::kTest);
    run_loop.Run();
  }

  // Update the homepage in the policy and trigger an invalidation.
  ASSERT_NO_FATAL_FAILURE(SetServerPolicy(GetTestPolicy("youtube.com"), 1,
                                          kPolicyInvalidationTopic));

  FirePolicyInvalidation();

  {
    base::RunLoop run_loop;
    on_policy_updated_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Check that the updated policy was fetched.
  PolicyMap expected;
  GetExpectedTestPolicy(&expected, "youtube.com");
  EXPECT_TRUE(expected.Equals(policy_service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))));

  policy_service->RemoveObserver(POLICY_DOMAIN_CHROME, this);
}

IN_PROC_BROWSER_TEST_P(CloudPolicyTest, FetchPolicyWithRotatedKey) {
  PolicyService* policy_service = GetPolicyService();
  {
    base::RunLoop run_loop;
    // This does the initial fetch and stores the initial key.
    policy_service->RefreshPolicies(run_loop.QuitClosure(),
                                    PolicyFetchReason::kTest);
    run_loop.Run();
  }
  // Non-ChromeOS policy stack persist policies in the background thread
  // The code path from RefreshPolicies does not wait for the persistence
  // to complete, unlike in the policy stack on ChromeOS.
  // Flush the tasks to persist the policies.
  FlushNonChromeOSStoreIOTasks();

  // Read the initial key.
  std::string initial_key;
  {
    base::ScopedAllowBlockingForTesting allow_io;
    ASSERT_TRUE(base::ReadFileToString(user_policy_key_file_, &initial_key));
  }

  PolicyMap default_policy;
  GetExpectedDefaultPolicy(&default_policy);
  EXPECT_TRUE(default_policy.Equals(policy_service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))));

  // Set the new policies and a new key at the server.
  ASSERT_NO_FATAL_FAILURE(SetServerPolicy(GetTestPolicy("google.com"), 2,
                                          kPolicyInvalidationTopic));
  PolicyMap expected;
  GetExpectedTestPolicy(&expected, "google.com");
  {
    base::RunLoop run_loop;
    // This fetches the new policies and does a key rotation.
    policy_service->RefreshPolicies(run_loop.QuitClosure(),
                                    PolicyFetchReason::kTest);
    run_loop.Run();
  }
  FlushNonChromeOSStoreIOTasks();
  EXPECT_TRUE(expected.Equals(policy_service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))));

  // Verify that the key was rotated.
  std::string rotated_key;
  {
    base::ScopedAllowBlockingForTesting allow_io;
    ASSERT_TRUE(base::ReadFileToString(user_policy_key_file_, &rotated_key));
  }
  EXPECT_NE(rotated_key, initial_key);

  // Another refresh using the same key won't rotate it again.
  {
    base::RunLoop run_loop;
    policy_service->RefreshPolicies(run_loop.QuitClosure(),
                                    PolicyFetchReason::kTest);
    run_loop.Run();
  }
  FlushNonChromeOSStoreIOTasks();
  EXPECT_TRUE(expected.Equals(policy_service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))));
  std::string current_key;
  {
    base::ScopedAllowBlockingForTesting allow_io;
    ASSERT_TRUE(base::ReadFileToString(user_policy_key_file_, &current_key));
  }

  EXPECT_EQ(rotated_key, current_key);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CloudPolicyTest,
    testing::Values(
        FeaturesTestParam{},
        FeaturesTestParam{.enabled_features =
                              {invalidation::kInvalidationsWithDirectMessages}},
        FeaturesTestParam{.enabled_features = {policy::kPolicyFetchWithSha256}},
        FeaturesTestParam{
            .disabled_features = {policy::kPolicyFetchWithSha256}}));

TEST(CloudPolicyProtoTest, VerifyProtobufEquivalence) {
  // There are 2 protobufs that can be used for user cloud policy:
  // cloud_policy.proto and chrome_settings.proto. chrome_settings.proto is the
  // version used by the server, but generates one proto message per policy; to
  // save binary size on the client, the other version shares proto messages for
  // policies of the same type. They generate the same bytes on the wire though,
  // so they are compatible. This test verifies that that stays true.

  // Build a ChromeSettingsProto message with one policy of each supported type.
  em::ChromeSettingsProto chrome_settings;
  chrome_settings.mutable_homepagelocation()->set_homepagelocation(
      "chromium.org");
  chrome_settings.mutable_savingbrowserhistorydisabled()
      ->set_savingbrowserhistorydisabled(true);
  chrome_settings.mutable_defaultjavascriptsetting()
      ->set_defaultjavascriptsetting(2);
  em::StringList* list = chrome_settings.mutable_synctypeslistdisabled()
                             ->mutable_synctypeslistdisabled();
  list->add_entries("bookmarks");
  list->add_entries("passwords");
  // Try explicitly setting a policy mode too.
  chrome_settings.mutable_searchsuggestenabled()->set_searchsuggestenabled(
      false);
  chrome_settings.mutable_searchsuggestenabled()
      ->mutable_policy_options()
      ->set_mode(em::PolicyOptions::MANDATORY);
  chrome_settings.mutable_syncdisabled()->set_syncdisabled(true);
  chrome_settings.mutable_syncdisabled()->mutable_policy_options()->set_mode(
      em::PolicyOptions::RECOMMENDED);

  // Build an equivalent CloudPolicySettings message.
  em::CloudPolicySettings cloud_policy;
  cloud_policy.mutable_homepagelocation()->set_value("chromium.org");
  cloud_policy.mutable_savingbrowserhistorydisabled()->set_value(true);
  cloud_policy.mutable_defaultjavascriptsetting()->set_value(2);
  list = cloud_policy.mutable_synctypeslistdisabled()->mutable_value();
  list->add_entries("bookmarks");
  list->add_entries("passwords");
  cloud_policy.mutable_searchsuggestenabled()->set_value(false);
  cloud_policy.mutable_searchsuggestenabled()
      ->mutable_policy_options()
      ->set_mode(em::PolicyOptions::MANDATORY);
  cloud_policy.mutable_syncdisabled()->set_value(true);
  cloud_policy.mutable_syncdisabled()->mutable_policy_options()->set_mode(
      em::PolicyOptions::RECOMMENDED);

  // They should now serialize to the same bytes.
  std::string chrome_settings_serialized;
  std::string cloud_policy_serialized;
  ASSERT_TRUE(chrome_settings.SerializeToString(&chrome_settings_serialized));
  ASSERT_TRUE(cloud_policy.SerializeToString(&cloud_policy_serialized));
  EXPECT_EQ(chrome_settings_serialized, cloud_policy_serialized);
}

}  // namespace policy

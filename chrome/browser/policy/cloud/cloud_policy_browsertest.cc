// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/invalidation/deprecated_profile_invalidation_provider_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/cloud/cloud_policy_test_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
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
#include "components/policy/test_support/local_policy_test_server.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_utils.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#else
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#endif

using testing::AnyNumber;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Return;
using testing::_;

namespace content {
class BrowserContext;
}

namespace em = enterprise_management;

namespace policy {

namespace {

std::unique_ptr<KeyedService> BuildFakeProfileInvalidationProvider(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return std::make_unique<invalidation::ProfileInvalidationProvider>(
      std::make_unique<invalidation::FakeInvalidationService>(),
      std::make_unique<invalidation::ProfileIdentityProvider>(
          IdentityManagerFactory::GetForProfile(profile)));
}

const char* GetTestUser() {
#if defined(OS_CHROMEOS)
  return user_manager::kStubUserEmail;
#else
  return "user@example.com";
#endif
}

std::string GetEmptyPolicy() {
  const char kEmptyPolicy[] =
      "{"
      "  \"%s\": {"
      "    \"mandatory\": {},"
      "    \"recommended\": {}"
      "  },"
      "  \"managed_users\": [ \"*\" ],"
      "  \"policy_user\": \"%s\","
      "  \"current_key_index\": 0"
      "}";

  return base::StringPrintf(
      kEmptyPolicy, dm_protocol::kChromeUserPolicyType, GetTestUser());
}

std::string GetTestPolicy(const char* homepage, int key_version) {
  const char kTestPolicy[] =
      "{"
      "  \"%s\": {"
      "    \"mandatory\": {"
      "      \"ShowHomeButton\": true,"
      "      \"RestoreOnStartup\": 4,"
      "      \"URLBlacklist\": [ \"dev.chromium.org\", \"youtube.com\" ],"
      "      \"MaxInvalidationFetchDelay\": 1000"
      "    },"
      "    \"recommended\": {"
      "      \"HomepageLocation\": \"%s\""
      "    }"
      "  },"
      "  \"managed_users\": [ \"*\" ],"
      "  \"policy_user\": \"%s\","
      "  \"current_key_index\": %d,"
      "  \"invalidation_source\": 16,"
      "  \"invalidation_name\": \"test_policy\""
      "}";

  return base::StringPrintf(kTestPolicy,
                            dm_protocol::kChromeUserPolicyType,
                            homepage,
                            GetTestUser(),
                            key_version);
}

void GetExpectedTestPolicy(PolicyMap* expected, const char* homepage) {
  GetExpectedDefaultPolicy(expected);

  expected->Set(key::kShowHomeButton, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(true),
                nullptr);
  expected->Set(key::kRestoreOnStartup, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                std::make_unique<base::Value>(4), nullptr);
  base::ListValue list;
  list.AppendString("dev.chromium.org");
  list.AppendString("youtube.com");
  expected->Set(key::kURLBlacklist, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                POLICY_SOURCE_CLOUD, list.CreateDeepCopy(), nullptr);
  expected->Set(key::kMaxInvalidationFetchDelay, POLICY_LEVEL_MANDATORY,
                POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                std::make_unique<base::Value>(1000), nullptr);
  expected->Set(key::kHomepageLocation, POLICY_LEVEL_RECOMMENDED,
                POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                std::make_unique<base::Value>(homepage), nullptr);
}

}  // namespace

// Tests the cloud policy stack(s).
class CloudPolicyTest : public InProcessBrowserTest,
                        public PolicyService::Observer {
 protected:
  CloudPolicyTest() {}
  ~CloudPolicyTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_NO_FATAL_FAILURE(SetServerPolicy(GetEmptyPolicy()));

    test_server_.reset(new LocalPolicyTestServer(policy_file_path()));
    ASSERT_TRUE(test_server_->Start());

    std::string url = test_server_->GetServiceURL().spec();

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kDeviceManagementUrl, url);

    invalidation::DeprecatedProfileInvalidationProviderFactory::GetInstance()
        ->RegisterTestingFactory(
            base::BindRepeating(&BuildFakeProfileInvalidationProvider));
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(PolicyServiceIsEmpty(g_browser_process->policy_service()))
        << "Pre-existing policies in this machine will make this test fail.";

    BrowserPolicyConnector* connector =
        g_browser_process->browser_policy_connector();
    connector->ScheduleServiceInitialization(0);

#if defined(OS_CHROMEOS)
    UserCloudPolicyManagerChromeOS* policy_manager =
        browser()->profile()->GetUserCloudPolicyManagerChromeOS();
    ASSERT_TRUE(policy_manager);
#else
    // Mock a signed-in user. This is used by the UserCloudPolicyStore to pass
    // the username to the UserCloudPolicyValidator.
    auto* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(identity_manager);
    signin::SetPrimaryAccount(identity_manager, GetTestUser());

    UserCloudPolicyManager* policy_manager =
        browser()->profile()->GetUserCloudPolicyManager();
    ASSERT_TRUE(policy_manager);
    policy_manager->Connect(
        g_browser_process->local_state(),
        UserCloudPolicyManager::CreateCloudPolicyClient(
            connector->device_management_service(),
            g_browser_process->shared_url_loader_factory()));
#endif  // defined(OS_CHROMEOS)

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
    EXPECT_CALL(observer, OnRegistrationStateChanged(_)).WillOnce(
        InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    policy_manager->core()->client()->AddObserver(&observer);

    // Give a bogus OAuth token to the |policy_manager|. This should make its
    // CloudPolicyClient fetch the DMToken.
    ASSERT_FALSE(policy_manager->core()->client()->is_registered());
    CloudPolicyClient::RegistrationParameters parameters(
#if defined(OS_CHROMEOS)
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

#if defined(OS_CHROMEOS)
    // Get the path to the user policy key file.
    base::FilePath user_policy_key_dir;
    ASSERT_TRUE(base::PathService::Get(
        chromeos::dbus_paths::DIR_USER_POLICY_KEYS, &user_policy_key_dir));
    std::string sanitized_username =
        chromeos::CryptohomeClient::GetStubSanitizedUsername(
            cryptohome::CreateAccountIdentifierFromAccountId(
                AccountId::FromUserEmail(GetTestUser())));
    user_policy_key_file_ = user_policy_key_dir.AppendASCII(sanitized_username)
                                               .AppendASCII("policy.pub");
#endif
  }

  PolicyService* GetPolicyService() {
    ProfilePolicyConnector* profile_connector =
        browser()->profile()->GetProfilePolicyConnector();
    return profile_connector->policy_service();
  }

  invalidation::FakeInvalidationService* GetInvalidationService() {
    return static_cast<invalidation::FakeInvalidationService*>(
        static_cast<invalidation::ProfileInvalidationProvider*>(
            invalidation::DeprecatedProfileInvalidationProviderFactory::
                GetInstance()
                    ->GetForProfile(browser()->profile()))
            ->GetInvalidationService());
  }

  void SetServerPolicy(const std::string& policy) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    int result = base::WriteFile(policy_file_path(), policy.data(),
                                 policy.size());
    ASSERT_EQ(base::checked_cast<int>(policy.size()), result);
  }

  base::FilePath policy_file_path() const {
    return temp_dir_.GetPath().AppendASCII("policy.json");
  }

  void OnPolicyUpdated(const PolicyNamespace& ns,
                       const PolicyMap& previous,
                       const PolicyMap& current) override {
    if (!on_policy_updated_.is_null()) {
      on_policy_updated_.Run();
      on_policy_updated_.Reset();
    }
  }

  void OnPolicyServiceInitialized(PolicyDomain domain) override {}

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<LocalPolicyTestServer> test_server_;
  base::FilePath user_policy_key_file_;
  base::Closure on_policy_updated_;
};

IN_PROC_BROWSER_TEST_F(CloudPolicyTest, FetchPolicy) {
  PolicyService* policy_service = GetPolicyService();
  {
    base::RunLoop run_loop;
    // This does the initial fetch and stores the initial key.
    policy_service->RefreshPolicies(run_loop.QuitClosure());
    run_loop.Run();
  }

  PolicyMap default_policy;
  GetExpectedDefaultPolicy(&default_policy);
  EXPECT_TRUE(default_policy.Equals(policy_service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))));

  ASSERT_NO_FATAL_FAILURE(SetServerPolicy(GetTestPolicy("google.com", 0)));
  PolicyMap expected;
  GetExpectedTestPolicy(&expected, "google.com");
  {
    base::RunLoop run_loop;
    // This fetches the new policies, using the same key.
    policy_service->RefreshPolicies(run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_TRUE(expected.Equals(policy_service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))));
}

#if defined(OS_CHROMEOS)
// ENTERPRISE_DEFAULT policies only are supported on Chrome OS currently.
IN_PROC_BROWSER_TEST_F(CloudPolicyTest, EnsureDefaultPoliciesSet) {
  PolicyService* policy_service = GetPolicyService();
  {
    base::RunLoop run_loop;
    // This does the initial fetch and stores the initial key.
    policy_service->RefreshPolicies(run_loop.QuitClosure());
    run_loop.Run();
  }

  PolicyMap default_policy;
  GetExpectedDefaultPolicy(&default_policy);
  // Make sure the expected policy has at least one of the policies we're
  // expecting.
  EXPECT_TRUE(default_policy.GetValue(key::kEasyUnlockAllowed));

  // Now make sure that these default policies are actually getting injected.
  EXPECT_TRUE(default_policy.Equals(policy_service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))));
}
#endif

IN_PROC_BROWSER_TEST_F(CloudPolicyTest, InvalidatePolicy) {
  PolicyService* policy_service = GetPolicyService();
  policy_service->AddObserver(POLICY_DOMAIN_CHROME, this);

  // Perform the initial fetch.
  ASSERT_NO_FATAL_FAILURE(SetServerPolicy(GetTestPolicy("google.com", 0)));
  {
    base::RunLoop run_loop;
    policy_service->RefreshPolicies(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Update the homepage in the policy and trigger an invalidation.
  ASSERT_NO_FATAL_FAILURE(SetServerPolicy(GetTestPolicy("youtube.com", 0)));
  base::TimeDelta now =
      base::Time::NowFromSystemTime() - base::Time::UnixEpoch();
  GetInvalidationService()->EmitInvalidationForTest(
      syncer::Invalidation::Init(
          invalidation::ObjectId(16, "test_policy"),
          now.InMicroseconds() /* version */,
          "payload"));
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

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(CloudPolicyTest, FetchPolicyWithRotatedKey) {
  PolicyService* policy_service = GetPolicyService();
  {
    base::RunLoop run_loop;
    // This does the initial fetch and stores the initial key.
    policy_service->RefreshPolicies(run_loop.QuitClosure());
    run_loop.Run();
  }

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
  ASSERT_NO_FATAL_FAILURE(SetServerPolicy(GetTestPolicy("google.com", 1)));
  PolicyMap expected;
  GetExpectedTestPolicy(&expected, "google.com");
  {
    base::RunLoop run_loop;
    // This fetches the new policies and does a key rotation.
    policy_service->RefreshPolicies(run_loop.QuitClosure());
    run_loop.Run();
  }
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
    policy_service->RefreshPolicies(run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_TRUE(expected.Equals(policy_service->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))));
  std::string current_key;
  {
    base::ScopedAllowBlockingForTesting allow_io;
    ASSERT_TRUE(base::ReadFileToString(user_policy_key_file_, &current_key));
  }
  EXPECT_EQ(rotated_key, current_key);
}
#endif

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
  chrome_settings.mutable_showhomebutton()->set_showhomebutton(true);
  chrome_settings.mutable_restoreonstartup()->set_restoreonstartup(4);
  em::StringList* list =
      chrome_settings.mutable_disabledschemes()->mutable_disabledschemes();
  list->add_entries("ftp");
  list->add_entries("mailto");
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
  cloud_policy.mutable_showhomebutton()->set_value(true);
  cloud_policy.mutable_restoreonstartup()->set_value(4);
  list = cloud_policy.mutable_disabledschemes()->mutable_value();
  list->add_entries("ftp");
  list->add_entries("mailto");
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

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base64url.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "components/policy/proto/chrome_extension_policy.pb.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/local_policy_test_server.h"
#include "components/signin/public/base/signin_metrics.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chromeos/constants/chromeos_switches.h"
#else
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#endif

using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Return;
using testing::_;

namespace em = enterprise_management;

namespace policy {

namespace {

const char kDMToken[] = "dmtoken";
const char kDeviceID[] = "deviceid";

const char kTestExtension[] = "kjmkgkdkpedkejedfhmfcenooemhbpbo";

const base::FilePath::CharType kTestExtensionPath[] =
    FILE_PATH_LITERAL("extensions/managed_extension");

const char kTestPolicy[] =
    "{"
    "  \"Name\": {"
    "    \"Value\": \"disable_all_the_things\""
    "  }"
    "}";

const char kTestExtension2[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";
const base::FilePath::CharType kTestExtension2Path[] =
    FILE_PATH_LITERAL("extensions/managed_extension2");

const char kTestPolicyJSON[] = "{\"Name\":\"disable_all_the_things\"}";

const char kTestPolicy2[] =
    "{"
    "  \"Another\": {"
    "    \"Value\": \"turn_it_off\""
    "  }"
    "}";

const char kTestPolicy2JSON[] = "{\"Another\":\"turn_it_off\"}";

}  // namespace

class ComponentCloudPolicyTest : public extensions::ExtensionBrowserTest {
 protected:
  ComponentCloudPolicyTest() {}
  ~ComponentCloudPolicyTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
#if defined(OS_CHROMEOS)
    // ExtensionBrowserTest sets the login users to a non-managed value;
    // replace it. This is the default username sent in policy blobs from the
    // testserver.
    command_line->AppendSwitchASCII(::chromeos::switches::kLoginUser,
                                    PolicyBuilder::kFakeUsername);
    // Let policy code know that policy is not required to be cached at startup
    // (it can be loaded asynchronously).
    command_line->AppendSwitchASCII(
        ::chromeos::switches::kProfileRequiresPolicy, "false");
#endif
  }

  void SetUpInProcessBrowserTestFixture() override {
    test_server_.RegisterClient(kDMToken, kDeviceID, {} /* state_keys */);
    EXPECT_TRUE(test_server_.UpdatePolicyData(
        dm_protocol::kChromeExtensionPolicyType, kTestExtension, kTestPolicy));
    ASSERT_TRUE(test_server_.Start());

    std::string url = test_server_.GetServiceURL().spec();
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kDeviceManagementUrl, url);

    extensions::ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(PolicyServiceIsEmpty(g_browser_process->policy_service()))
        << "Pre-existing policies in this machine will make this test fail.";

    // Install the initial extension.
    ExtensionTestMessageListener ready_listener("ready", false);
    event_listener_.reset(new ExtensionTestMessageListener("event", true));
    extension_ = LoadExtension(kTestExtensionPath);
    ASSERT_TRUE(extension_.get());
    ASSERT_EQ(kTestExtension, extension_->id());
    EXPECT_TRUE(ready_listener.WaitUntilSatisfied());

    // And start with a signed-in user.
    SignInAndRegister();

    // The extension will receive an update event.
    EXPECT_TRUE(event_listener_->WaitUntilSatisfied());
  }

  void TearDownOnMainThread() override {
    event_listener_.reset();
    extensions::ExtensionBrowserTest::TearDownOnMainThread();
  }

  scoped_refptr<const extensions::Extension> LoadExtension(
      const base::FilePath::CharType* path) {
    base::FilePath full_path;
    if (!base::PathService::Get(chrome::DIR_TEST_DATA, &full_path)) {
      ADD_FAILURE();
      return nullptr;
    }
    scoped_refptr<const extensions::Extension> extension(
        extensions::ExtensionBrowserTest::LoadExtension(
            full_path.Append(path)));
    if (!extension.get()) {
      ADD_FAILURE();
      return nullptr;
    }
    return extension;
  }

  void SignInAndRegister() {
    BrowserPolicyConnector* connector =
        g_browser_process->browser_policy_connector();
    connector->ScheduleServiceInitialization(0);

#if defined(OS_CHROMEOS)
    UserCloudPolicyManagerChromeOS* policy_manager =
        browser()->profile()->GetUserCloudPolicyManagerChromeOS();
    ASSERT_TRUE(policy_manager);
#else
    // Mock a signed-in user. This is used by the UserCloudPolicyStore to pass
    // the account id to the UserCloudPolicyValidator.
    signin::SetPrimaryAccount(
        IdentityManagerFactory::GetForProfile(browser()->profile()),
        PolicyBuilder::kFakeUsername);

    UserCloudPolicyManager* policy_manager =
        browser()->profile()->GetUserCloudPolicyManager();
    ASSERT_TRUE(policy_manager);
    policy_manager->SetSigninAccountId(
        PolicyBuilder::GetFakeAccountIdForTesting());
    policy_manager->Connect(
        g_browser_process->local_state(),
        UserCloudPolicyManager::CreateCloudPolicyClient(
            connector->device_management_service(),
            g_browser_process->shared_url_loader_factory()));

#endif  // defined(OS_CHROMEOS)

    // Register the cloud policy client.
    client_ = policy_manager->core()->client();
    ASSERT_TRUE(client_);
    base::RunLoop run_loop;
    MockCloudPolicyClientObserver observer;
    EXPECT_CALL(observer, OnRegistrationStateChanged(_))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    client_->AddObserver(&observer);
    client_->SetupRegistration(
        kDMToken, kDeviceID,
        std::vector<std::string>() /* user_affiliation_ids */);
    run_loop.Run();
    Mock::VerifyAndClearExpectations(&observer);
    client_->RemoveObserver(&observer);
  }

#if !defined(OS_CHROMEOS)
  void SignOut() {
    auto* primary_account_mutator =
        IdentityManagerFactory::GetForProfile(browser()->profile())
            ->GetPrimaryAccountMutator();
    primary_account_mutator->ClearPrimaryAccount(
        signin::PrimaryAccountMutator::ClearAccountsAction::kDefault,
        signin_metrics::SIGNOUT_TEST,
        signin_metrics::SignoutDelete::IGNORE_METRIC);
  }
#endif

  void RefreshPolicies() {
    ProfilePolicyConnector* profile_connector =
        browser()->profile()->GetProfilePolicyConnector();
    PolicyService* policy_service = profile_connector->policy_service();
    base::RunLoop run_loop;
    policy_service->RefreshPolicies(run_loop.QuitClosure());
    run_loop.Run();
  }

  LocalPolicyTestServer test_server_;
  scoped_refptr<const extensions::Extension> extension_;
  std::unique_ptr<ExtensionTestMessageListener> event_listener_;
  CloudPolicyClient* client_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(ComponentCloudPolicyTest, FetchExtensionPolicy) {
  // Read the initial policy.
  ExtensionTestMessageListener policy_listener(kTestPolicyJSON, false);
  event_listener_->Reply("get-policy-Name");
  EXPECT_TRUE(policy_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ComponentCloudPolicyTest, UpdateExtensionPolicy) {
  // Read the initial policy.
  ExtensionTestMessageListener policy_listener(kTestPolicyJSON, true);
  event_listener_->Reply("get-policy-Name");
  EXPECT_TRUE(policy_listener.WaitUntilSatisfied());

  // Update the policy at the server and reload policy.
  event_listener_.reset(new ExtensionTestMessageListener("event", true));
  policy_listener.Reply("idle");
  EXPECT_TRUE(test_server_.UpdatePolicyData(
      dm_protocol::kChromeExtensionPolicyType, kTestExtension, kTestPolicy2));
  RefreshPolicies();

  // Check that the update event was received, and verify the new policy
  // values.
  EXPECT_TRUE(event_listener_->WaitUntilSatisfied());

  // This policy was removed.
  ExtensionTestMessageListener policy_listener1("{}", true);
  event_listener_->Reply("get-policy-Name");
  EXPECT_TRUE(policy_listener1.WaitUntilSatisfied());

  ExtensionTestMessageListener policy_listener2(kTestPolicy2JSON, false);
  policy_listener1.Reply("get-policy-Another");
  EXPECT_TRUE(policy_listener2.WaitUntilSatisfied());
}

// Flaky on Mac. http://crbug.com/816647
#if defined(OS_MACOSX)
#define MAYBE_InstallNewExtension DISABLED_InstallNewExtension
#else
#define MAYBE_InstallNewExtension InstallNewExtension
#endif
IN_PROC_BROWSER_TEST_F(ComponentCloudPolicyTest, MAYBE_InstallNewExtension) {
  event_listener_->Reply("idle");
  event_listener_.reset();

  EXPECT_TRUE(test_server_.UpdatePolicyData(
      dm_protocol::kChromeExtensionPolicyType, kTestExtension2, kTestPolicy2));
  // Installing a new extension doesn't trigger another policy fetch because
  // the server always sends down the list of all extensions that have policy.
  // Fetch now that the configuration has been updated and before installing
  // the extension.
  RefreshPolicies();

  ExtensionTestMessageListener result_listener("ok", false);
  result_listener.set_failure_message("fail");
  scoped_refptr<const extensions::Extension> extension2 =
      LoadExtension(kTestExtension2Path);
  ASSERT_TRUE(extension2.get());
  ASSERT_EQ(kTestExtension2, extension2->id());

  // This extension only sends the 'policy' signal once it receives the policy,
  // and after verifying it has the expected value. Otherwise it sends 'fail'.
  EXPECT_TRUE(result_listener.WaitUntilSatisfied());
}

// Signing out on Chrome OS is a different process from signing out on the
// Desktop platforms. On Chrome OS the session is ended, and the user goes back
// to the sign-in screen; the Profile data is not affected. On the Desktop the
// session goes on though, and all the signed-in services are disconnected;
// in particular, the policy caches are dropped if the user signs out.
// This test verifies that when the user signs out then any existing component
// policy caches are dropped, and that it's still possible to sign back in and
// get policy for components working again.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ComponentCloudPolicyTest, SignOutAndBackIn) {
  // Read the initial policy.
  ExtensionTestMessageListener initial_policy_listener(kTestPolicyJSON, true);
  event_listener_->Reply("get-policy-Name");
  EXPECT_TRUE(initial_policy_listener.WaitUntilSatisfied());

  // Verify that the policy cache exists.
  std::string cache_key;
  base::Base64UrlEncode(
      "extension-policy", base::Base64UrlEncodePolicy::INCLUDE_PADDING,
      &cache_key);
  std::string cache_subkey;
  base::Base64UrlEncode(
      kTestExtension, base::Base64UrlEncodePolicy::INCLUDE_PADDING,
      &cache_subkey);
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath cache_path = browser()->profile()->GetPath()
      .Append(FILE_PATH_LITERAL("Policy"))
      .Append(FILE_PATH_LITERAL("Components"))
      .AppendASCII(cache_key)
      .AppendASCII(cache_subkey);
  EXPECT_TRUE(base::PathExists(cache_path));

  // Now sign-out. The policy cache should be removed, and the extension should
  // get an empty policy update.
  ExtensionTestMessageListener event_listener("event", true);
  initial_policy_listener.Reply("idle");
  SignOut();
  EXPECT_TRUE(event_listener.WaitUntilSatisfied());

  // The extension got an update event; verify that the policy was empty.
  ExtensionTestMessageListener signout_policy_listener("{}", false);
  event_listener.Reply("get-policy-Name");
  EXPECT_TRUE(signout_policy_listener.WaitUntilSatisfied());

  // Verify that the cache is gone.
  EXPECT_FALSE(base::PathExists(cache_path));

  // Verify that the policy is fetched again if the user signs back in.
  ExtensionTestMessageListener event_listener2("event", true);
  SignInAndRegister();
  EXPECT_TRUE(event_listener2.WaitUntilSatisfied());

  // The extension got updated policy; verify it.
  ExtensionTestMessageListener signin_policy_listener(kTestPolicyJSON, false);
  event_listener2.Reply("get-policy-Name");
  EXPECT_TRUE(signin_policy_listener.WaitUntilSatisfied());

  // And the cache is back.
  EXPECT_TRUE(base::PathExists(cache_path));
}
#endif

// Test of the component cloud policy when the policy test server is configured
// to perform the signing key rotation for each policy fetch.
class KeyRotationComponentCloudPolicyTest : public ComponentCloudPolicyTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    test_server_.EnableAutomaticRotationOfSigningKeys();
    ComponentCloudPolicyTest::SetUpInProcessBrowserTestFixture();
  }

  int GetFetchedPolicyPublicKeyVersion(const std::string& extension_id) {
    const em::PolicyFetchResponse* fetched_policy = client_->GetPolicyFor(
        dm_protocol::kChromeExtensionPolicyType, extension_id);
    if (!fetched_policy || !fetched_policy->has_policy_data())
      return -1;
    em::PolicyData policy_data;
    if (!policy_data.ParseFromString(fetched_policy->policy_data()) ||
        !policy_data.has_public_key_version())
      return -1;
    return policy_data.public_key_version();
  }
};

IN_PROC_BROWSER_TEST_F(KeyRotationComponentCloudPolicyTest, Basic) {
  // Read the initial policy.
  ExtensionTestMessageListener policy_listener(kTestPolicyJSON, true);
  event_listener_->Reply("get-policy-Name");
  EXPECT_TRUE(policy_listener.WaitUntilSatisfied());
  const int public_key_version =
      GetFetchedPolicyPublicKeyVersion(kTestExtension);
  EXPECT_NE(-1, public_key_version);

  // Update the policy at the server and reload the policy, causing also the key
  // rotation to be performed by the policy test server.
  event_listener_.reset(new ExtensionTestMessageListener("event", true));
  policy_listener.Reply("idle");
  EXPECT_TRUE(test_server_.UpdatePolicyData(
      dm_protocol::kChromeExtensionPolicyType, kTestExtension, kTestPolicy2));
  RefreshPolicies();

  // Check that the update event was received, and verify that the policy has
  // the new value and that the key rotation happened.
  EXPECT_TRUE(event_listener_->WaitUntilSatisfied());
  const int new_public_key_version =
      GetFetchedPolicyPublicKeyVersion(kTestExtension);
  EXPECT_LT(public_key_version, new_public_key_version);

  ExtensionTestMessageListener policy_listener1("{}", true);
  event_listener_->Reply("get-policy-Name");
  EXPECT_TRUE(policy_listener1.WaitUntilSatisfied());

  ExtensionTestMessageListener policy_listener2(kTestPolicy2JSON, false);
  policy_listener1.Reply("get-policy-Another");
  EXPECT_TRUE(policy_listener2.WaitUntilSatisfied());
}

}  // namespace policy

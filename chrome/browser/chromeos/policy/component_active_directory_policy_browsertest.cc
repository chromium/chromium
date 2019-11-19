// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/login_manager/policy_descriptor.pb.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/policy_service.h"
#include "components/user_manager/user_names.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr char kTestDomain[] = "test_domain";
constexpr char kTestDeviceId[] = "test_device_id";

constexpr char kTestExtensionId[] = "kjmkgkdkpedkejedfhmfcenooemhbpbo";
constexpr base::FilePath::CharType kTestExtensionPath[] =
    FILE_PATH_LITERAL("extensions/managed_extension");

constexpr char kTestExtension2Id[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";
constexpr base::FilePath::CharType kTestExtension2Path[] =
    FILE_PATH_LITERAL("extensions/managed_extension2");

constexpr char kTestPolicy[] = R"({
  "Policy": {
    "Name": "disable_all_the_things"
  }
})";
constexpr char kTestPolicyJSON[] = R"({"Name":"disable_all_the_things"})";

constexpr char kTestPolicy2[] = R"({
  "Policy": {
    "Another": "turn_it_off"
  }
})";
constexpr char kTestPolicy2JSON[] = R"({"Another":"turn_it_off"})";

constexpr char kEmptyPolicy[] = "{}";

void ExpectSuccess(base::OnceClosure callback, bool result) {
  EXPECT_TRUE(result);
  std::move(callback).Run();
}

}  // namespace

class ComponentActiveDirectoryPolicyTest
    : public extensions::ExtensionBrowserTest {
 protected:
  ComponentActiveDirectoryPolicyTest()
      : install_attributes_(
            chromeos::StubInstallAttributes::CreateActiveDirectoryManaged(
                kTestDomain,
                kTestDeviceId)) {
    builder_.policy_data().set_policy_type(
        dm_protocol::kChromeExtensionPolicyType);
    builder_.policy_data().set_settings_entity_id(kTestExtensionId);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);

    // Log in as Active Directory user.
    command_line->AppendSwitchASCII(::chromeos::switches::kLoginUser,
                                    ::user_manager::kStubAdUserEmail);

    // Without this, user manager code will shut down Chrome since it can't
    // find any policy.
    command_line->AppendSwitchASCII(
        ::chromeos::switches::kAllowFailedPolicyFetchForTest, "true");
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    // Install the initial extension.
    ExtensionTestMessageListener ready_listener("ready", false);
    event_listener_ =
        std::make_unique<ExtensionTestMessageListener>("event", true);
    extension_ = LoadExtension(kTestExtensionPath);
    ASSERT_TRUE(extension_.get());
    ASSERT_EQ(kTestExtensionId, extension_->id());
    EXPECT_TRUE(ready_listener.WaitUntilSatisfied());

    // Store test extension policy
    StorePolicy(kTestExtensionId, kTestPolicy);
    RefreshPolicies();

    // The extension will receive an update event.
    EXPECT_TRUE(event_listener_->WaitUntilSatisfied());
  }

  void TearDownOnMainThread() override {
    event_listener_.reset();
    ExtensionBrowserTest::TearDownOnMainThread();
  }

  scoped_refptr<const extensions::Extension> LoadExtension(
      const base::FilePath::CharType* path) {
    base::FilePath full_path;
    if (!base::PathService::Get(chrome::DIR_TEST_DATA, &full_path)) {
      ADD_FAILURE();
      return nullptr;
    }
    scoped_refptr<const extensions::Extension> extension(
        ExtensionBrowserTest::LoadExtension(full_path.Append(path)));
    if (!extension.get()) {
      ADD_FAILURE();
      return nullptr;
    }
    return extension;
  }

  void StorePolicy(const char* extension_id, const char* policy) {
    const AccountId& account_id = user_manager::StubAdAccountId();
    login_manager::PolicyDescriptor descriptor;
    descriptor.set_account_type(login_manager::ACCOUNT_TYPE_USER);
    descriptor.set_account_id(cryptohome::Identification(account_id).id());
    descriptor.set_domain(login_manager::POLICY_DOMAIN_EXTENSIONS);
    descriptor.set_component_id(extension_id);

    builder_.set_payload(policy);
    builder_.Build();
    base::RunLoop run_loop;
    chromeos::FakeSessionManagerClient::Get()->StorePolicy(
        descriptor, builder_.GetBlob(),
        base::BindOnce(&ExpectSuccess, run_loop.QuitClosure()));
    run_loop.Run();
  }

  void RefreshPolicies() {
    ProfilePolicyConnector* profile_connector =
        browser()->profile()->GetProfilePolicyConnector();
    PolicyService* policy_service = profile_connector->policy_service();
    base::RunLoop run_loop;
    policy_service->RefreshPolicies(run_loop.QuitClosure());
    run_loop.Run();
  }

  scoped_refptr<const extensions::Extension> extension_;
  std::unique_ptr<ExtensionTestMessageListener> event_listener_;
  chromeos::ScopedStubInstallAttributes install_attributes_;
  ComponentActiveDirectoryPolicyBuilder builder_;
};

// Checks policy "Name" is set with expected value.
IN_PROC_BROWSER_TEST_F(ComponentActiveDirectoryPolicyTest,
                       FetchExtensionPolicy) {
  // Read the initial policy.
  ExtensionTestMessageListener policy_listener(kTestPolicyJSON, false);
  event_listener_->Reply("get-policy-Name");
  EXPECT_TRUE(policy_listener.WaitUntilSatisfied());
}

// Uploads "Another" and verifies it. Also verifies that "Name" doesn't exist
// anymore.
IN_PROC_BROWSER_TEST_F(ComponentActiveDirectoryPolicyTest,
                       UpdateExtensionPolicy) {
  // Update and reload policy and make sure that the update event was received.
  event_listener_->Reply("idle");
  event_listener_->Reset();
  StorePolicy(kTestExtensionId, kTestPolicy2);
  RefreshPolicies();
  EXPECT_TRUE(event_listener_->WaitUntilSatisfied());

  // This policy was removed.
  ExtensionTestMessageListener policy_listener1(kEmptyPolicy, true);
  event_listener_->Reply("get-policy-Name");
  EXPECT_TRUE(policy_listener1.WaitUntilSatisfied());

  ExtensionTestMessageListener policy_listener2(kTestPolicy2JSON, false);
  policy_listener1.Reply("get-policy-Another");
  EXPECT_TRUE(policy_listener2.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ComponentActiveDirectoryPolicyTest,
                       InstallNewExtension) {
  event_listener_->Reply("idle");
  event_listener_.reset();

  // Store policy 2 for extension 2.
  StorePolicy(kTestExtension2Id, kTestPolicy2);

  // Installing a new extension should trigger a schema update, which should
  // trigger a policy refresh.
  ExtensionTestMessageListener result_listener("ok", false);
  result_listener.set_failure_message("fail");
  scoped_refptr<const extensions::Extension> extension2 =
      LoadExtension(kTestExtension2Path);
  ASSERT_TRUE(extension2.get());
  ASSERT_EQ(kTestExtension2Id, extension2->id());

  // This extension only sends the 'policy' signal once it receives the policy,
  // and after verifying it has the expected value. Otherwise it sends 'fail'.
  EXPECT_TRUE(result_listener.WaitUntilSatisfied());
}

}  // namespace policy

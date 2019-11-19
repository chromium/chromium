// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/api/management/management_api_constants.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/api/management.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/permission_set.h"

namespace extensions {

namespace {

std::unique_ptr<KeyedService> BuildManagementApi(
    content::BrowserContext* context) {
  return std::make_unique<ManagementAPI>(context);
}

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* profile) {
  return std::make_unique<extensions::EventRouter>(
      profile, ExtensionPrefs::Get(profile));
}

}  // namespace

namespace constants = extension_management_api_constants;

// TODO(devlin): Unittests are awesome. Test more with unittests and less with
// heavy api/browser tests.
class ManagementApiUnitTest : public ExtensionServiceTestWithInstall {
 protected:
  ManagementApiUnitTest() {}
  ~ManagementApiUnitTest() override {}

  // A wrapper around extension_function_test_utils::RunFunction that runs with
  // the associated browser, no flags, and can take stack-allocated arguments.
  bool RunFunction(const scoped_refptr<ExtensionFunction>& function,
                   const base::ListValue& args);

  Browser* browser() { return browser_.get(); }

 private:
  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

  // The browser (and accompanying window).
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;

  DISALLOW_COPY_AND_ASSIGN(ManagementApiUnitTest);
};

bool ManagementApiUnitTest::RunFunction(
    const scoped_refptr<ExtensionFunction>& function,
    const base::ListValue& args) {
  return extension_function_test_utils::RunFunction(
      function.get(), base::WrapUnique(args.DeepCopy()), browser(),
      api_test_utils::NONE);
}

void ManagementApiUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();
  ManagementAPI::GetFactoryInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildManagementApi));

  EventRouterFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildEventRouter));

  browser_window_.reset(new TestBrowserWindow());
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = browser_window_.get();
  browser_.reset(new Browser(params));
}

void ManagementApiUnitTest::TearDown() {
  browser_.reset();
  browser_window_.reset();
  ExtensionServiceTestBase::TearDown();
}

// Test the basic parts of management.setEnabled.
TEST_F(ManagementApiUnitTest, ManagementSetEnabled) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());
  scoped_refptr<const Extension> source_extension =
      ExtensionBuilder("Test").Build();
  service()->AddExtension(source_extension.get());
  std::string extension_id = extension->id();
  scoped_refptr<ManagementSetEnabledFunction> function(
      new ManagementSetEnabledFunction());
  function->set_extension(source_extension);

  base::ListValue disable_args;
  disable_args.AppendString(extension_id);
  disable_args.AppendBoolean(false);

  // Test disabling an (enabled) extension.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
  EXPECT_TRUE(RunFunction(function, disable_args)) << function->GetError();
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));

  base::ListValue enable_args;
  enable_args.AppendString(extension_id);
  enable_args.AppendBoolean(true);

  // Test re-enabling it.
  function = new ManagementSetEnabledFunction();
  EXPECT_TRUE(RunFunction(function, enable_args)) << function->GetError();
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));

  // Test that the enable function checks management policy, so that we can't
  // disable an extension that is required.
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS);
  ManagementPolicy* policy =
      ExtensionSystem::Get(profile())->management_policy();
  policy->RegisterProvider(&provider);

  function = new ManagementSetEnabledFunction();
  EXPECT_FALSE(RunFunction(function, disable_args));
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUserCantModifyError,
                                           extension_id),
            function->GetError());
  policy->UnregisterProvider(&provider);
}

// Test that component extensions cannot be disabled, and that policy extensions
// can be disabled only by component/policy extensions.
TEST_F(ManagementApiUnitTest, ComponentPolicyDisabling) {
  auto component =
      ExtensionBuilder("component").SetLocation(Manifest::COMPONENT).Build();
  auto component2 =
      ExtensionBuilder("component2").SetLocation(Manifest::COMPONENT).Build();
  auto policy =
      ExtensionBuilder("policy").SetLocation(Manifest::EXTERNAL_POLICY).Build();
  auto policy2 = ExtensionBuilder("policy2")
                     .SetLocation(Manifest::EXTERNAL_POLICY)
                     .Build();
  auto internal =
      ExtensionBuilder("internal").SetLocation(Manifest::INTERNAL).Build();

  service()->AddExtension(component.get());
  service()->AddExtension(component2.get());
  service()->AddExtension(policy.get());
  service()->AddExtension(policy2.get());
  service()->AddExtension(internal.get());

  auto extension_can_disable_extension =
      [this](scoped_refptr<const Extension> source_extension,
             scoped_refptr<const Extension> target_extension) {
        std::string id = target_extension->id();
        base::ListValue args;
        args.AppendString(id);
        args.AppendBoolean(false /* disable the extension */);
        auto function = base::MakeRefCounted<ManagementSetEnabledFunction>();
        function->set_extension(source_extension);
        bool did_disable = RunFunction(function, args);
        // If the extension was disabled, re-enable it.
        if (did_disable) {
          EXPECT_TRUE(registry()->disabled_extensions().Contains(id));
          service()->EnableExtension(id);
        } else {
          EXPECT_TRUE(registry()->enabled_extensions().Contains(id));
        }
        return did_disable;
      };

  // Component extension cannot be disabled.
  EXPECT_FALSE(extension_can_disable_extension(component2, component));
  EXPECT_FALSE(extension_can_disable_extension(policy, component));
  EXPECT_FALSE(extension_can_disable_extension(internal, component));

  // Policy extension can be disabled by component/policy extensions, but not
  // others.
  EXPECT_TRUE(extension_can_disable_extension(component, policy));
  EXPECT_TRUE(extension_can_disable_extension(policy2, policy));
  EXPECT_FALSE(extension_can_disable_extension(internal, policy));
}

// Test that policy extensions can be enabled only by component/policy
// extensions.
TEST_F(ManagementApiUnitTest, ComponentPolicyEnabling) {
  auto component =
      ExtensionBuilder("component").SetLocation(Manifest::COMPONENT).Build();
  auto policy =
      ExtensionBuilder("policy").SetLocation(Manifest::EXTERNAL_POLICY).Build();
  auto policy2 = ExtensionBuilder("policy2")
                     .SetLocation(Manifest::EXTERNAL_POLICY)
                     .Build();
  auto internal =
      ExtensionBuilder("internal").SetLocation(Manifest::INTERNAL).Build();

  service()->AddExtension(component.get());
  service()->AddExtension(policy.get());
  service()->AddExtension(policy2.get());
  service()->AddExtension(internal.get());
  service()->DisableExtensionWithSource(
      component.get(), policy->id(), disable_reason::DISABLE_BLOCKED_BY_POLICY);

  auto extension_can_enable_extension =
      [this, component](scoped_refptr<const Extension> source_extension,
                        scoped_refptr<const Extension> target_extension) {
        std::string id = target_extension->id();
        base::ListValue args;
        args.AppendString(id);
        args.AppendBoolean(true /* enable the extension */);
        auto function = base::MakeRefCounted<ManagementSetEnabledFunction>();
        function->set_extension(source_extension);
        bool did_enable = RunFunction(function, args);
        // If the extension was enabled, disable it.
        if (did_enable) {
          EXPECT_TRUE(registry()->enabled_extensions().Contains(id));
          service()->DisableExtensionWithSource(
              component.get(), id, disable_reason::DISABLE_BLOCKED_BY_POLICY);
        } else {
          EXPECT_TRUE(registry()->disabled_extensions().Contains(id));
        }
        return did_enable;
      };

  // Policy extension can be enabled by component/policy extensions, but not
  // others.
  EXPECT_TRUE(extension_can_enable_extension(component, policy));
  EXPECT_TRUE(extension_can_enable_extension(policy2, policy));
  EXPECT_FALSE(extension_can_enable_extension(internal, policy));
}

// Tests management.uninstall.
TEST_F(ManagementApiUnitTest, ManagementUninstall) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());
  std::string extension_id = extension->id();

  base::ListValue uninstall_args;
  uninstall_args.AppendString(extension->id());

  // Auto-accept any uninstalls.
  {
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);

    // Uninstall requires a user gesture, so this should fail.
    scoped_refptr<ExtensionFunction> function(
        new ManagementUninstallFunction());
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    EXPECT_EQ(std::string(constants::kGestureNeededForUninstallError),
              function->GetError());

    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

    function = new ManagementUninstallFunction();
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    // The extension should be uninstalled.
    EXPECT_FALSE(registry()->GetExtensionById(extension_id,
                                              ExtensionRegistry::EVERYTHING));
  }

  // Install the extension again, and try uninstalling, auto-canceling the
  // dialog.
  {
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::CANCEL);
    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

    service()->AddExtension(extension.get());
    scoped_refptr<ExtensionFunction> function =
        new ManagementUninstallFunction();
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    // The uninstall should have failed.
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUninstallCanceledError,
                                             extension_id),
              function->GetError());

    // Try again, using showConfirmDialog: false.
    std::unique_ptr<base::DictionaryValue> options(new base::DictionaryValue());
    options->SetBoolean("showConfirmDialog", false);
    uninstall_args.Append(std::move(options));
    function = new ManagementUninstallFunction();
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    // This should still fail, since extensions can only suppress the dialog for
    // uninstalling themselves.
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUninstallCanceledError,
                                             extension_id),
              function->GetError());

    // If we try uninstall the extension itself, the uninstall should succeed
    // (even though we auto-cancel any dialog), because the dialog is never
    // shown.
    uninstall_args.Remove(0u, nullptr);
    function = new ManagementUninstallSelfFunction();
    function->set_extension(extension);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    EXPECT_FALSE(registry()->GetExtensionById(extension_id,
                                              ExtensionRegistry::EVERYTHING));
  }
}

// Tests management.uninstall from Web Store
TEST_F(ManagementApiUnitTest, ManagementWebStoreUninstall) {
  scoped_refptr<const Extension> triggering_extension =
      ExtensionBuilder("Test").SetID(extensions::kWebStoreAppId).Build();
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());
  std::string extension_id = extension->id();
  base::ListValue uninstall_args;
  uninstall_args.AppendString(extension->id());

  {
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::CANCEL);
    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

    scoped_refptr<ExtensionFunction> function(
        new ManagementUninstallFunction());
    function->set_extension(triggering_extension);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    // Webstore does not suppress the dialog for uninstalling extensions.
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUninstallCanceledError,
                                             extension_id),
              function->GetError());
  }

  {
    scoped_refptr<ExtensionFunction> function(
        new ManagementUninstallFunction());
    function->set_extension(triggering_extension);

    bool did_show = false;
    auto callback = base::BindRepeating(
        [](bool* did_show, extensions::ExtensionUninstallDialog* dialog) {
          EXPECT_EQ("Remove \"Test\"?", dialog->GetHeadingText());
          *did_show = true;
        },
        &did_show);
    extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(
        &callback);

    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);
    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    // The extension should be uninstalled.
    EXPECT_EQ(nullptr, registry()->GetInstalledExtension(extension_id));
    EXPECT_TRUE(did_show);

    // Reset the callback.
    extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(nullptr);
  }
}

// Tests management.uninstall with programmatic uninstall.
TEST_F(ManagementApiUnitTest, ManagementProgrammaticUninstall) {
  scoped_refptr<const Extension> triggering_extension =
      ExtensionBuilder("Triggering Extension").SetID("123").Build();
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());
  std::string extension_id = extension->id();
  base::ListValue uninstall_args;
  uninstall_args.AppendString(extension->id());
  {
    scoped_refptr<ExtensionFunction> function(
        new ManagementUninstallFunction());
    function->set_extension(triggering_extension);

    bool did_show = false;
    auto callback = base::BindRepeating(
        [](bool* did_show, extensions::ExtensionUninstallDialog* dialog) {
          EXPECT_EQ("\"Triggering Extension\" would like to remove \"Test\".",
                    dialog->GetHeadingText());
          *did_show = true;
        },
        &did_show);
    extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(
        &callback);

    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);
    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    // The extension should be uninstalled.
    EXPECT_EQ(nullptr, registry()->GetInstalledExtension(extension_id));
    EXPECT_TRUE(did_show);

    // Reset the callback.
    extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(nullptr);
  }
}
// Tests uninstalling a blacklisted extension via management.uninstall.
TEST_F(ManagementApiUnitTest, ManagementUninstallBlacklisted) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());
  std::string id = extension->id();

  service()->BlacklistExtensionForTest(id);
  EXPECT_NE(nullptr, registry()->GetInstalledExtension(id));

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);
  ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
  scoped_refptr<ExtensionFunction> function(new ManagementUninstallFunction());
  base::ListValue uninstall_args;
  uninstall_args.AppendString(id);
  EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();

  EXPECT_EQ(nullptr, registry()->GetInstalledExtension(id));
}

TEST_F(ManagementApiUnitTest, ManagementEnableOrDisableBlacklisted) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());
  std::string id = extension->id();

  service()->BlacklistExtensionForTest(id);
  EXPECT_NE(nullptr, registry()->GetInstalledExtension(id));

  scoped_refptr<ExtensionFunction> function;

  // Test enabling it.
  {
    base::ListValue enable_args;
    enable_args.AppendString(id);
    enable_args.AppendBoolean(true);
    function = new ManagementSetEnabledFunction();
    EXPECT_TRUE(RunFunction(function, enable_args)) << function->GetError();
    EXPECT_FALSE(registry()->enabled_extensions().Contains(id));
    EXPECT_FALSE(registry()->disabled_extensions().Contains(id));
  }

  // Test disabling it
  {
    base::ListValue disable_args;
    disable_args.AppendString(id);
    disable_args.AppendBoolean(false);

    function = new ManagementSetEnabledFunction();
    EXPECT_TRUE(RunFunction(function, disable_args)) << function->GetError();
    EXPECT_FALSE(registry()->enabled_extensions().Contains(id));
    EXPECT_FALSE(registry()->disabled_extensions().Contains(id));
  }
}

TEST_F(ManagementApiUnitTest, ExtensionInfo_MayEnable) {
  using ExtensionInfo = api::management::ExtensionInfo;

  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());

  const std::string args =
      base::StringPrintf("[\"%s\"]", extension->id().c_str());
  scoped_refptr<ExtensionFunction> function;

  // Initially the extension should show as enabled.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
  {
    function = new ManagementGetFunction();
    std::unique_ptr<base::Value> value(
        extension_function_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), args, browser()));
    ASSERT_TRUE(value);
    std::unique_ptr<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
    ASSERT_TRUE(info);
    EXPECT_TRUE(info->enabled);
    // |may_enable| is only returned for extensions which are not enabled.
    EXPECT_FALSE(info->may_enable.get());
  }

  // Simulate blacklisting the extension and verify that the extension shows as
  // disabled with a false value of |may_enable|.
  ManagementPolicy* policy =
      ExtensionSystem::Get(profile())->management_policy();
  policy->UnregisterAllProviders();
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::PROHIBIT_LOAD);
  policy->RegisterProvider(&provider);
  service()->CheckManagementPolicy();
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  {
    function = new ManagementGetFunction();
    std::unique_ptr<base::Value> value(
        extension_function_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), args, browser()));
    ASSERT_TRUE(value);
    std::unique_ptr<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
    ASSERT_TRUE(info);
    EXPECT_FALSE(info->enabled);
    ASSERT_TRUE(info->may_enable.get());
    EXPECT_FALSE(*(info->may_enable));
  }

  // Re-enable the extension.
  policy->UnregisterAllProviders();
  service()->CheckManagementPolicy();
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));

  // Disable the extension with a normal user action. Verify the extension shows
  // as disabled with |may_enable| as true.
  service()->DisableExtension(extension->id(),
                              disable_reason::DISABLE_USER_ACTION);
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  {
    function = new ManagementGetFunction();
    std::unique_ptr<base::Value> value(
        extension_function_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), args, browser()));
    ASSERT_TRUE(value);
    std::unique_ptr<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
    ASSERT_TRUE(info);
    EXPECT_FALSE(info->enabled);
    ASSERT_TRUE(info->may_enable.get());
    EXPECT_TRUE(*(info->may_enable));
  }
}

TEST_F(ManagementApiUnitTest, ExtensionInfo_MayDisable) {
  using ExtensionInfo = api::management::ExtensionInfo;

  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());

  const std::string args =
      base::StringPrintf("[\"%s\"]", extension->id().c_str());

  // Initially the extension should show as enabled, so it may be disabled
  // freely.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
  {
    scoped_refptr<ExtensionFunction> function = new ManagementGetFunction();
    std::unique_ptr<base::Value> value(
        extension_function_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), args, browser()));
    ASSERT_TRUE(value);
    std::unique_ptr<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
    ASSERT_TRUE(info);
    EXPECT_TRUE(info->enabled);
    EXPECT_TRUE(info->may_disable);
  }

  // Simulate forcing the extension and verify that the extension shows with
  // a false value of |may_disable|.
  ManagementPolicy* policy =
      ExtensionSystem::Get(profile())->management_policy();
  policy->UnregisterAllProviders();
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::MUST_REMAIN_ENABLED);
  policy->RegisterProvider(&provider);
  service()->CheckManagementPolicy();
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
  {
    scoped_refptr<ExtensionFunction> function = new ManagementGetFunction();
    std::unique_ptr<base::Value> value(
        extension_function_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), args, browser()));
    ASSERT_TRUE(value);
    std::unique_ptr<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
    ASSERT_TRUE(info);
    EXPECT_TRUE(info->enabled);
    EXPECT_FALSE(info->may_disable);
  }
}

// Tests enabling an extension via management API after it was disabled due to
// permission increase.
TEST_F(ManagementApiUnitTest, SetEnabledAfterIncreasedPermissions) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");

  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension = PackAndInstallCRX(path, pem_path, INSTALL_NEW);
  // The extension must now be installed and enabled.
  ASSERT_TRUE(extension);
  ASSERT_TRUE(registry()->enabled_extensions().Contains(extension->id()));

  // Save the id, as |extension| will be destroyed during updating.
  std::string extension_id = extension->id();

  std::unique_ptr<const PermissionSet> known_perms =
      prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  // v1 extension doesn't have any permissions.
  EXPECT_TRUE(known_perms->IsEmpty());

  // Update to a new version with increased permissions.
  path = base_path.AppendASCII("v2");
  PackCRXAndUpdateExtension(extension_id, path, pem_path, DISABLED);

  // The extension should be disabled.
  ASSERT_FALSE(registry()->enabled_extensions().Contains(extension_id));

  // Due to a permission increase, prefs will contain escalation information.
  EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));

  auto enable_extension_via_management_api = [this, &web_contents](
      const std::string& extension_id, bool use_user_gesture,
      bool accept_dialog, bool expect_success) {
    ScopedTestDialogAutoConfirm auto_confirm(
        accept_dialog ? ScopedTestDialogAutoConfirm::ACCEPT
                      : ScopedTestDialogAutoConfirm::CANCEL);
    std::unique_ptr<ExtensionFunction::ScopedUserGestureForTests> gesture;
    if (use_user_gesture)
      gesture.reset(new ExtensionFunction::ScopedUserGestureForTests);
    scoped_refptr<ManagementSetEnabledFunction> function(
        new ManagementSetEnabledFunction());
    function->set_browser_context(profile());
    function->SetRenderFrameHost(web_contents->GetMainFrame());
    base::ListValue args;
    args.AppendString(extension_id);
    args.AppendBoolean(true);
    if (expect_success) {
      EXPECT_TRUE(RunFunction(function, args)) << function->GetError();
    } else {
      EXPECT_FALSE(RunFunction(function, args)) << function->GetError();
    }
  };

  // 1) Confirm re-enable prompt without user gesture, expect the extension to
  // stay disabled.
  {
    enable_extension_via_management_api(
        extension_id, false /* use_user_gesture */, true /* accept_dialog */,
        false /* expect_success */);
    EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs should still contain permissions escalation information.
    EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // 2) Deny re-enable prompt without user gesture, expect the extension to stay
  // disabled.
  {
    enable_extension_via_management_api(
        extension_id, false /* use_user_gesture */, false /* accept_dialog */,
        false /* expect_success */);
    EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs should still contain permissions escalation information.
    EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // 3) Deny re-enable prompt with user gesture, expect the extension to stay
  // disabled.
  {
    enable_extension_via_management_api(
        extension_id, true /* use_user_gesture */, false /* accept_dialog */,
        false /* expect_success */);
    EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs should still contain permissions escalation information.
    EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // 4) Accept re-enable prompt with user gesture, expect the extension to be
  // enabled.
  {
    enable_extension_via_management_api(
        extension_id, true /* use_user_gesture */, true /* accept_dialog */,
        true /* expect_success */);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs will no longer contain the escalation information as user has
    // accepted increased permissions.
    EXPECT_FALSE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // Some permissions for v2 extension should be granted by now.
  known_perms = prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  EXPECT_FALSE(known_perms->IsEmpty());
}

}  // namespace extensions

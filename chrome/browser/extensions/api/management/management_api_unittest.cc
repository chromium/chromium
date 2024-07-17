// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/management/management_api.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/background/background_contents.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_delegate_impl.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api/management/management_api_constants.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/api/management.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/permissions/permission_set.h"

using extensions::mojom::ManifestLocation;

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

namespace constants = extension_management_api_constants;

// TODO(devlin): Unittests are awesome. Test more with unittests and less with
// heavy api/browser tests.
class ManagementApiUnitTest : public ExtensionServiceTestWithInstall {
 public:
  ManagementApiUnitTest(const ManagementApiUnitTest&) = delete;
  ManagementApiUnitTest& operator=(const ManagementApiUnitTest&) = delete;

 protected:
  ManagementApiUnitTest() = default;
  ~ManagementApiUnitTest() override = default;

  // A wrapper around api_test_utils::RunFunction that runs with
  // the associated browser, no flags, and can take stack-allocated arguments.
  bool RunFunction(const scoped_refptr<ExtensionFunction>& function,
                   const base::Value::List& args);

  // Runs the management.setEnabled() function to enable an extension.
  bool RunSetEnabledFunction(content::WebContents* web_contents,
                             const ExtensionId& extension_id,
                             bool use_user_gesture,
                             bool accept_dialog,
                             std::string* error,
                             bool enabled = true);

  Browser* browser() { return browser_.get(); }

  // Returns the initialization parameters for the extension service.
  virtual ExtensionServiceInitParams GetExtensionServiceInitParams() {
    return ExtensionServiceInitParams();
  }

  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

 private:
  // This test does not create a root window. Because of this,
  // ScopedDisableRootChecking needs to be used (which disables the root window
  // check).
  test::ScopedDisableRootChecking disable_root_checking_;
  // The browser (and accompanying window).
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;
};

bool ManagementApiUnitTest::RunFunction(
    const scoped_refptr<ExtensionFunction>& function,
    const base::Value::List& args) {
  auto dispatcher = std::make_unique<ExtensionFunctionDispatcher>(profile());
  return api_test_utils::RunFunction(function.get(), args.Clone(),
                                     std::move(dispatcher),
                                     api_test_utils::FunctionMode::kNone);
}

bool ManagementApiUnitTest::RunSetEnabledFunction(
    content::WebContents* web_contents,
    const ExtensionId& extension_id,
    bool use_user_gesture,
    bool accept_dialog,
    std::string* error,
    bool enabled) {
  ScopedTestDialogAutoConfirm auto_confirm(
      accept_dialog ? ScopedTestDialogAutoConfirm::ACCEPT
                    : ScopedTestDialogAutoConfirm::CANCEL);
  std::optional<ExtensionFunction::ScopedUserGestureForTests> gesture =
      std::nullopt;
  if (use_user_gesture)
    gesture.emplace();
  auto function = base::MakeRefCounted<ManagementSetEnabledFunction>();
  if (web_contents)
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  base::Value::List args;
  args.Append(extension_id);
  args.Append(enabled);
  bool result = RunFunction(function, args);
  if (error)
    *error = function->GetError();
  return result;
}

void ManagementApiUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeExtensionService(GetExtensionServiceInitParams());
  ManagementAPI::GetFactoryInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildManagementApi));

  EventRouterFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildEventRouter));

  browser_window_ = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = browser_window_.get();
  browser_.reset(Browser::Create(params));
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
  const ExtensionId& extension_id = extension->id();
  auto function = base::MakeRefCounted<ManagementSetEnabledFunction>();
  function->set_extension(source_extension);

  base::Value::List disable_args;
  disable_args.Append(extension_id);
  disable_args.Append(false);

  // Test disabling an (enabled) extension.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
  EXPECT_TRUE(RunFunction(function, disable_args)) << function->GetError();
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));

  base::Value::List enable_args;
  enable_args.Append(extension_id);
  enable_args.Append(true);

  // Test re-enabling it.
  function = base::MakeRefCounted<ManagementSetEnabledFunction>();
  EXPECT_TRUE(RunFunction(function, enable_args)) << function->GetError();
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));

  // Test that the enable function checks management policy, so that we can't
  // disable an extension that is required.
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS);
  ManagementPolicy* policy =
      ExtensionSystem::Get(profile())->management_policy();
  policy->RegisterProvider(&provider);

  function = base::MakeRefCounted<ManagementSetEnabledFunction>();
  EXPECT_FALSE(RunFunction(function, disable_args));
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUserCantModifyError,
                                           extension_id),
            function->GetError());
  policy->UnregisterProvider(&provider);
}

// Test that component extensions cannot be disabled, and that policy extensions
// can be disabled only by component/policy extensions.
TEST_F(ManagementApiUnitTest, ComponentPolicyDisabling) {
  auto component = ExtensionBuilder("component")
                       .SetLocation(ManifestLocation::kComponent)
                       .Build();
  auto component2 = ExtensionBuilder("component2")
                        .SetLocation(ManifestLocation::kComponent)
                        .Build();
  auto policy = ExtensionBuilder("policy")
                    .SetLocation(ManifestLocation::kExternalPolicy)
                    .Build();
  auto policy2 = ExtensionBuilder("policy2")
                     .SetLocation(ManifestLocation::kExternalPolicy)
                     .Build();
  auto internal = ExtensionBuilder("internal")
                      .SetLocation(ManifestLocation::kInternal)
                      .Build();

  service()->AddExtension(component.get());
  service()->AddExtension(component2.get());
  service()->AddExtension(policy.get());
  service()->AddExtension(policy2.get());
  service()->AddExtension(internal.get());

  auto extension_can_disable_extension =
      [this](scoped_refptr<const Extension> source_extension,
             scoped_refptr<const Extension> target_extension) {
        const ExtensionId& id = target_extension->id();
        base::Value::List args;
        args.Append(id);
        args.Append(false /* disable the extension */);
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
  auto component = ExtensionBuilder("component")
                       .SetLocation(ManifestLocation::kComponent)
                       .Build();
  auto policy = ExtensionBuilder("policy")
                    .SetLocation(ManifestLocation::kExternalPolicy)
                    .Build();
  auto policy2 = ExtensionBuilder("policy2")
                     .SetLocation(ManifestLocation::kExternalPolicy)
                     .Build();
  auto internal = ExtensionBuilder("internal")
                      .SetLocation(ManifestLocation::kInternal)
                      .Build();

  service()->AddExtension(component.get());
  service()->AddExtension(policy.get());
  service()->AddExtension(policy2.get());
  service()->AddExtension(internal.get());
  service()->DisableExtensionWithSource(
      component.get(), policy->id(), disable_reason::DISABLE_BLOCKED_BY_POLICY);

  auto extension_can_enable_extension =
      [this, component](scoped_refptr<const Extension> source_extension,
                        scoped_refptr<const Extension> target_extension) {
        const ExtensionId& id = target_extension->id();
        base::Value::List args;
        args.Append(id);
        args.Append(true /* enable the extension */);
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
  // Note: uninstall calls must come from an extension, WebUI or the Webstore.
  // To test default behavior we test calling from a WebUI context, akin to what
  // we would get from the extension management page.
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());
  const ExtensionId& extension_id = extension->id();

  base::Value::List uninstall_args;
  uninstall_args.Append(extension->id());
  base::HistogramTester tester;

  // Auto-accept any uninstalls.
  {
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);

    // Uninstall requires a user gesture, so this should fail.
    auto function = base::MakeRefCounted<ManagementUninstallFunction>();
    function->set_source_context_type(mojom::ContextType::kWebUi);
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    EXPECT_EQ(std::string(constants::kGestureNeededForUninstallError),
              function->GetError());

    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

    function = base::MakeRefCounted<ManagementUninstallFunction>();
    function->set_source_context_type(mojom::ContextType::kWebUi);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    // The extension should be uninstalled.
    EXPECT_FALSE(registry()->GetExtensionById(extension_id,
                                              ExtensionRegistry::EVERYTHING));
    tester.ExpectBucketCount(
        "Extensions.UninstallSource",
        extensions::UNINSTALL_SOURCE_CHROME_EXTENSIONS_PAGE, 1);
  }

  // Install the extension again, and try uninstalling, auto-canceling the
  // dialog.
  {
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::CANCEL);
    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

    service()->AddExtension(extension.get());
    scoped_refptr<ExtensionFunction> function =
        base::MakeRefCounted<ManagementUninstallFunction>();
    function->set_source_context_type(mojom::ContextType::kWebUi);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    // The uninstall should have failed.
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUninstallCanceledError,
                                             extension_id),
              function->GetError());
    tester.ExpectBucketCount(
        "Extensions.UninstallSource",
        extensions::UNINSTALL_SOURCE_CHROME_EXTENSIONS_PAGE, 2);

    // Try again, using showConfirmDialog: false.
    base::Value::Dict options;
    options.Set("showConfirmDialog", false);
    uninstall_args.Append(std::move(options));
    function = base::MakeRefCounted<ManagementUninstallFunction>();
    function->set_source_context_type(mojom::ContextType::kWebUi);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    // This should still fail, since extensions can only suppress the dialog for
    // uninstalling themselves.
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUninstallCanceledError,
                                             extension_id),
              function->GetError());
    tester.ExpectBucketCount(
        "Extensions.UninstallSource",
        extensions::UNINSTALL_SOURCE_CHROME_EXTENSIONS_PAGE, 3);

    // If we have the extension uninstall itself, the uninstall should succeed
    // (even though we auto-cancel any dialog), because the dialog is never
    // shown.
    uninstall_args.erase(uninstall_args.begin());
    function = base::MakeRefCounted<ManagementUninstallSelfFunction>();
    // Note: this time the source is coming from the extension itself, not a
    // WebUI based context.
    function->set_extension(extension);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    EXPECT_FALSE(registry()->GetExtensionById(extension_id,
                                              ExtensionRegistry::EVERYTHING));
    // Note: No Extensins.UninstallSource bucket is incremented here, as no
    // dialog was shown.
  }
}

// Tests management.uninstall from the Web Store hosted app.
TEST_F(ManagementApiUnitTest, ManagementUninstallWebstoreHostedApp) {
  scoped_refptr<const Extension> triggering_extension =
      ExtensionBuilder("Test").SetID(extensions::kWebStoreAppId).Build();
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());
  const ExtensionId& extension_id = extension->id();
  base::Value::List uninstall_args;
  uninstall_args.Append(extension->id());
  base::HistogramTester tester;

  {
    auto function = base::MakeRefCounted<ManagementUninstallFunction>();
    function->set_extension(triggering_extension);

    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::CANCEL);
    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    // When the dialog is automatically canceled, an error will have been
    // reported to the extension telling it.
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUninstallCanceledError,
                                             extension_id),
              function->GetError());
    tester.ExpectBucketCount("Extensions.UninstallSource",
                             extensions::UNINSTALL_SOURCE_CHROME_WEBSTORE, 1);
  }

  {
    auto function = base::MakeRefCounted<ManagementUninstallFunction>();
    function->set_extension(triggering_extension);

    bool did_show = false;
    auto callback = base::BindRepeating(
        [](bool* did_show) { *did_show = true; }, &did_show);
    extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(
        &callback);

    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);
    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    // The extension should be uninstalled.
    EXPECT_EQ(nullptr, registry()->GetInstalledExtension(extension_id));
    EXPECT_TRUE(did_show);
    tester.ExpectBucketCount("Extensions.UninstallSource",
                             extensions::UNINSTALL_SOURCE_CHROME_WEBSTORE, 2);

    // Reset the callback.
    extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(nullptr);
  }
}

// Tests management.uninstall from the new Webstore domain.
TEST_F(ManagementApiUnitTest, ManagementUninstallNewWebstore) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());
  const ExtensionId& extension_id = extension->id();
  base::Value::List uninstall_args;
  uninstall_args.Append(extension->id());
  base::HistogramTester tester;

  // Note: no triggering extension is set on the ExtensionFunction, but the
  // associated URL should be from the webstore domain.
  auto function = base::MakeRefCounted<ManagementUninstallFunction>();
  function->set_source_url(GURL(extension_urls::GetNewWebstoreLaunchURL()));

  bool did_show = false;
  auto callback =
      base::BindRepeating([](bool* did_show) { *did_show = true; }, &did_show);
  extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(&callback);

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);
  ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

  EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
  // The extension should be uninstalled.
  EXPECT_EQ(nullptr, registry()->GetInstalledExtension(extension_id));
  EXPECT_TRUE(did_show);
  tester.ExpectBucketCount("Extensions.UninstallSource",
                           extensions::UNINSTALL_SOURCE_CHROME_WEBSTORE, 1);

  // Reset the callback.
  extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(nullptr);
}

// Tests management.uninstall from a normal extension, which will create a
// programmatic uninstall dialog that identifies the extension that called it.
TEST_F(ManagementApiUnitTest, ManagementUninstallProgramatic) {
  scoped_refptr<const Extension> triggering_extension =
      ExtensionBuilder("Triggering Extension").SetID("123").Build();
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());
  const ExtensionId& extension_id = extension->id();
  base::Value::List uninstall_args;
  uninstall_args.Append(extension->id());
  base::HistogramTester tester;
  {
    auto function = base::MakeRefCounted<ManagementUninstallFunction>();
    function->set_extension(triggering_extension);

    bool did_show = false;
    auto callback = base::BindRepeating(
        [](bool* did_show) { *did_show = true; }, &did_show);
    extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(
        &callback);

    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);
    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    // The extension should be uninstalled.
    EXPECT_EQ(nullptr, registry()->GetInstalledExtension(extension_id));
    EXPECT_TRUE(did_show);
    tester.ExpectBucketCount("Extensions.UninstallSource",
                             extensions::UNINSTALL_SOURCE_EXTENSION, 1);

    // Reset the callback.
    extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(nullptr);
  }
}
// Tests uninstalling a blocklisted extension via management.uninstall.
TEST_F(ManagementApiUnitTest, ManagementUninstallBlocklisted) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());
  const ExtensionId& id = extension->id();

  service()->BlocklistExtensionForTest(id);
  EXPECT_NE(nullptr, registry()->GetInstalledExtension(id));

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);
  ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
  auto function = base::MakeRefCounted<ManagementUninstallFunction>();
  function->set_source_context_type(mojom::ContextType::kWebUi);
  base::Value::List uninstall_args;
  uninstall_args.Append(id);
  EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();

  EXPECT_EQ(nullptr, registry()->GetInstalledExtension(id));
}

TEST_F(ManagementApiUnitTest, ManagementEnableOrDisableBlocklisted) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());
  const ExtensionId& id = extension->id();

  service()->BlocklistExtensionForTest(id);
  EXPECT_NE(nullptr, registry()->GetInstalledExtension(id));

  // Test enabling it.
  {
    base::Value::List enable_args;
    enable_args.Append(id);
    enable_args.Append(true);
    auto function = base::MakeRefCounted<ManagementSetEnabledFunction>();
    EXPECT_TRUE(RunFunction(function, enable_args)) << function->GetError();
    EXPECT_FALSE(registry()->enabled_extensions().Contains(id));
    EXPECT_FALSE(registry()->disabled_extensions().Contains(id));
  }

  // Test disabling it
  {
    base::Value::List disable_args;
    disable_args.Append(id);
    disable_args.Append(false);

    auto function = base::MakeRefCounted<ManagementSetEnabledFunction>();
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

  // Initially the extension should show as enabled.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
  {
    auto function = base::MakeRefCounted<ManagementGetFunction>();
    std::optional<base::Value> value =
        api_test_utils::RunFunctionAndReturnSingleResult(function.get(), args,
                                                         profile());
    ASSERT_TRUE(value);
    std::optional<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
    ASSERT_TRUE(info);
    EXPECT_TRUE(info->enabled);
    // |may_enable| is only returned for extensions which are not enabled.
    EXPECT_FALSE(info->may_enable);
  }

  // Simulate blocklisting the extension and verify that the extension shows as
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
    auto function = base::MakeRefCounted<ManagementGetFunction>();
    std::optional<base::Value> value =
        api_test_utils::RunFunctionAndReturnSingleResult(function.get(), args,
                                                         profile());
    ASSERT_TRUE(value);
    std::optional<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
    ASSERT_TRUE(info);
    EXPECT_FALSE(info->enabled);
    ASSERT_TRUE(info->may_enable);
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
    auto function = base::MakeRefCounted<ManagementGetFunction>();
    std::optional<base::Value> value =
        api_test_utils::RunFunctionAndReturnSingleResult(function.get(), args,
                                                         profile());
    ASSERT_TRUE(value);
    std::optional<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
    ASSERT_TRUE(info);
    EXPECT_FALSE(info->enabled);
    ASSERT_TRUE(info->may_enable);
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
    auto function = base::MakeRefCounted<ManagementGetFunction>();
    std::optional<base::Value> value =
        api_test_utils::RunFunctionAndReturnSingleResult(function.get(), args,
                                                         profile());
    ASSERT_TRUE(value);
    std::optional<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
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
    auto function = base::MakeRefCounted<ManagementGetFunction>();
    std::optional<base::Value> value =
        api_test_utils::RunFunctionAndReturnSingleResult(function.get(), args,
                                                         profile());
    ASSERT_TRUE(value);
    std::optional<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
    ASSERT_TRUE(info);
    EXPECT_TRUE(info->enabled);
    EXPECT_FALSE(info->may_disable);
  }
}

TEST_F(ManagementApiUnitTest, SetEnabled_UnsupportedRequirements) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  // Install an extension with unsupported requirements.
  base::FilePath base_path = data_dir().AppendASCII("requirements");
  base::FilePath pem_path = base_path.AppendASCII("v1_good.pem");
  base::FilePath path = base_path.AppendASCII("v2_bad_requirements");
  // No WebGL will be the unsupported requirement.
  content::GpuDataManager::GetInstance()->BlocklistWebGLForTesting();
  const Extension* extension =
      PackAndInstallCRX(path, pem_path, INSTALL_WITHOUT_LOAD);

  std::string error;
  bool success = RunSetEnabledFunction(web_contents.get(), extension->id(),
                                       false /* use_user_gesture */,
                                       true /* accept_dialog */, &error);
  EXPECT_FALSE(success);
  EXPECT_EQ(error, "There were missing requirements: WebGL is not supported.");
  EXPECT_FALSE(registry()->enabled_extensions().Contains(extension->id()));
}

// Tests enabling an extension via management API after it was disabled due to
// permission increase.
TEST_F(ManagementApiUnitTest, SetEnabled_IncreasedPermissions) {
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
  ExtensionId extension_id = extension->id();

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

  // 1) Confirm re-enable prompt without user gesture, expect the extension to
  // stay disabled.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents.get(), extension_id,
                                         false /* use_user_gesture */,
                                         true /* accept_dialog */, &error);
    EXPECT_FALSE(success);
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs should still contain permissions escalation information.
    EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // 2) Deny re-enable prompt without user gesture, expect the extension to stay
  // disabled.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents.get(), extension_id,
                                         false /* use_user_gesture */,
                                         false /* accept_dialog */, &error);
    EXPECT_FALSE(success);
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs should still contain permissions escalation information.
    EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // 3) Deny re-enable prompt with user gesture, expect the extension to stay
  // disabled.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents.get(), extension_id,
                                         true /* use_user_gesture */,
                                         false /* accept_dialog */, &error);
    EXPECT_FALSE(success);
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs should still contain permissions escalation information.
    EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // 4) Accept re-enable prompt with user gesture, expect the extension to be
  // enabled.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents.get(), extension_id,
                                         true /* use_user_gesture */,
                                         true /* accept_dialog */, &error);
    EXPECT_TRUE(success) << error;
    EXPECT_TRUE(error.empty());
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

TEST_F(ManagementApiUnitTest,
       SetEnabled_UnsupportedRequirementsAndPermissionsIncrease) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  // Install an extension with unsupported requirements and permissions
  // increase.
  base::FilePath base_path = data_dir().AppendASCII("requirements");
  base::FilePath pem_path = base_path.AppendASCII("v1_good.pem");
  base::FilePath path =
      base_path.AppendASCII("v2_bad_requirements_and_permissions");
  // No WebGL will be the unsupported requirement.
  content::GpuDataManager::GetInstance()->BlocklistWebGLForTesting();
  const Extension* extension =
      PackAndInstallCRX(path, pem_path, INSTALL_WITHOUT_LOAD);

  // Unsupported requirements should fail first.
  std::string error;
  bool success = RunSetEnabledFunction(web_contents.get(), extension->id(),
                                       false /* use_user_gesture */,
                                       true /* accept_dialog */, &error);
  EXPECT_FALSE(success);
  EXPECT_EQ(error, "There were missing requirements: WebGL is not supported.");
  EXPECT_FALSE(registry()->enabled_extensions().Contains(extension->id()));
}

// Test suite for cases where the user is in the "disable with re-enable"
// experiment phase.
class ManagementApiUnitTestMV2DisableWithReEnableUnitTest
    : public ManagementApiUnitTest {
 public:
  ManagementApiUnitTestMV2DisableWithReEnableUnitTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionManifestV2Disabled);
  }
  ~ManagementApiUnitTestMV2DisableWithReEnableUnitTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests the extension is enabled when management.setEnabled is called for
// enabling an extension disabled due to the MV2 deprecation, and user accepted
// the dialog.
TEST_F(ManagementApiUnitTestMV2DisableWithReEnableUnitTest,
       SetEnabled_MV2Deprecation) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  // Install an extension and disable it due to the MV2 deprecation.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test").SetManifestVersion(2).Build();
  const ExtensionId& extension_id = extension->id();
  service()->AddExtension(extension.get());
  service()->DisableExtension(
      extension_id, disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION);

  // 1) Deny re-enable prompt without user gesture, expect the extension to
  // stay disabled.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents.get(), extension_id,
                                         /*use_user_gesture=*/false,
                                         /*accept_dialog=*/false, &error);
    EXPECT_FALSE(success);
    EXPECT_EQ(error,
              "Re-enabling an extension disabled due to MV2 deprecation "
              "requires a user gesture.");
    EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_id));
  }

  // 2) Deny re-enable prompt with user gesture, expect the extension to
  // stay disabled.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents.get(), extension_id,
                                         /*use_user_gesture=*/true,
                                         /*accept_dialog=*/false, &error);
    EXPECT_FALSE(success);
    EXPECT_EQ(error, "The user did not accept the re-enable dialog.");
    EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_id));
  }

  // 3) Accept re-enable prompt without user gesture, expect the extension to
  // stay disabled.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents.get(), extension_id,
                                         /*use_user_gesture=*/false,
                                         /*accept_dialog=*/true, &error);
    EXPECT_FALSE(success);
    EXPECT_EQ(error,
              "Re-enabling an extension disabled due to MV2 deprecation "
              "requires a user gesture.");
    EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_id));
  }

  // 4) Accept re-enable prompt with user gesture, expect the extension to
  // be enabled.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents.get(), extension_id,
                                         /*use_user_gesture=*/true,
                                         /*accept_dialog=*/true, &error);
    EXPECT_TRUE(success);
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
  }
}

// Tests the extension is enabled when management.setEnabled is called for
// enabling an extension disabled due to the MV2 deprecation and with
// permissions increase, and user accepted both dialogs shown.
TEST_F(ManagementApiUnitTestMV2DisableWithReEnableUnitTest,
       SetEnabled_PermissionsIncreaseAndMV2Deprecation) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");
  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension = PackAndInstallCRX(path, pem_path, INSTALL_NEW);

  // Save the id, as `extension` will be destroyed during updating.
  ExtensionId extension_id = extension->id();

  // Update extension to a new version with increased permissions.
  path = base_path.AppendASCII("v2");
  PackCRXAndUpdateExtension(extension_id, path, pem_path, DISABLED);
  EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_id));
  EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));

  // Disable extension due to MV2 deprecation. Since extension is already
  // disabled, this will add another disable reason.
  service()->DisableExtension(
      extension_id, disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION);

  // management.setEnabled will trigger two dialogs (permissions increase and
  // mv2 deprecation). Since we have tested each individually, this test
  // only verifies extension is enabled when both dialogs are accepted.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents.get(), extension_id,
                                         /*use_user_gesture=*/true,
                                         /*accept_dialog=*/true, &error);
    EXPECT_TRUE(success);
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_FALSE(prefs->DidExtensionEscalatePermissions(extension_id));
  }
}

// A delegate that senses when extensions are enabled or disabled.
class TestManagementAPIDelegate : public ManagementAPIDelegate {
 public:
  TestManagementAPIDelegate() = default;
  ~TestManagementAPIDelegate() override = default;

  bool LaunchAppFunctionDelegate(
      const Extension* extension,
      content::BrowserContext* context) const override {
    return false;
  }
  GURL GetFullLaunchURL(const Extension* extension) const override {
    return GURL();
  }
  LaunchType GetLaunchType(const ExtensionPrefs* prefs,
                           const Extension* extension) const override {
    return LaunchType::LAUNCH_TYPE_DEFAULT;
  }
  std::unique_ptr<InstallPromptDelegate> SetEnabledFunctionDelegate(
      content::WebContents* web_contents,
      content::BrowserContext* browser_context,
      const Extension* extension,
      base::OnceCallback<void(bool)> callback) const override {
    return nullptr;
  }
  void EnableExtension(content::BrowserContext* context,
                       const ExtensionId& extension_id) const override {
    ++enable_count_;
  }
  void DisableExtension(
      content::BrowserContext* context,
      const Extension* source_extension,
      const ExtensionId& extension_id,
      disable_reason::DisableReason disable_reason) const override {}
  std::unique_ptr<UninstallDialogDelegate> UninstallFunctionDelegate(
      ManagementUninstallFunctionBase* function,
      const Extension* target_extension,
      bool show_programmatic_uninstall_ui) const override {
    return nullptr;
  }
  bool UninstallExtension(content::BrowserContext* context,
                          const ExtensionId& transient_extension_id,
                          UninstallReason reason,
                          std::u16string* error) const override {
    return true;
  }
  bool CreateAppShortcutFunctionDelegate(
      ManagementCreateAppShortcutFunction* function,
      const Extension* extension,
      std::string* error) const override {
    return true;
  }
  void SetLaunchType(content::BrowserContext* context,
                     const ExtensionId& extension_id,
                     LaunchType launch_type) const override {}

  std::unique_ptr<AppForLinkDelegate> GenerateAppForLinkFunctionDelegate(
      ManagementGenerateAppForLinkFunction* function,
      content::BrowserContext* context,
      const std::string& title,
      const GURL& launch_url) const override {
    return nullptr;
  }
  bool CanContextInstallWebApps(
      content::BrowserContext* context) const override {
    return true;
  }
  void InstallOrLaunchReplacementWebApp(
      content::BrowserContext* context,
      const GURL& web_app_url,
      InstallOrLaunchWebAppCallback callback) const override {}
  GURL GetIconURL(const Extension* extension,
                  int icon_size,
                  ExtensionIconSet::Match match,
                  bool grayscale) const override {
    return GURL();
  }
  GURL GetEffectiveUpdateURL(const extensions::Extension& extension,
                             content::BrowserContext* context) const override {
    return GURL();
  }
  void ShowMv2DeprecationReEnableDialog(
      content::BrowserContext* context,
      content::WebContents* web_contents,
      const extensions::Extension& extension,
      base::OnceCallback<void(bool)> done_callback) const override {}

  // EnableExtension is const, so this is mutable.
  mutable int enable_count_ = 0;
};

// A delegate that allows a child to try to install an extension and tracks
// whether the parent permission dialog would have opened.
class TestSupervisedUserExtensionsDelegate
    : public SupervisedUserExtensionsDelegateImpl {
 public:
  explicit TestSupervisedUserExtensionsDelegate(
      content::BrowserContext* context)
      : SupervisedUserExtensionsDelegateImpl(context) {}
  ~TestSupervisedUserExtensionsDelegate() override = default;

  // SupervisedUserExtensionsDelegate:
  bool IsChild() const override { return true; }

  void RequestToAddExtensionOrShowError(
      const extensions::Extension& extension,
      content::WebContents* contents,
      const gfx::ImageSkia& icon,
      SupervisedUserExtensionParentApprovalEntryPoint
          extension_approval_entry_point,
      ExtensionApprovalDoneCallback extension_approval_callback) override {
    // Preconditions.
    DCHECK(IsChild());
    DCHECK(!IsExtensionAllowedByParent(extension));

    if (CanInstallExtensions()) {
      ShowParentPermissionDialogForExtension(
          extension, contents, std::move(extension_approval_callback), icon);
    } else {
      ShowInstallBlockedByParentDialogForExtension(
          extension, contents,
          ExtensionInstalledBlockedByParentDialogAction::kAdd,
          base::BindOnce(std::move(extension_approval_callback),
                         SupervisedUserExtensionsDelegate::
                             ExtensionApprovalResult::kBlocked));
    }
  }

  void RequestToEnableExtensionOrShowError(
      const extensions::Extension& extension,
      content::WebContents* contents,
      SupervisedUserExtensionParentApprovalEntryPoint
          extension_approval_entry_point,
      ExtensionApprovalDoneCallback extension_approval_callback) override {
    // Preconditions.
    DCHECK(IsChild());
    DCHECK(!IsExtensionAllowedByParent(extension));

    if (CanInstallExtensions()) {
      ShowParentPermissionDialogForExtension(
          extension, contents, std::move(extension_approval_callback),
          gfx::ImageSkia());
    } else {
      ShowInstallBlockedByParentDialogForExtension(
          extension, contents,
          ExtensionInstalledBlockedByParentDialogAction::kEnable,
          base::BindOnce(std::move(extension_approval_callback),
                         SupervisedUserExtensionsDelegate::
                             ExtensionApprovalResult::kBlocked));
    }
  }

  void set_next_parent_permission_dialog_result(
      ExtensionApprovalResult result) {
    dialog_result_ = result;
  }

  int show_dialog_count() const { return show_dialog_count_; }
  int show_block_dialog_count() const { return show_block_dialog_count_; }

 private:
  // Shows a parent permission dialog for |extension| and call |done_callback|
  // when it completes.
  void ShowParentPermissionDialogForExtension(
      const extensions::Extension& extension,
      content::WebContents* contents,
      extensions::SupervisedUserExtensionsDelegate::
          ExtensionApprovalDoneCallback done_callback,
      const gfx::ImageSkia& icon) {
    ++show_dialog_count_;
    std::move(done_callback).Run(dialog_result_);
  }

  // Shows a dialog indicating that |extension| has been blocked and call
  // |done_callback| when it completes.
  void ShowInstallBlockedByParentDialogForExtension(
      const extensions::Extension& extension,
      content::WebContents* contents,
      ExtensionInstalledBlockedByParentDialogAction blocked_action,
      base::OnceClosure done_callback) {
    show_block_dialog_count_++;
    std::move(done_callback).Run();
  }

  ExtensionApprovalResult dialog_result_ = ExtensionApprovalResult::kFailed;
  int show_dialog_count_ = 0;
  int show_block_dialog_count_ = 0;
};

// Whether the extension parental controls are managed by the Family Link
// "Extensions" switch ("Allow child to add extensions without asking for
// permission") or the "Permissions" switch ("Permissions for sites").
enum class ExtensionManagementSwitch : int {
  kManagedByExtensions = 0,
  kManagedByPermissions = 1
};

// Tests for supervised users (child accounts). Supervised users are not allowed
// to install apps or extensions unless their parent approves.
class ManagementApiSupervisedUserTest
    : public ManagementApiUnitTest,
      public ::testing::WithParamInterface<ExtensionManagementSwitch> {
 public:
  ManagementApiSupervisedUserTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    enabled_features.push_back(
        supervised_user::
            kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    if (IsManagedBySwitch(
           ExtensionManagementSwitch::kManagedByExtensions)) {
      enabled_features.push_back(
          supervised_user::
              kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
    } else {
      disabled_features.push_back(
          supervised_user::
              kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ~ManagementApiSupervisedUserTest() override = default;

  // ManagementApiUnitTest:
  ExtensionServiceInitParams GetExtensionServiceInitParams() override {
    ExtensionServiceInitParams params;
    params.profile_is_supervised = true;
    return params;
  }

  supervised_user::SupervisedUserService* GetSupervisedUserService() {
    return SupervisedUserServiceFactory::GetForProfile(profile());
  }

  SupervisedUserExtensionsDelegate* GetSupervisedUserExtensionsDelegate() {
    return supervised_user_delegate_;
  }

  void SetUp() override {
    ManagementApiUnitTest::SetUp();

    // Set up custodians (parents) for the child.
    supervised_user_test_util::AddCustodians(browser()->profile());

    GetSupervisedUserService()->Init();
    // Set the pref to allow the child to request extension install.
    supervised_user_test_util::
        SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), true);

    // Create a WebContents to simulate the Chrome Web Store.
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

    management_api_ = ManagementAPI::GetFactoryInstance()->Get(profile());

    // Install a SupervisedUserExtensionsDelegate to sense the dialog state.
    supervised_user_delegate_ =
        new TestSupervisedUserExtensionsDelegate(profile());
    management_api_->set_supervised_user_extensions_delegate_for_test(
        base::WrapUnique(supervised_user_delegate_.get()));
  }

  bool IsManagedBySwitch(ExtensionManagementSwitch management_switch) {
    return GetParam() == management_switch;
  }

  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<ManagementAPI> management_api_ = nullptr;
  raw_ptr<TestSupervisedUserExtensionsDelegate> supervised_user_delegate_ =
      nullptr;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that locally approved extensions (on the feature release of
// `kEnableSupervisedUserSkipParentApprovalToInstallExtensions`) can be
// enabled by the supervised user. The enabling action does not grant parental
// approval. Prevents regressions to b/336759592.
TEST_P(ManagementApiSupervisedUserTest,
       SetEnabled_SetEnabledForLocallyApprovedExtension) {
  // Preconditions.
  ASSERT_TRUE(profile()->IsChild());
  base::HistogramTester histogram_tester;

  // Install an extension.
  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");
  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension =
      PackAndInstallCRX(path, pem_path, INSTALL_WITHOUT_LOAD);
  ASSERT_TRUE(extension);
  const ExtensionId& extension_id = extension->id();

  bool is_locally_parent_approved = false;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  if (IsManagedBySwitch(ExtensionManagementSwitch::kManagedByExtensions)) {
    // Simulate a local approval grant for this extension.
    base::Value::Dict locally_approved;
    locally_approved.Set(extension_id, true);
    profile()->GetPrefs()->SetDict(
        prefs::kSupervisedUserLocallyParentApprovedExtensions,
        std::move(locally_approved));
    is_locally_parent_approved = true;
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

  // Start with an initially disabled extension.
  ASSERT_TRUE(registry()->disabled_extensions().Contains(extension_id));
  ASSERT_EQ(supervised_user_delegate_->IsExtensionAllowedByParent(*extension),
            is_locally_parent_approved);

  // Try to enable it. If the extension is locally approved (Win/Linux/Mac when
  // managed by "Extensions" switch), the enabling should succeed. Otherwise it
  // should fail due to missing parent approval.
  std::string error;
  bool success = RunSetEnabledFunction(web_contents_.get(), extension_id,
                                       /*use_user_gesture=*/true,
                                       /*accept_dialog=*/true, &error);
  EXPECT_EQ(success, is_locally_parent_approved);
  EXPECT_EQ(error.empty(), is_locally_parent_approved);
  EXPECT_EQ(registry()->enabled_extensions().Contains(extension_id),
            is_locally_parent_approved);
  EXPECT_EQ(
      ExtensionPrefs::Get(profile())->HasDisableReason(
          extension_id, disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED),
      !is_locally_parent_approved);

  int expected_enabled_count = is_locally_parent_approved ? 1 : 0;
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName,
      SupervisedUserExtensionsMetricsRecorder::EnablementState::kEnabled,
      expected_enabled_count);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName,
      expected_enabled_count);
  // The enabling of the extension did not affect its parent approval state on
  // Preferences level. The extensions should not have parent approval granted,
  // even if it's locally approved on the present device.
  EXPECT_FALSE(profile()
                   ->GetPrefs()
                   ->GetDict(prefs::kSupervisedUserApprovedExtensions)
                   .contains(extension_id));
}

TEST_P(ManagementApiSupervisedUserTest, SetEnabled_BlockedByParent) {
  // Preconditions.
  ASSERT_TRUE(profile()->IsChild());

  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");

  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension =
      PackAndInstallCRX(path, pem_path, INSTALL_WITHOUT_LOAD);
  ASSERT_TRUE(extension);
  // The extension should be installed but disabled.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  const ExtensionId& extension_id = extension->id();
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(prefs->HasDisableReason(
      extension_id, disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED));

  // Simulate disabling Permissions for sites, apps and extensions in the
  // testing supervised user service delegate used by the Management API.
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), false);

  // The supervised user trying to enable the extensions
  // should fail because the extension is missing parent approval.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents_.get(), extension_id,
                                         /*use_user_gesture=*/true,
                                         /*accept_dialog=*/true, &error);
    EXPECT_FALSE(success);
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));

    // The block dialog should have been shown if extension parental controls
    // are governed by the "Permissions" switch. It is never shown if they are
    // managed by the "Extensions" switch.
    bool should_be_blocked = IsManagedBySwitch(
         ExtensionManagementSwitch::kManagedByPermissions);
    EXPECT_EQ(supervised_user_delegate_->show_block_dialog_count(),
              should_be_blocked ? 1 : 0);
  }

  // Metrics reporting cannot be tested here, because the current implementation
  // of `TestSupervisedUserExtensionsDelegate` overrides the
  // `ShowInstallBlockedByParentDialogForExtension` method that records the
  // metric in the production code.
}

// Tests enabling an extension via management API after it was disabled due to
// permission increase for supervised users.
// Prevents a regression to crbug/1068660.
TEST_P(ManagementApiSupervisedUserTest, SetEnabled_AfterIncreasedPermissions) {
  // Preconditions.
  ASSERT_TRUE(profile()->IsChild());

  base::HistogramTester histogram_tester;

  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");

  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension =
      PackAndInstallCRX(path, pem_path, INSTALL_WITHOUT_LOAD);
  ASSERT_TRUE(extension);
  // The extension should be installed but disabled pending custodian approval.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  // Save the id, as |extension| will be destroyed during updating.
  const ExtensionId extension_id = extension->id();

  // Simulate parent approval for the extension installation.
  GetSupervisedUserExtensionsDelegate()->AddExtensionApproval(*extension);
  // The extension should be enabled now.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));

  // Should see 1 kApprovalGranted UMA metric.
  histogram_tester.ExpectUniqueSample(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
          kApprovalGranted,
      1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 1);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  std::unique_ptr<const PermissionSet> known_perms =
      prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  // v1 extension doesn't have any permissions.
  EXPECT_TRUE(known_perms->IsEmpty());

  // Update to a new version with increased permissions.
  path = base_path.AppendASCII("v2");
  PackCRXAndUpdateExtension(extension_id, path, pem_path, DISABLED);
  // The extension should be disabled.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));
  // Due to a permission increase, prefs will contain escalation information.
  EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));

  // Accept re-enable prompt with user gesture, expect the extension to be
  // enabled.
  {
    // The supervised user will approve the additional permissions without
    // parent approval.
    std::string error;
    bool success = RunSetEnabledFunction(web_contents_.get(), extension_id,
                                         /*use_user_gesture=*/true,
                                         /*accept_dialog=*/true, &error);
    EXPECT_TRUE(success) << error;
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs will no longer contain the escalation information as the supervised
    // user has accepted the increased permissions.
    EXPECT_FALSE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // Permissions for v2 extension should be granted now.
  known_perms = prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  EXPECT_FALSE(known_perms->IsEmpty());

  // The parent approval dialog should have not appeared.
  EXPECT_EQ(0, supervised_user_delegate_->show_dialog_count());

  // Should see 1 kPermissionsIncreaseGranted UMA metric.
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
          kPermissionsIncreaseGranted,
      1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 2);
}

// Tests that supervised users can't approve permission updates by themselves
// when the "Permissions for sites, apps and extensions" toggle is off and the
// Extension parental controls are managed by the "Permissions" Family Link
// switch.
TEST_P(ManagementApiSupervisedUserTest,
       SetEnabled_CantApprovePermissionUpdatesToggleOff) {
  // Preconditions.
  ASSERT_TRUE(profile()->IsChild());

  base::HistogramTester histogram_tester;

  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");

  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension =
      PackAndInstallCRX(path, pem_path, INSTALL_WITHOUT_LOAD);
  ASSERT_TRUE(extension);
  // The extension should be installed but disabled pending custodian approval.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  // Save the id, as |extension| will be destroyed during updating.
  const ExtensionId extension_id = extension->id();

  // Simulate parent approval for the extension installation.
  GetSupervisedUserExtensionsDelegate()->AddExtensionApproval(*extension);
  // The extension should be enabled now.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));

  // There should be 1 kApprovalGranted UMA metric.
  histogram_tester.ExpectUniqueSample(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
          kApprovalGranted,
      1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 1);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  std::unique_ptr<const PermissionSet> known_perms =
      prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  // v1 extension doesn't have any permissions.
  EXPECT_TRUE(known_perms->IsEmpty());

  // Update to a new version with increased permissions.
  path = base_path.AppendASCII("v2");
  PackCRXAndUpdateExtension(extension_id, path, pem_path, DISABLED);
  // The extension should be disabled.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));
  // Due to a permission increase, prefs will contain escalation information.
  EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));

  // If the "Permissions for sites, apps and extensions" toggle is off, then the
  // enable attempt should fail if the extension parental controls are managed
  // by the "Permissions" Family Link switch. Otherwise it should be enabled.
  bool should_be_enabled = IsManagedBySwitch(
                              ExtensionManagementSwitch::kManagedByExtensions);
  {
    supervised_user_test_util::
        SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), false);
    std::string error;
    bool success = RunSetEnabledFunction(web_contents_.get(), extension_id,
                                         /*use_user_gesture=*/true,
                                         /*accept_dialog=*/true, &error);

    EXPECT_EQ(should_be_enabled, success);
    EXPECT_EQ(should_be_enabled, error.empty());
    EXPECT_EQ(!should_be_enabled,
              registry()->disabled_extensions().Contains(extension_id));
    EXPECT_EQ(should_be_enabled,
              registry()->enabled_extensions().Contains(extension_id));
    // Prefs will still contain the escalation information if the enable attempt
    // failed.
    EXPECT_EQ(!should_be_enabled,
              prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // Permissions for v2 extension should not be granted if the extension
  // parental controls are managed by the "Permissions" switch.
  known_perms = prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  EXPECT_EQ(!should_be_enabled, known_perms->IsEmpty());

  // The parent approval dialog should have not appeared: The parent approval
  // dialog should never appear when the "Permissions for sites, apps and
  // extensions" toggle is off and it is used to manage the extensions.
  // If the "Extensions" toggle is used to manage the extensions, then the
  // extension must have been enabled, so the dialog does not appear.
  EXPECT_EQ(0, supervised_user_delegate_->show_dialog_count());

  // There should be no new UMA metrics if the extension parental controls are
  // managed by the "Permissions" switch.
  // If the extension parental controls are managed by the "Extensions" switch
  // the extension become enabled and the Permission increase is granted.
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
          kPermissionsIncreaseGranted,
      should_be_enabled ? 1 : 0);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      should_be_enabled ? 2 : 1);
}

// Tests that if an extension still requires parental consent, the supervised
// user approving it for permissions increase won't enable the extension and
// bypass parental consent.
// Prevents a regression to crbug/1070760.
TEST_P(ManagementApiSupervisedUserTest,
       SetEnabled_CustodianApprovalRequiredAndPermissionsIncrease) {
  // Preconditions.
  ASSERT_TRUE(profile()->IsChild());

  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");

  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension =
      PackAndInstallCRX(path, pem_path, INSTALL_WITHOUT_LOAD);
  ASSERT_TRUE(extension);
  // The extension should be installed but disabled pending custodian approval.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  // Save the id, as |extension| will be destroyed during updating.
  const ExtensionId extension_id = extension->id();

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  std::unique_ptr<const PermissionSet> known_perms =
      prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  // v1 extension doesn't have any permissions.
  EXPECT_TRUE(known_perms->IsEmpty());

  // Update to a new version with increased permissions.
  path = base_path.AppendASCII("v2");
  PackCRXAndUpdateExtension(extension_id, path, pem_path, DISABLED);
  // The extension should still be disabled.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));
  // This extension has two concurrent disable reasons.
  EXPECT_TRUE(prefs->HasDisableReason(
      extension_id, disable_reason::DISABLE_PERMISSIONS_INCREASE));
  EXPECT_TRUE(prefs->HasDisableReason(
      extension_id, disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED));

  // The supervised user trying to enable without parent approval should fail.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents_.get(), extension_id,
                                         /*use_user_gesture=*/true,
                                         /*accept_dialog=*/true, &error);
    EXPECT_FALSE(success);
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));
    // Both disable reasons should still be present.
    EXPECT_TRUE(prefs->HasDisableReason(
        extension_id, disable_reason::DISABLE_PERMISSIONS_INCREASE));
    EXPECT_TRUE(prefs->HasDisableReason(
        extension_id, disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED));
  }

  // Permissions for v2 extension should not be granted.
  known_perms = prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  EXPECT_TRUE(known_perms->IsEmpty());

  // The parent approval dialog should have appeared.
  EXPECT_EQ(1, supervised_user_delegate_->show_dialog_count());

  // Now try again with parent approval, and this should succeed.
  {
    supervised_user_delegate_->set_next_parent_permission_dialog_result(
        SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kApproved);
    std::string error;
    bool success = RunSetEnabledFunction(web_contents_.get(), extension_id,
                                         /*use_user_gesture=*/true,
                                         /*accept_dialog=*/true, &error);
    EXPECT_TRUE(success) << error;
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    // All disable reasons are gone.
    EXPECT_FALSE(prefs->DidExtensionEscalatePermissions(extension_id));
    EXPECT_FALSE(prefs->HasDisableReason(
        extension_id, disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED));
  }

  // Permissions for v2 extension should now be granted.
  known_perms = prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  EXPECT_FALSE(known_perms->IsEmpty());

  // The parent approval dialog should have appeared again.
  EXPECT_EQ(2, supervised_user_delegate_->show_dialog_count());
}

// Tests that trying to enable an extension with parent approval for supervised
// users still fails, if there's unsupported requirements.
TEST_P(ManagementApiSupervisedUserTest, SetEnabled_UnsupportedRequirement) {
  // Preconditions.
  ASSERT_TRUE(profile()->IsChild());
  ASSERT_EQ(0, supervised_user_delegate_->show_dialog_count());

  // No WebGL will be the unsupported requirement.
  content::GpuDataManager::GetInstance()->BlocklistWebGLForTesting();

  base::FilePath base_path = data_dir().AppendASCII("requirements");
  base::FilePath pem_path = base_path.AppendASCII("v1_good.pem");
  base::FilePath path = base_path.AppendASCII("v2_bad_requirements");
  const Extension* extension =
      PackAndInstallCRX(path, pem_path, INSTALL_WITHOUT_LOAD);
  ASSERT_TRUE(extension);
  // The extension should be installed but disabled pending custodian approval
  // and unsupported requirements.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(prefs->HasDisableReason(
      extension->id(), disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED));
  EXPECT_TRUE(prefs->HasDisableReason(
      extension->id(), disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT));

  // Parent approval should fail because of the unsupported requirements.
  {
    supervised_user_delegate_->set_next_parent_permission_dialog_result(
        SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kApproved);
    std::string error;
    bool success = RunSetEnabledFunction(web_contents_.get(), extension->id(),
                                         /*user_user_gesture=*/true,
                                         /*accept_dialog=*/true, &error);
    EXPECT_FALSE(success);
    EXPECT_FALSE(error.empty());
    // The parent permission dialog was never opened.
    EXPECT_EQ(0, supervised_user_delegate_->show_dialog_count());
    EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
    // The extension should still require parent approval.
    EXPECT_TRUE(prefs->HasDisableReason(
        extension->id(), disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED));
    EXPECT_TRUE(prefs->HasDisableReason(
        extension->id(), disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT));
  }
}

// Tests UMA metrics related to supervised users enabling and disabling
// extensions.
TEST_P(ManagementApiSupervisedUserTest, SetEnabledDisabled_UmaMetrics) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");

  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension =
      PackAndInstallCRX(path, pem_path, INSTALL_WITHOUT_LOAD);
  ASSERT_TRUE(extension);

  // The parent will approve.
  supervised_user_delegate_->set_next_parent_permission_dialog_result(
      SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kApproved);

  RunSetEnabledFunction(web_contents_.get(), extension->id(),
                        /*use_user_gesture=*/true, /*accept_dialog=*/true,
                        nullptr, /*enabled=*/true);
  histogram_tester.ExpectUniqueSample(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName,
      SupervisedUserExtensionsMetricsRecorder::EnablementState::kEnabled, 1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName, 1);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                SupervisedUserExtensionsMetricsRecorder::kEnabledActionName));
  EXPECT_EQ(0,
            user_action_tester.GetActionCount(
                SupervisedUserExtensionsMetricsRecorder::kDisabledActionName));

  // Simulate supervised user disabling extension.
  RunSetEnabledFunction(web_contents_.get(), extension->id(),
                        /*use_user_gesture=*/true, /*accept_dialog=*/true,
                        nullptr, /*enabled=*/false);
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName,
      SupervisedUserExtensionsMetricsRecorder::EnablementState::kDisabled, 1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName, 2);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                SupervisedUserExtensionsMetricsRecorder::kEnabledActionName));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                SupervisedUserExtensionsMetricsRecorder::kDisabledActionName));

  // Simulate supervised user re-enabling extension.
  RunSetEnabledFunction(web_contents_.get(), extension->id(),
                        /*use_user_gesture=*/true, /*accept_dialog=*/true,
                        nullptr, /*enabled=*/true);
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName,
      SupervisedUserExtensionsMetricsRecorder::EnablementState::kEnabled, 2);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName, 3);
  EXPECT_EQ(2,
            user_action_tester.GetActionCount(
                SupervisedUserExtensionsMetricsRecorder::kEnabledActionName));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                SupervisedUserExtensionsMetricsRecorder::kDisabledActionName));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ManagementApiSupervisedUserTest,
    testing::Values(ExtensionManagementSwitch::kManagedByExtensions,
                    ExtensionManagementSwitch::kManagedByPermissions),
    [](const auto& info) {
      return std::string(info.param ==
                                 ExtensionManagementSwitch::kManagedByExtensions
                             ? "ManagedByExtensionsSwitch"
                             : "ManagedByPermissionsSwitch");
    });

// Tests for supervised users (child accounts) with additional setup code.
class ManagementApiSupervisedUserTestWithSetup
    : public ManagementApiSupervisedUserTest {
 public:
  ManagementApiSupervisedUserTestWithSetup() = default;
  ~ManagementApiSupervisedUserTestWithSetup() override = default;

  void SetUp() override {
    ManagementApiSupervisedUserTest::SetUp();

    // Install a ManagementAPIDelegate to sense extension enable.
    delegate_ = new TestManagementAPIDelegate;
    management_api_->set_delegate_for_test(base::WrapUnique(delegate_.get()));

    // Add a generic extension.
    extension_ = ExtensionBuilder("Test").Build();
    service()->AddExtension(extension_.get());
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_->id()));
  }

  raw_ptr<TestManagementAPIDelegate> delegate_ = nullptr;
  scoped_refptr<const Extension> extension_;
};

TEST_P(ManagementApiSupervisedUserTestWithSetup, SetEnabled_ParentApproves) {
  // Preconditions.
  ASSERT_TRUE(profile()->IsChild());
  ASSERT_EQ(0, delegate_->enable_count_);
  ASSERT_EQ(0, supervised_user_delegate_->show_dialog_count());

  // Start with a disabled extension that needs parent permission.
  service()->DisableExtension(
      extension_->id(), disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);

  // The parent will approve.
  supervised_user_delegate_->set_next_parent_permission_dialog_result(
      SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kApproved);

  // Simulate a call to chrome.management.setEnabled(). It should succeed.
  std::string error;
  bool success = RunSetEnabledFunction(web_contents_.get(), extension_->id(),
                                       /*use_user_gesture=*/true,
                                       /*accept_dialog=*/true, &error);
  EXPECT_TRUE(success) << error;
  EXPECT_TRUE(error.empty());

  // Parent permission dialog was opened.
  EXPECT_EQ(1, supervised_user_delegate_->show_dialog_count());

  // Extension was enabled.
  EXPECT_EQ(1, delegate_->enable_count_);
}

TEST_P(ManagementApiSupervisedUserTestWithSetup, SetEnabled_ParentDenies) {
  // Start with a disabled extension that needs parent permission.
  service()->DisableExtension(
      extension_->id(), disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);

  // The parent will deny the next dialog.
  supervised_user_delegate_->set_next_parent_permission_dialog_result(
      SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kCanceled);

  // Simulate a call to chrome.management.setEnabled(). It should not succeed.
  std::string error;
  bool success = RunSetEnabledFunction(web_contents_.get(), extension_->id(),
                                       /*use_user_gesture=*/true,
                                       /*accept_dialog=*/true, &error);
  EXPECT_FALSE(success);
  EXPECT_FALSE(error.empty());

  // Parent permission dialog was opened.
  EXPECT_EQ(1, supervised_user_delegate_->show_dialog_count());

  // Extension was not enabled.
  EXPECT_EQ(0, delegate_->enable_count_);
}

TEST_P(ManagementApiSupervisedUserTestWithSetup, SetEnabled_DialogFails) {
  // Start with a disabled extension that needs parent permission.
  service()->DisableExtension(
      extension_->id(), disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);

  // The next dialog will close due to a failure (e.g. network failure while
  // looking up parent information).
  supervised_user_delegate_->set_next_parent_permission_dialog_result(
      SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kFailed);

  // Simulate a call to chrome.management.setEnabled(). It should not succeed.
  std::string error;
  bool success = RunSetEnabledFunction(web_contents_.get(), extension_->id(),
                                       /*use_user_gesture=*/true,
                                       /*accept_dialog=*/true, &error);
  EXPECT_FALSE(success);
  EXPECT_FALSE(error.empty());

  // Extension was not enabled.
  EXPECT_EQ(0, delegate_->enable_count_);
}

TEST_P(ManagementApiSupervisedUserTestWithSetup, SetEnabled_PreviouslyAllowed) {
  // Disable the extension.
  service()->DisableExtension(extension_->id(),
                              disable_reason::DISABLE_USER_ACTION);

  // Simulate previous parent approval.
  GetSupervisedUserExtensionsDelegate()->AddExtensionApproval(*extension_);

  // Simulate a call to chrome.management.setEnabled().
  std::string error;
  bool success = RunSetEnabledFunction(web_contents_.get(), extension_->id(),
                                       /*use_user_gesture=*/true,
                                       /*accept_dialog=*/true, &error);
  EXPECT_TRUE(success) << error;
  EXPECT_TRUE(error.empty());

  // Parent permission dialog was not opened.
  EXPECT_EQ(0, supervised_user_delegate_->show_dialog_count());
}

// Tests launching the Parent Permission Dialog from a background page, where
// there isn't active web contents. The parent approves the request.
TEST_P(ManagementApiSupervisedUserTestWithSetup,
       SetEnabled_ParentPermissionApprovedFromBackgroundPage) {
  // Preconditions.
  ASSERT_TRUE(profile()->IsChild());

  // Start with a disabled extension that needs parent permission.
  service()->DisableExtension(
      extension_->id(), disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);

  // The parent will approve.
  supervised_user_delegate_->set_next_parent_permission_dialog_result(
      SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kApproved);

  // Simulate a call to chrome.management.setEnabled(). It should succeed
  // despite a lack of web contents.
  std::string error;
  bool success = RunSetEnabledFunction(
      /*web_contents=*/nullptr, extension_->id(), /*use_user_gesture=*/true,
      /*accept_dialog=*/true, &error);
  EXPECT_TRUE(success);
  EXPECT_TRUE(error.empty());

  // Parent Permission Dialog still opened despite the lack of web contents.
  EXPECT_EQ(1, supervised_user_delegate_->show_dialog_count());
  EXPECT_EQ(0, supervised_user_delegate_->show_block_dialog_count());

  // Extension is now enabled.
  EXPECT_EQ(1, delegate_->enable_count_);
}

// Tests launching the Parent Permission Dialog from a background page, where
// there isn't active web contents. The parent cancels the request.
TEST_P(ManagementApiSupervisedUserTestWithSetup,
       SetEnabled_ParentPermissionCanceledFromBackgroundPage) {
  // Preconditions.
  ASSERT_TRUE(profile()->IsChild());

  // Start with a disabled extension that needs parent permission.
  service()->DisableExtension(
      extension_->id(), disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);

  // The parent will cancel.
  supervised_user_delegate_->set_next_parent_permission_dialog_result(
      SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kCanceled);

  // Simulate a call to chrome.management.setEnabled() with no web contents.
  std::string error;
  bool success = RunSetEnabledFunction(
      /*web_contents=*/nullptr, extension_->id(), /*use_user_gesture=*/true,
      /*accept_dialog=*/true, &error);
  EXPECT_FALSE(success);
  EXPECT_EQ(extension_management_api_constants::kUserDidNotReEnableError,
            error);

  // Parent Permission Dialog still opened despite the lack of web contents.
  EXPECT_EQ(1, supervised_user_delegate_->show_dialog_count());
  EXPECT_EQ(0, supervised_user_delegate_->show_block_dialog_count());

  // Extension was not enabled.
  EXPECT_EQ(0, delegate_->enable_count_);
}

// Tests launching the Parent Permission Dialog from a background page, where
// there isn't active web contents. The request will fail due to some sort of
// error, such as a network error.
TEST_P(ManagementApiSupervisedUserTestWithSetup,
       SetEnabled_ParentPermissionFailedFromBackgroundPage) {
  // Preconditions.
  ASSERT_TRUE(profile()->IsChild());

  // Start with a disabled extension that needs parent permission.
  service()->DisableExtension(
      extension_->id(), disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);

  // The request will fail.
  supervised_user_delegate_->set_next_parent_permission_dialog_result(
      SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kFailed);

  // Simulate a call to chrome.management.setEnabled() with no web contents.
  std::string error;
  bool success = RunSetEnabledFunction(
      /*web_contents=*/nullptr, extension_->id(), /*use_user_gesture=*/true,
      /*accept_dialog=*/true, &error);
  EXPECT_FALSE(success);
  EXPECT_EQ(extension_management_api_constants::kParentPermissionFailedError,
            error);

  // Parent Permission Dialog still opened despite the lack of web contents.
  EXPECT_EQ(1, supervised_user_delegate_->show_dialog_count());
  EXPECT_EQ(0, supervised_user_delegate_->show_block_dialog_count());

  // Extension was not enabled.
  EXPECT_EQ(0, delegate_->enable_count_);
}

// Tests launching the Extension Install Blocked By Parent Dialog from a
// background page, where there isn't active web contents. This dialog
// appears only then the "Permissions" Family Link switch managed the
// extension parental controls.
TEST_P(ManagementApiSupervisedUserTestWithSetup,
       SetEnabled_ExtensionInstallBlockedByParentFromBackgroundPage) {
  // Preconditions.
  ASSERT_TRUE(profile()->IsChild());

  // Start with a disabled extension that needs parent permission.
  service()->DisableExtension(
      extension_->id(), disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);

  // Simulate the parent disabling the "Permissions for sites, apps and
  // extensions" toggle.
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), false);

  // Simulate a call to chrome.management.setEnabled(). The enable attempt
  // should fail as the extension is missing parent approval.
  std::string error;
  bool success = RunSetEnabledFunction(
      /*web_contents=*/nullptr, extension_->id(), /*use_user_gesture=*/true,
      /*accept_dialog=*/true, &error);
  EXPECT_FALSE(success);
  bool should_be_blocked = IsManagedBySwitch(
                              ExtensionManagementSwitch::kManagedByPermissions);
  if (should_be_blocked) {
    const std::string expected_error = ErrorUtils::FormatErrorMessage(
        extension_management_api_constants::kUserCantModifyError,
        extension_->id());
    EXPECT_EQ(expected_error, error);
  }
  // The Extension Install Blocked By Parent Dialog should have opened despite
  // the lack of web contents, if the extension is blocked (when managed by the
  // "Permissions" switch). Otherwise (when managed by "Extensions" switch), the
  // extension is never blocked and the parent permission dialog appears.
  EXPECT_EQ(should_be_blocked ? 1 : 0,
            supervised_user_delegate_->show_block_dialog_count());
  EXPECT_EQ(should_be_blocked ? 0 : 1,
            supervised_user_delegate_->show_dialog_count());

  // Extension was not enabled.
  EXPECT_EQ(0, delegate_->enable_count_);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ManagementApiSupervisedUserTestWithSetup,
    testing::Values(ExtensionManagementSwitch::kManagedByExtensions,
                    ExtensionManagementSwitch::kManagedByPermissions),
    [](const auto& info) {
      return std::string(info.param ==
                                 ExtensionManagementSwitch::kManagedByExtensions
                             ? "ManagedByExtensionsSwitch"
                             : "ManagedByPermissionsSwitch");
    });

}  // namespace
}  // namespace extensions

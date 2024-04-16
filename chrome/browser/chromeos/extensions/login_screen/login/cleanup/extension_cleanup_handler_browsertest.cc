// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/extension_cleanup_handler.h"

#include <memory>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/path_service.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/extensions/policy_test_utils.h"
#include "chrome/browser/policy/extension_force_install_mixin.h"
#include "chrome/common/chrome_paths.h"
#include "components/app_constants/constants.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"

namespace {

const char* const kExemptExtensions[] = {
    app_constants::kChromeAppId,
    app_constants::kLacrosAppId,
};

const char kAccountId[] = "public-session@test";

const char kAppId[] = "kbmnembihfiondgfjekmnmcbddelicoi";
const char kAppCrxPath[] = "extensions/hosted_app.crx";

const char kExemptExtensionId[] = "ongnjlefhnoajpbodoldndkbkdgfomlp";
const char kExemptExtensionCrxPath[] = "extensions/show_managed_storage.crx";

const char kUserExtensionId[] = "ngjnkanfphagcaokhjecbgkboelgfcnf";

}  // namespace

class ExtensionCleanupHandlerTest : public policy::DevicePolicyCrosBrowserTest {
 public:
  ExtensionCleanupHandlerTest(const ExtensionCleanupHandlerTest&) = delete;
  ExtensionCleanupHandlerTest& operator=(const ExtensionCleanupHandlerTest&) =
      delete;

 protected:
  ExtensionCleanupHandlerTest() = default;
  ~ExtensionCleanupHandlerTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile, "user");
    command_line->AppendSwitch(ash::switches::kOobeSkipPostLogin);
  }

  void SetUpOnMainThread() override {
    DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  }

  void SetUpDeviceLocalAccountPolicy() {
    enterprise_management::ChromeDeviceSettingsProto& proto(
        device_policy()->payload());
    enterprise_management::DeviceLocalAccountsProto* device_local_accounts =
        proto.mutable_device_local_accounts();
    enterprise_management::DeviceLocalAccountInfoProto* const account =
        device_local_accounts->add_account();
    account->set_account_id(kAccountId);
    account->set_type(enterprise_management::DeviceLocalAccountInfoProto::
                          ACCOUNT_TYPE_PUBLIC_SESSION);
    device_local_accounts->set_auto_login_id(kAccountId);
    device_local_accounts->set_auto_login_delay(0);
    RefreshDevicePolicy();
    policy_test_server_mixin_.UpdateDevicePolicy(proto);
  }

  void AddExemptExtensionToUserPolicyBuilder(
      policy::UserPolicyBuilder* user_policy_builder,
      const std::string& extension_id) {
    user_policy_builder->payload()
        .mutable_restrictedmanagedguestsessionextensioncleanupexemptlist()
        ->mutable_value()
        ->add_entries(extension_id);
  }

  void SetUpUserPolicyBuilderForPublicAccount(
      policy::UserPolicyBuilder* user_policy_builder) {
    enterprise_management::PolicyData& policy_data =
        user_policy_builder->policy_data();
    policy_data.set_public_key_version(1);
    policy_data.set_policy_type(
        policy::dm_protocol::kChromePublicAccountPolicyType);
    policy_data.set_username(kAccountId);
    policy_data.set_settings_entity_id(kAccountId);
    user_policy_builder->SetDefaultSigningKey();
  }

  void ForceInstallExtensionCrx(const std::string& extension_crx) {
    extensions::ExtensionId extension_id;
    EXPECT_TRUE(extension_force_install_mixin_.ForceInstallFromCrx(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
            .AppendASCII(extension_crx),
        ExtensionForceInstallMixin::WaitMode::kLoad, &extension_id));

    const extensions::Extension* extension =
        extension_force_install_mixin_.GetEnabledExtension(extension_id);
    ASSERT_TRUE(extension);
  }

  Profile* GetActiveUserProfile() {
    const user_manager::User* active_user =
        user_manager::UserManager::Get()->GetActiveUser();
    return ash::ProfileHelper::Get()->GetProfileByUser(active_user);
  }

  void InstallUserExtension(const std::string& extension_id) {
    extensions::ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(GetActiveUserProfile())
            ->extension_service();
    base::Value::Dict manifest(base::Value::Dict()
                                   .Set("name", "Foo")
                                   .Set("description", "Bar")
                                   .Set("manifest_version", 2)
                                   .Set("version", "1.0"));

    auto observer = GetTestExtensionRegistryObserver(extension_id);
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder()
            .SetLocation(extensions::mojom::ManifestLocation::kInternal)
            .SetID(extension_id)
            .SetManifest(std::move(manifest))
            .Build();
    extension_service->AddExtension(extension.get());
    observer->WaitForExtensionReady();
  }

  std::unique_ptr<extensions::TestExtensionRegistryObserver>
  GetTestExtensionRegistryObserver(const std::string& extension_id) {
    return std::make_unique<extensions::TestExtensionRegistryObserver>(
        extensions::ExtensionRegistry::Get(GetActiveUserProfile()),
        extension_id);
  }

  int GetNumberOfInstalledExtensions() {
    return extensions::ExtensionRegistry::Get(GetActiveUserProfile())
        ->GenerateInstalledExtensionsSet()
        .size();
  }

  void WaitForSessionStart() {
    if (IsSessionStarted())
      return;
    ash::test::WaitForPrimaryUserSessionStart();
  }

  bool IsSessionStarted() {
    return session_manager::SessionManager::Get()->IsSessionStarted();
  }

  void WaitForComponentExtensionsInstall() {
    extensions::ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(GetActiveUserProfile())
            ->extension_service();
    std::vector<std::string> registered_component_extensions =
        extension_service->component_loader()
            ->GetRegisteredComponentExtensionsIds();
    std::unordered_set<
        std::unique_ptr<extensions::TestExtensionRegistryObserver>>
        extension_observers;
    for (const auto& extension : registered_component_extensions) {
      if (extension_service->pending_extension_manager()->IsIdPending(
              extension)) {
        extension_observers.insert(GetTestExtensionRegistryObserver(extension));
      }
    }
    for (auto& extension_observer : extension_observers) {
      extension_observer->WaitForExtensionReady();
    }
  }

  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  ExtensionForceInstallMixin extension_force_install_mixin_{&mixin_host_};
};

// TODO(crbug.com/41491505): Disable flaky test.
#if BUILDFLAG(IS_CHROMEOS) && defined(ADDRESS_SANITIZER)
#define MAYBE_CleanupWithExemptExtensions DISABLED_CleanupWithExemptExtensions
#else
#define MAYBE_CleanupWithExemptExtensions CleanupWithExemptExtensions
#endif
IN_PROC_BROWSER_TEST_F(ExtensionCleanupHandlerTest,
                       MAYBE_CleanupWithExemptExtensions) {
  SetUpDeviceLocalAccountPolicy();
  WaitForSessionStart();
  WaitForComponentExtensionsInstall();

  int num_default_extensions = GetNumberOfInstalledExtensions();
  EXPECT_GT(num_default_extensions, 0);
  Profile* profile = GetActiveUserProfile();
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();

  // Set up the user policy builder and add an extension to the cleanup
  // exemption list.
  policy::UserPolicyBuilder user_policy_builder;
  SetUpUserPolicyBuilderForPublicAccount(&user_policy_builder);
  AddExemptExtensionToUserPolicyBuilder(&user_policy_builder,
                                        kExemptExtensionId);

  // Force install an app and extension. The extension is exempt from the
  // cleanup procedure. This also waits for the app and extension to be
  // installed.
  extension_force_install_mixin_.InitWithEmbeddedPolicyMixin(
      profile, &policy_test_server_mixin_, &user_policy_builder, kAccountId,
      policy::dm_protocol::kChromePublicAccountPolicyType);
  ForceInstallExtensionCrx(kAppCrxPath);
  ForceInstallExtensionCrx(kExemptExtensionCrxPath);
  EXPECT_EQ(GetNumberOfInstalledExtensions(), num_default_extensions + 2);

  // Add user-installed extension.
  InstallUserExtension(kUserExtensionId);
  EXPECT_EQ(GetNumberOfInstalledExtensions(), num_default_extensions + 3);

  // Create observers for all extensions and apps that will be cleaned up.
  const extensions::ExtensionSet all_installed_extensions =
      extension_registry->GenerateInstalledExtensionsSet();
  std::unordered_set<std::unique_ptr<extensions::TestExtensionRegistryObserver>>
      extension_observers;
  for (const auto& extension : all_installed_extensions) {
    // Don't observe exempt and user installed extensions.
    if (base::Contains(kExemptExtensions, extension->id()))
      continue;
    if (extension->id() == kExemptExtensionId ||
        extension->id() == kUserExtensionId)
      continue;
    extension_observers.insert(
        GetTestExtensionRegistryObserver(extension->id()));
  }
  ASSERT_FALSE(extension_observers.empty());

  // Observer for the force-installed, not exempt extension, in order to not
  // only rely on the extension_observers.
  auto force_installed_observer = GetTestExtensionRegistryObserver(kAppId);
  // Only observe the uninstall of the user-installed extension.
  auto user_installed_observer =
      GetTestExtensionRegistryObserver(kUserExtensionId);

  // Start cleanup.
  std::unique_ptr<chromeos::ExtensionCleanupHandler> extension_cleanup_handler =
      std::make_unique<chromeos::ExtensionCleanupHandler>();
  extension_cleanup_handler->Cleanup(base::DoNothing());

  // Wait for extension uninstall and install.
  for (auto& extension_observer : extension_observers) {
    extension_observer->WaitForExtensionUninstalled();
    extension_observer->WaitForExtensionReady();
  }
  force_installed_observer->WaitForExtensionUninstalled();
  user_installed_observer->WaitForExtensionUninstalled();
  force_installed_observer->WaitForExtensionReady();

  // User installed extension is not reinstalled.
  EXPECT_FALSE(
      extension_registry->enabled_extensions().Contains(kUserExtensionId));
  EXPECT_FALSE(extension_service->pending_extension_manager()->IsIdPending(
      kUserExtensionId));

  // Force-installed app and extension are reinstalled.
  EXPECT_TRUE(extension_registry->enabled_extensions().Contains(kAppId));
  EXPECT_TRUE(
      extension_registry->enabled_extensions().Contains(kExemptExtensionId));
  EXPECT_EQ(GetNumberOfInstalledExtensions(), num_default_extensions + 2);
}

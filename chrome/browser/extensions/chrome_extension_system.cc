// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_system.h"

#include <algorithm>
#include <memory>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_tokenizer.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/blocklist_factory.h"
#include "chrome/browser/extensions/chrome_content_verifier_delegate.h"
#include "chrome/browser/extensions/chrome_extension_system_factory.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_error_controller.h"
#include "chrome/browser/extensions/extension_garbage_collector.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_verifier_factory.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/extensions/sync/extension_sync_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/ui/webui/extensions/extensions_internals_source.h"
#include "chrome/common/chrome_switches.h"
#include "components/value_store/value_store_factory_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/content_verifier/content_verifier.h"
#include "extensions/browser/delayed_install_manager.h"
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/install_verifier.h"
#include "extensions/browser/quota_service.h"
#include "extensions/browser/service_worker_manager.h"
#include "extensions/browser/state_store.h"
#include "extensions/browser/update_install_gate.h"
#include "extensions/browser/updater/uninstall_ping_sender.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_url_handlers.h"
#include "ui/message_center/public/cpp/notifier_id.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_app_sorting.h"
#else
#include "chrome/browser/extensions/chrome_extension_registrar_delegate.h"
#include "extensions/browser/null_app_sorting.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ash/app_mode/kiosk_app_update_install_gate.h"
#include "chrome/browser/ash/extensions/device_local_account_management_policy_provider.h"
#include "chrome/browser/ash/extensions/extensions_permissions_tracker.h"
#include "chrome/browser/ash/extensions/signin_screen_policy_provider.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/user_manager/user_manager.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

// Helper to serve as an UninstallPingSender::Filter callback.
UninstallPingSender::FilterResult ShouldSendUninstallPing(
    Profile* profile,
    const Extension* extension,
    UninstallReason reason) {
  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(profile);
  if (extension && (extension->from_webstore() ||
                    extension_management->UpdatesFromWebstore(*extension))) {
    return UninstallPingSender::SEND_PING;
  }
  return UninstallPingSender::DO_NOT_SEND_PING;
}

}  // namespace

//
// ChromeExtensionSystem::Shared
//

ChromeExtensionSystem::Shared::Shared(Profile* profile) : profile_(profile) {}

ChromeExtensionSystem::Shared::~Shared() = default;

void ChromeExtensionSystem::Shared::InitPrefs() {
  store_factory_ = base::MakeRefCounted<value_store::ValueStoreFactoryImpl>(
      profile_->GetPath());

  // Three state stores. Two stores, which contain declarative rules and dynamic
  // user scripts respectively, must be loaded immediately so that the
  // rules/scripts are ready before we issue network requests.
  state_store_ = std::make_unique<StateStore>(
      profile_, store_factory_, StateStore::BackendType::STATE, true);

  rules_store_ = std::make_unique<StateStore>(
      profile_, store_factory_, StateStore::BackendType::RULES, false);

  dynamic_user_scripts_store_ = std::make_unique<StateStore>(
      profile_, store_factory_, StateStore::BackendType::SCRIPTS, false);

#if BUILDFLAG(IS_CHROMEOS)
  // We can not perform check for Signin Profile here, as it would result in
  // recursive call upon creation of Signin Profile, so we will create
  // SigninScreenPolicyProvider lazily in RegisterManagementPolicyProviders.

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (user) {
    auto device_local_account_type =
        policy::GetDeviceLocalAccountType(user->GetAccountId().GetUserEmail());
    if (device_local_account_type.has_value()) {
      device_local_account_management_policy_provider_ = std::make_unique<
          chromeos::DeviceLocalAccountManagementPolicyProvider>(
          device_local_account_type.value());
    }
  }
#endif
}

void ChromeExtensionSystem::Shared::RegisterManagementPolicyProviders() {
  management_policy_->RegisterProviders(
      ExtensionManagementFactory::GetForBrowserContext(profile_)
          ->GetProviders());

#if BUILDFLAG(IS_CHROMEOS)
  // Lazy creation of SigninScreenPolicyProvider.
  if (!signin_screen_policy_provider_) {
    if (ash::ProfileHelper::IsSigninProfile(profile_)) {
      signin_screen_policy_provider_ =
          std::make_unique<chromeos::SigninScreenPolicyProvider>();
    }
  }

  if (device_local_account_management_policy_provider_) {
    management_policy_->RegisterProvider(
        device_local_account_management_policy_provider_.get());
  }
  if (signin_screen_policy_provider_) {
    management_policy_->RegisterProvider(signin_screen_policy_provider_.get());
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  management_policy_->RegisterProvider(
      InstallVerifierFactory::GetForBrowserContext(profile_));
}

void ChromeExtensionSystem::Shared::InitInstallGates() {
  update_install_gate_ = std::make_unique<UpdateInstallGate>(profile_);
  auto* delayed_install_manager = DelayedInstallManager::Get(profile_);
  delayed_install_manager->RegisterInstallGate(
      ExtensionPrefs::DelayReason::kWaitForIdle, update_install_gate_.get());
  delayed_install_manager->RegisterInstallGate(
      ExtensionPrefs::DelayReason::kWaitForImports,
      SharedModuleService::Get(profile_));
#if BUILDFLAG(IS_CHROMEOS)
  if (IsRunningInForcedAppMode()) {
    kiosk_app_update_install_gate_ =
        std::make_unique<ash::KioskAppUpdateInstallGate>(profile_);
    delayed_install_manager->RegisterInstallGate(
        ExtensionPrefs::DelayReason::kWaitForOsUpdate,
        kiosk_app_update_install_gate_.get());
  }
#endif
}

void ChromeExtensionSystem::Shared::Init(bool extensions_enabled) {
  TRACE_EVENT0("browser,startup", "ChromeExtensionSystem::Shared::Init");
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  bool allow_noisy_errors =
      !command_line->HasSwitch(::switches::kNoErrorDialogs);
  LoadErrorReporter::Init(allow_noisy_errors);

  content_verifier_ = new ContentVerifier(
      profile_, std::make_unique<ChromeContentVerifierDelegate>(profile_));

  service_worker_manager_ = std::make_unique<ServiceWorkerManager>(profile_);

  user_script_manager_ = std::make_unique<UserScriptManager>(profile_);

  bool autoupdate_enabled =
      !profile_->IsGuestSession() && !profile_->IsSystemProfile();
#if BUILDFLAG(IS_CHROMEOS)
  if (!extensions_enabled) {
    autoupdate_enabled = false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  extension_service_ = std::make_unique<ExtensionService>(
      profile_, base::CommandLine::ForCurrentProcess(),
      profile_->GetPath().AppendASCII(kInstallDirectoryName),
      profile_->GetPath().AppendASCII(kUnpackedInstallDirectoryName),
      ExtensionPrefs::Get(profile_),
      BlocklistFactory::GetForBrowserContext(profile_),
      ExtensionErrorController::Get(profile_), autoupdate_enabled,
      extensions_enabled, &ready_);

  uninstall_ping_sender_ = std::make_unique<UninstallPingSender>(
      ExtensionRegistry::Get(profile_),
      base::BindRepeating(&ShouldSendUninstallPing, profile_));

  // These services must be registered before the ExtensionService tries to
  // load any extensions.
  {
    InstallVerifierFactory::GetForBrowserContext(profile_)->Init();
    ChromeContentVerifierDelegate::VerifyInfo::Mode mode =
        ChromeContentVerifierDelegate::GetDefaultMode();
#if BUILDFLAG(IS_CHROMEOS)
    mode = std::max(mode,
                    ChromeContentVerifierDelegate::VerifyInfo::Mode::BOOTSTRAP);
#endif  // BUILDFLAG(IS_CHROMEOS)
    if (mode >= ChromeContentVerifierDelegate::VerifyInfo::Mode::BOOTSTRAP) {
      content_verifier_->Start();
    }
#if BUILDFLAG(IS_CHROMEOS)
    // This class is used to check the permissions of the force-installed
    // extensions inside the managed guest session. It updates the local state
    // perf with the result, a boolean value deciding whether the full warning
    // or the normal one should be displayed. The next time on the login screen
    // of the managed guest sessions the warning will be decided according to
    // the value saved from the last session.
    if (chromeos::IsManagedGuestSession()) {
      extensions_permissions_tracker_ =
          std::make_unique<ExtensionsPermissionsTracker>(
              ExtensionRegistry::Get(profile_), profile_);
    }
#endif
    management_policy_ = std::make_unique<ManagementPolicy>();
    RegisterManagementPolicyProviders();
  }

  // Extension API calls require QuotaService, so create it before loading any
  // extensions.
  quota_service_ = std::make_unique<QuotaService>();

  bool skip_session_extensions = false;
  auto* component_loader = ComponentLoader::Get(profile_);
#if BUILDFLAG(IS_CHROMEOS)
  // Skip loading session extensions if we are not in a user session or if the
  // profile is the sign-in or lock screen app profile, which don't correspond
  // to a user session.
  skip_session_extensions = !ash::LoginState::Get()->IsUserLoggedIn() ||
                            !ash::ProfileHelper::IsUserProfile(profile_);
  if (IsRunningInForcedAppMode()) {
    component_loader->AddDefaultComponentExtensionsForKioskMode(
        skip_session_extensions);
  } else {
    component_loader->AddDefaultComponentExtensions(skip_session_extensions);
  }
#else
  component_loader->AddDefaultComponentExtensions(skip_session_extensions);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  app_sorting_ = std::make_unique<ChromeAppSorting>(profile_);
#else
  app_sorting_ = std::make_unique<NullAppSorting>();
#endif

  InitInstallGates();

  extension_service_->Init();

  // Make sure ExtensionSyncService is created.
  ExtensionSyncService::Get(profile_);

  // Make the chrome://extension-icon/ resource available.
  content::URLDataSource::Add(profile_,
                              std::make_unique<ExtensionIconSource>(profile_));

  // Register the source for the chrome://extensions-internals page.
  content::URLDataSource::Add(
      profile_, std::make_unique<ExtensionsInternalsSource>(profile_));
}

void ChromeExtensionSystem::Shared::Shutdown() {
  if (content_verifier_.get()) {
    content_verifier_->Shutdown();
  }
  if (extension_service_) {
    extension_service_->Shutdown();
  }
}

ServiceWorkerManager* ChromeExtensionSystem::Shared::service_worker_manager() {
  return service_worker_manager_.get();
}

StateStore* ChromeExtensionSystem::Shared::state_store() {
  return state_store_.get();
}

StateStore* ChromeExtensionSystem::Shared::rules_store() {
  return rules_store_.get();
}

StateStore* ChromeExtensionSystem::Shared::dynamic_user_scripts_store() {
  return dynamic_user_scripts_store_.get();
}

scoped_refptr<value_store::ValueStoreFactory>
ChromeExtensionSystem::Shared::store_factory() const {
  return store_factory_;
}

ExtensionService* ChromeExtensionSystem::Shared::extension_service() {
  return extension_service_.get();
}

ManagementPolicy* ChromeExtensionSystem::Shared::management_policy() {
  return management_policy_.get();
}

UserScriptManager* ChromeExtensionSystem::Shared::user_script_manager() {
  return user_script_manager_.get();
}

QuotaService* ChromeExtensionSystem::Shared::quota_service() {
  return quota_service_.get();
}

AppSorting* ChromeExtensionSystem::Shared::app_sorting() {
  return app_sorting_.get();
}

ContentVerifier* ChromeExtensionSystem::Shared::content_verifier() {
  return content_verifier_.get();
}

//
// ChromeExtensionSystem
//

ChromeExtensionSystem::ChromeExtensionSystem(Profile* profile)
    : profile_(profile) {
  shared_ = ChromeExtensionSystemSharedFactory::GetForBrowserContext(profile);

  if (!profile->IsOffTheRecord()) {
    shared_->InitPrefs();
  }
}

ChromeExtensionSystem::~ChromeExtensionSystem() = default;

void ChromeExtensionSystem::Shutdown() {}

void ChromeExtensionSystem::InitForRegularProfile(bool extensions_enabled) {
  TRACE_EVENT0("browser,startup",
               "ChromeExtensionSystem::InitForRegularProfile");

  if (user_script_manager() || extension_service()) {
    return;  // Already initialized.
  }

  shared_->Init(extensions_enabled);
}

ExtensionService* ChromeExtensionSystem::extension_service() {
  return shared_->extension_service();
}

ManagementPolicy* ChromeExtensionSystem::management_policy() {
  return shared_->management_policy();
}

ServiceWorkerManager* ChromeExtensionSystem::service_worker_manager() {
  return shared_->service_worker_manager();
}

UserScriptManager* ChromeExtensionSystem::user_script_manager() {
  return shared_->user_script_manager();
}

StateStore* ChromeExtensionSystem::state_store() {
  return shared_->state_store();
}

StateStore* ChromeExtensionSystem::rules_store() {
  return shared_->rules_store();
}

StateStore* ChromeExtensionSystem::dynamic_user_scripts_store() {
  return shared_->dynamic_user_scripts_store();
}

scoped_refptr<value_store::ValueStoreFactory>
ChromeExtensionSystem::store_factory() {
  return shared_->store_factory();
}

const base::OneShotEvent& ChromeExtensionSystem::ready() const {
  return shared_->ready();
}

bool ChromeExtensionSystem::is_ready() const {
  return shared_->is_ready();
}

QuotaService* ChromeExtensionSystem::quota_service() {
  return shared_->quota_service();
}

AppSorting* ChromeExtensionSystem::app_sorting() {
  return shared_->app_sorting();
}

ContentVerifier* ChromeExtensionSystem::content_verifier() {
  return shared_->content_verifier();
}

std::unique_ptr<ExtensionSet> ChromeExtensionSystem::GetDependentExtensions(
    const Extension* extension) {
  return SharedModuleService::Get(profile_)->GetDependentExtensions(extension);
}

void ChromeExtensionSystem::InstallUpdate(
    const std::string& extension_id,
    const std::string& public_key,
    const base::FilePath& unpacked_dir,
    bool install_immediately,
    InstallUpdateCallback install_update_callback) {
  DCHECK(!install_update_callback.is_null());

  scoped_refptr<CrxInstaller> installer =
      CrxInstaller::CreateSilent(profile_->GetOriginalProfile());
  installer->set_delete_source(true);
  installer->AddInstallerCallback(std::move(install_update_callback));
  installer->set_install_immediately(install_immediately);
  installer->UpdateExtensionFromUnpackedCrx(extension_id, public_key,
                                            unpacked_dir);
}

void ChromeExtensionSystem::PerformActionBasedOnOmahaAttributes(
    const std::string& extension_id,
    const base::Value::Dict& attributes) {
  extension_service()->PerformActionBasedOnOmahaAttributes(extension_id,
                                                           attributes);
}

}  // namespace extensions

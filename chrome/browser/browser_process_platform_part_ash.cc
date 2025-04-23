// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_ash.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/ash/app_list/search/essential_search/essential_search_manager.h"
#include "chrome/browser/ash/boot_times_recorder/boot_times_recorder.h"
#include "chrome/browser/ash/login/saml/in_session_password_change_manager.h"
#include "chrome/browser/ash/login/session/chrome_session_manager.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_registry.h"
#include "chrome/browser/ash/login/users/policy_user_manager_controller.h"
#include "chrome/browser/ash/login/users/profile_user_manager_controller.h"
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/net/ash_proxy_monitor.h"
#include "chrome/browser/ash/net/secure_dns_manager.h"
#include "chrome/browser/ash/net/system_proxy_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings_holder.h"
#include "chrome/browser/ash/system/automatic_reboot_manager.h"
#include "chrome/browser/ash/system/device_disabling_manager.h"
#include "chrome/browser/ash/system/device_disabling_manager_default_delegate.h"
#include "chrome/browser/ash/system/system_clock.h"
#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/metadata_table_chromeos.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_flusher.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller.h"
#include "chromeos/ash/components/scheduler_config/scheduler_configuration_manager.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/timezone/timezone_resolver.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/cpp/service.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace {

class PrimaryProfileServicesShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static PrimaryProfileServicesShutdownNotifierFactory* GetInstance() {
    return base::Singleton<
        PrimaryProfileServicesShutdownNotifierFactory>::get();
  }

  PrimaryProfileServicesShutdownNotifierFactory(
      const PrimaryProfileServicesShutdownNotifierFactory&) = delete;
  PrimaryProfileServicesShutdownNotifierFactory& operator=(
      const PrimaryProfileServicesShutdownNotifierFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<
      PrimaryProfileServicesShutdownNotifierFactory>;

  PrimaryProfileServicesShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "PrimaryProfileServices") {}
  ~PrimaryProfileServicesShutdownNotifierFactory() override = default;
};

}  // namespace

BrowserProcessPlatformPart::BrowserProcessPlatformPart()
    : browser_restore_observer_(this),
      created_profile_helper_(false),
      browser_context_flusher_(std::make_unique<ash::BrowserContextFlusher>()),
      account_manager_factory_(std::make_unique<ash::AccountManagerFactory>()) {
}

BrowserProcessPlatformPart::~BrowserProcessPlatformPart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool BrowserProcessPlatformPart::CanRestoreUrlsForProfile(
    const Profile* profile) const {
  return profile->IsRegularProfile() && !profile->IsSystemProfile() &&
         ash::ProfileHelper::IsUserProfile(profile) &&
         !ash::ProfileHelper::IsEphemeralUserProfile(profile);
}

BrowserProcessPlatformPart::BrowserRestoreObserver::BrowserRestoreObserver(
    const BrowserProcessPlatformPart* browser_process_platform_part)
    : browser_process_platform_part_(browser_process_platform_part) {
  BrowserList::AddObserver(this);
}

BrowserProcessPlatformPart::BrowserRestoreObserver::~BrowserRestoreObserver() {
  BrowserList::RemoveObserver(this);
}

void BrowserProcessPlatformPart::BrowserRestoreObserver::OnBrowserAdded(
    Browser* browser) {
  // If |browser| is the only browser, restores urls based on the on startup
  // setting.
  if (chrome::GetBrowserCount(browser->profile()) == 1 &&
      ShouldRestoreUrls(browser)) {
    if (ShouldOpenUrlsInNewBrowser(browser)) {
      // Delay creating a new browser until |browser| is activated.
      on_session_restored_callback_subscription_ =
          SessionRestore::RegisterOnSessionRestoredCallback(base::BindRepeating(
              &BrowserProcessPlatformPart::BrowserRestoreObserver::
                  OnSessionRestoreDone,
              base::Unretained(this)));
    } else {
      RestoreUrls(browser);
    }
  }

  // If the startup urls from LAST_AND_URLS pref are already opened in a new
  // browser, skip opening the same browser.
  if (browser->creation_source() ==
      Browser::CreationSource::kLastAndUrlsStartupPref) {
    DCHECK(on_session_restored_callback_subscription_);
    on_session_restored_callback_subscription_ = {};
  }
}

void BrowserProcessPlatformPart::BrowserRestoreObserver::OnSessionRestoreDone(
    Profile* profile,
    int num_tabs_restored) {
  // Ensure this callback to be called exactly once.
  on_session_restored_callback_subscription_ = {};

  // All browser windows are created. Open startup urls in a new browser.
  auto create_params = Browser::CreateParams(profile, /*user_gesture*/ false);
  Browser* browser = Browser::Create(create_params);
  RestoreUrls(browser);
  browser->window()->Show();
  browser->window()->Activate();
}

bool BrowserProcessPlatformPart::BrowserRestoreObserver::ShouldRestoreUrls(
    Browser* browser) const {
  Profile* profile = browser->profile();

  // Only open urls for regular sign in users.
  DCHECK(profile);
  if (!browser_process_platform_part_->CanRestoreUrlsForProfile(profile)) {
    return false;
  }

  // If during the restore process, or restore from a crash, don't launch urls.
  // However, in case of LAST_AND_URLS startup setting, urls should be opened
  // even when the restore session is in progress.
  SessionStartupPref pref =
      SessionStartupPref::GetStartupPref(browser->profile()->GetPrefs());
  if ((SessionRestore::IsRestoring(profile) &&
       pref.type != SessionStartupPref::LAST_AND_URLS) ||
      HasPendingUncleanExit(profile)) {
    return false;
  }

  // App windows should not be restored.
  auto window_type = WindowTypeForBrowserType(browser->type());
  if (window_type == sessions::SessionWindow::TYPE_APP ||
      window_type == sessions::SessionWindow::TYPE_APP_POPUP) {
    return false;
  }

  // If the browser is created by StartupBrowserCreator,
  // StartupBrowserCreatorImpl::OpenTabsInBrowser can open tabs, so don't
  // restore urls here.
  if (browser->creation_source() == Browser::CreationSource::kStartupCreator) {
    return false;
  }

  // If the startup setting is not open urls, don't launch urls.
  if (!pref.ShouldOpenUrls() || pref.urls.empty()) {
    return false;
  }

  return true;
}

// If the startup setting is both the restore last session and the open urls,
// those should be opened in a new browser.
bool BrowserProcessPlatformPart::BrowserRestoreObserver::
    ShouldOpenUrlsInNewBrowser(Browser* browser) const {
  SessionStartupPref pref =
      SessionStartupPref::GetStartupPref(browser->profile()->GetPrefs());
  return pref.type == SessionStartupPref::LAST_AND_URLS;
}

void BrowserProcessPlatformPart::BrowserRestoreObserver::RestoreUrls(
    Browser* browser) {
  DCHECK(browser);

  SessionStartupPref pref =
      SessionStartupPref::GetStartupPref(browser->profile()->GetPrefs());
  std::vector<GURL> urls;
  for (const auto& url : pref.urls) {
    urls.push_back(url);
  }

  custom_handlers::ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(browser->profile());
  for (const GURL& url : urls) {
    // We skip URLs that we'd have to launch an external protocol handler for.
    // This avoids us getting into an infinite loop asking ourselves to open
    // a URL, should the handler be (incorrectly) configured to be us. Anyone
    // asking us to open such a URL should really ask the handler directly.
    bool handled_by_chrome =
        ProfileIOData::IsHandledURL(url) ||
        (registry && registry->IsHandledProtocol(url.scheme()));
    if (!handled_by_chrome) {
      continue;
    }

    int add_types = AddTabTypes::ADD_NONE | AddTabTypes::ADD_FORCE_INDEX;
    NavigateParams params(browser, url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
    params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
    params.tabstrip_add_types = add_types;
    Navigate(&params);
  }
}

void BrowserProcessPlatformPart::InitializeAutomaticRebootManager() {
  DCHECK(!automatic_reboot_manager_);

  automatic_reboot_manager_ =
      std::make_unique<ash::system::AutomaticRebootManager>(
          base::DefaultClock::GetInstance(),
          base::DefaultTickClock::GetInstance());
}

void BrowserProcessPlatformPart::ShutdownAutomaticRebootManager() {
  automatic_reboot_manager_.reset();
}

void BrowserProcessPlatformPart::InitializeUserManager() {
  DCHECK(!user_manager_);
  CHECK(session_manager_);
  auto* local_state = g_browser_process->local_state();
  user_manager_ = std::make_unique<user_manager::UserManagerImpl>(
      std::make_unique<ash::UserManagerDelegateImpl>(), local_state,
      ash::CrosSettings::Get());
  profile_user_manager_controller_ =
      std::make_unique<ash::ProfileUserManagerController>(
          g_browser_process->profile_manager(), user_manager_.get());
  policy_user_manager_controller_ =
      std::make_unique<ash::PolicyUserManagerController>(
          user_manager_.get(), ash::CrosSettings::Get(),
          ash::DeviceSettingsService::Get(),
          browser_policy_connector_ash()->GetMinimumVersionPolicyHandler());
  user_image_manager_registry_ =
      std::make_unique<ash::UserImageManagerRegistry>(user_manager_.get());
  multi_user_sign_in_policy_controller_ =
      std::make_unique<user_manager::MultiUserSignInPolicyController>(
          local_state, user_manager_.get());
  session_manager_->OnUserManagerCreated(user_manager_.get());
  // LoginState and DeviceCloudPolicyManager outlives UserManager, so on
  // their initialization, there's no way to start observing UserManager.
  // This is the earliest timing to do so.
  // TODO(b/332481586): Consider move the initialization to the constructor
  // of each class.
  if (auto* login_state = ash::LoginState::Get()) {
    login_state->OnUserManagerCreated(user_manager_.get());
  }
  browser_policy_connector_ash()->OnUserManagerCreated(user_manager_.get());
  user_manager_->Initialize();
}

void BrowserProcessPlatformPart::ShutdownUserManager() {
  if (!user_manager_) {
    return;
  }
  user_image_manager_registry_->Shutdown();
  browser_policy_connector_ash()->OnUserManagerShutdown();
  policy_user_manager_controller_.reset();
  user_manager_->Shutdown();
}

void BrowserProcessPlatformPart::DestroyUserManager() {
  user_manager_->Destroy();
  browser_policy_connector_ash()->OnUserManagerWillBeDestroyed();
  if (auto* login_state = ash::LoginState::Get()) {
    login_state->OnUserManagerWillBeDestroyed(user_manager_.get());
  }

  multi_user_sign_in_policy_controller_.reset();
  user_image_manager_registry_.reset();
  profile_user_manager_controller_.reset();
  user_manager_.reset();
}

void BrowserProcessPlatformPart::
    InitializeDeviceRestrictionScheduleController() {
  device_restriction_schedule_controller_ =
      policy::DeviceRestrictionScheduleController::Create(
          CHECK_DEREF(g_browser_process->local_state()));
}

void BrowserProcessPlatformPart::ShutdownDeviceRestrictionScheduleController() {
  device_restriction_schedule_controller_.reset();
}

void BrowserProcessPlatformPart::InitializeDeviceDisablingManager() {
  DCHECK(!device_disabling_manager_);

  device_disabling_manager_delegate_ =
      std::make_unique<ash::system::DeviceDisablingManagerDefaultDelegate>();
  device_disabling_manager_ =
      std::make_unique<ash::system::DeviceDisablingManager>(
          device_disabling_manager_delegate_.get(), ash::CrosSettings::Get(),
          user_manager::UserManager::Get());
  device_disabling_manager_->Init();
}

void BrowserProcessPlatformPart::ShutdownDeviceDisablingManager() {
  device_disabling_manager_.reset();
  device_disabling_manager_delegate_.reset();
}

void BrowserProcessPlatformPart::InitializeSessionManager() {
  CHECK(ash::BootTimesRecorder::GetIfCreated());
  CHECK(!session_manager_);
  session_manager_ = std::make_unique<ash::ChromeSessionManager>();
  session_manager_->AddObserver(ash::BootTimesRecorder::Get());
}

void BrowserProcessPlatformPart::ShutdownSessionManager() {
  session_manager_->RemoveObserver(ash::BootTimesRecorder::Get());
  session_manager_.reset();
}

void BrowserProcessPlatformPart::InitializeCrosSettings() {
  CHECK(!cros_settings_holder_);
  cros_settings_holder_ = std::make_unique<ash::CrosSettingsHolder>(
      ash::DeviceSettingsService::Get(), g_browser_process->local_state());
}

void BrowserProcessPlatformPart::ShutdownCrosSettings() {
  cros_settings_holder_.reset();
}

void BrowserProcessPlatformPart::InitializeComponentManager() {
  if (using_testing_component_manager_ash_) {
    return;
  }

  DCHECK(!component_manager_ash_);
  component_manager_ash_ =
      base::MakeRefCounted<component_updater::CrOSComponentInstaller>(
          std::make_unique<component_updater::MetadataTable>(
              g_browser_process->local_state()),
          g_browser_process->component_updater());

  // Register all installed components for regular update.
  component_manager_ash_->RegisterInstalled();
}

void BrowserProcessPlatformPart::ShutdownComponentManager() {
  if (using_testing_component_manager_ash_) {
    return;
  }

  component_manager_ash_.reset();
}

void BrowserProcessPlatformPart::InitializeSchedulerConfigurationManager() {
  DCHECK(!scheduler_configuration_manager_);
  scheduler_configuration_manager_ =
      std::make_unique<ash::SchedulerConfigurationManager>(
          ash::DebugDaemonClient::Get(), g_browser_process->local_state());
}

void BrowserProcessPlatformPart::ShutdownSchedulerConfigurationManager() {
  scheduler_configuration_manager_.reset();
}

void BrowserProcessPlatformPart::InitializeAshProxyMonitor() {
  DCHECK(!ash_proxy_monitor_);
  ash_proxy_monitor_ = std::make_unique<ash::AshProxyMonitor>(
      g_browser_process->local_state(), g_browser_process->profile_manager());
}

void BrowserProcessPlatformPart::ShutdownAshProxyMonitor() {
  ash_proxy_monitor_.reset();
}

void BrowserProcessPlatformPart::InitializePrimaryProfileServices(
    Profile* primary_profile) {
  DCHECK(primary_profile);

  DCHECK(!essential_search_manager_);
  essential_search_manager_ =
      app_list::EssentialSearchManager::Create(primary_profile);

  DCHECK(!in_session_password_change_manager_);
  in_session_password_change_manager_ =
      ash::InSessionPasswordChangeManager::CreateIfEnabled(primary_profile);

  primary_profile_shutdown_subscription_ =
      PrimaryProfileServicesShutdownNotifierFactory::GetInstance()
          ->Get(primary_profile)
          ->Subscribe(base::BindRepeating(
              &BrowserProcessPlatformPart::ShutdownPrimaryProfileServices,
              base::Unretained(this)));

  if (ash::SystemProxyManager::Get()) {
    ash::SystemProxyManager::Get()->StartObservingPrimaryProfilePrefs(
        primary_profile);
  }

  // The current sesison may be guest session, where the Profile is
  // an OTR one. Take the original profile for the case.
  auto* user = user_manager::UserManager::Get()->FindUserAndModify(CHECK_DEREF(
      ash::AnnotatedAccountId::Get(primary_profile->GetOriginalProfile())));
  secure_dns_manager_ = std::make_unique<ash::SecureDnsManager>(
      g_browser_process->local_state(), CHECK_DEREF(user),
      primary_profile->GetProfilePolicyConnector()->IsManaged());
}

void BrowserProcessPlatformPart::ShutdownPrimaryProfileServices() {
  secure_dns_manager_.reset();
  if (ash::SystemProxyManager::Get())
    ash::SystemProxyManager::Get()->StopObservingPrimaryProfilePrefs();
  essential_search_manager_.reset();
  in_session_password_change_manager_.reset();
}

void BrowserProcessPlatformPart::RegisterKeepAlive() {
  DCHECK(!keep_alive_);
  keep_alive_ = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::BROWSER_PROCESS_CHROMEOS,
      KeepAliveRestartOption::DISABLED);
}

void BrowserProcessPlatformPart::UnregisterKeepAlive() {
  keep_alive_.reset();
}

ash::ProfileHelper* BrowserProcessPlatformPart::profile_helper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!created_profile_helper_)
    CreateProfileHelper();
  return profile_helper_.get();
}

policy::BrowserPolicyConnectorAsh*
BrowserProcessPlatformPart::browser_policy_connector_ash() {
  return static_cast<policy::BrowserPolicyConnectorAsh*>(
      g_browser_process->browser_policy_connector());
}

ash::system::TimeZoneResolverManager*
BrowserProcessPlatformPart::GetTimezoneResolverManager() {
  if (!timezone_resolver_manager_.get()) {
    timezone_resolver_manager_ =
        std::make_unique<ash::system::TimeZoneResolverManager>(
            ash::SimpleGeolocationProvider::GetInstance(),
            session_manager::SessionManager::Get());
  }
  return timezone_resolver_manager_.get();
}

void BrowserProcessPlatformPart::StartTearDown() {
  // Some tests check for memory leaks before this object is
  // destroyed.  So we need to destroy |timezone_resolver_manager_| here.
  timezone_resolver_manager_.reset();
  profile_helper_.reset();
  browser_context_flusher_.reset();
}

ash::system::SystemClock* BrowserProcessPlatformPart::GetSystemClock() {
  if (!system_clock_.get())
    system_clock_ = std::make_unique<ash::system::SystemClock>();
  return system_clock_.get();
}

void BrowserProcessPlatformPart::DestroySystemClock() {
  system_clock_.reset();
}

void BrowserProcessPlatformPart::CreateProfileHelper() {
  DCHECK(!created_profile_helper_ && !profile_helper_);
  created_profile_helper_ = true;
  profile_helper_ = ash::ProfileHelper::CreateInstance();
}

ash::AccountManagerFactory*
BrowserProcessPlatformPart::GetAccountManagerFactory() {
  return account_manager_factory_.get();
}

// static
void BrowserProcessPlatformPart::EnsureFactoryBuilt() {
  PrimaryProfileServicesShutdownNotifierFactory::GetInstance();
}

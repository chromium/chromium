// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/session/user_session_initializer.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/system/media/media_notification_provider.h"
#include "base/debug/crash_logging.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/accessibility/live_caption/system_live_caption_service_factory.h"
#include "chrome/browser/ash/arc/session/arc_service_launcher.h"
#include "chrome/browser/ash/boca/boca_manager_factory.h"
#include "chrome/browser/ash/calendar/calendar_keyed_service_factory.h"
#include "chrome/browser/ash/camera_mic/vm_camera_mic_manager.h"
#include "chrome/browser/ash/child_accounts/child_status_reporting_service_factory.h"
#include "chrome/browser/ash/child_accounts/child_user_service_factory.h"
#include "chrome/browser/ash/child_accounts/family_user_metrics_service_factory.h"
#include "chrome/browser/ash/child_accounts/screen_time_controller_factory.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/eche_app/eche_app_manager_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/lock_screen_apps/state_controller.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/ash/policy/reporting/app_install_event_log_manager_wrapper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/sparky/sparky_manager_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/component_updater/crl_set_component_installer.h"
#include "chrome/browser/google/google_brand_chromeos.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/screen_ai/screen_ai_dlc_installer.h"
#include "chrome/browser/ui/ash/birch/birch_keyed_service_factory.h"
#include "chrome/browser/ui/ash/glanceables/glanceables_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/ash/media_client/media_client_impl.h"
#include "chrome/browser/ui/webui/ash/settings/pages/privacy/peripheral_data_access_handler.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/pciguard/pciguard_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/ash/components/peripheral_notification/peripheral_notification_manager.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_factory.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/live_caption/caption_util.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/media_switches.h"

#if BUILDFLAG(ENABLE_RLZ)
#include "chrome/browser/rlz/chrome_rlz_tracker_delegate.h"
#include "components/rlz/rlz_tracker.h"  // nogncheck
#endif

namespace ash {

namespace {

UserSessionInitializer* g_instance = nullptr;

#if BUILDFLAG(ENABLE_RLZ)
// Flag file that disables RLZ tracking, when present.
const base::FilePath::CharType kRLZDisabledFlagName[] =
    FILE_PATH_LITERAL(".rlz_disabled");

base::FilePath GetRlzDisabledFlagPath() {
  base::FilePath homedir;
  base::PathService::Get(base::DIR_HOME, &homedir);
  return homedir.Append(kRLZDisabledFlagName);
}

UserSessionInitializer::RlzInitParams CollectRlzParams() {
  UserSessionInitializer::RlzInitParams params;
  params.disabled = base::PathExists(GetRlzDisabledFlagPath());
  params.time_since_oobe_completion =
      StartupUtils::GetTimeSinceOobeFlagFileCreation();
  return params;
}
#endif

void GetCertDBOnIOThread(
    NssCertDatabaseGetter database_getter,
    base::OnceCallback<void(net::NSSCertDatabase*)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  net::NSSCertDatabase* cert_db =
      std::move(database_getter).Run(std::move(split_callback.first));
  if (cert_db)
    std::move(split_callback.second).Run(cert_db);
}

// Configures the NetworkCertLoader for the primary user. This method is
// unsafe to call multiple times (e.g. for non-primary users).
// Note: This unsafely grabs a persistent pointer to the `NssService`'s
// `NSSCertDatabase` outside of the IO thread, and the `NSSCertDatabase`
// will be invalidated once the associated profile is shut down.
// TODO(crbug.com/40753707): Provide better lifetime guarantees and
// pass the Getter to the NetworkCertLoader.
void OnGotNSSCertDatabaseForUser(net::NSSCertDatabase* database) {
  if (!NetworkCertLoader::IsInitialized())
    return;

  NetworkCertLoader::Get()->SetUserNSSDB(database);
}

}  // namespace

UserSessionInitializer::UserSessionInitializer() {
  DCHECK(!g_instance);
  g_instance = this;
}

UserSessionInitializer::~UserSessionInitializer() {
  DCHECK(g_instance);
  g_instance = nullptr;
}

// static
UserSessionInitializer* UserSessionInitializer::Get() {
  return g_instance;
}

void UserSessionInitializer::OnUserProfileLoaded(const AccountId& account_id) {
  // TODO(b/371636008): Remove after fixing the crash.
  using user_manager::UserManager;
  SCOPED_CRASH_KEY_NUMBER("UserSessionInitializer", "LoggedInUsers",
                          UserManager::Get()->GetLoggedInUsers().size());
  SCOPED_CRASH_KEY_NUMBER(
      "UserSessionInitializer", "LoadedProfiles",
      g_browser_process->profile_manager()->GetLoadedProfiles().size());
  SCOPED_CRASH_KEY_BOOL("UserSessionInitializer", "FindUser",
                        UserManager::Get()->FindUser(account_id) != nullptr);
  if (auto* found_user = UserManager::Get()->FindUser(account_id);
      found_user != nullptr) {
    SCOPED_CRASH_KEY_NUMBER("UserSessionInitializer", "UserType",
                            static_cast<int>(found_user->GetType()));
    SCOPED_CRASH_KEY_BOOL("UserSessionInitializer", "ProfileCreated",
                          found_user->is_profile_created());
    SCOPED_CRASH_KEY_BOOL("UserSessionInitializer", "IsPrimary",
                          UserManager::Get()->GetPrimaryUser() == found_user);
    SCOPED_CRASH_KEY_BOOL("UserSessionInitializer", "IsActive",
                          UserManager::Get()->GetActiveUser() == found_user);
    SCOPED_CRASH_KEY_NUMBER("UserSessionInitializer", "NameHashSize",
                            found_user->username_hash().size());
  }

  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  CHECK(profile);
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  CHECK(user);

  if (user_manager::UserManager::Get()->GetPrimaryUser() == user) {
    // TODO(https://crbug.com/1208416): Investigate why OnUserProfileLoaded
    // is called more than once.
    if (primary_profile_ != nullptr) {
      NOTREACHED_IN_MIGRATION();
      CHECK_EQ(primary_profile_, profile);
      return;
    }
    primary_profile_ = profile;

    InitRlz(profile);
    InitializeCerts(profile);
    InitializeCRLSetFetcher();
    InitializePrimaryProfileServices(profile, user);

    FamilyUserMetricsServiceFactory::GetForBrowserContext(profile);
    if (chromeos::features::IsSparkyEnabled()) {
      ash::SparkyManagerServiceFactory::GetForProfile(profile);
    }
  }

  if (user->GetType() == user_manager::UserType::kChild) {
    InitializeChildUserServices(profile);
  }
}

void UserSessionInitializer::InitializeChildUserServices(Profile* profile) {
  ChildStatusReportingServiceFactory::GetForBrowserContext(profile);
  ChildUserServiceFactory::GetForBrowserContext(profile);
  ScreenTimeControllerFactory::GetForBrowserContext(profile);
}

void UserSessionInitializer::InitRlz(Profile* profile) {
#if BUILDFLAG(ENABLE_RLZ)
  // Initialize the brand code in the local prefs if it does not exist yet or
  // if it is empty.  The latter is to correct a problem in older builds where
  // an empty brand code would be persisted if the first login after OOBE was
  // a guest session.
  if (!g_browser_process->local_state()->HasPrefPath(::prefs::kRLZBrand) ||
      g_browser_process->local_state()
          ->GetValue(::prefs::kRLZBrand)
          .GetString()
          .empty()) {
    // Read brand code asynchronously from an OEM data and repost ourselves.
    google_brand::chromeos::InitBrand(base::BindOnce(
        &UserSessionInitializer::InitRlz, weak_factory_.GetWeakPtr(), profile));
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&CollectRlzParams),
      base::BindOnce(&UserSessionInitializer::InitRlzImpl,
                     weak_factory_.GetWeakPtr(), profile));
#endif
}

void UserSessionInitializer::InitializeCerts(Profile* profile) {
  // Now that the user profile has been initialized, the NSS database can
  // be used.
  if (NetworkCertLoader::IsInitialized() &&
      base::SysInfo::IsRunningOnChromeOS()) {
    // Note: This unsafely grabs a persistent reference to the `NssService`'s
    // `NSSCertDatabase`, which may be invalidated once `profile` is shut down.
    // TODO(crbug.com/40753707): Provide better lifetime guarantees and
    // pass the `NssCertDatabaseGetter` to the `NetworkCertLoader`.
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&GetCertDBOnIOThread,
                       NssServiceFactory::GetForContext(profile)
                           ->CreateNSSCertDatabaseGetterForIOThread(),
                       base::BindPostTaskToCurrentDefault(
                           base::BindOnce(&OnGotNSSCertDatabaseForUser))));
  }
}

void UserSessionInitializer::InitializeCRLSetFetcher() {
  component_updater::ComponentUpdateService* cus =
      g_browser_process->component_updater();
  if (cus)
    component_updater::RegisterCRLSetComponent(cus);
}

void UserSessionInitializer::InitializePrimaryProfileServices(
    Profile* profile,
    const user_manager::User* user) {
  // We should call this method at most once, when a user logs in. Logging out
  // kills the chrome process.
  static int call_count = 0;
  ++call_count;
  CHECK_EQ(call_count, 1);

  lock_screen_apps::StateController::Get()->SetPrimaryProfile(profile);

  if (user->GetType() == user_manager::UserType::kRegular) {
    // App install logs for extensions and ARC++ are uploaded via the user's
    // communication channel with the management server. This channel exists for
    // regular users only. `AppInstallEventLogManagerWrapper` and
    // `ExtensionInstallEventLogManagerWrapper` manages their own lifetime and
    // self-destruct on logout.
    policy::AppInstallEventLogManagerWrapper::CreateForProfile(profile);
  }

  arc::ArcServiceLauncher::Get()->OnPrimaryUserProfilePrepared(profile);
  guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile);

  crostini::CrostiniManager* crostini_manager =
      crostini::CrostiniManager::GetForProfile(profile);
  if (crostini_manager)
    crostini_manager->MaybeUpdateCrostini();

  if (captions::IsLiveCaptionFeatureSupported() &&
      features::IsSystemLiveCaptionEnabled()) {
    SystemLiveCaptionServiceFactory::GetInstance()->GetForProfile(profile);
  }

  g_browser_process->platform_part()->InitializePrimaryProfileServices(profile);
}

void UserSessionInitializer::InitializeScalableIph(Profile* profile) {
  ScalableIphFactory* scalable_iph_factory = ScalableIphFactory::GetInstance();
  CHECK(scalable_iph_factory);
  scalable_iph_factory->InitializeServiceForBrowserContext(profile);
}

void UserSessionInitializer::OnUserSessionStarted(bool is_primary_user) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);

  // Ensure that the `HoldingSpaceKeyedService` for `profile` is created.
  HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(profile);

  // Ensure that the `CalendarKeyedService` for `profile` is created. It is
  // created one per user in a multiprofile session.
  CalendarKeyedServiceFactory::GetInstance()->GetService(profile);

  // Ensure that the `GlanceablesKeyedService` for `profile` is created. It is
  // created one per user in a multiprofile session.
  GlanceablesKeyedServiceFactory::GetInstance()->GetService(profile);

  if (boca_util::IsEnabled()) {
    // Ensure that the `BocaManager` for `profile` is created. It is created one
    // per user in a multiprofile session.
    BocaManagerFactory::GetInstance()->GetForProfile(profile);
  }

  screen_ai::dlc_installer::ManageInstallation(
      g_browser_process->local_state());

  if (is_primary_user) {
    DCHECK_EQ(primary_profile_, profile);

    // Ensure that one `BirchKeyedService` is created for the primary profile.
    BirchKeyedServiceFactory::GetInstance()->GetService(profile);

    // Ensure that PhoneHubManager and EcheAppManager are created for the
    // primary profile.
    phonehub::PhoneHubManagerFactory::GetForProfile(profile);
    eche_app::EcheAppManagerFactory::GetForProfile(profile);

    // `ScalableIph` depends on `PhoneHubManager`. Initialize after
    // `PhoneHubManager`.
    InitializeScalableIph(profile);

    plugin_vm::PluginVmManager* plugin_vm_manager =
        plugin_vm::PluginVmManagerFactory::GetForProfile(primary_profile_);
    if (plugin_vm_manager)
      plugin_vm_manager->OnPrimaryUserSessionStarted();

    VmCameraMicManager::Get()->OnPrimaryUserSessionStarted(primary_profile_);

    // Pciguard can only be set by non-guest, primary users. By default,
    // Pciguard is turned on.
    if (PeripheralNotificationManager::IsInitialized()) {
      PeripheralNotificationManager::Get()->SetPcieTunnelingAllowedState(
          settings::PeripheralDataAccessHandler::GetPrefState());
    }
    PciguardClient::Get()->SendExternalPciDevicesPermissionState(
        settings::PeripheralDataAccessHandler::GetPrefState());
    TypecdClient::Get()->SetPeripheralDataAccessPermissionState(
        settings::PeripheralDataAccessHandler::GetPrefState());

    CrasAudioHandler::Get()->RefreshNoiseCancellationState();

    MediaNotificationProvider::Get()->OnPrimaryUserSessionStarted();
    if (base::FeatureList::IsEnabled(media::kShowForceRespectUiGainsToggle)) {
      CrasAudioHandler::Get()->RefreshForceRespectUiGainsState();
    }

    if (features::IsAudioHFPMicSRToggleEnabled()) {
      CrasAudioHandler::Get()->RefreshHfpMicSrState();
    }

    CrasAudioHandler::Get()->RefreshStyleTransferState();
  }
}

void UserSessionInitializer::OnUserSessionStartUpTaskCompleted() {
  const AccountId& account_id =
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();
  SessionManagerClient::Get()->EmitStartedUserSession(
      cryptohome::CreateAccountIdentifierFromAccountId(account_id));
}

void UserSessionInitializer::PreStartSession(bool is_primary_session) {
  if (is_primary_session) {
    NetworkCertLoader::Get()->MarkUserNSSDBWillBeInitialized();
  }
}

void UserSessionInitializer::InitRlzImpl(Profile* profile,
                                         const RlzInitParams& params) {
#if BUILDFLAG(ENABLE_RLZ)
  // If RLZ is disabled then clear the brand for the session.
  //
  // RLZ is disabled if disabled explicitly OR if the device's enrollment
  // state is not yet known. The device's enrollment state is definitively
  // known once the device is locked. Note that for enrolled devices, the
  // enrollment login locks the device.
  //
  // There the following cases to consider when a session starts:
  //
  // 1) This is a regular session.
  // 1a) The device is LOCKED. Thus, the enrollment state is KNOWN.
  // 1b) The device is NOT LOCKED. This should only happen on the first
  //     regular login (due to lock race condition with this code) if the
  //     device is NOT enrolled; thus, the enrollment state is also KNOWN.
  //
  // 2) This is a guest session.
  // 2a) The device is LOCKED. Thus, the enrollment state is KNOWN.
  // 2b) The device is NOT locked. This should happen if ONLY Guest mode
  //     sessions have ever been used on this device. This is the only
  //     situation where the enrollment state is NOT KNOWN at this point.

  PrefService* local_state = g_browser_process->local_state();
  if (params.disabled || (profile->IsGuestSession() &&
                          !InstallAttributes::Get()->IsDeviceLocked())) {
    // Empty brand code means an organic install (no RLZ pings are sent).
    google_brand::chromeos::ClearBrandForCurrentSession();
  }
  if (params.disabled != local_state->GetBoolean(::prefs::kRLZDisabled)) {
    // When switching to RLZ enabled/disabled state, clear all recorded events.
    rlz::RLZTracker::ClearRlzState();
    local_state->SetBoolean(::prefs::kRLZDisabled, params.disabled);
  }
  // Init the RLZ library.
  int ping_delay =
      profile->GetPrefs()->GetInteger(::prefs::kRlzPingDelaySeconds);
  // Negative ping delay means to send ping immediately after a first search is
  // recorded.
  bool send_ping_immediately = ping_delay < 0;
  base::TimeDelta delay =
      base::Seconds(abs(ping_delay)) - params.time_since_oobe_completion;
  rlz::RLZTracker::SetRlzDelegate(
      base::WrapUnique(new ChromeRLZTrackerDelegate));
  rlz::RLZTracker::InitRlzDelayed(
      user_manager::UserManager::Get()->IsCurrentUserNew(),
      send_ping_immediately, delay,
      ChromeRLZTrackerDelegate::IsGoogleDefaultSearch(profile),
      ChromeRLZTrackerDelegate::IsGoogleHomepage(profile),
      ChromeRLZTrackerDelegate::IsGoogleInStartpages(profile));
#endif
  if (init_rlz_impl_closure_for_testing_)
    std::move(init_rlz_impl_closure_for_testing_).Run();
  inited_for_testing_ = true;
}

}  // namespace ash

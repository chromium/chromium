// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/session/user_session_initializer.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/arc/session/arc_service_launcher.h"
#include "chrome/browser/chromeos/child_accounts/child_status_reporting_service_factory.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service_factory.h"
#include "chrome/browser/chromeos/child_accounts/family_user_metrics_service_factory.h"
#include "chrome/browser/chromeos/child_accounts/screen_time_controller_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/chromeos/policy/app_install_event_log_manager_wrapper.h"
#include "chrome/browser/chromeos/policy/extension_install_event_log_manager_wrapper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/component_updater/crl_set_component_installer.h"
#include "chrome/browser/component_updater/sth_set_component_remover.h"
#include "chrome/browser/google/google_brand_chromeos.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/ui/ash/clipboard_image_model_factory_impl.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(ENABLE_RLZ)
#include "chrome/browser/rlz/chrome_rlz_tracker_delegate.h"
#include "components/rlz/rlz_tracker.h"
#endif

namespace chromeos {

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
      chromeos::StartupUtils::GetTimeSinceOobeFlagFileCreation();
  return params;
}
#endif

// Callback to GetNSSCertDatabaseForProfile. It passes the user-specific NSS
// database to NetworkCertLoader. It must be called for primary user only.
void OnGetNSSCertDatabaseForUser(net::NSSCertDatabase* database) {
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
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);

  if (user_manager::UserManager::Get()->GetPrimaryUser() == user) {
    InitRlz(profile);
    InitializeCerts(profile);
    InitializeCRLSetFetcher();
    InitializeCertificateTransparencyComponents(user);
    InitializePrimaryProfileServices(profile, user);

    FamilyUserMetricsServiceFactory::GetForBrowserContext(profile);
  }

  if (user->GetType() == user_manager::USER_TYPE_CHILD)
    InitializeChildUserServices(profile);
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
  if (!g_browser_process->local_state()->HasPrefPath(prefs::kRLZBrand) ||
      g_browser_process->local_state()
          ->Get(prefs::kRLZBrand)
          ->GetString()
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
  // Now that the user profile has been initialized
  // |GetNSSCertDatabaseForProfile| is safe to be used.
  if (NetworkCertLoader::IsInitialized() &&
      base::SysInfo::IsRunningOnChromeOS()) {
    GetNSSCertDatabaseForProfile(profile,
                                 base::Bind(&OnGetNSSCertDatabaseForUser));
  }
}

void UserSessionInitializer::InitializeCRLSetFetcher() {
  component_updater::ComponentUpdateService* cus =
      g_browser_process->component_updater();
  if (cus)
    component_updater::RegisterCRLSetComponent(cus);
}

void UserSessionInitializer::InitializeCertificateTransparencyComponents(
    const user_manager::User* user) {
  const std::string username_hash = user->username_hash();
  if (!username_hash.empty()) {
    base::FilePath path =
        ProfileHelper::GetProfilePathByUserIdHash(username_hash);
    component_updater::DeleteLegacySTHSet(path);
  }
}

void UserSessionInitializer::InitializePrimaryProfileServices(
    Profile* profile,
    const user_manager::User* user) {
  lock_screen_apps::StateController::Get()->SetPrimaryProfile(profile);

  if (user->GetType() == user_manager::USER_TYPE_REGULAR) {
    // App install logs for extensions and ARC++ are uploaded via the user's
    // communication channel with the management server. This channel exists for
    // regular users only. |AppInstallEventLogManagerWrapper| and
    // |ExtensionInstallEventLogManagerWrapper| manages their own lifetime and
    // self-destruct on logout.
    policy::AppInstallEventLogManagerWrapper::CreateForProfile(profile);
    policy::ExtensionInstallEventLogManagerWrapper::CreateForProfile(profile);
  }

  arc::ArcServiceLauncher::Get()->OnPrimaryUserProfilePrepared(profile);

  plugin_vm::PluginVmManager* plugin_vm_manager =
      plugin_vm::PluginVmManagerFactory::GetForProfile(profile);
  if (plugin_vm_manager)
    plugin_vm_manager->OnPrimaryUserProfilePrepared();

  crostini::CrostiniManager* crostini_manager =
      crostini::CrostiniManager::GetForProfile(profile);
  if (crostini_manager)
    crostini_manager->MaybeUpdateCrostini();

  if (chromeos::features::IsClipboardHistoryEnabled()) {
    clipboard_image_model_factory_impl_ =
        std::make_unique<ClipboardImageModelFactoryImpl>(profile);
  }

  g_browser_process->platform_part()->InitializePrimaryProfileServices(profile);
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
  if (params.disabled != local_state->GetBoolean(prefs::kRLZDisabled)) {
    // When switching to RLZ enabled/disabled state, clear all recorded events.
    rlz::RLZTracker::ClearRlzState();
    local_state->SetBoolean(prefs::kRLZDisabled, params.disabled);
  }
  // Init the RLZ library.
  int ping_delay = profile->GetPrefs()->GetInteger(prefs::kRlzPingDelaySeconds);
  // Negative ping delay means to send ping immediately after a first search is
  // recorded.
  bool send_ping_immediately = ping_delay < 0;
  base::TimeDelta delay = base::TimeDelta::FromSeconds(abs(ping_delay)) -
                          params.time_since_oobe_completion;
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
}

}  // namespace chromeos

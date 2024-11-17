// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/session/chrome_session_manager.h"

#include <memory>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_vm_data_migration_status.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/webui/shimless_rma/shimless_rma.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/syslog_logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ash/account_manager/account_manager_util.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_mode/app_launch_utils.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_cryptohome_remover.h"
#include "chrome/browser/ash/boot_times_recorder/boot_times_recorder.h"
#include "chrome/browser/ash/login/chrome_restart_request.h"
#include "chrome/browser/ash/login/demo_mode/demo_components.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/enterprise_user_session_metrics.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/session/session_length_limiter.h"
#include "chrome/browser/ash/login/session/user_session_initializer.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/profiles/signin_profile_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/login/login_display_host_webui.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/arc_vm_data_migration_screen_handler.h"
#include "chrome/browser/ui/webui/ash/shimless_rma_dialog/shimless_rma_dialog.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/rmad/rmad_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/login/integrity/misconfigured_user_cleaner.h"
#include "chromeos/ash/components/osauth/public/auth_hub.h"
#include "components/account_id/account_id.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/user_manager/common_types.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/mojom/feature_session_type.mojom.h"

namespace ash {

namespace {

// Starts kiosk app launch and shows the splash screen.
void StartKioskSession(KioskAppId app, bool is_auto_launch = false) {
  // Kiosk app launcher starts with login state.
  CHECK_DEREF(session_manager::SessionManager::Get())
      .SetSessionState(session_manager::SessionState::LOGIN_PRIMARY);

  CHECK_DEREF(input_method::InputMethodManager::Get())
      .GetActiveIMEState()
      ->SetInputMethodLoginDefault();

  // Manages its own lifetime. See ShutdownDisplayHost().
  auto* display_host = new LoginDisplayHostWebUI();
  display_host->StartKiosk(app, is_auto_launch);

  // Login screen is skipped but 'login-prompt-visible' signal is still needed.
  VLOG(1) << "Kiosk app auto launch >> login-prompt-visible";
  SessionManagerClient::Get()->EmitLoginPromptVisible();
}

void StartAutoLaunchKioskSession() {
  auto app = KioskController::Get().GetAutoLaunchApp();
  CHECK(app.has_value());

  StartKioskSession(app.value().id(), /*is_auto_launch=*/true);
}

// Starts the login/oobe screen.
void StartLoginOobeSession() {
  // State will be defined once out-of-box/login branching is complete.
  ShowLoginWizard(OOBE_SCREEN_UNKNOWN);

  // Reset reboot after update flag when login screen is shown.
  if (!ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
    PrefService* local_state = g_browser_process->local_state();
    local_state->ClearPref(prefs::kRebootAfterUpdate);
  }
}

// Seed the stub user account in the same way as it's done in
// `UserSessionManager::InitProfilePreferences` for regular users.
void UpsertStubUserToAccountManager(Profile* user_profile,
                                    const user_manager::User* user) {
  // 1. Make sure that the account is present in
  // `account_manager::AccountManager`.
  account_manager::AccountManager* account_manager =
      g_browser_process->platform_part()
          ->GetAccountManagerFactory()
          ->GetAccountManager(user_profile->GetPath().value());

  DCHECK(account_manager->IsInitialized());

  const ::account_manager::AccountKey account_key{
      user->GetAccountId().GetGaiaId(), account_manager::AccountType::kGaia};

  account_manager->UpsertAccount(
      account_key, /*raw_email=*/user->GetDisplayEmail(),
      account_manager::AccountManager::kInvalidToken);

  DCHECK(account_manager->IsTokenAvailable(account_key));

  // 2. Seed it into `IdentityManager`.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(user_profile);
  signin::AccountsMutator* accounts_mutator =
      identity_manager->GetAccountsMutator();
  CoreAccountId account_id = accounts_mutator->SeedAccountInfo(
      user->GetAccountId().GetGaiaId(), user->GetDisplayEmail());

  // 3. Set it as the Primary Account.
  identity_manager->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_id, signin::ConsentLevel::kSync);

  CHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  CHECK_EQ(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync).gaia,
      user->GetAccountId().GetGaiaId());

  DCHECK_EQ(account_id, identity_manager->GetPrimaryAccountId(
                            signin::ConsentLevel::kSignin));
  VLOG(1) << "Seed IdentityManager for stub account, "
          << "success=" << !account_id.empty();
}

// Starts Chrome with an existing user session. Possible cases:
// 1. Chrome is restarted after crash.
// 2. Chrome is restarted for Guest session.
// 3. Chrome is started in browser_tests skipping the login flow.
// 4. Chrome is started on dev machine i.e. not on Chrome OS device w/o
//    login flow. In that case --login-user=[user_manager::kStubUserEmail] is
//    added. See PreEarlyInitialization().
void StartUserSession(user_manager::UserManager* user_manager,
                      Profile* user_profile,
                      const std::string& login_user_id) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  bool is_running_test = command_line->HasSwitch(::switches::kTestName) ||
                         command_line->HasSwitch(::switches::kTestType);

  if (command_line->HasSwitch(switches::kLoginUser)) {
    // TODO(crbug.com/41467249): There's a lot of code duplication with
    // UserSessionManager::FinalizePrepareProfile, which is (only!) run for
    // regular session starts. This needs to be refactored.

    // This is done in SessionManager::OnProfileCreated during normal login.
    UserSessionManager* user_session_mgr = UserSessionManager::GetInstance();
    const user_manager::User* user = user_manager->GetActiveUser();
    if (!user) {
      // This is possible if crash occured after profile removal
      // (see crbug.com/178290 for some more info).
      LOG(ERROR) << "Could not get active user after crash.";
      return;
    }

    auto* demo_session = DemoSession::Get();
    // In demo session, delay starting user session until the demo
    // session resources have been loaded.
    if (demo_session && demo_session->started() && demo_session->components() &&
        !demo_session->components()->resources_component_loaded()) {
      demo_session->EnsureResourcesLoaded(base::BindOnce(
          &StartUserSession, user_manager, user_profile, login_user_id));
      LOG(WARNING) << "Delay demo user session start until demo "
                   << "resources are loaded";
      return;
    }

    SigninProfileHandler::Get()->ProfileStartUp(user_profile);

    if (!is_running_test &&
        user->GetAccountId() == user_manager::StubAccountId()) {
      // Add stub user to Account Manager. (But not when running tests: this
      // allows tests to setup appropriate environment)
      InitializeAccountManager(
          user_profile->GetPath(),
          /*initialization_callback=*/base::BindOnce(
              &UpsertStubUserToAccountManager, user_profile, user));
    }

    user_session_mgr->OnUserProfileLoaded(user_profile, user);

    // This call will set session state to SESSION_STATE_ACTIVE (same one).
    session_manager::SessionManager::Get()->SessionStarted();

    // Now is the good time to retrieve other logged in users for this session.
    // First user has been already marked as logged in and active in
    // PreProfileInit(). Restore sessions for other users in the background.
    user_session_mgr->RestoreActiveSessions();

    // Chrome restart with existing user sessions and the last active user
    // profile is loaded. Notify ash to signal post login work done at this
    // stage for chrome restart.
    ash::Shell::Get()->login_unlock_throughput_recorder()->OnAshRestart();
  }

  if (!is_running_test) {
    // We did not log in (we crashed or are debugging), so we need to
    // restore Sync.
    UserSessionManager::GetInstance()->RestoreAuthenticationSession(
        user_profile);

    UserSessionManager::GetInstance()->StartTetherServiceIfPossible(
        user_profile);

    // Associates AppListClient with the current active profile.
    AppListClientImpl::GetInstance()->UpdateProfile();
  }

  if (base::FeatureList::IsEnabled(features::kEolWarningNotifications) &&
      !user_profile->GetProfilePolicyConnector()->IsManaged()) {
    UserSessionManager::GetInstance()->CheckEolInfo(user_profile);
  }

  UserSessionManager::GetInstance()->ShowNotificationsIfNeeded(user_profile);
  UserSessionManager::GetInstance()->PerformPostBrowserLaunchOOBEActions(
      user_profile);

  // If we have recently restarted in-session after a chrome crash, we need
  // to initialize `AuthHub` in in-session mode.
  // See documentation in `auth_hub.h` for more details.
  AuthHub::Get()->InitializeForMode(AuthHubMode::kInSession);
}

void LaunchShimlessRma() {
  VLOG(1) << "ChromeSessionManager::LaunchShimlessRma";
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::RMA);

  ShimlessRmaDialog::ShowDialog();
  // Login screen is skipped but 'login-prompt-visible' signal is still
  // needed.
  VLOG(1) << "Shimless RMA app auto launch >> login-prompt-visible";
  SessionManagerClient::Get()->EmitLoginPromptVisible();
}

// The callback invoked when RmadClient determines that RMA is required.
void OnRmaIsRequiredResponse() {
  VLOG(1) << "ChromeSessionManager::OnRmaIsRequiredResponse";
  switch (session_manager::SessionManager::Get()->session_state()) {
    case session_manager::SessionState::UNKNOWN:
      LOG(ERROR) << "OnRmaIsRequiredResponse callback triggered unexpectedly";
      break;
    case session_manager::SessionState::RMA:
      // Already in RMA, do nothing.
      break;
    // Restart Chrome and launch RMA from any session state as the user is
    // expecting to be in RMA.
    case session_manager::SessionState::ACTIVE:
    case session_manager::SessionState::LOCKED:
    case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
    case session_manager::SessionState::LOGIN_PRIMARY:
    case session_manager::SessionState::LOGIN_SECONDARY:
    case session_manager::SessionState::OOBE: {
      auto* existing_user_controller =
          ExistingUserController::current_controller();
      if (!existing_user_controller ||
          !existing_user_controller->IsSigninInProgress()) {
        if (existing_user_controller) {
          existing_user_controller->StopAutoLoginTimer();
        }
        // Append the kLaunchRma flag and restart Chrome to force launch RMA.
        const base::CommandLine& browser_command_line =
            *base::CommandLine::ForCurrentProcess();
        base::CommandLine command_line(browser_command_line);
        command_line.AppendSwitch(switches::kLaunchRma);
        RestartChrome(command_line, RestartChromeReason::kUserless);
        break;
      }
    }
  }
}

bool MaybeStartArcVmDataMigration(user_manager::UserManager* user_manager,
                                  Profile* profile) {
  // Migration should be performed only when the session is restarted with the
  // primary user.
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  if (user && user_manager->GetPrimaryUser() == user) {
    arc::ArcVmDataMigrationStatus data_migration_status =
        arc::GetArcVmDataMigrationStatus(profile->GetPrefs());
    if (data_migration_status == arc::ArcVmDataMigrationStatus::kConfirmed ||
        data_migration_status == arc::ArcVmDataMigrationStatus::kStarted) {
      ShowLoginWizard(ArcVmDataMigrationScreenView::kScreenId);
      return true;
    }
  }
  return false;
}

// NOTE: This has to be called before profile is initialized - so it is set up
// when extension are loaded during profile initialization.
void InitFeaturesSessionType(const user_manager::User* user) {
  // Kiosk session should be set as part of kiosk user session initialization
  // in normal circumstances (to be able to properly determine whether kiosk
  // was auto-launched); in case of user session restore, feature session
  // type has be set before kiosk app controller takes over, as at that point
  // kiosk app profile would already be initialized - feature session type
  // should be set before that.
  if (user->IsKioskType()) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kLoginUser)) {
      // For kiosk session crash recovery, feature session type has be set
      // before kiosk app controller takes over, as at that point iosk app
      // profile would already be initialized - feature session type
      // should be set before that.
      bool auto_launched = base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAppAutoLaunched);
      extensions::SetCurrentFeatureSessionType(
          auto_launched
              ? extensions::mojom::FeatureSessionType::kAutolaunchedKiosk
              : extensions::mojom::FeatureSessionType::kKiosk);
    }
    return;
  }

  extensions::SetCurrentFeatureSessionType(
      user->HasGaiaAccount() ? extensions::mojom::FeatureSessionType::kRegular
                             : extensions::mojom::FeatureSessionType::kUnknown);
}

}  // namespace

ChromeSessionManager::ChromeSessionManager()
    : oobe_configuration_(std::make_unique<OobeConfiguration>()),
      user_session_initializer_(std::make_unique<UserSessionInitializer>()) {
  AddObserver(user_session_initializer_.get());
}

ChromeSessionManager::~ChromeSessionManager() {
  RemoveObserver(user_session_initializer_.get());
}

// static
void ChromeSessionManager::RegisterPrefs(PrefRegistrySimple* registry) {
  SessionLengthLimiter::RegisterPrefs(registry);
  enterprise_user_session_metrics::RegisterPrefs(registry);
}

void ChromeSessionManager::OnUserManagerCreated(
    user_manager::UserManager* user_manager) {
  user_manager_ = user_manager;
  user_manager_observation_.Observe(user_manager_);

  // Record the stored session length for enrolled device.
  if (ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
    enterprise_user_session_metrics::RecordStoredSessionLength();
  }
}

void ChromeSessionManager::Initialize(
    const base::CommandLine& parsed_command_line,
    Profile* profile,
    bool is_running_test) {
  auto& local_state = CHECK_DEREF(g_browser_process->local_state());
  // If a forced powerwash was triggered and no confirmation from the user is
  // necessary, we trigger the device wipe here before the user can log in again
  // and return immediately because there is no need to show the login screen.
  if (local_state.GetBoolean(prefs::kForceFactoryReset)) {
    SessionManagerClient::Get()->StartDeviceWipe(base::DoNothing());
    return;
  }

  if (shimless_rma::IsShimlessRmaAllowed()) {
    // If we should be in Shimless RMA, start it and skip the rest of
    // initialization.
    if (shimless_rma::HasLaunchRmaSwitchAndIsAllowed()) {
      LaunchShimlessRma();
      return;
    }

    // If the RMA state is detected later, OnRmaIsRequiredResponse() is invoked
    // to append the kLaunchRma switch and restart Chrome in RMA mode.
    RmadClient::Get()->SetRmaRequiredCallbackForSessionManager(
        base::BindOnce(&OnRmaIsRequiredResponse));
  } else {
    VLOG(1) << "ChromeSessionManager::Initialize Shimless RMA is not allowed";
  }

  if (base::FeatureList::IsEnabled(arc::kEnableArcVmDataMigration) &&
      MaybeStartArcVmDataMigration(user_manager_, profile)) {
    return;
  }

  // Tests should be able to tune login manager before showing it. Thus only
  // show login UI (login and out-of-box) in normal (non-testing) mode with
  // --login-manager switch and if test passed --force-login-manager-in-tests.
  bool force_login_screen_in_test =
      parsed_command_line.HasSwitch(switches::kForceLoginManagerInTests);

  const user_manager::CryptohomeId cryptohome_id(
      parsed_command_line.GetSwitchValueASCII(switches::kLoginUser));
  user_manager::KnownUser known_user(&local_state);
  const AccountId login_account_id(
      known_user.GetAccountIdByCryptohomeId(cryptohome_id));

  KioskCryptohomeRemover::RemoveObsoleteCryptohomes();

  if (ShouldOneTimeAutoLaunchKioskApp(parsed_command_line, local_state)) {
    VLOG(1) << "One time auto launching kiosk app";
    KioskAppId app_id = ExtractOneTimeAutoLaunchKioskAppId(local_state);
    StartKioskSession(app_id);
  } else if (ShouldAutoLaunchKioskApp(parsed_command_line, local_state)) {
    VLOG(1) << "Starting Chrome with kiosk auto launch.";
    StartAutoLaunchKioskSession();
  } else if (parsed_command_line.HasSwitch(switches::kLoginManager)) {
    oobe_configuration_->CheckConfiguration();
    if (is_running_test && !force_login_screen_in_test) {
      return;
    }
    VLOG(1) << "Starting Chrome with login/oobe screen.";
    StartLoginOobeSession();
  } else {
    VLOG(1) << "Starting Chrome with a user session.";
    StartUserSession(user_manager_, profile, login_account_id.GetUserEmail());
  }
}

void ChromeSessionManager::Shutdown() {
  if (session_length_limiter_ &&
      ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
    // Store session length before tearing down `session_length_limiter_` for
    // enrolled devices so that it can be reported on the next run.
    const base::TimeDelta session_length =
        session_length_limiter_->GetSessionDuration();
    if (!session_length.is_zero()) {
      enterprise_user_session_metrics::StoreSessionLength(
          user_manager_->GetActiveUser()->GetType(), session_length);
    }
  }
  session_length_limiter_.reset();
}

void ChromeSessionManager::SessionStarted() {
  session_manager::SessionManager::SessionStarted();
  SetSessionState(session_manager::SessionState::ACTIVE);

  // Notifies UserManager so that it can update login state.
  user_manager_->OnSessionStarted();
}

void ChromeSessionManager::NotifyUserLoggedIn(const AccountId& user_account_id,
                                              const std::string& user_id_hash,
                                              bool browser_restart,
                                              bool is_child) {
  BootTimesRecorder* btl = BootTimesRecorder::Get();
  btl->AddLoginTimeMarker("UserLoggedIn-Start", false);
  session_manager::SessionManager::NotifyUserLoggedIn(
      user_account_id, user_id_hash, browser_restart, is_child);

  if (user_manager_->GetLoggedInUsers().size() == 1) {
    InitFeaturesSessionType(user_manager_->GetPrimaryUser());
  }

  // Initialize the session length limiter and start it only if
  // session limit is defined by the policy.
  session_length_limiter_ = std::make_unique<SessionLengthLimiter>(
      /*delegate=*/nullptr, browser_restart);

  btl->AddLoginTimeMarker("UserLoggedIn-End", false);
}

void ChromeSessionManager::OnUsersSignInConstraintsChanged() {
  const user_manager::UserList& logged_in_users =
      user_manager_->GetLoggedInUsers();
  for (user_manager::User* user : logged_in_users) {
    if (user->IsDeviceLocalAccount()) {
      continue;
    }
    if (!user_manager_->IsUserAllowed(*user)) {
      SYSLOG(ERROR)
          << "The current user is not allowed, terminating the session.";
      chrome::AttemptUserExit();
    }
  }
}

}  // namespace ash

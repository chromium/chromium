// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/login_display_host_mojo.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/cpp/login/login_utils.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/style/color_palette_controller.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/mojo_system_info_dispatcher.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/screens/chrome_user_selection_screen.h"
#include "chrome/browser/ash/login/screens/gaia_screen.h"
#include "chrome/browser/ash/login/security_token_session_controller.h"
#include "chrome/browser/ash/login/user_board_view_mojo.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/certificate_provider/pin_dialog_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login_screen_client_impl.h"
#include "chrome/browser/ui/ash/system_tray_client_impl.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/browser/ui/webui/ash/login/enable_adb_sideloading_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_password_changed_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/kiosk_autolaunch_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/lacros_data_migration_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/os_install_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/reset_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/signin_fatal_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/login/auth/public/auth_types.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/startup_metric_utils/common/startup_metric_utils.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/views/view.h"

namespace ash {
namespace {

chromeos::CertificateProviderService* GetLoginScreenCertProviderService() {
  DCHECK(ProfileHelper::IsSigninProfileInitialized());
  return chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
      ProfileHelper::GetSigninProfile());
}

// Returns true iff
// (i)   log in is restricted to some user list,
// (ii)  all users in the restricted list are present.
bool AllAllowlistedUsersPresent() {
  CrosSettings* cros_settings = CrosSettings::Get();
  bool allow_new_user = false;
  cros_settings->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  if (allow_new_user) {
    return false;
  }

  bool allow_family_link = false;
  cros_settings->GetBoolean(kAccountsPrefFamilyLinkAccountsAllowed,
                            &allow_family_link);
  if (allow_family_link) {
    return false;
  }

  const base::Value::List* allowlist = nullptr;
  if (!cros_settings->GetList(kAccountsPrefUsers, &allowlist) || !allowlist) {
    return false;
  }
  for (const base::Value& i : *allowlist) {
    const std::string* allowlisted_user = i.GetIfString();
    // NB: Wildcards in the allowlist are also detected as not present here.
    if (!allowlisted_user || !user_manager::UserManager::Get()->IsKnownUser(
                                 AccountId::FromUserEmail(*allowlisted_user))) {
      return false;
    }
  }
  return true;
}

bool IsLazyWebUILoadingEnabled() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableOobeTestAPI)) {
    // Load WebUI for the test API explicitly because it's Web API.
    return false;
  }

  // Policy override.
  if (g_browser_process->local_state()->IsManagedPreference(
          prefs::kLoginScreenWebUILazyLoading)) {
    return g_browser_process->local_state()->GetBoolean(
        ash::prefs::kLoginScreenWebUILazyLoading);
  }

  // Feature override.
  if (base::FeatureList::GetInstance()->IsFeatureOverridden(
          features::kEnableLazyLoginWebUILoading.name)) {
    return base::FeatureList::IsEnabled(features::kEnableLazyLoginWebUILoading);
  }

  // Disable for stable and beta.
  if ((chrome::GetChannel() == version_info::Channel::STABLE) ||
      (chrome::GetChannel() == version_info::Channel::BETA)) {
    return false;
  }

  // Enable for dev builds.
  return true;
}

void ShowOwnerPod(const AccountId& owner) {
  const user_manager::User* device_owner =
      user_manager::UserManager::Get()->FindUser(owner);
  CHECK(device_owner);

  std::vector<LoginUserInfo> user_info_list;
  LoginUserInfo user_info;
  user_info.basic_user_info.type = device_owner->GetType();
  user_info.basic_user_info.account_id = device_owner->GetAccountId();
  user_info.basic_user_info.display_name =
      base::UTF16ToUTF8(device_owner->GetDisplayName());
  user_info.basic_user_info.display_email = device_owner->display_email();
  user_info.basic_user_info.avatar = BuildAshUserAvatarForUser(*device_owner);
  user_info.auth_type = proximity_auth::mojom::AuthType::OFFLINE_PASSWORD;
  user_info.is_signed_in = device_owner->is_logged_in();
  user_info.is_device_owner = true;
  user_info.can_remove = false;
  user_info_list.push_back(user_info);

  LoginScreen::Get()->GetModel()->SetUserList(user_info_list);
  LoginScreen::Get()->SetAllowLoginAsGuest(false);
  LoginScreen::Get()->EnableAddUserButton(false);

  // Disable PIN.
  LoginScreen::Get()->GetModel()->SetPinEnabledForUser(owner,
                                                       /*enabled=*/false);
}

void UpdatePinAuthAvailability(const AccountId& account_id) {
  quick_unlock::PinBackend::GetInstance()->CanAuthenticate(
      // Currently if PIN is cryptohome-based, PinCanAuthenticate always return
      // true if there's a set up PIN, even if the quick unlock policy disables
      // it. And if PIN is pref-based it always returns false regardless of the
      // policy because pref-based PIN doesn't have capability to decrypt the
      // user's cryptohome. So just pass an arbitrary purpose here.
      account_id, quick_unlock::Purpose::kAny,
      base::BindOnce(
          [](const AccountId& account_id, bool can_authenticate) {
            if (!LoginScreen::Get() || !LoginScreen::Get()->GetModel()) {
              return;
            }
            LoginScreen::Get()->GetModel()->SetPinEnabledForUser(
                account_id, can_authenticate);
          },
          account_id));
}

void UpdateChallengeResponseAuthAvailability(const AccountId& account_id) {
  const bool enable_challenge_response =
      ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id);
  LoginScreen::Get()->GetModel()->SetChallengeResponseAuthEnabledForUser(
      account_id, enable_challenge_response);
}

LoginDisplayHostMojo* g_login_display_host_mojo = nullptr;

}  // namespace

LoginDisplayHostMojo::AuthState::AuthState(
    AccountId account_id,
    base::OnceCallback<void(bool)> callback)
    : account_id(account_id), callback(std::move(callback)) {}

LoginDisplayHostMojo::AuthState::~AuthState() = default;

LoginDisplayHostMojo::LoginDisplayHostMojo(DisplayedScreen displayed_screen)
    : user_board_view_mojo_(std::make_unique<UserBoardViewMojo>()),
      user_selection_screen_(
          std::make_unique<ChromeUserSelectionScreen>(displayed_screen)),
      system_info_updater_(std::make_unique<MojoSystemInfoDispatcher>()) {
  CHECK(!g_login_display_host_mojo);
  g_login_display_host_mojo = this;
  user_selection_screen_->SetView(user_board_view_mojo_.get());

  allow_new_user_subscription_ = CrosSettings::Get()->AddSettingsObserver(
      kAccountsPrefAllowNewUser,
      base::BindRepeating(&LoginDisplayHostMojo::OnDeviceSettingsChanged,
                          base::Unretained(this)));

  // Do not load WebUI before it is needed if policy and feature permit.
  // Force load WebUI if feature is not enabled.
  if (!IsLazyWebUILoadingEnabled() &&
      displayed_screen == DisplayedScreen::SIGN_IN_SCREEN) {
    EnsureOobeDialogLoaded();
  }
}

LoginDisplayHostMojo::~LoginDisplayHostMojo() {
  g_login_display_host_mojo = nullptr;
  scoped_activity_observation_.Reset();
  LoginScreenClientImpl::Get()->SetDelegate(nullptr);
  if (!dialog_) {
    return;
  }

  GetLoginScreenCertProviderService()
      ->pin_dialog_manager()
      ->RemovePinDialogHost(&security_token_pin_dialog_host_login_impl_);
  StopObservingOobeUI();
  dialog_->Close();
}

// static
LoginDisplayHostMojo* LoginDisplayHostMojo::Get() {
  return g_login_display_host_mojo;
}

void LoginDisplayHostMojo::OnDialogDestroyed(
    const OobeUIDialogDelegate* dialog) {
  LOG(WARNING) << "OnDialogDestroyed";
  if (dialog == dialog_) {
    StopObservingOobeUI();
    dialog_ = nullptr;
    wizard_controller_.reset();
  }
}

void LoginDisplayHostMojo::SetUsers(const user_manager::UserList& users) {
  const bool was_zero_users = !has_user_pods_;
  has_user_pods_ = users.size() > 0;

  // Hide Gaia dialog in case empty list of users switched to a non-empty one.
  // And if the dialog shows login screen.
  if (was_zero_users && has_user_pods_ && dialog_ && dialog_->IsVisible() &&
      (!wizard_controller_->is_initialized() ||
       (wizard_controller_->current_screen() &&
        WizardController::IsSigninScreen(
            wizard_controller_->current_screen()->screen_id())))) {
    HideOobeDialog();
  }

  UpdateAddUserButtonStatus();
  auto* client = LoginScreenClientImpl::Get();

  // SetUsers could be called multiple times. Init the Views-login UI only on
  // the first call.
  if (!initialized_) {
    client->SetDelegate(this);
    LoginScreen::Get()->ShowLoginScreen();
  }
  user_selection_screen_->Init(users);
  LoginScreen::Get()->GetModel()->SetUserList(
      user_selection_screen_->UpdateAndReturnUserListForAsh());
  user_selection_screen_->SetUsersLoaded(true /*loaded*/);

  if (user_manager::UserManager::IsInitialized()) {
    // Enable pin and challenge-response authentication for any users who can
    // use them.
    for (const user_manager::User* user : users) {
      if (!user->IsDeviceLocalAccount()) {
        UpdatePinAuthAvailability(user->GetAccountId());
        UpdateChallengeResponseAuthAvailability(user->GetAccountId());
      }
    }
  }

  if (initialized_) {
    return;
  }
  initialized_ = true;

  // login-prompt-visible is a special signal sent by chrome to notify upstart
  // utility and the rest of the platform that the chrome has successfully
  // started and the system can proceed with initialization of other system
  // services.
  VLOG(1) << "Emitting login-prompt-visible";
  SessionManagerClient::Get()->EmitLoginPromptVisible();

  // TODO(crbug.com/1305245) - Remove once the issue is fixed.
  LOG(WARNING) << __func__ << " NotifyLoginOrLockScreenVisible";
  session_manager::SessionManager::Get()->NotifyLoginOrLockScreenVisible();

  // If there no available users exist, delay showing the dialogs until after
  // GAIA dialog is shown (GAIA dialog will check these local state values,
  // too). Login UI will show GAIA dialog if no user are registered, which
  // might hide any UI shown here.
  if (users.empty()) {
    return;
  }

  // TODO(crbug.com/1105387): Part of initial screen logic.
  // Check whether factory reset or debugging feature have been requested in
  // prior session, and start reset or enable debugging wizard as needed.
  // This has to happen after login-prompt-visible, as some reset dialog
  // features (TPM firmware update) depend on system services running, which
  // is in turn blocked on the 'login-prompt-visible' signal.
  PrefService* local_state = g_browser_process->local_state();
  if (local_state->GetBoolean(::prefs::kFactoryResetRequested)) {
    StartWizard(ResetView::kScreenId);
  } else if (local_state->GetBoolean(::prefs::kDebuggingFeaturesRequested)) {
    StartWizard(EnableDebuggingScreenView::kScreenId);
  } else if (local_state->GetBoolean(::prefs::kEnableAdbSideloadingRequested)) {
    StartWizard(EnableAdbSideloadingScreenView::kScreenId);
  } else if (!KioskAppManager::Get()->GetAutoLaunchApp().empty() &&
             KioskAppManager::Get()->IsAutoLaunchRequested()) {
    VLOG(0) << "Showing auto-launch warning";
    StartWizard(KioskAutolaunchScreenView::kScreenId);
  }
}

void LoginDisplayHostMojo::ShowPasswordChangedDialogLegacy(
    const AccountId& account_id,
    bool show_password_error) {
  EnsureOobeDialogLoaded();
  DCHECK(GetOobeUI());
  wizard_controller_->ShowGaiaPasswordChangedScreenLegacy(account_id,
                                                          show_password_error);
  ShowDialog();
}

void LoginDisplayHostMojo::StartCryptohomeRecovery(
    std::unique_ptr<UserContext> user_context) {
  DCHECK(GetOobeUI());
  wizard_controller_->ShowCryptohomeRecoveryScreen(std::move(user_context));
  ShowDialog();
}

void LoginDisplayHostMojo::StartBrowserDataMigration() {
  DCHECK(GetOobeUI());
  wizard_controller_->AdvanceToScreen(LacrosDataMigrationScreenView::kScreenId);
}

void LoginDisplayHostMojo::HandleDisplayCaptivePortal() {
  EnsureOobeDialogLoaded();
  if (dialog_->IsVisible()) {
    GetOobeUI()->GetErrorScreen()->FixCaptivePortal();
  } else {
    dialog_->SetShouldDisplayCaptivePortal(true);
  }
}

ExistingUserController* LoginDisplayHostMojo::GetExistingUserController() {
  if (!existing_user_controller_) {
    CreateExistingUserController();
  }
  return existing_user_controller_.get();
}

gfx::NativeWindow LoginDisplayHostMojo::GetNativeWindow() const {
  // We can't access the login widget because it's in ash, return the native
  // window of the dialog widget if it exists.
  if (!dialog_) {
    return nullptr;
  }
  return dialog_->GetNativeWindow();
}

views::Widget* LoginDisplayHostMojo::GetLoginWindowWidget() const {
  return dialog_ ? dialog_->GetWebDialogWidget() : nullptr;
}

OobeUI* LoginDisplayHostMojo::GetOobeUI() const {
  if (!dialog_) {
    return nullptr;
  }
  return dialog_->GetOobeUI();
}

content::WebContents* LoginDisplayHostMojo::GetOobeWebContents() const {
  if (!dialog_) {
    return nullptr;
  }
  return dialog_->GetWebContents();
}

WebUILoginView* LoginDisplayHostMojo::GetWebUILoginView() const {
  return nullptr;
}

void LoginDisplayHostMojo::OnFinalize() {
  if (dialog_) {
    dialog_->Close();
  }

  ShutdownDisplayHost();
}

void LoginDisplayHostMojo::SetStatusAreaVisible(bool visible) {
  SystemTrayClientImpl::Get()->SetPrimaryTrayVisible(visible);
}

void LoginDisplayHostMojo::StartWizard(OobeScreenId first_screen) {
  EnsureOobeDialogLoaded();
  OobeUI* oobe_ui = GetOobeUI();
  DCHECK(oobe_ui);
  // Dialog is not shown immediately, and will be shown only when a screen
  // change occurs. This prevents the dialog from showing when there are no
  // screens to show.
  ObserveOobeUI();

  if (wizard_controller_->is_initialized()) {
    wizard_controller_->AdvanceToScreen(first_screen);
  } else {
    wizard_controller_->Init(first_screen);
    NotifyWizardCreated();
  }
}

WizardController* LoginDisplayHostMojo::GetWizardController() {
  EnsureOobeDialogLoaded();
  return wizard_controller_.get();
}

void LoginDisplayHostMojo::OnStartUserAdding() {
  VLOG(1) << "Login Mojo >> user adding";

  // Lock container can be transparent after lock screen animation.
  aura::Window* lock_container =
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          kShellWindowId_LockScreenContainersContainer);
  lock_container->layer()->SetOpacity(1.0);

  CreateExistingUserController();

  SetStatusAreaVisible(true);
  existing_user_controller_->Init(
      user_manager::UserManager::Get()->GetUsersAllowedForMultiProfile());
}

void LoginDisplayHostMojo::CancelUserAdding() {
  Finalize(base::OnceClosure());
}

void LoginDisplayHostMojo::OnStartSignInScreen() {
  // This function may be called early in startup flow, before
  // LoginScreenClientImpl has been initialized. Wait until
  // LoginScreenClientImpl is initialized as it is a common dependency.
  if (!LoginScreenClientImpl::HasInstance()) {
    // TODO(jdufault): Add a timeout here / make sure we do not post infinitely.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&LoginDisplayHostMojo::OnStartSignInScreen,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  if (signin_screen_started_) {
    // If we already have a signin screen instance, just reset the state of the
    // oobe dialog.

    // Try to switch to user creation screen.
    StartWizard(UserCreationView::kScreenId);

    if (wizard_controller_->current_screen() &&
        !WizardController::IsSigninScreen(
            wizard_controller_->current_screen()->screen_id())) {
      // Switching might fail due to the screen priorities. Do no hide the
      // dialog in that case.
      return;
    }

    // Maybe hide dialog if there are existing users. It also reloads Gaia.
    HideOobeDialog();

    return;
  }

  signin_screen_started_ = true;

  CreateExistingUserController();

  // Load the UI.
  existing_user_controller_->Init(user_manager::UserManager::Get()->GetUsers());

  user_selection_screen_->InitEasyUnlock();

  system_info_updater_->StartRequest();

  OnStartSignInScreenCommon();

  login::SecurityTokenSessionController::MaybeDisplayLoginScreenNotification();
}

void LoginDisplayHostMojo::OnStartAppLaunch() {
  EnsureOobeDialogLoaded();

  ShowFullScreen();
}

void LoginDisplayHostMojo::OnBrowserCreated() {
  base::TimeTicks startup_time =
      startup_metric_utils::GetCommon().MainEntryPointTicks();
  if (startup_time.is_null()) {
    return;
  }
  base::TimeDelta delta = base::TimeTicks::Now() - startup_time;
  UMA_HISTOGRAM_CUSTOM_TIMES("OOBE.BootToSignInCompleted", delta,
                             base::Milliseconds(10), base::Minutes(30), 100);
}

void LoginDisplayHostMojo::ShowGaiaDialog(const AccountId& prefilled_account) {
  EnsureOobeDialogLoaded();
  DCHECK(GetOobeUI());

  if (prefilled_account.is_valid()) {
    gaia_reauth_account_id_ = prefilled_account;
  } else {
    gaia_reauth_account_id_.reset();
  }
  ShowGaiaDialogCommon(prefilled_account);

  ShowDialog();
  // Refresh wallpaper once OobeDialogState is propagated after showing the
  // dialog.
  UpdateWallpaper(prefilled_account);
}

void LoginDisplayHostMojo::ShowOsInstallScreen() {
  StartWizard(OsInstallScreenView::kScreenId);
  ShowDialog();
}

void LoginDisplayHostMojo::ShowGuestTosScreen() {
  StartWizard(GuestTosScreenView::kScreenId);
  ShowDialog();
}

void LoginDisplayHostMojo::HideOobeDialog(bool saml_page_closed) {
  DCHECK(dialog_);

  // The dialog can be hidden if there are no users on the login screen only
  // on camera timeout during SAML login.
  // TODO(crbug.com/1283052): simplify the logic here.

  const bool no_users = GetExistingUserController() &&
                        !GetExistingUserController()->IsSigninInProgress() &&
                        !has_user_pods_;

  const bool kiosk_license_mode =
      Shell::Get()
          ->system_tray_model()
          ->enterprise_domain()
          ->management_device_mode() == ManagementDeviceMode::kKioskSku;
  if (no_users && !saml_page_closed && !kiosk_license_mode) {
    return;
  }

  user_selection_screen_->OnBeforeShow();
  if (features::IsInputDeviceSettingsSplitEnabled()) {
    InputDeviceSettingsController::Get()->OnLoginScreenFocusedPodChanged(
        focused_pod_account_id_);
  }
  HideDialog();
  // Update wallpaper once a new OobeDialogState is propagated.
  UpdateWallpaper(focused_pod_account_id_);

  // If the OOBE dialog was hidden due to closing of the SAML page (camera
  // timeout or ESC button) and there are no user pods and the user isn't using
  // ChromeVox - let the user go back to login flow with any action. Otherwise
  // the user can go back to login by pressing the arrow button.
  if (saml_page_closed && !has_user_pods_ && !kiosk_license_mode &&
      !scoped_activity_observation_.IsObserving() &&
      !AccessibilityManager::Get()->IsSpokenFeedbackEnabled()) {
    scoped_activity_observation_.Observe(ui::UserActivityDetector::Get());
  }
}

void LoginDisplayHostMojo::SetShelfButtonsEnabled(bool enabled) {
  // Do nothing as we do not need to disable the shelf buttons on lock/login
  // screen.
}

void LoginDisplayHostMojo::UpdateOobeDialogState(OobeDialogState state) {
  if (dialog_) {
    dialog_->SetState(state);
  }
}

void LoginDisplayHostMojo::UpdateAddUserButtonStatus() {
  LoginScreen::Get()->EnableAddUserButton(!AllAllowlistedUsersPresent());
}

void LoginDisplayHostMojo::RequestSystemInfoUpdate() {
  system_info_updater_->StartRequest();
}

bool LoginDisplayHostMojo::HasUserPods() {
  return has_user_pods_;
}

void LoginDisplayHostMojo::VerifyOwnerForKiosk(base::OnceClosure on_success) {
  // This UI is specific fo the consumer kiosk. We hide all the pods except for
  // the owner. User can't go back to the normal user screen from this. App
  // launch cancellation results in the Chrome restart (see
  // KioskLaunchController::OnCancelAppLaunch).
  CHECK(GetKioskLaunchController());
  DCHECK(!owner_verified_callback_);
  owner_verified_callback_ = std::move(on_success);
  owner_account_id_ = user_manager::UserManager::Get()->GetOwnerAccountId();
  CHECK(owner_account_id_.is_valid());
  ShowOwnerPod(owner_account_id_);
  HideOobeDialog();
}

void LoginDisplayHostMojo::AddObserver(LoginDisplayHost::Observer* observer) {
  observers_.AddObserver(observer);
}

void LoginDisplayHostMojo::RemoveObserver(
    LoginDisplayHost::Observer* observer) {
  observers_.RemoveObserver(observer);
}

SigninUI* LoginDisplayHostMojo::GetSigninUI() {
  return this;
}

bool LoginDisplayHostMojo::IsWizardControllerCreated() const {
  return wizard_controller_.get();
}

void LoginDisplayHostMojo::OnCancelPasswordChangedFlow() {
  HideOobeDialog();
}

void LoginDisplayHostMojo::ShowEnableConsumerKioskScreen() {
  NOTREACHED();
}

bool LoginDisplayHostMojo::GetKeyboardRemappedPrefValue(
    const std::string& pref_name,
    int* value) const {
  if (!focused_pod_account_id_.is_valid()) {
    return false;
  }
  user_manager::KnownUser known_user(g_browser_process->local_state());
  absl::optional<int> opt_val =
      known_user.FindIntPath(focused_pod_account_id_, pref_name);
  if (value && opt_val.has_value()) {
    *value = opt_val.value();
  }
  return opt_val.has_value();
}

bool LoginDisplayHostMojo::IsWebUIStarted() const {
  return dialog_;
}

bool LoginDisplayHostMojo::HandleAccelerator(LoginAcceleratorAction action) {
  // This accelerator is handled by the lock contents view.
  if (action == LoginAcceleratorAction::kToggleSystemInfo) {
    return false;
  }

  return LoginDisplayHostCommon::HandleAccelerator(action);
}

void LoginDisplayHostMojo::HandleAuthenticateUserWithPasswordOrPin(
    const AccountId& account_id,
    const std::string& password,
    bool authenticated_by_pin,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_EQ(account_id.GetUserEmail(),
            gaia::SanitizeEmail(account_id.GetUserEmail()));

  CHECK(!pending_auth_state_);
  pending_auth_state_ =
      std::make_unique<AuthState>(account_id, std::move(callback));

  const user_manager::User* const user =
      user_manager::UserManager::Get()->FindUser(account_id);
  DCHECK(user);
  UserContext user_context(*user);
  user_context.SetIsUsingPin(authenticated_by_pin);
  user_context.SetKey(Key(Key::KEY_TYPE_PASSWORD_PLAIN, "" /*salt*/, password));
  if (!authenticated_by_pin) {
    user_context.SetLocalPasswordInput(LocalPasswordInput{password});
  }
  user_context.SetPasswordKey(Key(password));
  user_context.SetLoginInputMethodIdUsed(input_method::InputMethodManager::Get()
                                             ->GetActiveIMEState()
                                             ->GetCurrentInputMethod()
                                             .id());

  if (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY) {
    LOG(FATAL) << "Incorrect Active Directory user type "
               << user_context.GetUserType();
    user_context.SetIsUsingOAuth(false);
  }

  if (owner_verified_callback_) {
    CheckOwnerCredentials(user_context);
    return;
  }

  existing_user_controller_->Login(user_context, SigninSpecifics());
}

void LoginDisplayHostMojo::HandleAuthenticateUserWithEasyUnlock(
    const AccountId& account_id) {
  user_selection_screen_->AttemptEasyUnlock(account_id);
}

void LoginDisplayHostMojo::HandleAuthenticateUserWithChallengeResponse(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> callback) {
  if (!ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id)) {
    LOG(ERROR)
        << "Challenge-response authentication isn't supported for the user";
    std::move(callback).Run(false);
    return;
  }

  challenge_response_auth_keys_loader_.LoadAvailableKeys(
      account_id,
      base::BindOnce(&LoginDisplayHostMojo::OnChallengeResponseKeysPrepared,
                     weak_factory_.GetWeakPtr(), account_id,
                     std::move(callback)));
}

void LoginDisplayHostMojo::HandleOnFocusPod(const AccountId& account_id) {
  user_selection_screen_->HandleFocusPod(account_id);
  WallpaperControllerClientImpl::Get()->ShowUserWallpaper(account_id);
  Shell::Get()->color_palette_controller()->SelectLocalAccount(account_id);
  if (features::IsInputDeviceSettingsSplitEnabled()) {
    InputDeviceSettingsController::Get()->OnLoginScreenFocusedPodChanged(
        account_id);
  }

  if (focused_pod_account_id_ != account_id) {
    MaybeUpdateOfflineLoginLinkVisibility(account_id);
  }
  focused_pod_account_id_ = account_id;
}

bool LoginDisplayHostMojo::HandleFocusLockScreenApps(bool reverse) {
  NOTREACHED();
  return false;
}

void LoginDisplayHostMojo::HandleFocusOobeDialog() {
  if (!dialog_->IsVisible()) {
    return;
  }

  dialog_->GetWebContents()->Focus();
}

void LoginDisplayHostMojo::HandleLaunchPublicSession(
    const AccountId& account_id,
    const std::string& locale,
    const std::string& input_method) {
  UserContext context(user_manager::USER_TYPE_PUBLIC_ACCOUNT, account_id);
  context.SetPublicSessionLocale(locale);
  context.SetPublicSessionInputMethod(input_method);
  existing_user_controller_->Login(context, SigninSpecifics());
}

void LoginDisplayHostMojo::OnAuthFailure(const AuthFailure& error) {
  // OnAuthFailure and OnAuthSuccess can be called if an authentication attempt
  // is not initiated from mojo, ie, if LoginDisplay::Delegate::Login() is
  // called directly.
  if (pending_auth_state_) {
    UpdatePinAuthAvailability(pending_auth_state_->account_id);
    GetLoginScreenCertProviderService()
        ->AbortSignatureRequestsForAuthenticatingUser(
            pending_auth_state_->account_id);
    std::move(pending_auth_state_->callback).Run(false);
    pending_auth_state_.reset();
  }
}

void LoginDisplayHostMojo::OnAuthSuccess(const UserContext& user_context) {
  if (pending_auth_state_) {
    std::move(pending_auth_state_->callback).Run(true);
    pending_auth_state_.reset();
  }

  if (gaia_reauth_account_id_.has_value()) {
    SendReauthReason(gaia_reauth_account_id_.value(),
                     false /* password changed */);
    gaia_reauth_account_id_.reset();
  }
}

void LoginDisplayHostMojo::OnPasswordChangeDetectedLegacy(
    const UserContext& user_context) {
  if (user_context.GetAccountId().is_valid()) {
    SendReauthReason(user_context.GetAccountId(), true /* password changed */);
  }
  gaia_reauth_account_id_.reset();
}

void LoginDisplayHostMojo::OnPasswordChangeDetectedFor(
    const AccountId& account) {
  if (account.is_valid()) {
    SendReauthReason(account, true /* password changed */);
  }
  gaia_reauth_account_id_.reset();
}

void LoginDisplayHostMojo::OnOldEncryptionDetected(
    std::unique_ptr<UserContext> user_context,
    bool has_incomplete_migration) {}

void LoginDisplayHostMojo::OnCurrentScreenChanged(OobeScreenId current_screen,
                                                  OobeScreenId new_screen) {
  DCHECK(dialog_);
  if (!dialog_->IsVisible()) {
    ShowDialog();
  }
}

void LoginDisplayHostMojo::OnDestroyingOobeUI() {
  StopObservingOobeUI();
}

// views::ViewObserver:
void LoginDisplayHostMojo::OnViewBoundsChanged(views::View* observed_view) {
  DCHECK(scoped_observation_.IsObservingSource(observed_view));
  for (auto& observer : observers_) {
    observer.WebDialogViewBoundsChanged(observed_view->GetBoundsInScreen());
  }
}

void LoginDisplayHostMojo::OnViewIsDeleting(views::View* observed_view) {
  DCHECK(scoped_observation_.IsObservingSource(observed_view));
  scoped_observation_.Reset();
}

bool LoginDisplayHostMojo::IsOobeUIDialogVisible() const {
  return dialog_ && dialog_->IsVisible();
}

OobeUIDialogDelegate* LoginDisplayHostMojo::EnsureDialogForTest() {
  EnsureOobeDialogLoaded();
  return dialog_;
}

void LoginDisplayHostMojo::EnsureOobeDialogLoaded() {
  if (dialog_) {
    return;
  }

  dialog_ = new OobeUIDialogDelegate(weak_factory_.GetWeakPtr());

  views::View* web_dialog_view = dialog_->GetWebDialogView();
  scoped_observation_.Observe(web_dialog_view);

  // Should be created after dialog was created and OobeUI was loaded.
  wizard_controller_ = std::make_unique<WizardController>(GetWizardContext());

  GetLoginScreenCertProviderService()->pin_dialog_manager()->AddPinDialogHost(
      &security_token_pin_dialog_host_login_impl_);

  // Update status of add user button in the shelf.
  UpdateAddUserButtonStatus();
}

void LoginDisplayHostMojo::OnChallengeResponseKeysPrepared(
    const AccountId& account_id,
    base::OnceCallback<void(bool)> on_auth_complete_callback,
    std::vector<ChallengeResponseKey> challenge_response_keys) {
  if (challenge_response_keys.empty()) {
    // TODO(crbug.com/826417): Indicate the error in the UI.
    std::move(on_auth_complete_callback).Run(false);
    return;
  }

  CHECK(!pending_auth_state_);
  pending_auth_state_ = std::make_unique<AuthState>(
      account_id, std::move(on_auth_complete_callback));

  const user_manager::User* const user =
      user_manager::UserManager::Get()->FindUser(account_id);
  DCHECK(user);
  UserContext user_context(*user);
  *user_context.GetMutableChallengeResponseKeys() =
      std::move(challenge_response_keys);

  existing_user_controller_->Login(user_context, SigninSpecifics());
}

void LoginDisplayHostMojo::ShowDialog() {
  EnsureOobeDialogLoaded();
  ObserveOobeUI();
  dialog_->Show();
}

void LoginDisplayHostMojo::ShowFullScreen() {
  EnsureOobeDialogLoaded();
  ObserveOobeUI();
  dialog_->ShowFullScreen();
}

void LoginDisplayHostMojo::HideDialog() {
  if (!dialog_) {
    return;
  }

  // Stop observing so that dialog will not be shown when a screen change
  // occurs. Screen changes can occur even when the dialog is not shown (e.g.
  // with hidden error screens).
  StopObservingOobeUI();
  dialog_->Hide();
  // Hide the current screen of the `WizardController` to force `Show()` to be
  // called on the first screen when the dialog reopens.
  GetWizardController()->HideCurrentScreen();
  gaia_reauth_account_id_.reset();
}

void LoginDisplayHostMojo::ObserveOobeUI() {
  if (added_as_oobe_observer_) {
    return;
  }

  OobeUI* oobe_ui = GetOobeUI();
  if (!oobe_ui) {
    return;
  }

  oobe_ui->AddObserver(this);
  added_as_oobe_observer_ = true;
}

void LoginDisplayHostMojo::StopObservingOobeUI() {
  if (!added_as_oobe_observer_) {
    return;
  }

  added_as_oobe_observer_ = false;

  OobeUI* oobe_ui = GetOobeUI();
  if (oobe_ui) {
    oobe_ui->RemoveObserver(this);
  }
}

void LoginDisplayHostMojo::CreateExistingUserController() {
  existing_user_controller_ = std::make_unique<ExistingUserController>();

  // We need auth attempt results to notify views-based login screen.
  existing_user_controller_->AddLoginStatusConsumer(this);
}

void LoginDisplayHostMojo::CheckOwnerCredentials(
    const UserContext& user_context) {
  CHECK_EQ(owner_account_id_, user_context.GetAccountId());
  if (!extended_authenticator_) {
    extended_authenticator_ = ExtendedAuthenticator::Create(this);
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ExtendedAuthenticator::AuthenticateToCheck,
                     extended_authenticator_.get(), user_context,
                     base::BindOnce(&LoginDisplayHostMojo::OnOwnerSigninSuccess,
                                    base::Unretained(this))));
}

void LoginDisplayHostMojo::OnOwnerSigninSuccess() {
  DCHECK(owner_verified_callback_);
  std::move(owner_verified_callback_).Run();
  extended_authenticator_.reset();
  ShowFullScreen();
}

void LoginDisplayHostMojo::MaybeUpdateOfflineLoginLinkVisibility(
    const AccountId& account_id) {
  bool offline_limit_expired = false;

  user_manager::KnownUser known_user(g_browser_process->local_state());
  const absl::optional<base::TimeDelta> offline_signin_interval =
      known_user.GetOfflineSigninLimit(account_id);

  // Check if the limit is set only.
  if (offline_signin_interval) {
    const base::Time last_online_signin =
        known_user.GetLastOnlineSignin(account_id);
    offline_limit_expired =
        login::TimeToOnlineSignIn(last_online_signin,
                                  offline_signin_interval.value()) <=
        base::TimeDelta();
  }

  ErrorScreen::AllowOfflineLoginPerUser(!offline_limit_expired);
}

void LoginDisplayHostMojo::OnUserActivity(const ui::Event* event) {
  // ESC button can be used to hide login dialog when SAML is configured.
  // Prevent reopening it with ESC.
  if (event && (event->IsKeyEvent() &&
                event->AsKeyEvent()->key_code() == ui::VKEY_ESCAPE)) {
    return;
  }
  scoped_activity_observation_.Reset();
  ShowGaiaDialog(EmptyAccountId());
}

void LoginDisplayHostMojo::OnDeviceSettingsChanged() {
  // Update status of add user button in the shelf.
  UpdateAddUserButtonStatus();

  if (!dialog_) {
    return;
  }

  // Reload Gaia.
  GetWizardController()->GetScreen<GaiaScreen>()->LoadOnlineGaia();
}

}  // namespace ash

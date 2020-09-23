// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_display_host_mojo.h"

#include <utility>

#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/certificate_provider/pin_dialog_manager.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/mojo_system_info_dispatcher.h"
#include "chrome/browser/chromeos/login/reauth_stats.h"
#include "chrome/browser/chromeos/login/screens/chrome_user_selection_screen.h"
#include "chrome/browser/chromeos/login/screens/gaia_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "chrome/browser/chromeos/login/ui/login_display_mojo.h"
#include "chrome/browser/chromeos/login/user_board_view_mojo.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_password_changed_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/user_creation_screen_handler.h"
#include "chromeos/login/auth/user_context.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/aura/window.h"

namespace chromeos {

namespace {

constexpr char kLoginDisplay[] = "login";

CertificateProviderService* GetLoginScreenCertProviderService() {
  DCHECK(ProfileHelper::IsSigninProfileInitialized());
  return CertificateProviderServiceFactory::GetForBrowserContext(
      ProfileHelper::GetSigninProfile());
}

}  // namespace

LoginDisplayHostMojo::AuthState::AuthState(
    AccountId account_id,
    base::OnceCallback<void(bool)> callback)
    : account_id(account_id), callback(std::move(callback)) {}

LoginDisplayHostMojo::AuthState::~AuthState() = default;

LoginDisplayHostMojo::LoginDisplayHostMojo(DisplayedScreen displayed_screen)
    : login_display_(std::make_unique<LoginDisplayMojo>(this)),
      user_board_view_mojo_(std::make_unique<UserBoardViewMojo>()),
      user_selection_screen_(
          std::make_unique<ChromeUserSelectionScreen>(kLoginDisplay)),
      system_info_updater_(std::make_unique<MojoSystemInfoDispatcher>()),
      displayed_screen_(displayed_screen) {
  user_selection_screen_->SetView(user_board_view_mojo_.get());

  if (displayed_screen == DisplayedScreen::SIGN_IN_SCREEN) {
    // Preload webui-based OOBE for add user, kiosk apps, etc.
    LoadOobeDialog();

    // Should be created after OobeUI loaded with the dialog.
    wizard_controller_ = std::make_unique<WizardController>();

    GetLoginScreenCertProviderService()->pin_dialog_manager()->AddPinDialogHost(
        &security_token_pin_dialog_host_ash_impl_);
  }
}

LoginDisplayHostMojo::~LoginDisplayHostMojo() {
  if (displayed_screen_ == DisplayedScreen::SIGN_IN_SCREEN) {
    GetLoginScreenCertProviderService()
        ->pin_dialog_manager()
        ->RemovePinDialogHost(&security_token_pin_dialog_host_ash_impl_);
  }
  LoginScreenClient::Get()->SetDelegate(nullptr);
  if (dialog_) {
    dialog_->GetOobeUI()->signin_screen_handler()->SetDelegate(nullptr);
    StopObservingOobeUI();
    dialog_->Close();
  }
}

void LoginDisplayHostMojo::OnDialogDestroyed(
    const OobeUIDialogDelegate* dialog) {
  if (dialog == dialog_) {
    StopObservingOobeUI();
    dialog_ = nullptr;
    wizard_controller_.reset();
  }
}

void LoginDisplayHostMojo::SetUserCount(int user_count) {
  const bool was_zero_users = (user_count_ == 0);
  user_count_ = user_count;
  if (GetOobeUI())
    GetOobeUI()->SetLoginUserCount(user_count_);

  // Hide Gaia dialog in case empty list of users switched to a non-empty one.
  // And if the dialog shows login screen.
  if (was_zero_users && user_count_ != 0 && dialog_ && dialog_->IsVisible() &&
      (!wizard_controller_->is_initialized() ||
       (wizard_controller_->current_screen() &&
        WizardController::IsSigninScreen(
            wizard_controller_->current_screen()->screen_id())))) {
    HideOobeDialog();
  }
}

void LoginDisplayHostMojo::ShowPasswordChangedDialog(
    bool show_password_error,
    const AccountId& account_id) {
  DCHECK(GetOobeUI());
  wizard_controller_->ShowGaiaPasswordChangedScreen(account_id,
                                                    show_password_error);
  ShowDialog();
}

void LoginDisplayHostMojo::ShowAllowlistCheckFailedError() {
  DCHECK(GetOobeUI());
  GetOobeUI()->signin_screen_handler()->ShowAllowlistCheckFailedError();
  ShowDialog();
}

void LoginDisplayHostMojo::ShowSigninUI(const std::string& email) {
  DCHECK(GetOobeUI());
  GetOobeUI()->signin_screen_handler()->ShowSigninUI(email);
  ShowDialog();
}

void LoginDisplayHostMojo::HandleDisplayCaptivePortal() {
  if (dialog_->IsVisible())
    GetOobeUI()->GetErrorScreen()->FixCaptivePortal();
  else
    dialog_->SetShouldDisplayCaptivePortal(true);
}

LoginDisplay* LoginDisplayHostMojo::GetLoginDisplay() {
  return login_display_.get();
}

ExistingUserController* LoginDisplayHostMojo::GetExistingUserController() {
  return existing_user_controller_.get();
}

gfx::NativeWindow LoginDisplayHostMojo::GetNativeWindow() const {
  // We can't access the login widget because it's in ash, return the native
  // window of the dialog widget if it exists.
  if (!dialog_)
    return nullptr;
  return dialog_->GetNativeWindow();
}

OobeUI* LoginDisplayHostMojo::GetOobeUI() const {
  if (!dialog_)
    return nullptr;
  return dialog_->GetOobeUI();
}

content::WebContents* LoginDisplayHostMojo::GetOobeWebContents() const {
  if (!dialog_)
    return nullptr;
  return dialog_->GetWebContents();
}

WebUILoginView* LoginDisplayHostMojo::GetWebUILoginView() const {
  NOTREACHED();
  return nullptr;
}

void LoginDisplayHostMojo::OnFinalize() {
  if (dialog_)
    dialog_->Close();

  ShutdownDisplayHost();
}

void LoginDisplayHostMojo::SetStatusAreaVisible(bool visible) {
  SystemTrayClient::Get()->SetPrimaryTrayVisible(visible);
}

void LoginDisplayHostMojo::StartWizard(OobeScreenId first_screen) {
  OobeUI* oobe_ui = GetOobeUI();
  DCHECK(oobe_ui);
  // Dialog is not shown immediately, and will be shown only when a screen
  // change occurs. This prevents the dialog from showing when there are no
  // screens to show.
  ObserveOobeUI();

  if (wizard_controller_->is_initialized())
    wizard_controller_->AdvanceToScreen(first_screen);
  else
    wizard_controller_->Init(first_screen);
}

WizardController* LoginDisplayHostMojo::GetWizardController() {
  return wizard_controller_.get();
}

void LoginDisplayHostMojo::OnStartUserAdding() {
  VLOG(1) << "Login Mojo >> user adding";

  // Lock container can be transparent after lock screen animation.
  aura::Window* lock_container = ash::Shell::GetContainer(
      ash::Shell::GetPrimaryRootWindow(),
      ash::kShellWindowId_LockScreenContainersContainer);
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
  // This function may be called early in startup flow, before LoginScreenClient
  // has been initialized. Wait until LoginScreenClient is initialized as it is
  // a common dependency.
  if (!LoginScreenClient::HasInstance()) {
    // TODO(jdufault): Add a timeout here / make sure we do not post infinitely.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
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

  // Update status of add user button in the shelf.
  UpdateAddUserButtonStatus();

  OnStartSignInScreenCommon();
}

void LoginDisplayHostMojo::OnPreferencesChanged() {
  NOTIMPLEMENTED();
}

void LoginDisplayHostMojo::OnStartAppLaunch() {
  ShowFullScreen();
}

void LoginDisplayHostMojo::OnBrowserCreated() {
  base::TimeTicks startup_time = startup_metric_utils::MainEntryPointTicks();
  if (startup_time.is_null())
    return;
  base::TimeDelta delta = base::TimeTicks::Now() - startup_time;
  UMA_HISTOGRAM_CUSTOM_TIMES("OOBE.BootToSignInCompleted", delta,
                             base::TimeDelta::FromMilliseconds(10),
                             base::TimeDelta::FromMinutes(30), 100);
}

void LoginDisplayHostMojo::ShowGaiaDialog(const AccountId& prefilled_account) {
  DCHECK(GetOobeUI());

  if (prefilled_account.is_valid()) {
    gaia_reauth_account_id_ = prefilled_account;
  } else {
    gaia_reauth_account_id_.reset();
  }
  ShowGaiaDialogCommon(prefilled_account);

  ShowDialog();
}

void LoginDisplayHostMojo::HideOobeDialog() {
  DCHECK(dialog_);

  // The dialog can not be hidden if there are no users on the login screen.
  // Reload it instead.

  // As ShowDialogCommon will not reload GAIA upon show for performance reasons,
  // reload it to ensure that no state is persisted between hide and
  // subsequent show.
  const bool no_users =
      !login_display_->IsSigninInProgress() && user_count_ == 0;
  if (no_users || GetOobeUI()->current_screen() == GaiaView::kScreenId) {
    GaiaScreen* gaia_screen =
        GaiaScreen::Get(GetWizardController()->screen_manager());
    gaia_screen->LoadOnline(EmptyAccountId());
    if (no_users)
      return;
  }

  user_selection_screen_->OnBeforeShow();
  LoadWallpaper(focused_pod_account_id_);
  HideDialog();
}

void LoginDisplayHostMojo::UpdateOobeDialogState(ash::OobeDialogState state) {
  if (dialog_)
    dialog_->SetState(state);
}

void LoginDisplayHostMojo::UpdateAddUserButtonStatus() {
  DCHECK(GetOobeUI());
  ash::LoginScreen::Get()->EnableAddUserButton(
      !GetOobeUI()->signin_screen_handler()->AllAllowlistedUsersPresent());
}

void LoginDisplayHostMojo::RequestSystemInfoUpdate() {
  system_info_updater_->StartRequest();
}

void LoginDisplayHostMojo::OnCancelPasswordChangedFlow() {
  HideOobeDialog();
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
  user_context.SetKey(
      Key(chromeos::Key::KEY_TYPE_PASSWORD_PLAIN, "" /*salt*/, password));
  user_context.SetPasswordKey(Key(password));

  if (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY) {
    if (user_context.GetUserType() !=
        user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY) {
      LOG(FATAL) << "Incorrect Active Directory user type "
                 << user_context.GetUserType();
    }
    user_context.SetIsUsingOAuth(false);
  }

  existing_user_controller_->Login(user_context, chromeos::SigninSpecifics());
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

void LoginDisplayHostMojo::HandleHardlockPod(const AccountId& account_id) {
  user_selection_screen_->HardLockPod(account_id);
}

void LoginDisplayHostMojo::HandleOnFocusPod(const AccountId& account_id) {
  user_selection_screen_->HandleFocusPod(account_id);
  WallpaperControllerClient::Get()->ShowUserWallpaper(account_id);
  focused_pod_account_id_ = account_id;
}

void LoginDisplayHostMojo::HandleOnNoPodFocused() {
  user_selection_screen_->HandleNoPodFocused();
}

bool LoginDisplayHostMojo::HandleFocusLockScreenApps(bool reverse) {
  NOTREACHED();
  return false;
}

void LoginDisplayHostMojo::HandleFocusOobeDialog() {
  if (!dialog_->IsVisible())
    return;

  dialog_->GetWebContents()->Focus();
}

void LoginDisplayHostMojo::HandleLaunchPublicSession(
    const AccountId& account_id,
    const std::string& locale,
    const std::string& input_method) {
  UserContext context(user_manager::USER_TYPE_PUBLIC_ACCOUNT, account_id);
  context.SetPublicSessionLocale(locale);
  context.SetPublicSessionInputMethod(input_method);
  existing_user_controller_->Login(context, chromeos::SigninSpecifics());
}

void LoginDisplayHostMojo::OnAuthFailure(const AuthFailure& error) {
  // OnAuthFailure and OnAuthSuccess can be called if an authentication attempt
  // is not initiated from mojo, ie, if LoginDisplay::Delegate::Login() is
  // called directly.
  if (pending_auth_state_) {
    login_display_->UpdatePinKeyboardState(pending_auth_state_->account_id);
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

void LoginDisplayHostMojo::OnPasswordChangeDetected(
    const UserContext& user_context) {
  if (user_context.GetAccountId().is_valid()) {
    SendReauthReason(user_context.GetAccountId(), true /* password changed */);
  }
  gaia_reauth_account_id_.reset();
}

void LoginDisplayHostMojo::OnOldEncryptionDetected(
    const UserContext& user_context,
    bool has_incomplete_migration) {}

void LoginDisplayHostMojo::OnCurrentScreenChanged(OobeScreenId current_screen,
                                                  OobeScreenId new_screen) {
  DCHECK(dialog_);
  if (!dialog_->IsVisible())
    ShowDialog();
}

void LoginDisplayHostMojo::OnDestroyingOobeUI() {
  StopObservingOobeUI();
}

bool LoginDisplayHostMojo::IsOobeUIDialogVisible() const {
  return dialog_ && dialog_->IsVisible();
}

void LoginDisplayHostMojo::LoadOobeDialog() {
  if (dialog_)
    return;

  dialog_ = new OobeUIDialogDelegate(weak_factory_.GetWeakPtr());
  dialog_->GetOobeUI()->signin_screen_handler()->SetDelegate(
      login_display_.get());
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

  existing_user_controller_->Login(user_context, chromeos::SigninSpecifics());
}

void LoginDisplayHostMojo::ShowDialog() {
  ObserveOobeUI();
  dialog_->Show();
}

void LoginDisplayHostMojo::ShowFullScreen() {
  ObserveOobeUI();
  dialog_->ShowFullScreen();
}

void LoginDisplayHostMojo::HideDialog() {
  // Stop observing so that dialog will not be shown when a screen change
  // occurs. Screen changes can occur even when the dialog is not shown (e.g.
  // with hidden error screens).
  StopObservingOobeUI();
  dialog_->Hide();
  gaia_reauth_account_id_.reset();
}

void LoginDisplayHostMojo::ObserveOobeUI() {
  if (added_as_oobe_observer_)
    return;

  OobeUI* oobe_ui = GetOobeUI();
  if (!oobe_ui)
    return;

  oobe_ui->AddObserver(this);
  added_as_oobe_observer_ = true;
}

void LoginDisplayHostMojo::StopObservingOobeUI() {
  if (!added_as_oobe_observer_)
    return;

  added_as_oobe_observer_ = false;

  OobeUI* oobe_ui = GetOobeUI();
  if (oobe_ui)
    oobe_ui->RemoveObserver(this);
}

void LoginDisplayHostMojo::CreateExistingUserController() {
  existing_user_controller_ = std::make_unique<ExistingUserController>();
  login_display_->set_delegate(existing_user_controller_.get());

  // We need auth attempt results to notify views-based login screen.
  existing_user_controller_->AddLoginStatusConsumer(this);
}

}  // namespace chromeos

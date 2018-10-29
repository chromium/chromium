// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/lock/webui_screen_locker.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/chromeos/login/ui/login_display_webui.h"
#include "chrome/browser/chromeos/login/ui/preloaded_web_view.h"
#include "chrome/browser/chromeos/login/ui/preloaded_web_view_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/login/base_screen_handler_utils.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/webview/webview.h"

namespace {

// URL which corresponds to the login WebUI.
const char kLoginURL[] = "chrome://oobe/lock";

}  // namespace

namespace chromeos {

// static
void WebUIScreenLocker::RequestPreload() {
  if (!ShouldPreloadLockScreen())
    return;

  VLOG(1) << "Preloading lock screen";
  PreloadedWebView* preloaded_web_view =
      PreloadedWebViewFactory::GetForProfile(ProfileHelper::GetSigninProfile());
  preloaded_web_view->PreloadOnIdle(
      base::BindOnce(&WebUIScreenLocker::DoPreload));
}

// static
bool WebUIScreenLocker::ShouldPreloadLockScreen() {
  // Only preload webui lock screen when it is used.
  if (ash::switches::IsUsingViewsLock())
    return false;

  // Bail for mash because IdleDetector/UserActivityDetector does not work
  // properly there.
  // TODO(xiyuan): Revisit after http://crbug.com/626899.
  if (features::IsMultiProcessMash())
    return false;

  Profile* profile = ProfileHelper::Get()->GetProfileByUser(
      user_manager::UserManager::Get()->GetActiveUser());

  // We only want to preload the lock screen if the user is likely to see the
  // lock screen (since caching the lock screen uses memory). Without
  // preloading, showing the lock screen can take so long we will timeout and
  // crash the browser process (which currently takes down all of Chrome). See
  // crbug.com/452599 for more context.
  //
  // prefs::kEnableAutoScreenLock controls if the lock screen is shown on
  // suspend, so that is our primary hueristic.

  // Note that |profile| can be null in tests.
  return base::FeatureList::IsEnabled(features::kPreloadLockScreen) &&
         profile &&
         profile->GetPrefs()->GetBoolean(ash::prefs::kEnableAutoScreenLock);
}

// static
std::unique_ptr<views::WebView> WebUIScreenLocker::DoPreload(Profile* profile) {
  auto web_view = std::make_unique<views::WebView>(profile);
  web_view->set_owned_by_client();
  web_view->LoadInitialURL(GURL(kLoginURL));
  InitializeWebView(web_view.get(), l10n_util::GetStringUTF16(
                                        IDS_LOCK_SCREEN_TASK_MANAGER_NAME));
  return web_view;
}

////////////////////////////////////////////////////////////////////////////////
// WebUIScreenLocker implementation.

WebUIScreenLocker::WebUIScreenLocker(ScreenLocker* screen_locker)
    : WebUILoginView(BuildConfigSettings()),
      screen_locker_(screen_locker),
      network_state_helper_(new login::NetworkStateHelper),
      weak_factory_(this) {
  set_should_emit_login_prompt_visible(false);
  display::Screen::GetScreen()->AddObserver(this);
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
}

WebUIScreenLocker::~WebUIScreenLocker() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
  // In case of shutdown, lock_window_ may be deleted before WebUIScreenLocker.
  if (lock_window_) {
    lock_window_->RemoveObserver(this);
    lock_window_->Close();
  }
  // If LockScreen() was called, we need to clear the signin screen handler
  // delegate set in ShowSigninScreen so that it no longer points to us.
  if (login_display_.get() && GetOobeUI())
    GetOobeUI()->ResetSigninScreenHandlerDelegate();

  ClearLockScreenAppFocusCyclerDelegate();

  RequestPreload();
}

void WebUIScreenLocker::LockScreen() {
  gfx::Rect bounds = display::Screen::GetScreen()->GetPrimaryDisplay().bounds();

  lock_time_ = base::TimeTicks::Now();
  lock_window_ = new ash::LockWindow();
  lock_window_->AddObserver(this);

  Init();
  content::WebContentsObserver::Observe(web_view()->GetWebContents());

  lock_window_->SetContentsView(this);
  lock_window_->SetBounds(bounds);
  lock_window_->Show();
  LoadURL(GURL(kLoginURL));
  OnLockWindowReady();

  signin_screen_controller_.reset(new SignInScreenController(GetOobeUI()));

  login_display_.reset(new LoginDisplayWebUI());
  login_display_->set_delegate(this);
  login_display_->set_parent_window(GetNativeWindow());
  login_display_->Init(screen_locker_->users(), false, true, false);

  GetOobeUI()->ShowSigninScreen(LoginScreenContext(), login_display_.get(),
                                login_display_.get());

  SetLockScreenAppFocusCyclerDelegate();
}

void WebUIScreenLocker::SetPasswordInputEnabled(bool enabled) {
  login_display_->SetUIEnabled(enabled);
}

void WebUIScreenLocker::ShowErrorMessage(
    int error_msg_id,
    HelpAppLauncher::HelpTopic help_topic_id) {
  login_display_->ShowError(error_msg_id, 0 /* login_attempts */,
                            help_topic_id);
}

void WebUIScreenLocker::AnimateAuthenticationSuccess() {
  GetWebUI()->CallJavascriptFunctionUnsafe(
      "cr.ui.Oobe.animateAuthenticationSuccess");
}

void WebUIScreenLocker::ClearErrors() {
  GetWebUI()->CallJavascriptFunctionUnsafe("cr.ui.Oobe.clearErrors");
}

void WebUIScreenLocker::ScreenLockReady() {
  UMA_HISTOGRAM_TIMES("LockScreen.LockReady",
                      base::TimeTicks::Now() - lock_time_);
  screen_locker_->ScreenLockReady();
  SetPasswordInputEnabled(true);
}

void WebUIScreenLocker::OnLockWindowReady() {
  VLOG(1) << "Lock window ready; WebUI is " << (webui_ready_ ? "too" : "not");
  lock_ready_ = true;
  if (webui_ready_)
    ScreenLockReady();
}

gfx::NativeWindow WebUIScreenLocker::GetNativeWindow() const {
  return lock_window_->GetNativeWindow();
}

void WebUIScreenLocker::FocusUserPod() {
  if (!webui_ready_)
    return;
  web_view()->RequestFocus();
  GetWebUI()->CallJavascriptFunctionUnsafe(
      "cr.ui.Oobe.forceLockedUserPodFocus");
}

void WebUIScreenLocker::ResetAndFocusUserPod() {
  if (!webui_ready_)
    return;
  GetWebUI()->CallJavascriptFunctionUnsafe("cr.ui.Oobe.clearUserPodPassword");
  FocusUserPod();
}

WebUILoginView::WebViewSettings WebUIScreenLocker::BuildConfigSettings() {
  chromeos::WebUILoginView::WebViewSettings settings;
  if (chromeos::WebUIScreenLocker::ShouldPreloadLockScreen()) {
    settings.check_for_preload = true;
    settings.web_view_title =
        l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_TASK_MANAGER_NAME);
  }
  return settings;
}

void WebUIScreenLocker::OnLockWebUIReady() {
  VLOG(1) << "WebUI ready; lock window is " << (lock_ready_ ? "too" : "not");
  webui_ready_ = true;
  if (lock_ready_)
    ScreenLockReady();
}

void WebUIScreenLocker::OnLockBackgroundDisplayed() {
  UMA_HISTOGRAM_TIMES("LockScreen.BackgroundReady",
                      base::TimeTicks::Now() - lock_time_);
}

void WebUIScreenLocker::OnHeaderBarVisible() {
  SessionControllerClient::Get()->NotifyChromeLockAnimationsComplete();
}

void WebUIScreenLocker::OnAshLockAnimationFinished() {
  // Release capture if any.
  aura::client::GetCaptureClient(GetNativeWindow()->GetRootWindow())
      ->SetCapture(nullptr);
  GetWebUI()->CallJavascriptFunctionUnsafe(
      "cr.ui.Oobe.animateOnceFullyDisplayed");
}

void WebUIScreenLocker::SetFingerprintState(
    const AccountId& account_id,
    ash::mojom::FingerprintState state) {
  NOTREACHED();
}

void WebUIScreenLocker::NotifyFingerprintAuthResult(const AccountId& account_id,
                                                    bool success) {
  NOTREACHED();
}

content::WebContents* WebUIScreenLocker::GetWebContents() {
  return WebUILoginView::GetWebContents();
}

////////////////////////////////////////////////////////////////////////////////
// WebUIScreenLocker, LoginDisplay::Delegate:
base::string16 WebUIScreenLocker::GetConnectedNetworkName() {
  return network_state_helper_->GetCurrentNetworkName();
}

bool WebUIScreenLocker::IsSigninInProgress() const {
  // The way how screen locker is implemented right now there's no
  // GAIA sign in in progress in any case.
  return false;
}

void WebUIScreenLocker::Login(const UserContext& user_context,
                              const SigninSpecifics& specifics) {
  chromeos::ScreenLocker::default_screen_locker()->Authenticate(
      user_context, ScreenLocker::AuthenticateCallback());
}

void WebUIScreenLocker::OnSigninScreenReady() {
  VLOG(2) << "Lock screen signin screen is ready";
}

void WebUIScreenLocker::OnStartEnterpriseEnrollment() {
  NOTREACHED();
}

void WebUIScreenLocker::OnStartEnableDebuggingScreen() {
  NOTREACHED();
}

void WebUIScreenLocker::OnStartKioskEnableScreen() {
  NOTREACHED();
}

void WebUIScreenLocker::OnStartKioskAutolaunchScreen() {
  NOTREACHED();
}

void WebUIScreenLocker::ShowWrongHWIDScreen() {
  NOTREACHED();
}

void WebUIScreenLocker::ShowUpdateRequiredScreen() {
  NOTREACHED();
}

void WebUIScreenLocker::ResetAutoLoginTimer() {}

void WebUIScreenLocker::Signout() {
  chromeos::ScreenLocker::default_screen_locker()->Signout();
}

////////////////////////////////////////////////////////////////////////////////
// WidgetObserver:

void WebUIScreenLocker::OnWidgetDestroying(views::Widget* widget) {
  lock_window_->RemoveObserver(this);
  lock_window_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// PowerManagerClient::Observer:

void WebUIScreenLocker::LidEventReceived(PowerManagerClient::LidState state,
                                         const base::TimeTicks& time) {
  if (state == PowerManagerClient::LidState::OPEN) {
    base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                             base::BindOnce(&WebUIScreenLocker::FocusUserPod,
                                            weak_factory_.GetWeakPtr()));
  }
}

void WebUIScreenLocker::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&WebUIScreenLocker::ResetAndFocusUserPod,
                     weak_factory_.GetWeakPtr()));
}

void WebUIScreenLocker::SuspendDone(const base::TimeDelta& sleep_duration) {
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindOnce(&WebUIScreenLocker::FocusUserPod,
                                          weak_factory_.GetWeakPtr()));
}

void WebUIScreenLocker::RenderProcessGone(base::TerminationStatus status) {
  if (browser_shutdown::GetShutdownType() == browser_shutdown::NOT_VALID &&
      status != base::TERMINATION_STATUS_NORMAL_TERMINATION) {
    LOG(ERROR) << "Renderer crash on lock screen; signing out";
    Signout();
  }
}

////////////////////////////////////////////////////////////////////////////////
// display::DisplayObserver:

void WebUIScreenLocker::OnDisplayMetricsChanged(const display::Display& display,
                                                uint32_t changed_metrics) {
  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  if (display.id() != primary_display.id() ||
      !(changed_metrics & DISPLAY_METRIC_BOUNDS)) {
    return;
  }

  if (GetOobeUI()) {
    const gfx::Size& size = primary_display.size();
    GetOobeUI()->GetCoreOobeView()->SetClientAreaSize(size.width(),
                                                      size.height());
  }
}

}  // namespace chromeos

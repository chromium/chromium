// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_display_host_webui.h"

#include <utility>
#include <vector>

#include "ash/accessibility/focus_ring_controller.h"
#include "ash/public/cpp/locale_update_controller.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/boot_times_recorder.h"
#include "chrome/browser/chromeos/first_run/drive_first_run_controller.h"
#include "chrome/browser/chromeos/first_run/first_run.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/input_events_blocker.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_mojo.h"
#include "chrome/browser/chromeos/login/ui/login_display_webui.h"
#include "chrome/browser/chromeos/login/ui/webui_accelerator_mapping.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/net/delay_network_call.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/device_disabling_manager.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/chromeos/system/timezone_resolver_manager.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/ash/system_tray_client.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/device_disabled_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "chromeos/constants/chromeos_constants.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "chromeos/settings/timezone_settings.h"
#include "chromeos/timezone/timezone_resolver.h"
#include "components/account_id/account_id.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"
#include "ui/aura/window.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace chromeos {
namespace {

// Maximum delay for startup sound after 'loginPromptVisible' signal.
const int kStartupSoundMaxDelayMs = 4000;

// URL which corresponds to the login WebUI.
const char kLoginURL[] = "chrome://oobe/login";

// URL which corresponds to the OOBE WebUI.
const char kOobeURL[] = "chrome://oobe/oobe";

// URL which corresponds to the user adding WebUI.
const char kUserAddingURL[] = "chrome://oobe/user-adding";

// URL which corresponds to the app launch splash WebUI.
const char kAppLaunchSplashURL[] = "chrome://oobe/app-launch-splash";

// Duration of sign-in transition animation.
const int kLoginFadeoutTransitionDurationMs = 700;

// Number of times we try to reload OOBE/login WebUI if it crashes.
const int kCrashCountLimit = 5;

// The default fade out animation time in ms.
const int kDefaultFadeTimeMs = 200;

// A class to observe an implicit animation and invokes the callback after the
// animation is completed.
class AnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  explicit AnimationObserver(const base::Closure& callback)
      : callback_(callback) {}
  ~AnimationObserver() override {}

 private:
  // ui::ImplicitAnimationObserver implementation:
  void OnImplicitAnimationsCompleted() override {
    callback_.Run();
    delete this;
  }

  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(AnimationObserver);
};

// Even if oobe is complete we may still want to show it, for example, if there
// are no users registered then the user may want to enterprise enroll.
bool IsOobeComplete() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();

  // Oobe is completed and we have a user or we are enterprise enrolled.
  return chromeos::StartupUtils::IsOobeCompleted() &&
         (!user_manager::UserManager::Get()->GetUsers().empty() ||
          connector->IsEnterpriseManaged());
}

// Returns true if signin (not oobe) should be displayed.
bool ShouldShowSigninScreen(chromeos::OobeScreenId first_screen) {
  return (first_screen == chromeos::OobeScreen::SCREEN_UNKNOWN &&
          IsOobeComplete());
}

void MaybeShowDeviceDisabledScreen() {
  DCHECK(chromeos::LoginDisplayHost::default_host());
  if (!g_browser_process->platform_part()->device_disabling_manager()) {
    // Device disabled check will be done in the DeviceDisablingManager.
    return;
  }

  if (!system::DeviceDisablingManager::IsDeviceDisabledDuringNormalOperation())
    return;

  chromeos::LoginDisplayHost::default_host()->StartWizard(
      DeviceDisabledScreenView::kScreenId);
}

void MaybeShutdownLoginDisplayHostWebUI() {
  if (!LoginDisplayHost::default_host())
    return;
  if (!LoginDisplayHost::default_host()->GetOobeUI())
    return;
  if (LoginDisplayHost::default_host()->GetOobeUI()->display_type() !=
      OobeUI::kOobeDisplay) {
    return;
  }
  LoginDisplayHost::default_host()->FinalizeImmediately();
  CHECK(!LoginDisplayHost::default_host());
}

// ShowLoginWizard is split into two parts. This function is sometimes called
// from TriggerShowLoginWizardFinish() directly, and sometimes from
// OnLanguageSwitchedCallback()
// (if locale was updated).
void ShowLoginWizardFinish(
    chromeos::OobeScreenId first_screen,
    const chromeos::StartupCustomizationDocument* startup_manifest) {
  TRACE_EVENT0("chromeos", "ShowLoginWizard::ShowLoginWizardFinish");

  if (ShouldShowSigninScreen(first_screen)) {
    // Shutdown WebUI host to replace with the Mojo one.
    MaybeShutdownLoginDisplayHostWebUI();
  }

  // TODO(crbug.com/781402): Move LoginDisplayHost creation out of
  // LoginDisplayHostWebUI, it is not specific to a particular implementation.

  // Create the LoginDisplayHost. Use the views-based implementation only for
  // the sign-in screen.
  chromeos::LoginDisplayHost* display_host = nullptr;
  if (chromeos::LoginDisplayHost::default_host()) {
    // Tests may have already allocated an instance for us to use.
    display_host = chromeos::LoginDisplayHost::default_host();
  } else if (ShouldShowSigninScreen(first_screen)) {
    display_host = new chromeos::LoginDisplayHostMojo(
        LoginDisplayHostMojo::DisplayedScreen::SIGN_IN_SCREEN);
  } else {
    display_host = new chromeos::LoginDisplayHostWebUI();
  }

  // Restore system timezone.
  std::string timezone;
  if (chromeos::system::PerUserTimezoneEnabled()) {
    timezone = g_browser_process->local_state()->GetString(
        prefs::kSigninScreenTimezone);
  }

  // TODO(crbug.com/1105387): Part of initial screen logic.
  if (ShouldShowSigninScreen(first_screen)) {
    display_host->StartSignInScreen();
  } else {
    display_host->StartWizard(first_screen);

    // Set initial timezone if specified by customization.
    const std::string customization_timezone =
        startup_manifest->initial_timezone();
    VLOG(1) << "Initial time zone: " << customization_timezone;
    // Apply locale customizations only once to preserve whatever locale
    // user has changed to during OOBE.
    if (!customization_timezone.empty())
      timezone = customization_timezone;
  }
  if (!timezone.empty()) {
    chromeos::system::SetSystemAndSigninScreenTimezone(timezone);
  }

  // This step requires the session manager to have been initialized and login
  // display host to be created.
  DCHECK(session_manager::SessionManager::Get());
  DCHECK(chromeos::LoginDisplayHost::default_host());
  WallpaperControllerClient::Get()->SetInitialWallpaper();
  // TODO(crbug.com/1105387): Part of initial screen logic.
  MaybeShowDeviceDisabledScreen();
}

struct ShowLoginWizardSwitchLanguageCallbackData {
  explicit ShowLoginWizardSwitchLanguageCallbackData(
      chromeos::OobeScreenId first_screen,
      const chromeos::StartupCustomizationDocument* startup_manifest)
      : first_screen(first_screen), startup_manifest(startup_manifest) {}

  const chromeos::OobeScreenId first_screen;
  const chromeos::StartupCustomizationDocument* const startup_manifest;

  // lock UI while resource bundle is being reloaded.
  chromeos::InputEventsBlocker events_blocker;
};

// Trigger OnLocaleChanged via ash::LocaleUpdateController.
void NotifyLocaleChange() {
  ash::LocaleUpdateController::Get()->OnLocaleChanged();
}

void OnLanguageSwitchedCallback(
    std::unique_ptr<ShowLoginWizardSwitchLanguageCallbackData> self,
    const chromeos::locale_util::LanguageSwitchResult& result) {
  if (!result.success)
    LOG(WARNING) << "Locale could not be found for '" << result.requested_locale
                 << "'";

  // Notify the locale change.
  NotifyLocaleChange();
  ShowLoginWizardFinish(self->first_screen, self->startup_manifest);
}

// Triggers ShowLoginWizardFinish directly if no locale switch is required
// (|switch_locale| is empty) or after a locale switch otherwise.
void TriggerShowLoginWizardFinish(
    std::string switch_locale,
    std::unique_ptr<ShowLoginWizardSwitchLanguageCallbackData> data) {
  if (switch_locale.empty()) {
    ShowLoginWizardFinish(data->first_screen, data->startup_manifest);
  } else {
    chromeos::locale_util::SwitchLanguageCallback callback(
        base::Bind(&OnLanguageSwitchedCallback, base::Passed(std::move(data))));

    // Load locale keyboards here. Hardware layout would be automatically
    // enabled.
    chromeos::locale_util::SwitchLanguage(
        switch_locale, true, true /* login_layouts_only */, callback,
        ProfileManager::GetActiveUserProfile());
  }
}

// Returns the login screen locale mandated by device policy, or an empty string
// if no policy-specified locale is set.
std::string GetManagedLoginScreenLocale() {
  chromeos::CrosSettings* cros_settings = chromeos::CrosSettings::Get();
  const base::ListValue* login_screen_locales = nullptr;
  if (!cros_settings->GetList(chromeos::kDeviceLoginScreenLocales,
                              &login_screen_locales))
    return std::string();

  // Currently, only the first element is used. The setting is a list for future
  // compatibility, if dynamically switching locales on the login screen will be
  // implemented.
  std::string login_screen_locale;
  if (login_screen_locales->empty() ||
      !login_screen_locales->GetString(0, &login_screen_locale))
    return std::string();

  return login_screen_locale;
}

// Disables virtual keyboard overscroll. Login UI will scroll user pods
// into view on JS side when virtual keyboard is shown.
void DisableKeyboardOverscroll() {
  auto* client = ChromeKeyboardControllerClient::Get();
  keyboard::KeyboardConfig config = client->GetKeyboardConfig();
  config.overscroll_behavior = keyboard::KeyboardOverscrollBehavior::kDisabled;
  client->SetKeyboardConfig(config);
}

void ResetKeyboardOverscrollBehavior() {
  auto* client = ChromeKeyboardControllerClient::Get();
  keyboard::KeyboardConfig config = client->GetKeyboardConfig();
  config.overscroll_behavior = keyboard::KeyboardOverscrollBehavior::kDefault;
  client->SetKeyboardConfig(config);
}

// Workaround for graphical glitches with animated user avatars due to a race
// between GPU process cleanup for the closing WebContents versus compositor
// draw of new animation frames. https://crbug.com/759148
class CloseAfterCommit : public ui::CompositorObserver,
                         public views::WidgetObserver {
 public:
  explicit CloseAfterCommit(views::Widget* widget) : widget_(widget) {
    widget->GetCompositor()->AddObserver(this);
    widget_->AddObserver(this);
  }
  ~CloseAfterCommit() override {
    widget_->RemoveObserver(this);
    widget_->GetCompositor()->RemoveObserver(this);
    CHECK(!IsInObserverList());
  }

  // ui::CompositorObserver:
  void OnCompositingDidCommit(ui::Compositor* compositor) override {
    DCHECK_EQ(widget_->GetCompositor(), compositor);
    widget_->Close();
  }

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override {
    DCHECK_EQ(widget, widget_);
    delete this;
  }

 private:
  views::Widget* const widget_;

  DISALLOW_COPY_AND_ASSIGN(CloseAfterCommit);
};

// Returns true if we have default audio device.
bool CanPlayStartupSound() {
  chromeos::AudioDevice device;
  bool found =
      chromeos::CrasAudioHandler::Get()->GetPrimaryActiveOutputDevice(&device);
  return found && device.stable_device_id_version &&
         device.type != chromeos::AudioDeviceType::AUDIO_TYPE_OTHER;
}

}  // namespace

// static
const trace_event_internal::TraceID LoginDisplayHostWebUI::kShowLoginWebUIid =
    TRACE_ID_WITH_SCOPE("ShowLoginWebUI", TRACE_ID_GLOBAL(1));
bool LoginDisplayHostWebUI::disable_restrictive_proxy_check_for_test_ = false;

// A class to handle special menu key for keyboard driven OOBE.
class LoginDisplayHostWebUI::KeyboardDrivenOobeKeyHandler
    : public ui::EventHandler {
 public:
  KeyboardDrivenOobeKeyHandler() {
    ash::Shell::Get()->AddPreTargetHandler(this);
  }
  ~KeyboardDrivenOobeKeyHandler() override {
    ash::Shell::Get()->RemovePreTargetHandler(this);
  }

 private:
  // ui::EventHandler
  void OnKeyEvent(ui::KeyEvent* event) override {
    if (event->key_code() == ui::VKEY_F6) {
      SystemTrayClient::Get()->SetPrimaryTrayVisible(false);
      event->StopPropagation();
    }
  }

  DISALLOW_COPY_AND_ASSIGN(KeyboardDrivenOobeKeyHandler);
};

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, public

LoginDisplayHostWebUI::LoginDisplayHostWebUI()
    : oobe_startup_sound_played_(StartupUtils::IsOobeCompleted()) {
  SessionManagerClient::Get()->AddObserver(this);
  CrasAudioHandler::Get()->AddAudioObserver(this);

  display::Screen::GetScreen()->AddObserver(this);

  ui::DeviceDataManager::GetInstance()->AddObserver(this);

  // When we wait for WebUI to be initialized we wait for one of
  // these notifications.
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN,
                 content::NotificationService::AllSources());

  audio::SoundsManager* manager = audio::SoundsManager::Get();
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  manager->Initialize(SOUND_STARTUP,
                      bundle.GetRawDataResource(IDR_SOUND_STARTUP_WAV));

  login_display_ = std::make_unique<LoginDisplayWebUI>();
}

LoginDisplayHostWebUI::~LoginDisplayHostWebUI() {
  SessionManagerClient::Get()->RemoveObserver(this);
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);

  if (waiting_for_configuration_) {
    OobeConfiguration::Get()->RemoveObserver(this);
    waiting_for_configuration_ = false;
  }

  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);

  if (login_view_ && login_window_)
    login_window_->RemoveRemovalsObserver(this);

  ash::MultiUserWindowManager* window_manager =
      MultiUserWindowManagerHelper::GetWindowManager();
  // MultiUserWindowManagerHelper instance might be null if no user is logged
  // in - or in a unit test.
  if (window_manager)
    window_manager->RemoveObserver(this);

  ResetKeyboardOverscrollBehavior();

  views::FocusManager::set_arrow_key_traversal_enabled(false);
  ResetLoginWindowAndView();

  // TODO(tengs): This should be refactored. See crbug.com/314934.
  if (user_manager::UserManager::Get()->IsCurrentUserNew()) {
    // DriveOptInController will delete itself when finished.
    (new DriveFirstRunController(ProfileManager::GetActiveUserProfile()))
        ->EnableOfflineMode();
  }
  CHECK(!views::WidgetObserver::IsInObserverList());
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, LoginDisplayHost:

LoginDisplay* LoginDisplayHostWebUI::GetLoginDisplay() {
  return login_display_.get();
}

ExistingUserController* LoginDisplayHostWebUI::GetExistingUserController() {
  return existing_user_controller_.get();
}

gfx::NativeWindow LoginDisplayHostWebUI::GetNativeWindow() const {
  return login_window_ ? login_window_->GetNativeWindow() : nullptr;
}

WebUILoginView* LoginDisplayHostWebUI::GetWebUILoginView() const {
  return login_view_;
}

void LoginDisplayHostWebUI::OnFinalize() {
  DVLOG(1) << "Finalizing LoginDisplayHost. User session starting";

  switch (finalize_animation_type_) {
    case ANIMATION_NONE:
      ShutdownDisplayHost();
      break;
    case ANIMATION_WORKSPACE:
      ScheduleWorkspaceAnimation();
      ShutdownDisplayHost();
      break;
    case ANIMATION_FADE_OUT:
      // Display host is deleted once animation is completed
      // since sign in screen widget has to stay alive.
      ScheduleFadeOutAnimation(kDefaultFadeTimeMs);
      break;
    case ANIMATION_ADD_USER:
      // Defer the deletion of LoginDisplayHost instance until the user adding
      // animation (which is done by UserSwitchAnimatorChromeOS) is finished.
      // This is to guarantee OnUserSwitchAnimationFinished() is called before
      // LoginDisplayHost deletes itself.
      // See crbug.com/541864.
      break;
  }
}

void LoginDisplayHostWebUI::SetStatusAreaVisible(bool visible) {
  status_area_saved_visibility_ = visible;
  if (login_view_)
    login_view_->SetStatusAreaVisible(status_area_saved_visibility_);
}

void LoginDisplayHostWebUI::OnOobeConfigurationChanged() {
  waiting_for_configuration_ = false;
  OobeConfiguration::Get()->RemoveObserver(this);
  StartWizard(first_screen_);
}

void LoginDisplayHostWebUI::StartWizard(OobeScreenId first_screen) {
  if (!StartupUtils::IsOobeCompleted()) {
    CHECK(OobeConfiguration::Get());
    if (waiting_for_configuration_)
      return;
    if (!OobeConfiguration::Get()->CheckCompleted()) {
      waiting_for_configuration_ = true;
      first_screen_ = first_screen;
      OobeConfiguration::Get()->AddAndFireObserver(this);
      VLOG(1) << "Login WebUI >> wizard waiting for configuration check";
      return;
    }
  }

  DisableKeyboardOverscroll();

  TryToPlayOobeStartupSound();

  // Keep parameters to restore if renderer crashes.
  restore_path_ = RESTORE_WIZARD;
  first_screen_ = first_screen;
  is_showing_login_ = WizardController::IsSigninScreen(first_screen);

  VLOG(1) << "Login WebUI >> wizard";

  if (!login_window_)
    LoadURL(GURL(kOobeURL));

  DVLOG(1) << "Starting wizard, first_screen: " << first_screen;
  oobe_progress_bar_visible_ = !StartupUtils::IsDeviceRegistered();
  SetOobeProgressBarVisible(oobe_progress_bar_visible_);

  // Create and show the wizard.
  if (wizard_controller_) {
    wizard_controller_->AdvanceToScreen(first_screen);
  } else {
    wizard_controller_ = std::make_unique<WizardController>();
    wizard_controller_->Init(first_screen);
  }
}

WizardController* LoginDisplayHostWebUI::GetWizardController() {
  return wizard_controller_.get();
}

void LoginDisplayHostWebUI::OnStartUserAdding() {
  DisableKeyboardOverscroll();

  restore_path_ = RESTORE_ADD_USER_INTO_SESSION;
  finalize_animation_type_ = ANIMATION_ADD_USER;

  // Observe the user switch animation and defer the deletion of itself only
  // after the animation is finished.
  ash::MultiUserWindowManager* window_manager =
      MultiUserWindowManagerHelper::GetWindowManager();
  // MultiUserWindowManagerHelper instance might be nullptr in a unit test.
  if (window_manager)
    window_manager->AddObserver(this);

  VLOG(1) << "Login WebUI >> user adding";
  if (!login_window_)
    LoadURL(GURL(kUserAddingURL));
  // We should emit this signal only at login screen (after reboot or sign out).
  login_view_->set_should_emit_login_prompt_visible(false);

  // Lock container can be transparent after lock screen animation.
  aura::Window* lock_container = ash::Shell::GetContainer(
      ash::Shell::GetPrimaryRootWindow(),
      ash::kShellWindowId_LockScreenContainersContainer);
  lock_container->layer()->SetOpacity(1.0);

  CreateExistingUserController();

  SetOobeProgressBarVisible(oobe_progress_bar_visible_ = false);
  SetStatusAreaVisible(true);
  existing_user_controller_->Init(
      user_manager::UserManager::Get()->GetUsersAllowedForMultiProfile());
  CHECK(login_display_);
  GetOobeUI()->ShowSigninScreen(login_display_.get());
}

void LoginDisplayHostWebUI::CancelUserAdding() {
  // ANIMATION_ADD_USER observes UserSwitchAnimatorChromeOS to shutdown the
  // login display host. However, the animation does not run when user adding is
  // canceled. Changing to ANIMATION_NONE so that Finalize() shuts down the host
  // immediately.
  finalize_animation_type_ = ANIMATION_NONE;
  Finalize(base::OnceClosure());
}

void LoginDisplayHostWebUI::OnStartSignInScreen() {
  DisableKeyboardOverscroll();

  restore_path_ = RESTORE_SIGN_IN;
  is_showing_login_ = true;
  finalize_animation_type_ = ANIMATION_WORKSPACE;

  VLOG(1) << "Login WebUI >> sign in";

  // TODO(crbug.com/784495): Make sure this is ported to views.
  if (!login_window_) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ui", "ShowLoginWebUI",
                                      kShowLoginWebUIid);
    TRACE_EVENT_NESTABLE_ASYNC_INSTANT0(
        "ui", "StartSignInScreen", LoginDisplayHostWebUI::kShowLoginWebUIid);
    BootTimesRecorder::Get()->RecordCurrentStats("login-start-signin-screen");
    LoadURL(GURL(kLoginURL));
  }

  DVLOG(1) << "Starting sign in screen";
  CreateExistingUserController();

  // TODO(crbug.com/784495): This is always false, since
  // LoginDisplayHost::StartSignInScreen marks the device as registered.
  oobe_progress_bar_visible_ = !StartupUtils::IsDeviceRegistered();
  SetOobeProgressBarVisible(oobe_progress_bar_visible_);
  existing_user_controller_->Init(user_manager::UserManager::Get()->GetUsers());

  CHECK(login_display_);
  GetOobeUI()->ShowSigninScreen(login_display_.get());

  OnStartSignInScreenCommon();

  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("ui", "WaitForScreenStateInitialize",
                                      LoginDisplayHostWebUI::kShowLoginWebUIid);

  // TODO(crbug.com/784495): Make sure this is ported to views.
  BootTimesRecorder::Get()->RecordCurrentStats(
      "login-wait-for-signin-state-initialize");
}

void LoginDisplayHostWebUI::OnPreferencesChanged() {
  if (is_showing_login_)
    login_display_->OnPreferencesChanged();
}

void LoginDisplayHostWebUI::OnStartAppLaunch() {
  finalize_animation_type_ = ANIMATION_FADE_OUT;
  if (!login_window_)
    LoadURL(GURL(kAppLaunchSplashURL));

  login_view_->set_should_emit_login_prompt_visible(false);
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, public

void LoginDisplayHostWebUI::OnBrowserCreated() {
  // Close lock window now so that the launched browser can receive focus.
  ResetLoginWindowAndView();
}

OobeUI* LoginDisplayHostWebUI::GetOobeUI() const {
  if (!login_view_)
    return nullptr;
  return login_view_->GetOobeUI();
}

content::WebContents* LoginDisplayHostWebUI::GetOobeWebContents() const {
  if (!login_view_)
    return nullptr;
  return login_view_->GetWebContents();
}
////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, content:NotificationObserver:

void LoginDisplayHostWebUI::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  LoginDisplayHostCommon::Observe(type, source, details);

  if (chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE == type ||
      chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN == type) {
    VLOG(1) << "Login WebUI >> WEBUI_VISIBLE";
    ShowWebUI();
    registrar_.Remove(this, chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                      content::NotificationService::AllSources());
    registrar_.Remove(this, chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN,
                      content::NotificationService::AllSources());
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, WebContentsObserver:

void LoginDisplayHostWebUI::RenderProcessGone(base::TerminationStatus status) {
  // Do not try to restore on shutdown
  if (browser_shutdown::HasShutdownStarted())
    return;

  crash_count_++;
  if (crash_count_ > kCrashCountLimit)
    return;

  if (status != base::TERMINATION_STATUS_NORMAL_TERMINATION) {
    // Render with login screen crashed. Let's crash browser process to let
    // session manager restart it properly. It is hard to reload the page
    // and get to controlled state that is fully functional.
    // If you see check, search for renderer crash for the same client.
    LOG(FATAL) << "Renderer crash on login window";
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, chromeos::SessionManagerClient::Observer:

void LoginDisplayHostWebUI::EmitLoginPromptVisibleCalled() {
  OnLoginPromptVisible();
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, chromeos::CrasAudioHandler::AudioObserver:

void LoginDisplayHostWebUI::OnActiveOutputNodeChanged() {
  PlayStartupSoundIfPossible();
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, display::DisplayObserver:

void LoginDisplayHostWebUI::OnDisplayAdded(
    const display::Display& new_display) {
  if (GetOobeUI())
    GetOobeUI()->OnDisplayConfigurationChanged();
}

void LoginDisplayHostWebUI::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  const display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  if (display.id() != primary_display.id() ||
      !(changed_metrics & DISPLAY_METRIC_BOUNDS)) {
    return;
  }

  if (GetOobeUI()) {
    const gfx::Size& size = primary_display.size();
    GetOobeUI()->GetCoreOobeView()->SetClientAreaSize(size.width(),
                                                      size.height());

    if (changed_metrics & DISPLAY_METRIC_PRIMARY)
      GetOobeUI()->OnDisplayConfigurationChanged();
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, ui::InputDeviceEventObserver
void LoginDisplayHostWebUI::OnInputDeviceConfigurationChanged(
    uint8_t input_device_types) {
  if ((input_device_types & ui::InputDeviceEventObserver::kTouchscreen) &&
      GetOobeUI()) {
    GetOobeUI()->OnDisplayConfigurationChanged();
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, views::WidgetRemovalsObserver:
void LoginDisplayHostWebUI::OnWillRemoveView(views::Widget* widget,
                                             views::View* view) {
  if (view != static_cast<views::View*>(login_view_))
    return;
  login_view_ = nullptr;
  widget->RemoveRemovalsObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, views::WidgetObserver:
void LoginDisplayHostWebUI::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(login_window_, widget);
  login_window_->RemoveRemovalsObserver(this);
  login_window_->RemoveObserver(this);

  login_window_ = nullptr;
  login_view_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, ash::MultiUserWindowManagerObserver:
void LoginDisplayHostWebUI::OnUserSwitchAnimationFinished() {
  ShutdownDisplayHost();
}

////////////////////////////////////////////////////////////////////////////////
// LoginDisplayHostWebUI, private

void LoginDisplayHostWebUI::ScheduleWorkspaceAnimation() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableLoginAnimations)) {
    ash::Shell::Get()->DoInitialWorkspaceAnimation();
  }
}

void LoginDisplayHostWebUI::ScheduleFadeOutAnimation(int animation_speed_ms) {
  // login window might have been closed by OnBrowserCreated() at this moment.
  // This may happen when adding another user into the session, and a browser
  // is created before session start, which triggers the close of the login
  // window. In this case, we should shut down the display host directly.
  if (!login_window_) {
    ShutdownDisplayHost();
    return;
  }
  ui::Layer* layer = login_window_->GetLayer();
  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.AddObserver(new AnimationObserver(
      base::Bind(&LoginDisplayHostWebUI::ShutdownDisplayHost,
                 weak_factory_.GetWeakPtr())));
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(animation_speed_ms));
  layer->SetOpacity(0);
}

void LoginDisplayHostWebUI::LoadURL(const GURL& url) {
  InitLoginWindowAndView();
  // Subscribe to crash events.
  content::WebContentsObserver::Observe(login_view_->GetWebContents());
  login_view_->LoadURL(url);
}

void LoginDisplayHostWebUI::ShowWebUI() {
  DCHECK(login_window_);
  DCHECK(login_view_);

  VLOG(1) << "Login WebUI >> Show already initialized UI";
  login_window_->Show();
  login_view_->GetWebContents()->Focus();
  login_view_->SetStatusAreaVisible(status_area_saved_visibility_);
  login_view_->OnPostponedShow();
}

void LoginDisplayHostWebUI::InitLoginWindowAndView() {
  if (login_window_)
    return;

  if (system::InputDeviceSettings::ForceKeyboardDrivenUINavigation()) {
    views::FocusManager::set_arrow_key_traversal_enabled(true);
    focus_ring_controller_ = std::make_unique<ash::FocusRingController>();
    focus_ring_controller_->SetVisible(true);

    keyboard_driven_oobe_key_handler_.reset(new KeyboardDrivenOobeKeyHandler);
  }

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = CalculateScreenBounds(gfx::Size());
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;

  ash_util::SetupWidgetInitParamsForContainer(
      &params, ash::kShellWindowId_LockScreenContainer);
  login_window_ = new views::Widget;
  login_window_->Init(std::move(params));

  login_view_ = new WebUILoginView(WebUILoginView::WebViewSettings(),
                                   weak_factory_.GetWeakPtr());
  login_view_->Init();
  if (login_view_->webui_visible())
    OnLoginPromptVisible();
  if (disable_restrictive_proxy_check_for_test_) {
    DisableRestrictiveProxyCheckForTest();
  }

  login_window_->SetVisibilityAnimationDuration(
      base::TimeDelta::FromMilliseconds(kLoginFadeoutTransitionDurationMs));
  login_window_->SetVisibilityAnimationTransition(views::Widget::ANIMATE_HIDE);

  login_window_->AddObserver(this);
  login_window_->AddRemovalsObserver(this);
  login_window_->SetContentsView(login_view_);
  login_window_->GetNativeView()->SetName("WebUILoginView");

  // Delay showing the window until the login webui is ready.
  VLOG(1) << "Login WebUI >> login window is hidden on create";
  login_view_->set_is_hidden(true);
}

void LoginDisplayHostWebUI::ResetLoginWindowAndView() {
  // Notify any oobe dialog state observers (e.g. login shelf) that the UI is
  // hidden (so they can reset any cached OOBE dialog state.)
  ash::LoginScreen::Get()->GetModel()->NotifyOobeDialogState(
      ash::OobeDialogState::HIDDEN);

  // Make sure to reset the |login_view_| pointer first; it is owned by
  // |login_window_|. Closing |login_window_| could immediately invalidate the
  // |login_view_| pointer.
  if (login_view_) {
    login_view_->SetUIEnabled(true);
    login_view_ = nullptr;
  }

  if (login_window_) {
    login_window_->Hide();
    // This CompositorObserver becomes "owned" by login_window_ after
    // construction and will delete itself once login_window_ is destroyed.
    new CloseAfterCommit(login_window_);
    login_window_->RemoveRemovalsObserver(this);
    login_window_->RemoveObserver(this);
    login_window_ = nullptr;
  }

  // Release wizard controller with the webui and hosting window so that it
  // does not find missing webui handlers in surprise.
  wizard_controller_.reset();
}

void LoginDisplayHostWebUI::SetOobeProgressBarVisible(bool visible) {
  GetOobeUI()->ShowOobeUI(visible);
}

void LoginDisplayHostWebUI::TryToPlayOobeStartupSound() {
  need_to_play_startup_sound_ = true;
  PlayStartupSoundIfPossible();
}

void LoginDisplayHostWebUI::OnLoginPromptVisible() {
  if (!login_prompt_visible_time_.is_null())
    return;
  login_prompt_visible_time_ = base::TimeTicks::Now();
  TryToPlayOobeStartupSound();
}

void LoginDisplayHostWebUI::CreateExistingUserController() {
  existing_user_controller_ = std::make_unique<ExistingUserController>();
  login_display_->set_delegate(existing_user_controller_.get());
}

// static
void LoginDisplayHostWebUI::DisableRestrictiveProxyCheckForTest() {
  if (default_host() && default_host()->GetOobeUI()) {
    default_host()
        ->GetOobeUI()
        ->GetView<GaiaScreenHandler>()
        ->DisableRestrictiveProxyCheckForTest();
    disable_restrictive_proxy_check_for_test_ = false;
  } else {
    disable_restrictive_proxy_check_for_test_ = true;
  }
}

void LoginDisplayHostWebUI::ShowGaiaDialog(const AccountId& prefilled_account) {
  // This is a special case, when WebUI sign-in screen shown with Views-based
  // launch bar. Then "Add user" button will be Views-based, and user click
  // will result in this call.
  ShowGaiaDialogCommon(prefilled_account);
}

void LoginDisplayHostWebUI::HideOobeDialog() {
  NOTREACHED();
}

void LoginDisplayHostWebUI::UpdateOobeDialogState(ash::OobeDialogState state) {
  ash::LoginScreen::Get()->GetModel()->NotifyOobeDialogState(state);
}

void LoginDisplayHostWebUI::HandleDisplayCaptivePortal() {
  GetOobeUI()->GetErrorScreen()->FixCaptivePortal();
}

void LoginDisplayHostWebUI::OnCancelPasswordChangedFlow() {}

void LoginDisplayHostWebUI::UpdateAddUserButtonStatus() {
  NOTREACHED();
}

void LoginDisplayHostWebUI::RequestSystemInfoUpdate() {
  NOTREACHED();
}

void LoginDisplayHostWebUI::PlayStartupSoundIfPossible() {
  if (!need_to_play_startup_sound_ || oobe_startup_sound_played_)
    return;

  if (login_prompt_visible_time_.is_null())
    return;

  if (!CanPlayStartupSound())
    return;

  need_to_play_startup_sound_ = false;
  oobe_startup_sound_played_ = true;

  const base::TimeDelta time_since_login_prompt_visible =
      base::TimeTicks::Now() - login_prompt_visible_time_;
  UMA_HISTOGRAM_TIMES("Accessibility.OOBEStartupSoundDelay",
                      time_since_login_prompt_visible);

  // Don't try to play startup sound if login prompt has been already visible
  // for a long time.
  if (time_since_login_prompt_visible >
      base::TimeDelta::FromMilliseconds(kStartupSoundMaxDelayMs)) {
    return;
  }
  AccessibilityManager::Get()->PlayEarcon(SOUND_STARTUP,
                                          PlaySoundOption::ALWAYS);
}

////////////////////////////////////////////////////////////////////////////////
// external

// Declared in login_wizard.h so that others don't need to depend on our .h.
// TODO(nkostylev): Split this into a smaller functions.
void ShowLoginWizard(OobeScreenId first_screen) {
  if (browser_shutdown::IsTryingToQuit())
    return;

  VLOG(1) << "Showing OOBE screen: " << first_screen;

  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::Get();

  // Set up keyboards. For example, when |locale| is "en-US", enable US qwerty
  // and US dvorak keyboard layouts.
  if (g_browser_process && g_browser_process->local_state()) {
    manager->GetActiveIMEState()->SetInputMethodLoginDefault();

    PrefService* prefs = g_browser_process->local_state();
    // Apply owner preferences for tap-to-click and mouse buttons swap for
    // login screen.
    system::InputDeviceSettings::Get()->SetPrimaryButtonRight(
        prefs->GetBoolean(prefs::kOwnerPrimaryMouseButtonRight));
    system::InputDeviceSettings::Get()->SetTapToClick(
        prefs->GetBoolean(prefs::kOwnerTapToClickEnabled));
  }
  system::InputDeviceSettings::Get()->SetNaturalScroll(
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNaturalScrollDefault));

  auto session_state = session_manager::SessionState::OOBE;
  if (IsOobeComplete())
    session_state = session_manager::SessionState::LOGIN_PRIMARY;
  session_manager::SessionManager::Get()->SetSessionState(session_state);

  bool show_app_launch_splash_screen =
      (first_screen == AppLaunchSplashScreenView::kScreenId);
  if (show_app_launch_splash_screen) {
    // Manages its own lifetime. See ShutdownDisplayHost().
    auto* display_host = new LoginDisplayHostWebUI();

    KioskAppId kiosk_app_id;
    const std::string& chrome_kiosk_app_id =
        KioskAppManager::Get()->GetAutoLaunchApp();
    const AccountId& web_kiosk_account_id =
        WebKioskAppManager::Get()->GetAutoLaunchAccountId();
    const AccountId& arc_kiosk_account_id =
        ArcKioskAppManager::Get()->GetAutoLaunchAccountId();
    if (!chrome_kiosk_app_id.empty())
      kiosk_app_id = KioskAppId::ForChromeApp(chrome_kiosk_app_id);
    else if (web_kiosk_account_id.is_valid())
      kiosk_app_id = KioskAppId::ForWebApp(web_kiosk_account_id);
    else if (arc_kiosk_account_id.is_valid())
      kiosk_app_id = KioskAppId::ForArcApp(arc_kiosk_account_id);

    display_host->StartKiosk(kiosk_app_id, /* auto_launch */ true);
    return;
  }

  // Check whether we need to execute OOBE flow.
  const policy::EnrollmentConfig enrollment_config =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetPrescribedEnrollmentConfig();
  if (enrollment_config.should_enroll() &&
      first_screen == OobeScreen::SCREEN_UNKNOWN) {
    // Manages its own lifetime. See ShutdownDisplayHost().
    auto* display_host = new LoginDisplayHostWebUI();
    // Shows networks screen instead of enrollment screen to resume the
    // interrupted auto start enrollment flow because enrollment screen does
    // not handle flaky network. See http://crbug.com/332572
    display_host->StartWizard(WelcomeView::kScreenId);
    return;
  }

  if (StartupUtils::IsEulaAccepted()) {
    DelayNetworkCall(
        base::TimeDelta::FromMilliseconds(kDefaultNetworkRetryDelayMS),
        ServicesCustomizationDocument::GetInstance()
            ->EnsureCustomizationAppliedClosure());

    g_browser_process->platform_part()
        ->GetTimezoneResolverManager()
        ->UpdateTimezoneResolver();
  }

  PrefService* prefs = g_browser_process->local_state();
  std::string current_locale =
      prefs->GetString(language::prefs::kApplicationLocale);
  language::ConvertToActualUILocale(&current_locale);
  VLOG(1) << "Current locale: " << current_locale;

  if (ShouldShowSigninScreen(first_screen)) {
    std::string switch_locale = GetManagedLoginScreenLocale();
    if (switch_locale == current_locale)
      switch_locale.clear();

    std::unique_ptr<ShowLoginWizardSwitchLanguageCallbackData> data =
        std::make_unique<ShowLoginWizardSwitchLanguageCallbackData>(
            first_screen, nullptr);
    TriggerShowLoginWizardFinish(switch_locale, std::move(data));
    return;
  }

  // Load startup manifest.
  const StartupCustomizationDocument* startup_manifest =
      StartupCustomizationDocument::GetInstance();

  // Switch to initial locale if specified by customization
  // and has not been set yet. We cannot call
  // LanguageSwitchMenu::SwitchLanguage here before
  // EmitLoginPromptReady.
  const std::string& locale = startup_manifest->initial_locale_default();

  const std::string& layout = startup_manifest->keyboard_layout();
  VLOG(1) << "Initial locale: " << locale << " keyboard layout: " << layout;

  // Determine keyboard layout from OEM customization (if provided) or
  // initial locale and save it in preferences.
  manager->GetActiveIMEState()->SetInputMethodLoginDefaultFromVPD(locale,
                                                                  layout);

  std::unique_ptr<ShowLoginWizardSwitchLanguageCallbackData> data(
      new ShowLoginWizardSwitchLanguageCallbackData(first_screen,
                                                    startup_manifest));

  if (!current_locale.empty() || locale.empty()) {
    TriggerShowLoginWizardFinish(std::string(), std::move(data));
    return;
  }

  // Save initial locale from VPD/customization manifest as current
  // Chrome locale. Otherwise it will be lost if Chrome restarts.
  // Don't need to schedule pref save because setting initial local
  // will enforce preference saving.
  prefs->SetString(language::prefs::kApplicationLocale, locale);
  StartupUtils::SetInitialLocale(locale);

  TriggerShowLoginWizardFinish(locale, std::move(data));
}

class WebUIToViewsSwitchMetricsReporter : public content::NotificationObserver {
 public:
  WebUIToViewsSwitchMetricsReporter() {
    registrar_.Add(this, chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                   content::NotificationService::AllSources());
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_EQ(LoginDisplayHost::default_host()->GetOobeUI()->display_type(),
              OobeUI::kGaiaSigninDisplay);
    UMA_HISTOGRAM_TIMES("OOBE.WebUIToViewsSwitch.Duration", timer_.Elapsed());
    registrar_.Remove(this, chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                      content::NotificationService::AllSources());
    base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

 private:
  content::NotificationRegistrar registrar_;
  base::ElapsedTimer timer_;
};

void SwitchWebUItoMojo() {
  DCHECK_EQ(LoginDisplayHost::default_host()->GetOobeUI()->display_type(),
            OobeUI::kOobeDisplay);

  // The object deletes itself.
  new WebUIToViewsSwitchMetricsReporter();

  // This replaces WebUI host with the Mojo (views) host.
  ShowLoginWizard(OobeScreen::SCREEN_UNKNOWN);
}

}  // namespace chromeos

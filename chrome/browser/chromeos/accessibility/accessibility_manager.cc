// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/accessibility_controller.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/accessibility_focus_ring_controller.h"
#include "ash/public/cpp/accessibility_focus_ring_info.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/mojom/constants.mojom.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/accessibility/accessibility_extension_api.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_extension_loader.h"
#include "chrome/browser/chromeos/accessibility/dictation_chromeos.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/accessibility/select_to_speak_event_handler_delegate.h"
#include "chrome/browser/chromeos/accessibility/switch_access_event_handler_delegate.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/api/braille_display_private/stub_braille_controller.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/system_connector.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/host_id.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

using content::BrowserThread;
using extensions::api::braille_display_private::BrailleController;
using extensions::api::braille_display_private::DisplayState;
using extensions::api::braille_display_private::KeyEvent;
using extensions::api::braille_display_private::StubBrailleController;

namespace chromeos {

namespace {

// When this flag is set, system sounds will not be played.
constexpr char kAshDisableSystemSounds[] = "ash-disable-system-sounds";

// A key for the spoken feedback enabled boolean state for a known user.
const char kUserSpokenFeedbackEnabled[] = "UserSpokenFeedbackEnabled";

// A key for the startup sound enabled boolean state for a known user.
const char kUserStartupSoundEnabled[] = "UserStartupSoundEnabled";

// A key for the bluetooth braille display for a user.
const char kUserBluetoothBrailleDisplayAddress[] =
    "UserBluetoothBrailleDisplayAddress";

// The name of the Brltty upstart job.
constexpr char kBrlttyUpstartJobName[] = "brltty";

static chromeos::AccessibilityManager* g_accessibility_manager = nullptr;

static BrailleController* g_braille_controller_for_test = nullptr;

BrailleController* GetBrailleController() {
  if (g_braille_controller_for_test)
    return g_braille_controller_for_test;
  // Don't use the real braille controller for tests to avoid automatically
  // starting ChromeVox which confuses some tests.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(::switches::kTestType))
    return StubBrailleController::GetInstance();
  return BrailleController::GetInstance();
}

void EnableChromeVoxAfterSwitchAccessMetric(bool val) {
  base::UmaHistogramBoolean("Accessibility.CrosChromeVoxAfterSwitchAccess",
                            val);
}

void EnableSwitchAccessAfterChromeVoxMetric(bool val) {
  base::UmaHistogramBoolean("Accessibility.CrosSwitchAccessAfterChromeVox",
                            val);
}

// Restarts (stops, then starts brltty). If |address| is empty, only stops.
// In Upstart, sending an explicit restart command is a no-op if the job isn't
// already started. Without knowledge regarding brltty's current job status,
// stop followed by start ensures we both stop a started job, and also start
// brltty.
void RestartBrltty(const std::string& address) {
  chromeos::UpstartClient* client = chromeos::UpstartClient::Get();
  client->StopJob(kBrlttyUpstartJobName, {}, EmptyVoidDBusMethodCallback());

  std::vector<std::string> args;
  if (address.empty())
    return;

  args.push_back(base::StringPrintf("ADDRESS=%s", address.c_str()));
  client->StartJob(kBrlttyUpstartJobName, args, EmptyVoidDBusMethodCallback());
}

bool VolumeAdjustSoundEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDisableVolumeAdjustSound);
}

}  // namespace

class AccessibilityPanelWidgetObserver : public views::WidgetObserver {
 public:
  AccessibilityPanelWidgetObserver(views::Widget* widget,
                                   base::OnceCallback<void()> on_destroying)
      : widget_(widget), on_destroying_(std::move(on_destroying)) {
    widget_->AddObserver(this);
  }

  ~AccessibilityPanelWidgetObserver() override = default;

  void OnWidgetClosing(views::Widget* widget) override {
    CHECK_EQ(widget_, widget);
    widget->RemoveObserver(this);
    std::move(on_destroying_).Run();
    // |this| should be deleted.
  }

  void OnWidgetDestroying(views::Widget* widget) override {
    CHECK_EQ(widget_, widget);
    widget->RemoveObserver(this);
    std::move(on_destroying_).Run();
    // |this| should be deleted.
  }

 private:
  views::Widget* widget_;

  base::OnceCallback<void()> on_destroying_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityPanelWidgetObserver);
};

///////////////////////////////////////////////////////////////////////////////
// AccessibilityStatusEventDetails

AccessibilityStatusEventDetails::AccessibilityStatusEventDetails(
    AccessibilityNotificationType notification_type,
    bool enabled)
    : notification_type(notification_type), enabled(enabled) {}

///////////////////////////////////////////////////////////////////////////////
//
// AccessibilityManager

// static
void AccessibilityManager::Initialize() {
  CHECK(g_accessibility_manager == NULL);
  g_accessibility_manager = new AccessibilityManager();
}

// static
void AccessibilityManager::Shutdown() {
  CHECK(g_accessibility_manager);
  delete g_accessibility_manager;
  g_accessibility_manager = NULL;
}

// static
AccessibilityManager* AccessibilityManager::Get() {
  return g_accessibility_manager;
}

// static
void AccessibilityManager::ShowAccessibilityHelp(Browser* browser) {
  ShowSingletonTab(browser, GURL(chrome::kChromeAccessibilityHelpURL));
}

AccessibilityManager::AccessibilityManager() {
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                              content::NotificationService::AllSources());
  input_method::InputMethodManager::Get()->AddObserver(this);
  user_manager::UserManager::Get()->AddSessionStateObserver(this);

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  audio::SoundsManager* manager = audio::SoundsManager::Get();
  manager->Initialize(SOUND_SHUTDOWN,
                      bundle.GetRawDataResource(IDR_SOUND_SHUTDOWN_WAV));
  manager->Initialize(
      SOUND_SPOKEN_FEEDBACK_ENABLED,
      bundle.GetRawDataResource(IDR_SOUND_SPOKEN_FEEDBACK_ENABLED_WAV));
  manager->Initialize(
      SOUND_SPOKEN_FEEDBACK_DISABLED,
      bundle.GetRawDataResource(IDR_SOUND_SPOKEN_FEEDBACK_DISABLED_WAV));
  manager->Initialize(SOUND_PASSTHROUGH,
                      bundle.GetRawDataResource(IDR_SOUND_PASSTHROUGH_WAV));
  manager->Initialize(SOUND_EXIT_SCREEN,
                      bundle.GetRawDataResource(IDR_SOUND_EXIT_SCREEN_WAV));
  manager->Initialize(SOUND_ENTER_SCREEN,
                      bundle.GetRawDataResource(IDR_SOUND_ENTER_SCREEN_WAV));
  manager->Initialize(SOUND_SPOKEN_FEEDBACK_TOGGLE_COUNTDOWN_HIGH,
                      bundle.GetRawDataResource(
                          IDR_SOUND_SPOKEN_FEEDBACK_TOGGLE_COUNTDOWN_HIGH_WAV));
  manager->Initialize(SOUND_SPOKEN_FEEDBACK_TOGGLE_COUNTDOWN_LOW,
                      bundle.GetRawDataResource(
                          IDR_SOUND_SPOKEN_FEEDBACK_TOGGLE_COUNTDOWN_LOW_WAV));
  manager->Initialize(SOUND_TOUCH_TYPE,
                      bundle.GetRawDataResource(IDR_SOUND_TOUCH_TYPE_WAV));
  manager->Initialize(SOUND_DICTATION_END,
                      bundle.GetRawDataResource(IDR_SOUND_DICTATION_END_WAV));
  manager->Initialize(SOUND_DICTATION_START,
                      bundle.GetRawDataResource(IDR_SOUND_DICTATION_START_WAV));
  manager->Initialize(
      SOUND_DICTATION_CANCEL,
      bundle.GetRawDataResource(IDR_SOUND_DICTATION_CANCEL_WAV));
  manager->Initialize(SOUND_STARTUP,
                      bundle.GetRawDataResource(IDR_SOUND_STARTUP_WAV));

  if (VolumeAdjustSoundEnabled()) {
    manager->Initialize(chromeos::SOUND_VOLUME_ADJUST,
                        bundle.GetRawDataResource(IDR_SOUND_VOLUME_ADJUST_WAV));
  }

  base::FilePath resources_path;
  if (!base::PathService::Get(chrome::DIR_RESOURCES, &resources_path))
    NOTREACHED();
  autoclick_extension_loader_ =
      base::WrapUnique(new AccessibilityExtensionLoader(
          extension_misc::kAutoclickExtensionId,
          resources_path.Append(extension_misc::kAutoclickExtensionPath),
          base::BindRepeating(&AccessibilityManager::PostUnloadAutoclick,
                              weak_ptr_factory_.GetWeakPtr())));
  chromevox_loader_ = base::WrapUnique(new AccessibilityExtensionLoader(
      extension_misc::kChromeVoxExtensionId,
      resources_path.Append(extension_misc::kChromeVoxExtensionPath),
      base::BindRepeating(&AccessibilityManager::PostUnloadChromeVox,
                          weak_ptr_factory_.GetWeakPtr())));
  select_to_speak_loader_ = base::WrapUnique(new AccessibilityExtensionLoader(
      extension_misc::kSelectToSpeakExtensionId,
      resources_path.Append(extension_misc::kSelectToSpeakExtensionPath),
      base::BindRepeating(&AccessibilityManager::PostUnloadSelectToSpeak,
                          weak_ptr_factory_.GetWeakPtr())));
  switch_access_loader_ = base::WrapUnique(new AccessibilityExtensionLoader(
      extension_misc::kSwitchAccessExtensionId,
      resources_path.Append(extension_misc::kSwitchAccessExtensionPath),
      base::BindRepeating(&AccessibilityManager::PostUnloadSwitchAccess,
                          weak_ptr_factory_.GetWeakPtr())));

  // Connect to the media session service.
  content::GetSystemConnector()->Connect(
      media_session::mojom::kServiceName,
      audio_focus_manager_.BindNewPipeAndPassReceiver());

  ash::AcceleratorController::SetVolumeAdjustmentSoundCallback(
      base::BindRepeating(&AccessibilityManager::PlayVolumeAdjustSound,
                          base::Unretained(this)));

  CrasAudioHandler::Get()->AddAudioObserver(this);
}

AccessibilityManager::~AccessibilityManager() {
  CHECK(this == g_accessibility_manager);
  AccessibilityStatusEventDetails details(ACCESSIBILITY_MANAGER_SHUTDOWN,
                                          false);
  NotifyAccessibilityStatusChanged(details);
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
  input_method::InputMethodManager::Get()->RemoveObserver(this);

  if (chromevox_panel_) {
    chromevox_panel_->CloseNow();
    chromevox_panel_ = nullptr;
  }

  ash::AcceleratorController::SetVolumeAdjustmentSoundCallback({});
}

bool AccessibilityManager::ShouldShowAccessibilityMenu() {
  // If any of the loaded profiles has an accessibility feature turned on - or
  // enforced to always show the menu - we return true to show the menu.
  // NOTE: This includes the login screen profile, so if a feature is turned on
  // at the login screen the menu will show even if the user has no features
  // enabled inside the session. http://crbug.com/755631
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (std::vector<Profile*>::iterator it = profiles.begin();
       it != profiles.end(); ++it) {
    PrefService* prefs = (*it)->GetPrefs();
    if (prefs->GetBoolean(ash::prefs::kAccessibilityStickyKeysEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityLargeCursorEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilitySpokenFeedbackEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilitySelectToSpeakEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilitySwitchAccessEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityHighContrastEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityAutoclickEnabled) ||
        prefs->GetBoolean(ash::prefs::kShouldAlwaysShowAccessibilityMenu) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityVirtualKeyboardEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityMonoAudioEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityCaretHighlightEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityCursorHighlightEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityFocusHighlightEnabled) ||
        prefs->GetBoolean(ash::prefs::kAccessibilityDictationEnabled) ||
        prefs->GetBoolean(ash::prefs::kDockedMagnifierEnabled)) {
      return true;
    }
  }
  return false;
}

void AccessibilityManager::UpdateAlwaysShowMenuFromPref() {
  if (!profile_)
    return;

  // Update system tray menu visibility.
  ash::AccessibilityController::Get()->NotifyAccessibilityStatusChanged();
}

void AccessibilityManager::EnableLargeCursor(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilityLargeCursorEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

void AccessibilityManager::OnLargeCursorChanged() {
  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_LARGE_CURSOR,
                                          IsLargeCursorEnabled());
  NotifyAccessibilityStatusChanged(details);
}

bool AccessibilityManager::IsLargeCursorEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilityLargeCursorEnabled);
}

void AccessibilityManager::EnableStickyKeys(bool enabled) {
  if (!profile_)
    return;
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilityStickyKeysEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsStickyKeysEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilityStickyKeysEnabled);
}

void AccessibilityManager::OnStickyKeysChanged() {
  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_STICKY_KEYS,
                                          IsStickyKeysEnabled());
  NotifyAccessibilityStatusChanged(details);
}

void AccessibilityManager::EnableSpokenFeedback(bool enabled) {
  if (!profile_)
    return;

  if (enabled) {
    if (IsSwitchAccessEnabled()) {
      SetSwitchAccessEnabled(false);
      EnableChromeVoxAfterSwitchAccessMetric(true);
    } else {
      EnableChromeVoxAfterSwitchAccessMetric(false);
    }
  }

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilitySpokenFeedbackEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

void AccessibilityManager::OnSpokenFeedbackChanged() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled);

  if (enabled) {
    if (IsSwitchAccessEnabled()) {
      LOG(ERROR) << "Switch Access and ChromeVox is not supported.";
      LOG(WARNING) << "Disabling Switch Access.";
      SetSwitchAccessEnabled(false);
      EnableChromeVoxAfterSwitchAccessMetric(true);
    } else {
      EnableChromeVoxAfterSwitchAccessMetric(false);
    }
  }

  user_manager::known_user::SetBooleanPref(
      multi_user_util::GetAccountIdFromProfile(profile_),
      kUserSpokenFeedbackEnabled, enabled);

  if (enabled) {
    chromevox_loader_->SetProfile(
        profile_,
        base::BindRepeating(&AccessibilityManager::PostSwitchChromeVoxProfile,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  if (spoken_feedback_enabled_ == enabled)
    return;

  spoken_feedback_enabled_ = enabled;

  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK,
                                          enabled);
  NotifyAccessibilityStatusChanged(details);

  if (enabled) {
    chromevox_loader_->Load(
        profile_, base::BindRepeating(&AccessibilityManager::PostLoadChromeVox,
                                      weak_ptr_factory_.GetWeakPtr()));
  } else {
    chromevox_loader_->Unload();
  }
  UpdateBrailleImeState();
}

bool AccessibilityManager::IsSpokenFeedbackEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilitySpokenFeedbackEnabled);
}

void AccessibilityManager::EnableHighContrast(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilityHighContrastEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsHighContrastEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilityHighContrastEnabled);
}

void AccessibilityManager::OnHighContrastChanged() {
  AccessibilityStatusEventDetails details(
      ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE, IsHighContrastEnabled());
  NotifyAccessibilityStatusChanged(details);
}

void AccessibilityManager::OnLocaleChanged() {
  if (!profile_)
    return;

  if (!IsSpokenFeedbackEnabled())
    return;

  // If the system locale changes and spoken feedback is enabled,
  // reload ChromeVox so that it switches its internal translations
  // to the new language.
  EnableSpokenFeedback(false);
  EnableSpokenFeedback(true);
}

void AccessibilityManager::OnViewFocusedInArc(
    const gfx::Rect& bounds_in_screen) {
  ash::AccessibilityController::Get()->SetFocusHighlightRect(bounds_in_screen);
}

bool AccessibilityManager::PlayEarcon(int sound_key, PlaySoundOption option) {
  DCHECK(sound_key < chromeos::SOUND_COUNT);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(kAshDisableSystemSounds))
    return false;
  if (option == PlaySoundOption::ONLY_IF_SPOKEN_FEEDBACK_ENABLED &&
      !IsSpokenFeedbackEnabled()) {
    return false;
  }
  return audio::SoundsManager::Get()->Play(sound_key);
}

void AccessibilityManager::OnTwoFingerTouchStart() {
  if (!profile())
    return;

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile());

  auto event_args = std::make_unique<base::ListValue>();
  auto event = std::make_unique<extensions::Event>(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_TWO_FINGER_TOUCH_START,
      extensions::api::accessibility_private::OnTwoFingerTouchStart::kEventName,
      std::move(event_args));
  event_router->BroadcastEvent(std::move(event));
}

void AccessibilityManager::OnTwoFingerTouchStop() {
  if (!profile())
    return;

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile());

  auto event_args = std::make_unique<base::ListValue>();
  auto event = std::make_unique<extensions::Event>(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_TWO_FINGER_TOUCH_STOP,
      extensions::api::accessibility_private::OnTwoFingerTouchStop::kEventName,
      std::move(event_args));
  event_router->BroadcastEvent(std::move(event));
}

bool AccessibilityManager::ShouldToggleSpokenFeedbackViaTouch() {
#if 1
  // Temporarily disabling this feature until UI feedback is fixed.
  // http://crbug.com/662501
  return false;
#else
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (!connector)
    return false;

  if (!connector->IsEnterpriseManaged())
    return false;

  const policy::DeviceCloudPolicyManagerChromeOS* const
      device_cloud_policy_manager = connector->GetDeviceCloudPolicyManager();
  if (!device_cloud_policy_manager)
    return false;

  if (!device_cloud_policy_manager->IsRemoraRequisition())
    return false;

  KioskAppManager* manager = KioskAppManager::Get();
  KioskAppManager::App app;
  CHECK(manager->GetApp(manager->GetAutoLaunchApp(), &app));
  return app.was_auto_launched_with_zero_delay;
#endif
}

bool AccessibilityManager::PlaySpokenFeedbackToggleCountdown(int tick_count) {
  return audio::SoundsManager::Get()->Play(
      tick_count % 2 ? SOUND_SPOKEN_FEEDBACK_TOGGLE_COUNTDOWN_HIGH
                     : SOUND_SPOKEN_FEEDBACK_TOGGLE_COUNTDOWN_LOW);
}

void AccessibilityManager::HandleAccessibilityGesture(
    ax::mojom::Gesture gesture) {
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile());

  std::unique_ptr<base::ListValue> event_args =
      std::make_unique<base::ListValue>();
  event_args->AppendString(ui::ToString(gesture));
  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_ACCESSIBILITY_GESTURE,
      extensions::api::accessibility_private::OnAccessibilityGesture::
          kEventName,
      std::move(event_args)));
  event_router->DispatchEventWithLazyListener(
      extension_misc::kChromeVoxExtensionId, std::move(event));
}

void AccessibilityManager::SetTouchAccessibilityAnchorPoint(
    const gfx::Point& anchor_point) {
  for (auto* rwc : ash::RootWindowController::root_window_controllers())
    rwc->SetTouchAccessibilityAnchorPoint(anchor_point);
}

void AccessibilityManager::EnableAutoclick(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilityAutoclickEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsAutoclickEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilityAutoclickEnabled);
}

void AccessibilityManager::OnAutoclickChanged() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilityAutoclickEnabled);

  if (enabled)
    autoclick_extension_loader_->SetProfile(
        profile_, base::Closure() /* done_callback */);

  if (autoclick_enabled_ == enabled)
    return;

  autoclick_enabled_ = enabled;
  if (enabled) {
    autoclick_extension_loader_->Load(
        profile_, base::BindRepeating(&AccessibilityManager::PostLoadAutoclick,
                                      weak_ptr_factory_.GetWeakPtr()));
    // TODO: Construct a delegate to connect Autoclick and its controller in
    // ash.
  } else {
    autoclick_extension_loader_->Unload();
  }
}

void AccessibilityManager::RequestAutoclickScrollableBoundsForPoint(
    gfx::Point& point_in_screen) {
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);
  std::unique_ptr<base::ListValue> event_args =
      extensions::api::accessibility_private::FindScrollableBoundsForPoint::
          Create(point_in_screen.x(), point_in_screen.y());
  std::unique_ptr<extensions::Event> event =
      std::make_unique<extensions::Event>(
          extensions::events::
              ACCESSIBILITY_PRIVATE_FIND_SCROLLABLE_BOUNDS_FOR_POINT,
          extensions::api::accessibility_private::FindScrollableBoundsForPoint::
              kEventName,
          std::move(event_args));
  event_router->DispatchEventWithLazyListener(
      extension_misc::kAutoclickExtensionId, std::move(event));
}

void AccessibilityManager::EnableVirtualKeyboard(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilityVirtualKeyboardEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsVirtualKeyboardEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilityVirtualKeyboardEnabled);
}

void AccessibilityManager::OnVirtualKeyboardChanged() {
  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_VIRTUAL_KEYBOARD,
                                          IsVirtualKeyboardEnabled());
  NotifyAccessibilityStatusChanged(details);
}

void AccessibilityManager::EnableMonoAudio(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilityMonoAudioEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsMonoAudioEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilityMonoAudioEnabled);
}

void AccessibilityManager::OnMonoAudioChanged() {
  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_MONO_AUDIO,
                                          IsMonoAudioEnabled());
  NotifyAccessibilityStatusChanged(details);
}

void AccessibilityManager::SetDarkenScreen(bool darken) {
  ash::AccessibilityController::Get()->SetDarkenScreen(darken);
}

void AccessibilityManager::SetCaretHighlightEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilityCaretHighlightEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsCaretHighlightEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilityCaretHighlightEnabled);
}

void AccessibilityManager::OnCaretHighlightChanged() {
  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_CARET_HIGHLIGHT,
                                          IsCaretHighlightEnabled());
  NotifyAccessibilityStatusChanged(details);
}

void AccessibilityManager::SetCursorHighlightEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilityCursorHighlightEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsCursorHighlightEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilityCursorHighlightEnabled);
}

void AccessibilityManager::OnCursorHighlightChanged() {
  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_CURSOR_HIGHLIGHT,
                                          IsCursorHighlightEnabled());
  NotifyAccessibilityStatusChanged(details);
}

bool AccessibilityManager::IsDictationEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilityDictationEnabled);
}

void AccessibilityManager::SetFocusHighlightEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilityFocusHighlightEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsFocusHighlightEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         ash::prefs::kAccessibilityFocusHighlightEnabled);
}

void AccessibilityManager::OnFocusHighlightChanged() {
  bool enabled = IsFocusHighlightEnabled();

  // Focus highlighting can't be on when spoken feedback is on, because
  // ChromeVox does its own focus highlighting.
  if (IsSpokenFeedbackEnabled())
    enabled = false;
  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_FOCUS_HIGHLIGHT,
                                          enabled);
  NotifyAccessibilityStatusChanged(details);
}

void AccessibilityManager::SetSelectToSpeakEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilitySelectToSpeakEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsSelectToSpeakEnabled() const {
  return select_to_speak_enabled_;
}

void AccessibilityManager::RequestSelectToSpeakStateChange() {
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);

  // Send an event to the Select-to-Speak extension requesting a state change.
  std::unique_ptr<base::ListValue> event_args =
      std::make_unique<base::ListValue>();
  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::
          ACCESSIBILITY_PRIVATE_ON_SELECT_TO_SPEAK_STATE_CHANGE_REQUESTED,
      extensions::api::accessibility_private::
          OnSelectToSpeakStateChangeRequested::kEventName,
      std::move(event_args)));
  event_router->DispatchEventWithLazyListener(
      extension_misc::kSelectToSpeakExtensionId, std::move(event));
}

void AccessibilityManager::OnSelectToSpeakStateChanged(
    ash::SelectToSpeakState state) {
  ash::AccessibilityController::Get()->SetSelectToSpeakState(state);

  if (select_to_speak_state_observer_for_test_)
    select_to_speak_state_observer_for_test_.Run();
}

void AccessibilityManager::OnSelectToSpeakChanged() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilitySelectToSpeakEnabled);
  if (enabled)
    select_to_speak_loader_->SetProfile(profile_, base::Closure());

  if (select_to_speak_enabled_ == enabled)
    return;

  select_to_speak_enabled_ = enabled;

  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_SELECT_TO_SPEAK,
                                          enabled);
  NotifyAccessibilityStatusChanged(details);

  if (enabled) {
    select_to_speak_loader_->Load(
        profile_,
        base::BindRepeating(&AccessibilityManager::PostLoadSelectToSpeak,
                            weak_ptr_factory_.GetWeakPtr()));
    // Construct a delegate to connect SelectToSpeak and its EventHandler in
    // ash.
    select_to_speak_event_handler_delegate_ =
        std::make_unique<chromeos::SelectToSpeakEventHandlerDelegate>();
  } else {
    select_to_speak_loader_->Unload();
    select_to_speak_event_handler_delegate_.reset();
  }
}

void AccessibilityManager::SetSwitchAccessEnabled(bool enabled) {
  if (!profile_)
    return;

  if (enabled) {
    if (IsSpokenFeedbackEnabled()) {
      LOG(ERROR) << "Enabling Switch Access with ChromeVox is not supported.";
      EnableSwitchAccessAfterChromeVoxMetric(true);
      return;
    }
    EnableSwitchAccessAfterChromeVoxMetric(false);
  }

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilitySwitchAccessEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsSwitchAccessEnabled() const {
  return switch_access_enabled_;
}

void AccessibilityManager::OnSwitchAccessChanged() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      ash::prefs::kAccessibilitySwitchAccessEnabled);

  // The Switch Access setting is behind a flag. Don't enable the feature
  // even if the preference is enabled, if the flag isn't also set.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(
          ::switches::kEnableExperimentalAccessibilitySwitchAccess)) {
    if (enabled) {
      LOG(WARNING) << "Switch Access enabled but experimental accessibility "
                   << "Switch Access flag is not set.";
    }
    return;
  }

  if (enabled) {
    if (IsSpokenFeedbackEnabled()) {
      LOG(ERROR) << "Enabling Switch Access with ChromeVox is not supported.";
      SetSwitchAccessEnabled(false);
      EnableSwitchAccessAfterChromeVoxMetric(true);
      return;
    }
    EnableSwitchAccessAfterChromeVoxMetric(false);
    switch_access_loader_->SetProfile(profile_, base::RepeatingClosure());
  }

  if (switch_access_enabled_ == enabled)
    return;
  switch_access_enabled_ = enabled;

  AccessibilityStatusEventDetails details(ACCESSIBILITY_TOGGLE_SWITCH_ACCESS,
                                          enabled);
  NotifyAccessibilityStatusChanged(details);

  if (enabled) {
    // Construct a delegate to connect Switch Access and its EventHandler in
    // ash. Do this before loading Switch Access so the keys_to_capture will
    // be properly set.
    switch_access_event_handler_delegate_ =
        std::make_unique<SwitchAccessEventHandlerDelegate>();
    switch_access_loader_->Load(
        profile_,
        base::BindRepeating(&AccessibilityManager::PostLoadSwitchAccess,
                            weak_ptr_factory_.GetWeakPtr()));
  } else {
    switch_access_loader_->Unload();
    switch_access_event_handler_delegate_.reset(nullptr);
  }
}

bool AccessibilityManager::IsBrailleDisplayConnected() const {
  return braille_display_connected_;
}

void AccessibilityManager::CheckBrailleState() {
  BrailleController* braille_controller = GetBrailleController();
  if (!scoped_braille_observer_.IsObserving(braille_controller))
    scoped_braille_observer_.Add(braille_controller);
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {BrowserThread::IO},
      base::Bind(&BrailleController::GetDisplayState,
                 base::Unretained(braille_controller)),
      base::Bind(&AccessibilityManager::ReceiveBrailleDisplayState,
                 weak_ptr_factory_.GetWeakPtr()));
}

void AccessibilityManager::ReceiveBrailleDisplayState(
    std::unique_ptr<extensions::api::braille_display_private::DisplayState>
        state) {
  OnBrailleDisplayStateChanged(*state);
}

void AccessibilityManager::UpdateBrailleImeState() {
  if (!profile_)
    return;
  PrefService* pref_service = profile_->GetPrefs();
  std::string preload_engines_str =
      pref_service->GetString(prefs::kLanguagePreloadEngines);
  std::vector<base::StringPiece> preload_engines = base::SplitStringPiece(
      preload_engines_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::vector<base::StringPiece>::iterator it =
      std::find(preload_engines.begin(), preload_engines.end(),
                extension_ime_util::kBrailleImeEngineId);
  bool is_enabled = (it != preload_engines.end());
  bool should_be_enabled =
      (IsSpokenFeedbackEnabled() && braille_display_connected_);
  if (is_enabled == should_be_enabled)
    return;
  if (should_be_enabled)
    preload_engines.push_back(extension_ime_util::kBrailleImeEngineId);
  else
    preload_engines.erase(it);
  pref_service->SetString(prefs::kLanguagePreloadEngines,
                          base::JoinString(preload_engines, ","));
  braille_ime_current_ = false;
}

// Overridden from InputMethodManager::Observer.
void AccessibilityManager::InputMethodChanged(
    input_method::InputMethodManager* manager,
    Profile* /* profile */,
    bool show_message) {
    ash::Shell::Get()->sticky_keys_controller()->SetModifiersEnabled(
        manager->IsISOLevel5ShiftUsedByCurrentInputMethod(),
        manager->IsAltGrUsedByCurrentInputMethod());
  const chromeos::input_method::InputMethodDescriptor descriptor =
      manager->GetActiveIMEState()->GetCurrentInputMethod();
  braille_ime_current_ =
      (descriptor.id() == extension_ime_util::kBrailleImeEngineId);
}

void AccessibilityManager::OnActiveOutputNodeChanged() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kFirstExecAfterBoot))
    return;

  AudioDevice device;
  CrasAudioHandler::Get()->GetPrimaryActiveOutputDevice(&device);
  if (device.type == AudioDeviceType::AUDIO_TYPE_OTHER)
    return;

  CrasAudioHandler::Get()->RemoveAudioObserver(this);
  if (GetStartupSoundEnabled()) {
    PlayEarcon(SOUND_STARTUP, PlaySoundOption::ALWAYS);
    return;
  }

  const auto& account_ids = user_manager::known_user::GetKnownAccountIds();
  for (size_t i = 0; i < account_ids.size(); ++i) {
    bool val;
    if (user_manager::known_user::GetBooleanPref(
            account_ids[i], kUserSpokenFeedbackEnabled, &val) &&
        val) {
      PlayEarcon(SOUND_STARTUP, PlaySoundOption::ALWAYS);
      break;
    }
  }
}

void AccessibilityManager::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK_EQ(profile_, profile);
  SetProfile(nullptr);
}

void AccessibilityManager::SetProfile(Profile* profile) {
  if (profile_ == profile)
    return;

  if (profile_)
    profile_observer_.Remove(profile_);
  DCHECK(!profile_observer_.IsObservingSources());

  pref_change_registrar_.reset();
  local_state_pref_change_registrar_.reset();

  // Clear all dictation state on profile change.
  dictation_.reset();

  if (profile) {
    // TODO(yoshiki): Move following code to PrefHandler.
    pref_change_registrar_.reset(new PrefChangeRegistrar);
    pref_change_registrar_->Init(profile->GetPrefs());
    pref_change_registrar_->Add(
        ash::prefs::kShouldAlwaysShowAccessibilityMenu,
        base::Bind(&AccessibilityManager::UpdateAlwaysShowMenuFromPref,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityLargeCursorEnabled,
        base::Bind(&AccessibilityManager::OnLargeCursorChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityLargeCursorDipSize,
        base::Bind(&AccessibilityManager::OnLargeCursorChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityStickyKeysEnabled,
        base::Bind(&AccessibilityManager::OnStickyKeysChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilitySpokenFeedbackEnabled,
        base::Bind(&AccessibilityManager::OnSpokenFeedbackChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityHighContrastEnabled,
        base::Bind(&AccessibilityManager::OnHighContrastChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityVirtualKeyboardEnabled,
        base::Bind(&AccessibilityManager::OnVirtualKeyboardChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityMonoAudioEnabled,
        base::Bind(&AccessibilityManager::OnMonoAudioChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityCaretHighlightEnabled,
        base::Bind(&AccessibilityManager::OnCaretHighlightChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityCursorHighlightEnabled,
        base::Bind(&AccessibilityManager::OnCursorHighlightChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityFocusHighlightEnabled,
        base::Bind(&AccessibilityManager::OnFocusHighlightChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilitySelectToSpeakEnabled,
        base::Bind(&AccessibilityManager::OnSelectToSpeakChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilitySwitchAccessEnabled,
        base::Bind(&AccessibilityManager::OnSwitchAccessChanged,
                   base::Unretained(this)));
    pref_change_registrar_->Add(
        ash::prefs::kAccessibilityAutoclickEnabled,
        base::Bind(&AccessibilityManager::OnAutoclickChanged,
                   base::Unretained(this)));

    local_state_pref_change_registrar_.reset(new PrefChangeRegistrar);
    local_state_pref_change_registrar_->Init(g_browser_process->local_state());
    local_state_pref_change_registrar_->Add(
        language::prefs::kApplicationLocale,
        base::Bind(&AccessibilityManager::OnLocaleChanged,
                   base::Unretained(this)));

    // Compute these histograms on the main (UI) thread because they
    // need to access PrefService.
    content::BrowserAccessibilityState::GetInstance()
        ->AddUIThreadHistogramCallback(base::BindOnce(
            &AccessibilityManager::UpdateChromeOSAccessibilityHistograms,
            base::Unretained(this)));

    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile);
    if (!extension_registry_observer_.IsObserving(registry))
      extension_registry_observer_.Add(registry);

    profile_observer_.Add(profile);
  }

  bool had_profile = (profile_ != NULL);
  profile_ = profile;

  if (!had_profile && profile)
    CheckBrailleState();
  else
    UpdateBrailleImeState();
  UpdateAlwaysShowMenuFromPref();

  // TODO(warx): reconcile to ash once the prefs registration above is moved to
  // ash.
  OnSpokenFeedbackChanged();
  OnSwitchAccessChanged();
  OnSelectToSpeakChanged();
  OnAutoclickChanged();
}

void AccessibilityManager::SetProfileByUser(const user_manager::User* user) {
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
  DCHECK(profile);
  SetProfile(profile);
}

void AccessibilityManager::ActiveUserChanged(user_manager::User* active_user) {
  if (!active_user)
    return;

  active_user->AddProfileCreatedObserver(
      base::BindOnce(&AccessibilityManager::SetProfileByUser,
                     weak_ptr_factory_.GetWeakPtr(), active_user));
}

base::TimeDelta AccessibilityManager::PlayShutdownSound() {
  if (!PlayEarcon(SOUND_SHUTDOWN,
                  PlaySoundOption::ONLY_IF_SPOKEN_FEEDBACK_ENABLED)) {
    return base::TimeDelta();
  }
  return audio::SoundsManager::Get()->GetDuration(SOUND_SHUTDOWN);
}

std::unique_ptr<AccessibilityStatusSubscription>
AccessibilityManager::RegisterCallback(const AccessibilityStatusCallback& cb) {
  return callback_list_.Add(cb);
}

void AccessibilityManager::NotifyAccessibilityStatusChanged(
    const AccessibilityStatusEventDetails& details) {
  callback_list_.Notify(details);

  if (details.notification_type == ACCESSIBILITY_TOGGLE_DICTATION) {
    ash::AccessibilityController::Get()->SetDictationActive(details.enabled);
    ash::AccessibilityController::Get()->NotifyAccessibilityStatusChanged();
    return;
  }

  // Update system tray menu visibility. Prefs tracked inside ash handle their
  // own updates to avoid race conditions (pref updates are asynchronous between
  // chrome and ash).
  if (details.notification_type == ACCESSIBILITY_TOGGLE_SCREEN_MAGNIFIER ||
      details.notification_type == ACCESSIBILITY_TOGGLE_DICTATION) {
    ash::AccessibilityController::Get()->NotifyAccessibilityStatusChanged();
  }
}

void AccessibilityManager::UpdateChromeOSAccessibilityHistograms() {
  base::UmaHistogramBoolean("Accessibility.CrosSpokenFeedback",
                            IsSpokenFeedbackEnabled());
  base::UmaHistogramBoolean("Accessibility.CrosHighContrast",
                            IsHighContrastEnabled());
  base::UmaHistogramBoolean("Accessibility.CrosVirtualKeyboard",
                            IsVirtualKeyboardEnabled());
  base::UmaHistogramBoolean("Accessibility.CrosStickyKeys",
                            IsStickyKeysEnabled());
  if (MagnificationManager::Get()) {
    base::UmaHistogramBoolean(
        "Accessibility.CrosScreenMagnifier",
        MagnificationManager::Get()->IsMagnifierEnabled());
    base::UmaHistogramBoolean(
        "Accessibility.CrosDockedMagnifier",
        MagnificationManager::Get()->IsDockedMagnifierEnabled());
  }
  if (profile_) {
    const PrefService* const prefs = profile_->GetPrefs();

    bool large_cursor_enabled =
        prefs->GetBoolean(ash::prefs::kAccessibilityLargeCursorEnabled);
    base::UmaHistogramBoolean("Accessibility.CrosLargeCursor",
                              large_cursor_enabled);
    if (large_cursor_enabled) {
      base::UmaHistogramCounts100(
          "Accessibility.CrosLargeCursorSize",
          prefs->GetInteger(ash::prefs::kAccessibilityLargeCursorDipSize));
    }

    base::UmaHistogramBoolean(
        "Accessibility.CrosAlwaysShowA11yMenu",
        prefs->GetBoolean(ash::prefs::kShouldAlwaysShowAccessibilityMenu));

    bool autoclick_enabled =
        prefs->GetBoolean(ash::prefs::kAccessibilityAutoclickEnabled);
    base::UmaHistogramBoolean("Accessibility.CrosAutoclick", autoclick_enabled);
  }
  base::UmaHistogramBoolean("Accessibility.CrosCaretHighlight",
                            IsCaretHighlightEnabled());
  base::UmaHistogramBoolean("Accessibility.CrosCursorHighlight",
                            IsCursorHighlightEnabled());
  base::UmaHistogramBoolean("Accessibility.CrosDictation",
                            IsDictationEnabled());
  base::UmaHistogramBoolean("Accessibility.CrosFocusHighlight",
                            IsFocusHighlightEnabled());
  base::UmaHistogramBoolean("Accessibility.CrosSelectToSpeak",
                            IsSelectToSpeakEnabled());
  base::UmaHistogramBoolean("Accessibility.CrosSwitchAccess",
                            IsSwitchAccessEnabled());
}

void AccessibilityManager::PlayVolumeAdjustSound() {
  if (VolumeAdjustSoundEnabled()) {
    PlayEarcon(chromeos::SOUND_VOLUME_ADJUST,
               chromeos::PlaySoundOption::ONLY_IF_SPOKEN_FEEDBACK_ENABLED);
  }
}

void AccessibilityManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE: {
      // Update |profile_| when entering the login screen.
      Profile* profile = ProfileManager::GetActiveUserProfile();
      if (ProfileHelper::IsSigninProfile(profile))
        SetProfile(profile);
      break;
    }
    case chrome::NOTIFICATION_APP_TERMINATING: {
      app_terminating_ = true;
      break;
    }
    case content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE: {
      // Avoid unnecessary mojo IPC to ash when focus highlight feature is not
      // enabled.
      if (!IsFocusHighlightEnabled())
        return;
      content::FocusedNodeDetails* node_details =
          content::Details<content::FocusedNodeDetails>(details).ptr();
      ash::AccessibilityController::Get()->SetFocusHighlightRect(
          node_details->node_bounds_in_screen);
      break;
    }
  }
}

void AccessibilityManager::OnBrailleDisplayStateChanged(
    const DisplayState& display_state) {
  braille_display_connected_ = display_state.available;
  ash::AccessibilityController::Get()->BrailleDisplayStateChanged(
      braille_display_connected_);
  UpdateBrailleImeState();
}

void AccessibilityManager::OnBrailleKeyEvent(const KeyEvent& event) {
  // Ensure the braille IME is active on braille keyboard (dots) input.
  if ((event.command ==
       extensions::api::braille_display_private::KEY_COMMAND_DOTS) &&
      !braille_ime_current_) {
    input_method::InputMethodManager::Get()
        ->GetActiveIMEState()
        ->ChangeInputMethod(extension_ime_util::kBrailleImeEngineId,
                            false /* show_message */);
  }
}

void AccessibilityManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  if (extension->id() == keyboard_listener_extension_id_)
    keyboard_listener_extension_id_ = std::string();
}

void AccessibilityManager::OnShutdown(extensions::ExtensionRegistry* registry) {
  extension_registry_observer_.Remove(registry);
}

void AccessibilityManager::PostLoadChromeVox() {
  // In browser_tests loading the ChromeVox extension can race with shutdown.
  // http://crbug.com/801700
  if (app_terminating_)
    return;

  // Do any setup work needed immediately after ChromeVox actually loads.
  const std::string& address = GetBluetoothBrailleDisplayAddress();
  if (!address.empty()) {
    // Maybe start brltty, when we have a bluetooth device stored for
    // connection.
    RestartBrltty(address);
  } else {
    // Otherwise, start brltty without an address. This covers cases when
    // ChromeVox is toggled off then back on all while a usb braille display is
    // connected.
    chromeos::UpstartClient::Get()->StartJob(kBrlttyUpstartJobName, {},
                                             EmptyVoidDBusMethodCallback());
  }

  PlayEarcon(SOUND_SPOKEN_FEEDBACK_ENABLED, PlaySoundOption::ALWAYS);

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);

  const std::string& extension_id = extension_misc::kChromeVoxExtensionId;

  std::unique_ptr<base::ListValue> event_args =
      std::make_unique<base::ListValue>();
  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_INTRODUCE_CHROME_VOX,
      extensions::api::accessibility_private::OnIntroduceChromeVox::kEventName,
      std::move(event_args)));
  event_router->DispatchEventWithLazyListener(extension_id, std::move(event));

  if (!chromevox_panel_) {
    chromevox_panel_ = new ChromeVoxPanel(profile_);
    chromevox_panel_widget_observer_.reset(new AccessibilityPanelWidgetObserver(
        chromevox_panel_->GetWidget(),
        base::BindOnce(&AccessibilityManager::OnChromeVoxPanelDestroying,
                       base::Unretained(this))));
  }

  audio_focus_manager_->SetEnforcementMode(
      media_session::mojom::EnforcementMode::kNone);

  InitializeFocusRings(extension_id);
}

void AccessibilityManager::PostUnloadChromeVox() {
  // Do any teardown work needed immediately after ChromeVox actually unloads.
  // Stop brltty.
  chromeos::UpstartClient::Get()->StopJob(kBrlttyUpstartJobName, {},
                                          EmptyVoidDBusMethodCallback());

  PlayEarcon(SOUND_SPOKEN_FEEDBACK_DISABLED, PlaySoundOption::ALWAYS);

  RemoveFocusRings(extension_misc::kChromeVoxExtensionId);

  if (chromevox_panel_) {
    chromevox_panel_->Close();
    chromevox_panel_ = nullptr;
  }

  // In case the user darkened the screen, undarken it now.
  SetDarkenScreen(false);

  // Stop speech.
  content::TtsController::GetInstance()->Stop();

  audio_focus_manager_->SetEnforcementMode(
      media_session::mojom::EnforcementMode::kDefault);
}

void AccessibilityManager::PostSwitchChromeVoxProfile() {
  if (chromevox_panel_) {
    chromevox_panel_->CloseNow();
    chromevox_panel_ = nullptr;
  }
  chromevox_panel_ = new ChromeVoxPanel(profile_);
  chromevox_panel_widget_observer_.reset(new AccessibilityPanelWidgetObserver(
      chromevox_panel_->GetWidget(),
      base::BindOnce(&AccessibilityManager::OnChromeVoxPanelDestroying,
                     base::Unretained(this))));
}

void AccessibilityManager::OnChromeVoxPanelDestroying() {
  chromevox_panel_widget_observer_.reset(nullptr);
  chromevox_panel_ = nullptr;
}

void AccessibilityManager::PostLoadSelectToSpeak() {
  InitializeFocusRings(extension_misc::kSelectToSpeakExtensionId);
}

void AccessibilityManager::PostUnloadSelectToSpeak() {
  // Do any teardown work needed immediately after Select-to-Speak actually
  // unloads.

  // Clear the accessibility focus ring and highlight.
  RemoveFocusRings(extension_misc::kSelectToSpeakExtensionId);
  HideHighlights();

  // Stop speech.
  content::TtsController::GetInstance()->Stop();
}

void AccessibilityManager::PostLoadSwitchAccess() {
  if (!switch_access_panel_) {
    switch_access_panel_ = new SwitchAccessPanel(profile_);
    switch_access_panel_widget_observer_.reset(
        new AccessibilityPanelWidgetObserver(
            switch_access_panel_->GetWidget(),
            base::BindOnce(&AccessibilityManager::OnSwitchAccessPanelDestroying,
                           base::Unretained(this))));
  }

  InitializeFocusRings(extension_misc::kSwitchAccessExtensionId);
}

void AccessibilityManager::PostUnloadSwitchAccess() {
  // Do any teardown work needed immediately after SwitchAccess actually
  // unloads.

  // Clear the accessibility focus ring.
  RemoveFocusRings(extension_misc::kSwitchAccessExtensionId);

  // Close the context menu
  if (switch_access_panel_) {
    switch_access_panel_->Close();
    switch_access_panel_ = nullptr;
  }
}

void AccessibilityManager::OnSwitchAccessPanelDestroying() {
  switch_access_panel_widget_observer_.reset(nullptr);
  switch_access_panel_ = nullptr;
}

void AccessibilityManager::PostLoadAutoclick() {
  InitializeFocusRings(extension_misc::kAutoclickExtensionId);
}

void AccessibilityManager::PostUnloadAutoclick() {
  // Clear the accessibility focus ring.
  RemoveFocusRings(extension_misc::kAutoclickExtensionId);
}

void AccessibilityManager::SetKeyboardListenerExtensionId(
    const std::string& id,
    content::BrowserContext* context) {
  keyboard_listener_extension_id_ = id;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(context);
  if (!extension_registry_observer_.IsObserving(registry) && !id.empty())
    extension_registry_observer_.Add(registry);
}

void AccessibilityManager::HideSwitchAccessMenu() {
  switch_access_panel_->Hide();
}

void AccessibilityManager::ShowSwitchAccessMenu(const gfx::Rect& element_bounds,
                                                int menu_width,
                                                int menu_height,
                                                bool back_button_only) {
  switch_access_panel_->Show(element_bounds, menu_width, menu_height,
                             back_button_only);
}

bool AccessibilityManager::ToggleDictation() {
  if (!profile_)
    return false;

  if (!dictation_.get())
    dictation_ = std::make_unique<DictationChromeos>(profile_);

  return dictation_->OnToggleDictation();
}

const std::string AccessibilityManager::GetFocusRingId(
    const std::string& extension_id,
    const std::string& focus_ring_name) {
  // Add the focus ring name to the list of focus rings for the extension.
  focus_ring_names_for_extension_id_.find(extension_id)
      ->second.insert(focus_ring_name);
  return extension_id + '-' + focus_ring_name;
}

void AccessibilityManager::InitializeFocusRings(
    const std::string& extension_id) {
  if (focus_ring_names_for_extension_id_.count(extension_id) == 0) {
    focus_ring_names_for_extension_id_.emplace(extension_id,
                                               std::set<std::string>());
  }
}

void AccessibilityManager::RemoveFocusRings(const std::string& extension_id) {
  if (focus_ring_names_for_extension_id_.count(extension_id) != 0) {
    const std::set<std::string>& focus_ring_names =
        focus_ring_names_for_extension_id_.find(extension_id)->second;

    for (const std::string& focus_ring_name : focus_ring_names)
      HideFocusRing(GetFocusRingId(extension_id, focus_ring_name));
  }
  focus_ring_names_for_extension_id_.erase(extension_id);
}

void AccessibilityManager::SetFocusRing(
    std::string focus_ring_id,
    std::unique_ptr<ash::AccessibilityFocusRingInfo> focus_ring) {
  ash::AccessibilityFocusRingController::Get()->SetFocusRing(
      focus_ring_id, std::move(focus_ring));

  if (focus_ring_observer_for_test_)
    focus_ring_observer_for_test_.Run();
}

void AccessibilityManager::HideFocusRing(std::string focus_ring_id) {
  ash::AccessibilityFocusRingController::Get()->HideFocusRing(focus_ring_id);
  if (focus_ring_observer_for_test_)
    focus_ring_observer_for_test_.Run();
}

void AccessibilityManager::SetHighlights(
    const std::vector<gfx::Rect>& rects_in_screen,
    SkColor color) {
  ash::AccessibilityFocusRingController::Get()->SetHighlights(rects_in_screen,
                                                              color);
}

void AccessibilityManager::HideHighlights() {
  ash::AccessibilityFocusRingController::Get()->HideHighlights();
}

void AccessibilityManager::SetCaretBounds(const gfx::Rect& bounds_in_screen) {
  // For efficiency only send mojo IPCs to ash if the highlight is enabled.
  if (!IsCaretHighlightEnabled())
    return;

  ash::AccessibilityController::Get()->SetCaretBounds(bounds_in_screen);

  if (caret_bounds_observer_for_test_)
    caret_bounds_observer_for_test_.Run(bounds_in_screen);
}

bool AccessibilityManager::GetStartupSoundEnabled() const {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::UserList& user_list = user_manager->GetUsers();
  if (user_list.empty())
    return false;

  // |user_list| is sorted by last log in date. Take the most recent user to
  // log in.
  bool val;
  return user_manager::known_user::GetBooleanPref(
             user_list[0]->GetAccountId(), kUserStartupSoundEnabled, &val) &&
         val;
}

void AccessibilityManager::SetStartupSoundEnabled(bool value) const {
  if (!profile_)
    return;

  user_manager::known_user::SetBooleanPref(
      multi_user_util::GetAccountIdFromProfile(profile_),
      kUserStartupSoundEnabled, value);
}

const std::string AccessibilityManager::GetBluetoothBrailleDisplayAddress()
    const {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::UserList& user_list = user_manager->GetUsers();
  if (user_list.empty())
    return std::string();

  // |user_list| is sorted by last log in date. Take the most recent user to
  // log in.
  std::string val;
  return user_manager::known_user::GetStringPref(
             user_list[0]->GetAccountId(), kUserBluetoothBrailleDisplayAddress,
             &val)
             ? val
             : std::string();
}

void AccessibilityManager::UpdateBluetoothBrailleDisplayAddress(
    const std::string& address) {
  CHECK(spoken_feedback_enabled_);
  if (!profile_)
    return;

  user_manager::known_user::SetStringPref(
      multi_user_util::GetAccountIdFromProfile(profile_),
      kUserBluetoothBrailleDisplayAddress, address);
  RestartBrltty(address);
}

void AccessibilityManager::SetProfileForTest(Profile* profile) {
  SetProfile(profile);
}

// static
void AccessibilityManager::SetBrailleControllerForTest(
    BrailleController* controller) {
  g_braille_controller_for_test = controller;
}

void AccessibilityManager::SetFocusRingObserverForTest(
    base::RepeatingCallback<void()> observer) {
  focus_ring_observer_for_test_ = observer;
}

void AccessibilityManager::SetSelectToSpeakStateObserverForTest(
    base::RepeatingCallback<void()> observer) {
  select_to_speak_state_observer_for_test_ = observer;
}

void AccessibilityManager::SetCaretBoundsObserverForTest(
    base::RepeatingCallback<void(const gfx::Rect&)> observer) {
  caret_bounds_observer_for_test_ = observer;
}

void AccessibilityManager::SetSwitchAccessKeysForTest(
    const std::vector<int>& keys) {
  // Sets all keys to "Select" command.
  ListPrefUpdate update(profile_->GetPrefs(),
                        ash::prefs::kAccessibilitySwitchAccessSelectKeyCodes);
  for (int key : keys)
    update->AppendInteger(key);

  profile_->GetPrefs()->CommitPendingWrite();
}

}  // namespace chromeos

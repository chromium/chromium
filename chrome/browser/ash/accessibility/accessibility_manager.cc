// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "ash/accessibility/sticky_keys/sticky_keys_controller.h"
#include "ash/components/audio/sounds.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/accessibility_controller.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/accessibility_focus_ring_controller.h"
#include "ash/public/cpp/accessibility_focus_ring_info.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/accessibility/accessibility_extension_api.h"
#include "chrome/browser/accessibility/soda_installer.h"
#include "chrome/browser/ash/accessibility/accessibility_extension_loader.h"
#include "chrome/browser/ash/accessibility/dictation.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/accessibility/select_to_speak_event_handler_delegate_impl.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_requisition_manager.h"
#include "chrome/browser/extensions/api/braille_display_private/stub_braille_controller.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
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
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_resource.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

namespace ash {
namespace {

namespace extension_ime_util = ::chromeos::extension_ime_util;
namespace input_method = ::chromeos::input_method;
using ::extensions::api::braille_display_private::BrailleController;
using ::extensions::api::braille_display_private::DisplayState;
using ::extensions::api::braille_display_private::KeyEvent;
using ::extensions::api::braille_display_private::StubBrailleController;

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

static AccessibilityManager* g_accessibility_manager = nullptr;

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

// Restarts (stops, then starts brltty). If |address| is empty, only stops.
// In Upstart, sending an explicit restart command is a no-op if the job isn't
// already started. Without knowledge regarding brltty's current job status,
// stop followed by start ensures we both stop a started job, and also start
// brltty.
void RestartBrltty(const std::string& address) {
  chromeos::UpstartClient* client = chromeos::UpstartClient::Get();
  client->StopJob(kBrlttyUpstartJobName, {}, base::DoNothing());

  std::vector<std::string> args;
  if (address.empty())
    return;

  args.push_back(base::StringPrintf("ADDRESS=%s", address.c_str()));
  client->StartJob(kBrlttyUpstartJobName, args, base::DoNothing());
}

bool VolumeAdjustSoundEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableVolumeAdjustSound);
}

std::string AccessibilityPrivateEnumForAction(SelectToSpeakPanelAction action) {
  switch (action) {
    case SelectToSpeakPanelAction::kPreviousSentence:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::
              SELECT_TO_SPEAK_PANEL_ACTION_PREVIOUSSENTENCE);
    case SelectToSpeakPanelAction::kPreviousParagraph:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::
              SELECT_TO_SPEAK_PANEL_ACTION_PREVIOUSPARAGRAPH);
    case SelectToSpeakPanelAction::kPause:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::
              SELECT_TO_SPEAK_PANEL_ACTION_PAUSE);
    case SelectToSpeakPanelAction::kResume:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::
              SELECT_TO_SPEAK_PANEL_ACTION_RESUME);
    case SelectToSpeakPanelAction::kNextSentence:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::
              SELECT_TO_SPEAK_PANEL_ACTION_NEXTSENTENCE);
    case SelectToSpeakPanelAction::kNextParagraph:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::
              SELECT_TO_SPEAK_PANEL_ACTION_NEXTPARAGRAPH);
    case SelectToSpeakPanelAction::kChangeSpeed:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::
              SELECT_TO_SPEAK_PANEL_ACTION_CHANGESPEED);
    case SelectToSpeakPanelAction::kExit:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::
              SELECT_TO_SPEAK_PANEL_ACTION_EXIT);
    case SelectToSpeakPanelAction::kNone:
      NOTREACHED();
      return "";
  }
}

}  // namespace

class AccessibilityPanelWidgetObserver : public views::WidgetObserver {
 public:
  AccessibilityPanelWidgetObserver(views::Widget* widget,
                                   base::OnceCallback<void()> on_destroying)
      : widget_(widget), on_destroying_(std::move(on_destroying)) {
    widget_->AddObserver(this);
  }

  ~AccessibilityPanelWidgetObserver() override { CHECK(!IsInObserverList()); }

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

  focus_changed_subscription_ =
      content::BrowserAccessibilityState::GetInstance()
          ->RegisterFocusChangedCallback(
              base::BindRepeating(&AccessibilityManager::OnFocusChangedInPage,
                                  base::Unretained(this)));

  input_method::InputMethodManager::Get()->AddObserver(this);
  user_manager::UserManager::Get()->AddSessionStateObserver(this);

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  audio::SoundsManager* manager = audio::SoundsManager::Get();
  manager->Initialize(static_cast<int>(Sound::kShutdown),
                      bundle.GetRawDataResource(IDR_SOUND_SHUTDOWN_WAV));
  manager->Initialize(
      static_cast<int>(Sound::kSpokenFeedbackEnabled),
      bundle.GetRawDataResource(IDR_SOUND_SPOKEN_FEEDBACK_ENABLED_WAV));
  manager->Initialize(
      static_cast<int>(Sound::kSpokenFeedbackDisabled),
      bundle.GetRawDataResource(IDR_SOUND_SPOKEN_FEEDBACK_DISABLED_WAV));
  manager->Initialize(static_cast<int>(Sound::kPassthrough),
                      bundle.GetRawDataResource(IDR_SOUND_PASSTHROUGH_WAV));
  manager->Initialize(static_cast<int>(Sound::kExitScreen),
                      bundle.GetRawDataResource(IDR_SOUND_EXIT_SCREEN_WAV));
  manager->Initialize(static_cast<int>(Sound::kEnterScreen),
                      bundle.GetRawDataResource(IDR_SOUND_ENTER_SCREEN_WAV));
  manager->Initialize(
      static_cast<int>(Sound::kSpokenFeedbackToggleCountdownHigh),
      bundle.GetRawDataResource(
          IDR_SOUND_SPOKEN_FEEDBACK_TOGGLE_COUNTDOWN_HIGH_WAV));
  manager->Initialize(
      static_cast<int>(Sound::kSpokenFeedbackToggleCountdownLow),
      bundle.GetRawDataResource(
          IDR_SOUND_SPOKEN_FEEDBACK_TOGGLE_COUNTDOWN_LOW_WAV));
  manager->Initialize(static_cast<int>(Sound::kTouchType),
                      bundle.GetRawDataResource(IDR_SOUND_TOUCH_TYPE_WAV));
  manager->Initialize(static_cast<int>(Sound::kDictationEnd),
                      bundle.GetRawDataResource(IDR_SOUND_DICTATION_END_WAV));
  manager->Initialize(static_cast<int>(Sound::kDictationStart),
                      bundle.GetRawDataResource(IDR_SOUND_DICTATION_START_WAV));
  manager->Initialize(
      static_cast<int>(Sound::kDictationCancel),
      bundle.GetRawDataResource(IDR_SOUND_DICTATION_CANCEL_WAV));
  manager->Initialize(static_cast<int>(Sound::kStartup),
                      bundle.GetRawDataResource(IDR_SOUND_STARTUP_WAV));

  if (VolumeAdjustSoundEnabled()) {
    manager->Initialize(static_cast<int>(Sound::kVolumeAdjust),
                        bundle.GetRawDataResource(IDR_SOUND_VOLUME_ADJUST_WAV));
  }

  base::FilePath resources_path;
  if (!base::PathService::Get(chrome::DIR_RESOURCES, &resources_path))
    NOTREACHED();
  accessibility_common_extension_loader_ =
      base::WrapUnique(new AccessibilityExtensionLoader(
          extension_misc::kAccessibilityCommonExtensionId,
          resources_path.Append(
              extension_misc::kAccessibilityCommonExtensionPath),
          extension_misc::kAccessibilityCommonManifestFilename,
          extension_misc::kAccessibilityCommonGuestManifestFilename,
          base::BindRepeating(
              &AccessibilityManager::PostUnloadAccessibilityCommon,
              weak_ptr_factory_.GetWeakPtr())));
  chromevox_loader_ = base::WrapUnique(new AccessibilityExtensionLoader(
      extension_misc::kChromeVoxExtensionId,
      resources_path.Append(extension_misc::kChromeVoxExtensionPath),
      extension_misc::kChromeVoxManifestFilename,
      extension_misc::kChromeVoxGuestManifestFilename,
      base::BindRepeating(&AccessibilityManager::PostUnloadChromeVox,
                          weak_ptr_factory_.GetWeakPtr())));
  select_to_speak_loader_ = base::WrapUnique(new AccessibilityExtensionLoader(
      extension_misc::kSelectToSpeakExtensionId,
      resources_path.Append(extension_misc::kSelectToSpeakExtensionPath),
      extension_misc::kSelectToSpeakManifestFilename,
      extension_misc::kSelectToSpeakGuestManifestFilename,
      base::BindRepeating(&AccessibilityManager::PostUnloadSelectToSpeak,
                          weak_ptr_factory_.GetWeakPtr())));
  switch_access_loader_ = base::WrapUnique(new AccessibilityExtensionLoader(
      extension_misc::kSwitchAccessExtensionId,
      resources_path.Append(extension_misc::kSwitchAccessExtensionPath),
      extension_misc::kSwitchAccessManifestFilename,
      extension_misc::kSwitchAccessGuestManifestFilename,
      base::BindRepeating(&AccessibilityManager::PostUnloadSwitchAccess,
                          weak_ptr_factory_.GetWeakPtr())));

  // Connect to the media session service.
  content::GetMediaSessionService().BindAudioFocusManager(
      audio_focus_manager_.BindNewPipeAndPassReceiver());

  AcceleratorController::SetVolumeAdjustmentSoundCallback(base::BindRepeating(
      &AccessibilityManager::PlayVolumeAdjustSound, base::Unretained(this)));

  CrasAudioHandler::Get()->AddAudioObserver(this);
}

AccessibilityManager::~AccessibilityManager() {
  CHECK(this == g_accessibility_manager);
  AccessibilityStatusEventDetails details(
      AccessibilityNotificationType::kManagerShutdown, false);
  NotifyAccessibilityStatusChanged(details);
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
  input_method::InputMethodManager::Get()->RemoveObserver(this);

  if (chromevox_panel_) {
    chromevox_panel_->CloseNow();
    chromevox_panel_ = nullptr;
  }

  AcceleratorController::SetVolumeAdjustmentSoundCallback({});
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
    if (prefs->GetBoolean(prefs::kAccessibilityStickyKeysEnabled) ||
        prefs->GetBoolean(prefs::kAccessibilityLargeCursorEnabled) ||
        prefs->GetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled) ||
        prefs->GetBoolean(prefs::kAccessibilitySelectToSpeakEnabled) ||
        prefs->GetBoolean(prefs::kAccessibilitySwitchAccessEnabled) ||
        prefs->GetBoolean(prefs::kAccessibilityHighContrastEnabled) ||
        prefs->GetBoolean(prefs::kAccessibilityAutoclickEnabled) ||
        prefs->GetBoolean(prefs::kShouldAlwaysShowAccessibilityMenu) ||
        prefs->GetBoolean(prefs::kAccessibilityScreenMagnifierEnabled) ||
        prefs->GetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled) ||
        prefs->GetBoolean(prefs::kAccessibilityMonoAudioEnabled) ||
        prefs->GetBoolean(prefs::kAccessibilityCaretHighlightEnabled) ||
        prefs->GetBoolean(prefs::kAccessibilityCursorHighlightEnabled) ||
        prefs->GetBoolean(prefs::kAccessibilityFocusHighlightEnabled) ||
        prefs->GetBoolean(prefs::kAccessibilityDictationEnabled) ||
        prefs->GetBoolean(prefs::kDockedMagnifierEnabled)) {
      return true;
    }
  }
  return false;
}

void AccessibilityManager::UpdateAlwaysShowMenuFromPref() {
  if (!profile_)
    return;

  // Update system tray menu visibility.
  AccessibilityController::Get()->NotifyAccessibilityStatusChanged();
}

void AccessibilityManager::EnableLargeCursor(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, enabled);
  pref_service->CommitPendingWrite();
}

void AccessibilityManager::OnLargeCursorChanged() {
  AccessibilityStatusEventDetails details(
      AccessibilityNotificationType::kToggleLargeCursor,
      IsLargeCursorEnabled());
  NotifyAccessibilityStatusChanged(details);
}

bool AccessibilityManager::IsLargeCursorEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         prefs::kAccessibilityLargeCursorEnabled);
}

void AccessibilityManager::EnableStickyKeys(bool enabled) {
  if (!profile_)
    return;
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilityStickyKeysEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsStickyKeysEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         prefs::kAccessibilityStickyKeysEnabled);
}

void AccessibilityManager::OnStickyKeysChanged() {
  AccessibilityStatusEventDetails details(
      AccessibilityNotificationType::kToggleStickyKeys, IsStickyKeysEnabled());
  NotifyAccessibilityStatusChanged(details);
}

void AccessibilityManager::EnableSpokenFeedback(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled, enabled);
  pref_service->CommitPendingWrite();
}

void AccessibilityManager::OnSpokenFeedbackChanged() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      prefs::kAccessibilitySpokenFeedbackEnabled);

  if (ProfileHelper::IsRegularProfile(profile_)) {
    user_manager::known_user::SetBooleanPref(
        multi_user_util::GetAccountIdFromProfile(profile_),
        kUserSpokenFeedbackEnabled, enabled);
  }

  if (enabled) {
    chromevox_loader_->SetProfile(
        profile_,
        base::BindRepeating(&AccessibilityManager::PostSwitchChromeVoxProfile,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  if (spoken_feedback_enabled_ == enabled)
    return;

  spoken_feedback_enabled_ = enabled;

  AccessibilityStatusEventDetails details(
      AccessibilityNotificationType::kToggleSpokenFeedback, enabled);
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
                         prefs::kAccessibilitySpokenFeedbackEnabled);
}

void AccessibilityManager::EnableHighContrast(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilityHighContrastEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsHighContrastEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         prefs::kAccessibilityHighContrastEnabled);
}

void AccessibilityManager::OnHighContrastChanged() {
  AccessibilityStatusEventDetails details(
      AccessibilityNotificationType::kToggleHighContrastMode,
      IsHighContrastEnabled());
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

void AccessibilityManager::OnViewFocusedInArc(const gfx::Rect& bounds_in_screen,
                                              bool is_editable) {
  AccessibilityController::Get()->SetFocusHighlightRect(bounds_in_screen);

  MagnificationManager* magnification_manager = MagnificationManager::Get();
  if (magnification_manager)
    magnification_manager->HandleFocusedRectChangedIfEnabled(bounds_in_screen,
                                                             is_editable);
}

bool AccessibilityManager::PlayEarcon(Sound sound_key, PlaySoundOption option) {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(kAshDisableSystemSounds))
    return false;
  if (option == PlaySoundOption::kOnlyIfSpokenFeedbackEnabled &&
      !IsSpokenFeedbackEnabled()) {
    return false;
  }
  return audio::SoundsManager::Get()->Play(static_cast<int>(sound_key));
}

void AccessibilityManager::OnTwoFingerTouchStart() {
  if (!profile_)
    return;

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);

  auto event_args = std::make_unique<base::ListValue>();
  auto event = std::make_unique<extensions::Event>(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_TWO_FINGER_TOUCH_START,
      extensions::api::accessibility_private::OnTwoFingerTouchStart::kEventName,
      std::move(event_args));
  event_router->BroadcastEvent(std::move(event));
}

void AccessibilityManager::OnTwoFingerTouchStop() {
  if (!profile_)
    return;

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);

  auto event_args = std::make_unique<base::ListValue>();
  auto event = std::make_unique<extensions::Event>(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_TWO_FINGER_TOUCH_STOP,
      extensions::api::accessibility_private::OnTwoFingerTouchStop::kEventName,
      std::move(event_args));
  event_router->BroadcastEvent(std::move(event));
}

bool AccessibilityManager::ShouldToggleSpokenFeedbackViaTouch() {
  return false;
}

bool AccessibilityManager::PlaySpokenFeedbackToggleCountdown(int tick_count) {
  return audio::SoundsManager::Get()->Play(
      tick_count % 2
          ? static_cast<int>(Sound::kSpokenFeedbackToggleCountdownHigh)
          : static_cast<int>(Sound::kSpokenFeedbackToggleCountdownLow));
}

void AccessibilityManager::HandleAccessibilityGesture(
    ax::mojom::Gesture gesture,
    gfx::PointF location) {
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);

  std::unique_ptr<base::ListValue> event_args =
      std::make_unique<base::ListValue>();
  event_args->AppendString(ui::ToString(gesture));
  event_args->AppendInteger(location.x());
  event_args->AppendInteger(location.y());
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
  for (auto* rwc : RootWindowController::root_window_controllers())
    rwc->SetTouchAccessibilityAnchorPoint(anchor_point);
}

void AccessibilityManager::EnableAutoclick(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilityAutoclickEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsAutoclickEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         prefs::kAccessibilityAutoclickEnabled);
}

void AccessibilityManager::OnAccessibilityCommonChanged(
    const std::string& pref_name) {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(pref_name);
  if (enabled) {
    accessibility_common_extension_loader_->SetProfile(
        profile_, base::OnceClosure() /* done_callback */);
  }

  size_t pref_count = accessibility_common_enabled_features_.count(pref_name);
  if ((pref_count != 0 && enabled) || (pref_count == 0 && !enabled))
    return;

  if (pref_name == ash::prefs::kAccessibilityDictationEnabled &&
      !::switches::IsExperimentalAccessibilityDictationExtensionEnabled()) {
    return;
  }

  if (enabled) {
    accessibility_common_enabled_features_.insert(pref_name);
    if (!accessibility_common_extension_loader_->loaded()) {
      accessibility_common_extension_loader_->Load(
          profile_, base::BindRepeating(
                        &AccessibilityManager::PostLoadAccessibilityCommon,
                        weak_ptr_factory_.GetWeakPtr()));
    } else {
      // It's already loaded. Just run the callback.
      PostLoadAccessibilityCommon();
    }
  } else {
    accessibility_common_enabled_features_.erase(pref_name);

    if (accessibility_common_enabled_features_.empty()) {
      accessibility_common_extension_loader_->Unload();
    }
  }
}

void AccessibilityManager::RequestAutoclickScrollableBoundsForPoint(
    gfx::Point& point_in_screen) {
  if (!profile_)
    return;

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);
  std::unique_ptr<base::ListValue> event_args = extensions::api::
      accessibility_private::OnScrollableBoundsForPointRequested::Create(
          point_in_screen.x(), point_in_screen.y());
  std::unique_ptr<extensions::Event> event =
      std::make_unique<extensions::Event>(
          extensions::events::
              ACCESSIBILITY_PRIVATE_FIND_SCROLLABLE_BOUNDS_FOR_POINT,
          extensions::api::accessibility_private::
              OnScrollableBoundsForPointRequested::kEventName,
          std::move(event_args));
  event_router->DispatchEventWithLazyListener(
      extension_misc::kAccessibilityCommonExtensionId, std::move(event));
}

void AccessibilityManager::MagnifierBoundsChanged(
    const gfx::Rect& bounds_in_screen) {
  if (!profile_)
    return;

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);

  auto magnifier_bounds =
      std::make_unique<extensions::api::accessibility_private::ScreenRect>();
  magnifier_bounds->left = bounds_in_screen.x();
  magnifier_bounds->top = bounds_in_screen.y();
  magnifier_bounds->width = bounds_in_screen.width();
  magnifier_bounds->height = bounds_in_screen.height();

  auto event_args =
      extensions::api::accessibility_private::OnMagnifierBoundsChanged::Create(
          *magnifier_bounds.get());

  auto event = std::make_unique<extensions::Event>(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_MAGNIFIER_BOUNDS_CHANGED,
      extensions::api::accessibility_private::OnMagnifierBoundsChanged::
          kEventName,
      std::move(event_args));

  event_router->DispatchEventWithLazyListener(
      extension_misc::kAccessibilityCommonExtensionId, std::move(event));
}

void AccessibilityManager::EnableVirtualKeyboard(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilityVirtualKeyboardEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsVirtualKeyboardEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         prefs::kAccessibilityVirtualKeyboardEnabled);
}

void AccessibilityManager::OnVirtualKeyboardChanged() {
  AccessibilityStatusEventDetails details(
      AccessibilityNotificationType::kToggleVirtualKeyboard,
      IsVirtualKeyboardEnabled());
  NotifyAccessibilityStatusChanged(details);
}

void AccessibilityManager::EnableMonoAudio(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilityMonoAudioEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsMonoAudioEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         prefs::kAccessibilityMonoAudioEnabled);
}

void AccessibilityManager::OnMonoAudioChanged() {
  AccessibilityStatusEventDetails details(
      AccessibilityNotificationType::kToggleMonoAudio, IsMonoAudioEnabled());
  NotifyAccessibilityStatusChanged(details);
}

void AccessibilityManager::SetDarkenScreen(bool darken) {
  AccessibilityController::Get()->SetDarkenScreen(darken);
}

void AccessibilityManager::SetCaretHighlightEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilityCaretHighlightEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsCaretHighlightEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         prefs::kAccessibilityCaretHighlightEnabled);
}

void AccessibilityManager::OnCaretHighlightChanged() {
  AccessibilityStatusEventDetails details(
      AccessibilityNotificationType::kToggleCaretHighlight,
      IsCaretHighlightEnabled());
  NotifyAccessibilityStatusChanged(details);
}

void AccessibilityManager::SetCursorHighlightEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilityCursorHighlightEnabled,
                           enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsCursorHighlightEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         prefs::kAccessibilityCursorHighlightEnabled);
}

void AccessibilityManager::OnCursorHighlightChanged() {
  AccessibilityStatusEventDetails details(
      AccessibilityNotificationType::kToggleCursorHighlight,
      IsCursorHighlightEnabled());
  NotifyAccessibilityStatusChanged(details);
}

void AccessibilityManager::SetDictationEnabled(bool enabled) const {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(ash::prefs::kAccessibilityDictationEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsDictationEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         prefs::kAccessibilityDictationEnabled);
}

void AccessibilityManager::OnDictationChanged() {
  OnAccessibilityCommonChanged(prefs::kAccessibilityDictationEnabled);
  if (!profile_)
    return;

  // Only need to check SODA installation if offline dictation is enabled.
  if (!::switches::IsExperimentalAccessibilityDictationOfflineEnabled())
    return;

  // Note: OnDictationChanged should not be called at start-up or it will
  // push back SODA deletion each time start-up occurs with dictation disabled.
  const bool enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kAccessibilityDictationEnabled);
  if (!enabled) {
    speech::SodaInstaller::GetInstance()->SetUninstallTimer(
        profile_->GetPrefs(), g_browser_process->local_state());
  }
  // TODO(crbug.com/1173135): Initialize SODA download when enabled.
}

void AccessibilityManager::SetFocusHighlightEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilityFocusHighlightEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsFocusHighlightEnabled() const {
  return profile_ && profile_->GetPrefs()->GetBoolean(
                         prefs::kAccessibilityFocusHighlightEnabled);
}

void AccessibilityManager::OnFocusHighlightChanged() {
  bool enabled = IsFocusHighlightEnabled();

  // Focus highlighting can't be on when spoken feedback is on, because
  // ChromeVox does its own focus highlighting.
  if (IsSpokenFeedbackEnabled())
    enabled = false;
  AccessibilityStatusEventDetails details(
      AccessibilityNotificationType::kToggleFocusHighlight, enabled);
  NotifyAccessibilityStatusChanged(details);

  // TODO(crbug.com/1096759): Load or unload the AccessibilityCommon extension
  // which will be used to get the currently focused view. This will enable us
  // to get views where focus is only represented in the accessibility tree,
  // like those set by aria-activedescendant. It will also possibly eliminate
  // the need for OnViewFocusedInArc.
}

void AccessibilityManager::SetSelectToSpeakEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilitySelectToSpeakEnabled, enabled);
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

void AccessibilityManager::SetSelectToSpeakState(SelectToSpeakState state) {
  AccessibilityController::Get()->SetSelectToSpeakState(state);

  if (select_to_speak_state_observer_for_test_)
    select_to_speak_state_observer_for_test_.Run();
}

void AccessibilityManager::OnSelectToSpeakChanged() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      prefs::kAccessibilitySelectToSpeakEnabled);
  if (enabled)
    select_to_speak_loader_->SetProfile(profile_, base::OnceClosure());

  if (select_to_speak_enabled_ == enabled)
    return;

  select_to_speak_enabled_ = enabled;

  AccessibilityStatusEventDetails details(
      AccessibilityNotificationType::kToggleSelectToSpeak, enabled);
  NotifyAccessibilityStatusChanged(details);

  if (enabled) {
    select_to_speak_loader_->Load(
        profile_,
        base::BindRepeating(&AccessibilityManager::PostLoadSelectToSpeak,
                            weak_ptr_factory_.GetWeakPtr()));
    // Construct a delegate to connect SelectToSpeak and its EventHandler in
    // ash.
    select_to_speak_event_handler_delegate_ =
        std::make_unique<SelectToSpeakEventHandlerDelegateImpl>();
  } else {
    select_to_speak_loader_->Unload();
    select_to_speak_event_handler_delegate_.reset();
  }
}

void AccessibilityManager::SetSwitchAccessEnabled(bool enabled) {
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kAccessibilitySwitchAccessEnabled, enabled);
  pref_service->CommitPendingWrite();
}

bool AccessibilityManager::IsSwitchAccessEnabled() const {
  return switch_access_enabled_;
}

void AccessibilityManager::OnSwitchAccessChanged() {
  if (!profile_)
    return;

  const bool enabled = profile_->GetPrefs()->GetBoolean(
      prefs::kAccessibilitySwitchAccessEnabled);

  if (enabled) {
    // Only update |was_vk_enabled_before_switch_access_| if the profile
    // changed.
    if (profile_ != switch_access_loader_->profile()) {
      was_vk_enabled_before_switch_access_ =
          ChromeKeyboardControllerClient::Get()->IsEnableFlagSet(
              keyboard::KeyboardEnableFlag::kExtensionEnabled);
    }

    switch_access_loader_->SetProfile(profile_, base::OnceClosure());

    // Make sure we always update the VK state, on every profile transition.
    ChromeKeyboardControllerClient::Get()->SetEnableFlag(
        keyboard::KeyboardEnableFlag::kExtensionEnabled);
  }

  if (switch_access_enabled_ == enabled)
    return;
  switch_access_enabled_ = enabled;

  AccessibilityStatusEventDetails details(
      AccessibilityNotificationType::kToggleSwitchAccess, enabled);
  NotifyAccessibilityStatusChanged(details);

  if (enabled) {
    switch_access_loader_->Load(
        profile_,
        base::BindRepeating(&AccessibilityManager::PostLoadSwitchAccess,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void AccessibilityManager::OnSwitchAccessDisabled() {
  switch_access_loader_->Unload();
}

bool AccessibilityManager::IsBrailleDisplayConnected() const {
  return braille_display_connected_;
}

void AccessibilityManager::CheckBrailleState() {
  BrailleController* braille_controller = GetBrailleController();
  if (!scoped_braille_observer_.IsObserving(braille_controller))
    scoped_braille_observer_.Add(braille_controller);
  content::GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&BrailleController::GetDisplayState,
                     base::Unretained(braille_controller)),
      base::BindOnce(&AccessibilityManager::ReceiveBrailleDisplayState,
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
      pref_service->GetString(::prefs::kLanguagePreloadEngines);
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
  pref_service->SetString(::prefs::kLanguagePreloadEngines,
                          base::JoinString(preload_engines, ","));
  braille_ime_current_ = false;
}

// Overridden from InputMethodManager::Observer.
void AccessibilityManager::InputMethodChanged(
    input_method::InputMethodManager* manager,
    Profile* /* profile */,
    bool show_message) {
  Shell::Get()->sticky_keys_controller()->SetModifiersEnabled(
      manager->IsISOLevel5ShiftUsedByCurrentInputMethod(),
      manager->IsAltGrUsedByCurrentInputMethod());
  const input_method::InputMethodDescriptor descriptor =
      manager->GetActiveIMEState()->GetCurrentInputMethod();
  braille_ime_current_ =
      (descriptor.id() == extension_ime_util::kBrailleImeEngineId);
}

void AccessibilityManager::OnActiveOutputNodeChanged() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kFirstExecAfterBoot))
    return;

  AudioDevice device;
  CrasAudioHandler::Get()->GetPrimaryActiveOutputDevice(&device);
  if (device.type == AudioDeviceType::kOther)
    return;

  CrasAudioHandler::Get()->RemoveAudioObserver(this);
  if (GetStartupSoundEnabled()) {
    PlayEarcon(Sound::kStartup, PlaySoundOption::kAlways);
    return;
  }

  const auto& account_ids = user_manager::known_user::GetKnownAccountIds();
  for (size_t i = 0; i < account_ids.size(); ++i) {
    bool val;
    if (user_manager::known_user::GetBooleanPref(
            account_ids[i], kUserSpokenFeedbackEnabled, &val) &&
        val) {
      PlayEarcon(Sound::kStartup, PlaySoundOption::kAlways);
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

  // All features supported by accessibility common which don't need
  // separate pref change handlers.
  static const char* kAccessibilityCommonFeatures[] = {
      prefs::kAccessibilityAutoclickEnabled,
      prefs::kAccessibilityScreenMagnifierEnabled,
      prefs::kDockedMagnifierEnabled};

  if (profile) {
    // TODO(yoshiki): Move following code to PrefHandler.
    pref_change_registrar_.reset(new PrefChangeRegistrar);
    pref_change_registrar_->Init(profile->GetPrefs());
    pref_change_registrar_->Add(
        prefs::kShouldAlwaysShowAccessibilityMenu,
        base::BindRepeating(&AccessibilityManager::UpdateAlwaysShowMenuFromPref,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityLargeCursorEnabled,
        base::BindRepeating(&AccessibilityManager::OnLargeCursorChanged,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityLargeCursorDipSize,
        base::BindRepeating(&AccessibilityManager::OnLargeCursorChanged,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityStickyKeysEnabled,
        base::BindRepeating(&AccessibilityManager::OnStickyKeysChanged,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilitySpokenFeedbackEnabled,
        base::BindRepeating(&AccessibilityManager::OnSpokenFeedbackChanged,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityHighContrastEnabled,
        base::BindRepeating(&AccessibilityManager::OnHighContrastChanged,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityVirtualKeyboardEnabled,
        base::BindRepeating(&AccessibilityManager::OnVirtualKeyboardChanged,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityMonoAudioEnabled,
        base::BindRepeating(&AccessibilityManager::OnMonoAudioChanged,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityCaretHighlightEnabled,
        base::BindRepeating(&AccessibilityManager::OnCaretHighlightChanged,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityCursorHighlightEnabled,
        base::BindRepeating(&AccessibilityManager::OnCursorHighlightChanged,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityFocusHighlightEnabled,
        base::BindRepeating(&AccessibilityManager::OnFocusHighlightChanged,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilitySelectToSpeakEnabled,
        base::BindRepeating(&AccessibilityManager::OnSelectToSpeakChanged,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilitySwitchAccessEnabled,
        base::BindRepeating(&AccessibilityManager::OnSwitchAccessChanged,
                            base::Unretained(this)));
    pref_change_registrar_->Add(
        prefs::kAccessibilityDictationEnabled,
        base::BindRepeating(&AccessibilityManager::OnDictationChanged,
                            base::Unretained(this)));

    for (const std::string& feature : kAccessibilityCommonFeatures) {
      pref_change_registrar_->Add(
          feature, base::BindRepeating(
                       &AccessibilityManager::OnAccessibilityCommonChanged,
                       base::Unretained(this)));
    }

    local_state_pref_change_registrar_.reset(new PrefChangeRegistrar);
    local_state_pref_change_registrar_->Init(g_browser_process->local_state());
    local_state_pref_change_registrar_->Add(
        language::prefs::kApplicationLocale,
        base::BindRepeating(&AccessibilityManager::OnLocaleChanged,
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

  for (const std::string& feature : kAccessibilityCommonFeatures)
    OnAccessibilityCommonChanged(feature);
  // Dictation is not in kAccessibilityCommonFeatures because it needs to
  // be handled in OnDictationChanged also. OnDictationChanged will call to
  // OnAccessibilityCommonChanged. However, OnDictationChanged shouldn't
  // be called at start-up, so we call OnAccessibilityCommonChanged directly
  // for dictation.
  OnAccessibilityCommonChanged(prefs::kAccessibilityDictationEnabled);
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
  if (!PlayEarcon(Sound::kShutdown,
                  PlaySoundOption::kOnlyIfSpokenFeedbackEnabled)) {
    return base::TimeDelta();
  }
  return audio::SoundsManager::Get()->GetDuration(
      static_cast<int>(Sound::kShutdown));
}

base::CallbackListSubscription AccessibilityManager::RegisterCallback(
    const AccessibilityStatusCallback& cb) {
  return callback_list_.Add(cb);
}

void AccessibilityManager::NotifyAccessibilityStatusChanged(
    const AccessibilityStatusEventDetails& details) {
  callback_list_.Notify(details);

  if (details.notification_type ==
      AccessibilityNotificationType::kToggleDictation) {
    AccessibilityController::Get()->SetDictationActive(details.enabled);
    AccessibilityController::Get()->NotifyAccessibilityStatusChanged();
    return;
  }

  // Update system tray menu visibility. Prefs tracked inside ash handle their
  // own updates to avoid race conditions (pref updates are asynchronous between
  // chrome and ash).
  // TODO(hferreiro): repeated condition
  if (details.notification_type ==
          AccessibilityNotificationType::kToggleScreenMagnifier ||
      details.notification_type ==
          AccessibilityNotificationType::kToggleDictation) {
    AccessibilityController::Get()->NotifyAccessibilityStatusChanged();
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
        prefs->GetBoolean(prefs::kAccessibilityLargeCursorEnabled);
    base::UmaHistogramBoolean("Accessibility.CrosLargeCursor",
                              large_cursor_enabled);
    if (large_cursor_enabled) {
      base::UmaHistogramCounts100(
          "Accessibility.CrosLargeCursorSize",
          prefs->GetInteger(prefs::kAccessibilityLargeCursorDipSize));
    }

    base::UmaHistogramBoolean(
        "Accessibility.CrosAlwaysShowA11yMenu",
        prefs->GetBoolean(prefs::kShouldAlwaysShowAccessibilityMenu));

    bool autoclick_enabled =
        prefs->GetBoolean(prefs::kAccessibilityAutoclickEnabled);
    base::UmaHistogramBoolean("Accessibility.CrosAutoclick", autoclick_enabled);

    base::UmaHistogramBoolean(
        "Accessibility.CrosCursorColor",
        prefs->GetBoolean(prefs::kAccessibilityCursorColorEnabled));
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
    PlayEarcon(Sound::kVolumeAdjust,
               PlaySoundOption::kOnlyIfSpokenFeedbackEnabled);
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
  }
}

void AccessibilityManager::OnFocusChangedInPage(
    const content::FocusedNodeDetails& details) {
  // Avoid unnecessary IPC to ash when focus highlight feature is not enabled.
  if (!IsFocusHighlightEnabled())
    return;

  AccessibilityController::Get()->SetFocusHighlightRect(
      details.node_bounds_in_screen);
}

void AccessibilityManager::OnBrailleDisplayStateChanged(
    const DisplayState& display_state) {
  braille_display_connected_ = display_state.available;
  AccessibilityController::Get()->BrailleDisplayStateChanged(
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

  if (extension->id() == extension_misc::kSwitchAccessExtensionId) {
    extensions::VirtualKeyboardAPI* api =
        extensions::BrowserContextKeyedAPIFactory<
            extensions::VirtualKeyboardAPI>::Get(browser_context);
    DCHECK(api);
    api->delegate()->SetRequestedKeyboardState(
        extensions::api::virtual_keyboard_private::KEYBOARD_STATE_AUTO);
  }
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
                                             base::DoNothing());
  }

  PlayEarcon(Sound::kSpokenFeedbackEnabled, PlaySoundOption::kAlways);

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

  // Force volume slide gesture to be on for Chromebox for Meetings provisioned
  // devices.
  if (policy::EnrollmentRequisitionManager::IsRemoraRequisition())
    AccessibilityController::Get()->EnableChromeVoxVolumeSlideGesture();
}

void AccessibilityManager::PostUnloadChromeVox() {
  // Do any teardown work needed immediately after ChromeVox actually unloads.
  // Stop brltty.
  chromeos::UpstartClient::Get()->StopJob(kBrlttyUpstartJobName, {},
                                          base::DoNothing());

  PlayEarcon(Sound::kSpokenFeedbackDisabled, PlaySoundOption::kAlways);

  RemoveFocusRings(extension_misc::kChromeVoxExtensionId);

  if (chromevox_panel_) {
    chromevox_panel_->Close();
    chromevox_panel_ = nullptr;
  }

  // In case the user darkened the screen, undarken it now.
  SetDarkenScreen(false);

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
}

void AccessibilityManager::PostLoadSwitchAccess() {
  InitializeFocusRings(extension_misc::kSwitchAccessExtensionId);
}

void AccessibilityManager::PostUnloadSwitchAccess() {
  // Do any teardown work needed immediately after Switch Access actually
  // unloads.

  // Clear the accessibility focus ring.
  RemoveFocusRings(extension_misc::kSwitchAccessExtensionId);

  if (!was_vk_enabled_before_switch_access_) {
    ChromeKeyboardControllerClient::Get()->ClearEnableFlag(
        keyboard::KeyboardEnableFlag::kExtensionEnabled);
  } else {
    was_vk_enabled_before_switch_access_ = false;
  }
}

void AccessibilityManager::PostLoadAccessibilityCommon() {
  // Do any setup work needed immediately after the Accessibility Common
  // extension actually loads. This may be used by all features which make
  // use of the Accessibility Common extension.
  InitializeFocusRings(extension_misc::kAccessibilityCommonExtensionId);
}

void AccessibilityManager::PostUnloadAccessibilityCommon() {
  // Do any teardown work needed immediately after the Accessibility Common
  // extension actually unloads. This may be used by all features which make
  // use of the Accessibility Common extension.
  RemoveFocusRings(extension_misc::kAccessibilityCommonExtensionId);
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

bool AccessibilityManager::ToggleDictation() {
  if (!profile_)
    return false;

  if (!dictation_.get())
    dictation_ = std::make_unique<Dictation>(profile_);

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
    std::unique_ptr<AccessibilityFocusRingInfo> focus_ring) {
  AccessibilityFocusRingController::Get()->SetFocusRing(focus_ring_id,
                                                        std::move(focus_ring));

  if (focus_ring_observer_for_test_)
    focus_ring_observer_for_test_.Run();
}

void AccessibilityManager::HideFocusRing(std::string focus_ring_id) {
  AccessibilityFocusRingController::Get()->HideFocusRing(focus_ring_id);
  if (focus_ring_observer_for_test_)
    focus_ring_observer_for_test_.Run();
}

void AccessibilityManager::SetHighlights(
    const std::vector<gfx::Rect>& rects_in_screen,
    SkColor color) {
  AccessibilityFocusRingController::Get()->SetHighlights(rects_in_screen,
                                                         color);
}

void AccessibilityManager::HideHighlights() {
  AccessibilityFocusRingController::Get()->HideHighlights();
}

void AccessibilityManager::SetCaretBounds(const gfx::Rect& bounds_in_screen) {
  // For efficiency only send mojo IPCs to ash if the highlight is enabled.
  if (!IsCaretHighlightEnabled())
    return;

  AccessibilityController::Get()->SetCaretBounds(bounds_in_screen);

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
    const std::set<int>& action_keys,
    const std::string& pref_name) {
  DictionaryPrefUpdate pref_update(profile_->GetPrefs(), pref_name);
  base::ListValue devices;
  devices.Append(kSwitchAccessInternalDevice);
  devices.Append(kSwitchAccessUsbDevice);
  devices.Append(kSwitchAccessBluetoothDevice);
  for (int key : action_keys) {
    const std::string& key_str = base::NumberToString(key);
    pref_update->SetPath(key_str, devices.Clone());
  }

  profile_->GetPrefs()->CommitPendingWrite();
}

// Sends a panel action event to the Select-to-speak extension.
void AccessibilityManager::OnSelectToSpeakPanelAction(
    SelectToSpeakPanelAction action,
    double value) {
  if (!profile_)
    return;

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile_);

  auto event_args = std::make_unique<base::ListValue>();
  event_args->AppendString(AccessibilityPrivateEnumForAction(action));
  if (value != 0.0) {
    event_args->AppendDouble(value);
  }

  auto event = std::make_unique<extensions::Event>(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_SELECT_TO_SPEAK_PANEL_ACTION,
      extensions::api::accessibility_private::OnSelectToSpeakPanelAction::
          kEventName,
      std::move(event_args));
  event_router->DispatchEventWithLazyListener(
      extension_misc::kSelectToSpeakExtensionId, std::move(event));
}

}  // namespace ash

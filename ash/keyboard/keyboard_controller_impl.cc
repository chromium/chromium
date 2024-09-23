// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/keyboard_controller_impl.h"

#include <optional>
#include <utility>

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_ui_factory.h"
#include "ash/keyboard/virtual_keyboard_controller.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/env.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/coordinate_conversion.h"

using keyboard::KeyboardConfig;
using keyboard::KeyboardEnableFlag;

namespace ash {

namespace {

// Boolean controlling whether auto-complete for virtual keyboard is
// enabled.
const char kAutoCompleteEnabledKey[] = "auto_complete_enabled";
// Boolean controlling whether auto-correct for virtual keyboard is
// enabled.
const char kAutoCorrectEnabledKey[] = "auto_correct_enabled";
// Boolean controlling whether handwriting for virtual keyboard is
// enabled.
const char kHandwritingEnabledKey[] = "handwriting_enabled";
// Boolean controlling whether spell check for virtual keyboard is
// enabled.
const char kSpellCheckEnabledKey[] = "spell_check_enabled";
// Boolean controlling whether voice input for virtual keyboard is
// enabled.
const char kVoiceInputEnabledKey[] = "voice_input_enabled";

std::optional<display::Display> GetFirstTouchDisplay() {
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    if (display.touch_support() == display::Display::TouchSupport::AVAILABLE)
      return display;
  }
  return std::nullopt;
}

bool GetVirtualKeyboardFeatureValue(PrefService* prefs,
                                    const std::string& feature_path) {
  DCHECK(prefs);
  const base::Value::Dict& features =
      prefs->GetDict(prefs::kAccessibilityVirtualKeyboardFeatures);

  return features.FindBool(feature_path).value_or(false);
}

}  // namespace

KeyboardControllerImpl::KeyboardControllerImpl(
    SessionControllerImpl* session_controller)
    : session_controller_(session_controller),
      keyboard_ui_controller_(
          std::make_unique<keyboard::KeyboardUIController>()) {
  if (session_controller_)  // May be null in tests.
    session_controller_->AddObserver(this);
  keyboard_ui_controller_->AddObserver(this);
}

KeyboardControllerImpl::~KeyboardControllerImpl() {
  keyboard_ui_controller_->RemoveObserver(this);
  if (session_controller_)  // May be null in tests.
    session_controller_->RemoveObserver(this);
}

// static
void KeyboardControllerImpl::RegisterProfilePrefs(PrefRegistrySimple* registry,
                                                  std::string_view country) {
  // Longpress diacritics pref is default on for NZ managed users only, default
  // off otherwise.
  registry->RegisterBooleanPref(
      ash::prefs::kLongPressDiacriticsEnabled,
      (country == "NZ" &&
       Shell::Get()
               ->system_tray_model()
               ->enterprise_domain()
               ->management_device_mode() == ManagementDeviceMode::kNone) ||
          base::FeatureList::IsEnabled(
              ash::features::kDiacriticsOnPhysicalKeyboardLongpressDefaultOn),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      ash::prefs::kXkbAutoRepeatEnabled, ash::kDefaultKeyAutoRepeatEnabled,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      ash::prefs::kXkbAutoRepeatDelay,
      ash::kDefaultKeyAutoRepeatDelay.InMilliseconds(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      ash::prefs::kXkbAutoRepeatInterval,
      ash::kDefaultKeyAutoRepeatInterval.InMilliseconds(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterDictionaryPref(
      prefs::kAccessibilityVirtualKeyboardFeatures);
}

void KeyboardControllerImpl::CreateVirtualKeyboard(
    std::unique_ptr<keyboard::KeyboardUIFactory> keyboard_ui_factory) {
  DCHECK(keyboard_ui_factory);
  virtual_keyboard_controller_ = std::make_unique<VirtualKeyboardController>();
  keyboard_ui_controller_->Initialize(std::move(keyboard_ui_factory), this);
}

void KeyboardControllerImpl::DestroyVirtualKeyboard() {
  virtual_keyboard_controller_.reset();
  keyboard_ui_controller_->Shutdown();
}

void KeyboardControllerImpl::SendOnKeyboardVisibleBoundsChanged(
    const gfx::Rect& screen_bounds) {
  DVLOG(1) << "OnKeyboardVisibleBoundsChanged: " << screen_bounds.ToString();
  for (auto& observer : observers_)
    observer.OnKeyboardVisibleBoundsChanged(screen_bounds);
}

void KeyboardControllerImpl::SendOnKeyboardUIDestroyed() {
  for (auto& observer : observers_)
    observer.OnKeyboardUIDestroyed();
}

// ash::KeyboardController

keyboard::KeyboardConfig KeyboardControllerImpl::GetKeyboardConfig() {
  if (!keyboard_config_from_pref_enabled_)
    return keyboard_ui_controller_->keyboard_config();

  PrefService* prefs = pref_change_registrar_->prefs();
  KeyboardConfig config;
  config.auto_complete =
      GetVirtualKeyboardFeatureValue(prefs, kAutoCompleteEnabledKey);
  config.auto_correct =
      GetVirtualKeyboardFeatureValue(prefs, kAutoCorrectEnabledKey);
  config.handwriting =
      GetVirtualKeyboardFeatureValue(prefs, kHandwritingEnabledKey);
  config.spell_check =
      GetVirtualKeyboardFeatureValue(prefs, kSpellCheckEnabledKey);
  config.voice_input =
      GetVirtualKeyboardFeatureValue(prefs, kVoiceInputEnabledKey);
  return config;
}

void KeyboardControllerImpl::SetKeyboardConfig(
    const KeyboardConfig& keyboard_config) {
  keyboard_ui_controller_->UpdateKeyboardConfig(keyboard_config);
}

bool KeyboardControllerImpl::IsKeyboardEnabled() {
  return keyboard_ui_controller_->IsEnabled();
}

void KeyboardControllerImpl::SetEnableFlag(KeyboardEnableFlag flag) {
  keyboard_ui_controller_->SetEnableFlag(flag);
}

void KeyboardControllerImpl::ClearEnableFlag(KeyboardEnableFlag flag) {
  keyboard_ui_controller_->ClearEnableFlag(flag);
}

const std::set<keyboard::KeyboardEnableFlag>&
KeyboardControllerImpl::GetEnableFlags() {
  return keyboard_ui_controller_->keyboard_enable_flags();
}

void KeyboardControllerImpl::ReloadKeyboardIfNeeded() {
  keyboard_ui_controller_->Reload();
}

void KeyboardControllerImpl::RebuildKeyboardIfEnabled() {
  // Test IsKeyboardEnableRequested in case of an unlikely edge case where this
  // is called while after the enable state changed to disabled (in which case
  // we do not want to override the requested state).
  keyboard_ui_controller_->RebuildKeyboardIfEnabled();
}

bool KeyboardControllerImpl::IsKeyboardVisible() {
  return keyboard_ui_controller_->IsKeyboardVisible();
}

void KeyboardControllerImpl::ShowKeyboard() {
  if (keyboard_ui_controller_->IsEnabled())
    keyboard_ui_controller_->ShowKeyboard(false /* lock */);
}

void KeyboardControllerImpl::HideKeyboard(HideReason reason) {
  if (!keyboard_ui_controller_->IsEnabled())
    return;
  switch (reason) {
    case HideReason::kUser:
      keyboard_ui_controller_->HideKeyboardByUser();
      break;
    case HideReason::kSystem:
      keyboard_ui_controller_->HideKeyboardExplicitlyBySystem();
      break;
  }
}

void KeyboardControllerImpl::SetContainerType(
    keyboard::ContainerType container_type,
    const gfx::Rect& target_bounds,
    SetContainerTypeCallback callback) {
  keyboard_ui_controller_->SetContainerType(container_type, target_bounds,
                                            std::move(callback));
}

void KeyboardControllerImpl::SetKeyboardLocked(bool locked) {
  keyboard_ui_controller_->set_keyboard_locked(locked);
}

void KeyboardControllerImpl::SetOccludedBounds(
    const std::vector<gfx::Rect>& bounds) {
  // TODO(crbug.com/41379402): Support occluded bounds with multiple
  // rectangles.
  keyboard_ui_controller_->SetOccludedBounds(bounds.empty() ? gfx::Rect()
                                                            : bounds[0]);
}

void KeyboardControllerImpl::SetHitTestBounds(
    const std::vector<gfx::Rect>& bounds) {
  keyboard_ui_controller_->SetHitTestBounds(bounds);
}

bool KeyboardControllerImpl::SetAreaToRemainOnScreen(const gfx::Rect& bounds) {
  return keyboard_ui_controller_->SetAreaToRemainOnScreen(bounds);
}

void KeyboardControllerImpl::SetDraggableArea(const gfx::Rect& bounds) {
  keyboard_ui_controller_->SetDraggableArea(bounds);
}

bool KeyboardControllerImpl::SetWindowBoundsInScreen(
    const gfx::Rect& bounds_in_screen) {
  return keyboard_ui_controller_->SetKeyboardWindowBoundsInScreen(
      bounds_in_screen);
}

void KeyboardControllerImpl::SetKeyboardConfigFromPref(bool enabled) {
  keyboard_config_from_pref_enabled_ = enabled;
  SendKeyboardConfigUpdate();
}

bool KeyboardControllerImpl::ShouldOverscroll() {
  return keyboard_ui_controller_->IsKeyboardOverscrollEnabled();
}

void KeyboardControllerImpl::AddObserver(KeyboardControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void KeyboardControllerImpl::RemoveObserver(
    KeyboardControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

std::optional<KeyRepeatSettings>
KeyboardControllerImpl::GetKeyRepeatSettings() {
  if (!pref_change_registrar_)
    return std::nullopt;
  PrefService* prefs = pref_change_registrar_->prefs();
  bool enabled = prefs->GetBoolean(ash::prefs::kXkbAutoRepeatEnabled);
  int delay_in_ms = prefs->GetInteger(ash::prefs::kXkbAutoRepeatDelay);
  int interval_in_ms = prefs->GetInteger(ash::prefs::kXkbAutoRepeatInterval);
  return KeyRepeatSettings{enabled, base::Milliseconds(delay_in_ms),
                           base::Milliseconds(interval_in_ms)};
}

bool KeyboardControllerImpl::AreTopRowKeysFunctionKeys() {
  if (ash::features::IsInputDeviceSettingsSplitEnabled()) {
    return Shell::Get()
        ->input_device_settings_controller()
        ->GetGeneralizedTopRowAreFKeys();
  }
  PrefService* prefs = pref_change_registrar_->prefs();
  return prefs->GetBoolean(ash::prefs::kSendFunctionKeys);
}

void KeyboardControllerImpl::SetSmartVisibilityEnabled(bool enabled) {
  if (keyboard_ui_controller_->IsEnabled()) {
    keyboard_ui_controller_->SetShouldShowOnTransientBlur(enabled);
  }
}

// SessionObserver
void KeyboardControllerImpl::OnSessionStateChanged(
    session_manager::SessionState state) {
  SetEnableFlagFromCommandLine();
  if (!keyboard_ui_controller_->IsEnabled())
    return;

  switch (state) {
    case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
    case session_manager::SessionState::ACTIVE:
      // Reload the keyboard on user profile change to refresh keyboard
      // extensions with the new profile and ensure the extensions call the
      // proper IME. |LOGGED_IN_NOT_ACTIVE| is needed so that the virtual
      // keyboard works on supervised user creation, http://crbug.com/712873.
      // |ACTIVE| is also needed for guest user workflow.
      RebuildKeyboardIfEnabled();
      break;
    default:
      break;
  }
}

void KeyboardControllerImpl::OnSigninScreenPrefServiceInitialized(
    PrefService* prefs) {
  ObservePrefs(prefs);
}

void KeyboardControllerImpl::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  auto account_id = Shell::Get()->session_controller()->GetActiveAccountId();
  if (prefs && !recorded_accounts_.contains(account_id)) {
    base::UmaHistogramBoolean(
        "ChromeOS.Settings.Device.KeyboardAutoRepeatEnabled",
        prefs->GetBoolean(prefs::kXkbAutoRepeatEnabled));
    base::UmaHistogramTimes(
        "ChromeOS.Settings.Device.KeyboardAutoRepeatDelay",
        base::Milliseconds(prefs->GetInteger(prefs::kXkbAutoRepeatDelay)));
    base::UmaHistogramTimes(
        "ChromeOS.Settings.Device.KeyboardAutoRepeatInterval",
        base::Milliseconds(prefs->GetInteger(prefs::kXkbAutoRepeatInterval)));
    recorded_accounts_.insert(account_id);
  }

  ObservePrefs(prefs);
}

// Start listening to key repeat preferences from the given service.
// Also immediately update observers with the service's current preferences.
//
// We only need to observe the most recent PrefService. It will either be the
// active user's PrefService, or the signin screen's PrefService if nobody's
// logged in yet.
void KeyboardControllerImpl::ObservePrefs(PrefService* prefs) {
  if (!prefs) {
    // Just for testing cases.
    pref_change_registrar_.reset();
    return;
  }

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);

  // Immediately tell all our observers to load this user's saved preferences.
  SendKeyRepeatUpdate();
  SendKeyboardConfigUpdate();

  // Listen to prefs changes and forward them to all observers.
  // |prefs| is assumed to outlive |pref_change_registrar_|, and therefore also
  // its callbacks.
  pref_change_registrar_->Add(
      ash::prefs::kXkbAutoRepeatEnabled,
      base::BindRepeating(&KeyboardControllerImpl::SendKeyRepeatUpdate,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      ash::prefs::kXkbAutoRepeatInterval,
      base::BindRepeating(&KeyboardControllerImpl::SendKeyRepeatUpdate,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      ash::prefs::kXkbAutoRepeatDelay,
      base::BindRepeating(&KeyboardControllerImpl::SendKeyRepeatUpdate,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      ash::prefs::kAccessibilityVirtualKeyboardFeatures,
      base::BindRepeating(&KeyboardControllerImpl::SendKeyboardConfigUpdate,
                          base::Unretained(this)));
}

void KeyboardControllerImpl::SendKeyRepeatUpdate() {
  auto key_repeat_settings = GetKeyRepeatSettings();
  DCHECK(key_repeat_settings.has_value());
  OnKeyRepeatSettingsChanged(key_repeat_settings.value());
}

void KeyboardControllerImpl::SendKeyboardConfigUpdate() {
  keyboard_ui_controller_->UpdateKeyboardConfig(GetKeyboardConfig());
}

void KeyboardControllerImpl::OnRootWindowClosing(aura::Window* root_window) {
  if (keyboard_ui_controller_->GetRootWindow() == root_window) {
    aura::Window* new_parent = GetContainerForDefaultDisplay();
    DCHECK_NE(root_window, new_parent);
    keyboard_ui_controller_->MoveToParentContainer(new_parent);
  }
}

aura::Window* KeyboardControllerImpl::GetContainerForDisplay(
    const display::Display& display) {
  DCHECK(display.is_valid());

  RootWindowController* controller =
      Shell::Get()->GetRootWindowControllerWithDisplayId(display.id());
  aura::Window* container =
      controller ? controller->GetContainer(kShellWindowId_VirtualKeyboardContainer) : nullptr ;
  DCHECK(container);
  return container;
}

aura::Window* KeyboardControllerImpl::GetContainerForDefaultDisplay() {
  const display::Screen* screen = display::Screen::GetScreen();
  const std::optional<display::Display> first_touch_display =
      GetFirstTouchDisplay();
  const bool has_touch_display = first_touch_display.has_value();

  if (window_util::GetFocusedWindow()) {
    // Return the focused display if that display has touch capability or no
    // other display has touch capability.
    const display::Display focused_display =
        screen->GetDisplayNearestWindow(window_util::GetFocusedWindow());
    if (focused_display.is_valid() &&
        (focused_display.touch_support() ==
             display::Display::TouchSupport::AVAILABLE ||
         !has_touch_display)) {
      return GetContainerForDisplay(focused_display);
    }
  }

  // Return the first touch display, or the primary display if there are none.
  return GetContainerForDisplay(
      has_touch_display ? *first_touch_display : screen->GetPrimaryDisplay());
}

void KeyboardControllerImpl::TransferGestureEventToShelf(
    const ui::GestureEvent& e) {
  ash::Shelf* shelf =
      ash::Shelf::ForWindow(keyboard_ui_controller_->GetKeyboardWindow());
  if (shelf) {
    shelf->ProcessGestureEvent(e);
    aura::Env::GetInstance()->gesture_recognizer()->TransferEventsTo(
        keyboard_ui_controller_->GetGestureConsumer(), shelf->GetWindow(),
        ui::TransferTouchesBehavior::kCancel);
    HideKeyboard(HideReason::kUser);
  }
}

void KeyboardControllerImpl::OnKeyboardConfigChanged(
    const keyboard::KeyboardConfig& config) {
  for (auto& observer : observers_)
    observer.OnKeyboardConfigChanged(config);
}

void KeyboardControllerImpl::OnKeyRepeatSettingsChanged(
    const KeyRepeatSettings& settings) {
  for (auto& observer : observers_)
    observer.OnKeyRepeatSettingsChanged(settings);
}

void KeyboardControllerImpl::OnKeyboardVisibilityChanged(bool is_visible) {
  for (auto& observer : observers_)
    observer.OnKeyboardVisibilityChanged(is_visible);
}

void KeyboardControllerImpl::OnKeyboardVisibleBoundsChanged(
    const gfx::Rect& screen_bounds) {
  SendOnKeyboardVisibleBoundsChanged(screen_bounds);
}

void KeyboardControllerImpl::OnKeyboardOccludedBoundsChanged(
    const gfx::Rect& screen_bounds) {
  DVLOG(1) << "OnKeyboardOccludedBoundsChanged: " << screen_bounds.ToString();
  for (auto& observer : observers_)
    observer.OnKeyboardOccludedBoundsChanged(screen_bounds);
}

void KeyboardControllerImpl::OnKeyboardEnableFlagsChanged(
    const std::set<keyboard::KeyboardEnableFlag>& flags) {
  for (auto& observer : observers_)
    observer.OnKeyboardEnableFlagsChanged(flags);
}

void KeyboardControllerImpl::OnKeyboardEnabledChanged(bool is_enabled) {
  for (auto& observer : observers_)
    observer.OnKeyboardEnabledChanged(is_enabled);
}

void KeyboardControllerImpl::SetEnableFlagFromCommandLine() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          keyboard::switches::kEnableVirtualKeyboard)) {
    keyboard_ui_controller_->SetEnableFlag(
        KeyboardEnableFlag::kCommandLineEnabled);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          keyboard::switches::kDisableVirtualKeyboard)) {
    keyboard_ui_controller_->SetEnableFlag(
        KeyboardEnableFlag::kCommandLineDisabled);
  }
}

}  // namespace ash

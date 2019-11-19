// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/virtual_keyboard_private/chrome_virtual_keyboard_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/keyboard/keyboard_types.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/system_connector.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/virtual_keyboard.h"
#include "extensions/common/api/virtual_keyboard_private.h"
#include "media/audio/audio_system.h"
#include "services/audio/public/cpp/audio_system_factory.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/event_injector.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/constants.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace keyboard_api = extensions::api::virtual_keyboard_private;

namespace {

// The hotrod keyboard must be enabled for each session and will remain enabled
// until / unless it is explicitly disabled.
bool g_hotrod_keyboard_enabled = false;

std::string GenerateFeatureFlag(const std::string& feature, bool enabled) {
  return feature + (enabled ? "-enabled" : "-disabled");
}

keyboard::ContainerType ConvertKeyboardModeToContainerType(int mode) {
  switch (mode) {
    case keyboard_api::KEYBOARD_MODE_FULL_WIDTH:
      return keyboard::ContainerType::kFullWidth;
    case keyboard_api::KEYBOARD_MODE_FLOATING:
      return keyboard::ContainerType::kFloating;
  }

  NOTREACHED();
  return keyboard::ContainerType::kFullWidth;
}

// Returns the ui::TextInputClient of the active InputMethod or nullptr.
ui::TextInputClient* GetFocusedTextInputClient() {
  ui::InputMethod* input_method =
      ui::IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  if (!input_method)
    return nullptr;

  return input_method->GetTextInputClient();
}

const char kKeyDown[] = "keydown";
const char kKeyUp[] = "keyup";

void SendProcessKeyEvent(ui::EventType type, aura::WindowTreeHost* host) {
  ui::KeyEvent event(type, ui::VKEY_PROCESSKEY, ui::DomCode::NONE,
                     ui::EF_IS_SYNTHESIZED, ui::DomKey::PROCESS,
                     ui::EventTimeForNow());
  ui::EventDispatchDetails details = aura::EventInjector().Inject(host, &event);
  CHECK(!details.dispatcher_destroyed);
}

// Sends a fabricated key event, where |type| is the event type (which must be
// "keydown" or "keyup"), |key_value| is the unicode value of the character,
// |key_code| is the legacy key code value, |key_name| is the name of the key as
// defined in the DOM3 key event specification, and |modifier| indicates if any
// modifier keys are being virtually pressed. The event is dispatched to the
// active TextInputClient associated with |host|.
bool SendKeyEventImpl(const std::string& type,
                      int key_value,
                      int key_code,
                      const std::string& key_name,
                      int modifiers,
                      aura::WindowTreeHost* host) {
  ui::EventType event_type;
  if (type == kKeyDown)
    event_type = ui::ET_KEY_PRESSED;
  else if (type == kKeyUp)
    event_type = ui::ET_KEY_RELEASED;
  else
    return false;

  ui::KeyboardCode code = static_cast<ui::KeyboardCode>(key_code);

  if (code == ui::VKEY_UNKNOWN) {
    // Handling of special printable characters (e.g. accented characters) for
    // which there is no key code.
    if (event_type == ui::ET_KEY_RELEASED) {
      // This can be null if no text input field is focused.
      ui::TextInputClient* tic = GetFocusedTextInputClient();

      SendProcessKeyEvent(ui::ET_KEY_PRESSED, host);

      ui::KeyEvent char_event(key_value, code, ui::DomCode::NONE, ui::EF_NONE);
      if (tic)
        tic->InsertChar(char_event);
      SendProcessKeyEvent(ui::ET_KEY_RELEASED, host);
    }
    return true;
  }

  ui::DomCode dom_code = ui::KeycodeConverter::CodeStringToDomCode(key_name);
  if (dom_code == ui::DomCode::NONE)
    dom_code = ui::UsLayoutKeyboardCodeToDomCode(code);
  CHECK(dom_code != ui::DomCode::NONE);

  ui::KeyEvent event(event_type, code, dom_code, modifiers);

  // Indicate that the simulated key event is from the Virtual Keyboard.
  ui::Event::Properties properties;
  properties[ui::kPropertyFromVK] =
      std::vector<uint8_t>(ui::kPropertyFromVKSize);
  event.SetProperties(properties);

  ui::EventDispatchDetails details = aura::EventInjector().Inject(host, &event);
  CHECK(!details.dispatcher_destroyed);
  return true;
}

std::string GetKeyboardLayout() {
  // TODO(bshe): layout string is currently hard coded. We should use more
  // standard keyboard layouts.
  return ChromeKeyboardControllerClient::Get()->IsEnableFlagSet(
             keyboard::KeyboardEnableFlag::kAccessibilityEnabled)
             ? "system-qwerty"
             : "qwerty";
}

}  // namespace

namespace extensions {

ChromeVirtualKeyboardDelegate::ChromeVirtualKeyboardDelegate(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

ChromeVirtualKeyboardDelegate::~ChromeVirtualKeyboardDelegate() {}

void ChromeVirtualKeyboardDelegate::GetKeyboardConfig(
    OnKeyboardSettingsCallback on_settings_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!audio_system_) {
    audio_system_ =
        audio::CreateAudioSystem(content::GetSystemConnector()->Clone());
  }
  audio_system_->HasInputDevices(
      base::BindOnce(&ChromeVirtualKeyboardDelegate::OnHasInputDevices,
                     weak_this_, std::move(on_settings_callback)));
}

void ChromeVirtualKeyboardDelegate::OnKeyboardConfigChanged() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetKeyboardConfig(base::Bind(
      &ChromeVirtualKeyboardDelegate::DispatchConfigChangeEvent, weak_this_));
}

bool ChromeVirtualKeyboardDelegate::HideKeyboard() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (!keyboard_client->is_keyboard_enabled())
    return false;

  // Pass HIDE_REASON_MANUAL since calls to HideKeyboard as part of this API
  // would be user generated.
  keyboard_client->HideKeyboard(ash::HideReason::kUser);
  return true;
}

bool ChromeVirtualKeyboardDelegate::InsertText(const base::string16& text) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ui::TextInputClient* tic = GetFocusedTextInputClient();
  if (!tic || tic->GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE)
    return false;

  tic->InsertText(text);
  return true;
}

bool ChromeVirtualKeyboardDelegate::OnKeyboardLoaded() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RecordAction(base::UserMetricsAction("VirtualKeyboardLoaded"));
  return true;
}

void ChromeVirtualKeyboardDelegate::SetHotrodKeyboard(bool enable) {
  if (g_hotrod_keyboard_enabled == enable)
    return;

  g_hotrod_keyboard_enabled = enable;

  // This reloads virtual keyboard even if it exists. This ensures virtual
  // keyboard gets the correct state of the hotrod keyboard through
  // chrome.virtualKeyboardPrivate.getKeyboardConfig.
  ChromeKeyboardControllerClient::Get()->RebuildKeyboardIfEnabled();
}

bool ChromeVirtualKeyboardDelegate::LockKeyboard(bool state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (!keyboard_client->is_keyboard_enabled())
    return false;

  keyboard_client->SetKeyboardLocked(state);
  return true;
}

bool ChromeVirtualKeyboardDelegate::SendKeyEvent(const std::string& type,
                                                 int char_value,
                                                 int key_code,
                                                 const std::string& key_name,
                                                 int modifiers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  aura::Window* window =
      ChromeKeyboardControllerClient::Get()->GetKeyboardWindow();
  return window && SendKeyEventImpl(type, char_value, key_code, key_name,
                                    modifiers, window->GetHost());
}

bool ChromeVirtualKeyboardDelegate::ShowLanguageSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (keyboard_client->is_keyboard_enabled())
    keyboard_client->HideKeyboard(ash::HideReason::kUser);

  base::RecordAction(base::UserMetricsAction("OpenLanguageOptionsDialog"));
  chrome::ShowSettingsSubPageForProfile(ProfileManager::GetActiveUserProfile(),
                                        chrome::kLanguageSubPage);
  return true;
}

bool ChromeVirtualKeyboardDelegate::SetVirtualKeyboardMode(
    int mode_enum,
    base::Optional<gfx::Rect> target_bounds,
    OnSetModeCallback on_set_mode_callback) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (!keyboard_client->is_keyboard_enabled())
    return false;

  keyboard_client->SetContainerType(
      ConvertKeyboardModeToContainerType(mode_enum), target_bounds,
      std::move(on_set_mode_callback));
  return true;
}

bool ChromeVirtualKeyboardDelegate::SetOccludedBounds(
    const std::vector<gfx::Rect>& bounds) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (!keyboard_client->is_keyboard_enabled())
    return false;

  keyboard_client->SetOccludedBounds(bounds);
  return true;
}

bool ChromeVirtualKeyboardDelegate::SetHitTestBounds(
    const std::vector<gfx::Rect>& bounds) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (!keyboard_client->is_keyboard_enabled())
    return false;

  keyboard_client->SetHitTestBounds(bounds);
  return true;
}

bool ChromeVirtualKeyboardDelegate::SetAreaToRemainOnScreen(
    const gfx::Rect& bounds) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (!keyboard_client->is_keyboard_enabled())
    return false;

  return keyboard_client->SetAreaToRemainOnScreen(bounds);
}

bool ChromeVirtualKeyboardDelegate::SetDraggableArea(
    const api::virtual_keyboard_private::Bounds& rect) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  // Since controller will be destroyed when system switch from VK to
  // physical keyboard, return true to avoid unneccessary exception.
  if (!keyboard_client->is_keyboard_enabled())
    return true;

  keyboard_client->SetDraggableArea(
      gfx::Rect(rect.left, rect.top, rect.width, rect.height));
  return true;
}

bool ChromeVirtualKeyboardDelegate::SetRequestedKeyboardState(int state_enum) {
  using keyboard::KeyboardEnableFlag;
  auto* client = ChromeKeyboardControllerClient::Get();
  keyboard_api::KeyboardState state =
      static_cast<keyboard_api::KeyboardState>(state_enum);
  switch (state) {
    case keyboard_api::KEYBOARD_STATE_ENABLED:
      client->SetEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
      break;
    case keyboard_api::KEYBOARD_STATE_DISABLED:
      client->SetEnableFlag(KeyboardEnableFlag::kExtensionDisabled);
      break;
    case keyboard_api::KEYBOARD_STATE_AUTO:
    case keyboard_api::KEYBOARD_STATE_NONE:
      client->ClearEnableFlag(KeyboardEnableFlag::kExtensionDisabled);
      client->ClearEnableFlag(KeyboardEnableFlag::kExtensionEnabled);
      break;
  }
  return true;
}

bool ChromeVirtualKeyboardDelegate::IsLanguageSettingsEnabled() {
  return (user_manager::UserManager::Get()->IsUserLoggedIn() &&
          !chromeos::UserAddingScreen::Get()->IsRunning() &&
          !(chromeos::ScreenLocker::default_screen_locker() &&
            chromeos::ScreenLocker::default_screen_locker()->locked()));
}

void ChromeVirtualKeyboardDelegate::OnHasInputDevices(
    OnKeyboardSettingsCallback on_settings_callback,
    bool has_audio_input_devices) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();

  std::unique_ptr<base::DictionaryValue> results(new base::DictionaryValue());
  results->SetString("layout", GetKeyboardLayout());

  // TODO(bshe): Consolidate a11y, hotrod and normal mode into a mode enum. See
  // crbug.com/529474.
  results->SetBoolean("a11ymode",
                      keyboard_client->IsEnableFlagSet(
                          keyboard::KeyboardEnableFlag::kAccessibilityEnabled));
  results->SetBoolean("hotrodmode", g_hotrod_keyboard_enabled);
  std::unique_ptr<base::ListValue> features(new base::ListValue());

  // TODO(https://crbug.com/880659): Cleanup these flags after removing these
  // flags from IME extension.
  features->AppendString(GenerateFeatureFlag("floatingkeyboard", true));
  features->AppendString(GenerateFeatureFlag("gesturetyping", true));
  // TODO(https://crbug.com/890134): Implement gesture editing.
  features->AppendString(GenerateFeatureFlag("gestureediting", false));
  features->AppendString(GenerateFeatureFlag("fullscreenhandwriting", false));
  features->AppendString(GenerateFeatureFlag("virtualkeyboardmdui", true));

  keyboard::KeyboardConfig config = keyboard_client->GetKeyboardConfig();
  // TODO(oka): Change this to use config.voice_input.
  features->AppendString(GenerateFeatureFlag(
      "voiceinput", has_audio_input_devices && config.voice_input));
  features->AppendString(
      GenerateFeatureFlag("autocomplete", config.auto_complete));
  features->AppendString(
      GenerateFeatureFlag("autocorrect", config.auto_correct));
  features->AppendString(GenerateFeatureFlag("spellcheck", config.spell_check));
  features->AppendString(
      GenerateFeatureFlag("handwriting", config.handwriting));
  features->AppendString(GenerateFeatureFlag(
      "handwritinggesture",
      base::FeatureList::IsEnabled(features::kHandwritingGesture)));
  features->AppendString(GenerateFeatureFlag(
      "usemojodecoder", base::FeatureList::IsEnabled(
                            chromeos::features::kImeDecoderWithSandbox)));
  features->AppendString(GenerateFeatureFlag(
      "hmminputlogic",
      base::FeatureList::IsEnabled(chromeos::features::kImeInputLogicHmm)));
  features->AppendString(GenerateFeatureFlag(
      "fstinputlogic",
      base::FeatureList::IsEnabled(chromeos::features::kImeInputLogicFst)));
  features->AppendString(GenerateFeatureFlag(
      "fstnonenglish",
      base::FeatureList::IsEnabled(chromeos::features::kImeInputLogicFst)));
  features->AppendString(GenerateFeatureFlag(
      "floatingkeyboarddefault",
      base::FeatureList::IsEnabled(
          chromeos::features::kVirtualKeyboardFloatingDefault)));
  features->AppendString(GenerateFeatureFlag(
      "mozcinputlogic",
      base::FeatureList::IsEnabled(chromeos::features::kImeInputLogicMozc)));
  features->AppendString(GenerateFeatureFlag(
      "borderedkey", base::FeatureList::IsEnabled(
                         chromeos::features::kVirtualKeyboardBorderedKey)));
  features->AppendString(GenerateFeatureFlag(
      "resizablefloatingkeyboard",
      base::FeatureList::IsEnabled(
          chromeos::features::kVirtualKeyboardFloatingResizable)));

  results->Set("features", std::move(features));

  std::move(on_settings_callback).Run(std::move(results));
}

void ChromeVirtualKeyboardDelegate::DispatchConfigChangeEvent(
    std::unique_ptr<base::DictionaryValue> settings) {
  EventRouter* router = EventRouter::Get(browser_context_);

  if (!router->HasEventListener(
          keyboard_api::OnKeyboardConfigChanged::kEventName))
    return;

  auto event_args = std::make_unique<base::ListValue>();
  event_args->Append(std::move(settings));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::VIRTUAL_KEYBOARD_PRIVATE_ON_KEYBOARD_CONFIG_CHANGED,
      keyboard_api::OnKeyboardConfigChanged::kEventName, std::move(event_args),
      browser_context_);
  router->BroadcastEvent(std::move(event));
}

api::virtual_keyboard::FeatureRestrictions
ChromeVirtualKeyboardDelegate::RestrictFeatures(
    const api::virtual_keyboard::RestrictFeatures::Params& params) {
  const api::virtual_keyboard::FeatureRestrictions& restrictions =
      params.restrictions;
  api::virtual_keyboard::FeatureRestrictions update;
  keyboard::KeyboardConfig current_config =
      ChromeKeyboardControllerClient::Get()->GetKeyboardConfig();
  keyboard::KeyboardConfig config(current_config);
  if (restrictions.spell_check_enabled &&
      config.spell_check != *restrictions.spell_check_enabled) {
    update.spell_check_enabled =
        std::make_unique<bool>(*restrictions.spell_check_enabled);
    config.spell_check = *restrictions.spell_check_enabled;
  }
  if (restrictions.auto_complete_enabled &&
      config.auto_complete != *restrictions.auto_complete_enabled) {
    update.auto_complete_enabled =
        std::make_unique<bool>(*restrictions.auto_complete_enabled);
    config.auto_complete = *restrictions.auto_complete_enabled;
  }
  if (restrictions.auto_correct_enabled &&
      config.auto_correct != *restrictions.auto_correct_enabled) {
    update.auto_correct_enabled =
        std::make_unique<bool>(*restrictions.auto_correct_enabled);
    config.auto_correct = *restrictions.auto_correct_enabled;
  }
  if (restrictions.voice_input_enabled &&
      config.voice_input != *restrictions.voice_input_enabled) {
    update.voice_input_enabled =
        std::make_unique<bool>(*restrictions.voice_input_enabled);
    config.voice_input = *restrictions.voice_input_enabled;
  }
  if (restrictions.handwriting_enabled &&
      config.handwriting != *restrictions.handwriting_enabled) {
    update.handwriting_enabled =
        std::make_unique<bool>(*restrictions.handwriting_enabled);
    config.handwriting = *restrictions.handwriting_enabled;
  }

  if (config != current_config) {
    ChromeKeyboardControllerClient::Get()->SetKeyboardConfig(config);
    // This reloads virtual keyboard even if it exists. This ensures virtual
    // keyboard gets the correct state through
    // chrome.virtualKeyboardPrivate.getKeyboardConfig.
    // TODO(oka): Extension should reload on it's own by receiving event
    ChromeKeyboardControllerClient::Get()->RebuildKeyboardIfEnabled();
  }
  return update;
}

}  // namespace extensions

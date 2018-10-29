// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/virtual_keyboard_private/chrome_virtual_keyboard_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/url_constants.h"
#include "chromeos/chromeos_features.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/virtual_keyboard.h"
#include "extensions/common/api/virtual_keyboard_private.h"
#include "media/audio/audio_system.h"
#include "services/audio/public/cpp/audio_system_factory.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ui_base_features.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_util.h"

namespace keyboard_api = extensions::api::virtual_keyboard_private;

namespace {

// The hotrod keyboard must be enabled for each session and will remain enabled
// until / unless it is explicitly disabled.
bool g_hotrod_keyboard_enabled = false;

aura::Window* GetKeyboardWindow() {
  auto* controller = keyboard::KeyboardController::Get();
  return controller->IsEnabled() ? controller->GetKeyboardWindow() : nullptr;
}

std::string GenerateFeatureFlag(const std::string& feature, bool enabled) {
  return feature + (enabled ? "-enabled" : "-disabled");
}

}  // namespace

namespace extensions {

ChromeVirtualKeyboardDelegate::ChromeVirtualKeyboardDelegate(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context), weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

ChromeVirtualKeyboardDelegate::~ChromeVirtualKeyboardDelegate() {}

void ChromeVirtualKeyboardDelegate::GetKeyboardConfig(
    OnKeyboardSettingsCallback on_settings_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!audio_system_)
    audio_system_ = audio::CreateAudioSystem(
        content::ServiceManagerConnection::GetForProcess()
            ->GetConnector()
            ->Clone());
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
  auto* controller = keyboard::KeyboardController::Get();
  if (!controller->IsEnabled())
    return false;

  // Pass HIDE_REASON_MANUAL since calls to HideKeyboard as part of this API
  // would be user generated.
  controller->HideKeyboardByUser();
  return true;
}

bool ChromeVirtualKeyboardDelegate::InsertText(const base::string16& text) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ui::InputMethod* input_method =
      ui::IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  if (!input_method)
    return false;

  ui::TextInputClient* tic = input_method->GetTextInputClient();
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
  ChromeKeyboardControllerClient::Get()->ReloadKeyboard();
}

bool ChromeVirtualKeyboardDelegate::LockKeyboard(bool state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* controller = keyboard::KeyboardController::Get();
  if (!controller->IsEnabled())
    return false;

  controller->set_keyboard_locked(state);
  return true;
}

bool ChromeVirtualKeyboardDelegate::SendKeyEvent(const std::string& type,
                                                 int char_value,
                                                 int key_code,
                                                 const std::string& key_name,
                                                 int modifiers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  aura::Window* window = GetKeyboardWindow();
  return window && keyboard::SendKeyEvent(type, char_value, key_code, key_name,
                                          modifiers, window->GetHost());
}

bool ChromeVirtualKeyboardDelegate::ShowLanguageSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* controller = keyboard::KeyboardController::Get();
  if (controller->IsEnabled())
    controller->DismissVirtualKeyboard();

  base::RecordAction(base::UserMetricsAction("OpenLanguageOptionsDialog"));
  chrome::ShowSettingsSubPageForProfile(ProfileManager::GetActiveUserProfile(),
                                        chrome::kLanguageOptionsSubPage);
  return true;
}

bool ChromeVirtualKeyboardDelegate::SetVirtualKeyboardMode(
    int mode_enum,
    base::Optional<gfx::Rect> target_bounds,
    OnSetModeCallback on_set_mode_callback) {
  auto* controller = keyboard::KeyboardController::Get();
  if (!controller->IsEnabled())
    return false;

  controller->SetContainerType(ConvertKeyboardModeToContainerType(mode_enum),
                               std::move(target_bounds),
                               std::move(on_set_mode_callback));
  return true;
}

bool ChromeVirtualKeyboardDelegate::SetOccludedBounds(
    const std::vector<gfx::Rect>& bounds) {
  keyboard::KeyboardController* controller =
      keyboard::KeyboardController::Get();
  if (!controller->IsEnabled())
    return false;

  // TODO(https://crbug.com/826617): Support occluded bounds with multiple
  // rectangles.
  controller->SetOccludedBounds(bounds.empty() ? gfx::Rect() : bounds[0]);
  return true;
}

bool ChromeVirtualKeyboardDelegate::SetHitTestBounds(
    const std::vector<gfx::Rect>& bounds) {
  keyboard::KeyboardController* controller =
      keyboard::KeyboardController::Get();
  if (!controller->IsEnabled())
    return false;

  controller->SetHitTestBounds(bounds);
  return true;
}

keyboard::ContainerType
ChromeVirtualKeyboardDelegate::ConvertKeyboardModeToContainerType(
    int mode) const {
  switch (mode) {
    case keyboard_api::KEYBOARD_MODE_FULL_WIDTH:
      return keyboard::ContainerType::FULL_WIDTH;
    case keyboard_api::KEYBOARD_MODE_FLOATING:
      return keyboard::ContainerType::FLOATING;
    case keyboard_api::KEYBOARD_MODE_FULLSCREEN:
      return keyboard::ContainerType::FULLSCREEN;
  }

  NOTREACHED();
  return keyboard::ContainerType::FULL_WIDTH;
}

bool ChromeVirtualKeyboardDelegate::SetDraggableArea(
    const api::virtual_keyboard_private::Bounds& rect) {
  auto* controller = keyboard::KeyboardController::Get();
  // Since controller will be destroyed when system switch from VK to
  // physical keyboard, return true to avoid unneccessary exception.
  if (!controller->IsEnabled())
    return true;
  return controller->SetDraggableArea(
      gfx::Rect(rect.left, rect.top, rect.width, rect.height));
}

bool ChromeVirtualKeyboardDelegate::SetRequestedKeyboardState(int state_enum) {
  using keyboard::mojom::KeyboardEnableFlag;
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
  std::unique_ptr<base::DictionaryValue> results(new base::DictionaryValue());
  results->SetString("layout", keyboard::GetKeyboardLayout());
  // TODO(bshe): Consolidate a11y, hotrod and normal mode into a mode enum. See
  // crbug.com/529474.
  results->SetBoolean("a11ymode", keyboard::GetAccessibilityKeyboardEnabled());
  results->SetBoolean("hotrodmode", g_hotrod_keyboard_enabled);
  std::unique_ptr<base::ListValue> features(new base::ListValue());

  // 'floatingvirtualkeyboard' is the name of the feature flag for the legacy
  // floating keyboard that was prototyped quite some time ago. It is currently
  // referenced by the extension even though we never enable this value and so
  // re-using that value is not feasible due to the semi-tandem nature of the
  // keyboard extension. The 'floatingkeybard' flag represents the new floating
  // keyboard and should be used for new extension-side feature work for the
  // floating keyboard.
  // TODO(blakeo): once the old flag's usages have been removed from the
  // extension and all pushes have settled, remove this overly verbose comment.
  features->AppendString(GenerateFeatureFlag(
      "floatingkeyboard",
      base::FeatureList::IsEnabled(features::kEnableFloatingVirtualKeyboard)));
  features->AppendString(GenerateFeatureFlag(
      "gesturetyping", !base::CommandLine::ForCurrentProcess()->HasSwitch(
                           keyboard::switches::kDisableGestureTyping)));
  // TODO(shend): Gesture editing is not implemented in the MD UI.
  // https://crbug.com/890134.
  bool enable_gesture_editing =
      !base::FeatureList::IsEnabled(features::kEnableVirtualKeyboardMdUi) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          keyboard::switches::kDisableGestureEditing);
  features->AppendString(
      GenerateFeatureFlag("gestureediting", enable_gesture_editing));
  features->AppendString(GenerateFeatureFlag(
      "fullscreenhandwriting",
      base::FeatureList::IsEnabled(
          features::kEnableFullscreenHandwritingVirtualKeyboard)));
  features->AppendString(GenerateFeatureFlag(
      "virtualkeyboardmdui",
      base::FeatureList::IsEnabled(features::kEnableVirtualKeyboardMdUi)));
  features->AppendString(GenerateFeatureFlag(
      "imeservice", base::FeatureList::IsEnabled(
                        chromeos::features::kImeServiceConnectable)));

  auto config = ChromeKeyboardControllerClient::Get()->GetKeyboardConfig();
  // TODO(oka): Change this to use config.voice_input.
  features->AppendString(GenerateFeatureFlag(
      "voiceinput", has_audio_input_devices && config.voice_input &&
                        !base::CommandLine::ForCurrentProcess()->HasSwitch(
                            keyboard::switches::kDisableVoiceInput)));
  features->AppendString(
      GenerateFeatureFlag("autocomplete", config.auto_complete));
  features->AppendString(
      GenerateFeatureFlag("autocorrect", config.auto_correct));
  features->AppendString(GenerateFeatureFlag("spellcheck", config.spell_check));
  features->AppendString(
      GenerateFeatureFlag("handwriting", config.handwriting));

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
  keyboard::mojom::KeyboardConfig current_config =
      ChromeKeyboardControllerClient::Get()->GetKeyboardConfig();
  keyboard::mojom::KeyboardConfig config(current_config);
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

  if (!config.Equals(current_config)) {
    ChromeKeyboardControllerClient::Get()->SetKeyboardConfig(config);
    // This reloads virtual keyboard even if it exists. This ensures virtual
    // keyboard gets the correct state through
    // chrome.virtualKeyboardPrivate.getKeyboardConfig.
    // TODO(oka): Extension should reload on it's own by receiving event
    ChromeKeyboardControllerClient::Get()->ReloadKeyboard();
  }
  return update;
}

}  // namespace extensions

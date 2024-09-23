// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"

#include <stddef.h>

#include <optional>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/keyboard/keyboard_config.h"
#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/input_method/assistive_window_properties.h"
#include "chrome/browser/ash/input_method/input_method_engine.h"
#include "chrome/browser/ash/input_method/native_input_method_engine.h"
#include "chrome/browser/ash/input_method/text_field_contextual_info_fetcher.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/common/extensions/api/input_ime.h"
#include "chrome/common/extensions/api/input_method_private.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "ui/base/ime/ash/component_extension_ime_manager.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/ime_keymap.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/text_input_method.h"
#include "ui/base/ime/constants.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace {

namespace input_ime = extensions::api::input_ime;
namespace input_method_private = extensions::api::input_method_private;
namespace DeleteSurroundingText =
    extensions::api::input_ime::DeleteSurroundingText;
namespace UpdateMenuItems = extensions::api::input_ime::UpdateMenuItems;
namespace HideInputView = extensions::api::input_ime::HideInputView;
namespace SetMenuItems = extensions::api::input_ime::SetMenuItems;
namespace SetCursorPosition = extensions::api::input_ime::SetCursorPosition;
namespace SetCandidates = extensions::api::input_ime::SetCandidates;
namespace SetCandidateWindowProperties =
    extensions::api::input_ime::SetCandidateWindowProperties;
namespace SetAssistiveWindowProperties =
    extensions::api::input_ime::SetAssistiveWindowProperties;
namespace SetAssistiveWindowButtonHighlighted =
    extensions::api::input_ime::SetAssistiveWindowButtonHighlighted;
namespace ClearComposition = extensions::api::input_ime::ClearComposition;
namespace OnScreenProjectionChanged =
    extensions::api::input_method_private::OnScreenProjectionChanged;
namespace FinishComposingText =
    extensions::api::input_method_private::FinishComposingText;

using ::ash::TextInputMethod;
using ::ash::input_method::InputMethodEngine;

const char kErrorEngineNotAvailable[] = "The engine is not available.";
const char kErrorSetMenuItemsFail[] = "Could not create menu items.";
const char kErrorUpdateMenuItemsFail[] = "Could not update menu items.";
const char kErrorEngineNotActive[] = "The engine is not active.";
const char kErrorRouterNotAvailable[] = "The router is not available.";

void SetMenuItemToMenu(const input_ime::MenuItem& input,
                       ash::input_method::InputMethodManager::MenuItem* out) {
  out->modified = 0;
  out->id = input.id;
  if (input.label) {
    out->modified |= InputMethodEngine::MENU_ITEM_MODIFIED_LABEL;
    out->label = *input.label;
  }

  if (input.style != input_ime::MenuItemStyle::kNone) {
    out->style =
        static_cast<ash::input_method::InputMethodManager::MenuItemStyle>(
            input.style);
  }

  out->visible = input.visible ? *input.visible : true;

  if (input.checked)
    out->modified |= InputMethodEngine::MENU_ITEM_MODIFIED_CHECKED;
  out->checked = input.checked ? *input.checked : false;

  out->enabled = input.enabled ? *input.enabled : true;
}

keyboard::KeyboardConfig GetKeyboardConfig() {
  return ChromeKeyboardControllerClient::Get()->GetKeyboardConfig();
}

ash::ime::AssistiveWindowType ConvertAssistiveWindowType(
    input_ime::AssistiveWindowType type) {
  switch (type) {
    case input_ime::AssistiveWindowType::kNone:
      return ash::ime::AssistiveWindowType::kNone;
    case input_ime::AssistiveWindowType::kUndo:
      return ash::ime::AssistiveWindowType::kUndoWindow;
  }
}

ui::ime::ButtonId ConvertAssistiveWindowButtonId(
    input_ime::AssistiveWindowButton id) {
  switch (id) {
    case input_ime::AssistiveWindowButton::kAddToDictionary:
      return ui::ime::ButtonId::kAddToDictionary;
    case input_ime::AssistiveWindowButton::kUndo:
      return ui::ime::ButtonId::kUndo;
    case input_ime::AssistiveWindowButton::kNone:
      return ui::ime::ButtonId::kNone;
  }
}

input_ime::AssistiveWindowButton ConvertAssistiveWindowButton(
    const ui::ime::ButtonId id) {
  switch (id) {
    case ui::ime::ButtonId::kNone:
    case ui::ime::ButtonId::kSmartInputsSettingLink:
    case ui::ime::ButtonId::kSuggestion:
    case ui::ime::ButtonId::kLearnMore:
    case ui::ime::ButtonId::kIgnoreSuggestion:
      return input_ime::AssistiveWindowButton::kNone;
    case ui::ime::ButtonId::kUndo:
      return input_ime::AssistiveWindowButton::kUndo;
    case ui::ime::ButtonId::kAddToDictionary:
      return input_ime::AssistiveWindowButton::kAddToDictionary;
  }
}

input_ime::AssistiveWindowType ConvertAssistiveWindowType(
    const ash::ime::AssistiveWindowType& type) {
  switch (type) {
    case ash::ime::AssistiveWindowType::kNone:
    case ash::ime::AssistiveWindowType::kEmojiSuggestion:
    case ash::ime::AssistiveWindowType::kPersonalInfoSuggestion:
    case ash::ime::AssistiveWindowType::kGrammarSuggestion:
    case ash::ime::AssistiveWindowType::kMultiWordSuggestion:
    case ash::ime::AssistiveWindowType::kLongpressDiacriticsSuggestion:
    case ash::ime::AssistiveWindowType::kLearnMore:
      return input_ime::AssistiveWindowType::kNone;
    case ash::ime::AssistiveWindowType::kUndoWindow:
      return input_ime::AssistiveWindowType::kUndo;
  }
}

std::string GetKeyFromEvent(const ui::KeyEvent& event) {
  const std::string code = event.GetCodeString();
  if (base::StartsWith(code, "Control", base::CompareCase::SENSITIVE))
    return "Ctrl";
  if (base::StartsWith(code, "Shift", base::CompareCase::SENSITIVE))
    return "Shift";
  if (base::StartsWith(code, "Alt", base::CompareCase::SENSITIVE))
    return "Alt";
  if (base::StartsWith(code, "Arrow", base::CompareCase::SENSITIVE))
    return code.substr(5);
  if (code == "Escape")
    return "Esc";
  if (code == "Backspace" || code == "Tab" || code == "Enter" ||
      code == "CapsLock" || code == "Power")
    return code;
  // Cases for media keys.
  switch (event.key_code()) {
    case ui::VKEY_BROWSER_BACK:
    case ui::VKEY_F1:
      return "HistoryBack";
    case ui::VKEY_BROWSER_FORWARD:
    case ui::VKEY_F2:
      return "HistoryForward";
    case ui::VKEY_BROWSER_REFRESH:
    case ui::VKEY_F3:
      return "BrowserRefresh";
    case ui::VKEY_ZOOM:
    case ui::VKEY_F4:
      return "ChromeOSFullscreen";
    case ui::VKEY_MEDIA_LAUNCH_APP1:
    case ui::VKEY_F5:
      return "ChromeOSSwitchWindow";
    case ui::VKEY_BRIGHTNESS_DOWN:
    case ui::VKEY_F6:
      return "BrightnessDown";
    case ui::VKEY_BRIGHTNESS_UP:
    case ui::VKEY_F7:
      return "BrightnessUp";
    case ui::VKEY_VOLUME_MUTE:
    case ui::VKEY_F8:
      return "AudioVolumeMute";
    case ui::VKEY_VOLUME_DOWN:
    case ui::VKEY_F9:
      return "AudioVolumeDown";
    case ui::VKEY_VOLUME_UP:
    case ui::VKEY_F10:
      return "AudioVolumeUp";
    default:
      break;
  }
  uint16_t ch = 0;
  // Ctrl+? cases, gets key value for Ctrl is not down.
  if (event.flags() & ui::EF_CONTROL_DOWN) {
    ui::KeyEvent event_no_ctrl(event.type(), event.key_code(),
                               event.flags() ^ ui::EF_CONTROL_DOWN);
    ch = event_no_ctrl.GetCharacter();
  } else {
    ch = event.GetCharacter();
  }
  return base::UTF16ToUTF8(std::u16string(1, ch));
}

std::string GetKeyFromEventForGoogleBrandedInputMethod(
    const ui::KeyEvent& event) {
  switch (event.key_code()) {
    case ui::VKEY_F1:
    case ui::VKEY_F2:
    case ui::VKEY_F3:
    case ui::VKEY_F4:
    case ui::VKEY_F5:
    case ui::VKEY_F6:
    case ui::VKEY_F7:
    case ui::VKEY_F8:
    case ui::VKEY_F9:
    case ui::VKEY_F10:
      return ui::KeycodeConverter::DomKeyToKeyString(event.GetDomKey());
    default:
      return GetKeyFromEvent(event);
  }
}

// TODO(b/247441188): Change the input extension JS API to use
// PersonalizationMode instead of a bool.
bool ConvertPersonalizationMode(const TextInputMethod::InputContext& context) {
  switch (context.personalization_mode) {
    case ash::PersonalizationMode::kEnabled:
      return true;
    case ash::PersonalizationMode::kDisabled:
      return false;
  }
}

InputMethodEngine* GetEngineIfActive(Profile* profile,
                                     const std::string& extension_id,
                                     std::string* error) {
  extensions::InputImeEventRouter* event_router =
      extensions::GetInputImeEventRouter(profile);
  if (!event_router) {
    *error = kErrorRouterNotAvailable;
    return nullptr;
  }
  InputMethodEngine* engine = static_cast<InputMethodEngine*>(
      event_router->GetEngineIfActive(extension_id, error));
  return engine;
}

class ImeObserverChromeOS
    : public ash::input_method::InputMethodEngineObserver {
 public:
  ImeObserverChromeOS(const std::string& extension_id, Profile* profile)
      : extension_id_(extension_id), profile_(profile) {}

  ImeObserverChromeOS(const ImeObserverChromeOS&) = delete;
  ImeObserverChromeOS& operator=(const ImeObserverChromeOS&) = delete;

  ~ImeObserverChromeOS() override = default;

  void OnCandidateClicked(const std::string& component_id,
                          int candidate_id,
                          ash::input_method::MouseButtonEvent button) override {
    if (extension_id_.empty() ||
        !HasListener(input_ime::OnCandidateClicked::kEventName))
      return;

    input_ime::MouseButton button_enum = input_ime::MouseButton::kNone;
    switch (button) {
      case ash::input_method::MOUSE_BUTTON_MIDDLE:
        button_enum = input_ime::MouseButton::kMiddle;
        break;

      case ash::input_method::MOUSE_BUTTON_RIGHT:
        button_enum = input_ime::MouseButton::kRight;
        break;

      case ash::input_method::MOUSE_BUTTON_LEFT:
      // Default to left.
      default:
        button_enum = input_ime::MouseButton::kLeft;
        break;
    }

    auto args(input_ime::OnCandidateClicked::Create(component_id, candidate_id,
                                                    button_enum));

    DispatchEventToExtension(extensions::events::INPUT_IME_ON_CANDIDATE_CLICKED,
                             input_ime::OnCandidateClicked::kEventName,
                             std::move(args));
  }

  void OnMenuItemActivated(const std::string& component_id,
                           const std::string& menu_id) override {
    if (extension_id_.empty() ||
        !HasListener(input_ime::OnMenuItemActivated::kEventName))
      return;

    auto args(input_ime::OnMenuItemActivated::Create(component_id, menu_id));

    DispatchEventToExtension(
        extensions::events::INPUT_IME_ON_MENU_ITEM_ACTIVATED,
        input_ime::OnMenuItemActivated::kEventName, std::move(args));
  }

  void OnScreenProjectionChanged(bool is_projected) override {
    if (extension_id_.empty() ||
        !HasListener(OnScreenProjectionChanged::kEventName)) {
      return;
    }
    // Note: this is a private API event.
    base::Value::List args;
    args.Append(is_projected);

    DispatchEventToExtension(
        extensions::events::INPUT_METHOD_PRIVATE_ON_SCREEN_PROJECTION_CHANGED,
        OnScreenProjectionChanged::kEventName, std::move(args));
  }

  void OnActivate(const std::string& component_id) override {
    // Don't check whether the extension listens on onActivate event here.
    // Send onActivate event to give the IME a chance to add their listeners.
    if (extension_id_.empty())
      return;

    auto args(input_ime::OnActivate::Create(
        component_id, input_ime::ParseScreenType(GetCurrentScreenType())));

    DispatchEventToExtension(extensions::events::INPUT_IME_ON_ACTIVATE,
                             input_ime::OnActivate::kEventName,
                             std::move(args));
  }

  void OnBlur(const std::string& engine_id, int context_id) override {
    if (extension_id_.empty() || !HasListener(input_ime::OnBlur::kEventName))
      return;

    auto args(input_ime::OnBlur::Create(context_id));

    DispatchEventToExtension(extensions::events::INPUT_IME_ON_BLUR,
                             input_ime::OnBlur::kEventName, std::move(args));
  }

  void OnKeyEvent(const std::string& component_id,
                  const ui::KeyEvent& event,
                  TextInputMethod::KeyEventDoneCallback callback) override {
    if (extension_id_.empty())
      return;

    // If there is no listener for the event, no need to dispatch the event to
    // extension. Instead, releases the key event for default system behavior.
    if (!ShouldForwardKeyEvent()) {
      // Continue processing the key event so that the physical keyboard can
      // still work.
      std::move(callback).Run(ui::ime::KeyEventHandledState::kNotHandled);
      return;
    }

    std::string error;
    InputMethodEngine* engine =
        GetEngineIfActive(profile_, extension_id_, &error);
    if (!engine)
      return;
    const std::string request_id =
        engine->AddPendingKeyEvent(component_id, std::move(callback));

    input_ime::KeyboardEvent keyboard_event;
    keyboard_event.type = (event.type() == ui::EventType::kKeyReleased)
                              ? input_ime::KeyboardEventType::kKeyup
                              : input_ime::KeyboardEventType::kKeydown;

    // For legacy reasons, we still put a |requestID| into the keyData, even
    // though there is already a |requestID| argument in OnKeyEvent.
    keyboard_event.request_id = request_id;

    // If the given key event is from VK, it means the key event was simulated.
    // Sets the |extension_id| value so that the IME extension can ignore it.
    auto* properties = event.properties();
    if (properties &&
        properties->find(ui::kPropertyFromVK) != properties->end())
      keyboard_event.extension_id = extension_id_;

    keyboard_event.key =
        (extension_id_ == "jkghodnilhceideoidjikpgommlajknk" &&
         base::FeatureList::IsEnabled(ash::features::kJapaneseFunctionRow))
            ? GetKeyFromEventForGoogleBrandedInputMethod(event)
            : GetKeyFromEvent(event);
    keyboard_event.code = event.code() == ui::DomCode::NONE
                              ? ash::KeyboardCodeToDomKeycode(event.key_code())
                              : event.GetCodeString();
    keyboard_event.alt_key = event.IsAltDown();
    keyboard_event.altgr_key = event.IsAltGrDown();
    keyboard_event.ctrl_key = event.IsControlDown();
    keyboard_event.shift_key = event.IsShiftDown();
    keyboard_event.caps_lock = event.IsCapsLockOn();

    auto args(input_ime::OnKeyEvent::Create(component_id, keyboard_event,
                                            request_id));

    DispatchEventToExtension(extensions::events::INPUT_IME_ON_KEY_EVENT,
                             input_ime::OnKeyEvent::kEventName,
                             std::move(args));
  }

  void OnReset(const std::string& component_id) override {
    if (extension_id_.empty() || !HasListener(input_ime::OnReset::kEventName))
      return;

    auto args(input_ime::OnReset::Create(component_id));

    DispatchEventToExtension(extensions::events::INPUT_IME_ON_RESET,
                             input_ime::OnReset::kEventName, std::move(args));
  }

  void OnDeactivated(const std::string& component_id) override {
    if (extension_id_.empty() ||
        !HasListener(input_ime::OnDeactivated::kEventName))
      return;

    auto args(input_ime::OnDeactivated::Create(component_id));

    DispatchEventToExtension(extensions::events::INPUT_IME_ON_DEACTIVATED,
                             input_ime::OnDeactivated::kEventName,
                             std::move(args));
  }

  void OnCaretBoundsChanged(const gfx::Rect& caret_bounds) override {
    if (extension_id_.empty() ||
        !HasListener(input_method_private::OnCaretBoundsChanged::kEventName)) {
      return;
    }

    // Note: this is a private API event;
    input_method_private::OnCaretBoundsChanged::CaretBounds caret_bounds_arg;
    caret_bounds_arg.x = caret_bounds.x();
    caret_bounds_arg.y = caret_bounds.y();
    caret_bounds_arg.w = caret_bounds.width();
    caret_bounds_arg.h = caret_bounds.height();

    DispatchEventToExtension(
        extensions::events::INPUT_METHOD_PRIVATE_ON_CARET_BOUNDS_CHANGED,
        input_method_private::OnCaretBoundsChanged::kEventName,
        input_method_private::OnCaretBoundsChanged::Create(caret_bounds_arg));
  }

  void OnFocus(const std::string& engine_id,
               int context_id,
               const TextInputMethod::InputContext& context) override {
    if (extension_id_.empty()) {
      return;
    }

    // There is both a public and private OnFocus event. The private OnFocus
    // event is only for ChromeOS and contains additional information about pen
    // inputs. We ensure that we only trigger one OnFocus event.
    if (ExtensionHasListener(input_method_private::OnFocus::kEventName)) {
      input_method_private::InputContext private_api_input_context;
      private_api_input_context.context_id = context_id;
      private_api_input_context.type =
          input_method_private::ParseInputContextType(
              ConvertInputContextType(context));
      private_api_input_context.mode = input_method_private::ParseInputModeType(
          ConvertInputContextMode(context));
      private_api_input_context.auto_correct =
          ConvertInputContextAutoCorrect(context.autocorrection_mode);
      private_api_input_context.auto_complete =
          ConvertInputContextAutoComplete(context.autocompletion_mode);
      private_api_input_context.auto_capitalize =
          ConvertInputContextAutoCapitalizePrivate(
              context.autocapitalization_mode);
      private_api_input_context.spell_check =
          ConvertInputContextSpellCheck(context.spellcheck_mode);
      private_api_input_context.should_do_learning =
          ConvertPersonalizationMode(context);
      private_api_input_context.focus_reason =
          input_method_private::ParseFocusReason(
              ConvertInputContextFocusReason(context));

      // Populate app key for private OnFocus.
      // TODO(b/163645900): Add app type later.
      ash::input_method::TextFieldContextualInfo info;
      ash::input_method::GetTextFieldAppTypeAndKey(info);
      private_api_input_context.app_key = info.app_key;

      auto args(
          input_method_private::OnFocus::Create(private_api_input_context));
      DispatchEventToExtension(
          extensions::events::INPUT_METHOD_PRIVATE_ON_FOCUS,
          input_method_private::OnFocus::kEventName, std::move(args));
    } else if (HasListener(input_ime::OnFocus::kEventName)) {
      input_ime::InputContext public_api_input_context;
      public_api_input_context.context_id = context_id;
      public_api_input_context.type =
          input_ime::ParseInputContextType(ConvertInputContextType(context));
      public_api_input_context.auto_correct =
          ConvertInputContextAutoCorrect(context.autocorrection_mode);
      public_api_input_context.auto_complete =
          ConvertInputContextAutoComplete(context.autocompletion_mode);
      public_api_input_context.auto_capitalize =
          ConvertInputContextAutoCapitalizePublic(
              context.autocapitalization_mode);
      public_api_input_context.spell_check =
          ConvertInputContextSpellCheck(context.spellcheck_mode);
      public_api_input_context.should_do_learning =
          ConvertPersonalizationMode(context);

      auto args(input_ime::OnFocus::Create(public_api_input_context));
      DispatchEventToExtension(extensions::events::INPUT_IME_ON_FOCUS,
                               input_ime::OnFocus::kEventName, std::move(args));
    }
  }

  void OnSurroundingTextChanged(const std::string& component_id,
                                const std::u16string& text,
                                const gfx::Range selection_range,
                                int offset_pos) override {
    if (extension_id_.empty() ||
        !HasListener(input_ime::OnSurroundingTextChanged::kEventName))
      return;

    input_ime::OnSurroundingTextChanged::SurroundingInfo info;
    // |info.text| is encoded in UTF8 here so |info.focus| etc may not match the
    // index in |info.text|, the javascript code on the extension side should
    // handle it.
    info.text = base::UTF16ToUTF8(text);
    // Due to a legacy mistake, the selection is reversed (i.e. 'focus' is the
    // start and 'anchor' is the end), opposite to what the API documentation
    // claims.
    // TODO(b/245020074): Fix this without breaking existing 3p IMEs.
    info.focus = selection_range.start();
    info.anchor = selection_range.end();
    info.offset = offset_pos;
    auto args(input_ime::OnSurroundingTextChanged::Create(component_id, info));

    DispatchEventToExtension(
        extensions::events::INPUT_IME_ON_SURROUNDING_TEXT_CHANGED,
        input_ime::OnSurroundingTextChanged::kEventName, std::move(args));
  }

  void OnAssistiveWindowButtonClicked(
      const ui::ime::AssistiveWindowButton& button) override {
    if (extension_id_.empty() ||
        !HasListener(input_ime::OnAssistiveWindowButtonClicked::kEventName)) {
      return;
    }
    input_ime::OnAssistiveWindowButtonClicked::Details details;
    details.button_id = ConvertAssistiveWindowButton(button.id);
    details.window_type = ConvertAssistiveWindowType(button.window_type);

    auto args(input_ime::OnAssistiveWindowButtonClicked::Create(details));
    DispatchEventToExtension(
        extensions::events::INPUT_IME_ON_ASSISTIVE_WINDOW_BUTTON_CLICKED,
        input_ime::OnAssistiveWindowButtonClicked::kEventName, std::move(args));
  }

  void OnAssistiveWindowChanged(
      const ash::ime::AssistiveWindow& window) override {}

  void OnSuggestionsChanged(
      const std::vector<std::string>& suggestions) override {
    auto args(input_method_private::OnSuggestionsChanged::Create(suggestions));
    DispatchEventToExtension(
        extensions::events::INPUT_IME_ON_SUGGESTIONS_CHANGED,
        input_method_private::OnSuggestionsChanged::kEventName,
        std::move(args));
  }

  void OnInputMethodOptionsChanged(const std::string& engine_id) override {
    auto args(
        input_method_private::OnInputMethodOptionsChanged::Create(engine_id));
    DispatchEventToExtension(
        extensions::events::INPUT_IME_ON_INPUT_METHOD_OPTIONS_CHANGED,
        input_method_private::OnInputMethodOptionsChanged::kEventName,
        std::move(args));
  }

 private:
  // Helper function used to forward the given event to the |profile_|'s event
  // router, which dipatches the event the extension with |extension_id_|.
  void DispatchEventToExtension(
      extensions::events::HistogramValue histogram_value,
      const std::string& event_name,
      base::Value::List args) {
    if (event_name == input_ime::OnActivate::kEventName) {
      // Send onActivate event regardless of it's listened by the IME.
      auto event = std::make_unique<extensions::Event>(
          histogram_value, event_name, std::move(args), profile_);
      extensions::EventRouter::Get(profile_)->DispatchEventWithLazyListener(
          extension_id_, std::move(event));
      return;
    }

    // For suspended IME extension (e.g. XKB extension), don't awake it by IME
    // events except onActivate. The IME extension should be awake by other
    // events (e.g. runtime.onMessage) from its other pages.
    // This is to save memory for steady state Chrome OS on which the users
    // don't want any IME features.
    extensions::ExtensionRegistry* extension_registry =
        extensions::ExtensionRegistry::Get(profile_);
    if (extension_registry) {
      const extensions::Extension* extension =
          extension_registry->enabled_extensions().GetByID(extension_id_);
      if (!extension)
        return;
      extensions::ProcessManager* process_manager =
          extensions::ProcessManager::Get(profile_);
      if (extensions::BackgroundInfo::HasBackgroundPage(extension) &&
          !process_manager->GetBackgroundHostForExtension(extension_id_)) {
        return;
      }
    }

    auto event = std::make_unique<extensions::Event>(
        histogram_value, event_name, std::move(args), profile_);
    extensions::EventRouter::Get(profile_)->DispatchEventToExtension(
        extension_id_, std::move(event));
  }

  // The component IME extensions need to know the current screen type (e.g.
  // lock screen, login screen, etc.) so that its on-screen keyboard page
  // won't open new windows/pages. See crbug.com/395621.
  std::string GetCurrentScreenType() {
    switch (ash::input_method::InputMethodManager::Get()
                ->GetActiveIMEState()
                ->GetUIStyle()) {
      case ash::input_method::InputMethodManager::UIStyle::kLogin:
        return "login";
      case ash::input_method::InputMethodManager::UIStyle::kSecondaryLogin:
        return "secondary-login";
      case ash::input_method::InputMethodManager::UIStyle::kLock:
        return "lock";
      case ash::input_method::InputMethodManager::UIStyle::kNormal:
        return "normal";
    }
  }

  // Returns true if the extension is ready to accept key event, otherwise
  // returns false.
  bool ShouldForwardKeyEvent() const {
    // Only forward key events to extension if there are non-lazy listeners
    // for onKeyEvent. Because if something wrong with the lazy background
    // page which doesn't register listener for onKeyEvent, it will not handle
    // the key events, and therefore, all key events will be eaten.
    // This is for error-tolerance, and it means that onKeyEvent will never wake
    // up lazy background page.
    const extensions::EventListenerMap::ListenerList& listeners =
        extensions::EventRouter::Get(profile_)
            ->listeners()
            .GetEventListenersByName(input_ime::OnKeyEvent::kEventName);
    for (const std::unique_ptr<extensions::EventListener>& listener :
         listeners) {
      if (listener->extension_id() == extension_id_ && !listener->IsLazy())
        return true;
    }
    return false;
  }

  // Returns true if there are any listeners on the given event.
  // TODO(crbug.com/41384866): Merge this with |ExtensionHasListener|.
  bool HasListener(const std::string& event_name) const {
    return extensions::EventRouter::Get(profile_)->HasEventListener(event_name);
  }

  // Returns true if the extension has any listeners on the given event.
  bool ExtensionHasListener(const std::string& event_name) const {
    return extensions::EventRouter::Get(profile_)->ExtensionHasEventListener(
        extension_id_, event_name);
  }

  std::string ConvertInputContextFocusReason(
      TextInputMethod::InputContext input_context) {
    switch (input_context.focus_reason) {
      case ui::TextInputClient::FOCUS_REASON_NONE:
        return "";
      case ui::TextInputClient::FOCUS_REASON_MOUSE:
        return "mouse";
      case ui::TextInputClient::FOCUS_REASON_TOUCH:
        return "touch";
      case ui::TextInputClient::FOCUS_REASON_PEN:
        return "pen";
      case ui::TextInputClient::FOCUS_REASON_OTHER:
        return "other";
    }
  }

  bool ConvertInputContextAutoCorrect(ash::AutocorrectionMode mode) {
    return GetKeyboardConfig().auto_correct &&
           mode != ash::AutocorrectionMode::kDisabled;
  }

  bool ConvertInputContextAutoComplete(ash::AutocompletionMode mode) {
    return GetKeyboardConfig().auto_complete &&
           mode != ash::AutocompletionMode::kDisabled;
  }

  input_method_private::AutoCapitalizeType
  ConvertInputContextAutoCapitalizePrivate(ash::AutocapitalizationMode mode) {
    if (!GetKeyboardConfig().auto_capitalize)
      return input_method_private::AutoCapitalizeType::kOff;

    switch (mode) {
      case ash::AutocapitalizationMode::kUnspecified:
        // Autocapitalize flag may be missing for native text fields,
        // crbug/1002713. As a safe default, use
        // input_method_private::AUTO_CAPITALIZE_TYPE_OFF
        // ("off" in API specs). This corresponds to Blink's "off" represented
        // by ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_NONE. Note: This fallback must
        // not be input_method_private::AUTO_CAPITALIZE_TYPE_NONE which means
        // "unspecified" and translates to JS falsy empty string, because the
        // API specifies a non-falsy AutoCapitalizeType enum for
        // InputContext.autoCapitalize.
        return input_method_private::AutoCapitalizeType::kOff;
      case ash::AutocapitalizationMode::kNone:
        return input_method_private::AutoCapitalizeType::kOff;
      case ash::AutocapitalizationMode::kCharacters:
        return input_method_private::AutoCapitalizeType::kCharacters;
      case ash::AutocapitalizationMode::kWords:
        return input_method_private::AutoCapitalizeType::kWords;
      case ash::AutocapitalizationMode::kSentences:
        return input_method_private::AutoCapitalizeType::kSentences;
    }
  }

  bool ConvertInputContextSpellCheck(ash::SpellcheckMode mode) {
    return GetKeyboardConfig().spell_check &&
           mode != ash::SpellcheckMode::kDisabled;
  }

  std::string ConvertInputContextMode(
      TextInputMethod::InputContext input_context) {
    std::string input_mode_type = "none";  // default to nothing
    switch (input_context.mode) {
      case ui::TEXT_INPUT_MODE_SEARCH:
        input_mode_type = "search";
        break;
      case ui::TEXT_INPUT_MODE_TEL:
        input_mode_type = "tel";
        break;
      case ui::TEXT_INPUT_MODE_URL:
        input_mode_type = "url";
        break;
      case ui::TEXT_INPUT_MODE_EMAIL:
        input_mode_type = "email";
        break;
      case ui::TEXT_INPUT_MODE_NUMERIC:
        input_mode_type = "numeric";
        break;
      case ui::TEXT_INPUT_MODE_DECIMAL:
        input_mode_type = "decimal";
        break;
      case ui::TEXT_INPUT_MODE_NONE:
        input_mode_type = "noKeyboard";
        break;
      case ui::TEXT_INPUT_MODE_TEXT:
        input_mode_type = "text";
        break;
      default:
        input_mode_type = "";
        break;
    }
    return input_mode_type;
  }

  std::string ConvertInputContextType(
      TextInputMethod::InputContext input_context) {
    std::string input_context_type = "text";
    switch (input_context.type) {
      case ui::TEXT_INPUT_TYPE_SEARCH:
        input_context_type = "search";
        break;
      case ui::TEXT_INPUT_TYPE_TELEPHONE:
        input_context_type = "tel";
        break;
      case ui::TEXT_INPUT_TYPE_URL:
        input_context_type = "url";
        break;
      case ui::TEXT_INPUT_TYPE_EMAIL:
        input_context_type = "email";
        break;
      case ui::TEXT_INPUT_TYPE_NUMBER:
        input_context_type = "number";
        break;
      case ui::TEXT_INPUT_TYPE_PASSWORD:
        input_context_type = "password";
        break;
      case ui::TEXT_INPUT_TYPE_NULL:
        input_context_type = "null";
        break;
      default:
        input_context_type = "text";
        break;
    }
    return input_context_type;
  }

  input_ime::AutoCapitalizeType ConvertInputContextAutoCapitalizePublic(
      ash::AutocapitalizationMode mode) {
    // NOTE: ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_NONE corresponds to Blink's
    // "none" that's a synonym for "off", while
    // input_ime::AUTO_CAPITALIZE_TYPE_NONE auto-generated via API specs means
    // "unspecified" and translates to empty string. The latter should not be
    // emitted as the API specifies a non-falsy enum. So technically there's a
    // bug here; either this impl or the API needs fixing. However, as a public
    // API, the behaviour is left intact for now.
    switch (mode) {
      case ash::AutocapitalizationMode::kNone:
        return input_ime::AutoCapitalizeType::kNone;
      case ash::AutocapitalizationMode::kCharacters:
        return input_ime::AutoCapitalizeType::kCharacters;
      case ash::AutocapitalizationMode::kWords:
        return input_ime::AutoCapitalizeType::kWords;
      case ash::AutocapitalizationMode::kSentences:
        return input_ime::AutoCapitalizeType::kSentences;
      case ash::AutocapitalizationMode::kUnspecified:
        // The default value is "sentences".
        return input_ime::AutoCapitalizeType::kSentences;
    }
  }

  extensions::ExtensionId extension_id_;
  raw_ptr<Profile, DanglingUntriaged> profile_;
};

}  // namespace

namespace extensions {

InputMethodEngine* GetEngine(content::BrowserContext* browser_context,
                             const std::string& extension_id,
                             std::string* error) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  InputImeEventRouter* event_router = GetInputImeEventRouter(profile);
  DCHECK(event_router) << kErrorRouterNotAvailable;
  InputMethodEngine* engine =
      static_cast<InputMethodEngine*>(event_router->GetEngine(extension_id));
  DCHECK(engine) << kErrorEngineNotAvailable;
  if (!engine)
    *error = kErrorEngineNotAvailable;
  return engine;
}

InputImeEventRouter::InputImeEventRouter(Profile* profile)
    : profile_(profile) {}

InputImeEventRouter::~InputImeEventRouter() = default;

bool InputImeEventRouter::RegisterImeExtension(
    const std::string& extension_id,
    const std::vector<InputComponentInfo>& input_components) {
  VLOG(1) << "RegisterImeExtension: " << extension_id;

  if (engine_map_[extension_id])
    return false;

  auto* manager = ash::input_method::InputMethodManager::Get();
  ash::ComponentExtensionIMEManager* comp_ext_ime_manager =
      manager->GetComponentExtensionIMEManager();

  ash::input_method::InputMethodDescriptors descriptors;
  // Only creates descriptors for 3rd party IME extension, because the
  // descriptors for component IME extensions are managed by InputMethodUtil.
  if (!comp_ext_ime_manager->IsAllowlistedExtension(extension_id)) {
    for (const auto& component : input_components) {
      // For legacy reasons, multiple physical keyboard XKB layouts can be
      // specified in the IME extension manifest for each input method. However,
      // CrOS only supports one layout per input method. Thus use the "first"
      // layout if specified, else default to "us". Please note however, as
      // "layouts" in the manifest are considered unordered and parsed into an
      // std::set, if there are multiple, it's actually undefined as to which
      // "first" entry is used. CrOS IME extension manifests should therefore
      // specify one and only one layout per input method to avoid confusion.
      const std::string& layout =
          component.layouts.empty() ? "us" : *component.layouts.begin();

      std::vector<std::string> languages;
      languages.assign(component.languages.begin(), component.languages.end());

      const std::string& input_method_id =
          ash::extension_ime_util::GetInputMethodID(extension_id, component.id);
      descriptors.push_back(ash::input_method::InputMethodDescriptor(
          input_method_id, component.name,
          std::string(),  // TODO(uekawa): Set short name.
          layout, languages,
          false,  // 3rd party IMEs are always not for login.
          component.options_page_url, component.input_view_url,
          // Not applicable to 3rd-party IMEs.
          /*handwriting_language=*/std::nullopt));
    }
  }

  Profile* profile = GetProfile();
  // TODO(https://crbug.com/1140236): Investigate whether profile selection
  // is really needed.
  bool is_login = false;
  // When Chrome starts with the Login screen, sometimes active IME State was
  // not set yet. It's asynchronous process. So we need a fallback for that.
  scoped_refptr<ash::input_method::InputMethodManager::State> active_state =
      ash::input_method::InputMethodManager::Get()->GetActiveIMEState();
  if (active_state) {
    is_login = active_state->GetUIStyle() ==
               ash::input_method::InputMethodManager::UIStyle::kLogin;
  } else {
    is_login = ash::ProfileHelper::IsSigninProfile(profile);
  }

  if (is_login && profile->HasPrimaryOTRProfile()) {
    profile = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }

  auto observer = std::make_unique<ImeObserverChromeOS>(extension_id, profile);
  auto engine =
      extension_id == "jkghodnilhceideoidjikpgommlajknk"
          ? std::make_unique<ash::input_method::NativeInputMethodEngine>()
          : std::make_unique<InputMethodEngine>();
  engine->Initialize(std::move(observer), extension_id.c_str(), profile);
  engine_map_[extension_id] = std::move(engine);

  ash::UserSessionManager::GetInstance()
      ->GetDefaultIMEState(profile)
      ->AddInputMethodExtension(extension_id, descriptors,
                                engine_map_[extension_id].get());

  return true;
}

void InputImeEventRouter::UnregisterAllImes(const std::string& extension_id) {
  auto it = engine_map_.find(extension_id);
  if (it != engine_map_.end()) {
    auto active_ime_state =
        ash::input_method::InputMethodManager::Get()->GetActiveIMEState();
    if (active_ime_state) {
      active_ime_state->RemoveInputMethodExtension(extension_id);
    }
    engine_map_.erase(it);
  }
}

InputMethodEngine* InputImeEventRouter::GetEngine(
    const std::string& extension_id) {
  auto it = engine_map_.find(extension_id);
  if (it == engine_map_.end()) {
    LOG(ERROR) << kErrorEngineNotAvailable << " extension id: " << extension_id;
    return nullptr;
  } else {
    return it->second.get();
  }
}

InputMethodEngine* InputImeEventRouter::GetEngineIfActive(
    const std::string& extension_id,
    std::string* error) {
  auto it = engine_map_.find(extension_id);
  if (it == engine_map_.end()) {
    LOG(ERROR) << kErrorEngineNotAvailable << " extension id: " << extension_id;
    *error = kErrorEngineNotAvailable;
    return nullptr;
  } else if (it->second->IsActive()) {
    return it->second.get();
  } else {
    LOG(WARNING) << kErrorEngineNotActive << " extension id: " << extension_id;
    *error = kErrorEngineNotActive;
    return nullptr;
  }
}

ExtensionFunction::ResponseAction InputImeClearCompositionFunction::Run() {
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine) {
    return RespondNow(Error(InformativeError(error, static_function_name())));
  }

  std::optional<ClearComposition::Params> parent_params =
      ClearComposition::Params::Create(args());
  const ClearComposition::Params::Parameters& params =
      parent_params->parameters;

  bool success = engine->ClearComposition(params.context_id, &error);
  base::Value::List results;
  results.Append(success);
  return RespondNow(success
                        ? ArgumentList(std::move(results))
                        : ErrorWithArguments(
                              std::move(results),
                              InformativeError(error, static_function_name())));
}

ExtensionFunction::ResponseAction InputImeHideInputViewFunction::Run() {
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine)
    return RespondNow(Error(InformativeError(error, static_function_name())));
  engine->HideInputView();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
InputImeSetAssistiveWindowPropertiesFunction::Run() {
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine) {
    return RespondNow(Error(InformativeError(error, static_function_name())));
  }
  std::optional<SetAssistiveWindowProperties::Params> parent_params =
      SetAssistiveWindowProperties::Params::Create(args());
  const SetAssistiveWindowProperties::Params::Parameters& params =
      parent_params->parameters;
  const input_ime::AssistiveWindowProperties& window = params.properties;
  ash::input_method::AssistiveWindowProperties assistive_window;

  assistive_window.visible = window.visible;
  assistive_window.type = ConvertAssistiveWindowType(window.type);
  if (window.announce_string)
    assistive_window.announce_string =
        base::UTF8ToUTF16(*window.announce_string);

  engine->SetAssistiveWindowProperties(params.context_id, assistive_window,
                                       &error);
  if (!error.empty())
    return RespondNow(Error(InformativeError(error, static_function_name())));
  return RespondNow(WithArguments(true));
}

ExtensionFunction::ResponseAction
InputImeSetAssistiveWindowButtonHighlightedFunction::Run() {
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine) {
    return RespondNow(Error(InformativeError(error, static_function_name())));
  }
  std::optional<SetAssistiveWindowButtonHighlighted::Params> parent_params =
      SetAssistiveWindowButtonHighlighted::Params::Create(args());
  const SetAssistiveWindowButtonHighlighted::Params::Parameters& params =
      parent_params->parameters;
  ui::ime::AssistiveWindowButton button;

  button.id = ConvertAssistiveWindowButtonId(params.button_id);
  button.window_type = ConvertAssistiveWindowType(params.window_type);
  if (params.announce_string)
    button.announce_string = base::UTF8ToUTF16(*params.announce_string);

  engine->SetButtonHighlighted(params.context_id, button, params.highlighted,
                               &error);
  if (!error.empty())
    return RespondNow(Error(InformativeError(error, static_function_name())));

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
InputImeSetCandidateWindowPropertiesFunction::Run() {
  std::optional<SetCandidateWindowProperties::Params> parent_params =
      SetCandidateWindowProperties::Params::Create(args());
  const SetCandidateWindowProperties::Params::Parameters& params =
      parent_params->parameters;

  std::string error;
  InputMethodEngine* engine =
      GetEngine(browser_context(), extension_id(), &error);
  if (!engine) {
    return RespondNow(Error(InformativeError(error, static_function_name())));
  }

  const SetCandidateWindowProperties::Params::Parameters::Properties&
      properties = params.properties;

  if (properties.visible &&
      !engine->SetCandidateWindowVisible(*properties.visible, &error)) {
    base::Value::List results;
    results.Append(false);
    return RespondNow(ErrorWithArguments(
        std::move(results), InformativeError(error, static_function_name())));
  }

  InputMethodEngine::CandidateWindowProperty properties_out =
      engine->GetCandidateWindowProperty(params.engine_id);
  bool modified = false;

  if (properties.cursor_visible) {
    properties_out.is_cursor_visible = *properties.cursor_visible;
    modified = true;
  }

  if (properties.vertical) {
    properties_out.is_vertical = *properties.vertical;
    modified = true;
  }

  if (properties.page_size) {
    properties_out.page_size = *properties.page_size;
    modified = true;
  }

  if (properties.window_position == input_ime::WindowPosition::kComposition) {
    properties_out.show_window_at_composition = true;
    modified = true;
  } else if (properties.window_position == input_ime::WindowPosition::kCursor) {
    properties_out.show_window_at_composition = false;
    modified = true;
  }

  if (properties.auxiliary_text) {
    properties_out.auxiliary_text = *properties.auxiliary_text;
    modified = true;
  }

  if (properties.auxiliary_text_visible) {
    properties_out.is_auxiliary_text_visible =
        *properties.auxiliary_text_visible;
    modified = true;
  }

  if (properties.current_candidate_index) {
    properties_out.current_candidate_index =
        *properties.current_candidate_index;
    modified = true;
  }

  if (properties.total_candidates) {
    properties_out.total_candidates = *properties.total_candidates;
    modified = true;
  }

  if (modified) {
    engine->SetCandidateWindowProperty(params.engine_id, properties_out);
  }

  return RespondNow(WithArguments(true));
}

ExtensionFunction::ResponseAction InputImeSetCandidatesFunction::Run() {
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine) {
    return RespondNow(Error(InformativeError(error, static_function_name())));
  }

  std::optional<SetCandidates::Params> parent_params =
      SetCandidates::Params::Create(args());
  const SetCandidates::Params::Parameters& params = parent_params->parameters;

  std::vector<InputMethodEngine::Candidate> candidates_out;
  for (const auto& candidate_in : params.candidates) {
    candidates_out.emplace_back();
    candidates_out.back().value = candidate_in.candidate;
    candidates_out.back().id = candidate_in.id;
    if (candidate_in.label)
      candidates_out.back().label = *candidate_in.label;
    if (candidate_in.annotation)
      candidates_out.back().annotation = *candidate_in.annotation;
    if (candidate_in.usage) {
      candidates_out.back().usage.title = candidate_in.usage->title;
      candidates_out.back().usage.body = candidate_in.usage->body;
    }
  }

  bool success =
      engine->SetCandidates(params.context_id, candidates_out, &error);
  base::Value::List results;
  results.Append(success);
  return RespondNow(success
                        ? ArgumentList(std::move(results))
                        : ErrorWithArguments(
                              std::move(results),
                              InformativeError(error, static_function_name())));
}

ExtensionFunction::ResponseAction InputImeSetCursorPositionFunction::Run() {
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine) {
    return RespondNow(Error(InformativeError(error, static_function_name())));
  }

  std::optional<SetCursorPosition::Params> parent_params =
      SetCursorPosition::Params::Create(args());
  const SetCursorPosition::Params::Parameters& params =
      parent_params->parameters;

  bool success =
      engine->SetCursorPosition(params.context_id, params.candidate_id, &error);
  base::Value::List results;
  results.Append(success);
  return RespondNow(success
                        ? ArgumentList(std::move(results))
                        : ErrorWithArguments(
                              std::move(results),
                              InformativeError(error, static_function_name())));
}

ExtensionFunction::ResponseAction InputImeSetMenuItemsFunction::Run() {
  std::optional<SetMenuItems::Params> parent_params =
      SetMenuItems::Params::Create(args());
  const input_ime::MenuParameters& params = parent_params->parameters;

  std::string error;
  InputMethodEngine* engine =
      GetEngine(browser_context(), extension_id(), &error);
  if (!engine) {
    return RespondNow(Error(InformativeError(error, static_function_name())));
  }

  if (engine->GetActiveComponentId() != params.engine_id) {
    return RespondNow(
        Error(InformativeError(kErrorEngineNotActive, static_function_name())));
  }

  std::vector<ash::input_method::InputMethodManager::MenuItem> items_out;
  for (const input_ime::MenuItem& item_in : params.items) {
    items_out.emplace_back();
    SetMenuItemToMenu(item_in, &items_out.back());
  }

  if (!engine->UpdateMenuItems(items_out, &error)) {
    return RespondNow(Error(InformativeError(
        base::StringPrintf("%s %s", kErrorSetMenuItemsFail, error.c_str()),
        static_function_name())));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction InputImeUpdateMenuItemsFunction::Run() {
  std::optional<UpdateMenuItems::Params> parent_params =
      UpdateMenuItems::Params::Create(args());
  const input_ime::MenuParameters& params = parent_params->parameters;

  std::string error;
  InputMethodEngine* engine =
      GetEngine(browser_context(), extension_id(), &error);
  if (!engine) {
    return RespondNow(Error(InformativeError(error, static_function_name())));
  }

  if (engine->GetActiveComponentId() != params.engine_id) {
    return RespondNow(
        Error(InformativeError(kErrorEngineNotActive, static_function_name())));
  }

  std::vector<ash::input_method::InputMethodManager::MenuItem> items_out;
  for (const input_ime::MenuItem& item_in : params.items) {
    items_out.emplace_back();
    SetMenuItemToMenu(item_in, &items_out.back());
  }

  if (!engine->UpdateMenuItems(items_out, &error)) {
    return RespondNow(Error(InformativeError(
        base::StringPrintf("%s %s", kErrorUpdateMenuItemsFail, error.c_str()),
        static_function_name())));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction InputImeDeleteSurroundingTextFunction::Run() {
  std::optional<DeleteSurroundingText::Params> parent_params =
      DeleteSurroundingText::Params::Create(args());
  const DeleteSurroundingText::Params::Parameters& params =
      parent_params->parameters;

  std::string error;
  InputMethodEngine* engine =
      GetEngine(browser_context(), extension_id(), &error);
  if (!engine) {
    return RespondNow(Error(InformativeError(error, static_function_name())));
  }

  engine->DeleteSurroundingText(params.context_id, params.offset, params.length,
                                &error);
  return RespondNow(
      error.empty() ? NoArguments()
                    : Error(InformativeError(error, static_function_name())));
}

ExtensionFunction::ResponseAction
InputMethodPrivateFinishComposingTextFunction::Run() {
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine)
    return RespondNow(Error(InformativeError(error, static_function_name())));
  std::optional<FinishComposingText::Params> parent_params =
      FinishComposingText::Params::Create(args());
  const FinishComposingText::Params::Parameters& params =
      parent_params->parameters;
  engine->FinishComposingText(params.context_id, &error);
  return RespondNow(
      error.empty() ? NoArguments()
                    : Error(InformativeError(error, static_function_name())));
}

void InputImeAPI::OnExtensionLoaded(content::BrowserContext* browser_context,
                                    const Extension* extension) {
  const std::vector<InputComponentInfo>* input_components =
      InputComponents::GetInputComponents(extension);
  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context));
  if (input_components && event_router) {
    if (extension->id() == event_router->GetUnloadedExtensionId()) {
      // After the 1st-party IME extension being unloaded unexpectedly,
      // we don't unregister the IME entries so after the extension being
      // reloaded we should reactivate the engine so that the IME extension
      // can receive the onActivate event to recover itself upon the
      // unexpected unload.
      std::string error;
      InputMethodEngine* engine =
          event_router->GetEngineIfActive(extension->id(), &error);
      DCHECK(engine) << error;
      // When extension is unloaded unexpectedly and reloaded, OS doesn't pass
      // details.browser_context value in OnListenerAdded callback. So we need
      // to reactivate engine here.
      if (engine)
        engine->Enable(engine->GetActiveComponentId());
      event_router->SetUnloadedExtensionId("");
    } else {
      event_router->RegisterImeExtension(extension->id(), *input_components);
    }
  }
}

void InputImeAPI::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      UnloadedExtensionReason reason) {
  const std::vector<InputComponentInfo>* input_components =
      InputComponents::GetInputComponents(extension);
  if (!input_components || input_components->empty())
    return;
  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context));
  if (!event_router)
    return;
  auto* manager = ash::input_method::InputMethodManager::Get();
  ash::ComponentExtensionIMEManager* comp_ext_ime_manager =
      manager->GetComponentExtensionIMEManager();

  if (comp_ext_ime_manager->IsAllowlistedExtension(extension->id())) {
    // Since the first party ime is not allow to uninstall, and when it's
    // unloaded unexpectedly, OS will recover the extension at once.
    // So should not unregister the IMEs. Otherwise the IME icons on the
    // desktop shelf will disappear. see bugs: 775507,788247,786273,761714.
    // But still need to unload keyboard container document. Since ime extension
    // need to re-render the document when it's recovered.
    auto* keyboard_client = ChromeKeyboardControllerClient::Get();
    if (keyboard_client->is_keyboard_enabled()) {
      // Keyboard controller "Reload" method only reload current page when the
      // url is changed. So we need unload the current page first. Then next
      // engine->Enable() can refresh the inputview page correctly.
      // Empties the content url and reload the controller to unload the
      // current page.
      // TODO(wuyingbing): Should add a new method to unload the document.
      manager->GetActiveIMEState()->DisableInputView();
      keyboard_client->ReloadKeyboardIfNeeded();
    }
    event_router->SetUnloadedExtensionId(extension->id());
  } else {
    event_router->UnregisterAllImes(extension->id());
  }
}

void InputImeAPI::OnListenerAdded(const EventListenerInfo& details) {
  if (details.is_lazy)
    return;

  // Other listeners may trigger this function, but only reactivate the IME
  // on focus event.
  if (details.event_name != input_ime::OnFocus::kEventName &&
      details.event_name != input_method_private::OnFocus::kEventName)
    return;

  std::string error;
  InputMethodEngine* engine =
      GetEngineIfActive(Profile::FromBrowserContext(details.browser_context),
                        details.extension_id, &error);
  // Notifies the IME extension for IME ready with onActivate/onFocus events.
  if (engine)
    engine->Enable(engine->GetActiveComponentId());
}

void InputImeAPI::OnListenerRemoved(const EventListenerInfo& details) {
  if (details.is_lazy)
    return;

  // If a key event listener was removed, cancel all the pending key events
  // because they might've been dropped by the IME.
  if (details.event_name != input_ime::OnKeyEvent::kEventName)
    return;

  std::string error;
  InputMethodEngine* engine =
      GetEngineIfActive(Profile::FromBrowserContext(details.browser_context),
                        details.extension_id, &error);
  if (engine)
    engine->CancelPendingKeyEvents();
}

}  // namespace extensions

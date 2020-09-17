// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "ash/public/cpp/keyboard/keyboard_config.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/input_method/assistive_window_properties.h"
#include "chrome/browser/chromeos/input_method/input_host_helper.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/chromeos/input_method/native_input_method_engine.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/common/extensions/api/input_ime.h"
#include "chrome/common/extensions/api/input_method_private.h"
#include "chromeos/constants/chromeos_features.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/ime_engine_handler_interface.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ui_base_features.h"

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
namespace OnCompositionBoundsChanged =
    extensions::api::input_method_private::OnCompositionBoundsChanged;
namespace NotifyImeMenuItemActivated =
    extensions::api::input_method_private::NotifyImeMenuItemActivated;
namespace OnScreenProjectionChanged =
    extensions::api::input_method_private::OnScreenProjectionChanged;
namespace SetSelectionRange =
    extensions::api::input_method_private::SetSelectionRange;
namespace FinishComposingText =
    extensions::api::input_method_private::FinishComposingText;
using chromeos::InputMethodEngine;
using chromeos::InputMethodEngineBase;
using ui::IMEEngineHandlerInterface;

namespace {
const char kErrorEngineNotAvailable[] = "The engine is not available.";
const char kErrorSetMenuItemsFail[] = "Could not create menu items.";
const char kErrorUpdateMenuItemsFail[] = "Could not update menu items.";
const char kErrorEngineNotActive[] = "The engine is not active.";
const char kErrorRouterNotAvailable[] = "The router is not available.";

void SetMenuItemToMenu(
    const input_ime::MenuItem& input,
    chromeos::input_method::InputMethodManager::MenuItem* out) {
  out->modified = 0;
  out->id = input.id;
  if (input.label) {
    out->modified |= InputMethodEngine::MENU_ITEM_MODIFIED_LABEL;
    out->label = *input.label;
  }

  if (input.style != input_ime::MENU_ITEM_STYLE_NONE) {
    out->modified |= InputMethodEngine::MENU_ITEM_MODIFIED_STYLE;
    out->style =
        static_cast<chromeos::input_method::InputMethodManager::MenuItemStyle>(
            input.style);
  }

  if (input.visible)
    out->modified |= InputMethodEngine::MENU_ITEM_MODIFIED_VISIBLE;
  out->visible = input.visible ? *input.visible : true;

  if (input.checked)
    out->modified |= InputMethodEngine::MENU_ITEM_MODIFIED_CHECKED;
  out->checked = input.checked ? *input.checked : false;

  if (input.enabled)
    out->modified |= InputMethodEngine::MENU_ITEM_MODIFIED_ENABLED;
  out->enabled = input.enabled ? *input.enabled : true;
}

keyboard::KeyboardConfig GetKeyboardConfig() {
  return ChromeKeyboardControllerClient::Get()->GetKeyboardConfig();
}

ui::ime::AssistiveWindowType ConvertAssistiveWindowType(
    input_ime::AssistiveWindowType type) {
  switch (type) {
    case input_ime::ASSISTIVE_WINDOW_TYPE_NONE:
      return ui::ime::AssistiveWindowType::kNone;
    case input_ime::ASSISTIVE_WINDOW_TYPE_UNDO:
      return ui::ime::AssistiveWindowType::kUndoWindow;
  }
}

ui::ime::ButtonId ConvertAssistiveWindowButtonId(
    input_ime::AssistiveWindowButton id) {
  switch (id) {
    case input_ime::ASSISTIVE_WINDOW_BUTTON_ADDTODICTIONARY:
      return ui::ime::ButtonId::kAddToDictionary;
    case input_ime::ASSISTIVE_WINDOW_BUTTON_UNDO:
      return ui::ime::ButtonId::kUndo;
    case input_ime::ASSISTIVE_WINDOW_BUTTON_NONE:
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
      return input_ime::ASSISTIVE_WINDOW_BUTTON_NONE;
    case ui::ime::ButtonId::kUndo:
      return input_ime::ASSISTIVE_WINDOW_BUTTON_UNDO;
    case ui::ime::ButtonId::kAddToDictionary:
      return input_ime::ASSISTIVE_WINDOW_BUTTON_ADDTODICTIONARY;
  }
}

input_ime::AssistiveWindowType ConvertAssistiveWindowType(
    const ui::ime::AssistiveWindowType& type) {
  switch (type) {
    case ui::ime::AssistiveWindowType::kNone:
    case ui::ime::AssistiveWindowType::kEmojiSuggestion:
    case ui::ime::AssistiveWindowType::kPersonalInfoSuggestion:
      return input_ime::AssistiveWindowType::ASSISTIVE_WINDOW_TYPE_NONE;
    case ui::ime::AssistiveWindowType::kUndoWindow:
      return input_ime::AssistiveWindowType::ASSISTIVE_WINDOW_TYPE_UNDO;
  }
}

class ImeObserverChromeOS : public ui::ImeObserver {
 public:
  ImeObserverChromeOS(const std::string& extension_id, Profile* profile)
      : ImeObserver(extension_id, profile) {}

  ~ImeObserverChromeOS() override = default;

  // chromeos::InputMethodEngineBase::Observer overrides.
  void OnInputContextUpdate(
      const IMEEngineHandlerInterface::InputContext& context) override {
    if (extension_id_.empty() ||
        !HasListener(input_ime::OnInputContextUpdate::kEventName))
      return;

    input_ime::InputContext context_value;
    context_value.context_id = context.id;
    context_value.type =
        input_ime::ParseInputContextType(ConvertInputContextType(context));

    std::unique_ptr<base::ListValue> args(
        input_ime::OnInputContextUpdate::Create(context_value));

    DispatchEventToExtension(
        extensions::events::INPUT_IME_ON_INPUT_CONTEXT_UPDATE,
        input_ime::OnInputContextUpdate::kEventName, std::move(args));
  }

  void OnCandidateClicked(
      const std::string& component_id,
      int candidate_id,
      InputMethodEngineBase::MouseButtonEvent button) override {
    if (extension_id_.empty() ||
        !HasListener(input_ime::OnCandidateClicked::kEventName))
      return;

    input_ime::MouseButton button_enum = input_ime::MOUSE_BUTTON_NONE;
    switch (button) {
      case InputMethodEngineBase::MOUSE_BUTTON_MIDDLE:
        button_enum = input_ime::MOUSE_BUTTON_MIDDLE;
        break;

      case InputMethodEngineBase::MOUSE_BUTTON_RIGHT:
        button_enum = input_ime::MOUSE_BUTTON_RIGHT;
        break;

      case InputMethodEngineBase::MOUSE_BUTTON_LEFT:
      // Default to left.
      default:
        button_enum = input_ime::MOUSE_BUTTON_LEFT;
        break;
    }

    std::unique_ptr<base::ListValue> args(input_ime::OnCandidateClicked::Create(
        component_id, candidate_id, button_enum));

    DispatchEventToExtension(extensions::events::INPUT_IME_ON_CANDIDATE_CLICKED,
                             input_ime::OnCandidateClicked::kEventName,
                             std::move(args));
  }

  void OnMenuItemActivated(const std::string& component_id,
                           const std::string& menu_id) override {
    if (extension_id_.empty() ||
        !HasListener(input_ime::OnMenuItemActivated::kEventName))
      return;

    std::unique_ptr<base::ListValue> args(
        input_ime::OnMenuItemActivated::Create(component_id, menu_id));

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
    std::unique_ptr<base::ListValue> args(new base::ListValue());
    args->Append(std::make_unique<base::Value>(is_projected));

    DispatchEventToExtension(
        extensions::events::INPUT_METHOD_PRIVATE_ON_SCREEN_PROJECTION_CHANGED,
        OnScreenProjectionChanged::kEventName, std::move(args));
  }

  void OnCompositionBoundsChanged(
      const std::vector<gfx::Rect>& bounds) override {
    if (extension_id_.empty() ||
        !HasListener(OnCompositionBoundsChanged::kEventName))
      return;

    // Note: this is a private API event.
    auto bounds_list = std::make_unique<base::ListValue>();
    for (auto bound : bounds) {
      auto bounds_value = std::make_unique<base::DictionaryValue>();
      bounds_value->SetInteger("x", bound.x());
      bounds_value->SetInteger("y", bound.y());
      bounds_value->SetInteger("w", bound.width());
      bounds_value->SetInteger("h", bound.height());
      bounds_list->Append(std::move(bounds_value));
    }

    if (bounds_list->GetSize() <= 0)
      return;
    std::unique_ptr<base::ListValue> args(new base::ListValue());

    // The old extension code uses the first parameter to get the bounds of the
    // first composition character, so for backward compatibility, add it here.
    base::Value* first_value = NULL;
    if (bounds_list->Get(0, &first_value))
      args->Append(first_value->CreateDeepCopy());
    args->Append(std::move(bounds_list));

    DispatchEventToExtension(
        extensions::events::INPUT_METHOD_PRIVATE_ON_COMPOSITION_BOUNDS_CHANGED,
        OnCompositionBoundsChanged::kEventName, std::move(args));
  }

  void OnFocus(
      const IMEEngineHandlerInterface::InputContext& context) override {
    if (extension_id_.empty())
      return;

    // There is both a public and private OnFocus event. The private OnFocus
    // event is only for ChromeOS and contains additional information about pen
    // inputs. We ensure that we only trigger one OnFocus event.
    if (ExtensionHasListener(input_method_private::OnFocus::kEventName)) {
      input_method_private::InputContext input_context;
      input_context.context_id = context.id;
      input_context.type = input_method_private::ParseInputContextType(
          ConvertInputContextType(context));
      input_context.mode = input_method_private::ParseInputModeType(
          ConvertInputContextMode(context));
      input_context.auto_correct = ConvertInputContextAutoCorrect(context);
      input_context.auto_complete = ConvertInputContextAutoComplete(context);
      input_context.auto_capitalize =
          ConvertInputContextAutoCapitalize(context.flags);
      input_context.spell_check = ConvertInputContextSpellCheck(context);
      input_context.has_been_password = ConvertHasBeenPassword(context);
      input_context.should_do_learning = context.should_do_learning;
      input_context.focus_reason = input_method_private::ParseFocusReason(
          ConvertInputContextFocusReason(context));

      // Populate app key for private OnFocus.
      // TODO(b/163645900): Add app type later.
      chromeos::input_host_helper::InputAssociatedHost host;
      chromeos::input_host_helper::PopulateInputHost(&host);
      input_context.app_key = std::make_unique<std::string>(host.app_key);

      std::unique_ptr<base::ListValue> args(
          input_method_private::OnFocus::Create(input_context));

      DispatchEventToExtension(
          extensions::events::INPUT_METHOD_PRIVATE_ON_FOCUS,
          input_method_private::OnFocus::kEventName, std::move(args));
      return;
    }

    ImeObserver::OnFocus(context);
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

    std::unique_ptr<base::ListValue> args(
        input_ime::OnAssistiveWindowButtonClicked::Create(details));
    DispatchEventToExtension(
        extensions::events::INPUT_IME_ON_ASSISTIVE_WINDOW_BUTTON_CLICKED,
        input_ime::OnAssistiveWindowButtonClicked::kEventName, std::move(args));
  }

  void OnSuggestionsChanged(
      const std::vector<std::string>& suggestions) override {
    std::unique_ptr<base::ListValue> args(
        input_method_private::OnSuggestionsChanged::Create(suggestions));
    DispatchEventToExtension(
        extensions::events::INPUT_IME_ON_SUGGESTIONS_CHANGED,
        input_method_private::OnSuggestionsChanged::kEventName,
        std::move(args));
  }

  void OnInputMethodOptionsChanged(const std::string& engine_id) override {
    std::unique_ptr<base::ListValue> args(
        input_method_private::OnInputMethodOptionsChanged::Create(engine_id));
    DispatchEventToExtension(
        extensions::events::INPUT_IME_ON_INPUT_METHOD_OPTIONS_CHANGED,
        input_method_private::OnInputMethodOptionsChanged::kEventName,
        std::move(args));
  }

 private:
  // ui::ImeObserver overrides.
  void DispatchEventToExtension(
      extensions::events::HistogramValue histogram_value,
      const std::string& event_name,
      std::unique_ptr<base::ListValue> args) override {
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
          extension_registry->GetExtensionById(
              extension_id_, extensions::ExtensionRegistry::ENABLED);
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
    extensions::EventRouter::Get(profile_)
        ->DispatchEventToExtension(extension_id_, std::move(event));
  }

  // The component IME extensions need to know the current screen type (e.g.
  // lock screen, login screen, etc.) so that its on-screen keyboard page
  // won't open new windows/pages. See crbug.com/395621.
  std::string GetCurrentScreenType() override {
    switch (chromeos::input_method::InputMethodManager::Get()
                ->GetUISessionState()) {
      case chromeos::input_method::InputMethodManager::STATE_LOGIN_SCREEN:
        return "login";
      case chromeos::input_method::InputMethodManager::STATE_LOCK_SCREEN:
        return "lock";
      case chromeos::input_method::InputMethodManager::
          STATE_SECONDARY_LOGIN_SCREEN:
        return "secondary-login";
      case chromeos::input_method::InputMethodManager::STATE_BROWSER_SCREEN:
      case chromeos::input_method::InputMethodManager::STATE_TERMINATING:
        return "normal";
    }
    NOTREACHED() << "New screen type is added. Please add new entry above.";
    return "normal";
  }

  std::string ConvertInputContextFocusReason(
      ui::IMEEngineHandlerInterface::InputContext input_context) {
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

  bool ConvertInputContextAutoCorrect(
      ui::IMEEngineHandlerInterface::InputContext input_context) override {
    if (!GetKeyboardConfig().auto_correct)
      return false;
    return ImeObserver::ConvertInputContextAutoCorrect(input_context);
  }

  bool ConvertInputContextAutoComplete(
      ui::IMEEngineHandlerInterface::InputContext input_context) override {
    if (!GetKeyboardConfig().auto_complete)
      return false;
    return ImeObserver::ConvertInputContextAutoComplete(input_context);
  }

  input_method_private::AutoCapitalizeType ConvertInputContextAutoCapitalize(
      int flags) {
    if (!GetKeyboardConfig().auto_capitalize)
      return input_method_private::AUTO_CAPITALIZE_TYPE_OFF;
    if (flags & ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_NONE)
      return input_method_private::AUTO_CAPITALIZE_TYPE_OFF;
    if (flags & ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_CHARACTERS)
      return input_method_private::AUTO_CAPITALIZE_TYPE_CHARACTERS;
    if (flags & ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_WORDS)
      return input_method_private::AUTO_CAPITALIZE_TYPE_WORDS;
    if (flags & ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_SENTENCES)
      return input_method_private::AUTO_CAPITALIZE_TYPE_SENTENCES;

    // Autocapitalize flag may be missing for native text fields.
    // See https://crbug.com/1002713.
    return input_method_private::AUTO_CAPITALIZE_TYPE_NONE;
  }

  bool ConvertInputContextSpellCheck(
      ui::IMEEngineHandlerInterface::InputContext input_context) override {
    if (!GetKeyboardConfig().spell_check)
      return false;
    return ImeObserver::ConvertInputContextSpellCheck(input_context);
  }

  bool ConvertHasBeenPassword(
      ui::IMEEngineHandlerInterface::InputContext input_context) {
    return input_context.flags & ui::TEXT_INPUT_FLAG_HAS_BEEN_PASSWORD;
  }

  std::string ConvertInputContextMode(
      ui::IMEEngineHandlerInterface::InputContext input_context) {
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

  DISALLOW_COPY_AND_ASSIGN(ImeObserverChromeOS);
};

}  // namespace

namespace extensions {

InputMethodEngine* GetEngineIfActive(Profile* profile,
                                     const std::string& extension_id,
                                     std::string* error) {
  InputImeEventRouter* event_router = GetInputImeEventRouter(profile);
  DCHECK(event_router) << kErrorRouterNotAvailable;
  InputMethodEngine* engine = static_cast<InputMethodEngine*>(
      event_router->GetEngineIfActive(extension_id, error));
  return engine;
}

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
    : InputImeEventRouterBase(profile) {}

InputImeEventRouter::~InputImeEventRouter() = default;

bool InputImeEventRouter::RegisterImeExtension(
    const std::string& extension_id,
    const std::vector<InputComponentInfo>& input_components) {
  VLOG(1) << "RegisterImeExtension: " << extension_id;

  if (engine_map_[extension_id])
    return false;

  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  chromeos::ComponentExtensionIMEManager* comp_ext_ime_manager =
      manager->GetComponentExtensionIMEManager();

  chromeos::input_method::InputMethodDescriptors descriptors;
  // Only creates descriptors for 3rd party IME extension, because the
  // descriptors for component IME extensions are managed by InputMethodUtil.
  if (!comp_ext_ime_manager->IsAllowlistedExtension(extension_id)) {
    for (const auto& component : input_components) {
      DCHECK(component.type == INPUT_COMPONENT_TYPE_IME);

      std::vector<std::string> layouts;
      layouts.assign(component.layouts.begin(), component.layouts.end());
      std::vector<std::string> languages;
      languages.assign(component.languages.begin(), component.languages.end());

      const std::string& input_method_id =
          chromeos::extension_ime_util::GetInputMethodID(extension_id,
                                                         component.id);
      descriptors.push_back(chromeos::input_method::InputMethodDescriptor(
          input_method_id,
          component.name,
          std::string(),  // TODO(uekawa): Set short name.
          layouts,
          languages,
          false,  // 3rd party IMEs are always not for login.
          component.options_page_url,
          component.input_view_url));
    }
  }

  Profile* profile = GetProfile();

  if (chromeos::input_method::InputMethodManager::Get()->GetUISessionState() ==
          chromeos::input_method::InputMethodManager::STATE_LOGIN_SCREEN &&
      profile->HasPrimaryOTRProfile()) {
    profile = profile->GetPrimaryOTRProfile();
  }

  auto observer = std::make_unique<ImeObserverChromeOS>(extension_id, profile);
  auto engine = extension_id == "jkghodnilhceideoidjikpgommlajknk"
                    ? std::make_unique<chromeos::NativeInputMethodEngine>()
                    : std::make_unique<chromeos::InputMethodEngine>();
  engine->Initialize(std::move(observer), extension_id.c_str(), profile);
  engine_map_[extension_id] = std::move(engine);

  chromeos::UserSessionManager::GetInstance()
      ->GetDefaultIMEState(profile)
      ->AddInputMethodExtension(extension_id, descriptors,
                                engine_map_[extension_id].get());

  return true;
}

void InputImeEventRouter::UnregisterAllImes(const std::string& extension_id) {
  auto it = engine_map_.find(extension_id);
  if (it != engine_map_.end()) {
    chromeos::input_method::InputMethodManager::Get()
        ->GetActiveIMEState()
        ->RemoveInputMethodExtension(extension_id);
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

InputMethodEngineBase* InputImeEventRouter::GetEngineIfActive(
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
    return RespondNow(Error(InformativeError(error, function_name())));
  }

  std::unique_ptr<ClearComposition::Params> parent_params(
      ClearComposition::Params::Create(*args_));
  const ClearComposition::Params::Parameters& params =
      parent_params->parameters;

  bool success = engine->ClearComposition(params.context_id, &error);
  std::unique_ptr<base::ListValue> results =
      std::make_unique<base::ListValue>();
  results->Append(std::make_unique<base::Value>(success));
  return RespondNow(
      success ? ArgumentList(std::move(results))
              : ErrorWithArguments(std::move(results),
                                   InformativeError(error, function_name())));
}

ExtensionFunction::ResponseAction InputImeHideInputViewFunction::Run() {
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine)
    return RespondNow(Error(InformativeError(error, function_name())));
  engine->HideInputView();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
InputImeSetAssistiveWindowPropertiesFunction::Run() {
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine) {
    return RespondNow(Error(InformativeError(error, function_name())));
  }
  std::unique_ptr<SetAssistiveWindowProperties::Params> parent_params(
      SetAssistiveWindowProperties::Params::Create(*args_));
  const SetAssistiveWindowProperties::Params::Parameters& params =
      parent_params->parameters;
  const input_ime::AssistiveWindowProperties& window = params.properties;
  chromeos::AssistiveWindowProperties assistive_window;

  assistive_window.visible = window.visible;
  assistive_window.type = ConvertAssistiveWindowType(window.type);
  if (window.announce_string)
    assistive_window.announce_string = *window.announce_string;

  engine->SetAssistiveWindowProperties(params.context_id, assistive_window,
                                       &error);
  if (!error.empty())
    return RespondNow(Error(InformativeError(error, function_name())));
  return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
}

ExtensionFunction::ResponseAction
InputImeSetAssistiveWindowButtonHighlightedFunction::Run() {
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine) {
    return RespondNow(Error(InformativeError(error, function_name())));
  }
  std::unique_ptr<SetAssistiveWindowButtonHighlighted::Params> parent_params(
      SetAssistiveWindowButtonHighlighted::Params::Create(*args_));
  const SetAssistiveWindowButtonHighlighted::Params::Parameters& params =
      parent_params->parameters;
  ui::ime::AssistiveWindowButton button;

  button.id = ConvertAssistiveWindowButtonId(params.button_id);
  button.window_type = ConvertAssistiveWindowType(params.window_type);
  if (params.announce_string)
    button.announce_string = *params.announce_string;

  engine->SetButtonHighlighted(params.context_id, button, params.highlighted,
                               &error);
  if (!error.empty())
    return RespondNow(Error(InformativeError(error, function_name())));

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
InputImeSetCandidateWindowPropertiesFunction::Run() {
  std::unique_ptr<SetCandidateWindowProperties::Params> parent_params(
      SetCandidateWindowProperties::Params::Create(*args_));
  const SetCandidateWindowProperties::Params::Parameters&
      params = parent_params->parameters;

  std::string error;
  InputMethodEngine* engine =
      GetEngine(browser_context(), extension_id(), &error);
  if (!engine) {
    return RespondNow(Error(InformativeError(error, function_name())));
  }

  const SetCandidateWindowProperties::Params::Parameters::Properties&
      properties = params.properties;

  if (properties.visible &&
      !engine->SetCandidateWindowVisible(*properties.visible, &error)) {
    std::unique_ptr<base::ListValue> results =
        std::make_unique<base::ListValue>();
    results->Append(std::make_unique<base::Value>(false));
    return RespondNow(ErrorWithArguments(
        std::move(results), InformativeError(error, function_name())));
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

  if (properties.window_position == input_ime::WINDOW_POSITION_COMPOSITION) {
    properties_out.show_window_at_composition = true;
    modified = true;
  } else if (properties.window_position == input_ime::WINDOW_POSITION_CURSOR) {
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

  return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
}

ExtensionFunction::ResponseAction InputImeSetCandidatesFunction::Run() {
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine) {
    return RespondNow(Error(InformativeError(error, function_name())));
  }

  std::unique_ptr<SetCandidates::Params> parent_params(
      SetCandidates::Params::Create(*args_));
  const SetCandidates::Params::Parameters& params =
      parent_params->parameters;

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
  std::unique_ptr<base::ListValue> results =
      std::make_unique<base::ListValue>();
  results->Append(std::make_unique<base::Value>(success));
  return RespondNow(
      success ? ArgumentList(std::move(results))
              : ErrorWithArguments(std::move(results),
                                   InformativeError(error, function_name())));
}

ExtensionFunction::ResponseAction InputImeSetCursorPositionFunction::Run() {
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine) {
    return RespondNow(Error(InformativeError(error, function_name())));
  }

  std::unique_ptr<SetCursorPosition::Params> parent_params(
      SetCursorPosition::Params::Create(*args_));
  const SetCursorPosition::Params::Parameters& params =
      parent_params->parameters;

  bool success =
      engine->SetCursorPosition(params.context_id, params.candidate_id, &error);
  std::unique_ptr<base::ListValue> results =
      std::make_unique<base::ListValue>();
  results->Append(std::make_unique<base::Value>(success));
  return RespondNow(
      success ? ArgumentList(std::move(results))
              : ErrorWithArguments(std::move(results),
                                   InformativeError(error, function_name())));
}

ExtensionFunction::ResponseAction InputImeSetMenuItemsFunction::Run() {
  std::unique_ptr<SetMenuItems::Params> parent_params(
      SetMenuItems::Params::Create(*args_));
  const SetMenuItems::Params::Parameters& params =
      parent_params->parameters;

  std::string error;
  InputMethodEngine* engine =
      GetEngine(browser_context(), extension_id(), &error);
  if (!engine) {
    return RespondNow(Error(InformativeError(error, function_name())));
  }

  std::vector<chromeos::input_method::InputMethodManager::MenuItem> items_out;
  for (const input_ime::MenuItem& item_in : params.items) {
    items_out.emplace_back();
    SetMenuItemToMenu(item_in, &items_out.back());
  }

  if (!engine->SetMenuItems(items_out, &error)) {
    return RespondNow(Error(InformativeError(
        base::StringPrintf("%s %s", kErrorSetMenuItemsFail, error.c_str()),
        function_name())));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction InputImeUpdateMenuItemsFunction::Run() {
  std::unique_ptr<UpdateMenuItems::Params> parent_params(
      UpdateMenuItems::Params::Create(*args_));
  const UpdateMenuItems::Params::Parameters& params =
      parent_params->parameters;

  std::string error;
  InputMethodEngine* engine =
      GetEngine(browser_context(), extension_id(), &error);
  if (!engine) {
    return RespondNow(Error(InformativeError(error, function_name())));
  }

  std::vector<chromeos::input_method::InputMethodManager::MenuItem> items_out;
  for (const input_ime::MenuItem& item_in : params.items) {
    items_out.emplace_back();
    SetMenuItemToMenu(item_in, &items_out.back());
  }

  if (!engine->UpdateMenuItems(items_out, &error)) {
    return RespondNow(Error(InformativeError(
        base::StringPrintf("%s %s", kErrorUpdateMenuItemsFail, error.c_str()),
        function_name())));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction InputImeDeleteSurroundingTextFunction::Run() {
  std::unique_ptr<DeleteSurroundingText::Params> parent_params(
      DeleteSurroundingText::Params::Create(*args_));
  const DeleteSurroundingText::Params::Parameters& params =
      parent_params->parameters;

  std::string error;
  InputMethodEngine* engine =
      GetEngine(browser_context(), extension_id(), &error);
  if (!engine) {
    return RespondNow(Error(InformativeError(error, function_name())));
  }

  engine->DeleteSurroundingText(params.context_id, params.offset, params.length,
                                &error);
  return RespondNow(error.empty()
                        ? NoArguments()
                        : Error(InformativeError(error, function_name())));
}

ExtensionFunction::ResponseAction
InputMethodPrivateFinishComposingTextFunction::Run() {
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine)
    return RespondNow(Error(InformativeError(error, function_name())));
  std::unique_ptr<FinishComposingText::Params> parent_params(
      FinishComposingText::Params::Create(*args_));
  const FinishComposingText::Params::Parameters& params =
      parent_params->parameters;
  engine->FinishComposingText(params.context_id, &error);
  return RespondNow(error.empty()
                        ? NoArguments()
                        : Error(InformativeError(error, function_name())));
}

ExtensionFunction::ResponseAction
InputMethodPrivateNotifyImeMenuItemActivatedFunction::Run() {
  chromeos::input_method::InputMethodDescriptor current_input_method =
      chromeos::input_method::InputMethodManager::Get()
          ->GetActiveIMEState()
          ->GetCurrentInputMethod();
  std::string active_extension_id =
      chromeos::extension_ime_util::GetExtensionIDFromInputMethodID(
          current_input_method.id());
  std::string error;
  InputMethodEngine* engine =
      GetEngineIfActive(Profile::FromBrowserContext(browser_context()),
                        active_extension_id, &error);
  if (!engine)
    return RespondNow(Error(InformativeError(error, function_name())));

  std::unique_ptr<NotifyImeMenuItemActivated::Params> params(
      NotifyImeMenuItemActivated::Params::Create(*args_));
  if (params->engine_id != engine->GetActiveComponentId())
    return RespondNow(
        Error(InformativeError(kErrorEngineNotActive, function_name())));
  engine->PropertyActivate(params->name);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
InputMethodPrivateGetCompositionBoundsFunction::Run() {
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine)
    return RespondNow(Error(InformativeError(error, function_name())));

  auto bounds_list = std::make_unique<base::ListValue>();
  for (const auto& bounds : engine->composition_bounds()) {
    auto bounds_value = std::make_unique<base::DictionaryValue>();
    bounds_value->SetInteger("x", bounds.x());
    bounds_value->SetInteger("y", bounds.y());
    bounds_value->SetInteger("w", bounds.width());
    bounds_value->SetInteger("h", bounds.height());
    bounds_list->Append(std::move(bounds_value));
  }

  return RespondNow(OneArgument(std::move(bounds_list)));
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
      InputMethodEngineBase* engine =
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
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  chromeos::ComponentExtensionIMEManager* comp_ext_ime_manager =
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
  if (!details.browser_context)
    return;
  std::string error;
  InputMethodEngine* engine =
      GetEngineIfActive(Profile::FromBrowserContext(details.browser_context),
                        details.extension_id, &error);
  // Notifies the IME extension for IME ready with onActivate/onFocus events.
  if (engine)
    engine->Enable(engine->GetActiveComponentId());
}

}  // namespace extensions

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/chrome_keyboard_controller_client.h"
#include "chrome/common/extensions/api/input_ime.h"
#include "chrome/common/extensions/api/input_method_private.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/ime_engine_handler_interface.h"
#include "ui/base/ui_base_features.h"
#include "ui/keyboard/keyboard_controller.h"

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
namespace ClearComposition = extensions::api::input_ime::ClearComposition;
namespace OnCompositionBoundsChanged =
    extensions::api::input_method_private::OnCompositionBoundsChanged;
namespace NotifyImeMenuItemActivated =
    extensions::api::input_method_private::NotifyImeMenuItemActivated;
using ui::IMEEngineHandlerInterface;
using input_method::InputMethodEngineBase;
using chromeos::InputMethodEngine;

namespace {
const char kInputImeApiChromeOSErrorEngineNotAvailable[] =
    "Engine is not available";
const char kInputImeApiChromeOSErrorSetMenuItemsFail[] =
    "Could not create menu Items";
const char kInputImeApiChromeOSErrorUpdateMenuItemsFail[] =
    "Could not update menu Items";
const char kInputImeApiChromeOSErrorEngineNotActive[] =
    "The engine is not active.";

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

keyboard::mojom::KeyboardConfig GetKeyboardConfig() {
  return ChromeKeyboardControllerClient::Get()->GetKeyboardConfig();
}

class ImeObserverChromeOS : public ui::ImeObserver {
 public:
  ImeObserverChromeOS(const std::string& extension_id, Profile* profile)
      : ImeObserver(extension_id, profile) {}

  ~ImeObserverChromeOS() override {}

  // input_method::InputMethodEngineBase::Observer overrides.
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

  void OnCompositionBoundsChanged(
      const std::vector<gfx::Rect>& bounds) override {
    if (extension_id_.empty() ||
        !HasListener(OnCompositionBoundsChanged::kEventName))
      return;

    // Note: this is a private API event.
    auto bounds_list = std::make_unique<base::ListValue>();
    for (size_t i = 0; i < bounds.size(); ++i) {
      auto bounds_value = std::make_unique<base::DictionaryValue>();
      bounds_value->SetInteger("x", bounds[i].x());
      bounds_value->SetInteger("y", bounds[i].y());
      bounds_value->SetInteger("w", bounds[i].width());
      bounds_value->SetInteger("h", bounds[i].height());
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
    if (ExtensionHasListener(input_method_private::OnFocus::kEventName) &&
        base::FeatureList::IsEnabled(features::kEnableStylusVirtualKeyboard)) {
      input_method_private::InputContext input_context;
      input_context.context_id = context.id;
      input_context.type = input_method_private::ParseInputContextType(
          ConvertInputContextType(context));
      input_context.auto_correct = ConvertInputContextAutoCorrect(context);
      input_context.auto_complete = ConvertInputContextAutoComplete(context);
      input_context.auto_capitalize = (input_method_private::AutoCapitalizeType)
          ConvertInputContextAutoCapitalize(context);
      input_context.spell_check = ConvertInputContextSpellCheck(context);
      input_context.has_been_password = ConvertHasBeenPassword(context);
      input_context.should_do_learning = context.should_do_learning;
      input_context.focus_reason = input_method_private::ParseFocusReason(
          ConvertInputContextFocusReason(context));

      std::unique_ptr<base::ListValue> args(
          input_method_private::OnFocus::Create(input_context));

      DispatchEventToExtension(
          extensions::events::INPUT_METHOD_PRIVATE_ON_FOCUS,
          input_method_private::OnFocus::kEventName, std::move(args));
      return;
    }

    ImeObserver::OnFocus(context);
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
    extensions::ExtensionSystem* extension_system =
        extensions::ExtensionSystem::Get(profile_);
    if (extension_system) {
      const extensions::Extension* extension =
          extension_system->extension_service()->GetExtensionById(
              extension_id_, false /* include_disabled */);
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

  input_ime::AutoCapitalizeType ConvertInputContextAutoCapitalize(
      ui::IMEEngineHandlerInterface::InputContext input_context) override {
    if (!GetKeyboardConfig().auto_capitalize)
      return input_ime::AUTO_CAPITALIZE_TYPE_NONE;
    return ImeObserver::ConvertInputContextAutoCapitalize(input_context);
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

  DISALLOW_COPY_AND_ASSIGN(ImeObserverChromeOS);
};

}  // namespace

namespace extensions {

InputMethodEngine* GetActiveEngine(Profile* profile,
                                   const std::string& extension_id) {
  InputImeEventRouter* event_router = GetInputImeEventRouter(profile);
  InputMethodEngine* engine =
      event_router ? static_cast<InputMethodEngine*>(
                         event_router->GetActiveEngine(extension_id))
                   : nullptr;
  return engine;
}

InputImeEventRouter::InputImeEventRouter(Profile* profile)
    : InputImeEventRouterBase(profile) {}

InputImeEventRouter::~InputImeEventRouter() {}

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
  if (!comp_ext_ime_manager->IsWhitelistedExtension(extension_id)) {
    for (std::vector<InputComponentInfo>::const_iterator it =
             input_components.begin();
         it != input_components.end();
         ++it) {
      const InputComponentInfo& component = *it;
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
      profile->HasOffTheRecordProfile()) {
    profile = profile->GetOffTheRecordProfile();
  }

  std::unique_ptr<InputMethodEngineBase::Observer> observer(
      new ImeObserverChromeOS(extension_id, profile));
  chromeos::InputMethodEngine* engine = new chromeos::InputMethodEngine();
  engine->Initialize(std::move(observer), extension_id.c_str(), profile);
  engine_map_[extension_id] = engine;
  chromeos::UserSessionManager::GetInstance()
      ->GetDefaultIMEState(profile)
      ->AddInputMethodExtension(extension_id, descriptors, engine);

  return true;
}

void InputImeEventRouter::UnregisterAllImes(const std::string& extension_id) {
  std::map<std::string, InputMethodEngine*>::iterator it =
      engine_map_.find(extension_id);
  if (it != engine_map_.end()) {
    chromeos::input_method::InputMethodManager::Get()
        ->GetActiveIMEState()
        ->RemoveInputMethodExtension(extension_id);
    delete it->second;
    engine_map_.erase(it);
  }
}

InputMethodEngine* InputImeEventRouter::GetEngine(
    const std::string& extension_id) {
  std::map<std::string, InputMethodEngine*>::iterator it =
      engine_map_.find(extension_id);
  return (it != engine_map_.end()) ? it->second : nullptr;
}

InputMethodEngineBase* InputImeEventRouter::GetActiveEngine(
    const std::string& extension_id) {
  std::map<std::string, InputMethodEngine*>::iterator it =
      engine_map_.find(extension_id);
  return (it != engine_map_.end() && it->second->IsActive()) ? it->second
                                                             : nullptr;
}

ExtensionFunction::ResponseAction InputImeClearCompositionFunction::Run() {
  InputMethodEngine* engine = GetActiveEngine(
      Profile::FromBrowserContext(browser_context()), extension_id());
  if (!engine) {
    return RespondNow(OneArgument(std::make_unique<base::Value>(false)));
  }

  std::unique_ptr<ClearComposition::Params> parent_params(
      ClearComposition::Params::Create(*args_));
  const ClearComposition::Params::Parameters& params =
      parent_params->parameters;

  std::string error;
  bool success = engine->ClearComposition(params.context_id, &error);
  std::unique_ptr<base::ListValue> results =
      std::make_unique<base::ListValue>();
  results->Append(std::make_unique<base::Value>(success));
  return RespondNow(success ? ArgumentList(std::move(results))
                            : ErrorWithArguments(std::move(results), error));
}

ExtensionFunction::ResponseAction InputImeHideInputViewFunction::Run() {
  InputMethodEngine* engine = GetActiveEngine(
      Profile::FromBrowserContext(browser_context()), extension_id());
  if (!engine)
    return RespondNow(NoArguments());
  engine->HideInputView();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
InputImeSetCandidateWindowPropertiesFunction::Run() {
  std::unique_ptr<SetCandidateWindowProperties::Params> parent_params(
      SetCandidateWindowProperties::Params::Create(*args_));
  const SetCandidateWindowProperties::Params::Parameters&
      params = parent_params->parameters;

  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()));
  InputMethodEngine* engine =
      event_router ? event_router->GetEngine(extension_id()) : nullptr;
  if (!engine) {
    return RespondNow(OneArgument(std::make_unique<base::Value>(false)));
  }

  const SetCandidateWindowProperties::Params::Parameters::Properties&
      properties = params.properties;

  std::string error;
  if (properties.visible &&
      !engine->SetCandidateWindowVisible(*properties.visible, &error)) {
    std::unique_ptr<base::ListValue> results =
        std::make_unique<base::ListValue>();
    results->Append(std::make_unique<base::Value>(false));
    return RespondNow(ErrorWithArguments(std::move(results), error));
  }

  InputMethodEngine::CandidateWindowProperty properties_out =
      engine->GetCandidateWindowProperty();
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

  if (modified) {
    engine->SetCandidateWindowProperty(properties_out);
  }

  return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
}

ExtensionFunction::ResponseAction InputImeSetCandidatesFunction::Run() {
  InputMethodEngine* engine = GetActiveEngine(
      Profile::FromBrowserContext(browser_context()), extension_id());
  if (!engine) {
    return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
  }

  std::unique_ptr<SetCandidates::Params> parent_params(
      SetCandidates::Params::Create(*args_));
  const SetCandidates::Params::Parameters& params =
      parent_params->parameters;

  std::vector<InputMethodEngine::Candidate> candidates_out;
  for (const auto& candidate_in : params.candidates) {
    candidates_out.push_back(InputMethodEngine::Candidate());
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

  std::string error;
  bool success =
      engine->SetCandidates(params.context_id, candidates_out, &error);
  std::unique_ptr<base::ListValue> results =
      std::make_unique<base::ListValue>();
  results->Append(std::make_unique<base::Value>(success));
  return RespondNow(success ? ArgumentList(std::move(results))
                            : ErrorWithArguments(std::move(results), error));
}

ExtensionFunction::ResponseAction InputImeSetCursorPositionFunction::Run() {
  InputMethodEngine* engine = GetActiveEngine(
      Profile::FromBrowserContext(browser_context()), extension_id());
  if (!engine) {
    return RespondNow(OneArgument(std::make_unique<base::Value>(false)));
  }

  std::unique_ptr<SetCursorPosition::Params> parent_params(
      SetCursorPosition::Params::Create(*args_));
  const SetCursorPosition::Params::Parameters& params =
      parent_params->parameters;

  std::string error;
  bool success =
      engine->SetCursorPosition(params.context_id, params.candidate_id, &error);
  std::unique_ptr<base::ListValue> results =
      std::make_unique<base::ListValue>();
  results->Append(std::make_unique<base::Value>(success));
  return RespondNow(success ? ArgumentList(std::move(results))
                            : ErrorWithArguments(std::move(results), error));
}

ExtensionFunction::ResponseAction InputImeSetMenuItemsFunction::Run() {
  std::unique_ptr<SetMenuItems::Params> parent_params(
      SetMenuItems::Params::Create(*args_));
  const SetMenuItems::Params::Parameters& params =
      parent_params->parameters;

  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()));
  InputMethodEngine* engine =
      event_router ? event_router->GetEngine(extension_id()) : nullptr;
  if (!engine)
    return RespondNow(Error(kInputImeApiChromeOSErrorEngineNotAvailable));

  std::vector<chromeos::input_method::InputMethodManager::MenuItem> items_out;
  for (const input_ime::MenuItem& item_in : params.items) {
    items_out.push_back(chromeos::input_method::InputMethodManager::MenuItem());
    SetMenuItemToMenu(item_in, &items_out.back());
  }

  if (!engine->SetMenuItems(items_out))
    return RespondNow(Error(kInputImeApiChromeOSErrorSetMenuItemsFail));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction InputImeUpdateMenuItemsFunction::Run() {
  std::unique_ptr<UpdateMenuItems::Params> parent_params(
      UpdateMenuItems::Params::Create(*args_));
  const UpdateMenuItems::Params::Parameters& params =
      parent_params->parameters;

  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()));
  InputMethodEngine* engine =
      event_router ? event_router->GetEngine(extension_id()) : nullptr;
  if (!engine)
    return RespondNow(Error(kInputImeApiChromeOSErrorEngineNotAvailable));

  std::vector<chromeos::input_method::InputMethodManager::MenuItem> items_out;
  for (const input_ime::MenuItem& item_in : params.items) {
    items_out.push_back(chromeos::input_method::InputMethodManager::MenuItem());
    SetMenuItemToMenu(item_in, &items_out.back());
  }

  if (!engine->UpdateMenuItems(items_out))
    return RespondNow(Error(kInputImeApiChromeOSErrorUpdateMenuItemsFail));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction InputImeDeleteSurroundingTextFunction::Run() {
  std::unique_ptr<DeleteSurroundingText::Params> parent_params(
      DeleteSurroundingText::Params::Create(*args_));
  const DeleteSurroundingText::Params::Parameters& params =
      parent_params->parameters;

  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()));
  InputMethodEngine* engine =
      event_router ? event_router->GetEngine(extension_id()) : nullptr;
  if (!engine)
    return RespondNow(Error(kInputImeApiChromeOSErrorEngineNotAvailable));

  std::string error;
  engine->DeleteSurroundingText(params.context_id, params.offset, params.length,
                                &error);
  return RespondNow(error.empty() ? NoArguments() : Error(error));
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
  InputMethodEngine* engine = GetActiveEngine(
      Profile::FromBrowserContext(browser_context()), active_extension_id);
  if (!engine)
    return RespondNow(Error(kInputImeApiChromeOSErrorEngineNotAvailable));

  std::unique_ptr<NotifyImeMenuItemActivated::Params> params(
      NotifyImeMenuItemActivated::Params::Create(*args_));
  if (params->engine_id != engine->GetActiveComponentId())
    return RespondNow(Error(kInputImeApiChromeOSErrorEngineNotActive));
  engine->PropertyActivate(params->name);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
InputMethodPrivateGetCompositionBoundsFunction::Run() {
  InputMethodEngine* engine = GetActiveEngine(
      Profile::FromBrowserContext(browser_context()), extension_id());
  if (!engine)
    return RespondNow(Error(kInputImeApiChromeOSErrorEngineNotAvailable));

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
      InputMethodEngineBase* engine =
          event_router->GetActiveEngine(extension->id());
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

  if (comp_ext_ime_manager->IsWhitelistedExtension(extension->id())) {
    // Since the first party ime is not allow to uninstall, and when it's
    // unloaded unexpectedly, OS will recover the extension at once.
    // So should not unregister the IMEs. Otherwise the IME icons on the
    // desktop shelf will disappear. see bugs: 775507,788247,786273,761714.
    // But still need to unload keyboard container document. Since ime extension
    // need to re-render the document when it's recovered.
    auto* keyboard_controller = keyboard::KeyboardController::Get();
    if (keyboard_controller->IsEnabled()) {
      // Keyboard controller "Reload" method only reload current page when the
      // url is changed. So we need unload the current page first. Then next
      // engine->Enable() can refresh the inputview page correctly.
      // Empties the content url and reload the controller to unload the
      // current page.
      // TODO(wuyingbing): Should add a new method to unload the document.
      manager->GetActiveIMEState()->DisableInputView();
      keyboard_controller->Reload();
    }
    event_router->SetUnloadedExtensionId(extension->id());
  } else {
    event_router->UnregisterAllImes(extension->id());
  }
}

void InputImeAPI::OnListenerAdded(const EventListenerInfo& details) {
  if (!details.browser_context)
    return;
  InputMethodEngine* engine =
      GetActiveEngine(Profile::FromBrowserContext(details.browser_context),
                      details.extension_id);
  // Notifies the IME extension for IME ready with onActivate/onFocus events.
  if (engine)
    engine->Enable(engine->GetActiveComponentId());
}

}  // namespace extensions

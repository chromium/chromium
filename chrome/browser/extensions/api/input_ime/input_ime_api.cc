// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"

#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_registry.h"
#include "ui/base/ime/ime_bridge.h"

namespace input_ime = extensions::api::input_ime;
namespace KeyEventHandled = extensions::api::input_ime::KeyEventHandled;
namespace SetComposition = extensions::api::input_ime::SetComposition;
namespace CommitText = extensions::api::input_ime::CommitText;
namespace SendKeyEvents = extensions::api::input_ime::SendKeyEvents;
using ui::IMEEngineHandlerInterface;
using input_method::InputMethodEngineBase;

namespace {
const char kInputImeApiErrorEngineNotAvailable[] = "Engine is not available";
const char kInputImeApiErrorSetKeyEventsFail[] = "Could not send key events";
}
namespace ui {

ImeObserver::ImeObserver(const std::string& extension_id, Profile* profile)
    : extension_id_(extension_id), profile_(profile) {}

void ImeObserver::OnActivate(const std::string& component_id) {
  // Don't check whether the extension listens on onActivate event here.
  // Send onActivate event to give the IME a chance to add their listeners.
  if (extension_id_.empty())
    return;

  std::unique_ptr<base::ListValue> args(input_ime::OnActivate::Create(
      component_id, input_ime::ParseScreenType(GetCurrentScreenType())));

  DispatchEventToExtension(extensions::events::INPUT_IME_ON_ACTIVATE,
                           input_ime::OnActivate::kEventName,
                           std::move(args));
}

void ImeObserver::OnFocus(
    const IMEEngineHandlerInterface::InputContext& context) {
  if (extension_id_.empty() || !HasListener(input_ime::OnFocus::kEventName))
    return;

  input_ime::InputContext context_value;
  context_value.context_id = context.id;
  context_value.type =
      input_ime::ParseInputContextType(ConvertInputContextType(context));
  context_value.auto_correct = ConvertInputContextAutoCorrect(context);
  context_value.auto_complete = ConvertInputContextAutoComplete(context);
  context_value.auto_capitalize = ConvertInputContextAutoCapitalize(context);
  context_value.spell_check = ConvertInputContextSpellCheck(context);
  context_value.should_do_learning = context.should_do_learning;

  std::unique_ptr<base::ListValue> args(
      input_ime::OnFocus::Create(context_value));

  DispatchEventToExtension(extensions::events::INPUT_IME_ON_FOCUS,
                           input_ime::OnFocus::kEventName, std::move(args));
}

void ImeObserver::OnBlur(int context_id) {
  if (extension_id_.empty() || !HasListener(input_ime::OnBlur::kEventName))
    return;

  std::unique_ptr<base::ListValue> args(input_ime::OnBlur::Create(context_id));

  DispatchEventToExtension(extensions::events::INPUT_IME_ON_BLUR,
                           input_ime::OnBlur::kEventName, std::move(args));
}

void ImeObserver::OnKeyEvent(
    const std::string& component_id,
    const InputMethodEngineBase::KeyboardEvent& event,
    IMEEngineHandlerInterface::KeyEventDoneCallback key_data) {
  if (extension_id_.empty())
    return;

  // If there is no listener for the event, no need to dispatch the event to
  // extension. Instead, releases the key event for default system behavior.
  if (!ShouldForwardKeyEvent()) {
    // Continue processing the key event so that the physical keyboard can
    // still work.
    std::move(key_data).Run(false);
    return;
  }

  extensions::InputImeEventRouter* event_router =
      extensions::GetInputImeEventRouter(profile_);
  if (!event_router || !event_router->GetActiveEngine(extension_id_))
    return;
  const std::string request_id =
      event_router->GetActiveEngine(extension_id_)
          ->AddRequest(component_id, std::move(key_data));

  input_ime::KeyboardEvent key_data_value;
  key_data_value.type = input_ime::ParseKeyboardEventType(event.type);
  key_data_value.request_id = request_id;
  if (!event.extension_id.empty())
      key_data_value.extension_id.reset(new std::string(event.extension_id));
  key_data_value.key = event.key;
  key_data_value.code = event.code;
  key_data_value.alt_key.reset(new bool(event.alt_key));
  key_data_value.ctrl_key.reset(new bool(event.ctrl_key));
  key_data_value.shift_key.reset(new bool(event.shift_key));
  key_data_value.caps_lock.reset(new bool(event.caps_lock));

  std::unique_ptr<base::ListValue> args(
      input_ime::OnKeyEvent::Create(component_id, key_data_value));

  DispatchEventToExtension(extensions::events::INPUT_IME_ON_KEY_EVENT,
                           input_ime::OnKeyEvent::kEventName, std::move(args));
}

void ImeObserver::OnReset(const std::string& component_id) {
  if (extension_id_.empty() || !HasListener(input_ime::OnReset::kEventName))
    return;

  std::unique_ptr<base::ListValue> args(
      input_ime::OnReset::Create(component_id));

  DispatchEventToExtension(extensions::events::INPUT_IME_ON_RESET,
                           input_ime::OnReset::kEventName, std::move(args));
}

void ImeObserver::OnDeactivated(const std::string& component_id) {
  if (extension_id_.empty() ||
      !HasListener(input_ime::OnDeactivated::kEventName))
    return;

  std::unique_ptr<base::ListValue> args(
      input_ime::OnDeactivated::Create(component_id));

  DispatchEventToExtension(extensions::events::INPUT_IME_ON_DEACTIVATED,
                           input_ime::OnDeactivated::kEventName,
                           std::move(args));
}

// TODO(azurewei): This function implementation should be shared on all
// platforms, while with some changing on the current code on ChromeOS.
void ImeObserver::OnCompositionBoundsChanged(
    const std::vector<gfx::Rect>& bounds) {}

bool ImeObserver::IsInterestedInKeyEvent() const {
  return ShouldForwardKeyEvent();
}

void ImeObserver::OnSurroundingTextChanged(const std::string& component_id,
                                           const std::string& text,
                                           int cursor_pos,
                                           int anchor_pos,
                                           int offset_pos) {
  if (extension_id_.empty() ||
      !HasListener(input_ime::OnSurroundingTextChanged::kEventName))
    return;

  input_ime::OnSurroundingTextChanged::SurroundingInfo info;
  info.text = text;
  info.focus = cursor_pos;
  info.anchor = anchor_pos;
  info.offset = offset_pos;
  std::unique_ptr<base::ListValue> args(
      input_ime::OnSurroundingTextChanged::Create(component_id, info));

  DispatchEventToExtension(
    extensions::events::INPUT_IME_ON_SURROUNDING_TEXT_CHANGED,
    input_ime::OnSurroundingTextChanged::kEventName, std::move(args));
}

bool ImeObserver::ShouldForwardKeyEvent() const {
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
  for (const std::unique_ptr<extensions::EventListener>& listener : listeners) {
    if (listener->extension_id() == extension_id_ && !listener->IsLazy())
      return true;
  }
  return false;
}

bool ImeObserver::HasListener(const std::string& event_name) const {
  return extensions::EventRouter::Get(profile_)->HasEventListener(event_name);
}

bool ImeObserver::ExtensionHasListener(const std::string& event_name) const {
  return extensions::EventRouter::Get(profile_)->ExtensionHasEventListener(
      extension_id_, event_name);
}

std::string ImeObserver::ConvertInputContextType(
    ui::IMEEngineHandlerInterface::InputContext input_context) {
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
    default:
      input_context_type = "text";
      break;
  }
  return input_context_type;
}

bool ImeObserver::ConvertInputContextAutoCorrect(
    ui::IMEEngineHandlerInterface::InputContext input_context) {
  return !(input_context.flags & ui::TEXT_INPUT_FLAG_AUTOCORRECT_OFF);
}

bool ImeObserver::ConvertInputContextAutoComplete(
    ui::IMEEngineHandlerInterface::InputContext input_context) {
  return !(input_context.flags & ui::TEXT_INPUT_FLAG_AUTOCOMPLETE_OFF);
}

input_ime::AutoCapitalizeType ImeObserver::ConvertInputContextAutoCapitalize(
    ui::IMEEngineHandlerInterface::InputContext input_context) {
  if (input_context.flags & ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_NONE)
    return input_ime::AUTO_CAPITALIZE_TYPE_NONE;
  if (input_context.flags & ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_CHARACTERS)
    return input_ime::AUTO_CAPITALIZE_TYPE_CHARACTERS;
  if (input_context.flags & ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_WORDS)
    return input_ime::AUTO_CAPITALIZE_TYPE_WORDS;
  // The default value is "sentences".
  return input_ime::AUTO_CAPITALIZE_TYPE_SENTENCES;
}

bool ImeObserver::ConvertInputContextSpellCheck(
    ui::IMEEngineHandlerInterface::InputContext input_context) {
  return !(input_context.flags & ui::TEXT_INPUT_FLAG_SPELLCHECK_OFF);
}

}  // namespace ui

namespace extensions {

InputImeEventRouterFactory* InputImeEventRouterFactory::GetInstance() {
  return base::Singleton<InputImeEventRouterFactory>::get();
}

InputImeEventRouterFactory::InputImeEventRouterFactory() {
}

InputImeEventRouterFactory::~InputImeEventRouterFactory() {
}

InputImeEventRouter* InputImeEventRouterFactory::GetRouter(Profile* profile) {
  if (!profile)
    return nullptr;
  InputImeEventRouter* router = router_map_[profile];
  if (!router) {
    router = new InputImeEventRouter(profile);
    router_map_[profile] = router;
  }
  return router;
}

void InputImeEventRouterFactory::RemoveProfile(Profile* profile) {
  if (!profile || router_map_.empty())
    return;
  auto it = router_map_.find(profile);
  // The routers are common between an incognito profile and its original
  // profile, and are keyed on the original profiles.
  // When a profile is removed, exact matching is used to ensure that the router
  // is deleted only when the original profile is removed.
  if (it != router_map_.end() && it->first == profile) {
    delete it->second;
    router_map_.erase(it);
  }
}

ExtensionFunction::ResponseAction InputImeKeyEventHandledFunction::Run() {
  std::unique_ptr<KeyEventHandled::Params> params(
      KeyEventHandled::Params::Create(*args_));
  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()));
  InputMethodEngineBase* engine =
      event_router ? event_router->GetActiveEngine(extension_id()) : nullptr;
  if (engine) {
    engine->KeyEventHandled(extension_id(), params->request_id,
                            params->response);
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction InputImeSetCompositionFunction::Run() {
  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()));
  InputMethodEngineBase* engine =
      event_router ? event_router->GetActiveEngine(extension_id()) : nullptr;
  if (engine) {
    std::unique_ptr<SetComposition::Params> parent_params(
        SetComposition::Params::Create(*args_));
    const SetComposition::Params::Parameters& params =
        parent_params->parameters;
    std::vector<InputMethodEngineBase::SegmentInfo> segments;
    if (params.segments) {
      for (const auto& segments_arg : *params.segments) {
        EXTENSION_FUNCTION_VALIDATE(segments_arg.style !=
                                    input_ime::UNDERLINE_STYLE_NONE);
        InputMethodEngineBase::SegmentInfo segment_info;
        segment_info.start = segments_arg.start;
        segment_info.end = segments_arg.end;
        if (segments_arg.style == input_ime::UNDERLINE_STYLE_UNDERLINE) {
          segment_info.style = InputMethodEngineBase::SEGMENT_STYLE_UNDERLINE;
        } else if (segments_arg.style ==
                   input_ime::UNDERLINE_STYLE_DOUBLEUNDERLINE) {
          segment_info.style =
              InputMethodEngineBase::SEGMENT_STYLE_DOUBLE_UNDERLINE;
        } else {
          segment_info.style =
              InputMethodEngineBase::SEGMENT_STYLE_NO_UNDERLINE;
        }
        segments.push_back(segment_info);
      }
    }
    int selection_start =
        params.selection_start ? *params.selection_start : params.cursor;
    int selection_end =
        params.selection_end ? *params.selection_end : params.cursor;
    std::string error;
    if (!engine->SetComposition(params.context_id, params.text.c_str(),
                                selection_start, selection_end, params.cursor,
                                segments, &error)) {
      std::unique_ptr<base::ListValue> results =
          std::make_unique<base::ListValue>();
      results->Append(std::make_unique<base::Value>(false));
      return RespondNow(ErrorWithArguments(std::move(results), error));
    }
  }
  return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
}

ExtensionFunction::ResponseAction InputImeCommitTextFunction::Run() {
  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()));
  InputMethodEngineBase* engine =
      event_router ? event_router->GetActiveEngine(extension_id()) : nullptr;
  if (engine) {
    std::unique_ptr<CommitText::Params> parent_params(
        CommitText::Params::Create(*args_));
    const CommitText::Params::Parameters& params = parent_params->parameters;
    std::string error;
    if (!engine->CommitText(params.context_id, params.text.c_str(), &error)) {
      std::unique_ptr<base::ListValue> results =
          std::make_unique<base::ListValue>();
      results->Append(std::make_unique<base::Value>(false));
      return RespondNow(ErrorWithArguments(std::move(results), error));
    }
  }
  return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
}

ExtensionFunction::ResponseAction InputImeSendKeyEventsFunction::Run() {
  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()));
  InputMethodEngineBase* engine =
      event_router ? event_router->GetActiveEngine(extension_id()) : nullptr;
  if (!engine)
    return RespondNow(Error(kInputImeApiErrorEngineNotAvailable));

  std::unique_ptr<SendKeyEvents::Params> parent_params(
      SendKeyEvents::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(parent_params);
  const SendKeyEvents::Params::Parameters& params = parent_params->parameters;
  std::vector<InputMethodEngineBase::KeyboardEvent> key_data_out;

  for (const auto& key_event : params.key_data) {
    key_data_out.push_back(InputMethodEngineBase::KeyboardEvent());
    InputMethodEngineBase::KeyboardEvent& event = key_data_out.back();
    event.type = input_ime::ToString(key_event.type);
    event.key = key_event.key;
    event.code = key_event.code;
    event.key_code = key_event.key_code.get() ? *(key_event.key_code) : 0;
    event.alt_key = key_event.alt_key ? *(key_event.alt_key) : false;
    event.ctrl_key = key_event.ctrl_key ? *(key_event.ctrl_key) : false;
    event.shift_key = key_event.shift_key ? *(key_event.shift_key) : false;
    event.caps_lock = key_event.caps_lock ? *(key_event.caps_lock) : false;
  }
  if (!engine->SendKeyEvents(params.context_id, key_data_out))
    return RespondNow(Error(kInputImeApiErrorSetKeyEventsFail));
  return RespondNow(NoArguments());
}

InputImeAPI::InputImeAPI(content::BrowserContext* context)
    : browser_context_(context), extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));

  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, input_ime::OnFocus::kEventName);
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllSources());
}

void InputImeAPI::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PROFILE_DESTROYED, type);
  extensions::InputImeEventRouterFactory::GetInstance()->RemoveProfile(
      content::Source<Profile>(source).ptr());
}

InputImeAPI::~InputImeAPI() = default;

void InputImeAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
  registrar_.RemoveAll();
  if (observer_ && ui::IMEBridge::Get()) {
    ui::IMEBridge::Get()->SetObserver(nullptr);
  }
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<InputImeAPI>>::
    DestructorAtExit g_input_ime_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<InputImeAPI>* InputImeAPI::GetFactoryInstance() {
  return g_input_ime_factory.Pointer();
}

InputImeEventRouter* GetInputImeEventRouter(Profile* profile) {
  if (!profile)
    return nullptr;
  return extensions::InputImeEventRouterFactory::GetInstance()->GetRouter(
      profile->GetOriginalProfile());
}

}  // namespace extensions

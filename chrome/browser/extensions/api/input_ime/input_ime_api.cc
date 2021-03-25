// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"

#include <memory>
#include <utility>
#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/browser/extension_registry.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/ime_keymap.h"
#include "ui/base/ime/constants.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace input_ime = extensions::api::input_ime;
namespace KeyEventHandled = extensions::api::input_ime::KeyEventHandled;
namespace SetComposition = extensions::api::input_ime::SetComposition;
namespace CommitText = extensions::api::input_ime::CommitText;
namespace SendKeyEvents = extensions::api::input_ime::SendKeyEvents;

namespace {
const char kErrorRouterNotAvailable[] = "The router is not available.";
const char kErrorSetKeyEventsFail[] = "Could not send key events.";

using chromeos::InputMethodEngine;
using chromeos::InputMethodEngineBase;

InputMethodEngine* GetEngineIfActive(Profile* profile,
                                     const std::string& extension_id,
                                     std::string* error) {
  extensions::InputImeEventRouter* event_router =
      extensions::GetInputImeEventRouter(profile);
  DCHECK(event_router) << kErrorRouterNotAvailable;
  InputMethodEngine* engine =
      event_router->GetEngineIfActive(extension_id, error);
  return engine;
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
    case ui::VKEY_MEDIA_LAUNCH_APP2:
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

ui::KeyEvent ConvertKeyboardEventToUIKeyEvent(
    const input_ime::KeyboardEvent& event) {
  const ui::EventType type =
      event.type == input_ime::KEYBOARD_EVENT_TYPE_KEYDOWN
          ? ui::ET_KEY_PRESSED
          : ui::ET_KEY_RELEASED;

  const auto key_code = static_cast<ui::KeyboardCode>(
      event.key_code && *event.key_code != ui::VKEY_UNKNOWN
          ? *event.key_code
          : ui::DomKeycodeToKeyboardCode(event.code));

  int flags = ui::EF_NONE;
  flags |= event.alt_key && *event.alt_key ? ui::EF_ALT_DOWN : ui::EF_NONE;
  flags |=
      event.altgr_key && *event.altgr_key ? ui::EF_ALTGR_DOWN : ui::EF_NONE;
  flags |=
      event.ctrl_key && *event.ctrl_key ? ui::EF_CONTROL_DOWN : ui::EF_NONE;
  flags |=
      event.shift_key && *event.shift_key ? ui::EF_SHIFT_DOWN : ui::EF_NONE;
  flags |=
      event.caps_lock && *event.caps_lock ? ui::EF_CAPS_LOCK_ON : ui::EF_NONE;

  return ui::KeyEvent(type, key_code,
                      ui::KeycodeConverter::CodeStringToDomCode(event.code),
                      flags, ui::KeycodeConverter::KeyStringToDomKey(event.key),
                      ui::EventTimeForNow());
}

}  // namespace

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
    int context_id,
    const IMEEngineHandlerInterface::InputContext& context) {
  if (extension_id_.empty() || !HasListener(input_ime::OnFocus::kEventName))
    return;

  input_ime::InputContext context_value;
  context_value.context_id = context_id;
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
    const ui::KeyEvent& event,
    IMEEngineHandlerInterface::KeyEventDoneCallback callback) {
  if (extension_id_.empty())
    return;

  // If there is no listener for the event, no need to dispatch the event to
  // extension. Instead, releases the key event for default system behavior.
  if (!ShouldForwardKeyEvent()) {
    // Continue processing the key event so that the physical keyboard can
    // still work.
    std::move(callback).Run(false);
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
  keyboard_event.type = (event.type() == ui::ET_KEY_RELEASED)
                            ? input_ime::KEYBOARD_EVENT_TYPE_KEYUP
                            : input_ime::KEYBOARD_EVENT_TYPE_KEYDOWN;

  // For legacy reasons, we still put a |requestID| into the keyData, even
  // though there is already a |requestID| argument in OnKeyEvent.
  keyboard_event.request_id = std::make_unique<std::string>(request_id);

  // If the given key event is from VK, it means the key event was simulated.
  // Sets the |extension_id| value so that the IME extension can ignore it.
  auto* properties = event.properties();
  if (properties && properties->find(ui::kPropertyFromVK) != properties->end())
    keyboard_event.extension_id = std::make_unique<std::string>(extension_id_);

  keyboard_event.key = GetKeyFromEvent(event);
  keyboard_event.code = event.code() == ui::DomCode::NONE
                            ? ui::KeyboardCodeToDomKeycode(event.key_code())
                            : event.GetCodeString();
  keyboard_event.alt_key = std::make_unique<bool>(event.IsAltDown());
  keyboard_event.altgr_key = std::make_unique<bool>(event.IsAltGrDown());
  keyboard_event.ctrl_key = std::make_unique<bool>(event.IsControlDown());
  keyboard_event.shift_key = std::make_unique<bool>(event.IsShiftDown());
  keyboard_event.caps_lock = std::make_unique<bool>(event.IsCapsLockOn());

  std::unique_ptr<base::ListValue> args(
      input_ime::OnKeyEvent::Create(component_id, keyboard_event, request_id));

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

void ImeObserver::OnSurroundingTextChanged(const std::string& component_id,
                                           const std::u16string& text,
                                           int cursor_pos,
                                           int anchor_pos,
                                           int offset_pos) {
  if (extension_id_.empty() ||
      !HasListener(input_ime::OnSurroundingTextChanged::kEventName))
    return;

  input_ime::OnSurroundingTextChanged::SurroundingInfo info;
  // |info.text| is encoded in UTF8 here so |info.focus| etc may not match the
  // index in |info.text|, the javascript code on the extension side should
  // handle it.
  info.text = base::UTF16ToUTF8(text);
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
    case ui::TEXT_INPUT_TYPE_NULL:
      input_context_type = "null";
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
  // NOTE: ui::TEXT_INPUT_FLAG_AUTOCAPITALIZE_NONE corresponds to Blink's "none"
  // that's a synonym for "off", while input_ime::AUTO_CAPITALIZE_TYPE_NONE
  // auto-generated via API specs means "unspecified" and translates to empty
  // string. The latter should not be emitted as the API specifies a non-falsy
  // enum. So technically there's a bug here; either this impl or the API needs
  // fixing. However, as a public API, the behaviour is left intact for now.
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

InputImeEventRouterFactory::InputImeEventRouterFactory() = default;
InputImeEventRouterFactory::~InputImeEventRouterFactory() = default;

InputImeEventRouter* InputImeEventRouterFactory::GetRouter(Profile* profile) {
  if (!profile)
    return nullptr;
  // The |router_map_| is keyed by the original profile.
  // Refers to the comments in |RemoveProfile| method for the reason.
  profile = profile->GetOriginalProfile();
  InputImeEventRouter* router = router_map_[profile];
  if (!router) {
    // The router must attach to the profile from which the extension can
    // receive events. If |profile| has an off-the-record profile, attach the
    // off-the-record profile. e.g. In guest mode, the extension is running with
    // the incognito profile instead of its original profile.
    router = new InputImeEventRouter(profile->HasPrimaryOTRProfile()
                                         ? profile->GetPrimaryOTRProfile()
                                         : profile);
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
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine)
    return RespondNow(Error(InformativeError(error, static_function_name())));

  engine->KeyEventHandled(extension_id(), params->request_id, params->response);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction InputImeSetCompositionFunction::Run() {
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine)
    return RespondNow(Error(InformativeError(error, static_function_name())));

  std::unique_ptr<SetComposition::Params> parent_params(
      SetComposition::Params::Create(*args_));
  const SetComposition::Params::Parameters& params = parent_params->parameters;
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
        segment_info.style = InputMethodEngineBase::SEGMENT_STYLE_NO_UNDERLINE;
      }
      segments.push_back(segment_info);
    }
  }
  int selection_start =
      params.selection_start ? *params.selection_start : params.cursor;
  int selection_end =
      params.selection_end ? *params.selection_end : params.cursor;
  if (!engine->SetComposition(params.context_id, params.text.c_str(),
                              selection_start, selection_end, params.cursor,
                              segments, &error)) {
    std::unique_ptr<base::ListValue> results =
        std::make_unique<base::ListValue>();
    results->Append(std::make_unique<base::Value>(false));
    return RespondNow(ErrorWithArguments(
        std::move(results), InformativeError(error, static_function_name())));
  }
  return RespondNow(OneArgument(base::Value(true)));
}

ExtensionFunction::ResponseAction InputImeCommitTextFunction::Run() {
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine)
    return RespondNow(Error(InformativeError(error, static_function_name())));

  std::unique_ptr<CommitText::Params> parent_params(
      CommitText::Params::Create(*args_));
  const CommitText::Params::Parameters& params = parent_params->parameters;
  if (!engine->CommitText(params.context_id, base::UTF8ToUTF16(params.text),
                          &error)) {
    std::unique_ptr<base::ListValue> results =
        std::make_unique<base::ListValue>();
    results->Append(std::make_unique<base::Value>(false));
    return RespondNow(ErrorWithArguments(
        std::move(results), InformativeError(error, static_function_name())));
  }
  return RespondNow(OneArgument(base::Value(true)));
}

ExtensionFunction::ResponseAction InputImeSendKeyEventsFunction::Run() {
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine)
    return RespondNow(Error(InformativeError(error, static_function_name())));

  std::unique_ptr<SendKeyEvents::Params> parent_params(
      SendKeyEvents::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(parent_params);
  const SendKeyEvents::Params::Parameters& params = parent_params->parameters;

  std::vector<ui::KeyEvent> key_data_out;
  key_data_out.reserve(params.key_data.size());
  for (const auto& key_event : params.key_data) {
    key_data_out.emplace_back(ConvertKeyboardEventToUIKeyEvent(key_event));
  }

  if (!engine->SendKeyEvents(params.context_id, key_data_out, &error))
    return RespondNow(Error(InformativeError(
        base::StringPrintf("%s %s", kErrorSetKeyEventsFail, error.c_str()),
        static_function_name())));
  return RespondNow(NoArguments());
}

InputImeAPI::InputImeAPI(content::BrowserContext* context)
    : browser_context_(context) {
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context_));

  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, input_ime::OnFocus::kEventName);
}

InputImeAPI::~InputImeAPI() = default;

void InputImeAPI::Shutdown() {
  extension_registry_observation_.Reset();
  InputImeEventRouterFactory::GetInstance()->RemoveProfile(
      Profile::FromBrowserContext(browser_context_));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
  if (observer_ && ui::IMEBridge::Get()) {
    ui::IMEBridge::Get()->RemoveObserver(observer_.get());
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
  return InputImeEventRouterFactory::GetInstance()->GetRouter(profile);
}

std::string InformativeError(const std::string& error,
                             const char* function_name) {
  return base::StringPrintf("[%s]: %s", function_name, error.c_str());
}

}  // namespace extensions

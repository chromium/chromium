// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"

#include <utility>
#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/api/input_method_private.h"
#include "extensions/browser/extension_registry.h"
#include "ui/base/ime/ash/ime_keymap.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace {

namespace input_ime = extensions::api::input_ime;
namespace KeyEventHandled = extensions::api::input_ime::KeyEventHandled;
namespace SetComposition = extensions::api::input_ime::SetComposition;
namespace CommitText = extensions::api::input_ime::CommitText;
namespace SendKeyEvents = extensions::api::input_ime::SendKeyEvents;

const char kErrorRouterNotAvailable[] = "The router is not available.";
const char kErrorSetKeyEventsFail[] = "Could not send key events.";

using ::ash::input_method::InputMethodEngine;

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

ui::KeyEvent ConvertKeyboardEventToUIKeyEvent(
    const input_ime::KeyboardEvent& event) {
  const ui::EventType type =
      event.type == input_ime::KeyboardEventType::kKeydown
          ? ui::EventType::kKeyPressed
          : ui::EventType::kKeyReleased;

  const auto key_code = static_cast<ui::KeyboardCode>(
      event.key_code && *event.key_code != ui::VKEY_UNKNOWN
          ? *event.key_code
          : ash::DomKeycodeToKeyboardCode(event.code));

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
    router = new InputImeEventRouter(
        profile->HasPrimaryOTRProfile()
            ? profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)
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
  std::optional<KeyEventHandled::Params> params =
      KeyEventHandled::Params::Create(args());
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

  std::optional<SetComposition::Params> parent_params =
      SetComposition::Params::Create(args());
  const SetComposition::Params::Parameters& params = parent_params->parameters;
  std::vector<InputMethodEngine::SegmentInfo> segments;
  if (params.segments) {
    for (const auto& segments_arg : *params.segments) {
      EXTENSION_FUNCTION_VALIDATE(segments_arg.style !=
                                  input_ime::UnderlineStyle::kNone);
      InputMethodEngine::SegmentInfo segment_info;
      segment_info.start = segments_arg.start;
      segment_info.end = segments_arg.end;
      if (segments_arg.style == input_ime::UnderlineStyle::kUnderline) {
        segment_info.style = InputMethodEngine::SEGMENT_STYLE_UNDERLINE;
      } else if (segments_arg.style ==
                 input_ime::UnderlineStyle::kDoubleUnderline) {
        segment_info.style = InputMethodEngine::SEGMENT_STYLE_DOUBLE_UNDERLINE;
      } else {
        segment_info.style = InputMethodEngine::SEGMENT_STYLE_NO_UNDERLINE;
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
    base::Value::List results;
    results.Append(false);
    return RespondNow(ErrorWithArguments(
        std::move(results), InformativeError(error, static_function_name())));
  }
  return RespondNow(WithArguments(true));
}

ExtensionFunction::ResponseAction InputImeCommitTextFunction::Run() {
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine)
    return RespondNow(Error(InformativeError(error, static_function_name())));

  std::optional<CommitText::Params> parent_params =
      CommitText::Params::Create(args());
  const CommitText::Params::Parameters& params = parent_params->parameters;
  if (!engine->CommitText(params.context_id, base::UTF8ToUTF16(params.text),
                          &error)) {
    base::Value::List results;
    results.Append(false);
    return RespondNow(ErrorWithArguments(
        std::move(results), InformativeError(error, static_function_name())));
  }
  return RespondNow(WithArguments(true));
}

ExtensionFunction::ResponseAction InputImeSendKeyEventsFunction::Run() {
  std::string error;
  InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine)
    return RespondNow(Error(InformativeError(error, static_function_name())));

  std::optional<SendKeyEvents::Params> parent_params =
      SendKeyEvents::Params::Create(args());
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
  event_router->RegisterObserver(
      this, api::input_method_private::OnFocus::kEventName);
  event_router->RegisterObserver(this, input_ime::OnKeyEvent::kEventName);
}

InputImeAPI::~InputImeAPI() = default;

void InputImeAPI::Shutdown() {
  extension_registry_observation_.Reset();
  InputImeEventRouterFactory::GetInstance()->RemoveProfile(
      Profile::FromBrowserContext(browser_context_));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
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

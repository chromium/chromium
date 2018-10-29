// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_extension_api.h"

#include <stddef.h>
#include <memory>
#include <set>
#include <vector>

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/image_util.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if defined(OS_CHROMEOS)
#include "ash/public/interfaces/accessibility_controller.mojom.h"
#include "ash/public/interfaces/accessibility_focus_ring_controller.mojom.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/event_rewriter_controller.mojom.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_helper_bridge.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/event_injector.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#endif

namespace accessibility_private = extensions::api::accessibility_private;

namespace {

const char kErrorNotSupported[] = "This API is not supported on this platform.";

#if defined(OS_CHROMEOS)
ash::mojom::AccessibilityControllerPtr GetAccessibilityController() {
  // Connect to the accessibility mojo interface in ash.
  ash::mojom::AccessibilityControllerPtr accessibility_controller;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &accessibility_controller);
  return accessibility_controller;
}
#endif

}  // namespace

ExtensionFunction::ResponseAction
AccessibilityPrivateSetNativeAccessibilityEnabledFunction::Run() {
  bool enabled = false;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &enabled));
  if (enabled) {
    content::BrowserAccessibilityState::GetInstance()->
        EnableAccessibility();
  } else {
    content::BrowserAccessibilityState::GetInstance()->
        DisableAccessibility();
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetFocusRingFunction::Run() {
#if defined(OS_CHROMEOS)

  std::unique_ptr<extensions::api::accessibility_private::SetFocusRing::Params>
      params(
          extensions::api::accessibility_private::SetFocusRing::Params::Create(
              *args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<gfx::Rect> rects;
  for (const extensions::api::accessibility_private::ScreenRect& rect :
       params->rects) {
    rects.push_back(gfx::Rect(rect.left, rect.top, rect.width, rect.height));
  }

  auto* accessibility_manager = chromeos::AccessibilityManager::Get();
  if (params->color) {
    SkColor color;
    if (!extensions::image_util::ParseHexColorString(*(params->color), &color))
      return RespondNow(Error("Could not parse hex color"));
    accessibility_manager->SetFocusRingColor(color, extension_id());
  } else {
    accessibility_manager->ResetFocusRingColor(extension_id());
  }

  // Move the visible focus ring to cover all of these rects.
  accessibility_manager->SetFocusRing(
      rects, ash::mojom::FocusRingBehavior::PERSIST_FOCUS_RING, extension_id());

  // Also update the touch exploration controller so that synthesized
  // touch events are anchored within the focused object.
  if (!rects.empty()) {
    chromeos::AccessibilityManager* manager =
        chromeos::AccessibilityManager::Get();
    manager->SetTouchAccessibilityAnchorPoint(rects[0].CenterPoint());
  }

  return RespondNow(NoArguments());
#endif  // defined(OS_CHROMEOS)

  return RespondNow(Error(kErrorNotSupported));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetHighlightsFunction::Run() {
#if defined(OS_CHROMEOS)
  std::unique_ptr<extensions::api::accessibility_private::SetHighlights::Params>
      params(
          extensions::api::accessibility_private::SetHighlights::Params::Create(
              *args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<gfx::Rect> rects;
  for (const extensions::api::accessibility_private::ScreenRect& rect :
       params->rects) {
    rects.push_back(gfx::Rect(rect.left, rect.top, rect.width, rect.height));
  }

  SkColor color;
  if (!extensions::image_util::ParseHexColorString(params->color, &color))
    return RespondNow(Error("Could not parse hex color"));

  // Set the highlights to cover all of these rects.
  chromeos::AccessibilityManager::Get()->SetHighlights(rects, color);

  return RespondNow(NoArguments());
#endif  // defined(OS_CHROMEOS)

  return RespondNow(Error(kErrorNotSupported));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetKeyboardListenerFunction::Run() {
  ChromeExtensionFunctionDetails details(this);
  CHECK(extension());

#if defined(OS_CHROMEOS)
  bool enabled;
  bool capture;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &enabled));
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(1, &capture));

  chromeos::AccessibilityManager* manager =
      chromeos::AccessibilityManager::Get();

  const std::string current_id = manager->keyboard_listener_extension_id();
  if (!current_id.empty() && extension()->id() != current_id)
    return RespondNow(Error("Existing keyboard listener registered."));

  manager->SetKeyboardListenerExtensionId(
      enabled ? extension()->id() : std::string(), details.GetProfile());

  ash::mojom::EventRewriterControllerPtr event_rewriter_controller_ptr;
  content::ServiceManagerConnection* connection =
      content::ServiceManagerConnection::GetForProcess();
  connection->GetConnector()->BindInterface(ash::mojom::kServiceName,
                                            &event_rewriter_controller_ptr);
  event_rewriter_controller_ptr->CaptureAllKeysForSpokenFeedback(enabled &&
                                                                 capture);
  return RespondNow(NoArguments());
#endif  // defined OS_CHROMEOS

  return RespondNow(Error(kErrorNotSupported));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateDarkenScreenFunction::Run() {
#if defined(OS_CHROMEOS)
  bool darken = false;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &darken));
  chromeos::AccessibilityManager::Get()->SetDarkenScreen(darken);
  return RespondNow(NoArguments());
#else
  return RespondNow(Error(kErrorNotSupported));
#endif
}

#if defined(OS_CHROMEOS)
ExtensionFunction::ResponseAction
AccessibilityPrivateSetSwitchAccessKeysFunction::Run() {
  std::unique_ptr<accessibility_private::SetSwitchAccessKeys::Params> params =
      accessibility_private::SetSwitchAccessKeys::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  // For now, only accept key code if it represents an alphanumeric character.
  std::set<int> key_codes;
  for (auto key_code : params->key_codes) {
    EXTENSION_FUNCTION_VALIDATE(key_code >= ui::VKEY_0 &&
                                key_code <= ui::VKEY_Z);
    key_codes.insert(key_code);
  }

  chromeos::AccessibilityManager* manager =
      chromeos::AccessibilityManager::Get();

  // AccessibilityManager can be null during system shut down, but no need to
  // return error in this case, so just check that manager is not null.
  if (manager)
    manager->SetSwitchAccessKeys(key_codes);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetNativeChromeVoxArcSupportForCurrentAppFunction::Run() {
  std::unique_ptr<
      accessibility_private::SetNativeChromeVoxArcSupportForCurrentApp::Params>
      params = accessibility_private::
          SetNativeChromeVoxArcSupportForCurrentApp::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  ChromeExtensionFunctionDetails details(this);
  arc::ArcAccessibilityHelperBridge* bridge =
      arc::ArcAccessibilityHelperBridge::GetForBrowserContext(
          details.GetProfile());
  if (bridge) {
    bool enabled;
    EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &enabled));
    bridge->SetNativeChromeVoxArcSupport(enabled);
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSendSyntheticKeyEventFunction::Run() {
  std::unique_ptr<accessibility_private::SendSyntheticKeyEvent::Params> params =
      accessibility_private::SendSyntheticKeyEvent::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::SyntheticKeyboardEvent* key_data = &params->key_event;

  int modifiers = 0;
  if (key_data->modifiers.get()) {
    if (key_data->modifiers->ctrl && *key_data->modifiers->ctrl)
      modifiers |= ui::EF_CONTROL_DOWN;
    if (key_data->modifiers->alt && *key_data->modifiers->alt)
      modifiers |= ui::EF_ALT_DOWN;
    if (key_data->modifiers->search && *key_data->modifiers->search)
      modifiers |= ui::EF_COMMAND_DOWN;
    if (key_data->modifiers->shift && *key_data->modifiers->shift)
      modifiers |= ui::EF_SHIFT_DOWN;
  }

  std::unique_ptr<ui::KeyEvent> synthetic_key_event =
      std::make_unique<ui::KeyEvent>(
          key_data->type ==
                  accessibility_private::SYNTHETIC_KEYBOARD_EVENT_TYPE_KEYUP
              ? ui::ET_KEY_RELEASED
              : ui::ET_KEY_PRESSED,
          static_cast<ui::KeyboardCode>(key_data->key_code),
          static_cast<ui::DomCode>(0), modifiers);

  ws::mojom::EventInjectorPtr event_injector_ptr;
  content::ServiceManagerConnection* connection =
      content::ServiceManagerConnection::GetForProcess();
  connection->GetConnector()->BindInterface(ws::mojom::kServiceName,
                                            &event_injector_ptr);
  event_injector_ptr->InjectEventNoAck(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      std::move(synthetic_key_event));

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateEnableChromeVoxMouseEventsFunction::Run() {
#if defined(OS_CHROMEOS)
  bool enabled = false;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &enabled));
  ash::mojom::EventRewriterControllerPtr event_rewriter_controller_ptr;
  content::ServiceManagerConnection* connection =
      content::ServiceManagerConnection::GetForProcess();
  connection->GetConnector()->BindInterface(ash::mojom::kServiceName,
                                            &event_rewriter_controller_ptr);
  event_rewriter_controller_ptr->SetSendMouseEventsToDelegate(enabled);
  return RespondNow(NoArguments());
#else
  return RespondNow(Error(kErrorNotSupported));
#endif
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSendSyntheticMouseEventFunction::Run() {
  std::unique_ptr<accessibility_private::SendSyntheticMouseEvent::Params>
      params = accessibility_private::SendSyntheticMouseEvent::Params::Create(
          *args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::SyntheticMouseEvent* mouse_data = &params->mouse_event;

  // TODO(crbug/893752) Choose correct display
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  int x = (int)(mouse_data->x * display.device_scale_factor());
  int y = (int)(mouse_data->y * display.device_scale_factor());

  gfx::Point location(x, y);
  ui::EventType type;
  switch (mouse_data->type) {
    case accessibility_private::SYNTHETIC_MOUSE_EVENT_TYPE_PRESS:
      type = ui::ET_MOUSE_PRESSED;
      break;
    case accessibility_private::SYNTHETIC_MOUSE_EVENT_TYPE_RELEASE:
      type = ui::ET_MOUSE_RELEASED;
      break;
    case accessibility_private::SYNTHETIC_MOUSE_EVENT_TYPE_DRAG:
      type = ui::ET_MOUSE_DRAGGED;
      break;
    case accessibility_private::SYNTHETIC_MOUSE_EVENT_TYPE_MOVE:
      type = ui::ET_MOUSE_MOVED;
      break;
    case accessibility_private::SYNTHETIC_MOUSE_EVENT_TYPE_ENTER:
      type = ui::ET_MOUSE_ENTERED;
      break;
    case accessibility_private::SYNTHETIC_MOUSE_EVENT_TYPE_EXIT:
      type = ui::ET_MOUSE_EXITED;
      break;
    default:
      NOTREACHED();
  }

  int flags = ui::EF_LEFT_MOUSE_BUTTON;

  std::unique_ptr<ui::MouseEvent> synthetic_mouse_event =
      std::make_unique<ui::MouseEvent>(type, location, location,
                                       ui::EventTimeForNow(), flags,
                                       flags /* changed_button_flags */);

  ws::mojom::EventInjectorPtr event_injector_ptr;
  content::ServiceManagerConnection* connection =
      content::ServiceManagerConnection::GetForProcess();
  connection->GetConnector()->BindInterface(ws::mojom::kServiceName,
                                            &event_injector_ptr);
  event_injector_ptr->InjectEventNoAck(display.id(),
                                       std::move(synthetic_mouse_event));

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateOnSelectToSpeakStateChangedFunction::Run() {
  std::unique_ptr<accessibility_private::OnSelectToSpeakStateChanged::Params>
      params =
          accessibility_private::OnSelectToSpeakStateChanged::Params::Create(
              *args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::SelectToSpeakState params_state = params->state;
  ash::mojom::SelectToSpeakState state;
  switch (params_state) {
    case accessibility_private::SelectToSpeakState::
        SELECT_TO_SPEAK_STATE_SELECTING:
      state = ash::mojom::SelectToSpeakState::kSelectToSpeakStateSelecting;
      break;
    case accessibility_private::SelectToSpeakState::
        SELECT_TO_SPEAK_STATE_SPEAKING:
      state = ash::mojom::SelectToSpeakState::kSelectToSpeakStateSpeaking;
      break;
    case accessibility_private::SelectToSpeakState::
        SELECT_TO_SPEAK_STATE_INACTIVE:
    case accessibility_private::SelectToSpeakState::SELECT_TO_SPEAK_STATE_NONE:
      state = ash::mojom::SelectToSpeakState::kSelectToSpeakStateInactive;
  }

  auto* accessibility_manager = chromeos::AccessibilityManager::Get();
  accessibility_manager->OnSelectToSpeakStateChanged(state);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateToggleDictationFunction::Run() {
  ash::mojom::DictationToggleSource source =
      ash::mojom::DictationToggleSource::kChromevox;
  if (extension()->id() == extension_misc::kSwitchAccessExtensionId)
    source = ash::mojom::DictationToggleSource::kSwitchAccess;
  else if (extension()->id() == extension_misc::kChromeVoxExtensionId)
    source = ash::mojom::DictationToggleSource::kChromevox;
  else
    NOTREACHED();

  GetAccessibilityController()->ToggleDictationFromSource(source);

  return RespondNow(NoArguments());
}

#endif  // defined (OS_CHROMEOS)

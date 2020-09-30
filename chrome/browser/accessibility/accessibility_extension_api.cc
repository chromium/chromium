// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_extension_api.h"

#include <stddef.h>
#include <memory>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/system_connector.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/image_util.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/accessibility_controller.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/accessibility_focus_ring_info.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "ash/public/cpp/window_tree_host_lookup.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_helper_bridge.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes_util.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#endif

namespace accessibility_private = extensions::api::accessibility_private;

namespace {

const char kErrorNotSupported[] = "This API is not supported on this platform.";

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
AccessibilityPrivateOpenSettingsSubpageFunction::Run() {
  using extensions::api::accessibility_private::OpenSettingsSubpage::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

#if defined(OS_CHROMEOS)
  // TODO(chrome-a11y-core): we can't open a settings page when you're on the
  // signin profile, but maybe we should notify the user and explain why?
  Profile* profile = chromeos::AccessibilityManager::Get()->profile();
  if (!chromeos::ProfileHelper::IsSigninProfile(profile) &&
      chromeos::settings::IsOSSettingsSubPage(params->subpage)) {
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
        profile, params->subpage);
  }
#else
  // This function should only be available on ChromeOS.
  EXTENSION_FUNCTION_VALIDATE(false);
#endif  // OS_CHROMEOS
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetFocusRingsFunction::Run() {
#if defined(OS_CHROMEOS)
  std::unique_ptr<accessibility_private::SetFocusRings::Params> params(
      accessibility_private::SetFocusRings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* accessibility_manager = chromeos::AccessibilityManager::Get();

  for (const accessibility_private::FocusRingInfo& focus_ring_info :
       params->focus_rings) {
    auto focus_ring = std::make_unique<ash::AccessibilityFocusRingInfo>();
    focus_ring->behavior = ash::FocusRingBehavior::PERSIST;

    // Convert the given rects into gfx::Rect objects.
    for (const accessibility_private::ScreenRect& rect :
         focus_ring_info.rects) {
      focus_ring->rects_in_screen.push_back(
          gfx::Rect(rect.left, rect.top, rect.width, rect.height));
    }

    const std::string id = accessibility_manager->GetFocusRingId(
        extension_id(), focus_ring_info.id ? *(focus_ring_info.id) : "");

    if (!extensions::image_util::ParseHexColorString(focus_ring_info.color,
                                                     &(focus_ring->color))) {
      return RespondNow(Error("Could not parse hex color"));
    }

    if (focus_ring_info.secondary_color &&
        !extensions::image_util::ParseHexColorString(
            *(focus_ring_info.secondary_color),
            &(focus_ring->secondary_color))) {
      return RespondNow(Error("Could not parse secondary hex color"));
    }

    switch (focus_ring_info.type) {
      case accessibility_private::FOCUS_TYPE_SOLID:
        focus_ring->type = ash::FocusRingType::SOLID;
        break;
      case accessibility_private::FOCUS_TYPE_DASHED:
        focus_ring->type = ash::FocusRingType::DASHED;
        break;
      case accessibility_private::FOCUS_TYPE_GLOW:
        focus_ring->type = ash::FocusRingType::GLOW;
        break;
      default:
        NOTREACHED();
    }

    if (focus_ring_info.background_color &&
        !extensions::image_util::ParseHexColorString(
            *(focus_ring_info.background_color),
            &(focus_ring->background_color))) {
      return RespondNow(Error("Could not parse background hex color"));
    }

    // Update the touch exploration controller so that synthesized touch events
    // are anchored within the focused object.
    // NOTE: The final anchor point will be determined by the first rect of the
    // final focus ring.
    if (!focus_ring->rects_in_screen.empty()) {
      accessibility_manager->SetTouchAccessibilityAnchorPoint(
          focus_ring->rects_in_screen[0].CenterPoint());
    }

    // Set the focus ring.
    accessibility_manager->SetFocusRing(id, std::move(focus_ring));
  }

  return RespondNow(NoArguments());
#else
  return RespondNow(Error(kErrorNotSupported));
#endif  // defined(OS_CHROMEOS)
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetHighlightsFunction::Run() {
#if defined(OS_CHROMEOS)
  std::unique_ptr<accessibility_private::SetHighlights::Params> params(
      accessibility_private::SetHighlights::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<gfx::Rect> rects;
  for (const accessibility_private::ScreenRect& rect : params->rects) {
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

  ash::EventRewriterController::Get()->CaptureAllKeysForSpokenFeedback(
      enabled && capture);
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

  auto* host = ash::GetWindowTreeHostForDisplay(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  DCHECK(host);
  // This skips rewriters.
  host->DeliverEventToSink(synthetic_key_event.get());
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateEnableChromeVoxMouseEventsFunction::Run() {
  bool enabled = false;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &enabled));
  ash::EventRewriterController::Get()->SetSendMouseEventsToDelegate(enabled);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSendSyntheticMouseEventFunction::Run() {
  std::unique_ptr<accessibility_private::SendSyntheticMouseEvent::Params>
      params = accessibility_private::SendSyntheticMouseEvent::Params::Create(
          *args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::SyntheticMouseEvent* mouse_data = &params->mouse_event;

  ui::EventType type = ui::ET_UNKNOWN;
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

  int flags = 0;
  if (type != ui::ET_MOUSE_MOVED)
    flags |= ui::EF_LEFT_MOUSE_BUTTON;

  int changed_button_flags = flags;

  flags |= ui::EF_IS_SYNTHESIZED;
  if (mouse_data->touch_accessibility && *(mouse_data->touch_accessibility))
    flags |= ui::EF_TOUCH_ACCESSIBILITY;

  // Locations are assumed to be display relative (and in DIPs).
  // TODO(crbug/893752) Choose correct display
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  gfx::Point location(mouse_data->x, mouse_data->y);
  std::unique_ptr<ui::MouseEvent> synthetic_mouse_event =
      std::make_unique<ui::MouseEvent>(type, location, location,
                                       ui::EventTimeForNow(), flags,
                                       changed_button_flags);

  auto* host = ash::GetWindowTreeHostForDisplay(display.id());
  DCHECK(host);
  // Transforming the coordinate to the root will apply the screen scale factor
  // to the event's location and also the screen rotation degree.
  synthetic_mouse_event->UpdateForRootTransform(
      host->GetRootTransform(),
      host->GetRootTransformForLocalEventCoordinates());
  // This skips rewriters.
  host->DeliverEventToSink(synthetic_mouse_event.get());

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetSelectToSpeakStateFunction::Run() {
  std::unique_ptr<accessibility_private::SetSelectToSpeakState::Params> params =
      accessibility_private::SetSelectToSpeakState::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::SelectToSpeakState params_state = params->state;
  ash::SelectToSpeakState state;
  switch (params_state) {
    case accessibility_private::SelectToSpeakState::
        SELECT_TO_SPEAK_STATE_SELECTING:
      state = ash::SelectToSpeakState::kSelectToSpeakStateSelecting;
      break;
    case accessibility_private::SelectToSpeakState::
        SELECT_TO_SPEAK_STATE_SPEAKING:
      state = ash::SelectToSpeakState::kSelectToSpeakStateSpeaking;
      break;
    case accessibility_private::SelectToSpeakState::
        SELECT_TO_SPEAK_STATE_INACTIVE:
    case accessibility_private::SelectToSpeakState::SELECT_TO_SPEAK_STATE_NONE:
      state = ash::SelectToSpeakState::kSelectToSpeakStateInactive;
  }

  auto* accessibility_manager = chromeos::AccessibilityManager::Get();
  accessibility_manager->SetSelectToSpeakState(state);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateHandleScrollableBoundsForPointFoundFunction::Run() {
  std::unique_ptr<
      accessibility_private::HandleScrollableBoundsForPointFound::Params>
      params = accessibility_private::HandleScrollableBoundsForPointFound::
          Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::ScreenRect rect = std::move(params->rect);
  gfx::Rect bounds(rect.left, rect.top, rect.width, rect.height);
  ash::AccessibilityController::Get()->HandleAutoclickScrollableBoundsFound(
      bounds);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateMoveMagnifierToRectFunction::Run() {
  std::unique_ptr<accessibility_private::MoveMagnifierToRect::Params> params =
      accessibility_private::MoveMagnifierToRect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::ScreenRect rect = std::move(params->rect);
  gfx::Rect bounds(rect.left, rect.top, rect.width, rect.height);

  chromeos::MagnificationManager* magnification_manager =
      chromeos::MagnificationManager::Get();
  if (magnification_manager)
    magnification_manager->HandleMoveMagnifierToRectIfEnabled(bounds);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateToggleDictationFunction::Run() {
  ash::DictationToggleSource source = ash::DictationToggleSource::kChromevox;
  if (extension()->id() == extension_misc::kSwitchAccessExtensionId)
    source = ash::DictationToggleSource::kSwitchAccess;
  else if (extension()->id() == extension_misc::kChromeVoxExtensionId)
    source = ash::DictationToggleSource::kChromevox;
  else
    NOTREACHED();

  ash::AccessibilityController::Get()->ToggleDictationFromSource(source);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateForwardKeyEventsToSwitchAccessFunction::Run() {
  std::unique_ptr<accessibility_private::ForwardKeyEventsToSwitchAccess::Params>
      params =
          accessibility_private::ForwardKeyEventsToSwitchAccess::Params::Create(
              *args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  return RespondNow(Error("Forwarding key events is no longer supported."));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateUpdateSwitchAccessBubbleFunction::Run() {
  std::unique_ptr<accessibility_private::UpdateSwitchAccessBubble::Params>
      params = accessibility_private::UpdateSwitchAccessBubble::Params::Create(
          *args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!params->show) {
    if (params->bubble ==
        accessibility_private::SWITCH_ACCESS_BUBBLE_BACKBUTTON)
      ash::AccessibilityController::Get()->HideSwitchAccessBackButton();
    else if (params->bubble == accessibility_private::SWITCH_ACCESS_BUBBLE_MENU)
      ash::AccessibilityController::Get()->HideSwitchAccessMenu();
    return RespondNow(NoArguments());
  }

  if (!params->anchor)
    return RespondNow(Error("An anchor rect is required to show a bubble."));

  gfx::Rect anchor(params->anchor->left, params->anchor->top,
                   params->anchor->width, params->anchor->height);

  if (params->bubble ==
      accessibility_private::SWITCH_ACCESS_BUBBLE_BACKBUTTON) {
    ash::AccessibilityController::Get()->ShowSwitchAccessBackButton(anchor);
    return RespondNow(NoArguments());
  }

  if (!params->actions)
    return RespondNow(Error("The menu cannot be shown without actions."));

  std::vector<std::string> actions_to_show;
  for (accessibility_private::SwitchAccessMenuAction extension_action :
       *(params->actions)) {
    std::string action = accessibility_private::ToString(extension_action);
    // Check that this action is not already in our actions list.
    if (std::find(actions_to_show.begin(), actions_to_show.end(), action) !=
        actions_to_show.end()) {
      continue;
    }
    actions_to_show.push_back(action);
  }

  ash::AccessibilityController::Get()->ShowSwitchAccessMenu(anchor,
                                                            actions_to_show);
  return RespondNow(NoArguments());
}

AccessibilityPrivateGetBatteryDescriptionFunction::
    AccessibilityPrivateGetBatteryDescriptionFunction() {}

AccessibilityPrivateGetBatteryDescriptionFunction::
    ~AccessibilityPrivateGetBatteryDescriptionFunction() {}

ExtensionFunction::ResponseAction
AccessibilityPrivateGetBatteryDescriptionFunction::Run() {
  return RespondNow(OneArgument(std::make_unique<base::Value>(
      ash::AccessibilityController::Get()->GetBatteryDescription())));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetVirtualKeyboardVisibleFunction::Run() {
  std::unique_ptr<accessibility_private::SetVirtualKeyboardVisible::Params>
      params = accessibility_private::SetVirtualKeyboardVisible::Params::Create(
          *args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  ash::AccessibilityController::Get()->SetVirtualKeyboardVisible(
      params->is_visible);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivatePerformAcceleratorActionFunction::Run() {
  std::unique_ptr<accessibility_private::PerformAcceleratorAction::Params>
      params = accessibility_private::PerformAcceleratorAction::Params::Create(
          *args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  ash::AcceleratorAction accelerator_action;
  switch (params->accelerator_action) {
    case accessibility_private::ACCELERATOR_ACTION_FOCUSPREVIOUSPANE:
      accelerator_action = ash::FOCUS_PREVIOUS_PANE;
      break;
    case accessibility_private::ACCELERATOR_ACTION_FOCUSNEXTPANE:
      accelerator_action = ash::FOCUS_NEXT_PANE;
      break;
    case accessibility_private::ACCELERATOR_ACTION_NONE:
      NOTREACHED();
      return RespondNow(Error("Invalid accelerator action."));
  }

  ash::AccessibilityController::Get()->PerformAcceleratorAction(
      accelerator_action);
  return RespondNow(NoArguments());
}

#endif  // defined (OS_CHROMEOS)

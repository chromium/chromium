// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_extension_api_ash.h"

#include <stddef.h>

#include <set>
#include <vector>

#include "ash/public/cpp/accessibility_controller.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/accessibility_focus_ring_info.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "ash/public/cpp/window_tree_host_lookup.h"
#include "ash/webui/settings/public/constants/routes_util.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/arc/accessibility/arc_accessibility_helper_bridge.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/embedded_accessibility_helper_client_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/crosapi/cpp/lacros_startup_state.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/common/color_parser.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "services/accessibility/public/mojom/assistive_technology_type.mojom.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_codes_array.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace {

namespace accessibility_private = ::extensions::api::accessibility_private;
using ::ash::AccessibilityManager;

ash::AccessibilityToastType ConvertToastType(
    accessibility_private::ToastType type) {
  switch (type) {
    case accessibility_private::ToastType::
        TOAST_TYPE_DICTATIONNOFOCUSEDTEXTFIELD:
      return ash::AccessibilityToastType::kDictationNoFocusedTextField;
    case accessibility_private::ToastType::TOAST_TYPE_DICTATIONMICMUTED:
      return ash::AccessibilityToastType::kDictationMicMuted;
    case accessibility_private::ToastType::TOAST_TYPE_NONE:
      NOTREACHED_NORETURN();
  }
}

ash::DictationBubbleHintType ConvertDictationHintType(
    accessibility_private::DictationBubbleHintType hint_type) {
  switch (hint_type) {
    case accessibility_private::DictationBubbleHintType::
        DICTATION_BUBBLE_HINT_TYPE_TRYSAYING:
      return ash::DictationBubbleHintType::kTrySaying;
    case accessibility_private::DictationBubbleHintType::
        DICTATION_BUBBLE_HINT_TYPE_TYPE:
      return ash::DictationBubbleHintType::kType;
    case accessibility_private::DictationBubbleHintType::
        DICTATION_BUBBLE_HINT_TYPE_DELETE:
      return ash::DictationBubbleHintType::kDelete;
    case accessibility_private::DictationBubbleHintType::
        DICTATION_BUBBLE_HINT_TYPE_SELECTALL:
      return ash::DictationBubbleHintType::kSelectAll;
    case accessibility_private::DictationBubbleHintType::
        DICTATION_BUBBLE_HINT_TYPE_UNDO:
      return ash::DictationBubbleHintType::kUndo;
    case accessibility_private::DictationBubbleHintType::
        DICTATION_BUBBLE_HINT_TYPE_HELP:
      return ash::DictationBubbleHintType::kHelp;
    case accessibility_private::DictationBubbleHintType::
        DICTATION_BUBBLE_HINT_TYPE_UNSELECT:
      return ash::DictationBubbleHintType::kUnselect;
    case accessibility_private::DictationBubbleHintType::
        DICTATION_BUBBLE_HINT_TYPE_COPY:
      return ash::DictationBubbleHintType::kCopy;
    case accessibility_private::DictationBubbleHintType::
        DICTATION_BUBBLE_HINT_TYPE_NONE:
      NOTREACHED();
      return ash::DictationBubbleHintType::kTrySaying;
  }
}

}  // namespace

ExtensionFunction::ResponseAction
AccessibilityPrivateClipboardCopyInActiveLacrosGoogleDocFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  std::string url = args()[0].GetString();
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->embedded_accessibility_helper_client_ash()
      ->ClipboardCopyInActiveGoogleDoc(url);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateDarkenScreenFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_bool());
  bool darken = args()[0].GetBool();
  AccessibilityManager::Get()->SetDarkenScreen(darken);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateEnableMouseEventsFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_bool());
  bool enabled = args()[0].GetBool();
  ash::EventRewriterController::Get()->SetSendMouseEvents(enabled);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateForwardKeyEventsToSwitchAccessFunction::Run() {
  absl::optional<accessibility_private::ForwardKeyEventsToSwitchAccess::Params>
      params =
          accessibility_private::ForwardKeyEventsToSwitchAccess::Params::Create(
              args());
  EXTENSION_FUNCTION_VALIDATE(params);

  return RespondNow(Error("Forwarding key events is no longer supported."));
}

AccessibilityPrivateGetBatteryDescriptionFunction::
    AccessibilityPrivateGetBatteryDescriptionFunction() {}

AccessibilityPrivateGetBatteryDescriptionFunction::
    ~AccessibilityPrivateGetBatteryDescriptionFunction() {}

ExtensionFunction::ResponseAction
AccessibilityPrivateGetBatteryDescriptionFunction::Run() {
  return RespondNow(WithArguments(
      ash::AccessibilityController::Get()->GetBatteryDescription()));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateGetDlcContentsFunction::Run() {
  absl::optional<accessibility_private::GetDlcContents::Params> params(
      accessibility_private::GetDlcContents::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::DlcType dlc = params->dlc;

  AccessibilityManager::Get()->GetDlcContents(
      dlc,
      base::BindOnce(
          &AccessibilityPrivateGetDlcContentsFunction::OnDlcContentsRetrieved,
          this));
  return RespondLater();
}

void AccessibilityPrivateGetDlcContentsFunction::OnDlcContentsRetrieved(
    const std::vector<uint8_t>& contents,
    absl::optional<std::string> error) {
  if (error.has_value()) {
    Respond(Error(error.value()));
    return;
  }

  Respond(WithArguments(base::Value(contents)));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateGetLocalizedDomKeyStringForKeyCodeFunction::Run() {
  absl::optional<
      accessibility_private::GetLocalizedDomKeyStringForKeyCode::Params>
      params = accessibility_private::GetLocalizedDomKeyStringForKeyCode::
          Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ui::KeyboardCode key_code = static_cast<ui::KeyboardCode>(params->key_code);
  ui::DomKey dom_key;
  ui::KeyboardCode key_code_to_compare = ui::VKEY_UNKNOWN;
  const ui::KeyboardLayoutEngine* layout_engine =
      ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine();
  ui::DomCode dom_code_for_key_code =
      ui::KeycodeConverter::MapUSPositionalShortcutKeyToDomCode(key_code);
  if (dom_code_for_key_code != ui::DomCode::NONE) {
    if (layout_engine->Lookup(dom_code_for_key_code,
                              /*event_flags=*/ui::EF_NONE, &dom_key,
                              &key_code_to_compare)) {
      if (dom_key.IsDeadKey() || !dom_key.IsValid()) {
        return RespondNow(Error("Invalid key code"));
      }
      return RespondNow(
          WithArguments(ui::KeycodeConverter::DomKeyToKeyString(dom_key)));
    }
  }

  for (const auto& dom_code : ui::kDomCodesArray) {
    if (!layout_engine->Lookup(dom_code, /*event_flags=*/ui::EF_NONE, &dom_key,
                               &key_code_to_compare)) {
      continue;
    }
    if (key_code_to_compare != key_code || !dom_key.IsValid() ||
        dom_key.IsDeadKey()) {
      continue;
    }
    return RespondNow(
        WithArguments(ui::KeycodeConverter::DomKeyToKeyString((dom_key))));
  }

  return RespondNow(WithArguments(std::string()));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateHandleScrollableBoundsForPointFoundFunction::Run() {
  absl::optional<
      accessibility_private::HandleScrollableBoundsForPointFound::Params>
      params = accessibility_private::HandleScrollableBoundsForPointFound::
          Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  gfx::Rect bounds(params->rect.left, params->rect.top, params->rect.width,
                   params->rect.height);
  ash::AccessibilityController::Get()->HandleAutoclickScrollableBoundsFound(
      bounds);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateInstallPumpkinForDictationFunction::Run() {
  AccessibilityManager::Get()->InstallPumpkinForDictation(
      base::BindOnce(&AccessibilityPrivateInstallPumpkinForDictationFunction::
                         OnPumpkinInstallFinished,
                     this));
  return RespondLater();
}

void AccessibilityPrivateInstallPumpkinForDictationFunction::
    OnPumpkinInstallFinished(
        absl::optional<accessibility_private::PumpkinData> data) {
  if (!data) {
    Respond(Error("Couldn't retrieve Pumpkin data."));
    return;
  }

  Respond(WithArguments(data->ToValue()));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateIsFeatureEnabledFunction::Run() {
  absl::optional<accessibility_private::IsFeatureEnabled::Params> params =
      accessibility_private::IsFeatureEnabled::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::AccessibilityFeature params_feature = params->feature;
  bool enabled;
  switch (params_feature) {
    case accessibility_private::AccessibilityFeature::
        ACCESSIBILITY_FEATURE_GOOGLETTSLANGUAGEPACKS:
      enabled = ::features::
          IsExperimentalAccessibilityGoogleTtsLanguagePacksEnabled();
      break;
    case accessibility_private::AccessibilityFeature::
        ACCESSIBILITY_FEATURE_GOOGLETTSHIGHQUALITYVOICES:
      enabled = ::features::
          IsExperimentalAccessibilityGoogleTtsHighQualityVoicesEnabled();
      break;
    case accessibility_private::AccessibilityFeature::
        ACCESSIBILITY_FEATURE_DICTATIONCONTEXTCHECKING:
      enabled = ::features::
          IsExperimentalAccessibilityDictationContextCheckingEnabled();
      break;
    case accessibility_private::AccessibilityFeature::
        ACCESSIBILITY_FEATURE_GAMEFACEINTEGRATION:
      enabled = ::features::IsAccessibilityGameFaceIntegrationEnabled();
      break;
    case accessibility_private::AccessibilityFeature::
        ACCESSIBILITY_FEATURE_NONE:
      return RespondNow(Error("Unrecognized feature"));
  }

  return RespondNow(WithArguments(enabled));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateMagnifierCenterOnPointFunction::Run() {
  absl::optional<accessibility_private::MagnifierCenterOnPoint::Params> params =
      accessibility_private::MagnifierCenterOnPoint::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  gfx::Point point_in_screen(params->point.x, params->point.y);

  auto* magnification_manager = ash::MagnificationManager::Get();
  DCHECK(magnification_manager);
  magnification_manager->HandleMagnifierCenterOnPointIfEnabled(point_in_screen);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateMoveMagnifierToRectFunction::Run() {
  absl::optional<accessibility_private::MoveMagnifierToRect::Params> params =
      accessibility_private::MoveMagnifierToRect::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  gfx::Rect bounds(params->rect.left, params->rect.top, params->rect.width,
                   params->rect.height);

  auto* magnification_manager = ash::MagnificationManager::Get();
  DCHECK(magnification_manager);
  magnification_manager->HandleMoveMagnifierToRectIfEnabled(bounds);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateOpenSettingsSubpageFunction::Run() {
  using extensions::api::accessibility_private::OpenSettingsSubpage::Params;
  const absl::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  AccessibilityManager::Get()->OpenSettingsSubpage(params->subpage);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivatePerformAcceleratorActionFunction::Run() {
  absl::optional<accessibility_private::PerformAcceleratorAction::Params>
      params = accessibility_private::PerformAcceleratorAction::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(params);
  ash::AcceleratorAction accelerator_action;
  switch (params->accelerator_action) {
    case accessibility_private::ACCELERATOR_ACTION_FOCUSPREVIOUSPANE:
      accelerator_action = ash::AcceleratorAction::kFocusPreviousPane;
      break;
    case accessibility_private::ACCELERATOR_ACTION_FOCUSNEXTPANE:
      accelerator_action = ash::AcceleratorAction::kFocusNextPane;
      break;
    case accessibility_private::ACCELERATOR_ACTION_NONE:
      NOTREACHED();
      return RespondNow(Error("Invalid accelerator action."));
  }

  ash::AccessibilityController::Get()->PerformAcceleratorAction(
      accelerator_action);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSendSyntheticKeyEventFunction::Run() {
  absl::optional<accessibility_private::SendSyntheticKeyEvent::Params> params =
      accessibility_private::SendSyntheticKeyEvent::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::SyntheticKeyboardEvent* key_data = &params->key_event;

  int modifiers = 0;
  if (key_data->modifiers) {
    if (key_data->modifiers->ctrl && *key_data->modifiers->ctrl) {
      modifiers |= ui::EF_CONTROL_DOWN;
    }
    if (key_data->modifiers->alt && *key_data->modifiers->alt) {
      modifiers |= ui::EF_ALT_DOWN;
    }
    if (key_data->modifiers->search && *key_data->modifiers->search) {
      modifiers |= ui::EF_COMMAND_DOWN;
    }
    if (key_data->modifiers->shift && *key_data->modifiers->shift) {
      modifiers |= ui::EF_SHIFT_DOWN;
    }
  }

  ui::KeyboardCode keyboard_code =
      static_cast<ui::KeyboardCode>(key_data->key_code);
  ui::KeyEvent synthetic_key_event(
      key_data->type ==
              accessibility_private::SYNTHETIC_KEYBOARD_EVENT_TYPE_KEYUP
          ? ui::ET_KEY_RELEASED
          : ui::ET_KEY_PRESSED,
      keyboard_code, ui::UsLayoutKeyboardCodeToDomCode(keyboard_code),
      modifiers);

  auto* host = ash::GetWindowTreeHostForDisplay(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  DCHECK(host);

  bool dictation_enabled = AccessibilityManager::Get()->IsDictationEnabled();
  bool from_accessibility_common =
      extension_id() == extension_misc::kAccessibilityCommonExtensionId;
  if (dictation_enabled && from_accessibility_common &&
      params->use_rewriters.has_value() && params->use_rewriters.value()) {
    // TODO(b/259397131): Remove the `useRewriters` property and remove this
    // if statement.
    host->SendEventToSink(&synthetic_key_event);
  } else {
    host->DeliverEventToSink(&synthetic_key_event);
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSendSyntheticMouseEventFunction::Run() {
  absl::optional<accessibility_private::SendSyntheticMouseEvent::Params>
      params = accessibility_private::SendSyntheticMouseEvent::Params::Create(
          args());
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
  if (type != ui::ET_MOUSE_MOVED) {
    switch (mouse_data->mouse_button) {
      case accessibility_private::SYNTHETIC_MOUSE_EVENT_BUTTON_LEFT:
        flags |= ui::EF_LEFT_MOUSE_BUTTON;
        break;
      case accessibility_private::SYNTHETIC_MOUSE_EVENT_BUTTON_MIDDLE:
        flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
        break;
      case accessibility_private::SYNTHETIC_MOUSE_EVENT_BUTTON_RIGHT:
        flags |= ui::EF_RIGHT_MOUSE_BUTTON;
        break;
      case accessibility_private::SYNTHETIC_MOUSE_EVENT_BUTTON_BACK:
        flags |= ui::EF_BACK_MOUSE_BUTTON;
        break;
      case accessibility_private::SYNTHETIC_MOUSE_EVENT_BUTTON_FOWARD:
        flags |= ui::EF_FORWARD_MOUSE_BUTTON;
        break;
      default:
        flags |= ui::EF_LEFT_MOUSE_BUTTON;
    }
  }

  int changed_button_flags = flags;

  flags |= ui::EF_IS_SYNTHESIZED;
  if (mouse_data->touch_accessibility && *(mouse_data->touch_accessibility)) {
    flags |= ui::EF_TOUCH_ACCESSIBILITY;
  }

  // Locations are assumed to be in screen coordinates.
  gfx::Point location_in_screen(mouse_data->x, mouse_data->y);
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(location_in_screen);
  auto* host = ash::GetWindowTreeHostForDisplay(display.id());
  if (!host) {
    return RespondNow(NoArguments());
  }

  aura::Window* root_window = host->window();
  if (!root_window) {
    return RespondNow(NoArguments());
  }

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);

  bool is_mouse_events_enabled = cursor_client->IsMouseEventsEnabled();
  if (!is_mouse_events_enabled) {
    cursor_client->EnableMouseEvents();
  }

  ::wm::ConvertPointFromScreen(root_window, &location_in_screen);

  ui::MouseEvent synthetic_mouse_event(
      type, location_in_screen, location_in_screen, ui::EventTimeForNow(),
      flags, changed_button_flags);

  // Transforming the coordinate to the root will apply the screen scale factor
  // to the event's location and also the screen rotation degree.
  synthetic_mouse_event.UpdateForRootTransform(
      host->GetRootTransform(),
      host->GetRootTransformForLocalEventCoordinates());
  // This skips rewriters.
  host->DeliverEventToSink(&synthetic_mouse_event);

  if (!is_mouse_events_enabled) {
    cursor_client->DisableMouseEvents();
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetFocusRingsFunction::Run() {
  absl::optional<accessibility_private::SetFocusRings::Params> params(
      accessibility_private::SetFocusRings::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* accessibility_manager = AccessibilityManager::Get();

  ax::mojom::AssistiveTechnologyType at_type;
  switch (params->at_type) {
    case extensions::api::accessibility_private::ASSISTIVE_TECHNOLOGY_TYPE_NONE:
      at_type = ax::mojom::AssistiveTechnologyType::kUnknown;
      break;
    case extensions::api::accessibility_private::
        ASSISTIVE_TECHNOLOGY_TYPE_CHROMEVOX:
      at_type = ax::mojom::AssistiveTechnologyType::kChromeVox;
      break;
    case extensions::api::accessibility_private::
        ASSISTIVE_TECHNOLOGY_TYPE_SELECTTOSPEAK:
      at_type = ax::mojom::AssistiveTechnologyType::kSelectToSpeak;
      break;
    case extensions::api::accessibility_private::
        ASSISTIVE_TECHNOLOGY_TYPE_SWITCHACCESS:
      at_type = ax::mojom::AssistiveTechnologyType::kSwitchAccess;
      break;
    case extensions::api::accessibility_private::
        ASSISTIVE_TECHNOLOGY_TYPE_AUTOCLICK:
      at_type = ax::mojom::AssistiveTechnologyType::kAutoClick;
      break;
    case extensions::api::accessibility_private::
        ASSISTIVE_TECHNOLOGY_TYPE_MAGNIFIER:
      at_type = ax::mojom::AssistiveTechnologyType::kMagnifier;
      break;
    case extensions::api::accessibility_private::
        ASSISTIVE_TECHNOLOGY_TYPE_DICTATION:
      at_type = ax::mojom::AssistiveTechnologyType::kDictation;
      break;
  }

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
        at_type, focus_ring_info.id ? *(focus_ring_info.id) : "");

    if (!content::ParseHexColorString(focus_ring_info.color,
                                      &(focus_ring->color))) {
      return RespondNow(Error("Could not parse hex color"));
    }

    if (focus_ring_info.secondary_color &&
        !content::ParseHexColorString(*(focus_ring_info.secondary_color),
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

    if (focus_ring_info.stacking_order) {
      switch (focus_ring_info.stacking_order) {
        case accessibility_private::
            FOCUS_RING_STACKING_ORDER_ABOVEACCESSIBILITYBUBBLES:
          focus_ring->stacking_order =
              ash::FocusRingStackingOrder::ABOVE_ACCESSIBILITY_BUBBLES;
          break;
        case accessibility_private::
            FOCUS_RING_STACKING_ORDER_BELOWACCESSIBILITYBUBBLES:
          focus_ring->stacking_order =
              ash::FocusRingStackingOrder::BELOW_ACCESSIBILITY_BUBBLES;
          break;
        default:
          NOTREACHED();
      }
    }

    if (focus_ring_info.background_color &&
        !content::ParseHexColorString(*(focus_ring_info.background_color),
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
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetHighlightsFunction::Run() {
  absl::optional<accessibility_private::SetHighlights::Params> params(
      accessibility_private::SetHighlights::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<gfx::Rect> rects;
  for (const accessibility_private::ScreenRect& rect : params->rects) {
    rects.push_back(gfx::Rect(rect.left, rect.top, rect.width, rect.height));
  }

  SkColor color;
  if (!content::ParseHexColorString(params->color, &color)) {
    return RespondNow(Error("Could not parse hex color"));
  }

  // Set the highlights to cover all of these rects.
  AccessibilityManager::Get()->SetHighlights(rects, color);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetKeyboardListenerFunction::Run() {
  CHECK(extension());

  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_bool());
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_bool());
  bool enabled = args()[0].GetBool();
  bool capture = args()[1].GetBool();

  AccessibilityManager* manager = AccessibilityManager::Get();

  const std::string current_id = manager->keyboard_listener_extension_id();
  if (!current_id.empty() && extension()->id() != current_id) {
    return RespondNow(Error("Existing keyboard listener registered."));
  }

  manager->SetKeyboardListenerExtensionId(
      enabled ? extension()->id() : std::string(),
      Profile::FromBrowserContext(browser_context()));

  ash::EventRewriterController::Get()->CaptureAllKeysForSpokenFeedback(
      enabled && capture);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetNativeAccessibilityEnabledFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_bool());
  bool enabled = args()[0].GetBool();
  if (enabled) {
    content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  } else {
    content::BrowserAccessibilityState::GetInstance()->DisableAccessibility();
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetNativeChromeVoxArcSupportForCurrentAppFunction::Run() {
  absl::optional<
      accessibility_private::SetNativeChromeVoxArcSupportForCurrentApp::Params>
      params = accessibility_private::
          SetNativeChromeVoxArcSupportForCurrentApp::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  arc::ArcAccessibilityHelperBridge* bridge =
      arc::ArcAccessibilityHelperBridge::GetForBrowserContext(
          browser_context());
  if (bridge) {
    EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
    EXTENSION_FUNCTION_VALIDATE(args()[0].is_bool());
    bool enabled = args()[0].GetBool();
    bridge->SetNativeChromeVoxArcSupport(
        enabled,
        base::BindOnce(
            &AccessibilityPrivateSetNativeChromeVoxArcSupportForCurrentAppFunction::
                OnResponse,
            this));
    return did_respond() ? AlreadyResponded() : RespondLater();
  }
  return RespondNow(ArgumentList(
      extensions::api::accessibility_private::
          SetNativeChromeVoxArcSupportForCurrentApp::Results::Create(
              extensions::api::accessibility_private::
                  SetNativeChromeVoxResponse::
                      SET_NATIVE_CHROME_VOX_RESPONSE_FAILURE)));
}

void AccessibilityPrivateSetNativeChromeVoxArcSupportForCurrentAppFunction::
    OnResponse(
        extensions::api::accessibility_private::SetNativeChromeVoxResponse
            response) {
  Respond(ArgumentList(
      extensions::api::accessibility_private::
          SetNativeChromeVoxArcSupportForCurrentApp::Results::Create(
              response)));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetPointScanStateFunction::Run() {
  absl::optional<accessibility_private::SetPointScanState::Params> params =
      accessibility_private::SetPointScanState::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::PointScanState params_state = params->state;

  switch (params_state) {
    case accessibility_private::PointScanState::POINT_SCAN_STATE_START:
      ash::AccessibilityController::Get()->StartPointScan();
      break;
    case accessibility_private::PointScanState::POINT_SCAN_STATE_STOP:
      ash::AccessibilityController::Get()->StopPointScan();
      break;
    case accessibility_private::PointScanState::POINT_SCAN_STATE_NONE:
      break;
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetSelectToSpeakStateFunction::Run() {
  absl::optional<accessibility_private::SetSelectToSpeakState::Params> params =
      accessibility_private::SetSelectToSpeakState::Params::Create(args());
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

  auto* accessibility_manager = AccessibilityManager::Get();
  accessibility_manager->SetSelectToSpeakState(state);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetVirtualKeyboardVisibleFunction::Run() {
  absl::optional<accessibility_private::SetVirtualKeyboardVisible::Params>
      params = accessibility_private::SetVirtualKeyboardVisible::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ash::AccessibilityController::Get()->SetVirtualKeyboardVisible(
      params->is_visible);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction AccessibilityPrivateShowToastFunction::Run() {
  absl::optional<accessibility_private::ShowToast::Params> params(
      accessibility_private::ShowToast::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  ash::AccessibilityController::Get()->ShowToast(
      ConvertToastType(params->type));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateShowConfirmationDialogFunction::Run() {
  absl::optional<accessibility_private::ShowConfirmationDialog::Params> params =
      accessibility_private::ShowConfirmationDialog::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::u16string title = base::UTF8ToUTF16(params->title);
  std::u16string description = base::UTF8ToUTF16(params->description);
  std::u16string cancel_name =
      params->cancel_name ? base::UTF8ToUTF16(params->cancel_name.value())
                          : l10n_util::GetStringUTF16(IDS_APP_CANCEL);
  ash::AccessibilityController::Get()->ShowConfirmationDialog(
      title, description, cancel_name,
      base::BindOnce(
          &AccessibilityPrivateShowConfirmationDialogFunction::OnDialogResult,
          this, /* confirmed */ true),
      base::BindOnce(
          &AccessibilityPrivateShowConfirmationDialogFunction::OnDialogResult,
          this, /* not confirmed */ false),
      base::BindOnce(
          &AccessibilityPrivateShowConfirmationDialogFunction::OnDialogResult,
          this, /* not confirmed */ false));

  return RespondLater();
}

void AccessibilityPrivateShowConfirmationDialogFunction::OnDialogResult(
    bool confirmed) {
  Respond(WithArguments(confirmed));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSilenceSpokenFeedbackFunction::Run() {
  ash::AccessibilityController::Get()->SilenceSpokenFeedback();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateToggleDictationFunction::Run() {
  ash::DictationToggleSource source = ash::DictationToggleSource::kChromevox;
  if (extension()->id() == extension_misc::kSwitchAccessExtensionId) {
    source = ash::DictationToggleSource::kSwitchAccess;
  } else if (extension()->id() == extension_misc::kChromeVoxExtensionId) {
    source = ash::DictationToggleSource::kChromevox;
  } else if (extension()->id() ==
             extension_misc::kAccessibilityCommonExtensionId) {
    source = ash::DictationToggleSource::kAccessibilityCommon;
  } else {
    NOTREACHED();
  }

  ash::AccessibilityController::Get()->ToggleDictationFromSource(source);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateUpdateDictationBubbleFunction::Run() {
  absl::optional<accessibility_private::UpdateDictationBubble::Params> params(
      accessibility_private::UpdateDictationBubble::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::DictationBubbleProperties& properties =
      params->properties;

  // Extract the icon type.
  ash::DictationBubbleIconType icon = ash::DictationBubbleIconType::kHidden;
  switch (properties.icon) {
    case accessibility_private::DictationBubbleIconType::
        DICTATION_BUBBLE_ICON_TYPE_HIDDEN:
      icon = ash::DictationBubbleIconType::kHidden;
      break;
    case accessibility_private::DictationBubbleIconType::
        DICTATION_BUBBLE_ICON_TYPE_STANDBY:
      icon = ash::DictationBubbleIconType::kStandby;
      break;
    case accessibility_private::DictationBubbleIconType::
        DICTATION_BUBBLE_ICON_TYPE_MACROSUCCESS:
      icon = ash::DictationBubbleIconType::kMacroSuccess;
      break;
    case accessibility_private::DictationBubbleIconType::
        DICTATION_BUBBLE_ICON_TYPE_MACROFAIL:
      icon = ash::DictationBubbleIconType::kMacroFail;
      break;
    case accessibility_private::DictationBubbleIconType::
        DICTATION_BUBBLE_ICON_TYPE_NONE:
      NOTREACHED();
      break;
  }

  // Extract text.
  absl::optional<std::u16string> text;
  if (properties.text) {
    text = base::UTF8ToUTF16(*properties.text);
  }

  // Extract hints.
  absl::optional<std::vector<ash::DictationBubbleHintType>> hints;
  if (properties.hints) {
    std::vector<ash::DictationBubbleHintType> converted_hints;
    for (size_t i = 0; i < (*properties.hints).size(); ++i) {
      converted_hints.emplace_back(
          ConvertDictationHintType((*properties.hints)[i]));
    }
    hints = std::move(converted_hints);
  }

  if (hints.has_value() && hints.value().size() > 5) {
    return RespondNow(Error("Should not provide more than five hints."));
  }

  ash::AccessibilityController::Get()->UpdateDictationBubble(properties.visible,
                                                             icon, text, hints);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateUpdateSelectToSpeakPanelFunction::Run() {
  absl::optional<accessibility_private::UpdateSelectToSpeakPanel::Params>
      params = accessibility_private::UpdateSelectToSpeakPanel::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!params->show) {
    ash::AccessibilityController::Get()->HideSelectToSpeakPanel();
    return RespondNow(NoArguments());
  }

  if (!params->anchor || !params->is_paused || !params->speed) {
    return RespondNow(Error("Required parameters missing to show panel."));
  }

  const gfx::Rect anchor =
      gfx::Rect(params->anchor->left, params->anchor->top,
                params->anchor->width, params->anchor->height);
  ash::AccessibilityController::Get()->ShowSelectToSpeakPanel(
      anchor, *params->is_paused, *params->speed);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateUpdateSwitchAccessBubbleFunction::Run() {
  absl::optional<accessibility_private::UpdateSwitchAccessBubble::Params>
      params = accessibility_private::UpdateSwitchAccessBubble::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!params->show) {
    if (params->bubble ==
        accessibility_private::SWITCH_ACCESS_BUBBLE_BACKBUTTON) {
      ash::AccessibilityController::Get()->HideSwitchAccessBackButton();
    } else if (params->bubble ==
               accessibility_private::SWITCH_ACCESS_BUBBLE_MENU) {
      ash::AccessibilityController::Get()->HideSwitchAccessMenu();
    }
    return RespondNow(NoArguments());
  }

  if (!params->anchor) {
    return RespondNow(Error("An anchor rect is required to show a bubble."));
  }

  gfx::Rect anchor(params->anchor->left, params->anchor->top,
                   params->anchor->width, params->anchor->height);

  if (params->bubble ==
      accessibility_private::SWITCH_ACCESS_BUBBLE_BACKBUTTON) {
    ash::AccessibilityController::Get()->ShowSwitchAccessBackButton(anchor);
    return RespondNow(NoArguments());
  }

  if (!params->actions) {
    return RespondNow(Error("The menu cannot be shown without actions."));
  }

  std::vector<std::string> actions_to_show;
  for (accessibility_private::SwitchAccessMenuAction extension_action :
       *(params->actions)) {
    std::string action = accessibility_private::ToString(extension_action);
    // Check that this action is not already in our actions list.
    if (base::Contains(actions_to_show, action)) {
      continue;
    }
    actions_to_show.push_back(action);
  }

  ash::AccessibilityController::Get()->ShowSwitchAccessMenu(anchor,
                                                            actions_to_show);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateIsLacrosPrimaryFunction::Run() {
  return RespondNow(
      WithArguments(crosapi::lacros_startup_state::IsLacrosEnabled()));
}

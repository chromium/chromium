// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_extension_api_ash.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/accessibility_focus_ring_info.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "ash/public/cpp/window_tree_host_lookup.h"
#include "ash/webui/settings/public/constants/routes_util.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
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
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "services/accessibility/public/mojom/assistive_technology_type.mojom.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/dom/dom_codes_array.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

namespace accessibility_private = ::extensions::api::accessibility_private;
using ::ash::AccessibilityManager;

ash::AccessibilityToastType ConvertToastType(
    accessibility_private::ToastType type) {
  switch (type) {
    case accessibility_private::ToastType::kDictationNoFocusedTextField:
      return ash::AccessibilityToastType::kDictationNoFocusedTextField;
    case accessibility_private::ToastType::kDictationMicMuted:
      return ash::AccessibilityToastType::kDictationMicMuted;
    case accessibility_private::ToastType::kNone:
      NOTREACHED();
  }
}

ash::DictationBubbleHintType ConvertDictationHintType(
    accessibility_private::DictationBubbleHintType hint_type) {
  switch (hint_type) {
    case accessibility_private::DictationBubbleHintType::kTrySaying:
      return ash::DictationBubbleHintType::kTrySaying;
    case accessibility_private::DictationBubbleHintType::kType:
      return ash::DictationBubbleHintType::kType;
    case accessibility_private::DictationBubbleHintType::kDelete:
      return ash::DictationBubbleHintType::kDelete;
    case accessibility_private::DictationBubbleHintType::kSelectAll:
      return ash::DictationBubbleHintType::kSelectAll;
    case accessibility_private::DictationBubbleHintType::kUndo:
      return ash::DictationBubbleHintType::kUndo;
    case accessibility_private::DictationBubbleHintType::kHelp:
      return ash::DictationBubbleHintType::kHelp;
    case accessibility_private::DictationBubbleHintType::kUnselect:
      return ash::DictationBubbleHintType::kUnselect;
    case accessibility_private::DictationBubbleHintType::kCopy:
      return ash::DictationBubbleHintType::kCopy;
    case accessibility_private::DictationBubbleHintType::kNone:
      NOTREACHED_IN_MIGRATION();
      return ash::DictationBubbleHintType::kTrySaying;
  }
}

ash::AccessibilityScrollDirection ConvertScrollDirection(
    accessibility_private::ScrollDirection direction) {
  switch (direction) {
    case accessibility_private::ScrollDirection::kUp:
      return ash::AccessibilityScrollDirection::kUp;
    case accessibility_private::ScrollDirection::kDown:
      return ash::AccessibilityScrollDirection::kDown;
    case accessibility_private::ScrollDirection::kLeft:
      return ash::AccessibilityScrollDirection::kLeft;
    case accessibility_private::ScrollDirection::kRight:
      return ash::AccessibilityScrollDirection::kRight;
    case accessibility_private::ScrollDirection::kNone:
      NOTREACHED_NORETURN();
  }
}

std::string ConvertFacialGestureType(
    accessibility_private::FacialGesture gesture_type) {
  switch (gesture_type) {
    case accessibility_private::FacialGesture::kBrowInnerUp:
      return "browInnerUp";
    case accessibility_private::FacialGesture::kBrowsDown:
      return "browsDown";
    case accessibility_private::FacialGesture::kEyeSquintLeft:
      return "eyeSquintLeft";
    case accessibility_private::FacialGesture::kEyeSquintRight:
      return "eyeSquintRight";
    case accessibility_private::FacialGesture::kEyesBlink:
      return "eyesBlink";
    case accessibility_private::FacialGesture::kEyesLookDown:
      return "eyesLookDown";
    case accessibility_private::FacialGesture::kEyesLookLeft:
      return "eyesLookLeft";
    case accessibility_private::FacialGesture::kEyesLookRight:
      return "eyesLookRight";
    case accessibility_private::FacialGesture::kEyesLookUp:
      return "eyesLookUp";
    case accessibility_private::FacialGesture::kJawLeft:
      return "jawLeft";
    case accessibility_private::FacialGesture::kJawOpen:
      return "jawOpen";
    case accessibility_private::FacialGesture::kJawRight:
      return "jawRight";
    case accessibility_private::FacialGesture::kMouthFunnel:
      return "mouthFunnel";
    case accessibility_private::FacialGesture::kMouthLeft:
      return "mouthLeft";
    case accessibility_private::FacialGesture::kMouthPucker:
      return "mouthPucker";
    case accessibility_private::FacialGesture::kMouthRight:
      return "mouthRight";
    case accessibility_private::FacialGesture::kMouthSmile:
      return "mouthSmile";
    case accessibility_private::FacialGesture::kMouthUpperUp:
      return "mouthUpperUp";
    case accessibility_private::FacialGesture::kNone:
      NOTREACHED_NORETURN();
  }
}

void DispatchAccessibilityFocusChangedEvent(
    extensions::events::HistogramValue histogram_value,
    extensions::api::accessibility_private::ScreenRect bounds) {
  std::unique_ptr<extensions::Event> event;

  if (histogram_value ==
      extensions::events::ACCESSIBILITY_PRIVATE_ON_CHROMEVOX_FOCUS_CHANGED) {
    auto event_args =
        extensions::api::accessibility_private::OnChromeVoxFocusChanged::Create(
            bounds);
    event = std::make_unique<extensions::Event>(
        histogram_value,
        extensions::api::accessibility_private::OnChromeVoxFocusChanged::
            kEventName,
        std::move(event_args));
  } else if (histogram_value ==
             extensions::events::
                 ACCESSIBILITY_PRIVATE_ON_SELECT_TO_SPEAK_FOCUS_CHANGED) {
    auto event_args = extensions::api::accessibility_private::
        OnSelectToSpeakFocusChanged::Create(bounds);
    event = std::make_unique<extensions::Event>(
        histogram_value,
        extensions::api::accessibility_private::OnSelectToSpeakFocusChanged::
            kEventName,
        std::move(event_args));
  }

  extensions::EventRouter::Get(AccessibilityManager::Get()->profile())
      ->DispatchEventWithLazyListener(
          extension_misc::kAccessibilityCommonExtensionId, std::move(event));
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
AccessibilityPrivateSetCursorPositionFunction::Run() {
  std::optional<accessibility_private::SetCursorPosition::Params> params =
      accessibility_private::SetCursorPosition::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  gfx::Point location_in_screen(params->point.x, params->point.y);
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(location_in_screen);
  auto* host = ash::GetWindowTreeHostForDisplay(display.id());
  if (!host) {
    return RespondNow(Error("Unable to find a window tree host"));
  }
  aura::Window* root_window = host->window();
  if (!root_window) {
    return RespondNow(Error("Unable to get root window"));
  }
  gfx::Point location_in_window(location_in_screen);
  ::wm::ConvertPointFromScreen(root_window, &location_in_window);
  host->MoveCursorToLocationInDIP(location_in_window);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateGetDisplayBoundsFunction::Run() {
  const std::vector<display::Display>& displays =
      display::Screen::GetScreen()->GetAllDisplays();
  base::Value::List result;
  for (auto& display : displays) {
    const gfx::Rect& bounds = display.bounds();
    auto screen_rect = accessibility_private::ScreenRect();
    screen_rect.left = bounds.x();
    screen_rect.top = bounds.y();
    screen_rect.width = bounds.width();
    screen_rect.height = bounds.height();
    result.Append(screen_rect.ToValue());
  }
  return RespondNow(WithArguments(std::move(result)));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateForwardKeyEventsToSwitchAccessFunction::Run() {
  std::optional<accessibility_private::ForwardKeyEventsToSwitchAccess::Params>
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

// TODO(b/286296201): AccessibilityPrivateGetDlcContentsFunction is deprecated
// (use AccessibilityPrivateGetTtsDlcContentsFunction instead). Delete
// GetDlcContents after uprreving Google TTS to use GetTtsDlcContents.
ExtensionFunction::ResponseAction
AccessibilityPrivateGetDlcContentsFunction::Run() {
  std::optional<accessibility_private::GetDlcContents::Params> params(
      accessibility_private::GetDlcContents::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::DlcType dlc = params->dlc;

  AccessibilityManager::Get()->GetTtsDlcContents(
      dlc, accessibility_private::TtsVariant::kLite,
      base::BindOnce(
          &AccessibilityPrivateGetDlcContentsFunction::OnDlcContentsRetrieved,
          this));
  return RespondLater();
}

void AccessibilityPrivateGetDlcContentsFunction::OnDlcContentsRetrieved(
    const std::vector<uint8_t>& contents,
    std::optional<std::string> error) {
  if (error.has_value()) {
    Respond(Error(error.value()));
    return;
  }

  Respond(WithArguments(base::Value(contents)));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateGetTtsDlcContentsFunction::Run() {
  std::optional<accessibility_private::GetTtsDlcContents::Params> params(
      accessibility_private::GetTtsDlcContents::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::DlcType dlc = params->dlc;
  accessibility_private::TtsVariant variant = params->variant;

  AccessibilityManager::Get()->GetTtsDlcContents(
      dlc, variant,
      base::BindOnce(&AccessibilityPrivateGetTtsDlcContentsFunction::
                         OnTtsDlcContentsRetrieved,
                     this));
  return RespondLater();
}

void AccessibilityPrivateGetTtsDlcContentsFunction::OnTtsDlcContentsRetrieved(
    const std::vector<uint8_t>& contents,
    std::optional<std::string> error) {
  if (error.has_value()) {
    Respond(Error(error.value()));
    return;
  }

  Respond(WithArguments(base::Value(contents)));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateGetLocalizedDomKeyStringForKeyCodeFunction::Run() {
  std::optional<
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
  std::optional<
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
AccessibilityPrivateInstallFaceGazeAssetsFunction::Run() {
  AccessibilityManager::Get()->InstallFaceGazeAssets(base::BindOnce(
      &AccessibilityPrivateInstallFaceGazeAssetsFunction::OnInstallFinished,
      this));
  return RespondLater();
}

void AccessibilityPrivateInstallFaceGazeAssetsFunction::OnInstallFinished(
    std::optional<accessibility_private::FaceGazeAssets> assets) {
  if (!assets) {
    Respond(Error("Couldn't retrieve FaceGaze assets."));
    return;
  }

  Respond(WithArguments(assets->ToValue()));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSendGestureInfoToSettingsFunction::Run() {
  std::optional<accessibility_private::SendGestureInfoToSettings::Params>
      params(accessibility_private::SendGestureInfoToSettings::Params::Create(
          args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<ash::FaceGazeGestureInfo> converted_gesture_info_array;

  for (const accessibility_private::GestureInfo& gesture_info :
       params->gesture_info) {
    auto converted_gesture_info = ash::FaceGazeGestureInfo();
    converted_gesture_info.confidence = gesture_info.confidence;
    converted_gesture_info.gesture =
        ConvertFacialGestureType(gesture_info.gesture);

    converted_gesture_info_array.push_back(converted_gesture_info);
  }

  AccessibilityManager::Get()->SendGestureInfoToSettings(
      std::move(converted_gesture_info_array));
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
        std::optional<accessibility_private::PumpkinData> data) {
  if (!data) {
    Respond(Error("Couldn't retrieve Pumpkin data."));
    return;
  }

  Respond(WithArguments(data->ToValue()));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateIsFeatureEnabledFunction::Run() {
  std::optional<accessibility_private::IsFeatureEnabled::Params> params =
      accessibility_private::IsFeatureEnabled::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::AccessibilityFeature params_feature = params->feature;
  bool enabled;
  switch (params_feature) {
    case accessibility_private::AccessibilityFeature::
        kGoogleTtsHighQualityVoices:
      enabled = ::features::
          IsExperimentalAccessibilityGoogleTtsHighQualityVoicesEnabled();
      break;
    case accessibility_private::AccessibilityFeature::kDictationContextChecking:
      enabled = ::features::
          IsExperimentalAccessibilityDictationContextCheckingEnabled();
      break;
    case accessibility_private::AccessibilityFeature::kFaceGaze:
      enabled = ::features::IsAccessibilityFaceGazeEnabled();
      break;
    case accessibility_private::AccessibilityFeature::kFaceGazeGravityWells:
      enabled = ::features::IsAccessibilityFaceGazeGravityWellsEnabled();
      break;
    case accessibility_private::AccessibilityFeature::kNone:
      return RespondNow(Error("Unrecognized feature"));
  }

  return RespondNow(WithArguments(enabled));
}

ExtensionFunction::ResponseAction
AccessibilityPrivateMagnifierCenterOnPointFunction::Run() {
  std::optional<accessibility_private::MagnifierCenterOnPoint::Params> params =
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
  std::optional<accessibility_private::MoveMagnifierToRect::Params> params =
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
  const std::optional<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  AccessibilityManager::Get()->OpenSettingsSubpage(params->subpage);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivatePerformAcceleratorActionFunction::Run() {
  std::optional<accessibility_private::PerformAcceleratorAction::Params>
      params = accessibility_private::PerformAcceleratorAction::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(params);
  ash::AcceleratorAction accelerator_action;
  switch (params->accelerator_action) {
    case accessibility_private::AcceleratorAction::kFocusPreviousPane:
      accelerator_action = ash::AcceleratorAction::kFocusPreviousPane;
      break;
    case accessibility_private::AcceleratorAction::kFocusNextPane:
      accelerator_action = ash::AcceleratorAction::kFocusNextPane;
      break;
    case accessibility_private::AcceleratorAction::kNone:
      NOTREACHED_IN_MIGRATION();
      return RespondNow(Error("Invalid accelerator action."));
  }

  ash::AccessibilityController::Get()->PerformAcceleratorAction(
      accelerator_action);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSendSyntheticKeyEventFunction::Run() {
  std::optional<accessibility_private::SendSyntheticKeyEvent::Params> params =
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
              accessibility_private::SyntheticKeyboardEventType::kKeyup
          ? ui::EventType::kKeyReleased
          : ui::EventType::kKeyPressed,
      keyboard_code, ui::UsLayoutKeyboardCodeToDomCode(keyboard_code),
      modifiers);

  auto* host = ash::GetWindowTreeHostForDisplay(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  DCHECK(host);

  bool dictation_enabled = AccessibilityManager::Get()->IsDictationEnabled();
  bool from_accessibility_common =
      extension_id() == extension_misc::kAccessibilityCommonExtensionId;
  bool facegaze_enabled = AccessibilityManager::Get()->IsFaceGazeEnabled();
  if ((dictation_enabled || facegaze_enabled) && from_accessibility_common &&
      params->use_rewriters.has_value() && params->use_rewriters.value()) {
    // TODO(b/259397131): Remove the `useRewriters` property.
    // Call SendEventToSink so that the event can be processed by event
    // rewriters, like Game Controls.
    host->SendEventToSink(&synthetic_key_event);
  } else {
    host->DeliverEventToSink(&synthetic_key_event);
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSendSyntheticMouseEventFunction::Run() {
  std::optional<accessibility_private::SendSyntheticMouseEvent::Params> params =
      accessibility_private::SendSyntheticMouseEvent::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::SyntheticMouseEvent* mouse_data = &params->mouse_event;

  ui::EventType type = ui::EventType::kUnknown;
  switch (mouse_data->type) {
    case accessibility_private::SyntheticMouseEventType::kPress:
      type = ui::EventType::kMousePressed;
      break;
    case accessibility_private::SyntheticMouseEventType::kRelease:
      type = ui::EventType::kMouseReleased;
      break;
    case accessibility_private::SyntheticMouseEventType::kDrag:
      type = ui::EventType::kMouseDragged;
      break;
    case accessibility_private::SyntheticMouseEventType::kMove:
      type = ui::EventType::kMouseMoved;
      break;
    case accessibility_private::SyntheticMouseEventType::kEnter:
      type = ui::EventType::kMouseEntered;
      break;
    case accessibility_private::SyntheticMouseEventType::kExit:
      type = ui::EventType::kMouseExited;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  int flags = 0;
  if (type != ui::EventType::kMouseMoved) {
    switch (mouse_data->mouse_button) {
      case accessibility_private::SyntheticMouseEventButton::kLeft:
        flags |= ui::EF_LEFT_MOUSE_BUTTON;
        break;
      case accessibility_private::SyntheticMouseEventButton::kMiddle:
        flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
        break;
      case accessibility_private::SyntheticMouseEventButton::kRight:
        flags |= ui::EF_RIGHT_MOUSE_BUTTON;
        break;
      case accessibility_private::SyntheticMouseEventButton::kBack:
        flags |= ui::EF_BACK_MOUSE_BUTTON;
        break;
      case accessibility_private::SyntheticMouseEventButton::kFoward:
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

  if (mouse_data->is_double_click && *(mouse_data->is_double_click)) {
    flags |= ui::EF_IS_DOUBLE_CLICK;
  }

  // Locations are assumed to be in screen coordinates.
  gfx::Point location_in_screen(mouse_data->x, mouse_data->y);
  AccessibilityManager::Get()->SendSyntheticMouseEvent(
      type, flags, changed_button_flags, location_in_screen);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateScrollAtPointFunction::Run() {
  std::optional<accessibility_private::ScrollAtPoint::Params> params =
      accessibility_private::ScrollAtPoint::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  gfx::Point point(params->target.x, params->target.y);
  ash::AccessibilityController::Get()->ScrollAtPoint(
      point, ConvertScrollDirection(params->direction));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetFocusRingsFunction::Run() {
  std::optional<accessibility_private::SetFocusRings::Params> params(
      accessibility_private::SetFocusRings::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* accessibility_manager = AccessibilityManager::Get();

  ax::mojom::AssistiveTechnologyType at_type;
  switch (params->at_type) {
    case extensions::api::accessibility_private::AssistiveTechnologyType::kNone:
      at_type = ax::mojom::AssistiveTechnologyType::kUnknown;
      break;
    case extensions::api::accessibility_private::AssistiveTechnologyType::
        kChromeVox:
      at_type = ax::mojom::AssistiveTechnologyType::kChromeVox;
      break;
    case extensions::api::accessibility_private::AssistiveTechnologyType::
        kSelectToSpeak:
      at_type = ax::mojom::AssistiveTechnologyType::kSelectToSpeak;
      break;
    case extensions::api::accessibility_private::AssistiveTechnologyType::
        kSwitchAccess:
      at_type = ax::mojom::AssistiveTechnologyType::kSwitchAccess;
      break;
    case extensions::api::accessibility_private::AssistiveTechnologyType::
        kAutoClick:
      at_type = ax::mojom::AssistiveTechnologyType::kAutoClick;
      break;
    case extensions::api::accessibility_private::AssistiveTechnologyType::
        kMagnifier:
      at_type = ax::mojom::AssistiveTechnologyType::kMagnifier;
      break;
    case extensions::api::accessibility_private::AssistiveTechnologyType::
        kDictation:
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
      case accessibility_private::FocusType::kSolid:
        focus_ring->type = ash::FocusRingType::SOLID;
        break;
      case accessibility_private::FocusType::kDashed:
        focus_ring->type = ash::FocusRingType::DASHED;
        break;
      case accessibility_private::FocusType::kGlow:
        focus_ring->type = ash::FocusRingType::GLOW;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }

    if (focus_ring_info.stacking_order !=
        accessibility_private::FocusRingStackingOrder::kNone) {
      switch (focus_ring_info.stacking_order) {
        case accessibility_private::FocusRingStackingOrder::
            kAboveAccessibilityBubbles:
          focus_ring->stacking_order =
              ash::FocusRingStackingOrder::ABOVE_ACCESSIBILITY_BUBBLES;
          break;
        case accessibility_private::FocusRingStackingOrder::
            kBelowAccessibilityBubbles:
          focus_ring->stacking_order =
              ash::FocusRingStackingOrder::BELOW_ACCESSIBILITY_BUBBLES;
          break;
        default:
          NOTREACHED_IN_MIGRATION();
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
  std::optional<accessibility_private::SetHighlights::Params> params(
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

// TODO(b/347953090): Investigate a generic SetAccessibilityFocusFunction to
// share code for extensibility.
ExtensionFunction::ResponseAction
AccessibilityPrivateSetChromeVoxFocusFunction::Run() {
  std::optional<accessibility_private::SetChromeVoxFocus::Params> params(
      accessibility_private::SetChromeVoxFocus::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!features::IsAccessibilityMagnifierFollowsChromeVoxEnabled()) {
    return RespondNow(NoArguments());
  }

  if (!ash::AccessibilityController::Get()->fullscreen_magnifier().enabled() &&
      !ash::AccessibilityController::Get()->docked_magnifier().enabled()) {
    return RespondNow(NoArguments());
  }

  // Ship this event to AccessibilityCommon for docked or fullscreen
  // magnifier.
  extensions::api::accessibility_private::ScreenRect bounds;
  bounds.left = params->bounds.left;
  bounds.top = params->bounds.top;
  bounds.width = params->bounds.width;
  bounds.height = params->bounds.height;
  DispatchAccessibilityFocusChangedEvent(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_CHROMEVOX_FOCUS_CHANGED,
      std::move(bounds));

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetSelectToSpeakFocusFunction::Run() {
  std::optional<accessibility_private::SetSelectToSpeakFocus::Params> params(
      accessibility_private::SetSelectToSpeakFocus::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!features::IsAccessibilityMagnifierFollowsStsEnabled()) {
    return RespondNow(NoArguments());
  }

  if (!ash::AccessibilityController::Get()->fullscreen_magnifier().enabled() &&
      !ash::AccessibilityController::Get()->docked_magnifier().enabled()) {
    return RespondNow(NoArguments());
  }

  // Ship this event to AccessibilityCommon for docked or fullscreen
  // magnifier.
  extensions::api::accessibility_private::ScreenRect bounds;
  bounds.left = params->bounds.left;
  bounds.top = params->bounds.top;
  bounds.width = params->bounds.width;
  bounds.height = params->bounds.height;
  DispatchAccessibilityFocusChangedEvent(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_CHROMEVOX_FOCUS_CHANGED,
      std::move(bounds));

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
  std::optional<
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
                  SetNativeChromeVoxResponse::kFailure)));
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
  std::optional<accessibility_private::SetPointScanState::Params> params =
      accessibility_private::SetPointScanState::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::PointScanState params_state = params->state;

  switch (params_state) {
    case accessibility_private::PointScanState::kStart:
      ash::AccessibilityController::Get()->StartPointScan();
      break;
    case accessibility_private::PointScanState::kStop:
      ash::AccessibilityController::Get()->StopPointScan();
      break;
    case accessibility_private::PointScanState::kNone:
      break;
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetSelectToSpeakStateFunction::Run() {
  std::optional<accessibility_private::SetSelectToSpeakState::Params> params =
      accessibility_private::SetSelectToSpeakState::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::SelectToSpeakState params_state = params->state;
  ash::SelectToSpeakState state;
  switch (params_state) {
    case accessibility_private::SelectToSpeakState::kSelecting:
      state = ash::SelectToSpeakState::kSelectToSpeakStateSelecting;
      break;
    case accessibility_private::SelectToSpeakState::kSpeaking:
      state = ash::SelectToSpeakState::kSelectToSpeakStateSpeaking;
      break;
    case accessibility_private::SelectToSpeakState::kInactive:
    case accessibility_private::SelectToSpeakState::kNone:
      state = ash::SelectToSpeakState::kSelectToSpeakStateInactive;
  }

  auto* accessibility_manager = AccessibilityManager::Get();
  accessibility_manager->SetSelectToSpeakState(state);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateSetVirtualKeyboardVisibleFunction::Run() {
  std::optional<accessibility_private::SetVirtualKeyboardVisible::Params>
      params = accessibility_private::SetVirtualKeyboardVisible::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ash::AccessibilityController::Get()->SetVirtualKeyboardVisible(
      params->is_visible);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction AccessibilityPrivateShowToastFunction::Run() {
  std::optional<accessibility_private::ShowToast::Params> params(
      accessibility_private::ShowToast::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  ash::AccessibilityController::Get()->ShowToast(
      ConvertToastType(params->type));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateShowConfirmationDialogFunction::Run() {
  std::optional<accessibility_private::ShowConfirmationDialog::Params> params =
      accessibility_private::ShowConfirmationDialog::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::u16string title = base::UTF8ToUTF16(params->title);
  std::u16string description = base::UTF8ToUTF16(params->description);
  std::u16string confirm = l10n_util::GetStringUTF16(IDS_APP_CONTINUE);
  std::u16string cancel_name =
      params->cancel_name ? base::UTF8ToUTF16(params->cancel_name.value())
                          : l10n_util::GetStringUTF16(IDS_APP_CANCEL);
  ash::AccessibilityController::Get()->ShowConfirmationDialog(
      title, description, confirm, cancel_name,
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
    NOTREACHED_IN_MIGRATION();
  }

  ash::AccessibilityController::Get()->ToggleDictationFromSource(source);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateUpdateDictationBubbleFunction::Run() {
  std::optional<accessibility_private::UpdateDictationBubble::Params> params(
      accessibility_private::UpdateDictationBubble::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  accessibility_private::DictationBubbleProperties& properties =
      params->properties;

  // Extract the icon type.
  ash::DictationBubbleIconType icon = ash::DictationBubbleIconType::kHidden;
  switch (properties.icon) {
    case accessibility_private::DictationBubbleIconType::kHidden:
      icon = ash::DictationBubbleIconType::kHidden;
      break;
    case accessibility_private::DictationBubbleIconType::kStandby:
      icon = ash::DictationBubbleIconType::kStandby;
      break;
    case accessibility_private::DictationBubbleIconType::kMacroSuccess:
      icon = ash::DictationBubbleIconType::kMacroSuccess;
      break;
    case accessibility_private::DictationBubbleIconType::kMacroFail:
      icon = ash::DictationBubbleIconType::kMacroFail;
      break;
    case accessibility_private::DictationBubbleIconType::kNone:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  // Extract text.
  std::optional<std::u16string> text;
  if (properties.text) {
    text = base::UTF8ToUTF16(*properties.text);
  }

  // Extract hints.
  std::optional<std::vector<ash::DictationBubbleHintType>> hints;
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
AccessibilityPrivateUpdateFaceGazeBubbleFunction::Run() {
  std::optional<accessibility_private::UpdateFaceGazeBubble::Params> params(
      accessibility_private::UpdateFaceGazeBubble::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  ash::AccessibilityController::Get()->UpdateFaceGazeBubble(
      base::UTF8ToUTF16(params->text));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AccessibilityPrivateUpdateSelectToSpeakPanelFunction::Run() {
  std::optional<accessibility_private::UpdateSelectToSpeakPanel::Params>
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
  std::optional<accessibility_private::UpdateSwitchAccessBubble::Params>
      params = accessibility_private::UpdateSwitchAccessBubble::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!params->show) {
    if (params->bubble ==
        accessibility_private::SwitchAccessBubble::kBackButton) {
      ash::AccessibilityController::Get()->HideSwitchAccessBackButton();
    } else if (params->bubble ==
               accessibility_private::SwitchAccessBubble::kMenu) {
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
      accessibility_private::SwitchAccessBubble::kBackButton) {
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

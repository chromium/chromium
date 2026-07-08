// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/events_api.h"

#include <optional>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_manager.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/events_api_converters.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/common/chromeos/extensions/api/events.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/features/feature_provider.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

constexpr char kKeyboardDiagnosticsUrl[] =
    "chrome://diagnostics?input&showDefaultKeyboardTester";

namespace cx_events = ::chromeos::api::os_events;

void OpenDiagnosticsKeyboardPage(content::BrowserContext* browser_context) {
  NavigateParams navigate_params(Profile::FromBrowserContext(browser_context),
                                 GURL(kKeyboardDiagnosticsUrl),
                                 ui::PAGE_TRANSITION_FIRST);
  Navigate(&navigate_params);
}

std::string GetFeatureName(cx_events::EventCategory category) {
  switch (category) {
    case cx_events::EventCategory::kNone:
      return "";
    case cx_events::EventCategory::kAudioJack:
      return "os.events.onAudioJackEvent";
    case cx_events::EventCategory::kLid:
      return "os.events.onLidEvent";
    case cx_events::EventCategory::kUsb:
      return "os.events.onUsbEvent";
    case cx_events::EventCategory::kExternalDisplay:
      return "os.events.onExternalDisplayEvent";
    case cx_events::EventCategory::kSdCard:
      return "os.events.onSdCardEvent";
    case cx_events::EventCategory::kPower:
      return "os.events.onPowerEvent";
    case cx_events::EventCategory::kKeyboardDiagnostic:
      return "os.events.onKeyboardDiagnosticEvent";
    case cx_events::EventCategory::kStylusGarage:
      return "os.events.onStylusGarageEvent";
    case cx_events::EventCategory::kTouchpadButton:
      return "os.events.onTouchpadButtonEvent";
    case cx_events::EventCategory::kTouchpadTouch:
      return "os.events.onTouchpadTouchEvent";
    case cx_events::EventCategory::kTouchpadConnected:
      return "os.events.onTouchpadConnectedEvent";
    case cx_events::EventCategory::kTouchscreenTouch:
      return "os.events.onTouchscreenTouchEvent";
    case cx_events::EventCategory::kTouchscreenConnected:
      return "os.events.onTouchscreenConnectedEvent";
    case cx_events::EventCategory::kStylusTouch:
      return "os.events.onStylusTouchEvent";
    case cx_events::EventCategory::kStylusConnected:
      return "os.events.onStylusConnectedEvent";
  }
  NOTREACHED();
}

}  // namespace

// EventsApiFunctionBase -------------------------------------------------------

EventsApiFunctionBase::EventsApiFunctionBase() = default;

EventsApiFunctionBase::~EventsApiFunctionBase() = default;

template <class Params>
std::optional<Params> EventsApiFunctionBase::GetParams() {
  auto params = Params::Create(args());
  if (!params) {
    SetBadMessage();
    Respond(BadMessage());
  }

  return params;
}

// OsEventsIsEventSupportedFunction --------------------------------------------

void OsEventsIsEventSupportedFunction::RunIfAllowed() {
  const auto params = GetParams<cx_events::IsEventSupported::Params>();
  if (!params) {
    return;
  }

  std::string feature_name = GetFeatureName(params->category);
  const auto* feature =
      extensions::FeatureProvider::GetAPIFeatures()->GetFeature(feature_name);
  // Healthd team uses feature flag "TelemetryExtensionPendingApprovalApi" for
  // pending APIs. "os.events" API has been released, and we keep adding API
  // under it like "os.events.abc" and use the feature flag for pending
  // approval. Once it's approved, we remove it from the "_api_features.json"
  // file.
  //
  // Hence, for the API under "os.events", as long as we can find it in the
  // "_api_features.json" file, it means it's behind a feature flag and then we
  // should report it as "unsupported".
  //
  // Note 1: This check is based on the above assumption. That is, if we need to
  // add the feature into _api_features.json due to other reasons, this check
  // will report incorrect answer.
  //
  // TODO(b/296816372): Retrieve the feature flag name to see if it's really
  // behind a flag.
  //
  // Note 2: Indeed this will not work if we control feature access using ways
  // other than adding feature flag (such as through blocklist).
  if (feature) {
    cx_events::EventSupportStatusInfo result;
    result.status = cx_events::EventSupportStatus::kUnsupported;
    Respond(ArgumentList(cx_events::IsEventSupported::Results::Create(result)));
    return;
  }

  auto* event_manager = EventManager::Get(browser_context());
  event_manager->IsEventSupported(
      params->category,
      base::BindOnce(&OsEventsIsEventSupportedFunction::OnEventManagerResult,
                     this));
}

void OsEventsIsEventSupportedFunction::OnEventManagerResult(
    ash::cros_healthd::mojom::SupportStatusPtr status) {
  if (!status) {
    Respond(Error("API internal error."));
    return;
  }

  switch (status->which()) {
    case ash::cros_healthd::mojom::SupportStatus::Tag::kUnmappedUnionField:
      Respond(Error("API internal error."));
      break;
    case ash::cros_healthd::mojom::SupportStatus::Tag::kException:
      Respond(Error(status->get_exception()->debug_message));
      break;
    case ash::cros_healthd::mojom::SupportStatus::Tag::kSupported: {
      cx_events::EventSupportStatusInfo success;
      success.status = cx_events::EventSupportStatus::kSupported;
      Respond(
          ArgumentList(cx_events::IsEventSupported::Results::Create(success)));
      break;
    }
    case ash::cros_healthd::mojom::SupportStatus::Tag::kUnsupported:
      cx_events::EventSupportStatusInfo result;
      result.status = cx_events::EventSupportStatus::kUnsupported;

      Respond(
          ArgumentList(cx_events::IsEventSupported::Results::Create(result)));
      break;
  }
}

// OsEventsStartCapturingEventsFunction ----------------------------------------

void OsEventsStartCapturingEventsFunction::RunIfAllowed() {
  const auto params = GetParams<cx_events::StartCapturingEvents::Params>();
  if (!params) {
    return;
  }

  auto* event_manager = EventManager::Get(browser_context());
  // If this is the "kKeyboardDiagnostic", we want to open the first party diag
  // tool to allow the user to run the diagnostic and then return the event.
  if (params->category == cx_events::EventCategory::kKeyboardDiagnostic) {
    OpenDiagnosticsKeyboardPage(browser_context());
  }

  auto result = event_manager->RegisterExtensionForEvent(extension_id(),
                                                         params->category);

  switch (result) {
    case EventManager::kSuccess:
      Respond(NoArguments());
      break;
    case EventManager::kAppUiClosed:
      Respond(Error("Companion app UI is not open."));
      break;
    case EventManager::kAppUiNotFocused:
      Respond(Error("Companion app UI is not focused."));
      break;
  }
}

// OsEventsStopCapturingEventsFunction -----------------------------------------

void OsEventsStopCapturingEventsFunction::RunIfAllowed() {
  const auto params = GetParams<cx_events::StartCapturingEvents::Params>();
  if (!params) {
    return;
  }

  auto* event_manager = EventManager::Get(browser_context());
  event_manager->RemoveObservationsForExtensionAndCategory(extension_id(),
                                                           params->category);
  Respond(NoArguments());
}

}  // namespace chromeos

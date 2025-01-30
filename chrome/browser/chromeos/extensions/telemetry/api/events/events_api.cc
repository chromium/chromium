// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/events_api.h"

#include <optional>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_manager.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/events_api_converters.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/remote_event_service_strategy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/chromeos/extensions/api/events.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

const char kKeyboardDiagnosticsUrl[] =
    "chrome://diagnostics?input&showDefaultKeyboardTester";

namespace cx_events = ::chromeos::api::os_events;
namespace crosapi = ::crosapi::mojom;

void OpenDiagnosticsKeyboardPage(content::BrowserContext* browser_context) {
  NavigateParams navigate_params(Profile::FromBrowserContext(browser_context),
                                 GURL(kKeyboardDiagnosticsUrl),
                                 ui::PAGE_TRANSITION_FIRST);
  Navigate(&navigate_params);
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

  auto* event_manager = EventManager::Get(browser_context());
  event_manager->IsEventSupported(
      converters::events::Convert(params->category),
      base::BindOnce(&OsEventsIsEventSupportedFunction::OnEventManagerResult,
                     this));
}

void OsEventsIsEventSupportedFunction::OnEventManagerResult(
    crosapi::TelemetryExtensionSupportStatusPtr status) {
  if (!status) {
    Respond(Error("API internal error."));
    return;
  }

  switch (status->which()) {
    case crosapi::TelemetryExtensionSupportStatus::Tag::kUnmappedUnionField:
      Respond(Error("API internal error."));
      break;
    case crosapi::TelemetryExtensionSupportStatus::Tag::kException:
      Respond(Error(status->get_exception()->debug_message));
      break;
    case crosapi::TelemetryExtensionSupportStatus::Tag::kSupported: {
      cx_events::EventSupportStatusInfo success;
      success.status = cx_events::EventSupportStatus::kSupported;
      Respond(
          ArgumentList(cx_events::IsEventSupported::Results::Create(success)));
      break;
    }
    case crosapi::TelemetryExtensionSupportStatus::Tag::kUnsupported:
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

  auto result = event_manager->RegisterExtensionForEvent(
      extension_id(), converters::events::Convert(params->category));

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
  event_manager->RemoveObservationsForExtensionAndCategory(
      extension_id(), converters::events::Convert(params->category));
  Respond(NoArguments());
}

}  // namespace chromeos

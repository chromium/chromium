// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/events_api.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/event_manager.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/events_api_converters.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/events/remote_event_service_strategy.h"
#include "chrome/common/chromeos/extensions/api/events.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

// EventsApiFunctionBase -------------------------------------------------------

EventsApiFunctionBase::EventsApiFunctionBase() = default;

EventsApiFunctionBase::~EventsApiFunctionBase() = default;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool EventsApiFunctionBase::IsCrosApiAvailable() {
  return LacrosService::Get()
      ->IsAvailable<crosapi::mojom::TelemetryEventService>();
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

template <class Params>
absl::optional<Params> EventsApiFunctionBase::GetParams() {
  auto params = Params::Create(args());
  if (!params) {
    SetBadMessage();
    Respond(BadMessage());
  }

  return params;
}

// OsEventsIsEventSupportedFunction --------------------------------------------

void OsEventsIsEventSupportedFunction::RunIfAllowed() {
  const auto params = GetParams<api::os_events::IsEventSupported::Params>();
  if (!params) {
    return;
  }

  auto* event_manager = EventManager::Get(browser_context());
  event_manager->IsEventSupported(
      converters::Convert(params->category),
      base::BindOnce(&OsEventsIsEventSupportedFunction::OnEventManagerResult,
                     this));
}

void OsEventsIsEventSupportedFunction::OnEventManagerResult(
    crosapi::mojom::TelemetryExtensionSupportStatusPtr status) {
  if (!status) {
    Respond(Error("API internal error."));
    return;
  }

  switch (status->which()) {
    case crosapi::mojom::internal::TelemetryExtensionSupportStatus_Data::
        TelemetryExtensionSupportStatus_Tag::kUnmappedUnionField:
      Respond(Error("API internal error."));
      break;
    case crosapi::mojom::internal::TelemetryExtensionSupportStatus_Data::
        TelemetryExtensionSupportStatus_Tag::kException:
      Respond(Error(status->get_exception()->debug_message));
      break;
    case crosapi::mojom::internal::TelemetryExtensionSupportStatus_Data::
        TelemetryExtensionSupportStatus_Tag::kSupported: {
      api::os_events::EventSupportStatusInfo success;
      success.status = api::os_events::EventSupportStatus::kSupported;
      Respond(ArgumentList(
          api::os_events::IsEventSupported::Results::Create(success)));
      break;
    }
    case crosapi::mojom::internal::TelemetryExtensionSupportStatus_Data::
        TelemetryExtensionSupportStatus_Tag::kUnsupported:
      api::os_events::EventSupportStatusInfo result;
      result.status = api::os_events::EventSupportStatus::kUnsupported;

      Respond(ArgumentList(
          api::os_events::IsEventSupported::Results::Create(result)));
      break;
  }
}

// OsEventsStartCapturingEventsFunction ----------------------------------------

void OsEventsStartCapturingEventsFunction::RunIfAllowed() {
  const auto params = GetParams<api::os_events::StartCapturingEvents::Params>();
  if (!params) {
    return;
  }

  auto* event_manager = EventManager::Get(browser_context());
  auto result = event_manager->RegisterExtensionForEvent(
      extension_id(), converters::Convert(params->category));

  switch (result) {
    case EventManager::kSuccess:
      Respond(NoArguments());
      break;
    case EventManager::kPwaClosed:
      Respond(Error("Companion PWA UI is not open."));
      break;
  }
}

// OsEventsStopCapturingEventsFunction -----------------------------------------

void OsEventsStopCapturingEventsFunction::RunIfAllowed() {
  const auto params = GetParams<api::os_events::StartCapturingEvents::Params>();
  if (!params) {
    return;
  }

  auto* event_manager = EventManager::Get(browser_context());
  event_manager->RemoveObservationsForExtensionAndCategory(
      extension_id(), converters::Convert(params->category));
  Respond(NoArguments());
}

}  // namespace chromeos

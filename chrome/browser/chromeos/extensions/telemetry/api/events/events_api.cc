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
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/profiles/profile.h"             // nogncheck
#include "chrome/browser/ui/browser_navigator.h"         // nogncheck
#include "chrome/browser/ui/browser_navigator_params.h"  // nogncheck
#include "ui/base/page_transition_types.h"               // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kKeyboardDiagnosticsUrl[] = "chrome://diagnostics?input";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
const char kKeyboardDiagnosticsUrl[] = "os://diagnostics?input";
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace cx_events = ::chromeos::api::os_events;
namespace crosapi = ::crosapi::mojom;

void OpenDiagnosticsKeyboardPage(content::BrowserContext* browser_context) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  NavigateParams navigate_params(Profile::FromBrowserContext(browser_context),
                                 GURL(kKeyboardDiagnosticsUrl),
                                 ui::PAGE_TRANSITION_FIRST);
  Navigate(&navigate_params);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* lacros_service = LacrosService::Get();
  DCHECK(lacros_service);
  DCHECK(lacros_service->IsAvailable<crosapi::UrlHandler>());

  lacros_service->GetRemote<crosapi::UrlHandler>()->OpenUrl(
      GURL(kKeyboardDiagnosticsUrl));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

}  // namespace

// EventsApiFunctionBase -------------------------------------------------------

EventsApiFunctionBase::EventsApiFunctionBase() = default;

EventsApiFunctionBase::~EventsApiFunctionBase() = default;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool EventsApiFunctionBase::IsCrosApiAvailable() {
  return LacrosService::Get()->IsAvailable<crosapi::TelemetryEventService>();
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
  const auto params = GetParams<cx_events::IsEventSupported::Params>();
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
    crosapi::TelemetryExtensionSupportStatusPtr status) {
  if (!status) {
    Respond(Error("API internal error."));
    return;
  }

  switch (status->which()) {
    case crosapi::internal::TelemetryExtensionSupportStatus_Data::
        TelemetryExtensionSupportStatus_Tag::kUnmappedUnionField:
      Respond(Error("API internal error."));
      break;
    case crosapi::internal::TelemetryExtensionSupportStatus_Data::
        TelemetryExtensionSupportStatus_Tag::kException:
      Respond(Error(status->get_exception()->debug_message));
      break;
    case crosapi::internal::TelemetryExtensionSupportStatus_Data::
        TelemetryExtensionSupportStatus_Tag::kSupported: {
      cx_events::EventSupportStatusInfo success;
      success.status = cx_events::EventSupportStatus::kSupported;
      Respond(
          ArgumentList(cx_events::IsEventSupported::Results::Create(success)));
      break;
    }
    case crosapi::internal::TelemetryExtensionSupportStatus_Data::
        TelemetryExtensionSupportStatus_Tag::kUnsupported:
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
  const auto params = GetParams<cx_events::StartCapturingEvents::Params>();
  if (!params) {
    return;
  }

  auto* event_manager = EventManager::Get(browser_context());
  event_manager->RemoveObservationsForExtensionAndCategory(
      extension_id(), converters::Convert(params->category));
  Respond(NoArguments());
}

}  // namespace chromeos

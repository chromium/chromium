// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_observation.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/uuid.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_converters.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/common/extension_id.h"

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;
namespace cx_diag = api::os_diagnostics;

std::unique_ptr<extensions::Event> GetEventForFinishedRoutine(
    crosapi::TelemetryDiagnosticRoutineStateFinishedPtr finished,
    base::Uuid uuid,
    content::BrowserContext* browser_context) {
  switch (finished->detail->which()) {
    case crosapi::TelemetryDiagnosticRoutineDetail::Tag::kUnrecognizedArgument:
      LOG(WARNING) << "Got unknown routine detail";
      return nullptr;
    case crosapi::TelemetryDiagnosticRoutineDetail::Tag::kMemory: {
      auto finished_info = converters::routines::ConvertPtr(
          std::move(finished->detail->get_memory()), uuid,
          finished->has_passed);
      return std::make_unique<extensions::Event>(
          extensions::events::OS_DIAGNOSTICS_ON_MEMORY_ROUTINE_FINISHED,
          cx_diag::OnMemoryRoutineFinished::kEventName,
          base::Value::List().Append(finished_info.ToValue()), browser_context);
    }
  }
  NOTREACHED_NORETURN();
}

}  // namespace

DiagnosticRoutineObservation::DiagnosticRoutineObservation(
    extensions::ExtensionId extension_id,
    base::Uuid uuid,
    content::BrowserContext* context,
    mojo::PendingReceiver<crosapi::TelemetryDiagnosticRoutineObserver>
        pending_receiver)
    : extension_id_(extension_id),
      uuid_(uuid),
      browser_context_(context),
      receiver_(this, std::move(pending_receiver)) {}

DiagnosticRoutineObservation::~DiagnosticRoutineObservation() = default;

void DiagnosticRoutineObservation::OnRoutineStateChange(
    crosapi::TelemetryDiagnosticRoutineStatePtr state) {
  std::unique_ptr<extensions::Event> event;
  switch (state->state_union->which()) {
    case crosapi::TelemetryDiagnosticRoutineStateUnion::Tag::
        kUnrecognizedArgument:
      LOG(WARNING) << "Got unknown routine state";
      return;
    case crosapi::TelemetryDiagnosticRoutineStateUnion::Tag::kInitialized: {
      auto init_info = converters::routines::ConvertPtr(
          std::move(state->state_union->get_initialized()), uuid_);
      event = std::make_unique<extensions::Event>(
          extensions::events::OS_DIAGNOSTICS_ON_ROUTINE_INITIALIZED,
          cx_diag::OnRoutineInitialized::kEventName,
          base::Value::List().Append(init_info.ToValue()), browser_context_);
      break;
    }
    case crosapi::TelemetryDiagnosticRoutineStateUnion::Tag::kRunning: {
      auto running_info = converters::routines::ConvertPtr(
          std::move(state->state_union->get_running()), uuid_,
          state->percentage);
      event = std::make_unique<extensions::Event>(
          extensions::events::OS_DIAGNOSTICS_ON_ROUTINE_RUNNING,
          cx_diag::OnRoutineRunning::kEventName,
          base::Value::List().Append(running_info.ToValue()), browser_context_);
      break;
    }
    case crosapi::TelemetryDiagnosticRoutineStateUnion::Tag::kWaiting: {
      auto running_info = converters::routines::ConvertPtr(
          std::move(state->state_union->get_waiting()), uuid_,
          state->percentage);
      event = std::make_unique<extensions::Event>(
          extensions::events::OS_DIAGNOSTICS_ON_ROUTINE_WAITING,
          cx_diag::OnRoutineWaiting::kEventName,
          base::Value::List().Append(running_info.ToValue()), browser_context_);
      break;
    }
    case crosapi::TelemetryDiagnosticRoutineStateUnion::Tag::kFinished: {
      event = GetEventForFinishedRoutine(
          std::move(state->state_union->get_finished()), uuid_,
          browser_context_);
      if (!event) {
        return;
      }
    }
  }

  // The `EventRouter` might be unavailable in unittests.
  if (!extensions::EventRouter::Get(browser_context_)) {
    CHECK_IS_TEST();
    return;
  }

  extensions::EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id_, std::move(event));
}

}  // namespace chromeos

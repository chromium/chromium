// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_observation.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/notreached.h"
#include "base/uuid.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_converters.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_info.h"
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

std::unique_ptr<extensions::Event>
CreateEventForLegacyFinishedVolumeButtonRoutine(
    bool has_passed,
    base::Uuid uuid,
    content::BrowserContext* browser_context) {
  auto finished_info = converters::routines::ConvertPtr(
      crosapi::TelemetryDiagnosticVolumeButtonRoutineDetail::New(), uuid,
      has_passed);
  return std::make_unique<extensions::Event>(
      extensions::events::OS_DIAGNOSTICS_ON_VOLUME_BUTTON_ROUTINE_FINISHED,
      cx_diag::OnVolumeButtonRoutineFinished::kEventName,
      base::Value::List().Append(finished_info.ToValue()), browser_context);
}

std::unique_ptr<extensions::Event> GetEventForLegacyFinishedRoutine(
    crosapi::TelemetryDiagnosticRoutineStateFinishedPtr finished,
    base::Uuid uuid,
    content::BrowserContext* browser_context,
    crosapi::TelemetryDiagnosticRoutineArgument::Tag
        argument_tag_for_legacy_finished_events) {
  // The volume button routine has no detail.
  if (argument_tag_for_legacy_finished_events ==
      crosapi::TelemetryDiagnosticRoutineArgument::Tag::kVolumeButton) {
    return CreateEventForLegacyFinishedVolumeButtonRoutine(
        finished->has_passed, uuid, browser_context);
  }

  if (finished->detail.is_null()) {
    return nullptr;
  }

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
    case crosapi::TelemetryDiagnosticRoutineDetail::Tag::kVolumeButton: {
      // Though unexpected, we should handle it gracefully because the input is
      // from another service.
      LOG(WARNING)
          << "Got volume button routine detail for non-volume-button routine";
      return nullptr;
    }
    case crosapi::TelemetryDiagnosticRoutineDetail::Tag::kFan: {
      auto finished_info = converters::routines::ConvertPtr(
          std::move(finished->detail->get_fan()), uuid, finished->has_passed);
      return std::make_unique<extensions::Event>(
          extensions::events::OS_DIAGNOSTICS_ON_FAN_ROUTINE_FINISHED,
          cx_diag::OnFanRoutineFinished::kEventName,
          base::Value::List().Append(finished_info.ToValue()), browser_context);
    }
    case crosapi::TelemetryDiagnosticRoutineDetail::Tag::kNetworkBandwidth:
    case crosapi::TelemetryDiagnosticRoutineDetail::Tag::kCameraFrameAnalysis:
      // No need to support legacy finished events for newer routines.
      return nullptr;
  }
  NOTREACHED();
}

std::unique_ptr<extensions::Event> GetEventForFinishedRoutine(
    crosapi::TelemetryDiagnosticRoutineStateFinishedPtr finished,
    base::Uuid uuid,
    content::BrowserContext* browser_context) {
  bool has_passed = finished->has_passed;
  auto finished_info =
      converters::routines::ConvertPtr(std::move(finished), uuid, has_passed);
  return std::make_unique<extensions::Event>(
      extensions::events::OS_DIAGNOSTICS_ON_ROUTINE_FINISHED,
      cx_diag::OnRoutineFinished::kEventName,
      base::Value::List().Append(finished_info.ToValue()), browser_context);
}

}  // namespace

DiagnosticRoutineObservation::DiagnosticRoutineObservation(
    DiagnosticRoutineInfo info,
    OnRoutineFinished on_routine_finished,
    mojo::PendingReceiver<crosapi::TelemetryDiagnosticRoutineObserver>
        pending_receiver)
    : info_(info),
      on_routine_finished_(std::move(on_routine_finished)),
      receiver_(this, std::move(pending_receiver)) {}

DiagnosticRoutineObservation::~DiagnosticRoutineObservation() = default;

void DiagnosticRoutineObservation::OnRoutineStateChange(
    crosapi::TelemetryDiagnosticRoutineStatePtr state) {
  std::unique_ptr<extensions::Event> event;
  std::unique_ptr<extensions::Event> legacy_finished_event;
  switch (state->state_union->which()) {
    case crosapi::TelemetryDiagnosticRoutineStateUnion::Tag::
        kUnrecognizedArgument:
      LOG(WARNING) << "Got unknown routine state";
      return;
    case crosapi::TelemetryDiagnosticRoutineStateUnion::Tag::kInitialized: {
      auto init_info = converters::routines::ConvertPtr(
          std::move(state->state_union->get_initialized()), info_.uuid);
      event = std::make_unique<extensions::Event>(
          extensions::events::OS_DIAGNOSTICS_ON_ROUTINE_INITIALIZED,
          cx_diag::OnRoutineInitialized::kEventName,
          base::Value::List().Append(init_info.ToValue()),
          info_.browser_context);
      break;
    }
    case crosapi::TelemetryDiagnosticRoutineStateUnion::Tag::kRunning: {
      auto running_info = converters::routines::ConvertPtr(
          std::move(state->state_union->get_running()), info_.uuid,
          state->percentage);
      event = std::make_unique<extensions::Event>(
          extensions::events::OS_DIAGNOSTICS_ON_ROUTINE_RUNNING,
          cx_diag::OnRoutineRunning::kEventName,
          base::Value::List().Append(running_info.ToValue()),
          info_.browser_context);
      break;
    }
    case crosapi::TelemetryDiagnosticRoutineStateUnion::Tag::kWaiting: {
      auto running_info = converters::routines::ConvertPtr(
          std::move(state->state_union->get_waiting()), info_.uuid,
          state->percentage);
      event = std::make_unique<extensions::Event>(
          extensions::events::OS_DIAGNOSTICS_ON_ROUTINE_WAITING,
          cx_diag::OnRoutineWaiting::kEventName,
          base::Value::List().Append(running_info.ToValue()),
          info_.browser_context);
      break;
    }
    case crosapi::TelemetryDiagnosticRoutineStateUnion::Tag::kFinished: {
      legacy_finished_event = GetEventForLegacyFinishedRoutine(
          state->state_union->get_finished().Clone(), info_.uuid,
          info_.browser_context, info_.argument_tag_for_legacy_finished_events);
      event = GetEventForFinishedRoutine(
          std::move(state->state_union->get_finished()), info_.uuid,
          info_.browser_context);
      break;
    }
  }

  // The `EventRouter` might be unavailable in unittests.
  if (!extensions::EventRouter::Get(info_.browser_context)) {
    CHECK_IS_TEST();
  } else {
    extensions::EventRouter::Get(info_.browser_context)
        ->DispatchEventToExtension(info_.extension_id, std::move(event));
    if (legacy_finished_event) {
      extensions::EventRouter::Get(info_.browser_context)
          ->DispatchEventToExtension(info_.extension_id,
                                     std::move(legacy_finished_event));
    }
  }

  if (state->state_union->is_finished() && on_routine_finished_) {
    std::move(on_routine_finished_).Run(info_);
  }
}

}  // namespace chromeos

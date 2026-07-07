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
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/common/extension_id.h"

namespace chromeos {

namespace {

namespace cx_diag = api::os_diagnostics;

std::unique_ptr<extensions::Event>
CreateEventForLegacyFinishedVolumeButtonRoutine(
    bool has_passed,
    base::Uuid uuid,
    content::BrowserContext* browser_context) {
  cx_diag::LegacyVolumeButtonRoutineFinishedInfo finished_info;
  finished_info.uuid = uuid.AsLowercaseString();
  finished_info.has_passed = has_passed;
  return std::make_unique<extensions::Event>(
      extensions::events::OS_DIAGNOSTICS_ON_VOLUME_BUTTON_ROUTINE_FINISHED,
      cx_diag::OnVolumeButtonRoutineFinished::kEventName,
      base::ListValue().Append(finished_info.ToValue()), browser_context);
}

std::unique_ptr<extensions::Event> GetEventForLegacyFinishedRoutine(
    ash::cros_healthd::mojom::RoutineStateFinishedPtr finished,
    base::Uuid uuid,
    content::BrowserContext* browser_context,
    ash::cros_healthd::mojom::RoutineArgument::Tag
        argument_tag_for_legacy_finished_events) {
  // The volume button routine has no detail.
  if (argument_tag_for_legacy_finished_events ==
      ash::cros_healthd::mojom::RoutineArgument::Tag::kVolumeButton) {
    return CreateEventForLegacyFinishedVolumeButtonRoutine(
        finished->has_passed, uuid, browser_context);
  }

  if (finished->detail.is_null()) {
    return nullptr;
  }

  switch (finished->detail->which()) {
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kUnrecognizedArgument:
      LOG(WARNING) << "Got unknown routine detail";
      return nullptr;
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kMemory: {
      auto finished_info = converters::routines::ConvertPtr(
          std::move(finished->detail->get_memory()), uuid,
          finished->has_passed);
      return std::make_unique<extensions::Event>(
          extensions::events::OS_DIAGNOSTICS_ON_MEMORY_ROUTINE_FINISHED,
          cx_diag::OnMemoryRoutineFinished::kEventName,
          base::ListValue().Append(finished_info.ToValue()), browser_context);
    }
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kFan: {
      auto finished_info = converters::routines::ConvertPtr(
          std::move(finished->detail->get_fan()), uuid, finished->has_passed);
      return std::make_unique<extensions::Event>(
          extensions::events::OS_DIAGNOSTICS_ON_FAN_ROUTINE_FINISHED,
          cx_diag::OnFanRoutineFinished::kEventName,
          base::ListValue().Append(finished_info.ToValue()), browser_context);
    }
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kNetworkBandwidth:
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kCameraFrameAnalysis:
      // No need to support legacy finished events for newer routines.
      return nullptr;

    // Unsupported enums
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kAudioDriver:
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kUfsLifetime:
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kBluetoothPower:
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kBluetoothDiscovery:
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kBluetoothScanning:
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kBluetoothPairing:
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kCameraAvailability:
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kSensitiveSensor:
    case ash::cros_healthd::mojom::RoutineDetail::Tag::kBatteryDischarge:
      return nullptr;
  }
  NOTREACHED();
}

std::unique_ptr<extensions::Event> GetEventForFinishedRoutine(
    ash::cros_healthd::mojom::RoutineStateFinishedPtr finished,
    base::Uuid uuid,
    content::BrowserContext* browser_context) {
  bool has_passed = finished->has_passed;
  auto finished_info =
      converters::routines::ConvertPtr(std::move(finished), uuid, has_passed);
  return std::make_unique<extensions::Event>(
      extensions::events::OS_DIAGNOSTICS_ON_ROUTINE_FINISHED,
      cx_diag::OnRoutineFinished::kEventName,
      base::ListValue().Append(finished_info.ToValue()), browser_context);
}

}  // namespace

DiagnosticRoutineObservation::DiagnosticRoutineObservation(
    DiagnosticRoutineInfo info,
    OnRoutineFinished on_routine_finished,
    mojo::PendingReceiver<ash::cros_healthd::mojom::RoutineObserver>
        pending_receiver)
    : info_(info),
      on_routine_finished_(std::move(on_routine_finished)),
      receiver_(this, std::move(pending_receiver)) {}

DiagnosticRoutineObservation::~DiagnosticRoutineObservation() = default;

void DiagnosticRoutineObservation::OnRoutineStateChange(
    ash::cros_healthd::mojom::RoutineStatePtr state) {
  std::unique_ptr<extensions::Event> event;
  std::unique_ptr<extensions::Event> legacy_finished_event;
  switch (state->state_union->which()) {
    case ash::cros_healthd::mojom::RoutineStateUnion::Tag::
        kUnrecognizedArgument:
      LOG(WARNING) << "Got unknown routine state";
      return;
    case ash::cros_healthd::mojom::RoutineStateUnion::Tag::kInitialized: {
      auto init_info = converters::routines::ConvertPtr(
          std::move(state->state_union->get_initialized()), info_.uuid);
      event = std::make_unique<extensions::Event>(
          extensions::events::OS_DIAGNOSTICS_ON_ROUTINE_INITIALIZED,
          cx_diag::OnRoutineInitialized::kEventName,
          base::ListValue().Append(init_info.ToValue()), info_.browser_context);
      break;
    }
    case ash::cros_healthd::mojom::RoutineStateUnion::Tag::kRunning: {
      auto running_info = converters::routines::ConvertPtr(
          std::move(state->state_union->get_running()), info_.uuid,
          state->percentage);
      event = std::make_unique<extensions::Event>(
          extensions::events::OS_DIAGNOSTICS_ON_ROUTINE_RUNNING,
          cx_diag::OnRoutineRunning::kEventName,
          base::ListValue().Append(running_info.ToValue()),
          info_.browser_context);
      break;
    }
    case ash::cros_healthd::mojom::RoutineStateUnion::Tag::kWaiting: {
      auto running_info = converters::routines::ConvertPtr(
          std::move(state->state_union->get_waiting()), info_.uuid,
          state->percentage);
      event = std::make_unique<extensions::Event>(
          extensions::events::OS_DIAGNOSTICS_ON_ROUTINE_WAITING,
          cx_diag::OnRoutineWaiting::kEventName,
          base::ListValue().Append(running_info.ToValue()),
          info_.browser_context);
      break;
    }
    case ash::cros_healthd::mojom::RoutineStateUnion::Tag::kFinished: {
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

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_converters.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_info.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_observation.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;
namespace cx_diag = api::os_diagnostics;

}  // namespace

DiagnosticRoutine::DiagnosticRoutine(
    mojo::PendingRemote<crosapi::TelemetryDiagnosticRoutineControl>
        control_remote,
    mojo::PendingReceiver<crosapi::TelemetryDiagnosticRoutineObserver>
        observer_receiver,
    DiagnosticRoutineInfo info,
    OnRoutineFinishedOrException on_routine_finished_or_exception)
    : routine_control_(std::move(control_remote)),
      observation_(info,
                   base::IgnoreArgs<DiagnosticRoutineInfo>(
                       base::BindOnce(&DiagnosticRoutine::CallDeleter,
                                      base::Unretained(this))),
                   std::move(observer_receiver)),
      info_(info),
      on_routine_finished_or_exception_(
          std::move(on_routine_finished_or_exception)) {
  routine_control_.set_disconnect_with_reason_handler(
      base::BindOnce(&DiagnosticRoutine::OnRoutineControlDisconnect,
                     weak_factory.GetWeakPtr()));
}

DiagnosticRoutine::~DiagnosticRoutine() = default;

void DiagnosticRoutine::OnRoutineControlDisconnect(uint32_t error_code,
                                                   const std::string& message) {
  cx_diag::ExceptionInfo exception;
  exception.uuid = info_.uuid.AsLowercaseString();
  exception.reason = converters::routines::Convert(
      static_cast<crosapi::TelemetryExtensionException::Reason>(error_code));
  exception.debug_message = message;

  auto event = std::make_unique<extensions::Event>(
      extensions::events::OS_DIAGNOSTICS_ON_ROUTINE_EXCEPTION,
      cx_diag::OnRoutineException::kEventName,
      base::Value::List().Append(exception.ToValue()), info_.browser_context);

  // The `EventRouter` might be unavailable in unittests.
  if (!extensions::EventRouter::Get(info_.browser_context)) {
    CHECK_IS_TEST();
  } else {
    extensions::EventRouter::Get(info_.browser_context)
        ->DispatchEventToExtension(info_.extension_id, std::move(event));
  }
  CallDeleter();
}

mojo::Remote<crosapi::TelemetryDiagnosticRoutineControl>&
DiagnosticRoutine::GetRemote() {
  return routine_control_;
}

void DiagnosticRoutine::CallDeleter() {
  if (on_routine_finished_or_exception_) {
    std::move(on_routine_finished_or_exception_).Run(info_);
  }
}

}  // namespace chromeos

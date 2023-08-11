// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_observation.h"

#include <memory>

#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "base/notreached.h"
#include "base/uuid.h"
#include "chrome/common/chromeos/extensions/api/diagnostics.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/common/extension_id.h"

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

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
      api::os_diagnostics::RoutineInitializedInfo arg;
      arg.uuid = uuid_.AsLowercaseString();
      event = std::make_unique<extensions::Event>(
          extensions::events::OS_DIAGNOSTICS_ON_ROUTINE_INITIALIZED,
          api::os_diagnostics::OnRoutineInitialized::kEventName,
          base::Value::List().Append(arg.ToValue()), browser_context_);
      break;
    }
    case crosapi::TelemetryDiagnosticRoutineStateUnion::Tag::kRunning:
    case crosapi::TelemetryDiagnosticRoutineStateUnion::Tag::kWaiting:
    case crosapi::TelemetryDiagnosticRoutineStateUnion::Tag::kFinished:
      NOTIMPLEMENTED();
      break;
  }

  extensions::EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id_, std::move(event));
}

}  // namespace chromeos

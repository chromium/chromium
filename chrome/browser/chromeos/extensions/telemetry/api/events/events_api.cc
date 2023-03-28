// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/events_api.h"

#include "base/notreached.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool EventsApiFunctionBase::IsCrosApiAvailable() {
  return LacrosService::Get()
      ->IsAvailable<crosapi::mojom::TelemetryEventService>();
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

void OsEventsIsEventSupportedFunction::RunIfAllowed() {
  NOTIMPLEMENTED();
}

void OsEventsStartCapturingEventsFunction::RunIfAllowed() {
  NOTIMPLEMENTED();
}

void OsEventsStopCapturingEventsFunction::RunIfAllowed() {
  NOTIMPLEMENTED();
}

}  // namespace chromeos

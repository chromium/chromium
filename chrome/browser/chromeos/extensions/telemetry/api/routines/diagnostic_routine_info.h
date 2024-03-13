// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_INFO_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_INFO_H_

#include "base/uuid.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/extension_id.h"

namespace chromeos {

struct DiagnosticRoutineInfo {
  extensions::ExtensionId extension_id;
  base::Uuid uuid;
  raw_ptr<content::BrowserContext> browser_context;
  // The tag of the argument to create this routine. It is used to emit legacy
  // `os.diagnostics.onXYZRoutineFinished` events for routines which have no
  // detail.
  // TODO(b/320394027): Remove the workaround after the legacy finished events
  // are removed.
  ::crosapi::mojom::TelemetryDiagnosticRoutineArgument::Tag
      argument_tag_for_legacy_finished_events;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_INFO_H_

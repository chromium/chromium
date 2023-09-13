// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_INFO_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_INFO_H_

#include "base/uuid.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/extension_id.h"

namespace chromeos {

struct DiagnosticRoutineInfo {
  extensions::ExtensionId extension_id;
  base::Uuid uuid;
  raw_ptr<content::BrowserContext> browser_context;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_INFO_H_

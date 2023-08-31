// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_MANAGER_H_

#include "content/public/browser/browser_context.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace chromeos {

// The `DiagnosticRoutineManager` is responsible for creating routines and
// managing them throughout there lifecycle. Once a routine is created, we can
// start executing it and get updates via an observer (see classes
// `DiagnosticRoutine` and `DiagnosticRoutineObservation`).
// A routine can only be started if the PWA for an extension is currently open.
// Once the PWA is closed, we also close the routine connection and thus end
// execution of that routine.
class DiagnosticRoutineManager : public extensions::BrowserContextKeyedAPI {
 public:
  explicit DiagnosticRoutineManager(content::BrowserContext* context);

  DiagnosticRoutineManager(const DiagnosticRoutineManager&) = delete;
  DiagnosticRoutineManager& operator=(const DiagnosticRoutineManager&) = delete;

  ~DiagnosticRoutineManager() override;

  // extensions::BrowserContextKeyedAPI:
  static extensions::BrowserContextKeyedAPIFactory<DiagnosticRoutineManager>*
  GetFactoryInstance();

  // Convenience method to get the DiagnosticRoutineManager for a
  // `content::BrowserContext`.
  static DiagnosticRoutineManager* Get(
      content::BrowserContext* browser_context);

 private:
  friend class extensions::BrowserContextKeyedAPIFactory<
      DiagnosticRoutineManager>;

  // extensions::BrowserContextKeyedAPI:
  static const char* service_name() { return "DiagnosticRoutineManager"; }
  static const bool kServiceIsCreatedInGuestMode = false;
  static const bool kServiceRedirectedInIncognito = true;

  const raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_MANAGER_H_

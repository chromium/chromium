// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_MANAGER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/app_ui_observer.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/diagnostic_routine_info.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/routines/remote_diagnostic_routines_service_strategy.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "chromeos/crosapi/mojom/telemetry_extension_exception.mojom.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

// The `DiagnosticRoutineManager` is responsible for creating routines and
// managing them throughout there lifecycle. Once a routine is created, we can
// start executing it and get updates via an observer (see classes
// `DiagnosticRoutine` and `DiagnosticRoutineObservation`).
// A routine can only be started if the PWA for an extension is currently open.
// Once the PWA is closed, we also close the routine connection and thus end
// execution of that routine.
class DiagnosticRoutineManager : public extensions::BrowserContextKeyedAPI,
                                 extensions::ExtensionRegistryObserver {
 public:
  enum Error {
    kAppUiClosed,
    kExtensionUnloaded,
  };

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

  base::expected<base::Uuid, Error> CreateRoutine(
      extensions::ExtensionId extension_id,
      crosapi::mojom::TelemetryDiagnosticRoutineArgumentPtr routine_argument);
  // Tries to start the routine with `routine_id`, returns true if successful,
  // otherwise false.
  bool StartRoutineForExtension(extensions::ExtensionId extension_id,
                                base::Uuid routine_id);
  // Stops the routine with `routine_id`.
  void CancelRoutineForExtension(extensions::ExtensionId extension_id,
                                 base::Uuid routine_id);
  // Sends reply to the inquiry of the routine with `routine_id`. Returns true
  // if successful, otherwise false.
  bool ReplyToRoutineInquiryForExtension(
      const extensions::ExtensionId& extension_id,
      const base::Uuid& routine_id,
      crosapi::mojom::TelemetryDiagnosticRoutineInquiryReplyPtr reply);

  void IsRoutineArgumentSupported(
      crosapi::mojom::TelemetryDiagnosticRoutineArgumentPtr arg,
      base::OnceCallback<
          void(crosapi::mojom::TelemetryExtensionSupportStatusPtr)> callback);

  // `ExtensionRegistryObserver`:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

 private:
  friend class extensions::BrowserContextKeyedAPIFactory<
      DiagnosticRoutineManager>;
  friend class TelemetryExtensionDiagnosticRoutinesManagerTest;

  void OnAppUiClosed(extensions::ExtensionId extension_id);

  base::expected<std::unique_ptr<AppUiObserver>,
                 DiagnosticRoutineManager::Error>
  CreateAppUiObserver(extensions::ExtensionId extension_id);

  // Called once a specific `DiagnosticRoutine` signals a cut connection or
  // enters the finished state. We are removing the routine in that case.
  void OnRoutineExceptionOrFinished(DiagnosticRoutineInfo info);

  mojo::Remote<crosapi::mojom::TelemetryDiagnosticRoutinesService>&
  GetRemoteService();

  // extensions::BrowserContextKeyedAPI:
  static const char* service_name() { return "DiagnosticRoutineManager"; }
  static const bool kServiceIsCreatedInGuestMode = false;
  static const bool kServiceRedirectedInIncognito = true;

  base::flat_map<extensions::ExtensionId, std::unique_ptr<AppUiObserver>>
      app_ui_observers_;
  base::flat_map<extensions::ExtensionId,
                 std::vector<std::unique_ptr<DiagnosticRoutine>>>
      routines_per_extension_;

  std::unique_ptr<RemoteDiagnosticRoutineServiceStrategy> remote_strategy_;
  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace chromeos

namespace extensions {

template <>
struct BrowserContextFactoryDependencies<chromeos::DiagnosticRoutineManager> {
  static void DeclareFactoryDependencies(
      BrowserContextKeyedAPIFactory<chromeos::DiagnosticRoutineManager>*
          factory) {
    factory->DependsOn(ExtensionRegistryFactory::GetInstance());
  }
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_DIAGNOSTIC_ROUTINE_MANAGER_H_

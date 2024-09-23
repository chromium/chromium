// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_FAKE_DIAGNOSTIC_ROUTINES_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_FAKE_DIAGNOSTIC_ROUTINES_SERVICE_FACTORY_H_

#include <memory>

#include "chrome/browser/chromeos/extensions/telemetry/api/routines/fake_diagnostic_routines_service.h"
#include "chromeos/ash/components/telemetry_extension/routines/telemetry_diagnostic_routine_service_ash.h"
#include "chromeos/crosapi/mojom/telemetry_diagnostic_routine_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {

class FakeDiagnosticRoutinesServiceFactory
    : public ash::TelemetryDiagnosticsRoutineServiceAsh::Factory {
 public:
  FakeDiagnosticRoutinesServiceFactory();
  ~FakeDiagnosticRoutinesServiceFactory() override;

  void SetCreateInstanceResponse(
      std::unique_ptr<FakeDiagnosticRoutinesService> fake_service);

 protected:
  // TelemetryEventServiceAsh::Factory:
  std::unique_ptr<crosapi::mojom::TelemetryDiagnosticRoutinesService>
  CreateInstance(
      mojo::PendingReceiver<crosapi::mojom::TelemetryDiagnosticRoutinesService>
          receiver) override;

 private:
  std::unique_ptr<FakeDiagnosticRoutinesService> fake_service_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_ROUTINES_FAKE_DIAGNOSTIC_ROUTINES_SERVICE_FACTORY_H_

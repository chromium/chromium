// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_FAKE_DIAGNOSTICS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_FAKE_DIAGNOSTICS_SERVICE_FACTORY_H_

#include <memory>

#include "chrome/browser/chromeos/extensions/telemetry/api/diagnostics/fake_diagnostics_service.h"
#include "chromeos/ash/components/telemetry_extension/diagnostics/diagnostics_service_ash.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {
class FakeDiagnosticsServiceFactory
    : public ash::DiagnosticsServiceAsh::Factory {
 public:
  FakeDiagnosticsServiceFactory();
  ~FakeDiagnosticsServiceFactory() override;

  void SetCreateInstanceResponse(
      std::unique_ptr<FakeDiagnosticsService> fake_service);

 protected:
  // DiagnosticsServiceAsh::Factory:
  std::unique_ptr<crosapi::mojom::DiagnosticsService> CreateInstance(
      mojo::PendingReceiver<crosapi::mojom::DiagnosticsService> receiver)
      override;

 private:
  std::unique_ptr<FakeDiagnosticsService> fake_service_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_FAKE_DIAGNOSTICS_SERVICE_FACTORY_H_

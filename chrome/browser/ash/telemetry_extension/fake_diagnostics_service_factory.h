// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_FAKE_DIAGNOSTICS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_FAKE_DIAGNOSTICS_SERVICE_FACTORY_H_

#include <memory>

#include "chrome/browser/ash/telemetry_extension/diagnostics_service_ash.h"
#include "chrome/browser/ash/telemetry_extension/fake_diagnostics_service.h"
#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {
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

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_FAKE_DIAGNOSTICS_SERVICE_FACTORY_H_

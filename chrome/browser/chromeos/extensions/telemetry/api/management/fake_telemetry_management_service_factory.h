// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_MANAGEMENT_FAKE_TELEMETRY_MANAGEMENT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_MANAGEMENT_FAKE_TELEMETRY_MANAGEMENT_SERVICE_FACTORY_H_

#include <memory>

#include "chrome/browser/chromeos/extensions/telemetry/api/management/fake_telemetry_management_service.h"
#include "chromeos/ash/components/telemetry_extension/management/telemetry_management_service_ash.h"
#include "chromeos/crosapi/mojom/telemetry_management_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {

class FakeTelemetryManagementServiceFactory
    : public ash::TelemetryManagementServiceAsh::Factory {
 public:
  FakeTelemetryManagementServiceFactory();
  ~FakeTelemetryManagementServiceFactory() override;

  void SetCreateInstanceResponse(
      std::unique_ptr<FakeTelemetryManagementService> fake_service);

 protected:
  // TelemetryManagementServiceAsh::Factory:
  std::unique_ptr<crosapi::mojom::TelemetryManagementService> CreateInstance(
      mojo::PendingReceiver<crosapi::mojom::TelemetryManagementService>
          receiver) override;

 private:
  std::unique_ptr<FakeTelemetryManagementService> fake_service_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_MANAGEMENT_FAKE_TELEMETRY_MANAGEMENT_SERVICE_FACTORY_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_FAKE_PROBE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_FAKE_PROBE_SERVICE_FACTORY_H_

#include <memory>

#include "chrome/browser/ash/telemetry_extension/telemetry/probe_service_ash.h"
#include "chrome/browser/chromeos/telemetry/fake_probe_service.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {

class FakeProbeServiceFactory : public ash::ProbeServiceAsh::Factory {
 public:
  FakeProbeServiceFactory();
  ~FakeProbeServiceFactory() override;

  void SetCreateInstanceResponse(
      std::unique_ptr<FakeProbeService> fake_service);

 protected:
  // ProbeServiceAsh::Factory:
  std::unique_ptr<crosapi::mojom::TelemetryProbeService> CreateInstance(
      mojo::PendingReceiver<crosapi::mojom::TelemetryProbeService> receiver)
      override;

 private:
  std::unique_ptr<FakeProbeService> fake_service_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_FAKE_PROBE_SERVICE_FACTORY_H_

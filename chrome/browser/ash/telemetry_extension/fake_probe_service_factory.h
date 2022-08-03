// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_FAKE_PROBE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_FAKE_PROBE_SERVICE_FACTORY_H_

#include <memory>

#include "chrome/browser/ash/telemetry_extension/fake_probe_service.h"
#include "chrome/browser/ash/telemetry_extension/probe_service_ash.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

class FakeProbeServiceFactory : public ash::ProbeServiceAsh::Factory {
 public:
  FakeProbeServiceFactory();
  ~FakeProbeServiceFactory() override;

  void SetCreateInstanceResponse(
      std::unique_ptr<FakeProbeService> fake_service);

 protected:
  // ProbeServiceAsh::Factory:
  std::unique_ptr<crosapi::mojom::ProbeService> CreateInstance(
      mojo::PendingReceiver<crosapi::mojom::ProbeService> receiver) override;

 private:
  std::unique_ptr<FakeProbeService> fake_service_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_FAKE_PROBE_SERVICE_FACTORY_H_

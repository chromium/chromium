// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_PROBE_SERVICE_H_
#define CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_PROBE_SERVICE_H_

#include <memory>
#include <vector>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd.mojom.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// TODO(https://crbug.com/1164001): Remove if cros_healthd::mojom moved to ash.
namespace cros_healthd {
namespace mojom = ::chromeos::cros_healthd::mojom;
}  // namespace cros_healthd

class ProbeService : public health::mojom::ProbeService {
 public:
  class Factory {
   public:
    static std::unique_ptr<health::mojom::ProbeService> Create(
        mojo::PendingReceiver<health::mojom::ProbeService> receiver);
    static void SetForTesting(Factory* test_factory);

    virtual ~Factory();

   protected:
    virtual std::unique_ptr<health::mojom::ProbeService> CreateInstance(
        mojo::PendingReceiver<health::mojom::ProbeService> receiver) = 0;

   private:
    static Factory* test_factory_;
  };

  ProbeService(const ProbeService&) = delete;
  ProbeService& operator=(const ProbeService&) = delete;
  ~ProbeService() override;

 private:
  explicit ProbeService(
      mojo::PendingReceiver<health::mojom::ProbeService> receiver);

  // health::mojom::ProbeService override
  void ProbeTelemetryInfo(
      const std::vector<health::mojom::ProbeCategoryEnum>& categories,
      ProbeTelemetryInfoCallback callback) override;
  void GetOemData(GetOemDataCallback callback) override;

  // Ensures that |service_| created and connected to the
  // CrosHealthdProbeService.
  cros_healthd::mojom::CrosHealthdProbeService* GetService();

  void OnDisconnect();

  // Pointer to real implementation.
  mojo::Remote<cros_healthd::mojom::CrosHealthdProbeService> service_;

  // We must destroy |receiver_| before destroying |service_|, so we will close
  // interface pipe before destroying pending response callbacks owned by
  // |service_|. It is an error to drop response callbacks which still
  // correspond to an open interface pipe.
  mojo::Receiver<health::mojom::ProbeService> receiver_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when ChromeOS code migration is done.
namespace chromeos {
using ::ash::ProbeService;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_TELEMETRY_EXTENSION_PROBE_SERVICE_H_

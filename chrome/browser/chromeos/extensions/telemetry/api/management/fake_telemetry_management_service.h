// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_MANAGEMENT_FAKE_TELEMETRY_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_MANAGEMENT_FAKE_TELEMETRY_MANAGEMENT_SERVICE_H_

#include "chromeos/crosapi/mojom/telemetry_management_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

class FakeTelemetryManagementService
    : public crosapi::mojom::TelemetryManagementService {
 public:
  FakeTelemetryManagementService();
  FakeTelemetryManagementService(const FakeTelemetryManagementService&) =
      delete;
  FakeTelemetryManagementService& operator=(
      const FakeTelemetryManagementService&) = delete;
  ~FakeTelemetryManagementService() override;

  void BindPendingReceiver(
      mojo::PendingReceiver<crosapi::mojom::TelemetryManagementService>
          receiver);

  mojo::PendingRemote<crosapi::mojom::TelemetryManagementService>
  BindNewPipeAndPassRemote();

  // TelemetryManagementService:
  void SetAudioGain(uint64_t node_id,
                    int32_t gain,
                    SetAudioGainCallback callback) override;
  void SetAudioVolume(uint64_t node_id,
                      int32_t volume,
                      bool is_muted,
                      SetAudioVolumeCallback callback) override;

 private:
  mojo::Receiver<crosapi::mojom::TelemetryManagementService> receiver_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_MANAGEMENT_FAKE_TELEMETRY_MANAGEMENT_SERVICE_H_

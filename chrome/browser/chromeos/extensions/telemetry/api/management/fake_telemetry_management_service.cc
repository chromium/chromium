// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/management/fake_telemetry_management_service.h"

#include <utility>

#include "chromeos/crosapi/mojom/telemetry_management_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {

namespace {
namespace crosapi = ::crosapi::mojom;
}  // namespace

FakeTelemetryManagementService::FakeTelemetryManagementService() = default;

FakeTelemetryManagementService::~FakeTelemetryManagementService() = default;

void FakeTelemetryManagementService::BindPendingReceiver(
    mojo::PendingReceiver<crosapi::TelemetryManagementService> receiver) {
  receiver_.Bind(std::move(receiver));
}

mojo::PendingRemote<crosapi::TelemetryManagementService>
FakeTelemetryManagementService::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void FakeTelemetryManagementService::SetAudioGain(
    uint64_t node_id,
    int32_t gain,
    SetAudioGainCallback callback) {
  std::move(callback).Run(true);
}

void FakeTelemetryManagementService::SetAudioVolume(
    uint64_t node_id,
    int32_t volume,
    bool is_muted,
    SetAudioVolumeCallback callback) {
  std::move(callback).Run(true);
}

}  // namespace chromeos

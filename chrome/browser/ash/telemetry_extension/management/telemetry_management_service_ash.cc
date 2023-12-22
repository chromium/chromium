// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/management/telemetry_management_service_ash.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

namespace {

namespace crosapi = crosapi::mojom;

constexpr int32_t kMaxAudioGain = 100;
constexpr int32_t kMinAudioGain = 0;

}  // namespace

// static
TelemetryManagementServiceAsh::Factory*
    TelemetryManagementServiceAsh::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<crosapi::TelemetryManagementService>
TelemetryManagementServiceAsh::Factory::Create(
    mojo::PendingReceiver<crosapi::TelemetryManagementService> receiver) {
  if (test_factory_) {
    return test_factory_->CreateInstance(std::move(receiver));
  }

  auto telemetry_management_service =
      std::make_unique<TelemetryManagementServiceAsh>();
  telemetry_management_service->BindReceiver(std::move(receiver));
  return telemetry_management_service;
}

// static
void TelemetryManagementServiceAsh::Factory::SetForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

TelemetryManagementServiceAsh::Factory::~Factory() = default;

TelemetryManagementServiceAsh::TelemetryManagementServiceAsh() = default;

TelemetryManagementServiceAsh::~TelemetryManagementServiceAsh() = default;

void TelemetryManagementServiceAsh::BindReceiver(
    mojo::PendingReceiver<crosapi::TelemetryManagementService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void TelemetryManagementServiceAsh::SetAudioGain(
    uint64_t node_id,
    int32_t gain,
    SetAudioGainCallback callback) {
  gain = std::clamp(gain, kMinAudioGain, kMaxAudioGain);
  CrasAudioHandler::Get()->SetVolumeGainPercentForDevice(node_id, gain);
  std::move(callback).Run();
}

}  // namespace ash

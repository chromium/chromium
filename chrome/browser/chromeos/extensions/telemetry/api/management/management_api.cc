// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/management/management_api.h"

#include <inttypes.h>

#include <algorithm>
#include <optional>

#include "chrome/common/chromeos/extensions/api/management.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "extensions/common/permissions/permissions_data.h"

namespace chromeos {

namespace {
namespace cx_manage = api::os_management;

constexpr int32_t kMaxAudioGain = 100;
constexpr int32_t kMinAudioGain = 0;
constexpr int32_t kMaxAudioVolume = 100;
constexpr int32_t kMinAudioVolume = 0;
}  // namespace

// ManagementApiFunctionBase ---------------------------------------------------

ManagementApiFunctionBase::ManagementApiFunctionBase() = default;
ManagementApiFunctionBase::~ManagementApiFunctionBase() = default;

template <class Params>
std::optional<Params> ManagementApiFunctionBase::GetParams() {
  auto params = Params::Create(args());
  if (!params) {
    SetBadMessage();
    Respond(BadMessage());
  }

  return params;
}

// OsManagementSetAudioGainFunction --------------------------------------------

void OsManagementSetAudioGainFunction::RunIfAllowed() {
  // Clamping |gain| to [0, 100] is done in Telemetry Extension Service
  // `TelemetryManagementServiceAsh::SetAudioGain`.
  const auto params = GetParams<cx_manage::SetAudioGain::Params>();
  if (!params) {
    return;
  }

  // Only input audio node is supported.
  auto node_id = params.value().args.node_id;
  auto* cras_audio_handler = ash::CrasAudioHandler::Get();
  const ash::AudioDevice* device = cras_audio_handler->GetDeviceFromId(node_id);
  if (!device || !device->is_input) {
    Respond(WithArguments(false));
    return;
  }

  auto gain =
      std::clamp(params.value().args.gain, kMinAudioGain, kMaxAudioGain);
  cras_audio_handler->SetVolumeGainPercentForDevice(node_id, gain);
  Respond(WithArguments(true));
}

// OsManagementSetAudioVolumeFunction ------------------------------------------

void OsManagementSetAudioVolumeFunction::RunIfAllowed() {
  // Clamping |volume| to [0, 100] is done in Telemetry Extension Service
  // `TelemetryManagementServiceAsh::SetAudioVolume`.
  const auto params = GetParams<cx_manage::SetAudioVolume::Params>();
  if (!params) {
    return;
  }

  // Only output audio node is supported.
  auto node_id = params.value().args.node_id;
  auto* cras_audio_handler = ash::CrasAudioHandler::Get();
  const ash::AudioDevice* device = cras_audio_handler->GetDeviceFromId(node_id);
  if (!device || device->is_input) {
    Respond(WithArguments(false));
    return;
  }

  auto volume =
      std::clamp(params.value().args.volume, kMinAudioVolume, kMaxAudioVolume);
  cras_audio_handler->SetVolumeGainPercentForDevice(node_id, volume);
  cras_audio_handler->SetMuteForDevice(node_id, params.value().args.is_muted);
  Respond(WithArguments(true));
}

}  // namespace chromeos

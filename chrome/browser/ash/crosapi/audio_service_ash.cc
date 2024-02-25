// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/audio_service_ash.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/api/audio/audio_device_id_calculator.h"
#include "extensions/browser/api/audio/audio_service_utils.h"

namespace crosapi {

AudioServiceAsh::Observer::Observer() = default;
AudioServiceAsh::Observer::~Observer() = default;

void AudioServiceAsh::Observer::Initialize(extensions::AudioService* service) {
  DCHECK(service);
  audio_service_observation_.Observe(service);
}

void AudioServiceAsh::Observer::OnLevelChanged(const std::string& id,
                                               int level) {
  for (auto& observer : observers_) {
    observer->OnLevelChanged(id, level);
  }
}

void AudioServiceAsh::Observer::OnMuteChanged(bool is_input, bool is_muted) {
  for (auto& observer : observers_) {
    observer->OnMuteChanged(is_input, is_muted);
  }
}

void AudioServiceAsh::Observer::OnDevicesChanged(
    const extensions::DeviceInfoList& devices) {
  for (auto& observer : observers_) {
    std::vector<mojom::AudioDeviceInfoPtr> result;
    base::ranges::transform(devices, std::back_inserter(result),
                            extensions::ConvertAudioDeviceInfoToMojom);
    observer->OnDeviceListChanged(std::move(result));
  }
}

void AudioServiceAsh::Observer::AddCrosapiObserver(
    mojo::PendingRemote<mojom::AudioChangeObserver> observer) {
  mojo::Remote<mojom::AudioChangeObserver> remote(std::move(observer));
  observers_.Add(std::move(remote));
}

AudioServiceAsh::AudioServiceAsh() = default;
AudioServiceAsh::~AudioServiceAsh() = default;

void AudioServiceAsh::Initialize(Profile* profile) {
  CHECK(profile);
  if (stable_id_calculator_) {
    VLOG(1) << "AudioServiceAsh is already initialized. Skip init.";
    return;
  }

  stable_id_calculator_ =
      std::make_unique<extensions::AudioDeviceIdCalculator>(profile);
  service_ =
      extensions::AudioService::CreateInstance(stable_id_calculator_.get());
  observer_.Initialize(service_.get());
}

void AudioServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::AudioService> pending_receiver) {
  DCHECK(stable_id_calculator_);
  DCHECK(service_);
  receivers_.Add(this, std::move(pending_receiver));
}

void AudioServiceAsh::GetDevices(mojom::DeviceFilterPtr filter,
                                 GetDevicesCallback callback) {
  DCHECK(service_);
  const auto ext_filter = extensions::ConvertDeviceFilterFromMojom(filter);

  auto extapi_callback = base::BindOnce(
      [](GetDevicesCallback crosapi_callback, bool success,
         std::vector<extensions::api::audio::AudioDeviceInfo> devices_src) {
        std::optional<std::vector<mojom::AudioDeviceInfoPtr>> result;

        if (success) {
          result.emplace();  // construct empty vector in-place
          result->reserve(devices_src.size());
          base::ranges::transform(devices_src,
                                  std::back_inserter(result.value()),
                                  extensions::ConvertAudioDeviceInfoToMojom);
        }

        std::move(crosapi_callback).Run(std::move(result));
      },
      std::move(callback));

  service_->GetDevices(ext_filter.get(), std::move(extapi_callback));
}

void AudioServiceAsh::GetMute(mojom::StreamType stream_type,
                              GetMuteCallback callback) {
  DCHECK(service_);

  if (stream_type == mojom::StreamType::kNone) {
    std::move(callback).Run(false, false);
    return;
  }

  const bool is_input = (stream_type == mojom::StreamType::kInput);
  service_->GetMute(is_input, std::move(callback));
}

void AudioServiceAsh::SetActiveDeviceLists(
    mojom::DeviceIdListsPtr ids,
    SetActiveDeviceListsCallback callback) {
  DCHECK(service_);
  const extensions::DeviceIdList* input_devices =
      ids ? &(ids->inputs) : nullptr;
  const extensions::DeviceIdList* output_devices =
      ids ? &(ids->outputs) : nullptr;

  service_->SetActiveDeviceLists(input_devices, output_devices,
                                 std::move(callback));
}

void AudioServiceAsh::SetMute(mojom::StreamType stream_type,
                              bool is_muted,
                              SetMuteCallback callback) {
  DCHECK(service_);

  if (stream_type == mojom::StreamType::kNone) {
    std::move(callback).Run(false);
    return;
  }

  const bool is_input = (stream_type == mojom::StreamType::kInput);
  service_->SetMute(is_input, is_muted, std::move(callback));
}

void AudioServiceAsh::SetProperties(const std::string& id,
                                    mojom::AudioDevicePropertiesPtr properties,
                                    SetPropertiesCallback callback) {
  DCHECK(service_);
  if (properties) {
    service_->SetDeviceSoundLevel(id, properties->level, std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

void AudioServiceAsh::AddAudioChangeObserver(
    mojo::PendingRemote<mojom::AudioChangeObserver> observer) {
  observer_.AddCrosapiObserver(std::move(observer));
}

}  // namespace crosapi

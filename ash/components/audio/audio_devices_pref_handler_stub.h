// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_STUB_H_
#define ASH_COMPONENTS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_STUB_H_

#include <stdint.h>

#include <map>

#include "ash/components/audio/audio_devices_pref_handler.h"
#include "base/component_export.h"

namespace ash {

// Stub class for AudioDevicesPrefHandler, used for testing.
class COMPONENT_EXPORT(ASH_COMPONENTS_AUDIO) AudioDevicesPrefHandlerStub
    : public AudioDevicesPrefHandler {
 public:
  struct DeviceState {
    bool active;
    bool activate_by_user;
  };

  using AudioDeviceMute = std::map<uint64_t, bool>;
  using AudioDeviceVolumeGain = std::map<uint64_t, int>;
  using AudioDeviceStateMap = std::map<uint64_t, DeviceState>;

  AudioDevicesPrefHandlerStub();

  AudioDevicesPrefHandlerStub(const AudioDevicesPrefHandlerStub&) = delete;
  AudioDevicesPrefHandlerStub& operator=(const AudioDevicesPrefHandlerStub&) =
      delete;

  // AudioDevicesPrefHandler:
  double GetOutputVolumeValue(const AudioDevice* device) override;
  double GetInputGainValue(const AudioDevice* device) override;
  void SetVolumeGainValue(const AudioDevice& device, double value) override;
  bool GetMuteValue(const AudioDevice& device) override;
  void SetMuteValue(const AudioDevice& device, bool mute_on) override;
  void SetDeviceActive(const AudioDevice& device,
                       bool active,
                       bool activate_by_user) override;
  bool GetDeviceActive(const AudioDevice& device,
                       bool* active,
                       bool* activate_by_user) override;
  bool GetAudioOutputAllowedValue() override;
  void AddAudioPrefObserver(AudioPrefObserver* observer) override;
  void RemoveAudioPrefObserver(AudioPrefObserver* observer) override;

  bool GetNoiseCancellationState() override;
  void SetNoiseCancellationState(bool noise_cancellation_state) override;

 protected:
  ~AudioDevicesPrefHandlerStub() override;

 private:
  AudioDeviceMute audio_device_mute_map_;
  AudioDeviceVolumeGain audio_device_volume_gain_map_;
  AudioDeviceStateMap audio_device_state_map_;

  bool noise_cancellation_state_ = true;
};

}  // namespace ash

#endif  // ASH_COMPONENTS_AUDIO_AUDIO_DEVICES_PREF_HANDLER_STUB_H_

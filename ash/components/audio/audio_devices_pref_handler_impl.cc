// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/audio/audio_devices_pref_handler_impl.h"

#include <stdint.h>

#include <algorithm>

#include "ash/components/audio/audio_device.h"
#include "ash/constants/ash_pref_names.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {
namespace {

// Values used for muted preference.
const int kPrefMuteOff = 0;
const int kPrefMuteOn = 1;

// Prefs keys.
const char kActiveKey[] = "active";
const char kActivateByUserKey[] = "activate_by_user";

// Gets the device id string for storing audio preference. The format of
// device string is a string consisting of 3 parts:
// |version of stable device ID| :
// |integer from lower 32 bit of device id| :
// |0(output device) or 1(input device)|
// If an audio device has both integrated input and output devices, the first 2
// parts of the string could be identical, only the last part will differentiate
// them.
// Note that |version of stable device ID| is present only for devices with
// stable device ID version >= 2. For devices with version 1, the device id
// string contains only latter 2 parts - in order to preserve backward
// compatibility with existing ID from before v2 stable device ID was
// introduced.
std::string GetVersionedDeviceIdString(const AudioDevice& device, int version) {
  CHECK(device.stable_device_id_version >= version);
  DCHECK_GE(device.stable_device_id_version, 1);
  DCHECK_LE(device.stable_device_id_version, 2);

  bool use_deprecated_id = version == 1 && device.stable_device_id_version == 2;
  uint64_t stable_device_id = use_deprecated_id
                                  ? device.deprecated_stable_device_id
                                  : device.stable_device_id;
  std::string version_prefix = version == 2 ? "2 : " : "";
  std::string device_id_string =
      version_prefix +
      base::NumberToString(stable_device_id &
                           static_cast<uint64_t>(0xffffffff)) +
      " : " + (device.is_input ? "1" : "0");
  // Replace any periods from the device id string with a space, since setting
  // names cannot contain periods.
  std::replace(device_id_string.begin(), device_id_string.end(), '.', ' ');
  return device_id_string;
}

std::string GetDeviceIdString(const AudioDevice& device) {
  return GetVersionedDeviceIdString(device, device.stable_device_id_version);
}

// Migrates an entry associated with |device|'s v1 stable device ID in
// |settings| to the key derived from |device|'s v2 stable device ID
// (which is expected to be equal to |intended_key|), if the entry can
// be found.
// Returns whether the migration occurred.
bool MigrateDeviceIdInSettings(base::Value* settings,
                               const std::string& intended_key,
                               const AudioDevice& device) {
  if (device.stable_device_id_version == 1)
    return false;

  DCHECK_EQ(2, device.stable_device_id_version);

  std::string old_device_id = GetVersionedDeviceIdString(device, 1);
  absl::optional<base::Value> value = settings->ExtractKey(old_device_id);
  if (!value)
    return false;

  DCHECK_EQ(intended_key, GetDeviceIdString(device));
  settings->SetPath(intended_key, std::move(*value));
  return true;
}

}  // namespace

double AudioDevicesPrefHandlerImpl::GetOutputVolumeValue(
    const AudioDevice* device) {
  if (!device)
    return kDefaultOutputVolumePercent;
  else
    return GetOutputVolumePrefValue(*device);
}

double AudioDevicesPrefHandlerImpl::GetInputGainValue(
    const AudioDevice* device) {
  DCHECK(device);
  return GetInputGainPrefValue(*device);
}

void AudioDevicesPrefHandlerImpl::SetVolumeGainValue(
    const AudioDevice& device, double value) {
  // TODO(baileyberro): Refactor public interface to use two explicit methods.
  device.is_input ? SetInputGainPrefValue(device, value)
                  : SetOutputVolumePrefValue(device, value);
}

void AudioDevicesPrefHandlerImpl::SetOutputVolumePrefValue(
    const AudioDevice& device,
    double value) {
  DCHECK(!device.is_input);
  // Use this opportunity to remove device record under deprecated device ID,
  // if one exists.
  if (device.stable_device_id_version == 2) {
    std::string old_device_id = GetVersionedDeviceIdString(device, 1);
    device_volume_settings_.RemoveKey(old_device_id);
  }
  device_volume_settings_.SetDoubleKey(GetDeviceIdString(device), value);

  SaveDevicesVolumePref();
}

void AudioDevicesPrefHandlerImpl::SetInputGainPrefValue(
    const AudioDevice& device,
    double value) {
  DCHECK(device.is_input);

  const std::string device_id = GetDeviceIdString(device);

  // Use this opportunity to remove input device record from
  // |device_volume_settings_|.
  // TODO(baileyberro): Remove this check in M94.
  if (device_volume_settings_.FindKey(device_id)) {
    device_volume_settings_.RemoveKey(device_id);
    SaveDevicesVolumePref();
  }

  device_gain_settings_.SetDoubleKey(device_id, value);
  SaveDevicesGainPref();
}

bool AudioDevicesPrefHandlerImpl::GetMuteValue(const AudioDevice& device) {
  std::string device_id_str = GetDeviceIdString(device);
  if (!device_mute_settings_.FindKey(device_id_str))
    MigrateDeviceMuteSettings(device_id_str, device);

  int mute =
      device_mute_settings_.FindIntKey(device_id_str).value_or(kPrefMuteOff);
  return (mute == kPrefMuteOn);
}

void AudioDevicesPrefHandlerImpl::SetMuteValue(const AudioDevice& device,
                                               bool mute) {
  // Use this opportunity to remove device record under deprecated device ID,
  // if one exists.
  if (device.stable_device_id_version == 2) {
    std::string old_device_id = GetVersionedDeviceIdString(device, 1);
    device_mute_settings_.RemoveKey(old_device_id);
  }
  device_mute_settings_.SetIntKey(GetDeviceIdString(device),
                                  mute ? kPrefMuteOn : kPrefMuteOff);
  SaveDevicesMutePref();
}

void AudioDevicesPrefHandlerImpl::SetDeviceActive(const AudioDevice& device,
                                                  bool active,
                                                  bool activate_by_user) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetBoolKey(kActiveKey, active);
  if (active)
    dict.SetBoolKey(kActivateByUserKey, activate_by_user);

  // Use this opportunity to remove device record under deprecated device ID,
  // if one exists.
  if (device.stable_device_id_version == 2) {
    std::string old_device_id = GetVersionedDeviceIdString(device, 1);
    device_state_settings_.RemoveKey(old_device_id);
  }
  device_state_settings_.SetPath(GetDeviceIdString(device), std::move(dict));
  SaveDevicesStatePref();
}

bool AudioDevicesPrefHandlerImpl::GetDeviceActive(const AudioDevice& device,
                                                  bool* active,
                                                  bool* activate_by_user) {
  const std::string device_id_str = GetDeviceIdString(device);
  if (!device_state_settings_.FindKey(device_id_str) &&
      !MigrateDevicesStatePref(device_id_str, device)) {
    return false;
  }

  base::Value* dict = device_state_settings_.FindDictPath(device_id_str);
  if (!dict) {
    LOG(ERROR) << "Could not get device state for device:" << device.ToString();
    return false;
  }

  absl::optional<bool> active_opt = dict->FindBoolKey(kActiveKey);
  if (!active_opt.has_value()) {
    LOG(ERROR) << "Could not get active value for device:" << device.ToString();
    return false;
  }

  *active = active_opt.value();
  if (!*active)
    return true;

  absl::optional<bool> activate_by_user_opt =
      dict->FindBoolKey(kActivateByUserKey);
  if (!activate_by_user_opt.has_value()) {
    LOG(ERROR) << "Could not get activate_by_user value for previously "
                  "active device:"
               << device.ToString();
    return false;
  }

  *activate_by_user = activate_by_user_opt.value();
  return true;
}

bool AudioDevicesPrefHandlerImpl::GetAudioOutputAllowedValue() const {
  return local_state_->GetBoolean(prefs::kAudioOutputAllowed);
}

void AudioDevicesPrefHandlerImpl::AddAudioPrefObserver(
    AudioPrefObserver* observer) {
  observers_.AddObserver(observer);
}

void AudioDevicesPrefHandlerImpl::RemoveAudioPrefObserver(
    AudioPrefObserver* observer) {
  observers_.RemoveObserver(observer);
}

double AudioDevicesPrefHandlerImpl::GetOutputVolumePrefValue(
    const AudioDevice& device) {
  DCHECK(!device.is_input);
  std::string device_id_str = GetDeviceIdString(device);
  if (!device_volume_settings_.FindKey(device_id_str))
    MigrateDeviceVolumeGainSettings(device_id_str, device);
  return *device_volume_settings_.FindDoubleKey(device_id_str);
}

double AudioDevicesPrefHandlerImpl::GetInputGainPrefValue(
    const AudioDevice& device) {
  DCHECK(device.is_input);
  std::string device_id_str = GetDeviceIdString(device);
  if (!device_gain_settings_.FindKey(device_id_str))
    SetInputGainPrefValue(device, kDefaultInputGainPercent);
  return *device_gain_settings_.FindDoubleKey(device_id_str);
}

double AudioDevicesPrefHandlerImpl::GetDeviceDefaultOutputVolume(
    const AudioDevice& device) {
  if (device.type == AudioDeviceType::kHdmi)
    return kDefaultHdmiOutputVolumePercent;
  else
    return kDefaultOutputVolumePercent;
}

bool AudioDevicesPrefHandlerImpl::GetNoiseCancellationState() {
  return local_state_->GetBoolean(prefs::kInputNoiseCancellationEnabled);
}

void AudioDevicesPrefHandlerImpl::SetNoiseCancellationState(
    bool noise_cancellation_state) {
  local_state_->SetBoolean(prefs::kInputNoiseCancellationEnabled,
                           noise_cancellation_state);
}

AudioDevicesPrefHandlerImpl::AudioDevicesPrefHandlerImpl(
    PrefService* local_state)
    : device_mute_settings_(base::Value::Type::DICTIONARY),
      device_volume_settings_(base::Value::Type::DICTIONARY),
      device_gain_settings_(base::Value::Type::DICTIONARY),
      device_state_settings_(base::Value::Type::DICTIONARY),
      local_state_(local_state) {
  InitializePrefObservers();

  LoadDevicesMutePref();
  LoadDevicesVolumePref();
  LoadDevicesGainPref();
  LoadDevicesStatePref();
}

AudioDevicesPrefHandlerImpl::~AudioDevicesPrefHandlerImpl() = default;

void AudioDevicesPrefHandlerImpl::InitializePrefObservers() {
  pref_change_registrar_.Init(local_state_);
  base::RepeatingClosure callback =
      base::BindRepeating(&AudioDevicesPrefHandlerImpl::NotifyAudioPolicyChange,
                          base::Unretained(this));
  pref_change_registrar_.Add(prefs::kAudioOutputAllowed, callback);
}

void AudioDevicesPrefHandlerImpl::LoadDevicesMutePref() {
  const base::Value* mute_prefs =
      local_state_->GetDictionary(prefs::kAudioDevicesMute);
  if (mute_prefs)
    device_mute_settings_ = mute_prefs->Clone();
}

void AudioDevicesPrefHandlerImpl::SaveDevicesMutePref() {
  DictionaryPrefUpdate dict_update(local_state_, prefs::kAudioDevicesMute);
  dict_update->DictClear();
  dict_update->MergeDictionary(&device_mute_settings_);
}

void AudioDevicesPrefHandlerImpl::LoadDevicesVolumePref() {
  const base::Value* volume_prefs =
      local_state_->GetDictionary(prefs::kAudioDevicesVolumePercent);
  if (volume_prefs)
    device_volume_settings_ = volume_prefs->Clone();
}

void AudioDevicesPrefHandlerImpl::SaveDevicesVolumePref() {
  DictionaryPrefUpdate dict_update(local_state_,
                                   prefs::kAudioDevicesVolumePercent);
  dict_update->DictClear();
  dict_update->MergeDictionary(&device_volume_settings_);
}

void AudioDevicesPrefHandlerImpl::LoadDevicesGainPref() {
  const base::Value* gain_prefs =
      local_state_->GetDictionary(prefs::kAudioDevicesGainPercent);
  if (gain_prefs)
    device_gain_settings_ = gain_prefs->Clone();
}

void AudioDevicesPrefHandlerImpl::SaveDevicesGainPref() {
  DictionaryPrefUpdate dict_update(local_state_,
                                   prefs::kAudioDevicesGainPercent);
  dict_update->DictClear();
  dict_update->MergeDictionary(&device_gain_settings_);
}

void AudioDevicesPrefHandlerImpl::LoadDevicesStatePref() {
  const base::Value* state_prefs =
      local_state_->GetDictionary(prefs::kAudioDevicesState);
  if (state_prefs)
    device_state_settings_ = state_prefs->Clone();
}

void AudioDevicesPrefHandlerImpl::SaveDevicesStatePref() {
  DictionaryPrefUpdate dict_update(local_state_, prefs::kAudioDevicesState);
  dict_update->DictClear();
  dict_update->MergeDictionary(&device_state_settings_);
}

bool AudioDevicesPrefHandlerImpl::MigrateDevicesStatePref(
    const std::string& device_key,
    const AudioDevice& device) {
  if (!MigrateDeviceIdInSettings(&device_state_settings_, device_key, device)) {
    return false;
  }

  SaveDevicesStatePref();
  return true;
}

void AudioDevicesPrefHandlerImpl::MigrateDeviceMuteSettings(
    const std::string& device_key,
    const AudioDevice& device) {
  if (!MigrateDeviceIdInSettings(&device_mute_settings_, device_key, device)) {
    // If there was no recorded value for deprecated device ID, use value from
    // global mute pref.
    int old_mute = local_state_->GetInteger(prefs::kAudioMute);
    device_mute_settings_.SetIntKey(device_key, old_mute);
  }
  SaveDevicesMutePref();
}

void AudioDevicesPrefHandlerImpl::MigrateDeviceVolumeGainSettings(
    const std::string& device_key,
    const AudioDevice& device) {
  DCHECK(!device.is_input);
  if (!MigrateDeviceIdInSettings(&device_volume_settings_, device_key,
                                 device)) {
    // If there was no recorded value for deprecated device ID, use value from
    // global vloume pref.
    double old_volume = local_state_->GetDouble(prefs::kAudioVolumePercent);
    device_volume_settings_.SetDoubleKey(device_key, old_volume);
  }
  SaveDevicesVolumePref();
}

void AudioDevicesPrefHandlerImpl::NotifyAudioPolicyChange() {
  for (auto& observer : observers_)
    observer.OnAudioPolicyPrefChanged();
}

// static
void AudioDevicesPrefHandlerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kAudioDevicesVolumePercent);
  registry->RegisterDictionaryPref(prefs::kAudioDevicesGainPercent);
  registry->RegisterDictionaryPref(prefs::kAudioDevicesMute);
  registry->RegisterDictionaryPref(prefs::kAudioDevicesState);
  registry->RegisterBooleanPref(prefs::kInputNoiseCancellationEnabled, false);

  // Register the prefs backing the audio muting policies.
  // Policy for audio input is handled by kAudioCaptureAllowed in the Chrome
  // media system.
  registry->RegisterBooleanPref(prefs::kAudioOutputAllowed, true);

  // Register the legacy audio prefs for migration.
  registry->RegisterDoublePref(prefs::kAudioVolumePercent,
                               kDefaultOutputVolumePercent);
  registry->RegisterIntegerPref(prefs::kAudioMute, kPrefMuteOff);
}

}  // namespace ash

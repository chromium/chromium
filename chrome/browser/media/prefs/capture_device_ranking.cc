// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/prefs/capture_device_ranking.h"

#include "chrome/browser/media/prefs/pref_names.h"
#include "media/capture/video/video_capture_device_info.h"

#include <string>

namespace media_prefs {

void RegisterUserPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kAudioInputUserPreferenceRanking);
  registry->RegisterListPref(kVideoInputUserPreferenceRanking);
}

namespace internal {

std::string DeviceInfoToStableId(
    const media::VideoCaptureDeviceInfo& device_info) {
  return device_info.descriptor.GetNameAndModel();
}

std::string DeviceInfoToStableId(const blink::WebMediaDeviceInfo& device_info) {
  return device_info.label;
}

std::string DeviceInfoToStableId(const blink::MediaStreamDevice& device_info) {
  return device_info.name;
}

std::string DeviceInfoToStableId(
    const media::AudioDeviceDescription& device_info) {
  return device_info.device_name;
}

std::string DeviceInfoToUniqueId(
    const media::VideoCaptureDeviceInfo& device_info) {
  return device_info.descriptor.device_id;
}

std::string DeviceInfoToUniqueId(const blink::WebMediaDeviceInfo& device_info) {
  return device_info.device_id;
}

std::string DeviceInfoToUniqueId(const blink::MediaStreamDevice& device_info) {
  return device_info.id;
}

std::string DeviceInfoToUniqueId(
    const media::AudioDeviceDescription& device_info) {
  return device_info.unique_id;
}
}  // namespace internal
}  // namespace media_prefs

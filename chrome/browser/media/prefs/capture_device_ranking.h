// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_PREFS_CAPTURE_DEVICE_RANKING_H_
#define CHROME_BROWSER_MEDIA_PREFS_CAPTURE_DEVICE_RANKING_H_

#include "chrome/browser/media/prefs/pref_names.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "media/audio/audio_device_description.h"
#include "media/capture/video/video_capture_device_info.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

#include <string>

namespace media_prefs::internal {

// Returns a stable device id given `device_info`. This isn't guaranteed
// to be unique, but it is stable across reboots and device plug/unplug.
std::string DeviceInfoToStableId(
    const media::VideoCaptureDeviceInfo& device_info);
std::string DeviceInfoToStableId(
    const media::AudioDeviceDescription& device_info);
std::string DeviceInfoToStableId(const blink::WebMediaDeviceInfo& device_info);
std::string DeviceInfoToStableId(const blink::MediaStreamDevice& device_info);

// Returns a unique id given `device_info`. This is unique for the current
// session, but isn't guaranteed to be stable through reboots or device
// plug/unplug.
std::string DeviceInfoToUniqueId(
    const media::VideoCaptureDeviceInfo& device_info);
std::string DeviceInfoToUniqueId(
    const media::AudioDeviceDescription& device_info);
std::string DeviceInfoToUniqueId(const blink::WebMediaDeviceInfo& device_info);
std::string DeviceInfoToUniqueId(const blink::MediaStreamDevice& device_info);

}  // namespace media_prefs::internal

namespace {

using media_prefs::internal::DeviceInfoToStableId;
using media_prefs::internal::DeviceInfoToUniqueId;

// Returns the rank index for the passed `info`. If `info` doesn't have a
// rank then return size of `id_to_rank` because that will rank lower than
// any of the ranked devices.
template <typename T>
size_t GetRank(const T& info,
               const base::flat_map<std::string, size_t>& id_to_rank) {
  auto found_iter = id_to_rank.find(DeviceInfoToStableId(info));
  if (found_iter != id_to_rank.end()) {
    return found_iter->second;
  }

  return id_to_rank.size();
}

// Map device ids to a numeric ranking. Smaller values are more preferred.
template <typename Iterator>
base::flat_map<std::string, size_t> GetIdToRankMap(Iterator begin,
                                                   Iterator end) {
  base::flat_map<std::string, size_t> id_to_rank;
  for (auto iter = begin; iter < end; ++iter) {
    id_to_rank[iter->GetString()] = iter - begin;
  }

  return id_to_rank;
}

template <typename T>
const base::Value::List& GetAndMaybeMigratePref(PrefService& prefs,
                                                const std::string& pref_name,
                                                std::vector<T>& device_infos);

// Reorders the passed `device_infos`, so that ranked devices are ordered at the
// beginning of the list. Devices don't have a ranking will retain their
// ordering, but will be shifted below ranked devices.
//
// This must be run on the UI thread as it reads from the PrefService.
template <typename T>
void PreferenceRankDeviceInfos(PrefService& prefs,
                               const std::string& pref_name,
                               std::vector<T>& device_infos) {
  if (!prefs.FindPreference(pref_name)) {
    LOG(WARNING) << "Can't rank device infos because " << pref_name
                 << " isn't registered";
    return;
  }
  const base::Value::List& ranking =
      GetAndMaybeMigratePref(prefs, pref_name, device_infos);
  const auto id_to_rank = GetIdToRankMap(ranking.begin(), ranking.end());

  std::stable_sort(
      device_infos.begin(), device_infos.end(),
      [id_to_rank = std::move(id_to_rank)](const T& a, const T& b) {
        return GetRank(a, id_to_rank) < GetRank(b, id_to_rank);
      });
}

// Updates device ranking given a preferred device and the list of competing
// devices.
//
// The algorithm is as follows:
// 1. Identify the best ranked device among the current devices
//   a. If a device isn't found in the ranking list, then it is appended
// 2. If the best ranked device is the preferred device, then exit
// 3. Else, move the preferred device to be just before the best ranked device
//
// The goal of the algorithm is to ensure that given the same set of devices we
// would get the same device ranking.
//
// Example case:
// Update 1 -
// The user selects the USB mic as preferred, and the other current
// devices are at the end of the list in the order enumerated by the OS.
// current ranking: []
// preferred_device_iter: 'USB Camera Mic',
// current_device_infos: 'bluetooth mic', 'builtin mic', 'USB Camera Mic'
//
// updated ranking: 'USB Camera Mic', 'bluetooth mic', 'builtin mic'
//
// Update 2 -
// The user disconnects the USB camera because they left their desk
// and they select the 'builtin mic' as preferred.
// current ranking: 'USB Camera Mic', 'builtin mic', 'bluetooth mic'
// preferred_device_iter: 'builtin mic'
// current_deivce_infos: 'bluetooth mic', 'builtin mic'
//
// updated ranking: 'USB Camera Mic', 'builtin mic', 'bluetooth mic'
//
// Note that the second update leaves the 'USB Camera Mic' as preferred over the
// other two mics because it wasn't present to be considered.
//
// This must be run on the UI thread as it read/writes to the PrefService.
template <typename T>
void UpdateDevicePreferenceRanking(
    PrefService& prefs,
    const std::string& pref_name,
    const typename std::vector<T>::const_iterator preferred_device_iter,
    const std::vector<T>& current_device_infos) {
  CHECK(preferred_device_iter < current_device_infos.end());
  auto preferred_device_stable_id =
      DeviceInfoToStableId(*preferred_device_iter);
  ScopedListPrefUpdate ranking(&prefs, pref_name);
  const auto id_to_rank = GetIdToRankMap(ranking->begin(), ranking->end());

  // Start `best_competing_rank` with a bigger number than is possible.
  auto best_competing_rank = ranking->size();
  for (const auto& device_info : current_device_infos) {
    // Skip the preferred device because we're looking for the best ranked
    // competing device.
    if (DeviceInfoToUniqueId(device_info) ==
        DeviceInfoToUniqueId(*preferred_device_iter)) {
      continue;
    }

    auto rank = id_to_rank.find(DeviceInfoToStableId(device_info));
    if (rank == id_to_rank.end()) {
      // The device doesn't have a rank, so insert it at the end of the list.
      ranking->Append(DeviceInfoToStableId(device_info));
      continue;
    }

    if (rank->second < best_competing_rank) {
      best_competing_rank = rank->second;
    }
  }

  auto preferred_device_rank = id_to_rank.find(preferred_device_stable_id);
  if (preferred_device_rank == id_to_rank.end()) {
    // The preferred device didn't previously have a rank, so insert it before
    // the best ranked competitor.
    ranking->Insert(ranking->begin() + best_competing_rank,
                    base::Value{preferred_device_stable_id});
    return;
  }

  // Only move the preferred device if there is a better ranked competitor.
  if (best_competing_rank < preferred_device_rank->second) {
    // Move the preferred device to be just before the best ranked competitor.
    ranking->erase(ranking->begin() + preferred_device_rank->second);
    ranking->Insert(ranking->begin() + best_competing_rank,
                    base::Value{preferred_device_stable_id});
  }
}

// Get the value of the ranking pref.
//
// If the ranking pref is unset, it will be initialized from the default device
// pref. If the default device pref is unset or the device isn't in
// `device_infos`, then the ranking pref will remain uninitialized.
// `pref_name` is required to be in {kAudioInputUserPreferenceRanking,
// kVideoInputUserPreferenceRanking}.
//
// Example scenarios:
// For all scenarios the user has set dev2 as the default and updated the
// browser.
//
// Scenario A:
//  1) User connects [dev1, dev2]
//  2) User visits a website that calls enumerateDevices or getUserMedia
//  3) Device ranking pref is updated to [dev2, dev1]
//  4) All subsequent usages refer to the device ranking pref and ignore legacy
//     default pref
//
// Scenario B:
//  1) User connects [dev1]
//  2) User visits a website that calls enumerateDevices or getUserMedia
//  3) Device ranking pref remains uninitialized because the default device
//     isn't present
//  4) User connects [dev1, dev2]
//  5) User visits a website that calls enumerateDevices or getUserMedia
//  6) Device ranking pref is updated to [dev2, dev1]
//  7) All subsequent usages refer to the device ranking pref and
//     ignore legacy default pref.
//
// Scenario C:
//  1) User connects [dev1]
//  2) User visits a website that calls enumerateDevices or getUserMedia
//  3) Device ranking pref remains uninitialized because the default device
//     isn't present
//  4) User expresses preference for dev1 in permission bubble or
//     Chrome settings
//  5) Device ranking pref is updated to [dev1]
//  6) All subsequent usages refer to the device ranking pref and ignore legacy
//     default pref.
//
// TODO(crbug.com/311205211): Remove this special initialization logic once the
// default device pref is removed.
template <typename T>
const base::Value::List& GetAndMaybeMigratePref(PrefService& prefs,
                                                const std::string& pref_name,
                                                std::vector<T>& device_infos) {
  const base::Value::List& ranking = prefs.GetList(pref_name);
  if (!ranking.empty()) {
    return ranking;
  }

  // Initialize the ranking pref with the legacy default device because it is
  // unset.
  std::string default_device_pref_name;
  if (pref_name == kAudioInputUserPreferenceRanking) {
    default_device_pref_name = prefs::kDefaultAudioCaptureDeviceDeprecated;
  } else if (pref_name == kVideoInputUserPreferenceRanking) {
    default_device_pref_name = prefs::kDefaultVideoCaptureDeviceDeprecated;
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  if (!prefs.HasPrefPath(default_device_pref_name)) {
    LOG(WARNING) << "Can't initialize the value of " << pref_name << " because "
                 << default_device_pref_name << " isn't registered or is empty";
    return ranking;
  }

  const auto& default_id = prefs.GetString(default_device_pref_name);
  for (auto iter = device_infos.begin(); iter < device_infos.end(); ++iter) {
    if (DeviceInfoToUniqueId(*iter) == default_id) {
      UpdateDevicePreferenceRanking(prefs, pref_name, iter, device_infos);
      return prefs.GetList(pref_name);
    }
  }

  return ranking;
}

}  // namespace

namespace media_prefs {

void RegisterUserPrefs(PrefRegistrySimple* registry);

template <typename T>
void PreferenceRankAudioDeviceInfos(PrefService& prefs,
                                    std::vector<T>& device_infos) {
  static_assert(std::is_same_v<media::AudioDeviceDescription, T> ||
                std::is_same_v<blink::WebMediaDeviceInfo, T> ||
                std::is_same_v<blink::MediaStreamDevice, T>);
  PreferenceRankDeviceInfos(prefs, kAudioInputUserPreferenceRanking,
                            device_infos);
}

template <typename T>
void PreferenceRankVideoDeviceInfos(PrefService& prefs,
                                    std::vector<T>& device_infos) {
  static_assert(std::is_same_v<media::VideoCaptureDeviceInfo, T> ||
                std::is_same_v<blink::WebMediaDeviceInfo, T> ||
                std::is_same_v<blink::MediaStreamDevice, T>);
  PreferenceRankDeviceInfos(prefs, kVideoInputUserPreferenceRanking,
                            device_infos);
}

template <typename T>
void UpdateAudioDevicePreferenceRanking(
    PrefService& prefs,
    const typename std::vector<T>::const_iterator preferred_device_iter,
    const std::vector<T>& current_device_infos) {
  static_assert(std::is_same_v<media::AudioDeviceDescription, T> ||
                std::is_same_v<blink::WebMediaDeviceInfo, T> ||
                std::is_same_v<blink::MediaStreamDevice, T>);
  UpdateDevicePreferenceRanking(prefs, kAudioInputUserPreferenceRanking,
                                preferred_device_iter, current_device_infos);
}

template <typename T>
void UpdateVideoDevicePreferenceRanking(
    PrefService& prefs,
    const typename std::vector<T>::const_iterator preferred_device_iter,
    const std::vector<T>& current_device_infos) {
  static_assert(std::is_same_v<media::VideoCaptureDeviceInfo, T> ||
                std::is_same_v<blink::WebMediaDeviceInfo, T> ||
                std::is_same_v<blink::MediaStreamDevice, T>);
  UpdateDevicePreferenceRanking(prefs, kVideoInputUserPreferenceRanking,
                                preferred_device_iter, current_device_infos);
}

}  // namespace media_prefs

#endif  // CHROME_BROWSER_MEDIA_PREFS_CAPTURE_DEVICE_RANKING_H_

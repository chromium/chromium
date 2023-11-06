// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_PREFS_CAPTURE_DEVICE_RANKING_H_
#define CHROME_BROWSER_MEDIA_PREFS_CAPTURE_DEVICE_RANKING_H_

#include "chrome/browser/media/prefs/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "media/audio/audio_device_description.h"
#include "media/capture/video/video_capture_device_info.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"

#include <string>

namespace media_prefs::internal {

// Returns a stable device id given `device_info`. This isn't guaranteed
// to be unique, but it is stable across reboots and device plug/unplug.
std::string DeviceInfoToStableId(
    const media::VideoCaptureDeviceInfo& device_info);
std::string DeviceInfoToStableId(
    const media::AudioDeviceDescription& device_info);
std::string DeviceInfoToStableId(const blink::WebMediaDeviceInfo& device_info);

// Returns a unique id given `device_info`. This is unique for the current
// session, but isn't guaranteed to be stable through reboots or device
// plug/unplug.
std::string DeviceInfoToUniqueId(
    const media::VideoCaptureDeviceInfo& device_info);
std::string DeviceInfoToUniqueId(
    const media::AudioDeviceDescription& device_info);
std::string DeviceInfoToUniqueId(const blink::WebMediaDeviceInfo& device_info);

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

// Reorders the passed `device_infos`, so that ranked devices are ordered at the
// beginning of the list. Devices don't have a ranking will retain their
// ordering, but will be shifted below ranked devices.
//
// This must be run on the UI thread as it reads from the PrefService.
template <typename T>
void PreferenceRankDeviceInfos(const PrefService& prefs,
                               const std::string& pref_name,
                               std::vector<T>& device_infos) {
  if (!prefs.HasPrefPath(pref_name)) {
    LOG(WARNING) << "Can't rank device infos because " << pref_name
                 << " isn't registered";
    return;
  }
  const base::Value::List& ranking = prefs.GetList(pref_name);
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

}  // namespace

namespace media_prefs {

void RegisterUserPrefs(PrefRegistrySimple* registry);

template <typename T>
void PreferenceRankAudioDeviceInfos(const PrefService& prefs,
                                    std::vector<T>& device_infos) {
  static_assert(std::is_same_v<media::AudioDeviceDescription, T> ||
                std::is_same_v<blink::WebMediaDeviceInfo, T>);
  PreferenceRankDeviceInfos(prefs, kAudioInputUserPreferenceRanking,
                            device_infos);
}

template <typename T>
void PreferenceRankVideoDeviceInfos(const PrefService& prefs,
                                    std::vector<T>& device_infos) {
  static_assert(std::is_same_v<media::VideoCaptureDeviceInfo, T> ||
                std::is_same_v<blink::WebMediaDeviceInfo, T>);
  PreferenceRankDeviceInfos(prefs, kVideoInputUserPreferenceRanking,
                            device_infos);
}

template <typename T>
void UpdateAudioDevicePreferenceRanking(
    PrefService& prefs,
    const typename std::vector<T>::const_iterator preferred_device_iter,
    const std::vector<T>& current_device_infos) {
  static_assert(std::is_same_v<media::AudioDeviceDescription, T> ||
                std::is_same_v<blink::WebMediaDeviceInfo, T>);
  UpdateDevicePreferenceRanking(prefs, kAudioInputUserPreferenceRanking,
                                preferred_device_iter, current_device_infos);
}

template <typename T>
void UpdateVideoDevicePreferenceRanking(
    PrefService& prefs,
    const typename std::vector<T>::const_iterator preferred_device_iter,
    const std::vector<T>& current_device_infos) {
  static_assert(std::is_same_v<media::VideoCaptureDeviceInfo, T> ||
                std::is_same_v<blink::WebMediaDeviceInfo, T>);
  UpdateDevicePreferenceRanking(prefs, kVideoInputUserPreferenceRanking,
                                preferred_device_iter, current_device_infos);
}

}  // namespace media_prefs

#endif  // CHROME_BROWSER_MEDIA_PREFS_CAPTURE_DEVICE_RANKING_H_

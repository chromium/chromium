// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "components/cross_device/logging/logging.h"

const size_t kNearbyShareDeviceNameMaxLength = 32;

NearbyShareLocalDeviceDataManager::NearbyShareLocalDeviceDataManager() =
    default;

NearbyShareLocalDeviceDataManager::~NearbyShareLocalDeviceDataManager() =
    default;

void NearbyShareLocalDeviceDataManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NearbyShareLocalDeviceDataManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void NearbyShareLocalDeviceDataManager::Start() {
  if (is_running_)
    return;

  is_running_ = true;
  OnStart();
}

void NearbyShareLocalDeviceDataManager::Stop() {
  if (!is_running_)
    return;

  is_running_ = false;
  OnStop();
}

void NearbyShareLocalDeviceDataManager::NotifyLocalDeviceDataChanged(
    bool did_device_name_change,
    bool did_full_name_change,
    bool did_icon_change) {
  CD_LOG(INFO, Feature::NS)
      << __func__ << ": did_device_name_change="
      << (did_device_name_change ? "true" : "false")
      << ", did_full_name_change=" << (did_full_name_change ? "true" : "false")
      << ", did_icon_change=" << (did_icon_change ? "true" : "false");
  for (auto& observer : observers_) {
    observer.OnLocalDeviceDataChanged(did_device_name_change,
                                      did_full_name_change, did_icon_change);
  }
}

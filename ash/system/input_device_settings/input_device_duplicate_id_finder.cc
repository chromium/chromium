// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_duplicate_id_finder.h"

#include "ui/events/devices/device_data_manager.h"

namespace ash {

namespace {

template <typename T>
void UpdateList(
    const std::vector<T>& devices,
    base::flat_map<InputDeviceDuplicateIdFinder::VidPidIdentifier,
                   base::flat_set<int>>& duplicate_ids_map,
    base::flat_map<int, InputDeviceDuplicateIdFinder::VidPidIdentifier>&
        vid_pid_map) {
  for (const auto& device : devices) {
    duplicate_ids_map[{device.vendor_id, device.product_id}].insert(device.id);
    vid_pid_map[device.id] = {device.vendor_id, device.product_id};
  }
}

}  // namespace

bool operator<(const InputDeviceDuplicateIdFinder::VidPidIdentifier& lhs,
               const InputDeviceDuplicateIdFinder::VidPidIdentifier& rhs) {
  if (lhs.vendor_id != rhs.vendor_id) {
    return lhs.vendor_id < rhs.vendor_id;
  }
  return lhs.product_id < rhs.product_id;
}

InputDeviceDuplicateIdFinder::InputDeviceDuplicateIdFinder() {
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
}
InputDeviceDuplicateIdFinder::~InputDeviceDuplicateIdFinder() {
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
}

const base::flat_set<int>* InputDeviceDuplicateIdFinder::GetDuplicateDeviceIds(
    int id) const {
  auto vid_pid_iter = vid_pid_map_.find(id);
  if (vid_pid_iter == vid_pid_map_.end()) {
    return nullptr;
  }

  auto dedupes_iter = duplicate_ids_map_.find(vid_pid_iter->second);
  if (dedupes_iter == duplicate_ids_map_.end()) {
    return nullptr;
  }

  return &dedupes_iter->second;
}

void InputDeviceDuplicateIdFinder::OnInputDeviceConfigurationChanged(
    uint8_t input_device_type) {
  duplicate_ids_map_.clear();
  vid_pid_map_.clear();

  RefreshKeyboards();
  RefreshMice();
  RefreshPointingSticks();
  RefreshTouchpads();
  RefreshTouchscreens();
  RefreshGraphicsTablets();
  RefreshUncategorized();
}

void InputDeviceDuplicateIdFinder::OnDeviceListsComplete() {
  duplicate_ids_map_.clear();
  vid_pid_map_.clear();

  RefreshKeyboards();
  RefreshMice();
  RefreshPointingSticks();
  RefreshTouchpads();
  RefreshTouchscreens();
  RefreshGraphicsTablets();
  RefreshUncategorized();
}

void InputDeviceDuplicateIdFinder::RefreshKeyboards() {
  UpdateList(ui::DeviceDataManager::GetInstance()->GetKeyboardDevices(),
             duplicate_ids_map_, vid_pid_map_);
}

void InputDeviceDuplicateIdFinder::RefreshMice() {
  UpdateList(ui::DeviceDataManager::GetInstance()->GetMouseDevices(),
             duplicate_ids_map_, vid_pid_map_);
}

void InputDeviceDuplicateIdFinder::RefreshPointingSticks() {
  UpdateList(ui::DeviceDataManager::GetInstance()->GetPointingStickDevices(),
             duplicate_ids_map_, vid_pid_map_);
}

void InputDeviceDuplicateIdFinder::RefreshTouchpads() {
  UpdateList(ui::DeviceDataManager::GetInstance()->GetTouchpadDevices(),
             duplicate_ids_map_, vid_pid_map_);
}

void InputDeviceDuplicateIdFinder::RefreshTouchscreens() {
  UpdateList(ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices(),
             duplicate_ids_map_, vid_pid_map_);
}

void InputDeviceDuplicateIdFinder::RefreshGraphicsTablets() {
  UpdateList(ui::DeviceDataManager::GetInstance()->GetGraphicsTabletDevices(),
             duplicate_ids_map_, vid_pid_map_);
}

void InputDeviceDuplicateIdFinder::RefreshUncategorized() {
  UpdateList(ui::DeviceDataManager::GetInstance()->GetUncategorizedDevices(),
             duplicate_ids_map_, vid_pid_map_);
}

}  // namespace ash

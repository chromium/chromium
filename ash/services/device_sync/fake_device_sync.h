// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_H_
#define ASH_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_H_

#include <queue>

#include "ash/services/device_sync/device_sync_base.h"

namespace chromeos {

namespace device_sync {

// Test double DeviceSync implementation.
class FakeDeviceSync : public DeviceSyncBase {
 public:
  FakeDeviceSync();

  FakeDeviceSync(const FakeDeviceSync&) = delete;
  FakeDeviceSync& operator=(const FakeDeviceSync&) = delete;

  ~FakeDeviceSync() override;

  using DeviceSyncBase::NotifyOnEnrollmentFinished;
  using DeviceSyncBase::NotifyOnNewDevicesSynced;

  void set_force_enrollment_now_completed_success(
      bool force_enrollment_now_completed_success) {
    force_enrollment_now_completed_success_ =
        force_enrollment_now_completed_success;
  }

  void set_force_sync_now_completed_success(
      bool force_sync_now_completed_success) {
    force_sync_now_completed_success_ = force_sync_now_completed_success;
  }

  void InvokePendingGetLocalDeviceMetadataCallback(
      const absl::optional<multidevice::RemoteDevice>& local_device_metadata);
  void InvokePendingGetSyncedDevicesCallback(
      const absl::optional<std::vector<multidevice::RemoteDevice>>&
          remote_devices);
  void InvokePendingSetSoftwareFeatureStateCallback(
      ash::device_sync::mojom::NetworkRequestResult result_code);
  void InvokePendingSetFeatureStatusCallback(
      ash::device_sync::mojom::NetworkRequestResult result_code);
  void InvokePendingFindEligibleDevicesCallback(
      ash::device_sync::mojom::NetworkRequestResult result_code,
      ash::device_sync::mojom::FindEligibleDevicesResponsePtr
          find_eligible_devices_response_ptr);
  void InvokePendingNotifyDevicesCallback(
      ash::device_sync::mojom::NetworkRequestResult result_code);
  void InvokePendingGetDevicesActivityStatusCallback(
      ash::device_sync::mojom::NetworkRequestResult result_code,
      absl::optional<
          std::vector<ash::device_sync::mojom::DeviceActivityStatusPtr>>
          get_devices_activity_status_response);
  void InvokePendingGetDebugInfoCallback(
      ash::device_sync::mojom::DebugInfoPtr debug_info_ptr);

 protected:
  // ash::device_sync::mojom::DeviceSync:
  void ForceEnrollmentNow(ForceEnrollmentNowCallback callback) override;
  void ForceSyncNow(ForceSyncNowCallback callback) override;
  void GetLocalDeviceMetadata(GetLocalDeviceMetadataCallback callback) override;
  void GetSyncedDevices(GetSyncedDevicesCallback callback) override;
  void SetSoftwareFeatureState(
      const std::string& device_public_key,
      multidevice::SoftwareFeature software_feature,
      bool enabled,
      bool is_exclusive,
      SetSoftwareFeatureStateCallback callback) override;
  void SetFeatureStatus(const std::string& device_instance_id,
                        multidevice::SoftwareFeature feature,
                        FeatureStatusChange status_change,
                        SetFeatureStatusCallback callback) override;
  void FindEligibleDevices(multidevice::SoftwareFeature software_feature,
                           FindEligibleDevicesCallback callback) override;
  void NotifyDevices(const std::vector<std::string>& device_instance_ids,
                     cryptauthv2::TargetService target_service,
                     multidevice::SoftwareFeature feature,
                     NotifyDevicesCallback callback) override;
  void GetDebugInfo(GetDebugInfoCallback callback) override;
  void GetDevicesActivityStatus(
      GetDevicesActivityStatusCallback callback) override;

 private:
  bool force_enrollment_now_completed_success_ = true;
  bool force_sync_now_completed_success_ = true;

  std::queue<GetLocalDeviceMetadataCallback>
      get_local_device_metadata_callback_queue_;
  std::queue<GetSyncedDevicesCallback> get_synced_devices_callback_queue_;
  std::queue<SetSoftwareFeatureStateCallback>
      set_software_feature_state_callback_queue_;
  std::queue<SetFeatureStatusCallback> set_feature_status_callback_queue_;
  std::queue<FindEligibleDevicesCallback> find_eligible_devices_callback_queue_;
  std::queue<NotifyDevicesCallback> notify_devices_callback_queue_;
  std::queue<GetDevicesActivityStatusCallback>
      get_devices_activity_status_callback_queue_;
  std::queue<GetDebugInfoCallback> get_debug_info_callback_queue_;
};

}  // namespace device_sync

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when it moved to ash.
namespace ash::device_sync {
using ::chromeos::device_sync::FakeDeviceSync;
}

#endif  // ASH_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_H_

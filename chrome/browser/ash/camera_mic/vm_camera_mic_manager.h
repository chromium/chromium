// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CAMERA_MIC_VM_CAMERA_MIC_MANAGER_H_
#define CHROME_BROWSER_ASH_CAMERA_MIC_VM_CAMERA_MIC_MANAGER_H_

#include <bitset>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/keyed_service/core/keyed_service.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace ash {

// This class manages camera/mic access (and the access notifications) for VMs
// (crostini and parallels for now). All of the notifications are sent to the
// primary profile since all VMs support only the primary profile. We might need
// to change this if we extend this class to support the browser, in which case
// we will also need to make the notification ids different for different
// profiles.
class VmCameraMicManager : public media::CameraActiveClientObserver,
                           public media::CameraPrivacySwitchObserver,
                           public CrasAudioHandler::AudioObserver {
 public:
  enum class VmType { kCrostiniVm, kPluginVm, kBorealis };

  enum class DeviceType {
    kMic,
    kCamera,
    kMaxValue = kCamera,
  };

  using NotificationType =
      std::bitset<static_cast<size_t>(DeviceType::kMaxValue) + 1>;
  static constexpr NotificationType kMicNotification{
      1 << static_cast<size_t>(DeviceType::kMic)};
  static constexpr NotificationType kCameraNotification{
      1 << static_cast<size_t>(DeviceType::kCamera)};
  static constexpr NotificationType kCameraAndMicNotification{
      (1 << static_cast<size_t>(DeviceType::kMic)) |
      (1 << static_cast<size_t>(DeviceType::kCamera))};

  static constexpr base::TimeDelta kDebounceTime = base::Milliseconds(300);

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnVmCameraMicActiveChanged(VmCameraMicManager*) {}
  };

  static VmCameraMicManager* Get();

  VmCameraMicManager();
  ~VmCameraMicManager() override;

  void OnPrimaryUserSessionStarted(Profile* primary_profile);

  VmCameraMicManager(const VmCameraMicManager&) = delete;
  VmCameraMicManager& operator=(const VmCameraMicManager&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Return true if any of the VMs is using the device. Note that if the camera
  // privacy switch is on, this always returns false for `kCamera`.
  bool IsDeviceActive(DeviceType device) const;
  // Return true if the selected VM is using the device. Note that if the camera
  // privacy switch is on, this always returns false for `kCamera`.
  bool IsDeviceActive(VmType vm, DeviceType device) const;
  // Return true if any of the VMs is displaying the `notification`.
  bool IsNotificationActive(NotificationType notification) const;

 private:
  friend class VmCameraMicManagerTest;

  static constexpr NotificationType kNoNotification{};

  class VmInfo;

  void MaybeSubscribeToCameraService(bool should_use_cros_camera_service);

  // media::CameraActiveClientObserver
  void OnActiveClientChange(
      cros::mojom::CameraClientType type,
      bool is_new_active_client,
      const base::flat_set<std::string>& active_device_ids) override;

  // media::CameraPrivacySwitchObserver
  void OnCameraHWPrivacySwitchStateChanged(
      const std::string& device_id,
      cros::mojom::CameraPrivacySwitchState state) override;

  // CrasAudioHandler::AudioObserver
  void OnNumberOfInputStreamsWithPermissionChanged() override;

  void SetCameraAccessing(VmType vm, bool accessing);
  void SetCameraPrivacyIsOn(bool is_on);
  void SetMicActive(VmType vm, bool active);

  static std::string GetNotificationId(VmType vm, NotificationType type);

  void UpdateVmInfo(VmType vm, void (VmInfo::*updator)(bool), bool value);
  void NotifyActiveChanged();

  raw_ptr<Profile, LeakedDanglingUntriaged> primary_profile_ = nullptr;
  std::map<VmType, VmInfo> vm_info_map_;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CAMERA_MIC_VM_CAMERA_MIC_MANAGER_H_

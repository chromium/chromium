// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CAMERA_MIC_VM_CAMERA_MIC_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_CAMERA_MIC_VM_CAMERA_MIC_MANAGER_H_

#include <bitset>
#include <memory>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "components/keyed_service/core/keyed_service.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace chromeos {

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
  enum class VmType { kCrostiniVm, kPluginVm };

  enum class DeviceType {
    kMic,
    kCamera,
    kMaxValue = kCamera,
  };

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

  // When a VM is using both camera and mic, we only show a single "camera and
  // mic" notification, which is considered a camera notification but not a mic
  // notification because it uses the camera icon. So, if only "camera only" or
  // "camera and mic" notifications are shown, this function returns true for
  // `kCamera` but false for `kMic`. If a "mic only" notification is shown, this
  // function returns true for `kMic`.
  bool IsNotificationActive(DeviceType device) const;
 private:
  friend class VmCameraMicManagerTest;

  using NotificationType =
      std::bitset<static_cast<size_t>(DeviceType::kMaxValue) + 1>;
  static constexpr NotificationType kNoNotification{};
  static constexpr NotificationType kMicNotification{
      1 << static_cast<size_t>(DeviceType::kMic)};
  static constexpr NotificationType kCameraNotification{
      1 << static_cast<size_t>(DeviceType::kCamera)};
  static constexpr NotificationType kCameraWithMicNotification{
      (1 << static_cast<size_t>(DeviceType::kMic)) |
      (1 << static_cast<size_t>(DeviceType::kCamera))};

  class VmInfo {
   public:
    VmInfo();
    VmInfo(const VmInfo&);
    ~VmInfo();

    NotificationType notification_type() const { return notification_type_; }

    void SetMicActive(bool active);
    void SetCameraAccessing(bool accessing);
    void SetCameraPrivacyIsOn(bool on);

   private:
    void OnCameraUpdated();

    bool camera_accessing_ = false;
    // We don't actually need to store this separately for each VM, but this
    // makes code simpler.
    bool camera_privacy_is_on_ = false;

    NotificationType notification_type_;
  };

  class VmNotificationObserver : public message_center::NotificationObserver {
   public:
    using OpenSettingsFunction = base::RepeatingCallback<void(Profile*)>;

    VmNotificationObserver();
    ~VmNotificationObserver();

    void Initialize(Profile* profile, OpenSettingsFunction open_settings);

    base::WeakPtr<NotificationObserver> GetWeakPtr();

    // message_center::NotificationObserver:
    void Click(const base::Optional<int>& button_index,
               const base::Optional<base::string16>& reply) override;

   private:
    Profile* profile_ = nullptr;
    OpenSettingsFunction open_settings_;
    base::WeakPtrFactory<VmNotificationObserver> weak_ptr_factory_{this};
  };

  void MaybeSubscribeToCameraService(bool should_use_cros_camera_service);

  // media::CameraActiveClientObserver
  void OnActiveClientChange(cros::mojom::CameraClientType type,
                            bool is_active) override;

  // media::CameraPrivacySwitchObserver
  void OnCameraPrivacySwitchStatusChanged(
      cros::mojom::CameraPrivacySwitchState state) override;

  // CrasAudioHandler::AudioObserver
  void OnNumberOfInputStreamsWithPermissionChanged() override;

  static std::string GetNotificationId(VmType vm, NotificationType type);

  void UpdateVmInfoAndNotifications(VmType vm,
                                    void (VmInfo::*updator)(bool),
                                    bool value);
  void NotifyActiveChanged();

  void OpenNotification(VmType vm, NotificationType type);
  void CloseNotification(VmType vm, NotificationType type);

  Profile* primary_profile_ = nullptr;
  VmNotificationObserver crostini_vm_notification_observer_;
  VmNotificationObserver plugin_vm_notification_observer_;
  base::flat_map<VmType, VmInfo> vm_info_map_;

  base::RetainingOneShotTimer observer_timer_;
  base::ObserverList<Observer> observers_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CAMERA_MIC_VM_CAMERA_MIC_MANAGER_H_

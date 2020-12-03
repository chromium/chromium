// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CAMERA_MIC_VM_CAMERA_MIC_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_CAMERA_MIC_VM_CAMERA_MIC_MANAGER_H_

#include <memory>
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace chromeos {

// This class manages camera/mic access (and the access notifications) for VMs
// (crostini and parallels for now). Like all the VMs, it is only available for
// the primary and non-incognito profile. We might need to change this if we
// extend this class to support the browser, in which case we will also need to
// make the notification ids different for different profiles.
class VmCameraMicManager : public KeyedService {
 public:
  enum class VmType { kCrostiniVm, kPluginVm };

  enum class DeviceType {
    kMic,
    kCamera,
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnVmCameraMicActiveChanged(VmCameraMicManager*) {}
  };

  // Return nullptr if the profile is non-primary or incognito.
  static VmCameraMicManager* GetForProfile(Profile* profile);

  explicit VmCameraMicManager(Profile* profile);
  ~VmCameraMicManager() override;

  VmCameraMicManager(const VmCameraMicManager&) = delete;
  VmCameraMicManager& operator=(const VmCameraMicManager&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetActive(VmType vm, DeviceType device, bool active);
  bool GetActive(VmType vm, DeviceType device) const;
  // Return true if any of the VMs is using the device.
  bool GetDeviceActive(DeviceType device) const;

 private:
  using ActiveMap = base::flat_map<std::pair<VmType, DeviceType>, bool>;

  class VmNotificationObserver : public message_center::NotificationObserver {
   public:
    using OpenSettingsFunction = base::RepeatingCallback<void(Profile*)>;

    VmNotificationObserver(Profile* profile,
                           OpenSettingsFunction open_settings);
    ~VmNotificationObserver();

    base::WeakPtr<NotificationObserver> GetWeakPtr();

    // message_center::NotificationObserver:
    void Click(const base::Optional<int>& button_index,
               const base::Optional<base::string16>& reply) override;

   private:
    Profile* const profile_;
    OpenSettingsFunction open_settings_;
    base::WeakPtrFactory<VmNotificationObserver> weak_ptr_factory_{this};
  };

  void NotifyActiveChanged();

  void OpenNotification(VmType vm, DeviceType device);
  void CloseNotification(VmType vm, DeviceType device);

  Profile* const profile_;
  VmNotificationObserver crostini_vm_notification_observer_;
  VmNotificationObserver plugin_vm_notification_observer_;
  ActiveMap active_map_;

  base::RetainingOneShotTimer observer_timer_;
  base::ObserverList<Observer> observers_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CAMERA_MIC_VM_CAMERA_MIC_MANAGER_H_

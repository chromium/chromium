// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_PHONEHUB_MULTIDEVICE_FEATURE_ACCESS_MANAGER_H_
#define ASH_COMPONENTS_PHONEHUB_MULTIDEVICE_FEATURE_ACCESS_MANAGER_H_

#include <ostream>

#include "ash/components/phonehub/notification_access_setup_operation.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {
namespace phonehub {

// Tracks the status of whether the user has granted permissions for the
// following features to be enabled on the host device:
// 1. Notification
// 2. Camera roll
//
// While Phone Hub can be enabled via Chrome OS, access to
// notifications requires that the user grant access via Android settings. If a
// Phone Hub connection to the phone has never succeeded, we assume that access
// has not yet been granted. If there is no active Phone Hub connection, we
// assume that the last access value seen is the current value.
//
// Additionally, this class provides an API for requesting the notification
// access setup flow via AttemptNotificationSetup().
//
// In order for user to use camera roll feature, users need to explicit give
// consent for the feature to be enabled on the phone via the feature setup
// dialog.
class MultideviceFeatureAccessManager {
 public:
  // Status of a feature's access. Numerical values are stored in prefs and
  // should not be changed or reused.
  enum class AccessStatus {
    // Access has not been granted and is prohibited from being granted (e.g.,
    // if the phone is using a Work Profile when trying to use notification
    // fearture).
    kProhibited = 0,

    // Access has not been granted, but the user is free to grant access.
    kAvailableButNotGranted = 1,

    // Access has been granted by the user.
    kAccessGranted = 2
  };

  enum class AccessProhibitedReason {
    // Access is either not prohibited or is unset. Use as a safe default value.
    kUnknown = 0,
    // Access is prohibited because the phone is using a Work Profile and on
    // Android version <N.
    kWorkProfile = 1,
    // Access is prohibited because the phone is using a Work Profile, and the
    // policy managing the phone disables access.
    kDisabledByPhonePolicy = 2
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when notification access has changed; use
    // GetNotificationAccessStatus() for the new status.
    virtual void OnNotificationAccessChanged();

    // Called when camera roll access has changed; use
    // GetCameraRollAccessStatus() for the new status.
    virtual void OnCameraRollAccessChanged();
  };

  MultideviceFeatureAccessManager(MultideviceFeatureAccessManager&) = delete;
  MultideviceFeatureAccessManager& operator=(MultideviceFeatureAccessManager&) =
      delete;
  virtual ~MultideviceFeatureAccessManager();

  virtual AccessStatus GetNotificationAccessStatus() const = 0;

  virtual AccessStatus GetCameraRollAccessStatus() const = 0;

  virtual AccessStatus GetAppsAccessStatus() const = 0;

  // Returns the reason notification access status is prohibited. The return
  // result is valid if the current access status (from GetAccessStatus())
  // is AccessStatus::kProhibited. Otherwise, the result is undefined and should
  // not be used.
  virtual AccessProhibitedReason GetNotificationAccessProhibitedReason()
      const = 0;

  virtual bool HasMultideviceFeatureSetupUiBeenDismissed() const = 0;

  // Disables the ability to show the banner within the PhoneHub UI.
  virtual void DismissSetupRequiredUi() = 0;

  // Starts an attempt to enable the notification access. |delegate| will be
  // updated with the status of the flow as long as the operation object
  // returned by this function remains instantiated.
  //
  // To cancel an ongoing setup attempt, delete the operation. If a setup
  // attempt fails, clients can retry by calling AttemptNotificationSetup()
  // again to start a new attempt.
  //
  // If notification access has already been granted, this function returns null
  // since there is nothing to set up.
  std::unique_ptr<NotificationAccessSetupOperation> AttemptNotificationSetup(
      NotificationAccessSetupOperation::Delegate* delegate);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  MultideviceFeatureAccessManager();

  void NotifyNotificationAccessChanged();
  void NotifyCameraRollAccessChanged();
  void SetNotificationSetupOperationStatus(
      NotificationAccessSetupOperation::Status new_status);

  bool IsSetupOperationInProgress() const;

  virtual void OnSetupRequested() {}

 private:
  friend class MultideviceFeatureAccessManagerImplTest;
  friend class PhoneStatusProcessor;

  // Sets the internal AccessStatus but does not send a request for
  // a new status to the remote phone device.
  virtual void SetNotificationAccessStatusInternal(
      AccessStatus access_status,
      AccessProhibitedReason reason) = 0;
  // Sets the internal AccessStatus but does not send a request for
  // a new status to the remote phone device.
  virtual void SetCameraRollAccessStatusInternal(
      AccessStatus camera_roll_access_status) = 0;

  void OnSetupOperationDeleted(int operation_id);

  int next_operation_id_ = 0;
  base::flat_map<int, NotificationAccessSetupOperation*> id_to_operation_map_;
  base::ObserverList<Observer> observer_list_;
  base::WeakPtrFactory<MultideviceFeatureAccessManager> weak_ptr_factory_{this};
};

std::ostream& operator<<(std::ostream& stream,
                         MultideviceFeatureAccessManager::AccessStatus status);
std::ostream& operator<<(
    std::ostream& stream,
    MultideviceFeatureAccessManager::AccessProhibitedReason reason);
std::ostream& operator<<(
    std::ostream& stream,
    std::pair<MultideviceFeatureAccessManager::AccessStatus,
              MultideviceFeatureAccessManager::AccessProhibitedReason>
        status_reason);

}  // namespace phonehub
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos {
namespace phonehub {
using ::ash::phonehub::MultideviceFeatureAccessManager;
}
}  // namespace chromeos

#endif  // ASH_COMPONENTS_PHONEHUB_MULTIDEVICE_FEATURE_ACCESS_MANAGER_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_APPS_ACCESS_MANAGER_H_
#define ASH_WEBUI_ECHE_APP_UI_APPS_ACCESS_MANAGER_H_

#include <ostream>

#include "ash/webui/eche_app_ui/apps_access_setup_operation.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager.h"

namespace ash {
namespace eche_app {

// Histogram for tracking Phone Hub Apps setup requests.
constexpr char kEcheOnboardingHistogramName[] = "Eche.Onboarding.UserAction";

using AccessStatus =
    ash::phonehub::MultideviceFeatureAccessManager::AccessStatus;
// Tracks the status of whether the user has enabled apps access on
// their phones.
class AppsAccessManager {
 public:
  // Note: Numerical values should not be changed, they are persisted to logs
  // and should not be renumbered or re-used. See
  // tools/metrics/histograms/enums.xml.
  enum class OnboardingUserActionMetric {
    // Initial state.
    kUserActionUnknown = 0,

    // Onboarding is started by user action.
    kUserActionStartClicked = 1,

    // The permission is granted by user action.
    kUserActionPermissionGranted = 2,

    // Users explicitly decline the permission request.
    kUserActionPermissionRejected = 3,

    // The permission request time out after 20 seconds.
    kUserActionTimeout = 4,

    // The permission request is canceled from the remote device, e.g. device
    // screen off.
    kUserActionRemoteInterrupt = 5,

    // System exceptions thrown out.
    kSystemError = 6,

    // The permission request is canceled from the onboarding UI.
    kUserActionCanceled = 7,

    // The permission request is canceled when the device is disconnected.
    kFailedConnection = 8,

    // The permission request is delivered to phone's Exo package.
    kAckByExo = 9,

    kMaxValue = kAckByExo
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when apps access has changed; use OnAppsAccessChanged()
    // for the new status.
    virtual void OnAppsAccessChanged() = 0;
  };

  AppsAccessManager(const AppsAccessManager&) = delete;
  AppsAccessManager& operator=(const AppsAccessManager&) = delete;
  virtual ~AppsAccessManager();

  virtual AccessStatus GetAccessStatus() const = 0;
  virtual void NotifyAppsAccessCanceled() = 0;

  // Starts an attempt to enable the apps access.
  std::unique_ptr<AppsAccessSetupOperation> AttemptAppsAccessSetup(
      AppsAccessSetupOperation::Delegate* delegate);
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  void NotifyAppsAccessChanged();

 protected:
  AppsAccessManager();

  void SetAppsSetupOperationStatus(AppsAccessSetupOperation::Status new_status);

  bool IsSetupOperationInProgress() const;
  virtual void OnSetupRequested() = 0;

 private:
  friend class AppsAccessManagerImplTest;
  virtual void SetAccessStatusInternal(AccessStatus access_status) = 0;
  void OnSetupOperationDeleted(int operation_id);

  int next_operation_id_ = 0;
  base::flat_map<int, raw_ptr<AppsAccessSetupOperation, CtnExperimental>>
      id_to_operation_map_;
  base::ObserverList<Observer> observer_list_;
  base::WeakPtrFactory<AppsAccessManager> weak_ptr_factory_{this};
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_APPS_ACCESS_MANAGER_H_

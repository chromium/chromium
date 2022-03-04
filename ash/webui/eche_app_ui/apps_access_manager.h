// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_APPS_ACCESS_MANAGER_H_
#define ASH_WEBUI_ECHE_APP_UI_APPS_ACCESS_MANAGER_H_

#include <ostream>

#include "ash/components/phonehub/multidevice_feature_access_manager.h"
#include "ash/webui/eche_app_ui/apps_access_setup_operation.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {
namespace eche_app {

using AccessStatus =
    ash::phonehub::MultideviceFeatureAccessManager::AccessStatus;
// Tracks the status of whether the user has enabled apps access on
// their phones.
class AppsAccessManager {
 public:
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
  base::flat_map<int, AppsAccessSetupOperation*> id_to_operation_map_;
  base::ObserverList<Observer> observer_list_;
  base::WeakPtrFactory<AppsAccessManager> weak_ptr_factory_{this};
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_APPS_ACCESS_MANAGER_H_

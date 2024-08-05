// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_PACKAGE_SYNC_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_PACKAGE_SYNC_DATA_TYPE_CONTROLLER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/service/data_type_controller.h"

class Profile;

namespace syncer {
class DataTypeSyncBridge;
class SyncableService;
class SyncService;
}  // namespace syncer

// A DataTypeController for arc package sync datatypes, which enables or
// disables these types based on whether ArcAppInstance is ready and whether
// the OS sync feature is enabled.
class ArcPackageSyncDataTypeController
    : public syncer::DataTypeController,
      public ArcAppListPrefs::Observer,
      public arc::ArcSessionManagerObserver {
 public:
  // |dump_stack| is called when an unrecoverable error occurs.
  ArcPackageSyncDataTypeController(
      syncer::OnceDataTypeStoreFactory store_factory,
      base::WeakPtr<syncer::SyncableService> syncable_service,
      const base::RepeatingClosure& dump_stack,
      syncer::SyncService* sync_service,
      Profile* profile);

  ArcPackageSyncDataTypeController(const ArcPackageSyncDataTypeController&) =
      delete;
  ArcPackageSyncDataTypeController& operator=(
      const ArcPackageSyncDataTypeController&) = delete;

  ~ArcPackageSyncDataTypeController() override;

  // DataTypeController overrides.
  PreconditionState GetPreconditionState() const override;

  // ArcAppListPrefs::Observer overrides.
  void OnPackageListInitialRefreshed() override;

  // ArcSessionManagerObserver:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;
  void OnArcInitialStart() override;

 private:
  void OnOsSyncFeaturePrefChanged();

  std::unique_ptr<syncer::DataTypeSyncBridge> bridge_;
  const raw_ptr<syncer::SyncService> sync_service_;
  const raw_ptr<Profile> profile_;
  const raw_ptr<ArcAppListPrefs> arc_prefs_;
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_PACKAGE_SYNC_DATA_TYPE_CONTROLLER_H_

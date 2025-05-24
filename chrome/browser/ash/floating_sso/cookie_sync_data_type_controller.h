// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_SSO_COOKIE_SYNC_DATA_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_ASH_FLOATING_SSO_COOKIE_SYNC_DATA_TYPE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/data_type_controller.h"

namespace syncer {
class DataTypeControllerDelegate;
class SyncService;
}  // namespace syncer

namespace ash::floating_sso {

// A DataTypeController for the Cookie sync datatype. The controller manages the
// state of the datatype based on the FloatingSsoEnabled policy.
class CookieSyncDataTypeController : public syncer::DataTypeController {
 public:
  CookieSyncDataTypeController(
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_full_sync_mode,
      syncer::SyncService* sync_service,
      PrefService* prefs);

  CookieSyncDataTypeController(const CookieSyncDataTypeController&) = delete;
  CookieSyncDataTypeController& operator=(const CookieSyncDataTypeController&) =
      delete;

  ~CookieSyncDataTypeController() override;

  // syncer::DataTypeController:
  PreconditionState GetPreconditionState() const override;

 private:
  void OnFloatingSsoPrefChanged();

  const raw_ptr<syncer::SyncService> sync_service_;
  const raw_ptr<PrefService> prefs_;

  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace ash::floating_sso

#endif  // CHROME_BROWSER_ASH_FLOATING_SSO_COOKIE_SYNC_DATA_TYPE_CONTROLLER_H_

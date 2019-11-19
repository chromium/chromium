// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINTERS_MODEL_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINTERS_MODEL_TYPE_CONTROLLER_H_

#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/model_type_controller.h"

class PrefService;

namespace syncer {
class ModelTypeControllerDelegate;
class SyncService;
}  // namespace syncer

// Controls syncing of ModelType PRINTERS.
class PrintersModelTypeController : public syncer::ModelTypeController {
 public:
  PrintersModelTypeController(
      std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate,
      PrefService* pref_service,
      syncer::SyncService* sync_service);
  ~PrintersModelTypeController() override;

  PrintersModelTypeController(const PrintersModelTypeController&) = delete;
  PrintersModelTypeController& operator=(const PrintersModelTypeController&) =
      delete;

  // DataTypeController:
  PreconditionState GetPreconditionState() const override;

 private:
  // Callback for changes to the OS sync feature enabled pref.
  void OnUserPrefChanged();

  PrefService* const pref_service_;
  syncer::SyncService* const sync_service_;

  PrefChangeRegistrar pref_registrar_;
};

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINTERS_MODEL_TYPE_CONTROLLER_H_

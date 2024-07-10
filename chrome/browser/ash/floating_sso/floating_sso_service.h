// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_SSO_FLOATING_SSO_SERVICE_H_
#define CHROME_BROWSER_ASH_FLOATING_SSO_FLOATING_SSO_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/floating_sso/floating_sso_sync_bridge.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/model/model_type_store.h"

namespace syncer {
class ModelTypeChangeProcessor;
class ModelTypeControllerDelegate;
}  // namespace syncer

class PrefService;

namespace ash::floating_sso {

class FloatingSsoService : public KeyedService {
 public:
  FloatingSsoService(
      PrefService* prefs,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      syncer::OnceModelTypeStoreFactory create_store_callback);
  FloatingSsoService(const FloatingSsoService&) = delete;
  FloatingSsoService& operator=(const FloatingSsoService&) = delete;

  ~FloatingSsoService() override;

  // KeyedService:
  void Shutdown() override;

  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetControllerDelegate();

  // TODO: b/346354327 - temporary flag used for testing. Remove after
  // actual behavior is implemented.
  bool is_enabled_for_testing_ = false;

 private:
  // Check if the feature is enabled based on the corresponding enterprise
  // policy. If yes, start observing cookies and uploading them to Sync, and
  // apply cookies from Sync if needed. If not, stop all of the above.
  void StartOrStop();

  raw_ptr<PrefService> prefs_ = nullptr;
  FloatingSsoSyncBridge bridge_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

}  // namespace ash::floating_sso

#endif  // CHROME_BROWSER_ASH_FLOATING_SSO_FLOATING_SSO_SERVICE_H_

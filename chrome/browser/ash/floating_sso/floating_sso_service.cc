// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_sso/floating_sso_service.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/floating_sso/floating_sso_sync_bridge.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"

namespace ash::floating_sso {

FloatingSsoService::FloatingSsoService(
    PrefService* prefs,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    syncer::OnceModelTypeStoreFactory create_store_callback)
    : prefs_(prefs),
      bridge_(std::move(change_processor), std::move(create_store_callback)),
      pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()) {
  pref_change_registrar_->Init(prefs_);
  pref_change_registrar_->Add(
      prefs::kFloatingSsoEnabled,
      base::BindRepeating(&FloatingSsoService::StartOrStop,
                          base::Unretained(this)));
  StartOrStop();
}

void FloatingSsoService::StartOrStop() {
  // TODO: b/346354255 - subscribe to cookie changes to commit them to Sync when
  // needed. Remove `is_enabled_for_testing_` after we can can observe
  // meaningful behavior in tests.
  is_enabled_for_testing_ =
      prefs_->FindPreference(prefs::kFloatingSsoEnabled)->GetValue()->GetBool();
}

FloatingSsoService::~FloatingSsoService() = default;

void FloatingSsoService::Shutdown() {
  pref_change_registrar_.reset();
  prefs_ = nullptr;
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
FloatingSsoService::GetControllerDelegate() {
  return bridge_.change_processor()->GetControllerDelegate();
}

}  // namespace ash::floating_sso

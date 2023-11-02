// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sync/os_syncable_service_model_type_controller.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/syncable_service_based_model_type_controller.h"
#include "components/sync/model/forwarding_model_type_controller_delegate.h"
#include "components/sync/model/model_type_controller_delegate.h"

OsSyncableServiceModelTypeController::OsSyncableServiceModelTypeController(
    syncer::ModelType type,
    syncer::OnceModelTypeStoreFactory store_factory,
    base::WeakPtr<syncer::SyncableService> syncable_service,
    const base::RepeatingClosure& dump_stack,
    PrefService* pref_service,
    syncer::SyncService* sync_service)
    : syncer::SyncableServiceBasedModelTypeController(
          type,
          std::move(store_factory),
          std::move(syncable_service),
          dump_stack,
          DelegateMode::kTransportModeWithSingleModel),
      pref_service_(pref_service),
      sync_service_(sync_service) {
  DCHECK(type == syncer::APP_LIST || type == syncer::OS_PREFERENCES ||
         type == syncer::OS_PRIORITY_PREFERENCES);
  DCHECK(pref_service_);
  DCHECK(sync_service_);
}

OsSyncableServiceModelTypeController::~OsSyncableServiceModelTypeController() =
    default;

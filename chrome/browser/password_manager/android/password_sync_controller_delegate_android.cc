// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_sync_controller_delegate_android.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"
#include "components/sync/model/type_entities_count.h"

namespace password_manager {

PasswordSyncControllerDelegateAndroid::PasswordSyncControllerDelegateAndroid() =
    default;

PasswordSyncControllerDelegateAndroid::
    ~PasswordSyncControllerDelegateAndroid() = default;

void PasswordSyncControllerDelegateAndroid::OnSyncStarting(
    const syncer::DataTypeActivationRequest& request,
    StartCallback callback) {
  // Sync started for passwords, either because the user just turned it on or
  // because of a browser startup.
  // TODO(crbug.com/1260837): Cache a boolean or similar enum in local storage
  // to distinguish browser startups from the case where the user just turned
  // sync on. This cached value will need cleanup in OnSyncStopping().
  NOTIMPLEMENTED();

  // Set |skip_engine_connection| to true to indicate that, actually, this sync
  // datatype doesn't depend on the built-in SyncEngine to communicate changes
  // to/from the Sync server. Instead, Android specific functionality is
  // leveraged to achieve similar behavior.
  auto activation_response =
      std::make_unique<syncer::DataTypeActivationResponse>();
  activation_response->skip_engine_connection = true;
  std::move(callback).Run(std::move(activation_response));
}

void PasswordSyncControllerDelegateAndroid::OnSyncStopping(
    syncer::SyncStopMetadataFate metadata_fate) {
  switch (metadata_fate) {
    case syncer::KEEP_METADATA:
      // Sync got temporarily paused. Just ignore.
      break;
    case syncer::CLEAR_METADATA:
      // The user (or something equivalent like an enterprise policy)
      // permanently disrabled sync, either fully or specifically for passwords.
      // This also includes more advanced cases like the user having cleared all
      // sync data in the dashboard (birthday reset) or, at least in theory, the
      // sync server reporting that all sync metadata is obsolete (i.e.
      // CLIENT_DATA_OBSOLETE in the sync protocol).
      // TODO(crbug.com/1260837): Notify |bridge_| that sync was permanently
      // disabled such that sync-ed data remains available in local storage. If
      // OnSyncStarting() caches any local state, it should probably be cleared
      // here.
      NOTIMPLEMENTED();
      break;
  }
}

void PasswordSyncControllerDelegateAndroid::GetAllNodesForDebugging(
    AllNodesCallback callback) {
  // This is not implemented because it's not worth the hassle just to display
  // debug information in chrome://sync-internals.
  std::move(callback).Run(syncer::PASSWORDS,
                          std::make_unique<base::ListValue>());
}

void PasswordSyncControllerDelegateAndroid::GetTypeEntitiesCountForDebugging(
    base::OnceCallback<void(const syncer::TypeEntitiesCount&)> callback) const {
  // This is not implemented because it's not worth the hassle just to display
  // debug information in chrome://sync-internals.
  std::move(callback).Run(syncer::TypeEntitiesCount(syncer::PASSWORDS));
}

void PasswordSyncControllerDelegateAndroid::
    RecordMemoryUsageAndCountsHistograms() {
  // This is not implemented because it's not worth the hassle. Password sync
  // module on Android doesn't hold any password. Instead passwords are
  // requested on demand from the GMS Core.
}

}  // namespace password_manager

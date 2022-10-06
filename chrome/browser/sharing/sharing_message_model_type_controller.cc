// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_message_model_type_controller.h"

#include <utility>

#include "components/sync/driver/sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"

SharingMessageModelTypeController::SharingMessageModelTypeController(
    syncer::SyncService* sync_service,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_transport_mode)
    : syncer::ModelTypeController(syncer::SHARING_MESSAGE,
                                  std::move(delegate_for_full_sync_mode),
                                  std::move(delegate_for_transport_mode)),
      sync_service_(sync_service) {
  sync_service_->AddObserver(this);
}

SharingMessageModelTypeController::~SharingMessageModelTypeController() {
  sync_service_->RemoveObserver(this);
}

syncer::DataTypeController::PreconditionState
SharingMessageModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  // TODO(crbug.com/1156584): No need to handle IsPersistentError() once the
  // feature toggle is cleaned up and sync gets paused for all persistent auth
  // errors. Instead, consider forcing DISABLE_SYNC_AND_CLEAR_DATA in Stop().
  return sync_service_->GetAuthError().IsPersistentError()
             ? PreconditionState::kMustStopAndClearData
             : PreconditionState::kPreconditionsMet;
}

void SharingMessageModelTypeController::OnStateChanged(
    syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  // Most of these calls will be no-ops but SyncService handles that just fine.
  sync_service_->DataTypePreconditionChanged(type());
}

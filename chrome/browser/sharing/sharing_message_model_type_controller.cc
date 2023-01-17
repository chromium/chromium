// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_message_model_type_controller.h"

#include <utility>

SharingMessageModelTypeController::SharingMessageModelTypeController(
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_transport_mode)
    : syncer::ModelTypeController(syncer::SHARING_MESSAGE,
                                  std::move(delegate_for_full_sync_mode),
                                  std::move(delegate_for_transport_mode)) {}

SharingMessageModelTypeController::~SharingMessageModelTypeController() =
    default;

void SharingMessageModelTypeController::Stop(
    syncer::ShutdownReason shutdown_reason,
    StopCallback callback) {
  DCHECK(CalledOnValidThread());
  switch (shutdown_reason) {
    case syncer::ShutdownReason::STOP_SYNC_AND_KEEP_DATA:
      // Clear sync metadata even when sync gets paused (e.g. persistent auth
      // error). This is needed because SharingMessageBridgeImpl uses the
      // processor's IsTrackingMetadata() bit to determine whether sharing
      // messages can be sent (they can't if sync is paused).
      shutdown_reason = syncer::ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA;
      break;
    case syncer::ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA:
    case syncer::ShutdownReason::BROWSER_SHUTDOWN_AND_KEEP_DATA:
      break;
  }
  ModelTypeController::Stop(shutdown_reason, std::move(callback));
}

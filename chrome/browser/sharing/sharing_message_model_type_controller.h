// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_MESSAGE_MODEL_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SHARING_SHARING_MESSAGE_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "components/sync/driver/model_type_controller.h"
#include "components/sync/driver/sync_service_observer.h"

namespace syncer {
class SyncService;
}  // namespace syncer

// Controls syncing of SHARING_MESSAGE.
class SharingMessageModelTypeController : public syncer::ModelTypeController,
                                          public syncer::SyncServiceObserver {
 public:
  // The |delegate_for_full_sync_mode|, |delegate_for_transport_mode| and
  // |sync_service| must not be null. Furthermore, |sync_service| must outlive
  // this object.
  SharingMessageModelTypeController(
      syncer::SyncService* sync_service,
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_transport_mode);
  ~SharingMessageModelTypeController() override;
  SharingMessageModelTypeController(const SharingMessageModelTypeController&) =
      delete;
  SharingMessageModelTypeController& operator=(
      const SharingMessageModelTypeController&) = delete;

  // DataTypeController overrides.
  PreconditionState GetPreconditionState() const override;

  // SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  syncer::SyncService* const sync_service_;
};

#endif  // CHROME_BROWSER_SHARING_SHARING_MESSAGE_MODEL_TYPE_CONTROLLER_H_

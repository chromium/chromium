// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SYNC_MODEL_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SYNC_MODEL_TYPE_CONTROLLER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "components/sync/driver/syncable_service_based_model_type_controller.h"

class Profile;

namespace browser_sync {
class BrowserSyncClient;
}  // namespace browser_sync

// A DataTypeController for supervised user sync datatypes, which enables or
// disables these types based on the profile's IsSupervised state.
class SupervisedUserSyncModelTypeController
    : public syncer::SyncableServiceBasedModelTypeController {
 public:
  // |sync_client| and |profile| must not be null and must outlive this object.
  SupervisedUserSyncModelTypeController(
      syncer::ModelType type,
      const Profile* profile,
      const base::RepeatingClosure& dump_stack,
      browser_sync::BrowserSyncClient* sync_client);
  ~SupervisedUserSyncModelTypeController() override;

  // DataTypeController override.
  PreconditionState GetPreconditionState() const override;

 private:
  const Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserSyncModelTypeController);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SYNC_MODEL_TYPE_CONTROLLER_H_

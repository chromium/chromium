// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DRIVE_INTEGRATION_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_DRIVE_INTEGRATION_SERVICE_ASH_H_

#include "chromeos/crosapi/mojom/drive_integration_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the crosapi interface for DriveIntegrationService. Lives in
// Ash-Chrome on the UI thread.
class DriveIntegrationServiceAsh : public mojom::DriveIntegrationService {
 public:
  DriveIntegrationServiceAsh();
  DriveIntegrationServiceAsh(const DriveIntegrationServiceAsh&) = delete;
  DriveIntegrationServiceAsh& operator=(const DriveIntegrationServiceAsh&) =
      delete;
  ~DriveIntegrationServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::DriveIntegrationService> receiver);

  // crosapi::mojom::DriveIntegrationService:
  void GetMountPointPath(GetMountPointPathCallback callback) override;

 private:
  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes.
  mojo::ReceiverSet<mojom::DriveIntegrationService> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DRIVE_INTEGRATION_SERVICE_ASH_H_

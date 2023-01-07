// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/sharing_mojo_service.h"

#include "chromeos/ash/services/nearby/public/mojom/sharing.mojom.h"
#include "content/public/browser/service_process_host.h"

namespace sharing {

mojo::PendingRemote<mojom::Sharing> LaunchSharing() {
  mojo::PendingRemote<mojom::Sharing> remote;
  content::ServiceProcessHost::Launch<mojom::Sharing>(
      remote.InitWithNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName("Sharing Service")
          .Pass());
  return remote;
}

}  // namespace sharing

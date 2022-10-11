// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_SHARING_MOJO_SERVICE_H_
#define CHROME_BROWSER_NEARBY_SHARING_SHARING_MOJO_SERVICE_H_

#include "chromeos/ash/services/nearby/public/mojom/sharing.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace sharing {

// Launches a new instance of the Sharing service in an isolated, sandboxed
// process, and returns a remote interface to control the service. The lifetime
// of the process is tied to that of the Remote. May be called from any thread.
mojo::PendingRemote<mojom::Sharing> LaunchSharing();

}  // namespace sharing

#endif  // CHROME_BROWSER_NEARBY_SHARING_SHARING_MOJO_SERVICE_H_

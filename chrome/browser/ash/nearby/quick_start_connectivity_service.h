// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NEARBY_QUICK_START_CONNECTIVITY_SERVICE_H_
#define CHROME_BROWSER_ASH_NEARBY_QUICK_START_CONNECTIVITY_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

class NearbyConnectionsManager;

namespace ash::quick_start {

// This service is tied to the signin profile and provides profile-scoped
// dependencies to Quick Start.
class QuickStartConnectivityService : public KeyedService {
 public:
  virtual raw_ptr<NearbyConnectionsManager> GetNearbyConnectionsManager() = 0;

  virtual mojo::SharedRemote<mojom::QuickStartDecoder>
  GetQuickStartDecoder() = 0;

  virtual void Cleanup() = 0;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_NEARBY_QUICK_START_CONNECTIVITY_SERVICE_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/fake_quick_start_connectivity_service.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager.h"
#include "chromeos/ash/components/quick_start/fake_quick_start_decoder.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace ash::quick_start {

FakeQuickStartConnectivityService::FakeQuickStartConnectivityService() =
    default;

FakeQuickStartConnectivityService::~FakeQuickStartConnectivityService() =
    default;

raw_ptr<NearbyConnectionsManager>
FakeQuickStartConnectivityService::GetNearbyConnectionsManager() {
  return &fake_nearby_connections_manager_;
}

mojo::SharedRemote<mojom::QuickStartDecoder>
FakeQuickStartConnectivityService::GetQuickStartDecoder() {
  return mojo::SharedRemote<mojom::QuickStartDecoder>(
      fake_quick_start_decoder_->GetRemote());
}

void FakeQuickStartConnectivityService::Cleanup() {
  is_cleanup_called_ = true;
}

raw_ptr<FakeNearbyConnectionsManager>
FakeQuickStartConnectivityService::GetFakeNearbyConnectionsManager() {
  return &fake_nearby_connections_manager_;
}

}  // namespace ash::quick_start

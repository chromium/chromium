// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/quick_start_connectivity_service_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager_impl.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_process_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace ash::quick_start {

namespace {

constexpr char kServiceId[] = "com.google.android.gms.smartdevice.NC_ID";

}  // namespace

QuickStartConnectivityServiceImpl::QuickStartConnectivityServiceImpl(
    nearby::NearbyProcessManager* nearby_process_manager)
    : nearby_process_manager_(nearby_process_manager) {}

QuickStartConnectivityServiceImpl::~QuickStartConnectivityServiceImpl() =
    default;

raw_ptr<NearbyConnectionsManager>
QuickStartConnectivityServiceImpl::GetNearbyConnectionsManager() {
  CHECK(nearby_process_manager_);

  if (!nearby_connections_manager_) {
    nearby_connections_manager_ =
        std::make_unique<NearbyConnectionsManagerImpl>(nearby_process_manager_,
                                                       kServiceId);
  }

  return nearby_connections_manager_.get();
}

mojo::SharedRemote<mojom::QuickStartDecoder>
QuickStartConnectivityServiceImpl::GetQuickStartDecoder() {
  CHECK(nearby_process_manager_);

  if (!nearby_process_reference_) {
    nearby_process_reference_ =
        nearby_process_manager_->GetNearbyProcessReference(base::BindOnce(
            &QuickStartConnectivityServiceImpl::OnNearbyProcessStopped,
            weak_ptr_factory_.GetWeakPtr()));
  }

  return nearby_process_reference_->GetQuickStartDecoder();
}

void QuickStartConnectivityServiceImpl::Cleanup() {
  nearby_process_reference_ = nullptr;
  nearby_process_manager_->ShutDownProcess();
  if (nearby_connections_manager_) {
    nearby_connections_manager_->Shutdown();
  }
}

void QuickStartConnectivityServiceImpl::OnNearbyProcessStopped(
    nearby::NearbyProcessManager::NearbyProcessShutdownReason shutdown_reason) {
  Cleanup();
}

}  // namespace ash::quick_start

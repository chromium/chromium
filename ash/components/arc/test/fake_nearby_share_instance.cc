// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_nearby_share_instance.h"

#include "ash/components/arc/mojom/nearby_share.mojom.h"
#include "base/functional/callback_helpers.h"

namespace arc {

FakeNearbyShareInstance::FakeNearbyShareInstance() = default;

FakeNearbyShareInstance::~FakeNearbyShareInstance() = default;

void FakeNearbyShareInstance::Init(
    mojo::PendingRemote<mojom::NearbyShareHost> host_remote,
    InitCallback callback) {
  ++num_init_called_;
  // For every change in a connection bind latest remote.
  host_remote_.reset();
  host_remote_.Bind(std::move(host_remote));
  std::move(callback).Run();
}

}  // namespace arc

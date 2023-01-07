// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_NEARBY_SHARE_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_NEARBY_SHARE_INSTANCE_H_

#include "ash/components/arc/mojom/nearby_share.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

class FakeNearbyShareInstance : public mojom::NearbyShareInstance {
 public:
  FakeNearbyShareInstance();
  FakeNearbyShareInstance(const FakeNearbyShareInstance&) = delete;
  FakeNearbyShareInstance& operator=(const FakeNearbyShareInstance&) = delete;
  ~FakeNearbyShareInstance() override;

  // mojom::NearbyShareInstance overrides:
  void Init(mojo::PendingRemote<mojom::NearbyShareHost> host_remote,
            InitCallback callback) override;

  size_t num_init_called() const { return num_init_called_; }

 private:
  // Keeps the binding alive so that calls to this class can be correctly
  // routed.
  mojo::Remote<mojom::NearbyShareHost> host_remote_;
  size_t num_init_called_ = 0;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_NEARBY_SHARE_INSTANCE_H_

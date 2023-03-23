// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_SYSTEM_STATE_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_SYSTEM_STATE_INSTANCE_H_

#include "ash/components/arc/mojom/system_state.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {
class FakeSystemStateInstance : public mojom::SystemStateInstance {
 public:
  FakeSystemStateInstance();
  FakeSystemStateInstance(const FakeSystemStateInstance&) = delete;
  FakeSystemStateInstance& operator=(const FakeSystemStateInstance&) = delete;
  ~FakeSystemStateInstance() override;

  // mojom::SystemStateInstance overrides:
  void Init(::mojo::PendingRemote<mojom::SystemStateHost> host_remote,
            InitCallback callback) override;

  size_t num_init_called() const { return num_init_called_; }

 private:
  mojo::Remote<mojom::SystemStateHost> host_remote_;
  size_t num_init_called_ = 0;
};
}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_SYSTEM_STATE_INSTANCE_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_system_state_instance.h"

namespace arc {
FakeSystemStateInstance::FakeSystemStateInstance() = default;
FakeSystemStateInstance::~FakeSystemStateInstance() = default;

void FakeSystemStateInstance::Init(
    ::mojo::PendingRemote<mojom::SystemStateHost> host_remote,
    InitCallback callback) {
  ++num_init_called_;
  host_remote_.reset();
  host_remote_.Bind(std::move(host_remote));
  std::move(callback).Run();
}

}  // namespace arc

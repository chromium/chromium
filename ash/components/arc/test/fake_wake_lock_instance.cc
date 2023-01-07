// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ash/components/arc/test/fake_wake_lock_instance.h"

namespace arc {

FakeWakeLockInstance::FakeWakeLockInstance() = default;

FakeWakeLockInstance::~FakeWakeLockInstance() = default;

void FakeWakeLockInstance::Init(
    mojo::PendingRemote<mojom::WakeLockHost> host_remote,
    InitCallback callback) {
  host_remote_.reset();
  host_remote_.Bind(std::move(host_remote));
  std::move(callback).Run();
}

}  // namespace arc

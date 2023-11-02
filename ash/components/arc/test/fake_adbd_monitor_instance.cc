// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_adbd_monitor_instance.h"

namespace arc {

FakeAdbdMonitorInstance::FakeAdbdMonitorInstance() = default;

FakeAdbdMonitorInstance::~FakeAdbdMonitorInstance() = default;

void FakeAdbdMonitorInstance::Init(
    mojo::PendingRemote<mojom::AdbdMonitorHost> host_remote,
    InitCallback callback) {
  host_remote_.reset();
  host_remote_.Bind(std::move(host_remote));
  std::move(callback).Run();
}

}  // namespace arc

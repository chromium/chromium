// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_disk_space_instance.h"

namespace arc {

FakeDiskSpaceInstance::FakeDiskSpaceInstance() = default;
FakeDiskSpaceInstance::~FakeDiskSpaceInstance() = default;

void FakeDiskSpaceInstance::Init(
    ::mojo::PendingRemote<mojom::DiskSpaceHost> host_remote,
    InitCallback callback) {
  host_remote_.reset();
  host_remote_.Bind(std::move(host_remote));
  std::move(callback).Run();
}

void FakeDiskSpaceInstance::GetApplicationsSize(
    GetApplicationsSizeCallback callback) {
  ++num_get_applications_size_called_;
  mojom::ApplicationsSizePtr size = mojom::ApplicationsSize::New();
  size->total_code_bytes = 42;
  size->total_data_bytes = 43;
  size->total_cache_bytes = 44;
  std::move(callback).Run(/*succeeded=*/true, std::move(size));
}

void FakeDiskSpaceInstance::ResizeStorageBalloon(int64_t free_space_bytes) {
  free_space_bytes_ = free_space_bytes;
}

}  // namespace arc

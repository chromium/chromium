// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_storage_manager_instance.h"

namespace arc {

FakeStorageManagerInstance::FakeStorageManagerInstance() = default;
FakeStorageManagerInstance::~FakeStorageManagerInstance() = default;

void FakeStorageManagerInstance::OpenPrivateVolumeSettings() {
  ++num_open_private_volume_settings_called_;
}

void FakeStorageManagerInstance::GetApplicationsSize(
    GetApplicationsSizeCallback callback) {
  ++num_get_applications_size_called_;
  mojom::ApplicationsSizePtr size = mojom::ApplicationsSize::New();
  size->total_code_bytes = 42;
  size->total_data_bytes = 43;
  size->total_cache_bytes = 44;
  std::move(callback).Run(/*succeeded=*/true, std::move(size));
}

}  // namespace arc

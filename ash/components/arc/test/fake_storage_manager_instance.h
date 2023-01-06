// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_STORAGE_MANAGER_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_STORAGE_MANAGER_INSTANCE_H_

#include "ash/components/arc/mojom/storage_manager.mojom.h"
#include "base/functional/callback_forward.h"

namespace arc {

class FakeStorageManagerInstance : public mojom::StorageManagerInstance {
 public:
  FakeStorageManagerInstance();
  FakeStorageManagerInstance(const FakeStorageManagerInstance&) = delete;
  FakeStorageManagerInstance& operator=(const FakeStorageManagerInstance&) =
      delete;
  ~FakeStorageManagerInstance() override;

  // mojom::StorageManagerInstance overrides:
  void OpenPrivateVolumeSettings() override;
  using GetApplicationsSizeCallback =
      base::OnceCallback<void(bool, mojom::ApplicationsSizePtr)>;
  void GetApplicationsSize(GetApplicationsSizeCallback callback) override;

  size_t num_open_private_volume_settings_called() const {
    return num_open_private_volume_settings_called_;
  }
  size_t num_get_applications_size_called() const {
    return num_get_applications_size_called_;
  }
  size_t num_delete_applications_cache_called() const {
    return num_delete_applications_cache_called_;
  }

 private:
  size_t num_open_private_volume_settings_called_ = 0;
  size_t num_get_applications_size_called_ = 0;
  size_t num_delete_applications_cache_called_ = 0;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_STORAGE_MANAGER_INSTANCE_H_

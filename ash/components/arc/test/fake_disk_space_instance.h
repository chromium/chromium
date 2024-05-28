// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_DISK_SPACE_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_DISK_SPACE_INSTANCE_H_

#include "ash/components/arc/mojom/disk_space.mojom.h"
#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

class FakeDiskSpaceInstance : public mojom::DiskSpaceInstance {
 public:
  FakeDiskSpaceInstance();
  FakeDiskSpaceInstance(const FakeDiskSpaceInstance&) = delete;
  FakeDiskSpaceInstance& operator=(const FakeDiskSpaceInstance&) = delete;
  ~FakeDiskSpaceInstance() override;

  // mojom::DiskSpaceInstance overrides:
  void Init(::mojo::PendingRemote<mojom::DiskSpaceHost> host_remote,
            InitCallback callback) override;
  void GetApplicationsSize(GetApplicationsSizeCallback callback) override;

  size_t num_get_applications_size_called() const {
    return num_get_applications_size_called_;
  }

 private:
  mojo::Remote<mojom::DiskSpaceHost> host_remote_;

  size_t num_get_applications_size_called_ = 0;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_DISK_SPACE_INSTANCE_H_

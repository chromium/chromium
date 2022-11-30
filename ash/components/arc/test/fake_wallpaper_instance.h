// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_WALLPAPER_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_WALLPAPER_INSTANCE_H_

#include <stdint.h>
#include <vector>

#include "ash/components/arc/mojom/wallpaper.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

class FakeWallpaperInstance : public mojom::WallpaperInstance {
 public:
  FakeWallpaperInstance();

  FakeWallpaperInstance(const FakeWallpaperInstance&) = delete;
  FakeWallpaperInstance& operator=(const FakeWallpaperInstance&) = delete;

  ~FakeWallpaperInstance() override;

  const std::vector<int32_t>& changed_ids() const { return changed_ids_; }

  // Overridden from mojom::WallpaperInstance
  void Init(mojo::PendingRemote<mojom::WallpaperHost> host_remote,
            InitCallback callback) override;
  void OnWallpaperChanged(int32_t walpaper_id) override;

 private:
  std::vector<int32_t> changed_ids_;

  // Keeps the binding alive so that calls to this class can be correctly
  // routed.
  mojo::Remote<mojom::WallpaperHost> host_remote_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_WALLPAPER_INSTANCE_H_

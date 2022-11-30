// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_privacy_items_instance.h"

#include <utility>

namespace arc {

FakePrivacyItemsInstance::FakePrivacyItemsInstance() = default;

FakePrivacyItemsInstance::~FakePrivacyItemsInstance() = default;

void FakePrivacyItemsInstance::Init(
    mojo::PendingRemote<mojom::PrivacyItemsHost> host_remote,
    InitCallback callback) {
  host_remote_.reset();
  host_remote_.Bind(std::move(host_remote));
  std::move(callback).Run();
}

void FakePrivacyItemsInstance::OnStaticPrivacyIndicatorBoundsChanged(
    int32_t display_id,
    const std::vector<gfx::Rect>& bounds) {
  last_bounds_display_id_ = display_id;
  last_bounds_ = bounds;
}

}  // namespace arc

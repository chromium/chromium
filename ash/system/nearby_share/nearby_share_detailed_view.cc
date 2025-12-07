// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/nearby_share/nearby_share_detailed_view.h"

#include "ash/system/nearby_share/nearby_share_detailed_view_impl.h"

namespace ash {

std::unique_ptr<NearbyShareDetailedView>
NearbyShareDetailedView::Factory::Create(
    DetailedViewDelegate* detailed_view_delegate) {
  return std::make_unique<NearbyShareDetailedViewImpl>(detailed_view_delegate);
}

}  // namespace ash

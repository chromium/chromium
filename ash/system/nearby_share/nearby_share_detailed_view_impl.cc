// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/nearby_share/nearby_share_detailed_view_impl.h"

#include "ash/strings/grit/ash_strings.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

NearbyShareDetailedViewImpl::NearbyShareDetailedViewImpl(
    DetailedViewDelegate* detailed_view_delegate)
    : TrayDetailedView(detailed_view_delegate) {
  // TODO(brandosocarras, b/360150790): Create and use a Quick Share string.
  CreateTitleRow(IDS_ASH_STATUS_TRAY_NEARBY_SHARE_TILE_LABEL);
}

NearbyShareDetailedViewImpl::~NearbyShareDetailedViewImpl() = default;

views::View* NearbyShareDetailedViewImpl::GetAsView() {
  return this;
}

BEGIN_METADATA(NearbyShareDetailedViewImpl)
END_METADATA

}  // namespace ash

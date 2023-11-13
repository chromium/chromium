// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_group_container_view.h"

#include "ash/wm/overview/overview_group_item.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

OverviewGroupContainerView::OverviewGroupContainerView(
    OverviewGroupItem* overview_group_item)
    : overview_group_item_(overview_group_item) {
}

OverviewGroupContainerView::~OverviewGroupContainerView() = default;

BEGIN_METADATA(OverviewGroupContainerView, views::View)
END_METADATA

}  // namespace ash

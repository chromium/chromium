// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_GROUP_CONTAINER_VIEW_H_
#define ASH_WM_OVERVIEW_OVERVIEW_GROUP_CONTAINER_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class OverviewGroupItem;

// A view that contains individual overview item widgets that constitute the
// group item view. This class will be handling events to be passed on to
// `OverviewGroupItem`.
class OverviewGroupContainerView : public views::View {
 public:
  METADATA_HEADER(OverviewGroupContainerView);

  explicit OverviewGroupContainerView(OverviewGroupItem* overview_group_item);
  OverviewGroupContainerView(const OverviewGroupContainerView&) = delete;
  OverviewGroupContainerView& operator=(const OverviewGroupContainerView&) =
      delete;
  ~OverviewGroupContainerView() override;

 private:
  const raw_ptr<OverviewGroupItem> overview_group_item_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_GROUP_CONTAINER_VIEW_H_

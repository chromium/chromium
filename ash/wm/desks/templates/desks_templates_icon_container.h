// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ICON_CONTAINER_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ICON_CONTAINER_H_

#include <string>
#include <utility>
#include <vector>

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

class DeskTemplate;
class DesksTemplatesIconView;

// This class for determines which app icons/favicons to show for a desk
// template and creates the according DesksTemplatesIconView's for them.
class DesksTemplatesIconContainer : public views::BoxLayoutView {
 public:
  METADATA_HEADER(DesksTemplatesIconContainer);

  DesksTemplatesIconContainer();
  DesksTemplatesIconContainer(const DesksTemplatesIconContainer&) = delete;
  DesksTemplatesIconContainer& operator=(const DesksTemplatesIconContainer&) =
      delete;
  ~DesksTemplatesIconContainer() override;

  // The maximum number of icons that can be displayed.
  static constexpr int kMaxIcons = 4;

  // Given a desk template, determine which icons to show in this and create
  // the according DesksTemplatesIconView's.
  void PopulateIconContainerFromTemplate(DeskTemplate* desk_template);

  // views::BoxLayoutView:
  void Layout() override;

 private:
  friend class DesksTemplatesItemViewTestApi;

  // Given a vector of pairs, where the first entry is an icon's identifier and
  // the second entry is its count, create views for them.
  void SetIcons(
      const std::vector<std::pair<std::string, int>>& identifiers_and_counts);

  // A vector of the `DesksTemplatesIconView`s stored in this. They
  // are owned by the views hierarchy but store pointers to them here as well.
  // The last element of `icon_views_` is always an `DesksTemplatesIconView`
  // used for storing the overflow count of icons. Not every View in this
  // vector is visible.
  std::vector<DesksTemplatesIconView*> icon_views_;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   DesksTemplatesIconContainer,
                   views::BoxLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::DesksTemplatesIconContainer)

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_ICON_CONTAINER_H_

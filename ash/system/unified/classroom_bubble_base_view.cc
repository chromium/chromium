// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/classroom_bubble_base_view.h"

#include <memory>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr int kSpacingAboveListContainerView = 16;

}  // namespace

ClassroomBubbleBaseView::ClassroomBubbleBaseView(
    DetailedViewDelegate* delegate,
    std::unique_ptr<ui::ComboboxModel> combobox_model)
    : GlanceableTrayChildBubble(delegate) {
  header_view_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  header_view_->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  header_view_->SetOrientation(views::LayoutOrientation::kHorizontal);
  header_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred));
  header_view_->SetProperty(views::kMarginsKey, gfx::Insets(16));

  auto* const header_icon =
      header_view_->AddChildView(std::make_unique<views::ImageView>());
  header_icon->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysBaseElevated, 16));
  header_icon->SetImage(ui::ImageModel::FromVectorIcon(
      kGlanceablesClassroomIcon, cros_tokens::kCrosSysOnSurface, 20));
  header_icon->SetPreferredSize(gfx::Size(32, 32));
  header_icon->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, 4));

  combo_box_view_ = header_view_->AddChildView(
      std::make_unique<views::Combobox>(std::move(combobox_model)));
  combo_box_view_->SetID(kComboBoxViewId);
  combo_box_view_->SetSelectedIndex(0);
  // TODO(b:283370907): Implement accessibility behavior.
  combo_box_view_->SetTooltipTextAndAccessibleName(u"Assignment list selector");

  list_container_view_ =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  list_container_view_->SetID(kListContainerViewId);
  list_container_view_->SetOrientation(views::LayoutOrientation::kVertical);
  list_container_view_->SetPaintToLayer();
  list_container_view_->layer()->SetFillsBoundsOpaquely(false);
  list_container_view_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(16));
  list_container_view_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kSpacingAboveListContainerView, 0, 0, 0));
}

ClassroomBubbleBaseView::~ClassroomBubbleBaseView() = default;

// views::View:
void ClassroomBubbleBaseView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // TODO(b:283370907): Implement accessibility behavior.
  if (!GetVisible()) {
    return;
  }
  node_data->role = ax::mojom::Role::kListBox;
  node_data->SetName(u"Glanceables Bubble Classroom View Accessible Name");
}

BEGIN_METADATA(ClassroomBubbleBaseView, views::View)
END_METADATA

}  // namespace ash

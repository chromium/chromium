// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/classroom_bubble_base_view.h"

#include <memory>

#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_item_view.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/types/cxx23_to_underlying.h"
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
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr int kSpacingAboveListContainerView = 16;

constexpr int kInteriorGlanceableBubbleMargin = 16;

constexpr int kMaxAssignments = 3;

}  // namespace

ClassroomBubbleBaseView::ClassroomBubbleBaseView(
    DetailedViewDelegate* delegate,
    std::unique_ptr<ui::ComboboxModel> combobox_model)
    : GlanceableTrayChildBubble(delegate) {
  auto* layout_manager =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager
      ->SetInteriorMargin(gfx::Insets::TLBR(kInteriorGlanceableBubbleMargin,
                                            kInteriorGlanceableBubbleMargin, 0,
                                            kInteriorGlanceableBubbleMargin))
      .SetOrientation(views::LayoutOrientation::kVertical);

  header_view_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  header_view_->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  header_view_->SetOrientation(views::LayoutOrientation::kHorizontal);
  header_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred));

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
  combo_box_view_->SetID(
      base::to_underlying(GlanceablesViewId::kClassroomBubbleComboBox));
  combo_box_view_->SetSelectedIndex(0);
  // TODO(b:283370907): Implement accessibility behavior.
  combo_box_view_->SetTooltipTextAndAccessibleName(u"Assignment list selector");

  list_container_view_ = AddChildView(std::make_unique<views::View>());
  list_container_view_->SetID(
      base::to_underlying(GlanceablesViewId::kClassroomBubbleListContainer));
  list_container_view_->SetPaintToLayer();
  list_container_view_->layer()->SetFillsBoundsOpaquely(false);
  list_container_view_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(16));
  list_container_view_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kSpacingAboveListContainerView, 0, 0, 0));
  auto* layout =
      list_container_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_between_child_spacing(2);

  list_footer_view_ = AddChildView(
      std::make_unique<GlanceablesListFooterView>(base::BindRepeating(
          &ClassroomBubbleBaseView::OnSeeAllPressed, base::Unretained(this))));
  list_footer_view_->SetID(
      base::to_underlying(GlanceablesViewId::kClassroomBubbleListFooter));
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

void ClassroomBubbleBaseView::OnGetAssignments(
    std::vector<std::unique_ptr<GlanceablesClassroomAssignment>> assignments) {
  const size_t old_item_count = list_container_view_->children().size();
  list_container_view_->RemoveAllChildViews();

  for (const auto& assignment : assignments) {
    list_container_view_->AddChildView(
        std::make_unique<GlanceablesClassroomItemView>(
            assignment.get(),
            base::BindRepeating(&ClassroomBubbleBaseView::OpenUrl,
                                base::Unretained(this), assignment->link)));

    if (list_container_view_->children().size() >= kMaxAssignments) {
      break;
    }
  }
  list_footer_view_->UpdateItemsCount(list_container_view_->children().size(),
                                      assignments.size());
  if (list_container_view_->children().size() != old_item_count) {
    PreferredSizeChanged();
  }
}

void ClassroomBubbleBaseView::OpenUrl(const GURL& url) const {
  const auto* const client =
      Shell::Get()->glanceables_v2_controller()->GetClassroomClient();
  if (client) {
    client->OpenUrl(url);
  }
}

BEGIN_METADATA(ClassroomBubbleBaseView, views::View)
END_METADATA

}  // namespace ash

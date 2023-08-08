// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/classroom_bubble_base_view.h"

#include <algorithm>
#include <memory>

#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_item_view.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/common/glanceables_progress_bar_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr int kInteriorGlanceableBubbleMargin = 16;

constexpr size_t kMaxAssignments = 3;

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
  // TODO(b/294681832): Finalize, and then localize strings.
  combo_box_view_->SetTooltipTextAndAccessibleName(u"Classwork type");
  combo_box_view_->SetAccessibleDescription(u"");
  combobox_view_observation_.Observe(combo_box_view_);

  progress_bar_ = AddChildView(std::make_unique<GlanceablesProgressBarView>());
  progress_bar_->UpdateProgressBarVisibility(/*visible=*/false);

  list_container_view_ = AddChildView(std::make_unique<views::View>());
  list_container_view_->SetID(
      base::to_underlying(GlanceablesViewId::kClassroomBubbleListContainer));
  auto* layout =
      list_container_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_between_child_spacing(2);
  list_container_view_->SetAccessibleRole(ax::mojom::Role::kList);

  const auto* const typography_provider = TypographyProvider::Get();
  empty_list_label_ = AddChildView(
      views::Builder<views::Label>()
          .SetProperty(views::kMarginsKey, gfx::Insets::TLBR(24, 0, 32, 0))
          .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
          .SetFontList(typography_provider->ResolveTypographyToken(
              TypographyToken::kCrosButton2))
          .SetLineHeight(typography_provider->ResolveLineHeight(
              TypographyToken::kCrosButton2))
          .SetID(base::to_underlying(
              GlanceablesViewId::kClassroomBubbleEmptyListLabel))
          .Build());

  list_footer_view_ = AddChildView(
      std::make_unique<GlanceablesListFooterView>(base::BindRepeating(
          &ClassroomBubbleBaseView::OnSeeAllPressed, base::Unretained(this))));
  list_footer_view_->SetID(
      base::to_underlying(GlanceablesViewId::kClassroomBubbleListFooter));
}

ClassroomBubbleBaseView::~ClassroomBubbleBaseView() = default;

void ClassroomBubbleBaseView::OnViewFocused(views::View* view) {
  CHECK_EQ(view, combo_box_view_);

  AnnounceListStateOnComboBoxAccessibility();
}

void ClassroomBubbleBaseView::AboutToRequestAssignments() {
  progress_bar_->UpdateProgressBarVisibility(/*visible=*/true);
  combo_box_view_->SetAccessibleDescription(u"");
}

void ClassroomBubbleBaseView::OnGetAssignments(
    const std::u16string& list_name,
    bool success,
    std::vector<std::unique_ptr<GlanceablesClassroomAssignment>> assignments) {
  const gfx::Size old_preferred_size = GetPreferredSize();

  progress_bar_->UpdateProgressBarVisibility(/*visible=*/false);

  list_container_view_->RemoveAllChildViews();
  total_assignments_ = assignments.size();

  const size_t num_assignments = std::min(kMaxAssignments, assignments.size());
  for (size_t i = 0; i < num_assignments; ++i) {
    list_container_view_->AddChildView(
        std::make_unique<GlanceablesClassroomItemView>(
            assignments[i].get(),
            base::BindRepeating(&ClassroomBubbleBaseView::OpenUrl,
                                base::Unretained(this), assignments[i]->link),
            /*item_index=*/i, /*last_item_index=*/num_assignments - 1));
  }
  const size_t shown_assignments = list_container_view_->children().size();
  list_footer_view_->UpdateItemsCount(shown_assignments, total_assignments_);

  const bool is_list_empty = shown_assignments == 0;
  empty_list_label_->SetVisible(is_list_empty);
  list_footer_view_->SetVisible(!is_list_empty);

  // TODO(b/294681832): Finalize, and then localize strings.
  list_container_view_->SetAccessibleName(u"Classwork " + list_name);
  list_container_view_->SetAccessibleDescription(
      list_footer_view_->items_count_label());
  list_container_view_->NotifyAccessibilityEvent(
      ax::mojom::Event::kChildrenChanged,
      /*send_native_event=*/true);

  // The list is shown in response to the action on the assignment selector
  // combobox, notify the user of the list state id the combox is still focused.
  AnnounceListStateOnComboBoxAccessibility();

  if (old_preferred_size != GetPreferredSize()) {
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

void ClassroomBubbleBaseView::AnnounceListStateOnComboBoxAccessibility() {
  if (empty_list_label_->GetVisible()) {
    combo_box_view_->GetViewAccessibility().AnnounceText(
        empty_list_label_->GetText());
  } else if (list_footer_view_->items_count_label()->GetVisible()) {
    combo_box_view_->GetViewAccessibility().AnnounceText(
        list_footer_view_->items_count_label()->GetText());
  }
}

BEGIN_METADATA(ClassroomBubbleBaseView, views::View)
END_METADATA

}  // namespace ash

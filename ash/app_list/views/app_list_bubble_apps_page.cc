// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_apps_page.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/views/continue_section_view.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/controls/rounded_scroll_bar.h"
#include "ash/public/cpp/style/color_provider.h"
#include "base/check.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;

namespace ash {

namespace {

constexpr int kContinueColumnCount = 2;

// Insets for the vertical scroll bar.
constexpr gfx::Insets kVerticalScrollInsets(1, 0, 1, 1);

}  // namespace

AppListBubbleAppsPage::AppListBubbleAppsPage(
    AppListViewDelegate* view_delegate,
    ApplicationDragAndDropHost* drag_and_drop_host,
    AppListA11yAnnouncer* a11y_announcer,
    AppListFolderController* folder_controller) {
  DCHECK(view_delegate);
  DCHECK(drag_and_drop_host);
  DCHECK(a11y_announcer);
  DCHECK(folder_controller);

  SetUseDefaultFillLayout(true);

  // The entire page scrolls.
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll_view_->SetDrawOverflowIndicator(false);
  // Don't paint a background. The bubble already has one.
  scroll_view_->SetBackgroundColor(absl::nullopt);
  // Arrow keys are used to select app icons.
  scroll_view_->SetAllowKeyboardScrolling(false);

  // Set up scroll bars.
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  auto vertical_scroll =
      std::make_unique<RoundedScrollBar>(/*horizontal=*/false);
  vertical_scroll->SetInsets(kVerticalScrollInsets);
  scroll_view_->SetVerticalScrollBar(std::move(vertical_scroll));

  auto scroll_contents = std::make_unique<views::View>();
  auto* layout = scroll_contents->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStretch);

  // Continue section row.
  continue_section_ =
      scroll_contents->AddChildView(std::make_unique<ContinueSectionView>(
          view_delegate, kContinueColumnCount));

  // Recent apps row.
  recent_apps_ = scroll_contents->AddChildView(
      std::make_unique<RecentAppsView>(view_delegate));

  // Horizontal separator.
  auto* separator =
      scroll_contents->AddChildView(std::make_unique<views::Separator>());
  separator->SetColor(ColorProvider::Get()->GetContentLayerColor(
      ColorProvider::ContentLayerType::kSeparatorColor));

  // All apps section.
  scrollable_apps_grid_view_ =
      scroll_contents->AddChildView(std::make_unique<ScrollableAppsGridView>(
          a11y_announcer, view_delegate,
          /*folder_delegate=*/nullptr, scroll_view_, folder_controller));
  scrollable_apps_grid_view_->SetDragAndDropHostOfCurrentAppList(
      drag_and_drop_host);
  scrollable_apps_grid_view_->Init();
  AppListModel* model = view_delegate->GetModel();
  scrollable_apps_grid_view_->SetModel(model);
  scrollable_apps_grid_view_->SetItemList(model->top_level_item_list());
  scrollable_apps_grid_view_->ResetForShowApps();

  scroll_view_->SetContents(std::move(scroll_contents));
  continue_section_->UpdateSuggestionTasks();
}

AppListBubbleAppsPage::~AppListBubbleAppsPage() = default;

BEGIN_METADATA(AppListBubbleAppsPage, views::View)
END_METADATA

}  // namespace ash

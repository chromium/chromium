// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_apps_page.h"

#include <limits>
#include <string>
#include <utility>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/views/app_list_a11y_announcer.h"
#include "ash/app_list/views/continue_section_view.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/bubble/bubble_utils.h"
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
}  // namespace

AppListBubbleAppsPage::AppListBubbleAppsPage(
    AppListViewDelegate* view_delegate,
    ApplicationDragAndDropHost* drag_and_drop_host) {
  DCHECK(view_delegate);
  DCHECK(drag_and_drop_host);

  SetUseDefaultFillLayout(true);

  a11y_announcer_ = std::make_unique<AppListA11yAnnouncer>(
      AddChildView(std::make_unique<views::View>()));

  // The entire page scrolls.
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  // Don't paint a background. The bubble already has one.
  scroll_view_->SetBackgroundColor(absl::nullopt);

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
          a11y_announcer_.get(), view_delegate,
          /*folder_delegate=*/nullptr, scroll_view_));
  scrollable_apps_grid_view_->SetDragAndDropHostOfCurrentAppList(
      drag_and_drop_host);
  scrollable_apps_grid_view_->Init();
  AppListModel* model = view_delegate->GetModel();
  scrollable_apps_grid_view_->SetModel(model);
  scrollable_apps_grid_view_->SetItemList(model->top_level_item_list());
  scrollable_apps_grid_view_->ResetForShowApps();

  scroll_view_->SetContents(std::move(scroll_contents));
}

AppListBubbleAppsPage::~AppListBubbleAppsPage() {
  // `a11y_announcer_` depends on a child view, so shut it down before view
  // hierarchy is destroyed.
  a11y_announcer_->Shutdown();
}

BEGIN_METADATA(AppListBubbleAppsPage, views::View)
END_METADATA

}  // namespace ash

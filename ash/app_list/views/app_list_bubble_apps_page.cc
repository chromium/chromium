// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_apps_page.h"

#include <algorithm>
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
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;

namespace ash {

namespace {

constexpr int kContinueColumnCount = 2;

// Insets for the vertical scroll bar.
constexpr gfx::Insets kVerticalScrollInsets(1, 0, 1, 1);

// The padding between different sections within the apps page. Also used for
// interior apps page container margin.
constexpr int kVerticalPaddingBetweenSections = 16;

// The horizontal interior margin for the apps page container - i.e. the margin
// between the apps page bounds and the page content.
constexpr int kHorizontalInteriorMargin = 20;

// Insets for the separator between the continue section and apps.
constexpr gfx::Insets kSeparatorInsets(0, 12);

}  // namespace

AppListBubbleAppsPage::AppListBubbleAppsPage(
    AppListViewDelegate* view_delegate,
    ApplicationDragAndDropHost* drag_and_drop_host,
    AppListConfig* app_list_config,
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
  auto* layout = scroll_contents->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical,
      gfx::Insets(kVerticalPaddingBetweenSections, kHorizontalInteriorMargin),
      kVerticalPaddingBetweenSections));
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStretch);

  // Continue section row.
  continue_section_ =
      scroll_contents->AddChildView(std::make_unique<ContinueSectionView>(
          view_delegate, kContinueColumnCount));

  // Recent apps row.
  recent_apps_ = scroll_contents->AddChildView(
      std::make_unique<RecentAppsView>(this, view_delegate));
  recent_apps_->UpdateAppListConfig(app_list_config);
  recent_apps_->ShowResults(view_delegate->GetSearchModel(),
                            view_delegate->GetModel());

  // Horizontal separator.
  auto* separator =
      scroll_contents->AddChildView(std::make_unique<views::Separator>());
  separator->SetBorder(views::CreateEmptyBorder(kSeparatorInsets));
  separator->SetColor(ColorProvider::Get()->GetContentLayerColor(
      ColorProvider::ContentLayerType::kSeparatorColor));

  // All apps section.
  scrollable_apps_grid_view_ =
      scroll_contents->AddChildView(std::make_unique<ScrollableAppsGridView>(
          a11y_announcer, view_delegate,
          /*folder_delegate=*/nullptr, scroll_view_, folder_controller,
          /*focus_delegate=*/this));
  scrollable_apps_grid_view_->SetDragAndDropHostOfCurrentAppList(
      drag_and_drop_host);
  scrollable_apps_grid_view_->Init();
  scrollable_apps_grid_view_->UpdateAppListConfig(app_list_config);
  scrollable_apps_grid_view_->SetMaxColumns(5);
  AppListModel* model = view_delegate->GetModel();
  scrollable_apps_grid_view_->SetModel(model);
  scrollable_apps_grid_view_->SetItemList(model->top_level_item_list());
  scrollable_apps_grid_view_->ResetForShowApps();

  scroll_view_->SetContents(std::move(scroll_contents));
  continue_section_->UpdateSuggestionTasks();
}

AppListBubbleAppsPage::~AppListBubbleAppsPage() = default;

void AppListBubbleAppsPage::DisableFocusForShowingActiveFolder(bool disabled) {
  continue_section_->DisableFocusForShowingActiveFolder(disabled);
  recent_apps_->DisableFocusForShowingActiveFolder(disabled);
  scrollable_apps_grid_view_->DisableFocusForShowingActiveFolder(disabled);
}

void AppListBubbleAppsPage::MoveFocusUpFromRecents() {
  DCHECK_GT(recent_apps_->GetItemViewCount(), 0);
  AppListItemView* first_recent = recent_apps_->GetItemViewAt(0);
  // Find the view one step in reverse from the first recent app.
  views::View* previous_view = GetFocusManager()->GetNextFocusableView(
      first_recent, GetWidget(), /*reverse=*/true, /*dont_loop=*/false);
  DCHECK(previous_view);
  previous_view->RequestFocus();
}

void AppListBubbleAppsPage::MoveFocusDownFromRecents(int column) {
  int top_level_item_count =
      scrollable_apps_grid_view_->view_model()->view_size();
  if (top_level_item_count <= 0)
    return;
  // Attempt to focus the item at `column` in the first row, or the last item if
  // there aren't enough items. This could happen if the user's apps are in a
  // small number of folders.
  int index = std::min(column, top_level_item_count - 1);
  AppListItemView* item = scrollable_apps_grid_view_->GetItemViewAt(index);
  DCHECK(item);
  item->RequestFocus();
}

bool AppListBubbleAppsPage::MoveFocusUpFromAppsGrid(int column) {
  DVLOG(1) << __FUNCTION__;
  const int recent_app_count = recent_apps_->GetItemViewCount();
  // If there aren't any recent apps, don't change focus here. Fall back to the
  // app grid's default behavior.
  if (!recent_apps_->GetVisible() || recent_app_count <= 0)
    return false;
  // Attempt to focus the item at `column`, or the last item if there aren't
  // enough items.
  int index = std::min(column, recent_app_count - 1);
  AppListItemView* item = recent_apps_->GetItemViewAt(index);
  DCHECK(item);
  item->RequestFocus();
  return true;
}

BEGIN_METADATA(AppListBubbleAppsPage, views::View)
END_METADATA

}  // namespace ash

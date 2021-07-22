// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_search_page.h"

#include <limits>
#include <memory>
#include <utility>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/result_selection_controller.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_view.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/notreached.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;

namespace ash {

AppListBubbleSearchPage::AppListBubbleSearchPage(
    AppListViewDelegate* view_delegate,
    SearchBoxView* search_box_view)
    : search_box_view_(search_box_view) {
  DCHECK(view_delegate);
  DCHECK(search_box_view_);
  SetUseDefaultFillLayout(true);

  // The entire page scrolls.
  auto* scroll = AddChildView(std::make_unique<views::ScrollView>());
  scroll->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll->SetDrawOverflowIndicator(false);
  scroll->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  // Don't paint a background. The bubble already has one.
  scroll->SetBackgroundColor(absl::nullopt);

  auto scroll_contents = std::make_unique<views::View>();
  scroll_contents->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));

  result_selection_controller_ = std::make_unique<ResultSelectionController>(
      &result_container_views_,
      base::BindRepeating(&AppListBubbleSearchPage::OnSelectedResultChanged,
                          base::Unretained(this)));
  search_box_view_->SetResultSelectionController(
      result_selection_controller_.get());

  // TODO(https://crbug.com/1204551): Provide a custom search result list view,
  // instead of recycling this one from fullscreen launcher.
  auto* result_container =
      scroll_contents->AddChildView(std::make_unique<SearchResultListView>(
          /*main_view=*/nullptr, view_delegate));
  // SearchResultListView uses SearchResultView, which requires a light
  // background for the text to be readable.
  result_container->SetBackground(views::CreateSolidBackground(
      AppListColorProvider::Get()->GetSearchBoxBackgroundColor()));
  result_container->SetResults(view_delegate->GetSearchModel()->results());
  result_container->set_delegate(this);
  result_container_views_.push_back(result_container);

  scroll->SetContents(std::move(scroll_contents));
}

AppListBubbleSearchPage::~AppListBubbleSearchPage() = default;

void AppListBubbleSearchPage::OnSearchResultContainerResultsChanging() {
  // Block any result selection changes while result updates are in flight.
  // The selection will be reset once the results are all updated.
  result_selection_controller_->set_block_selection_changes(true);
}

void AppListBubbleSearchPage::OnSearchResultContainerResultsChanged() {
  // TODO(crbug.com/1204551): Accessibility notifications, similar to
  // SearchResultPageView.

  // Find the first result view.
  DCHECK(!result_container_views_.empty());
  SearchResultBaseView* first_result_view =
      result_container_views_.front()->GetFirstResultView();

  // Reset selection to first when things change. The first result is set as
  // as the default result.
  result_selection_controller_->set_block_selection_changes(false);
  result_selection_controller_->ResetSelection(/*key_event=*/nullptr,
                                               /*default_selection=*/true);
  // Update SearchBoxView search box autocomplete as necessary based on new
  // first result view.
  search_box_view_->ProcessAutocomplete(first_result_view);
}

void AppListBubbleSearchPage::OnSelectedResultChanged() {
  // TODO(crbug.com/1204551): Accessibility announcement, similar to
  // SearchResultPageView.
  NOTIMPLEMENTED_LOG_ONCE();
}

bool AppListBubbleSearchPage::CanSelectSearchResults() {
  DCHECK(!result_container_views_.empty());
  return result_container_views_.front()->num_results() > 0;
}

BEGIN_METADATA(AppListBubbleSearchPage, views::View)
END_METADATA

}  // namespace ash

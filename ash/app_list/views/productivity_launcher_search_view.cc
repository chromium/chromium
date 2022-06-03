// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/productivity_launcher_search_view.h"

#include <limits>
#include <memory>
#include <utility>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/result_selection_controller.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_view.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;

namespace ash {

namespace {

// The amount of time by which notifications to accessibility framework about
// result page changes are delayed.
constexpr base::TimeDelta kNotifyA11yDelay = base::Milliseconds(1500);

}  // namespace

ProductivityLauncherSearchView::ProductivityLauncherSearchView(
    AppListViewDelegate* view_delegate,
    SearchBoxView* search_box_view)
    : search_box_view_(search_box_view) {
  DCHECK(view_delegate);
  DCHECK(search_box_view_);
  SetUseDefaultFillLayout(true);

  // The entire page scrolls. Use layer scrolling so that the contents will
  // paint on top of the parent, which uses SetPaintToLayer().
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  // Don't paint a background. The bubble already has one.
  scroll_view_->SetBackgroundColor(absl::nullopt);

  auto scroll_contents = std::make_unique<views::View>();
  scroll_contents->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));

  result_selection_controller_ = std::make_unique<ResultSelectionController>(
      &result_container_views_,
      base::BindRepeating(
          &ProductivityLauncherSearchView::OnSelectedResultChanged,
          base::Unretained(this)));
  search_box_view_->SetResultSelectionController(
      result_selection_controller_.get());

  for (SearchResultListView::SearchResultListType list_type :
       SearchResultListView::GetAllListTypesForCategoricalSearch()) {
    auto* result_container =
        scroll_contents->AddChildView(std::make_unique<SearchResultListView>(
            /*main_view=*/nullptr, view_delegate));
    result_container->SetListType(list_type);
    result_container->SetResults(
        AppListModelProvider::Get()->search_model()->results());
    result_container->set_delegate(this);
    result_container_views_.push_back(result_container);
  }

  scroll_view_->SetContents(std::move(scroll_contents));

  AppListModelProvider::Get()->AddObserver(this);
}

ProductivityLauncherSearchView::~ProductivityLauncherSearchView() {
  AppListModelProvider::Get()->RemoveObserver(this);
}

void ProductivityLauncherSearchView::OnSearchResultContainerResultsChanging() {
  // Block any result selection changes while result updates are in flight.
  // The selection will be reset once the results are all updated.
  result_selection_controller_->set_block_selection_changes(true);

  notify_a11y_results_changed_timer_.Stop();
  SetIgnoreResultChangesForA11y(true);
}

void ProductivityLauncherSearchView::OnSearchResultContainerResultsChanged() {
  DCHECK(!result_container_views_.empty());

  int result_count = 0;
  // Only sort and layout the containers when they have all updated.
  for (SearchResultContainerView* view : result_container_views_) {
    if (view->UpdateScheduled())
      return;
    result_count += view->num_results();
  }

  last_search_result_count_ = result_count;

  ScheduleResultsChangedA11yNotification();
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

void ProductivityLauncherSearchView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  if (!GetVisible())
    return;

  node_data->role = ax::mojom::Role::kListBox;

  std::u16string value;
  std::u16string query =
      AppListModelProvider::Get()->search_model()->search_box()->text();
  if (!query.empty()) {
    if (last_search_result_count_ == 1) {
      value = l10n_util::GetStringFUTF16(
          IDS_APP_LIST_SEARCHBOX_RESULTS_ACCESSIBILITY_ANNOUNCEMENT_SINGLE_RESULT,
          query);
    } else {
      value = l10n_util::GetStringFUTF16(
          IDS_APP_LIST_SEARCHBOX_RESULTS_ACCESSIBILITY_ANNOUNCEMENT,
          base::NumberToString16(last_search_result_count_), query);
    }
  } else {
    // TODO(crbug.com/1204551): New(?) accessibility announcement. We used to
    // have a zero state A11Y announcement but zero state is removed for the
    // bubble launcher.
    value = std::u16string();
  }

  node_data->SetValue(value);
}

void ProductivityLauncherSearchView::OnActiveAppListModelsChanged(
    AppListModel* model,
    SearchModel* search_model) {
  for (auto* container : result_container_views_)
    container->SetResults(search_model->results());
}

void ProductivityLauncherSearchView::OnSelectedResultChanged() {
  if (!result_selection_controller_->selected_result()) {
    return;
  }

  views::View* selected_row = result_selection_controller_->selected_result();
  selected_row->ScrollViewToVisible();

  MaybeNotifySelectedResultChanged();
}

void ProductivityLauncherSearchView::SetIgnoreResultChangesForA11y(
    bool ignore) {
  if (ignore_result_changes_for_a11y_ == ignore)
    return;
  ignore_result_changes_for_a11y_ = ignore;
  SetViewIgnoredForAccessibility(this, ignore);
}

void ProductivityLauncherSearchView::ScheduleResultsChangedA11yNotification() {
  if (!ignore_result_changes_for_a11y_) {
    NotifyA11yResultsChanged();
    return;
  }

  notify_a11y_results_changed_timer_.Start(
      FROM_HERE, kNotifyA11yDelay,
      base::BindOnce(&ProductivityLauncherSearchView::NotifyA11yResultsChanged,
                     base::Unretained(this)));
}

void ProductivityLauncherSearchView::NotifyA11yResultsChanged() {
  SetIgnoreResultChangesForA11y(false);

  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
  MaybeNotifySelectedResultChanged();
}

void ProductivityLauncherSearchView::MaybeNotifySelectedResultChanged() {
  if (ignore_result_changes_for_a11y_)
    return;

  // Ignore result selection change if the focus moved away from the search box
  // textfield, for example to the close button.
  if (!search_box_view_->search_box()->HasFocus())
    return;

  if (!result_selection_controller_->selected_result())
    return;

  views::View* selected_view =
      result_selection_controller_->selected_result()->GetSelectedView();
  if (!selected_view)
    return;

  selected_view->NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  NotifyAccessibilityEvent(ax::mojom::Event::kSelectedChildrenChanged, true);
  search_box_view_->set_a11y_selection_on_search_result(true);
}

bool ProductivityLauncherSearchView::CanSelectSearchResults() {
  DCHECK(!result_container_views_.empty());
  return result_container_views_.front()->num_results() > 0;
}

BEGIN_METADATA(ProductivityLauncherSearchView, views::View)
END_METADATA

}  // namespace ash

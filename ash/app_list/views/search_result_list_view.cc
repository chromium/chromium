// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_list_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/dcheck_is_on.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kPreferredTitleHorizontalMargins = 16;
constexpr int kPreferredTitleTopMargins = 12;
constexpr int kPreferredTitleBottomMargins = 4;

constexpr base::TimeDelta kImpressionThreshold = base::Seconds(3);

// TODO(crbug.com/1199206): Move this into SharedAppListConfig once the UI for
// categories is more developed.
constexpr size_t kMaxResultsWithCategoricalSearch = 3;
constexpr int kAnswerCardMaxResults = 1;

SearchResultIdWithPositionIndices GetSearchResultsForLogging(
    std::vector<SearchResultView*> search_result_views) {
  SearchResultIdWithPositionIndices results;
  for (const auto* item : search_result_views) {
    if (item->result()) {
      results.emplace_back(SearchResultIdWithPositionIndex(
          item->result()->id(), item->index_in_container()));
    }
  }
  return results;
}

size_t GetMaxSearchResultListItems() {
  if (app_list_features::IsCategoricalSearchEnabled())
    return kMaxResultsWithCategoricalSearch;
  return SharedAppListConfig::instance().max_search_result_list_items();
}

// Maps 'AppListSearchResultCategory' to 'SearchResultListType'.
SearchResultListView::SearchResultListType CategoryToListType(
    ash::AppListSearchResultCategory category) {
  switch (category) {
    case ash::AppListSearchResultCategory::kApps:
      return SearchResultListView::SearchResultListType::kApps;
    case ash::AppListSearchResultCategory::kAppShortcuts:
      return SearchResultListView::SearchResultListType::kAppShortcuts;
    case ash::AppListSearchResultCategory::kWeb:
      return SearchResultListView::SearchResultListType::kWeb;
    case ash::AppListSearchResultCategory::kFiles:
      return SearchResultListView::SearchResultListType::kFiles;
    case ash::AppListSearchResultCategory::kSettings:
      return SearchResultListView::SearchResultListType::kSettings;
    case ash::AppListSearchResultCategory::kHelp:
      return SearchResultListView::SearchResultListType::kHelp;
    case ash::AppListSearchResultCategory::kPlayStore:
      return SearchResultListView::SearchResultListType::kPlayStore;
    case ash::AppListSearchResultCategory::kSearchAndAssistant:
      return SearchResultListView::SearchResultListType::kSearchAndAssistant;
    case ash::AppListSearchResultCategory::kUnknown:
      NOTREACHED();
      return SearchResultListView::SearchResultListType::kUnified;
  }
}

}  // namespace

SearchResultListView::SearchResultListView(
    AppListMainView* main_view,
    AppListViewDelegate* view_delegate,
    SearchResultPageDialogController* dialog_controller,
    SearchResultView::SearchResultViewType search_result_view_type,
    absl::optional<size_t> productivity_launcher_index)
    : SearchResultContainerView(view_delegate),
      main_view_(main_view),
      view_delegate_(view_delegate),
      results_container_(new views::View),
      productivity_launcher_index_(productivity_launcher_index),
      search_result_view_type_(search_result_view_type) {
  auto* layout = results_container_->SetLayoutManager(
      std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  title_label_ = AddChildView(std::make_unique<views::Label>(
      u"", CONTEXT_SEARCH_RESULT_CATEGORY_LABEL, STYLE_PRODUCTIVITY_LAUNCHER));
  title_label_->SetBackgroundColor(SK_ColorTRANSPARENT);
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetBorder(views::CreateEmptyBorder(
      kPreferredTitleTopMargins, kPreferredTitleHorizontalMargins,
      kPreferredTitleBottomMargins, kPreferredTitleHorizontalMargins));
  title_label_->SetVisible(false);
  results_container_->AddChildView(title_label_);

  size_t result_count =
      GetMaxSearchResultListItems() +
      SharedAppListConfig::instance().max_assistant_search_result_list_items();

  for (size_t i = 0; i < result_count; ++i) {
    search_result_views_.emplace_back(new SearchResultView(
        this, view_delegate_, dialog_controller, search_result_view_type));
    search_result_views_.back()->set_index_in_container(i);
    results_container_->AddChildView(search_result_views_.back());
    AddObservedResultView(search_result_views_.back());
  }
  AddChildView(results_container_);
}

SearchResultListView::~SearchResultListView() = default;

void SearchResultListView::SetListType(SearchResultListType list_type) {
  list_type_ = list_type;
  switch (list_type_.value()) {
    case SearchResultListType::kUnified:
    case SearchResultListType::kAnswerCard:
      // kUnified and kAnswerCard SearchResultListView do not have labels.
      title_label_->SetText(u"");
      break;
    case SearchResultListType::kBestMatch:
      title_label_->SetText(l10n_util::GetStringUTF16(
          IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_BEST_MATCH));
      break;
    case SearchResultListType::kApps:
      title_label_->SetText(
          l10n_util::GetStringUTF16(IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_APPS));
      break;
    case SearchResultListType::kAppShortcuts:
      title_label_->SetText(l10n_util::GetStringUTF16(
          IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_APP_SHORTCUTS));
      break;
    case SearchResultListType::kWeb:
      title_label_->SetText(
          l10n_util::GetStringUTF16(IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_WEB));
      break;
    case SearchResultListType::kFiles:
      title_label_->SetText(l10n_util::GetStringUTF16(
          IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_FILES));
      break;
    case SearchResultListType::kSettings:
      title_label_->SetText(l10n_util::GetStringUTF16(
          IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_SETTINGS));
      break;
    case SearchResultListType::kHelp:
      title_label_->SetText(
          l10n_util::GetStringUTF16(IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_HELP));
      break;
    case SearchResultListType::kPlayStore:
      title_label_->SetText(l10n_util::GetStringUTF16(
          IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_PLAY_STORE));
      break;
    case SearchResultListType::kSearchAndAssistant:
      title_label_->SetText(l10n_util::GetStringUTF16(
          IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_SEARCH_AND_ASSISTANT));
      break;
  }

  switch (list_type_.value()) {
    case SearchResultListType::kUnified:
    case SearchResultListType::kAnswerCard:
      // Classic SearchResultListView and Productivity Launcher Answer Card do
      // not have category labels.
      title_label_->SetVisible(false);
      break;
    case SearchResultListType::kBestMatch:
    case SearchResultListType::kApps:
    case SearchResultListType::kAppShortcuts:
    case SearchResultListType::kWeb:
    case SearchResultListType::kFiles:
    case SearchResultListType::kSettings:
    case SearchResultListType::kHelp:
    case SearchResultListType::kPlayStore:
    case SearchResultListType::kSearchAndAssistant:
      title_label_->SetVisible(true);
      break;
  }

#if DCHECK_IS_ON()
  switch (list_type_.value()) {
    case SearchResultListType::kAnswerCard:
      DCHECK(search_result_view_type_ ==
             SearchResultView::SearchResultViewType::kAnswerCard);
      break;
    case SearchResultListType::kUnified:
      DCHECK(search_result_view_type_ ==
             SearchResultView::SearchResultViewType::kClassic);
      break;
    case SearchResultListType::kBestMatch:
    case SearchResultListType::kApps:
    case SearchResultListType::kAppShortcuts:
    case SearchResultListType::kWeb:
    case SearchResultListType::kFiles:
    case SearchResultListType::kSettings:
    case SearchResultListType::kHelp:
    case SearchResultListType::kPlayStore:
    case SearchResultListType::kSearchAndAssistant:
      DCHECK(search_result_view_type_ ==
             SearchResultView::SearchResultViewType::kDefault);
      break;
  }
#endif
}

void SearchResultListView::ListItemsRemoved(size_t start, size_t count) {
  size_t last = std::min(start + count, search_result_views_.size());
  for (size_t i = start; i < last; ++i)
    GetResultViewAt(i)->ClearResult();

  SearchResultContainerView::ListItemsRemoved(start, count);
}

SearchResultView* SearchResultListView::GetResultViewAt(size_t index) {
  DCHECK(index >= 0 && index < search_result_views_.size());
  return search_result_views_[index];
}
std::vector<SearchResultListView::SearchResultListType>
SearchResultListView::GetAllListTypesForCategoricalSearch() {
  static const std::vector<SearchResultListType> categorical_search_types = {
      SearchResultListType::kAnswerCard,
      SearchResultListType::kBestMatch,
      SearchResultListType::kApps,
      SearchResultListType::kAppShortcuts,
      SearchResultListType::kWeb,
      SearchResultListType::kFiles,
      SearchResultListType::kSettings,
      SearchResultListType::kHelp,
      SearchResultListType::kPlayStore,
      SearchResultListType::kSearchAndAssistant};
  return categorical_search_types;
}

int SearchResultListView::DoUpdate() {
  if (productivity_launcher_index_.has_value()) {
    std::vector<ash::AppListSearchResultCategory>* ordered_categories =
        AppListModelProvider::Get()->search_model()->ordered_categories();
    if (productivity_launcher_index_ < ordered_categories->size()) {
      enabled_ = true;
      SetListType(CategoryToListType(
          (*ordered_categories)[productivity_launcher_index_.value()]));
    } else {
      enabled_ = false;
      list_type_.reset();
    }
  }

  SetVisible(enabled_);
  if (!enabled_ || !GetWidget() || !GetWidget()->IsVisible()) {
    for (auto* result_view : search_result_views_) {
      result_view->SetResult(nullptr);
      result_view->SetVisible(false);
    }
    return 0;
  }

  std::vector<SearchResult*> display_results = GetCategorizedSearchResults();
  size_t num_results = display_results.size();
  for (size_t i = 0; i < search_result_views_.size(); ++i) {
    SearchResultView* result_view = GetResultViewAt(i);
    if (i < num_results) {
      result_view->SetResult(display_results[i]);
      result_view->SizeToPreferredSize();
      result_view->SetVisible(true);
    } else {
      result_view->SetResult(nullptr);
      result_view->SetVisible(false);
    }
  }
  // the search_result_list_view should be hidden if there are no results.
  SetVisible(num_results > 0);

  auto* notifier = view_delegate_->GetNotifier();

  // TODO(crbug/1216097): replace metrics with something more meaningful.
  if (notifier) {
    std::vector<AppListNotifier::Result> notifier_results;
    for (const auto* result : display_results)
      notifier_results.emplace_back(result->id(), result->metrics_type());
    notifier->NotifyResultsUpdated(SearchResultDisplayType::kList,
                                   notifier_results);
  }

  // Logic for logging impression of items that were shown to user.
  // Each time DoUpdate() called, start a timer that will be fired after a
  // certain amount of time |kImpressionThreshold|. If during the waiting time,
  // there's another DoUpdate() called, reset the timer and start a new timer
  // with updated result list.
  if (impression_timer_.IsRunning())
    impression_timer_.Stop();
  impression_timer_.Start(FROM_HERE, kImpressionThreshold, this,
                          &SearchResultListView::LogImpressions);
  return display_results.size();
}

void SearchResultListView::LogImpressions() {
  // TODO(crbug.com/1216097): Handle impressions for bubble launcher.
  if (!main_view_)
    return;

  // Since no items is actually clicked, send the position index of clicked item
  // as -1.
  SearchModel* const search_model = AppListModelProvider::Get()->search_model();
  if (main_view_->search_box_view()->is_search_box_active()) {
    view_delegate_->NotifySearchResultsForLogging(
        search_model->search_box()->text(),
        GetSearchResultsForLogging(search_result_views_),
        -1 /* position_index */);
  }
}

void SearchResultListView::Layout() {
  results_container_->SetBoundsRect(GetLocalBounds());
}

gfx::Size SearchResultListView::CalculatePreferredSize() const {
  return results_container_->GetPreferredSize();
}

const char* SearchResultListView::GetClassName() const {
  return "SearchResultListView";
}

int SearchResultListView::GetHeightForWidth(int w) const {
  return results_container_->GetHeightForWidth(w);
}

void SearchResultListView::OnThemeChanged() {
  SearchResultContainerView::OnThemeChanged();
  title_label_->SetEnabledColor(
      AppListColorProvider::Get()->GetSearchBoxSecondaryTextColor(
          kDeprecatedSearchBoxTextDefaultColor));
}

void SearchResultListView::SearchResultActivated(SearchResultView* view,
                                                 int event_flags,
                                                 bool by_button_press) {
  if (!view_delegate_ || !view || !view->result())
    return;

  auto* result = view->result();

  RecordSearchResultOpenSource(result, view_delegate_->GetAppListViewState(),
                               view_delegate_->IsInTabletMode());
  SearchModel* const search_model = AppListModelProvider::Get()->search_model();
  view_delegate_->NotifySearchResultsForLogging(
      search_model->search_box()->text(),
      GetSearchResultsForLogging(search_result_views_),
      view->index_in_container());

  AppListLaunchType launch_type =
      IsAppListSearchResultAnApp(result->result_type())
          ? AppListLaunchType::kAppSearchResult
          : AppListLaunchType::kSearchResult;
  view_delegate_->OpenSearchResult(
      result->id(), result->result_type(), event_flags,
      AppListLaunchedFrom::kLaunchedFromSearchBox, launch_type,
      -1 /* suggestion_index */,
      !by_button_press && view->is_default_result() /* launch_as_default */);
}

void SearchResultListView::SearchResultActionActivated(
    SearchResultView* view,
    SearchResultActionType action) {
  if (view_delegate_ && view->result()) {
    switch (action) {
      case SearchResultActionType::kRemove:
        view_delegate_->InvokeSearchResultAction(view->result()->id(), action);
        break;
      case SearchResultActionType::kAppend:
        main_view_->search_box_view()->UpdateQuery(view->result()->title());
        break;
      case SearchResultActionType::kSearchResultActionTypeMax:
        NOTREACHED();
    }
  }
}

void SearchResultListView::VisibilityChanged(View* starting_from,
                                             bool is_visible) {
  SearchResultContainerView::VisibilityChanged(starting_from, is_visible);
  // We only do this work when is_visible is false.
  if (is_visible)
    return;
}

std::vector<SearchResult*> SearchResultListView::GetAssistantResults() {
  // Only show Assistant results if there are no tiles. There is not enough
  // room in launcher to display Assistant results if there are tiles visible.
  bool visible_tiles = !SearchModel::FilterSearchResultsByDisplayType(
                            results(), SearchResult::DisplayType::kTile,
                            /*excludes=*/{}, /*max_results=*/1)
                            .empty();

  if (visible_tiles)
    return std::vector<SearchResult*>();

  return SearchModel::FilterSearchResultsByFunction(
      results(), base::BindRepeating([](const SearchResult& search_result) {
        return search_result.display_type() == SearchResultDisplayType::kList &&
               search_result.result_type() ==
                   AppListSearchResultType::kAssistantText;
      }),
      /*max_results=*/
      SharedAppListConfig::instance().max_assistant_search_result_list_items());
}

std::vector<SearchResult*> SearchResultListView::GetUnifiedSearchResults() {
  std::vector<SearchResult*> search_results =
      SearchModel::FilterSearchResultsByFunction(
          results(), base::BindRepeating([](const SearchResult& result) {
            return result.display_type() == SearchResultDisplayType::kList &&
                   result.result_type() !=
                       AppListSearchResultType::kAssistantText;
          }),
          GetMaxSearchResultListItems());

  std::vector<SearchResult*> assistant_results = GetAssistantResults();

  search_results.insert(search_results.end(), assistant_results.begin(),
                        assistant_results.end());

  return search_results;
}

SearchResult::Category SearchResultListView::GetSearchCategory() {
  DCHECK(list_type_.has_value());
  switch (list_type_.value()) {
    case SearchResultListType::kUnified:
    case SearchResultListType::kBestMatch:
    case SearchResultListType::kAnswerCard:
      // Categories are undefined for |kUnified|, |KBestMatch|, and
      // |kAnswerCard| list types.
      NOTREACHED();
      return SearchResult::Category::kUnknown;
    case SearchResultListType::kApps:
      return SearchResult::Category::kApps;
    case SearchResultListType::kAppShortcuts:
      return SearchResult::Category::kAppShortcuts;
    case SearchResultListType::kWeb:
      return SearchResult::Category::kWeb;
    case SearchResultListType::kFiles:
      return SearchResult::Category::kFiles;
    case SearchResultListType::kSettings:
      return SearchResult::Category::kSettings;
    case SearchResultListType::kHelp:
      return SearchResult::Category::kHelp;
    case SearchResultListType::kPlayStore:
      return SearchResult::Category::kPlayStore;
    case SearchResultListType::kSearchAndAssistant:
      return SearchResult::Category::kSearchAndAssistant;
  }
}

std::vector<SearchResult*> SearchResultListView::GetCategorizedSearchResults() {
  DCHECK(enabled_ && list_type_.has_value());
  switch (list_type_.value()) {
    case SearchResultListType::kUnified:
      // Use classic search results for the kUnified list view.
      return GetUnifiedSearchResults();
    case SearchResultListType::kAnswerCard:
      return SearchModel::FilterSearchResultsByFunction(
          results(), base::BindRepeating([](const SearchResult& result) {
            return result.display_type() ==
                   SearchResultDisplayType::kAnswerCard;
          }),
          kAnswerCardMaxResults);
    case SearchResultListType::kBestMatch:
      // Filter results based on whether they have the best_match label.
      return SearchModel::FilterSearchResultsByFunction(
          results(), base::BindRepeating([](const SearchResult& result) {
            return result.best_match() &&
                   result.display_type() == SearchResultDisplayType::kList;
          }),
          GetMaxSearchResultListItems());
    case SearchResultListType::kApps:
    case SearchResultListType::kAppShortcuts:
    case SearchResultListType::kWeb:
    case SearchResultListType::kFiles:
    case SearchResultListType::kSettings:
    case SearchResultListType::kHelp:
    case SearchResultListType::kPlayStore:
    case SearchResultListType::kSearchAndAssistant:
      // filter results based on category. Filter out best match items to avoid
      // duplication between different types of search_result_list_views.
      SearchResult::Category search_category = GetSearchCategory();
      auto filter_function = base::BindRepeating(
          [](const SearchResult::Category& search_category,
             const SearchResult& result) -> bool {
            return result.category() == search_category &&
                   !result.best_match() &&
                   result.display_type() == SearchResultDisplayType::kList;
          },
          search_category);
      return SearchModel::FilterSearchResultsByFunction(
          results(), filter_function, GetMaxSearchResultListItems());
  }
}

}  // namespace ash

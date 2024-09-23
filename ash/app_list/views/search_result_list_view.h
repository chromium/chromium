// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_LIST_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_LIST_VIEW_H_

#include <stddef.h>

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "ash/app_list/views/search_result_container_view.h"
#include "ash/app_list/views/search_result_view.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

namespace test {
class SearchResultListViewTest;
}

class AppListViewDelegate;
class SearchResultPageDialogController;

// SearchResultListView displays SearchResultList with a list of
// SearchResultView.
class ASH_EXPORT SearchResultListView : public SearchResultContainerView {
  METADATA_HEADER(SearchResultListView, SearchResultContainerView)

 public:
  enum class SearchResultListType {
    // kAnswerCard list view contains a single result that has an extremely high
    // chance of being exactly what the user is looking for.
    kAnswerCard,
    // kBestMatch list view contains the results that are the best match for the
    // current query. This category should be used when productivity launcher is
    // enabled. All search results will show up under this category until search
    // metadata is updated with the other category labels.
    kBestMatch,
    // kApps list view contains existing non-game ARC and PWA apps that are
    // installed and are relevant to but not the best match for the current
    // query.
    kApps,
    // kAppShortcuts list view contains shortcuts to actions for existing apps.
    kAppShortcuts,
    // kWeb list view contains links to relevant websites.
    kWeb,
    // kFiles list view contains relevant local and Google Drive files.
    kFiles,
    // kSettings list view contains relevant system settings and personalization
    // settings.
    kSettings,
    // kHelp list view contains help articles from Showoff and Keyboard
    // Shortcuts.
    kHelp,
    // kPlayStore contains suggested apps from the playstore that are not
    // currently installed.
    kPlayStore,
    // kSearchAndAssistant contains suggestions from Search and Google
    // Assistant.
    kSearchAndAssistant,
    // kGames contains cloud game search results.
    kGames,
    kMaxValue = kGames,
  };

  SearchResultListView(
      AppListViewDelegate* view_delegate,
      SearchResultPageDialogController* dialog_controller,
      SearchResultView::SearchResultViewType search_result_view_type,
      std::optional<size_t> productivity_launcher_index);

  SearchResultListView(const SearchResultListView&) = delete;
  SearchResultListView& operator=(const SearchResultListView&) = delete;
  ~SearchResultListView() override;

  // Updates the type of search results the list view shows.
  void SetListType(SearchResultListType list_type);

  void SearchResultActivated(SearchResultView* view,
                             int event_flags,
                             bool by_button_press);

  void SearchResultActionActivated(SearchResultView* view,
                                   SearchResultActionType action);

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // Overridden from SearchResultContainerView:
  SearchResultView* GetResultViewAt(size_t index) override;
  void AppendShownResultMetadata(
      std::vector<SearchResultAimationMetadata>* result_metadata_) override;

  // Gets all the SearchResultListTypes that should be used when categorical
  // search is enabled.
  static std::vector<SearchResultListType>
  GetAllListTypesForCategoricalSearch();

  // This should not be called on a disabled list view as list_type_ will be
  // reset.
  const std::optional<SearchResultListType>& list_type_for_test() const {
    return list_type_;
  }

  views::Label* title_label_for_test() { return title_label_; }

 private:
  friend class test::SearchResultListViewTest;

  // Overridden from SearchResultContainerView:
  void OnSelectedResultChanged() override;
  int DoUpdate() override;
  void UpdateResultsVisibility(bool force_hide) override;
  views::View* GetTitleLabel() override;
  std::vector<views::View*> GetViewsToAnimate() override;

  // Overridden from views::View:
  void Layout(PassKey) override;

  // Fetches the category of results this view should show.
  SearchResult::Category GetSearchCategory();

  // Returns search results for the class's current list_type_.
  std::vector<SearchResult*> GetCategorizedSearchResults();

  // Updates the set of results shown in this result list.
  std::vector<SearchResult*> UpdateResultViews();

  // A filter that returns whether a search result should be shown in the best
  // matches container.
  bool FilterBestMatches(const SearchResult& result) const;

  // A filter that returns whether a search results should be shown in the
  // categorized flavour of search result list.
  bool FilterSearchResultsByCategory(const SearchResult::Category& category,
                                     const SearchResult& result) const;

  raw_ptr<views::View> results_container_;

  std::vector<raw_ptr<SearchResultView, VectorExperimental>>
      search_result_views_;  // Not owned.

  // The SearchResultListViewType dictates what kinds of results will be shown.
  std::optional<SearchResultListType> list_type_ =
      SearchResultListType::kBestMatch;
  raw_ptr<views::Label> title_label_ = nullptr;  // Owned by view hierarchy.

  // The search result list view's location in the
  // productivity_launcher_search_view_'s list of 'search_result_list_view_'.
  // Not set if productivity_launcher is disabled or if the position of the
  // category is const as for kBestMatch.
  const std::optional<size_t> productivity_launcher_index_;

  const SearchResultView::SearchResultViewType search_result_view_type_;

  // Set of results that have been removed from the result list using remove
  // search result actions. Used to filter those results out from the list of
  // shown results until results in the search model get refreshed.
  std::set<std::string> removed_results_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_LIST_VIEW_H_

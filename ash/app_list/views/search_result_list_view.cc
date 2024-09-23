// Copyright 2012 The Chromium Authors
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
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "base/dcheck_is_on.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kPreferredTitleHorizontalMargins = 16;
constexpr int kPreferredTitleTopMargins = 12;
constexpr int kPreferredTitleBottomMargins = 4;

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
    case ash::AppListSearchResultCategory::kGames:
      return SearchResultListView::SearchResultListType::kGames;
    case ash::AppListSearchResultCategory::kUnknown:
      NOTREACHED();
  }
}

}  // namespace

SearchResultListView::SearchResultListView(
    AppListViewDelegate* view_delegate,
    SearchResultPageDialogController* dialog_controller,
    SearchResultView::SearchResultViewType search_result_view_type,
    std::optional<size_t> productivity_launcher_index)
    : SearchResultContainerView(view_delegate),
      results_container_(new views::View),
      productivity_launcher_index_(productivity_launcher_index),
      search_result_view_type_(search_result_view_type) {
  auto* layout = results_container_->SetLayoutManager(
      std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  title_label_ = AddChildView(std::make_unique<views::Label>(
      u"", CONTEXT_SEARCH_RESULT_CATEGORY_LABEL, STYLE_LAUNCHER));
  title_label_->SetBackgroundColor(SK_ColorTRANSPARENT);
  title_label_->SetAutoColorReadabilityEnabled(false);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2,
                                        *title_label_);
  title_label_->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);

  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kPreferredTitleTopMargins, kPreferredTitleHorizontalMargins,
      kPreferredTitleBottomMargins, kPreferredTitleHorizontalMargins)));
  title_label_->SetVisible(false);
  title_label_->SetPaintToLayer();
  title_label_->layer()->SetFillsBoundsOpaquely(false);

  results_container_->AddChildView(title_label_.get());

  size_t result_count =
      ash::SharedAppListConfig::instance()
          .max_results_with_categorical_search() +
      SharedAppListConfig::instance().max_assistant_search_result_list_items();

  for (size_t i = 0; i < result_count; ++i) {
    search_result_views_.emplace_back(new SearchResultView(
        this, view_delegate, dialog_controller, search_result_view_type_));
    search_result_views_.back()->set_index_in_container(i);
    search_result_views_.back()->SetPaintToLayer();
    search_result_views_.back()->layer()->SetFillsBoundsOpaquely(false);
    results_container_->AddChildView(search_result_views_.back());
    AddObservedResultView(search_result_views_.back());
  }
  AddChildView(results_container_.get());
}

SearchResultListView::~SearchResultListView() = default;

void SearchResultListView::SetListType(SearchResultListType list_type) {
  if (list_type_ != list_type)
    removed_results_.clear();

  list_type_ = list_type;
  switch (list_type_.value()) {
    case SearchResultListType::kAnswerCard:
      // kAnswerCard SearchResultListView do not have labels.
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
    case SearchResultListType::kGames:
      title_label_->SetText(l10n_util::GetStringUTF16(
          IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_GAMES));
      break;
  }

  switch (list_type_.value()) {
    case SearchResultListType::kAnswerCard:
      // Answer Cards do not have category labels.
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
    case SearchResultListType::kGames:
      title_label_->SetVisible(true);
      break;
  }

  // A valid role must be set prior to setting the name.
  GetViewAccessibility().SetRole(ax::mojom::Role::kListBox);
  GetViewAccessibility().SetName(
      l10n_util::GetStringFUTF16(
          IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_ACCESSIBLE_NAME,
          title_label_->GetText()),
      ax::mojom::NameFrom::kAttribute);

#if DCHECK_IS_ON()
  switch (list_type_.value()) {
    case SearchResultListType::kAnswerCard:
      DCHECK(search_result_view_type_ ==
             SearchResultView::SearchResultViewType::kAnswerCard);
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
    case SearchResultListType::kGames:
      DCHECK(search_result_view_type_ ==
             SearchResultView::SearchResultViewType::kDefault);
      break;
  }
#endif
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
      SearchResultListType::kSearchAndAssistant,
      SearchResultListType::kGames};
  return categorical_search_types;
}

void SearchResultListView::AppendShownResultMetadata(
    std::vector<SearchResultAimationMetadata>* result_metadata_) {
  for (size_t i = 0; i < search_result_views_.size(); ++i) {
    SearchResultView* result_view = GetResultViewAt(i);
    if (i >= num_results() || !result_view->result()) {
      return;
    }
    SearchResultAimationMetadata metadata;
    metadata.result_id = result_view->result()->id();
    metadata.skip_animations = result_view->result()->skip_update_animation();
    result_metadata_->push_back(std::move(metadata));
  }
}

void SearchResultListView::OnSelectedResultChanged() {
  for (SearchResultView* view : search_result_views_)
    view->OnSelectedResultChanged();
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

  if (!enabled_ || !GetWidget() || !GetWidget()->IsVisible()) {
    ResetAndHide();
    return 0;
  }

  std::vector<SearchResult*> displayed_results = UpdateResultViews();
  NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged, false);

  auto* notifier = view_delegate()->GetNotifier();

  // TODO(crbug.com/40184658): replace metrics with something more meaningful.
  if (notifier) {
    std::vector<AppListNotifier::Result> notifier_results;
    for (const auto* result : displayed_results)
      notifier_results.emplace_back(result->id(), result->metrics_type(),
                                    result->continue_file_suggestion_type());
    notifier->NotifyResultsUpdated(
        list_type_ == SearchResultListType::kAnswerCard
            ? SearchResultDisplayType::kAnswerCard
            : SearchResultDisplayType::kList,
        notifier_results);
  }
  return displayed_results.size();
}

void SearchResultListView::UpdateResultsVisibility(bool force_hide) {
  SetVisible(num_results() > 0 && enabled_ && !force_hide);
  for (size_t i = 0; i < search_result_views_.size(); ++i) {
    SearchResultView* result_view = GetResultViewAt(i);
    result_view->SetVisible(i < num_results() && !force_hide);
  }
}

views::View* SearchResultListView::GetTitleLabel() {
  return title_label_.get();
}

std::vector<views::View*> SearchResultListView::GetViewsToAnimate() {
  std::vector<views::View*> results;
  for (size_t i = 0; i < search_result_views_.size() && i < num_results();
       ++i) {
    results.push_back(GetResultViewAt(i));
  }
  return results;
}

void SearchResultListView::Layout(PassKey) {
  results_container_->SetBoundsRect(GetLocalBounds());
}

gfx::Size SearchResultListView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return results_container_->GetPreferredSize(available_size);
}

void SearchResultListView::SearchResultActivated(SearchResultView* view,
                                                 int event_flags,
                                                 bool by_button_press) {
  if (!view_delegate() || !view || !view->result()) {
    return;
  }

  auto* result = view->result();

  AppListLaunchType launch_type =
      IsAppListSearchResultAnApp(result->result_type())
          ? AppListLaunchType::kAppSearchResult
          : AppListLaunchType::kSearchResult;
  view_delegate()->OpenSearchResult(
      result->id(), event_flags, AppListLaunchedFrom::kLaunchedFromSearchBox,
      launch_type, -1 /* suggestion_index */,
      !by_button_press && view->is_default_result() /* launch_as_default */);
}

void SearchResultListView::SearchResultActionActivated(
    SearchResultView* view,
    SearchResultActionType action) {
  if (view_delegate() && view->result()) {
    switch (action) {
      case SearchResultActionType::kRemove: {
        const std::string result_id = view->result()->id();
        removed_results_.insert(result_id);
        view_delegate()->InvokeSearchResultAction(result_id, action);
        Update();
        break;
      }
    }
  }
}

SearchResult::Category SearchResultListView::GetSearchCategory() {
  DCHECK(list_type_.has_value());
  switch (list_type_.value()) {
    case SearchResultListType::kBestMatch:
    case SearchResultListType::kAnswerCard:
      // Categories are undefined for |KBestMatch|, and
      // |kAnswerCard| list types.
      NOTREACHED();
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
    case SearchResultListType::kGames:
      return SearchResult::Category::kGames;
  }
}

std::vector<SearchResult*> SearchResultListView::GetCategorizedSearchResults() {
  DCHECK(enabled_ && list_type_.has_value());
  switch (list_type_.value()) {
    case SearchResultListType::kAnswerCard:
      return SearchModel::FilterSearchResultsByFunction(
          results(), base::BindRepeating([](const SearchResult& result) {
            return result.display_type() ==
                   SearchResultDisplayType::kAnswerCard;
          }),
          ash::SharedAppListConfig::instance().answer_card_max_results());
    case SearchResultListType::kBestMatch:
      // Filter results based on whether they have the best_match label.
      return SearchModel::FilterSearchResultsByFunction(
          results(),
          base::BindRepeating(&SearchResultListView::FilterBestMatches,
                              base::Unretained(this)),
          ash::SharedAppListConfig::instance()
              .max_results_with_categorical_search());
    case SearchResultListType::kApps:
    case SearchResultListType::kAppShortcuts:
    case SearchResultListType::kWeb:
    case SearchResultListType::kFiles:
    case SearchResultListType::kSettings:
    case SearchResultListType::kHelp:
    case SearchResultListType::kPlayStore:
    case SearchResultListType::kSearchAndAssistant:
    case SearchResultListType::kGames:
      SearchResult::Category search_category = GetSearchCategory();
      return SearchModel::FilterSearchResultsByFunction(
          results(),
          base::BindRepeating(
              &SearchResultListView::FilterSearchResultsByCategory,
              base::Unretained(this), search_category),
          ash::SharedAppListConfig::instance()
              .max_results_with_categorical_search());
  }
}

std::vector<SearchResult*> SearchResultListView::UpdateResultViews() {
  std::vector<SearchResult*> display_results = GetCategorizedSearchResults();
  const size_t num_results = display_results.size();
  for (size_t i = 0; i < search_result_views_.size(); ++i) {
    SearchResultView* result_view = GetResultViewAt(i);
    if (i < num_results) {
      result_view->SetResult(display_results[i]);
      result_view->SizeToPreferredSize();
    } else {
      result_view->SetResult(nullptr);
    }
  }

  return display_results;
}

bool SearchResultListView::FilterBestMatches(const SearchResult& result) const {
  // Filter out results that have been removed from the list by the user.
  if (removed_results_.count(result.id()))
    return false;
  return result.best_match() &&
         result.display_type() == SearchResultDisplayType::kList;
}

bool SearchResultListView::FilterSearchResultsByCategory(
    const SearchResult::Category& category,
    const SearchResult& result) const {
  // Filter out results that have been removed from the list by the user.
  if (removed_results_.count(result.id()))
    return false;
  // Filter out best match items to avoid
  // duplication between different types of search_result_list_views.
  return result.category() == category && !result.best_match() &&
         result.display_type() == SearchResultDisplayType::kList;
}

BEGIN_METADATA(SearchResultListView)
END_METADATA

}  // namespace ash

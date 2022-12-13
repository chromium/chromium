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
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/bind.h"
#include "base/dcheck_is_on.h"
#include "base/time/time.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
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

constexpr static base::TimeDelta kFadeInDuration = base::Milliseconds(100);
constexpr static base::TimeDelta kIdentityTranslationDuration =
    base::Milliseconds(200);

constexpr static base::TimeDelta kFastFadeInDuration = base::Milliseconds(0);

// TODO(crbug.com/1199206): Move this into SharedAppListConfig once the UI for
// categories is more developed.
constexpr size_t kMaxResultsWithCategoricalSearch = 3;
constexpr int kAnswerCardMaxResults = 1;

// Show animations for search result views and titles have a translation
// distance of 'kAnimatedOffsetMultiplier' * i where i is the position of the
// view in the 'ProductivityLauncherSearchView'.
constexpr int kAnimatedOffsetMultiplier = 4;

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
      return SearchResultListView::SearchResultListType::kBestMatch;
  }
}

}  // namespace

SearchResultListView::SearchResultListView(
    AppListViewDelegate* view_delegate,
    SearchResultPageDialogController* dialog_controller,
    SearchResultView::SearchResultViewType search_result_view_type,
    bool animates_result_updates,
    absl::optional<size_t> productivity_launcher_index)
    : SearchResultContainerView(view_delegate),
      view_delegate_(view_delegate),
      animates_result_updates_(animates_result_updates),
      results_container_(new views::View),
      productivity_launcher_index_(productivity_launcher_index),
      search_result_view_type_(search_result_view_type) {
  auto* layout = results_container_->SetLayoutManager(
      std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  title_label_ = AddChildView(std::make_unique<views::Label>(
      u"", CONTEXT_SEARCH_RESULT_CATEGORY_LABEL, STYLE_PRODUCTIVITY_LAUNCHER));
  title_label_->SetBackgroundColor(SK_ColorTRANSPARENT);
  title_label_->SetAutoColorReadabilityEnabled(false);
  title_label_->SetEnabledColorId(kColorAshTextColorSecondary);
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kPreferredTitleTopMargins, kPreferredTitleHorizontalMargins,
      kPreferredTitleBottomMargins, kPreferredTitleHorizontalMargins)));
  title_label_->SetVisible(false);
  title_label_->SetPaintToLayer();
  title_label_->layer()->SetFillsBoundsOpaquely(false);

  results_container_->AddChildView(title_label_);

  size_t result_count =
      kMaxResultsWithCategoricalSearch +
      SharedAppListConfig::instance().max_assistant_search_result_list_items();

  for (size_t i = 0; i < result_count; ++i) {
    search_result_views_.emplace_back(new SearchResultView(
        this, view_delegate_, dialog_controller, search_result_view_type_));
    search_result_views_.back()->set_index_in_container(i);
    search_result_views_.back()->SetPaintToLayer();
    search_result_views_.back()->layer()->SetFillsBoundsOpaquely(false);
    results_container_->AddChildView(search_result_views_.back());
    AddObservedResultView(search_result_views_.back());
  }
  AddChildView(results_container_);
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
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kListBox);
  GetViewAccessibility().OverrideName(l10n_util::GetStringFUTF16(
      IDS_ASH_SEARCH_RESULT_CATEGORY_LABEL_ACCESSIBLE_NAME,
      title_label_->GetText()));

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

absl::optional<SearchResultContainerView::ResultsAnimationInfo>
SearchResultListView::ScheduleResultAnimations(
    const ResultsAnimationInfo& aggregate_animation_info) {
  DCHECK(animates_result_updates_);

  // Collect current container animation info.
  ResultsAnimationInfo current_animation_info;

  if (num_results_ < 1 || !enabled_) {
    SetVisible(false);
    for (auto* result_view : search_result_views_)
      result_view->SetVisible(false);
    return current_animation_info;
  }

  // All views should be animated if
  // *   the container is being shown, or
  // *   any of the result views that precede the container in the search UI are
  //     animating, or
  // *   if.the first animating result view is in a preceding container.
  bool force_animation =
      !GetVisible() || aggregate_animation_info.animating_views > 0 ||
      aggregate_animation_info.first_animated_result_view_index <=
          aggregate_animation_info.total_result_views;

  SetVisible(true);
  current_animation_info.use_short_animations =
      aggregate_animation_info.use_short_animations;

  auto schedule_animation = [this, &current_animation_info,
                             &aggregate_animation_info](views::View* view) {
    ShowViewWithAnimation(view,
                          current_animation_info.total_views +
                              aggregate_animation_info.total_views,
                          current_animation_info.use_short_animations);
    ++current_animation_info.animating_views;
  };

  if (title_label_->GetVisible()) {
    if (force_animation)
      schedule_animation(title_label_);
    ++current_animation_info.total_views;
  }

  for (size_t i = 0; i < search_result_views_.size(); ++i) {
    SearchResultView* result_view = GetResultViewAt(i);
    result_view->SetVisible(i < num_results_);

    if (i < num_results_) {
      // Checks whether the index of the current result view is greater than
      // or equal to the index of the first result view that should be animated.
      // Force animations if true.
      if (aggregate_animation_info.total_result_views +
              current_animation_info.total_result_views >=
          aggregate_animation_info.first_animated_result_view_index) {
        force_animation = true;
      }
      if (force_animation)
        schedule_animation(result_view);

      ++current_animation_info.total_views;
      ++current_animation_info.total_result_views;
    }
  }

  return current_animation_info;
}

void SearchResultListView::AppendShownResultMetadata(
    std::vector<SearchResultAimationMetadata>* result_metadata_) {
  for (size_t i = 0; i < search_result_views_.size(); ++i) {
    SearchResultView* result_view = GetResultViewAt(i);
    if (i >= num_results_ || !result_view->result())
      return;
    SearchResultAimationMetadata metadata;
    metadata.result_id = result_view->result()->id();
    metadata.skip_animations = result_view->result()->skip_update_animation();
    result_metadata_->push_back(std::move(metadata));
  }
}

bool SearchResultListView::HasAnimatingChildView() {
  auto is_animating = [](views::View* view) {
    return (view->GetVisible() && view->layer() &&
            view->layer()->GetAnimator() &&
            view->layer()->GetAnimator()->is_animating());
  };

  if (is_animating(title_label_))
    return true;

  for (size_t i = 0; i < search_result_views_.size(); ++i) {
    if (is_animating(GetResultViewAt(i)))
      return true;
  }
  return false;
}

void SearchResultListView::ShowViewWithAnimation(views::View* view,
                                                 int position,
                                                 bool use_short_animations) {
  DCHECK(view->layer()->GetAnimator());

  // Abort any in-progress layer animation.
  view->layer()->GetAnimator()->AbortAllAnimations();

  // Animation spec:
  //
  // Y Position: Down (offset) → End position
  // offset: position * kAnimatedOffsetMultiplier px
  // Duration: 200ms
  // Ease: (0.00, 0.00, 0.20, 1.00)

  // Opacity: 0% -> 100%
  // Duration: 100 ms
  // Ease: Linear

  gfx::Transform translate_down;
  translate_down.Translate(0, position * kAnimatedOffsetMultiplier);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetOpacity(view, 0.0f)
      .SetTransform(view, translate_down)
      .Then()
      .SetOpacity(view, 1.0f, gfx::Tween::LINEAR)
      .SetDuration(use_short_animations ? kFastFadeInDuration : kFadeInDuration)
      .At(base::TimeDelta())
      .SetDuration(
          use_short_animations
              ? app_list_features::DynamicSearchUpdateAnimationDuration()
              : kIdentityTranslationDuration)
      .SetTransform(view, gfx::Transform(), gfx::Tween::LINEAR_OUT_SLOW_IN);
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

  auto* notifier = view_delegate_->GetNotifier();

  // TODO(crbug/1216097): replace metrics with something more meaningful.
  if (notifier) {
    std::vector<AppListNotifier::Result> notifier_results;
    for (const auto* result : displayed_results)
      notifier_results.emplace_back(result->id(), result->metrics_type());
    notifier->NotifyResultsUpdated(
        list_type_ == SearchResultListType::kAnswerCard
            ? SearchResultDisplayType::kAnswerCard
            : SearchResultDisplayType::kList,
        notifier_results);
  }
  return displayed_results.size();
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

void SearchResultListView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // With productivity launcher disabled, the parent search result page view
  // will have the list box role.
  if (ash::features::IsProductivityLauncherEnabled())
    node_data->role = ax::mojom::Role::kListBox;
}

int SearchResultListView::GetHeightForWidth(int w) const {
  return results_container_->GetHeightForWidth(w);
}

void SearchResultListView::SearchResultActivated(SearchResultView* view,
                                                 int event_flags,
                                                 bool by_button_press) {
  if (!view_delegate_ || !view || !view->result())
    return;

  auto* result = view->result();

  RecordSearchResultOpenSource(result, view_delegate_->GetAppListViewState(),
                               view_delegate_->IsInTabletMode());

  AppListLaunchType launch_type =
      IsAppListSearchResultAnApp(result->result_type())
          ? AppListLaunchType::kAppSearchResult
          : AppListLaunchType::kSearchResult;
  view_delegate_->OpenSearchResult(
      result->id(), event_flags, AppListLaunchedFrom::kLaunchedFromSearchBox,
      launch_type, -1 /* suggestion_index */,
      !by_button_press && view->is_default_result() /* launch_as_default */);
}

void SearchResultListView::SearchResultActionActivated(
    SearchResultView* view,
    SearchResultActionType action) {
  if (view_delegate_ && view->result()) {
    switch (action) {
      case SearchResultActionType::kRemove: {
        const std::string result_id = view->result()->id();
        removed_results_.insert(result_id);
        view_delegate_->InvokeSearchResultAction(result_id, action);
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
          kAnswerCardMaxResults);
    case SearchResultListType::kBestMatch:
      // Filter results based on whether they have the best_match label.
      return SearchModel::FilterSearchResultsByFunction(
          results(),
          base::BindRepeating(&SearchResultListView::FilterBestMatches,
                              base::Unretained(this)),
          kMaxResultsWithCategoricalSearch);
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
          kMaxResultsWithCategoricalSearch);
  }
}

std::vector<SearchResult*> SearchResultListView::UpdateResultViews() {
  std::vector<SearchResult*> display_results = GetCategorizedSearchResults();
  size_t num_results = display_results.size();
  num_results_ = num_results;
  for (size_t i = 0; i < search_result_views_.size(); ++i) {
    SearchResultView* result_view = GetResultViewAt(i);
    if (i < num_results) {
      result_view->SetResult(display_results[i]);
      result_view->SizeToPreferredSize();
    } else {
      result_view->SetResult(nullptr);
    }
    // If result updates are animated, the result visibility will be updated in
    // `ScheduleResultAnimations()`
    if (!animates_result_updates_)
      result_view->SetVisible(i < num_results);
  }

  // If result updates are animated, the container visibility will be updated in
  // `ScheduleResultAnimations()`
  if (!animates_result_updates_)
    SetVisible(num_results > 0);
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

}  // namespace ash

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_search_view.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/app_list_toast_view.h"
#include "ash/app_list/views/result_selection_controller.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_notifier_controller.h"
#include "ash/app_list/views/search_result_image_list_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_view.h"
#include "ash/constants/ash_features.h"
#include "ash/controls/rounded_scroll_bar.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

using views::BoxLayout;

namespace ash {

namespace {

// The amount of time by which notifications to accessibility framework about
// result page changes are delayed.
constexpr base::TimeDelta kNotifyA11yDelay = base::Milliseconds(1500);

// Insets for the vertical scroll bar.
constexpr auto kVerticalScrollInsets = gfx::Insets::TLBR(1, 0, 1, 1);

// The amount of time after search result animations are preempted during which
// result animations should be sped up.
constexpr base::TimeDelta kForcedFastAnimationInterval =
    base::Milliseconds(500);

// The size of the icon in the search notifier.
constexpr int kSearchNotifierIconSize = 30;

}  // namespace

AppListSearchView::AppListSearchView(
    AppListViewDelegate* view_delegate,
    SearchResultPageDialogController* dialog_controller,
    SearchBoxView* search_box_view)
    : dialog_controller_(dialog_controller), search_box_view_(search_box_view) {
  DCHECK(view_delegate);
  DCHECK(search_box_view_);
  SetUseDefaultFillLayout(true);

  // The entire page scrolls. Use layer scrolling so that the contents will
  // paint on top of the parent, which uses SetPaintToLayer().
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll_view_->SetDrawOverflowIndicator(false);

  // Arrow keys are used for focus updating and the result selection handles the
  // scrolling.
  scroll_view_->SetAllowKeyboardScrolling(false);

  // Don't paint a background. The bubble already has one.
  scroll_view_->SetBackgroundColor(absl::nullopt);

  // Set up scroll bars.
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  auto vertical_scroll =
      std::make_unique<RoundedScrollBar>(/*horizontal=*/false);
  vertical_scroll->SetInsets(kVerticalScrollInsets);
  scroll_view_->SetVerticalScrollBar(std::move(vertical_scroll));

  auto scroll_contents = std::make_unique<views::View>();
  scroll_contents->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));

  result_selection_controller_ = std::make_unique<ResultSelectionController>(
      &result_container_views_,
      base::BindRepeating(&AppListSearchView::OnSelectedResultChanged,
                          base::Unretained(this)));
  search_box_view_->SetResultSelectionController(
      result_selection_controller_.get());

  if (features::IsProductivityLauncherImageSearchEnabled()) {
    search_notifier_controller_ = std::make_unique<SearchNotifierController>();
    if (search_notifier_controller_->ShouldShowPrivacyNotice()) {
      // The image search category should be always disabled unless the search
      // notifier is accepted or timeout.
      view_delegate->SetCategoryEnabled(AppListSearchControlCategory::kImages,
                                        false);

      const std::u16string notifier_title = l10n_util::GetStringUTF16(
          IDS_ASH_SEARCH_IMAGE_SEARCH_PRIVACY_NOTICE_TITLE);
      const std::u16string notifier_subtitle = l10n_util::GetStringUTF16(
          IDS_ASH_SEARCH_IMAGE_SEARCH_PRIVACY_NOTICE_CONTENT);

      AppListToastView::Builder toast_view_builder(notifier_title);
      toast_view_builder.SetButton(
          l10n_util::GetStringUTF16(IDS_ASH_CONTINUE_BUTTON),
          base::BindRepeating(&AppListSearchView::OnSearchNotifierButtonPressed,
                              weak_ptr_factory_.GetWeakPtr()));
      search_notifier_ = scroll_contents->AddChildView(
          toast_view_builder.SetViewDelegate(view_delegate)
              .SetIcon(ui::ImageModel::FromVectorIcon(
                  vector_icons::kImageSearchIcon, ui::kColorMenuIcon,
                  kSearchNotifierIconSize))
              .SetSubtitle(notifier_subtitle)
              .SetSubtitleMultiline(true)
              .Build());
      search_notifier_->SetProperty(views::kMarginsKey,
                                    gfx::Insets::TLBR(16, 16, 0, 16));
      search_notifier_->icon()->SetProperty(views::kMarginsKey,
                                            gfx::Insets::TLBR(8, 8, 8, 0));
      search_notifier_->toast_button()->SetAccessibleName(notifier_subtitle);

      // Add a guidance under AppListSearchView to guide users to move focus to
      // the search notifier.
      auto notifier_guide = std::make_unique<views::AXVirtualView>();
      search_notifier_guide_ = notifier_guide.get();
      auto& data = notifier_guide->GetCustomData();
      data.role = ax::mojom::Role::kAlert;
      data.SetName(notifier_title);
      data.SetDescription(l10n_util::GetStringUTF16(
          IDS_ASH_SEARCH_IMAGE_SEARCH_PRIVACY_NOTICE_ACCESSIBILITY_GUIDANCE));
      GetViewAccessibility().AddVirtualChildView(std::move(notifier_guide));
    }
  }

  auto add_result_container = [&](SearchResultContainerView* new_container) {
    new_container->SetResults(
        AppListModelProvider::Get()->search_model()->results());
    new_container->set_delegate(this);
    new_container->SetVisible(false);
    result_container_views_.push_back(new_container);
  };

  // kAnswerCard is always the first list view shown.
  auto* answer_card_container =
      scroll_contents->AddChildView(std::make_unique<SearchResultListView>(
          view_delegate, dialog_controller_,
          SearchResultView::SearchResultViewType::kAnswerCard, absl::nullopt));
  answer_card_container->SetListType(
      SearchResultListView::SearchResultListType::kAnswerCard);
  add_result_container(answer_card_container);

  // kBestMatch is always the second list view shown.
  auto* best_match_container =
      scroll_contents->AddChildView(std::make_unique<SearchResultListView>(
          view_delegate, dialog_controller_,
          SearchResultView::SearchResultViewType::kDefault, absl::nullopt));
  best_match_container->SetListType(
      SearchResultListView::SearchResultListType::kBestMatch);
  add_result_container(best_match_container);

  // Launcher image search container is always the third view shown.
  if (features::IsProductivityLauncherImageSearchEnabled()) {
    image_search_container_ = scroll_contents->AddChildView(
        std::make_unique<SearchResultImageListView>(view_delegate));
    add_result_container(image_search_container_);
  }

  // SearchResultListViews are aware of their relative position in the
  // Productivity launcher search view. SearchResultListViews with mutable
  // positions are passed their productivity_launcher_search_view_position to
  // update their own category type. kAnswerCard and kBestMatch have already
  // been constructed.
  const size_t category_count =
      SearchResultListView::GetAllListTypesForCategoricalSearch().size() -
      result_container_views_.size();
  for (size_t i = 0; i < category_count; ++i) {
    auto* result_container =
        scroll_contents->AddChildView(std::make_unique<SearchResultListView>(
            view_delegate, dialog_controller_,
            SearchResultView::SearchResultViewType::kDefault, i));
    add_result_container(result_container);
  }

  scroll_view_->SetContents(std::move(scroll_contents));

  AppListModelProvider* const model_provider = AppListModelProvider::Get();
  model_provider->AddObserver(this);
}

AppListSearchView::~AppListSearchView() {
  AppListModelProvider::Get()->RemoveObserver(this);
}

void AppListSearchView::OnSearchResultContainerResultsChanging() {
  // Block any result selection changes while result updates are in flight.
  // The selection will be reset once the results are all updated.
  result_selection_controller_->set_block_selection_changes(true);

  notify_a11y_results_changed_timer_.Stop();
  SetIgnoreResultChangesForA11y(true);
}

void AppListSearchView::OnSearchResultContainerResultsChanged() {
  DCHECK(!result_container_views_.empty());

  int result_count = 0;
  // Only sort and layout the containers when they have all updated.
  for (SearchResultContainerView* view : result_container_views_) {
    if (view->UpdateScheduled()) {
      return;
    }
    result_count += view->num_results();
  }

  SearchResultBaseView* first_result_view = nullptr;
  std::vector<SearchResultContainerView::SearchResultAimationMetadata>
      search_result_metadata;

  // If the user cleared the search box text, skip animating the views. The
  // visible views will animate out and the whole search page will be hidden.
  // See AppListBubbleSearchPage::AnimateHidePage().
  if (search_box_view_->HasSearch()) {
    using AnimationInfo = SearchResultContainerView::ResultsAnimationInfo;
    AnimationInfo aggregate_animation_info;
    // Search result changes within `kForcedFastAnimationInterval` of
    // `search_result_fast_update_time_` should also use fast animations and
    // refresh the timestamp.
    if (search_result_fast_update_time_.has_value() &&
        app_list_features::IsDynamicSearchUpdateAnimationEnabled()) {
      const base::TimeDelta time_since_last_update =
          base::TimeTicks::Now() - search_result_fast_update_time_.value();
      if (time_since_last_update < kForcedFastAnimationInterval) {
        aggregate_animation_info.use_short_animations = true;
      }
    }

    for (SearchResultContainerView* view : result_container_views_) {
      view->AppendShownResultMetadata(&search_result_metadata);
    }

    int first_animated_result_view_index = 0;
    for (size_t i = 0; i < std::min(search_result_metadata.size(),
                                    last_result_metadata_.size());
         ++i) {
      const bool matching_result_id = search_result_metadata[i].result_id ==
                                      last_result_metadata_[i].result_id;
      const bool skip_animations = search_result_metadata[i].skip_animations &&
                                   last_result_metadata_[i].skip_animations;
      if (!skip_animations && !matching_result_id) {
        break;
      }
      first_animated_result_view_index += 1;
    }

    aggregate_animation_info.first_animated_result_view_index =
        first_animated_result_view_index;

    for (SearchResultContainerView* view : result_container_views_) {
      absl::optional<AnimationInfo> container_animation_info =
          view->ScheduleResultAnimations(aggregate_animation_info);
      if (container_animation_info) {
        aggregate_animation_info.total_views +=
            container_animation_info->total_views;
        aggregate_animation_info.total_result_views +=
            container_animation_info->total_result_views;
        aggregate_animation_info.animating_views +=
            container_animation_info->animating_views;
      }

      // Fetch the first visible search result view for search box autocomplete.
      if (!first_result_view) {
        first_result_view = view->GetFirstResultView();
      }
    }
    // Update the `search_result_fast_update_time_` if fast animations were
    // used.
    if (aggregate_animation_info.use_short_animations) {
      search_result_fast_update_time_ = base::TimeTicks::Now();
    }

    // Records metrics on whether shortened search animations were used.
    base::UmaHistogramBoolean("Ash.SearchResultUpdateAnimationShortened",
                              aggregate_animation_info.use_short_animations);
  }
  Layout();

  last_search_result_count_ = result_count;
  last_result_metadata_.swap(search_result_metadata);

  ScheduleResultsChangedA11yNotification();

  // Reset selection to first when things change. The first result is set as
  // as the default result.
  result_selection_controller_->set_block_selection_changes(false);
  result_selection_controller_->ResetSelection(/*key_event=*/nullptr,
                                               /*default_selection=*/true);
  // Update SearchBoxView search box autocomplete as necessary based on new
  // first result view.
  if (first_result_view) {
    search_box_view_->ProcessAutocomplete(first_result_view);
  } else {
    search_box_view_->ClearAutocompleteText();
  }
}

void AppListSearchView::VisibilityChanged(View* starting_from,
                                          bool is_visible) {
  if (!is_visible) {
    result_selection_controller_->ClearSelection();
    for (auto* container : result_container_views_) {
      container->ResetAndHide();
    }
  }
}

void AppListSearchView::OnKeyEvent(ui::KeyEvent* event) {
  // Only handle the key event that triggers the focus or result selection
  // traversal here.
  if (event->type() != ui::ET_KEY_PRESSED ||
      !(IsUnhandledArrowKeyEvent(*event) ||
        event->key_code() == ui::VKEY_TAB)) {
    return;
  }

  // Only handle the case when the search notifier has the focus.
  if (!search_notifier_ || !search_notifier_->toast_button()->HasFocus()) {
    return;
  }

  // Left/Right key shouldn't update the focus. Set the event to handled to make
  // left/right keys no-ops.
  if (IsUnhandledLeftRightKeyEvent(*event)) {
    event->SetHandled();
    return;
  }

  bool moving_down =
      (event->key_code() == ui::VKEY_TAB && !event->IsShiftDown()) ||
      event->key_code() == ui::VKEY_DOWN;
  if (moving_down) {
    search_box_view_->EnterSearchResultSelection(*event);
  } else {
    search_box_view_->close_button()->RequestFocus();
  }
  event->SetHandled();
}

void AppListSearchView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (!GetVisible()) {
    return;
  }

  // Set the role of AppListSearchView to ListBox along with notifying value
  // change to "interject" the node announcement before the search result is
  // announced.
  node_data->role = ax::mojom::Role::kListBox;

  std::u16string value;
  const std::u16string& query = search_box_view_->current_query();
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

void AppListSearchView::OnActiveAppListModelsChanged(
    AppListModel* model,
    SearchModel* search_model) {
  for (auto* container : result_container_views_) {
    container->SetResults(search_model->results());
  }
}

bool AppListSearchView::OverrideKeyNavigationAboveSearchResults(
    const ui::KeyEvent& key_event) {
  // The toast button on the search notifier is the only target to check if the
  // `key_event` should be handled. Other events will be handled by either the
  // search box or the SearchBoxViewDelegate.
  if (!search_notifier_) {
    return false;
  }

  search_notifier_->toast_button()->RequestFocus();
  return true;
}

void AppListSearchView::UpdateForNewSearch(bool search_active) {
  for (auto* container : result_container_views_) {
    container->SetActive(search_active);
  }

  if (app_list_features::IsDynamicSearchUpdateAnimationEnabled()) {
    if (search_active) {
      // Scan result_container_views_ to see if there are any in progress
      // animations when the search model is updated.
      for (SearchResultContainerView* view : result_container_views_) {
        if (view->HasAnimatingChildView()) {
          search_result_fast_update_time_ = base::TimeTicks::Now();
        }
      }
    } else {
      search_result_fast_update_time_.reset();
    }
  }
}

void AppListSearchView::RemoveSearchNotifierView() {
  if (!search_notifier_) {
    return;
  }

  auto* scroll_contents = search_notifier_->parent();
  scroll_contents->RemoveChildViewT(std::exchange(search_notifier_, nullptr));
  scroll_contents->InvalidateLayout();
}

void AppListSearchView::OnBoundsChanged(const gfx::Rect& old_bounds) {
  if (image_search_container_ && width() != old_bounds.width()) {
    image_search_container_->ConfigureLayoutForAvailableWidth(width());
  }
}

void AppListSearchView::OnSelectedResultChanged() {
  if (search_notifier_guide_) {
    // Only announce the guidance to the notifier if the selected result is the
    // first available one.
    ui::AXNodeData& notifier_guidance_node_data =
        search_notifier_guide_->GetCustomData();
    if (search_notifier_ && search_box_view_->search_box()->HasFocus() &&
        result_selection_controller_
            ->IsSelectedResultAtFirstAvailableLocation()) {
      notifier_guidance_node_data.RemoveState(ax::mojom::State::kIgnored);
    } else {
      notifier_guidance_node_data.AddState(ax::mojom::State::kIgnored);
    }
  }

  if (!result_selection_controller_->selected_result()) {
    return;
  }

  views::View* selected_row = result_selection_controller_->selected_result();
  selected_row->ScrollViewToVisible();

  for (SearchResultContainerView* view : result_container_views_) {
    view->OnSelectedResultChanged();
  }

  MaybeNotifySelectedResultChanged();
}

void AppListSearchView::SetIgnoreResultChangesForA11y(bool ignore) {
  if (ignore_result_changes_for_a11y_ == ignore) {
    return;
  }
  ignore_result_changes_for_a11y_ = ignore;
  SetViewIgnoredForAccessibility(this, ignore);
}

void AppListSearchView::ScheduleResultsChangedA11yNotification() {
  if (!ignore_result_changes_for_a11y_) {
    NotifyA11yResultsChanged();
    return;
  }

  notify_a11y_results_changed_timer_.Start(
      FROM_HERE, kNotifyA11yDelay,
      base::BindOnce(&AppListSearchView::NotifyA11yResultsChanged,
                     base::Unretained(this)));
}

void AppListSearchView::NotifyA11yResultsChanged() {
  SetIgnoreResultChangesForA11y(false);

  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
  MaybeNotifySelectedResultChanged();
}

void AppListSearchView::MaybeNotifySelectedResultChanged() {
  if (ignore_result_changes_for_a11y_) {
    return;
  }

  if (!result_selection_controller_->selected_result()) {
    search_box_view_->SetA11yActiveDescendant(absl::nullopt);
    return;
  }

  views::View* selected_view =
      result_selection_controller_->selected_result()->GetSelectedView();
  if (!selected_view) {
    search_box_view_->SetA11yActiveDescendant(absl::nullopt);
    return;
  }

  search_box_view_->SetA11yActiveDescendant(
      selected_view->GetViewAccessibility().GetUniqueId().Get());
}

void AppListSearchView::OnSearchNotifierButtonPressed() {
  search_notifier_controller_->SetPrivacyNoticeAcceptedPref();
  RemoveSearchNotifierView();

  // Update the search results as the image search category should be populated
  // now.
  search_box_view()->TriggerSearch();
}

bool AppListSearchView::CanSelectSearchResults() {
  DCHECK(!result_container_views_.empty());
  return last_search_result_count_ > 0;
}

int AppListSearchView::TabletModePreferredHeight() {
  int max_height = 0;
  for (SearchResultContainerView* view : result_container_views_) {
    if (view->GetVisible()) {
      max_height += view->GetPreferredSize().height();
    }
  }
  return max_height;
}

ui::Layer* AppListSearchView::GetPageAnimationLayer() const {
  // The scroll view has a layer containing all the visible contents, so use
  // that for "whole page" animations.
  return scroll_view_->contents()->layer();
}

BEGIN_METADATA(AppListSearchView, views::View)
END_METADATA

}  // namespace ash

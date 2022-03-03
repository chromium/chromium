// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_page_view.h"

#include <stddef.h>

#include <algorithm>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/privacy_container_view.h"
#include "ash/app_list/views/productivity_launcher_search_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_page_anchored_dialog.h"
#include "ash/app_list/views/search_result_tile_item_list_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/public/cpp/view_shadow.h"
#include "ash/search_box/search_box_constants.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/system_shadow.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

namespace {

constexpr int kMinHeight = 440;
constexpr int kWidth = 640;

// The horizontal padding of the separator.
constexpr int kSeparatorPadding = 12;
constexpr int kSeparatorThickness = 1;

// The height of the active search box in this page.
constexpr int kActiveSearchBoxHeight = 56;

// The spacing between search box bottom and separator line.
// Add 1 pixel spacing so that the search bbox bottom will not paint over
// the separator line drawn by SearchResultPageBackground in some scale factors
// due to the round up.
constexpr int kSearchBoxBottomSpacing = 1;

// Minimum spacing between shelf and bottom of search box.
constexpr int kSearchResultPageMinimumBottomMargin = 24;

// The shadow type for the shadow of the expanded search box.
constexpr SystemShadow::Type kSearchBoxSearchResultShadowType =
    SystemShadow::Type::kElevation12;

// The amount of time by which notifications to accessibility framework about
// result page changes are delayed.
constexpr base::TimeDelta kNotifyA11yDelay = base::Milliseconds(1500);

// The duration of the search result page view expanding animation.
constexpr base::TimeDelta kExpandingSearchResultDuration =
    base::Milliseconds(200);

// The duration of the search result page view closing animation.
constexpr base::TimeDelta kClosingSearchResultDuration =
    base::Milliseconds(100);

// The duration of the search result page view decreasing height animation
// within the kExpanded state.
constexpr base::TimeDelta kDecreasingHeightSearchResultsDuration =
    base::Milliseconds(200);

// A container view that ensures the card background and the shadow are painted
// in the correct order.
class SearchCardView : public views::View {
 public:
  METADATA_HEADER(SearchCardView);
  explicit SearchCardView(std::unique_ptr<views::View> content_view) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(std::move(content_view));
  }
  SearchCardView(const SearchCardView&) = delete;
  SearchCardView& operator=(const SearchCardView&) = delete;
  ~SearchCardView() override = default;
};

BEGIN_METADATA(SearchCardView, views::View)
END_METADATA

class ZeroWidthVerticalScrollBar : public views::OverlayScrollBar {
 public:
  ZeroWidthVerticalScrollBar() : OverlayScrollBar(false) {}
  ZeroWidthVerticalScrollBar(const ZeroWidthVerticalScrollBar&) = delete;
  ZeroWidthVerticalScrollBar& operator=(const ZeroWidthVerticalScrollBar&) =
      delete;
  ~ZeroWidthVerticalScrollBar() override = default;

  // OverlayScrollBar overrides:
  int GetThickness() const override { return 0; }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    // Arrow keys should be handled by FocusManager to move focus. When a search
    // result is focused, it will be set visible in scroll view.
    return false;
  }
};

class SearchResultPageBackground : public views::Background {
 public:
  explicit SearchResultPageBackground(SkColor color) {
    SetNativeControlColor(color);
  }
  SearchResultPageBackground(const SearchResultPageBackground&) = delete;
  SearchResultPageBackground& operator=(const SearchResultPageBackground&) =
      delete;
  ~SearchResultPageBackground() override = default;

 private:
  // views::Background overrides:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    canvas->DrawColor(get_color());
    gfx::Rect bounds = view->GetContentsBounds();
    if (bounds.height() <= kActiveSearchBoxHeight)
      return;
    // Draw a separator between SearchBoxView and SearchResultPageView.
    bounds.set_y(kActiveSearchBoxHeight + kSearchBoxBottomSpacing);
    bounds.set_height(kSeparatorThickness);
    canvas->FillRect(bounds, AppListColorProvider::Get()->GetSeparatorColor());
  }
};

}  // namespace

class SearchResultPageView::HorizontalSeparator : public views::View {
 public:
  explicit HorizontalSeparator(int preferred_width)
      : preferred_width_(preferred_width) {
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets(0, kSeparatorPadding, 0, kSeparatorPadding)));
  }

  HorizontalSeparator(const HorizontalSeparator&) = delete;
  HorizontalSeparator& operator=(const HorizontalSeparator&) = delete;

  ~HorizontalSeparator() override = default;

  // views::View overrides:
  const char* GetClassName() const override { return "HorizontalSeparator"; }

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(preferred_width_, kSeparatorThickness);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    gfx::Rect rect = GetContentsBounds();
    canvas->FillRect(rect, AppListColorProvider::Get()->GetSeparatorColor());
    View::OnPaint(canvas);
  }

 private:
  const int preferred_width_;
};

SearchResultPageView::SearchResultPageView() : contents_view_(new views::View) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  contents_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  view_shadow_ = std::make_unique<ViewShadow>(
      this,
      SystemShadow::GetElevationFromType(kSearchBoxSearchResultShadowType));
  view_shadow_->shadow()->SetShadowStyle(gfx::ShadowStyle::kChromeOSSystemUI);
  view_shadow_->SetRoundedCornerRadius(
      kSearchBoxBorderCornerRadiusSearchResult);

  // Hides this view behind the search box by using the same color and
  // background border corner radius. All child views' background should be
  // set transparent so that the rounded corner is not overwritten.
  SetBackground(std::make_unique<SearchResultPageBackground>(
      features::IsProductivityLauncherEnabled()
          ? ColorProvider::Get()->GetBaseLayerColor(
                ColorProvider::BaseLayerType::kTransparent80)
          : AppListColorProvider::Get()->GetSearchBoxCardBackgroundColor()));
  if (features::IsProductivityLauncherEnabled()) {
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
    layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(
        kExpandedSearchBoxCornerRadiusForProductivityLauncher));
  }
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // App list bubble search page has its own scroller and result selection
  // controller so we do not need to construct new ones here.
  if (features::IsProductivityLauncherEnabled()) {
    contents_view_->SetBorder(views::CreateEmptyBorder(gfx::Insets(
        kActiveSearchBoxHeight + kSearchBoxBottomSpacing + kSeparatorThickness,
        0, 0, 0)));
    AddChildView(contents_view_);
  } else {
    auto scroller = std::make_unique<views::ScrollView>();
    // Leaves a placeholder area for the search box and the separator below it.
    scroller->SetBorder(views::CreateEmptyBorder(gfx::Insets(
        kActiveSearchBoxHeight + kSearchBoxBottomSpacing + kSeparatorThickness,
        0, 0, 0)));
    scroller->SetDrawOverflowIndicator(false);
    scroller->SetContents(base::WrapUnique(contents_view_));
    // Setting clip height is necessary to make ScrollView take into account its
    // contents' size. Using zeroes doesn't prevent it from scrolling and sizing
    // correctly.
    scroller->ClipHeightTo(0, 0);
    scroller->SetVerticalScrollBar(
        std::make_unique<ZeroWidthVerticalScrollBar>());
    scroller->SetBackgroundColor(absl::nullopt);
    AddChildView(std::move(scroller));

    result_selection_controller_ = std::make_unique<ResultSelectionController>(
        &result_container_views_,
        base::BindRepeating(&SearchResultPageView::SelectedResultChanged,
                            base::Unretained(this)));
  }

  AppListModelProvider* const model_provider = AppListModelProvider::Get();
  model_provider->AddObserver(this);
  search_box_observation_.Observe(model_provider->search_model()->search_box());
}

SearchResultPageView::~SearchResultPageView() {
  AppListModelProvider::Get()->RemoveObserver(this);
}

void SearchResultPageView::InitializeContainers(
    AppListViewDelegate* view_delegate,
    AppListMainView* app_list_main_view,
    SearchBoxView* search_box_view) {
  DCHECK(view_delegate);
  view_delegate_ = view_delegate;
  dialog_controller_ = std::make_unique<SearchResultPageDialogController>(this);

  if (features::IsProductivityLauncherEnabled()) {
    std::unique_ptr<ProductivityLauncherSearchView> search_view_ptr =
        std::make_unique<ProductivityLauncherSearchView>(
            view_delegate, dialog_controller_.get(), search_box_view);
    productivity_launcher_search_view_ = search_view_ptr.get();
    contents_view_->AddChildView(
        std::make_unique<SearchCardView>(std::move(search_view_ptr)));
  } else {
    privacy_container_view_ = AddSearchResultContainerView(
        std::make_unique<PrivacyContainerView>(view_delegate));
    search_result_tile_item_list_view_ = AddSearchResultContainerView(
        std::make_unique<SearchResultTileItemListView>(
            search_box_view->search_box(), view_delegate));
    result_lists_separator_ = contents_view_->AddChildView(
        std::make_unique<HorizontalSeparator>(bounds().width()));
    // productivity_launcher_index is not set as the feature is not enabled.
    search_result_list_view_ =
        AddSearchResultContainerView(std::make_unique<SearchResultListView>(
            app_list_main_view, view_delegate, dialog_controller_.get(),
            SearchResultView::SearchResultViewType::kClassic,
            /*animates_result_updates=*/false, absl::nullopt));

    search_box_view->SetResultSelectionController(
        result_selection_controller());
  }
}

void SearchResultPageView::AddSearchResultContainerViewInternal(
    std::unique_ptr<SearchResultContainerView> result_container) {
  auto* result_container_ptr = result_container.get();
  contents_view_->AddChildView(
      std::make_unique<SearchCardView>(std::move(result_container)));
  result_container_views_.push_back(result_container_ptr);
  result_container_ptr->SetResults(
      AppListModelProvider::Get()->search_model()->results());
  result_container_ptr->set_delegate(this);
}

bool SearchResultPageView::IsFirstResultTile() const {
  // In the event that the result does not exist, it is not a tile.
  if (!first_result_view_ || !first_result_view_->result())
    return false;

  return first_result_view_->result()->display_type() ==
         SearchResultDisplayType::kTile;
}

bool SearchResultPageView::IsFirstResultHighlighted() const {
  DCHECK(first_result_view_);
  return first_result_view_->selected();
}

const char* SearchResultPageView::GetClassName() const {
  return "SearchResultPageView";
}

gfx::Size SearchResultPageView::CalculatePreferredSize() const {
  // TODO(https://crbug.com/1216097) Update height based on available space.
  if (!features::IsProductivityLauncherEnabled())
    return gfx::Size(kWidth, kMinHeight);
  int adjusted_height = std::min(
      std::max(kMinHeight,
               productivity_launcher_search_view_->TabletModePreferredHeight() +
                   kActiveSearchBoxHeight + kSearchBoxBottomSpacing +
                   kSeparatorThickness),
      AppListPage::contents_view()->height());
  return gfx::Size(kWidth, adjusted_height);
}

void SearchResultPageView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (previous_bounds.size() != bounds().size()) {
    // If the clip rect is currently animating, then animate from the current
    // clip rect bounds to the newly set bounds.
    if (features::IsProductivityLauncherEnabled() &&
        layer()->GetAnimator()->is_animating()) {
      AnimateBetweenBounds(layer()->clip_rect(), gfx::Rect(bounds().size()));
      return;
    }

    // The clip rect set for page state animations needs to be reset when the
    // bounds change because page size change invalidates the previous bounds.
    // This allows content to properly follow target bounds when screen
    // rotates.
    layer()->SetClipRect(gfx::Rect());
  }
}

void SearchResultPageView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (!GetVisible())
    return;

  // With productivity launcher enabled, individual child result list views will
  // have the list box role.
  if (!features::IsProductivityLauncherEnabled())
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
    // TODO(https://crbug.com/1216097) Zero state is removed for bubble launcher
    // so we need a new A11y announcement.
    value = l10n_util::GetStringUTF16(
        IDS_APP_LIST_SEARCHBOX_RESULTS_ACCESSIBILITY_ANNOUNCEMENT_ZERO_STATE);
  }

  node_data->SetValue(value);
}

void SearchResultPageView::OnThemeChanged() {
  AppListPage::OnThemeChanged();
  GetBackground()->SetNativeControlColor(
      AppListColorProvider::Get()->GetSearchBoxCardBackgroundColor());
}

void SearchResultPageView::UpdateResultContainersVisibility() {
  bool should_show_page_view = false;
  if (features::IsProductivityLauncherEnabled()) {
    should_show_page_view = ShouldShowSearchResultView();
    AnimateToSearchResultsState(should_show_page_view
                                    ? SearchResultsState::kExpanded
                                    : SearchResultsState::kActive);
  } else {
    bool should_show_search_result_view = ShouldShowSearchResultView();
    for (auto* container : result_container_views_) {
      // Containers are wrapped by a `SearchCardView`, so update the parent
      // visibility.
      bool should_show_container_view =
          container->num_results() && should_show_search_result_view;
      container->parent()->SetVisible(should_show_container_view);
      container->SetVisible(should_show_container_view);
      should_show_page_view =
          should_show_page_view || should_show_container_view;
    }

    result_lists_separator_->SetVisible(
        search_result_tile_item_list_view_->num_results() &&
        search_result_list_view_->num_results() &&
        ShouldShowSearchResultView());
    AppListPage::contents_view()
        ->GetSearchBoxView()
        ->OnResultContainerVisibilityChanged(should_show_page_view);
  }
  Layout();
}

void SearchResultPageView::SelectedResultChanged() {
  // Result selection should be handled by |productivity_launcher_search_page_|.
  DCHECK(!features::IsProductivityLauncherEnabled());
  if (!result_selection_controller_->selected_location_details() ||
      !result_selection_controller_->selected_result()) {
    return;
  }

  const ResultLocationDetails* selection_details =
      result_selection_controller_->selected_location_details();
  views::View* selected_row = nullptr;
  // For horizontal containers ensure that the whole container fits in the
  // scroll view, to account for vertical padding within the container.
  if (selection_details->container_is_horizontal) {
    selected_row = result_container_views_[selection_details->container_index];
  } else {
    selected_row = result_selection_controller_->selected_result();
  }

  selected_row->ScrollViewToVisible();

  NotifySelectedResultChanged();
}

void SearchResultPageView::SetIgnoreResultChangesForA11y(bool ignore) {
  if (ignore_result_changes_for_a11y_ == ignore)
    return;
  ignore_result_changes_for_a11y_ = ignore;

  GetViewAccessibility().OverrideIsLeaf(ignore);
  GetViewAccessibility().OverrideIsIgnored(ignore);
  NotifyAccessibilityEvent(ax::mojom::Event::kTreeChanged, true);
}

void SearchResultPageView::ScheduleResultsChangedA11yNotification() {
  if (!ignore_result_changes_for_a11y_) {
    NotifyA11yResultsChanged();
    return;
  }

  notify_a11y_results_changed_timer_.Start(
      FROM_HERE, kNotifyA11yDelay,
      base::BindOnce(&SearchResultPageView::NotifyA11yResultsChanged,
                     base::Unretained(this)));
}

void SearchResultPageView::NotifyA11yResultsChanged() {
  SetIgnoreResultChangesForA11y(false);

  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
  NotifySelectedResultChanged();
}

void SearchResultPageView::NotifySelectedResultChanged() {
  // Result selection should be handled by |productivity_launcher_search_page_|.
  DCHECK(!features::IsProductivityLauncherEnabled());
  if (ignore_result_changes_for_a11y_)
    return;

  SearchBoxView* search_box = AppListPage::contents_view()->GetSearchBoxView();
  if (!result_selection_controller_->selected_location_details() ||
      !result_selection_controller_->selected_result()) {
    search_box->SetA11yActiveDescendant(absl::nullopt);
    return;
  }

  views::View* selected_view =
      result_selection_controller_->selected_result()->GetSelectedView();
  if (!selected_view) {
    search_box->SetA11yActiveDescendant(absl::nullopt);
    return;
  }

  search_box->SetA11yActiveDescendant(
      selected_view->GetViewAccessibility().GetUniqueId().Get());
}

void SearchResultPageView::UpdatePageBoundsForState(
    AppListState state,
    const gfx::Rect& contents_bounds,
    const gfx::Rect& search_box_bounds) {
  if (!features::IsProductivityLauncherEnabled()) {
    AppListPage::UpdatePageBoundsForState(state, contents_bounds,
                                          search_box_bounds);
    return;
  }

  if (state != AppListState::kStateSearchResults)
    return;

  const gfx::Rect to_bounds =
      GetPageBoundsForResultState(current_search_results_state_);

  if (layer()->GetAnimator()->is_animating()) {
    DCHECK(!layer()->clip_rect().IsEmpty());
    // When already animating, for an increasing target height, set the bounds
    // before animating to keep the animation visible.
    if (to_bounds.height() > layer()->clip_rect().height())
      SetBoundsRect(to_bounds);
    AnimateBetweenBounds(layer()->clip_rect(), gfx::Rect(to_bounds.size()));
  } else {
    // When no animation is in progress, we only animate when the target
    // height is decreasing, otherwise set bounds immediately.
    if (to_bounds.height() < bounds().height()) {
      AnimateBetweenBounds(gfx::Rect(bounds().size()),
                           gfx::Rect(to_bounds.size()));
    } else {
      SetBoundsRect(to_bounds);
    }
  }
}

void SearchResultPageView::AnimateToSearchResultsState(
    SearchResultsState to_state) {
  // The search results page is only visible in expanded state. Exit early when
  // transitioning between states where results UI is invisible.
  if (current_search_results_state_ != SearchResultsState::kExpanded &&
      to_state != SearchResultsState::kExpanded) {
    SetVisible(false);
    current_search_results_state_ = to_state;
    return;
  }

  gfx::Rect from_rect =
      GetPageBoundsForResultState(current_search_results_state_);
  const gfx::Rect to_rect = GetPageBoundsForResultState(to_state);

  if (current_search_results_state_ == SearchResultsState::kExpanded &&
      to_state == SearchResultsState::kExpanded) {
    // Use current bounds when animating within the expanded state.
    from_rect = bounds();

    // Only set bounds when the height is increasing so that the entire
    // animation between |to_rect| and |from_rect| is visible.
    if (to_rect.height() > from_rect.height())
      SetBoundsRect(to_rect);

  } else if (to_state == SearchResultsState::kExpanded) {
    // Set bounds here because this is a result opening transition. We avoid
    // setting bounds for closing transitions because then the animation would
    // be hidden, instead set the bounds for closing transitions once the
    // animation has completed.
    SetBoundsRect(to_rect);
    AppListPage::contents_view()
        ->GetSearchBoxView()
        ->OnResultContainerVisibilityChanged(true);
  }

  current_search_results_state_ = to_state;
  AnimateBetweenBounds(from_rect, to_rect);
}

void SearchResultPageView::AnimateBetweenBounds(const gfx::Rect& from_rect,
                                                const gfx::Rect& to_rect) {
  if (from_rect == to_rect)
    return;

  // Return if already animating to the correct target size.
  if (layer()->GetAnimator()->is_animating() &&
      to_rect.size() == layer()->GetTargetClipRect().size()) {
    return;
  }

  gfx::Rect clip_rect = from_rect;
  clip_rect -= to_rect.OffsetFromOrigin();
  layer()->SetClipRect(clip_rect);
  view_shadow_.reset();

  base::TimeDelta duration;
  if (from_rect.height() < to_rect.height()) {
    duration = kExpandingSearchResultDuration;
  } else {
    duration = (current_search_results_state_ == SearchResultsState::kExpanded)
                   ? kDecreasingHeightSearchResultsDuration
                   : kClosingSearchResultDuration;
  }

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(
          base::BindOnce(&SearchResultPageView::OnAnimationBetweenBoundsEnded,
                         base::Unretained(this)))
      .Once()
      .SetDuration(duration)
      .SetClipRect(layer(), gfx::Rect(to_rect.size()),
                   gfx::Tween::FAST_OUT_SLOW_IN)
      .SetRoundedCorners(
          layer(),
          gfx::RoundedCornersF(GetCornerRadiusForSearchResultsState(
              current_search_results_state_)),
          gfx::Tween::FAST_OUT_SLOW_IN);
}

void SearchResultPageView::OnAnimationBetweenBoundsEnded() {
  view_shadow_ = std::make_unique<ViewShadow>(
      this,
      SystemShadow::GetElevationFromType(kSearchBoxSearchResultShadowType));
  view_shadow_->shadow()->SetShadowStyle(gfx::ShadowStyle::kChromeOSSystemUI);
  view_shadow_->SetRoundedCornerRadius(
      GetCornerRadiusForSearchResultsState(current_search_results_state_));

  // To keep the animation visible for closing transitions from expanded search
  // results, bounds are set here once the animation completes.
  SetBoundsRect(GetPageBoundsForResultState(current_search_results_state_));

  // Avoid visible overlap with the search box when the search results are not
  // expanded.
  if (current_search_results_state_ != SearchResultsState::kExpanded) {
    SetVisible(false);
    AppListPage::contents_view()
        ->GetSearchBoxView()
        ->OnResultContainerVisibilityChanged(false);
  }
}

gfx::Rect SearchResultPageView::GetPageBoundsForResultState(
    SearchResultsState state) const {
  AppListState app_list_state = (state == SearchResultsState::kClosed)
                                    ? AppListState::kStateApps
                                    : AppListState::kStateSearchResults;
  const ContentsView* const contents_view = AppListPage::contents_view();
  const gfx::Rect contents_bounds = contents_view->GetContentsBounds();

  gfx::Rect final_bounds =
      GetPageBoundsForState(app_list_state, contents_bounds,
                            contents_view->GetSearchBoxBounds(app_list_state));

  // Ensure the height is set according to |state|, because
  // GetPageBoundForState() returns a height according to |app_list_state| which
  // does not account for kActive search result state.
  if (state == SearchResultsState::kActive)
    final_bounds.set_height(kActiveSearchBoxHeight);

  return final_bounds;
}

int SearchResultPageView::GetCornerRadiusForSearchResultsState(
    SearchResultsState state) {
  switch (state) {
    case SearchResultsState::kClosed:
      return kSearchBoxBorderCornerRadius;
    case SearchResultsState::kActive:
      return kExpandedSearchBoxCornerRadiusForProductivityLauncher;
    case SearchResultsState::kExpanded:
      return kExpandedSearchBoxCornerRadiusForProductivityLauncher;
  }
}

void SearchResultPageView::OnActiveAppListModelsChanged(
    AppListModel* model,
    SearchModel* search_model) {
  search_box_observation_.Reset();
  search_box_observation_.Observe(search_model->search_box());
  for (auto* container : result_container_views_)
    container->SetResults(search_model->results());
}

void SearchResultPageView::OnSearchResultContainerResultsChanging() {
  // Result selection should be handled by |productivity_launcher_search_page_|.
  DCHECK(!features::IsProductivityLauncherEnabled());
  // Block any result selection changes while result updates are in flight.
  // The selection will be reset once the results are all updated.
  result_selection_controller_->set_block_selection_changes(true);

  notify_a11y_results_changed_timer_.Stop();
  SetIgnoreResultChangesForA11y(true);
}

void SearchResultPageView::OnSearchResultContainerResultsChanged() {
  // Skip updates during shutdown.
  if (!view_delegate_->HasValidProfile())
    return;

  // Result selection should be handled by |productivity_launcher_search_page_|.
  DCHECK(!features::IsProductivityLauncherEnabled());
  DCHECK(!result_container_views_.empty());

  int result_count = 0;
  // Only sort and layout the containers when they have all updated.
  for (SearchResultContainerView* view : result_container_views_) {
    if (view->UpdateScheduled())
      return;
    result_count += view->num_results();
  }

  last_search_result_count_ = result_count;

  UpdateResultContainersVisibility();

  ScheduleResultsChangedA11yNotification();

  // Find the first result view.
  first_result_view_ = nullptr;
  for (auto* container : result_container_views_) {
    first_result_view_ = container->GetFirstResultView();
    if (first_result_view_)
      break;
  }

  // Reset selection to first when things change. The first result is set as
  // as the default result.
  result_selection_controller_->set_block_selection_changes(false);
  result_selection_controller_->ResetSelection(nullptr /*key_event*/,
                                               true /* default_selection */);
  // Update SearchBoxView search box autocomplete as necessary based on new
  // first result view.
  AppListPage::contents_view()->GetSearchBoxView()->ProcessAutocomplete(
      first_result_view_);
}

void SearchResultPageView::Update() {
  notify_a11y_results_changed_timer_.Stop();
}

void SearchResultPageView::SearchEngineChanged() {}

void SearchResultPageView::ShowAssistantChanged() {}

bool SearchResultPageView::CanSelectSearchResults() const {
  if (!GetVisible())
    return false;

  if (features::IsProductivityLauncherEnabled())
    return productivity_launcher_search_view_->CanSelectSearchResults();

  return first_result_view_;
}

SkColor SearchResultPageView::GetBackgroundColorForState(
    AppListState state) const {
  if (state == AppListState::kStateSearchResults)
    return AppListColorProvider::Get()->GetSearchBoxCardBackgroundColor();
  return AppListColorProvider::Get()->GetSearchBoxBackgroundColor();
}

PrivacyContainerView* SearchResultPageView::GetPrivacyContainerViewForTest() {
  return privacy_container_view_;
}

SearchResultTileItemListView*
SearchResultPageView::GetSearchResultTileItemListViewForTest() {
  return search_result_tile_item_list_view_;
}

SearchResultListView* SearchResultPageView::GetSearchResultListViewForTest() {
  return search_result_list_view_;
}

bool SearchResultPageView::ShouldShowSearchResultView() const {
  SearchModel* search_model = AppListModelProvider::Get()->search_model();
  return (!features::IsProductivityLauncherEnabled() ||
          !base::TrimWhitespace(search_model->search_box()->text(),
                                base::TrimPositions::TRIM_ALL)
               .empty());
}

void SearchResultPageView::OnHidden() {
  // Hide the search results page when it is behind search box to avoid focus
  // being moved onto suggested apps when zero state is enabled.
  AppListPage::OnHidden();
  notify_a11y_results_changed_timer_.Stop();
  dialog_controller_->SetEnabled(false);
  SetVisible(false);
  for (auto* container_view : result_container_views_) {
    container_view->SetShown(false);
  }

  AppListPage::contents_view()
      ->GetSearchBoxView()
      ->OnResultContainerVisibilityChanged(false);
}

void SearchResultPageView::OnShown() {
  AppListPage::OnShown();

  dialog_controller_->SetEnabled(true);

  for (auto* container_view : result_container_views_) {
    container_view->SetShown(ShouldShowSearchResultView());
  }

  AppListPage::contents_view()
      ->GetSearchBoxView()
      ->OnResultContainerVisibilityChanged(ShouldShowSearchResultView());
  if (!features::IsProductivityLauncherEnabled())
    ScheduleResultsChangedA11yNotification();
}

void SearchResultPageView::AnimateYPosition(AppListViewState target_view_state,
                                            const TransformAnimator& animator,
                                            float default_offset) {
  // Search result page view may host a native view to show answer card results.
  // The native view hosts use view to widget coordinate conversion to calculate
  // the native view bounds, and thus depend on the view transform values.
  // Make sure the view is laid out before starting the transform animation so
  // native views are not placed according to interim, animated page transform
  // value.
  layer()->GetAnimator()->StopAnimatingProperty(
      ui::LayerAnimationElement::TRANSFORM);
  if (needs_layout())
    Layout();

  animator.Run(default_offset, layer());
  if (view_shadow_)
    animator.Run(default_offset, view_shadow_->shadow()->shadow_layer());
  SearchResultPageAnchoredDialog* search_page_dialog =
      dialog_controller_->dialog();
  if (search_page_dialog) {
    const float offset =
        search_page_dialog->AdjustVerticalTransformOffset(default_offset);
    animator.Run(offset, search_page_dialog->widget()->GetLayer());
  }
}

void SearchResultPageView::UpdatePageOpacityForState(AppListState state,
                                                     float search_box_opacity,
                                                     bool restore_opacity) {
  layer()->SetOpacity(search_box_opacity);
}

gfx::Rect SearchResultPageView::GetPageBoundsForState(
    AppListState state,
    const gfx::Rect& contents_bounds,
    const gfx::Rect& search_box_bounds) const {
  if (state != AppListState::kStateSearchResults) {
    // Hides this view behind the search box by using the same bounds.
    return search_box_bounds;
  }

  gfx::Rect bounding_rect = contents_bounds;
  bounding_rect.Inset(0, 0, 0, kSearchResultPageMinimumBottomMargin);

  gfx::Rect preferred_bounds = gfx::Rect(
      search_box_bounds.origin(),
      gfx::Size(search_box_bounds.width(), CalculatePreferredSize().height()));
  preferred_bounds.Intersect(bounding_rect);

  return preferred_bounds;
}

void SearchResultPageView::OnAnimationStarted(AppListState from_state,
                                              AppListState to_state) {
  if (from_state != AppListState::kStateSearchResults &&
      to_state != AppListState::kStateSearchResults) {
    return;
  }

  if (features::IsProductivityLauncherEnabled()) {
    SearchResultsState to_result_state;
    if (to_state == AppListState::kStateApps) {
      to_result_state = SearchResultsState::kClosed;
    } else {
      to_result_state = ShouldShowSearchResultView()
                            ? SearchResultsState::kExpanded
                            : SearchResultsState::kActive;
    }

    AnimateToSearchResultsState(to_result_state);
  } else {
    const ContentsView* const contents_view = AppListPage::contents_view();
    const gfx::Rect contents_bounds = contents_view->GetContentsBounds();
    const gfx::Rect from_rect =
        GetPageBoundsForState(from_state, contents_bounds,
                              contents_view->GetSearchBoxBounds(from_state));
    const gfx::Rect to_rect = GetPageBoundsForState(
        to_state, contents_bounds, contents_view->GetSearchBoxBounds(to_state));
    if (from_rect == to_rect)
      return;

    const int to_radius =
        contents_view->GetSearchBoxView()
            ->GetSearchBoxBorderCornerRadiusForState(to_state);

    // Here does the following animations;
    // - clip-rect, so it looks like expanding from |from_rect| to |to_rect|.
    // - rounded-rect
    // - transform of the shadow
    SetBoundsRect(to_rect);
    gfx::Rect clip_rect = from_rect;
    clip_rect -= to_rect.OffsetFromOrigin();
    layer()->SetClipRect(clip_rect);
    {
      auto settings = contents_view->CreateTransitionAnimationSettings(layer());
      layer()->SetClipRect(gfx::Rect(to_rect.size()));
      // This changes the shadow's corner immediately while this corner bounds
      // gradually. This would be fine because this would be unnoticeable to
      // users.
      view_shadow_->SetRoundedCornerRadius(to_radius);
    }

    // Animate the shadow's bounds through transform.
    {
      gfx::Transform transform;
      transform.Translate(from_rect.origin() - to_rect.origin());
      transform.Scale(
          static_cast<float>(from_rect.width()) / to_rect.width(),
          static_cast<float>(from_rect.height()) / to_rect.height());
      view_shadow_->shadow()->layer()->SetTransform(transform);

      auto settings = contents_view->CreateTransitionAnimationSettings(
          view_shadow_->shadow()->layer());
      view_shadow_->shadow()->layer()->SetTransform(gfx::Transform());
    }
  }
}

void SearchResultPageView::OnAnimationUpdated(double progress,
                                              AppListState from_state,
                                              AppListState to_state) {
  if (from_state != AppListState::kStateSearchResults &&
      to_state != AppListState::kStateSearchResults) {
    return;
  }
  const SkColor color = gfx::Tween::ColorValueBetween(
      progress, GetBackgroundColorForState(from_state),
      GetBackgroundColorForState(to_state));

  if (color != background()->get_color()) {
    background()->SetNativeControlColor(color);
    SchedulePaint();
  }
}

gfx::Size SearchResultPageView::GetPreferredSearchBoxSize() const {
  static gfx::Size size = gfx::Size(kWidth, kActiveSearchBoxHeight);
  return size;
}

}  // namespace ash

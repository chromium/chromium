// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_page_view.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "ash/app_list/views/app_list_search_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/search_box/search_box_constants.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/system_shadow.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

constexpr int kMinHeight = 440;
constexpr int kWidth = 640;

// The height of the active search box in this page.
constexpr int kActiveSearchBoxHeight = 56;

// Minimum spacing between shelf and bottom of search box.
constexpr int kSearchResultPageMinimumBottomMargin = 24;

// The shadow type for the shadow of the expanded search box.
constexpr SystemShadow::Type kSearchBoxSearchResultShadowType =
    SystemShadow::Type::kElevation12;

// The duration of the search result page view expanding animation.
constexpr base::TimeDelta kExpandingSearchResultDuration =
    base::Milliseconds(200);

// The duration of the search result page view going from expanded to active.
constexpr base::TimeDelta kExpandedToActiveSearchResultDuration =
    base::Milliseconds(100);

// The duration of the search result page view going from expanded to closed.
constexpr base::TimeDelta kExpandedToClosedSearchResultDuration =
    base::Milliseconds(250);

// The duration of the search result page view decreasing height animation
// within the kExpanded state.
constexpr base::TimeDelta kDecreasingHeightSearchResultsDuration =
    base::Milliseconds(200);

// A container view that ensures the card background and the shadow are painted
// in the correct order.
class SearchCardView : public views::View {
  METADATA_HEADER(SearchCardView, views::View)

 public:
  explicit SearchCardView(std::unique_ptr<views::View> content_view) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(std::move(content_view));
  }
  SearchCardView(const SearchCardView&) = delete;
  SearchCardView& operator=(const SearchCardView&) = delete;
  ~SearchCardView() override = default;
};

BEGIN_METADATA(SearchCardView)
END_METADATA

}  // namespace

SearchResultPageView::SearchResultPageView() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kActiveSearchBoxHeight, 0, 0, 0)));
  shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
      this, kSearchBoxSearchResultShadowType);
  shadow_->SetRoundedCornerRadius(kSearchBoxBorderCornerRadiusSearchResult);

  // Hides this view behind the search box by using the same color and
  // background border corner radius. All child views' background should be
  // set transparent so that the rounded corner is not overwritten.
  SetBackground(views::CreateThemedSolidBackground(kColorAshShieldAndBase80));
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kExpandedSearchBoxCornerRadius));
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

SearchResultPageView::~SearchResultPageView() = default;

void SearchResultPageView::InitializeContainers(
    AppListViewDelegate* view_delegate,
    SearchBoxView* search_box_view) {
  DCHECK(view_delegate);

  // For productivity launcher, the dialog will be anchored to the search box
  // to keep the position of dialogs consistent.
  dialog_controller_ =
      std::make_unique<SearchResultPageDialogController>(search_box_view);
  std::unique_ptr<AppListSearchView> search_view_ptr =
      std::make_unique<AppListSearchView>(
          view_delegate, dialog_controller_.get(), search_box_view);
  search_view_ = search_view_ptr.get();
  AddChildView(std::make_unique<SearchCardView>(std::move(search_view_ptr)));
}

gfx::Size SearchResultPageView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  int adjusted_height =
      std::min(std::max(kMinHeight, search_view_->TabletModePreferredHeight() +
                                        kActiveSearchBoxHeight +
                                        kExpandedSearchBoxCornerRadius),
               contents_view()->height());
  return gfx::Size(kWidth, adjusted_height);
}

void SearchResultPageView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (previous_bounds.size() == bounds().size())
    return;

  // If the clip rect is currently animating, then animate from the current
  // clip rect bounds to the newly set bounds.
  if (layer()->GetAnimator()->is_animating()) {
    AnimateBetweenBounds(layer()->clip_rect(), gfx::Rect(bounds().size()));
    return;
  }

  // The clip rect set for page state animations needs to be reset when the
  // bounds change because page size change invalidates the previous bounds.
  // This allows content to properly follow target bounds when screen
  // rotates.
  layer()->SetClipRect(gfx::Rect());
}

void SearchResultPageView::UpdateForNewSearch() {
  search_view_->UpdateForNewSearch(ShouldShowSearchResultView());
}

void SearchResultPageView::UpdateResultContainersVisibility() {
  AnimateToSearchResultsState(ShouldShowSearchResultView()
                                  ? SearchResultsState::kExpanded
                                  : SearchResultsState::kActive);
  DeprecatedLayoutImmediately();
}

void SearchResultPageView::UpdatePageBoundsForState(
    AppListState state,
    const gfx::Rect& contents_bounds,
    const gfx::Rect& search_box_bounds) {
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
    contents_view()->GetSearchBoxView()->OnResultContainerVisibilityChanged(
        true);
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

  const bool is_expanding = from_rect.height() < to_rect.height();
  gfx::Rect clip_rect;
  gfx::Rect to_clip_rect;

  // The clip rects will always be located relative to the view bounds current
  // OffsetFromOrigin(). To ensure the animation is not cutoff by the view
  // bounds, the view bounds will equal the larger of `from_rect` and
  // `to_rect`. Because of this, calculate the clip rects so that their 0,0
  // origin is located at the offset of the wider input bounds (widest between
  // `from_rect` and `to_rect`).
  if (is_expanding) {
    clip_rect = from_rect - to_rect.OffsetFromOrigin();
    to_clip_rect = gfx::Rect(to_rect.size());
  } else {
    clip_rect = gfx::Rect(from_rect.size());
    to_clip_rect = to_rect - from_rect.OffsetFromOrigin();
  }
  layer()->SetClipRect(clip_rect);
  shadow_.reset();

  base::TimeDelta duration;
  switch (current_search_results_state_) {
    case SearchResultsState::kExpanded:
      duration = is_expanding ? kExpandingSearchResultDuration
                              : kDecreasingHeightSearchResultsDuration;
      break;
    case SearchResultsState::kActive:
      duration = kExpandedToActiveSearchResultDuration;
      break;
    case SearchResultsState::kClosed:
      duration = kExpandedToClosedSearchResultDuration;
      break;
  }

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(
          base::BindOnce(&SearchResultPageView::OnAnimationBetweenBoundsEnded,
                         base::Unretained(this)))
      .Once()
      .SetDuration(duration)
      .SetClipRect(layer(), to_clip_rect, gfx::Tween::FAST_OUT_SLOW_IN)
      .SetRoundedCorners(
          layer(),
          gfx::RoundedCornersF(GetCornerRadiusForSearchResultsState(
              current_search_results_state_)),
          gfx::Tween::FAST_OUT_SLOW_IN);
}

void SearchResultPageView::OnAnimationBetweenBoundsEnded() {
  shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
      this, kSearchBoxSearchResultShadowType);
  shadow_->SetRoundedCornerRadius(
      GetCornerRadiusForSearchResultsState(current_search_results_state_));

  // To keep the animation visible for closing transitions from expanded search
  // results, bounds are set here once the animation completes.
  SetBoundsRect(GetPageBoundsForResultState(current_search_results_state_));

  // Avoid visible overlap with the search box when the search results are not
  // expanded.
  if (current_search_results_state_ != SearchResultsState::kExpanded) {
    SetVisible(false);
    contents_view()->GetSearchBoxView()->OnResultContainerVisibilityChanged(
        false);
  }
}

gfx::Rect SearchResultPageView::GetPageBoundsForResultState(
    SearchResultsState state) const {
  AppListState app_list_state = (state == SearchResultsState::kClosed)
                                    ? AppListState::kStateApps
                                    : AppListState::kStateSearchResults;
  const gfx::Rect contents_bounds = contents_view()->GetContentsBounds();

  gfx::Rect final_bounds = GetPageBoundsForState(
      app_list_state, contents_bounds,
      contents_view()->GetSearchBoxBounds(app_list_state));

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
    case SearchResultsState::kExpanded:
      return kExpandedSearchBoxCornerRadius;
  }
}

bool SearchResultPageView::CanSelectSearchResults() const {
  if (!GetVisible())
    return false;

  return search_view_->CanSelectSearchResults();
}

bool SearchResultPageView::ShouldShowSearchResultView() const {
  return contents_view()->GetSearchBoxView()->HasValidQuery();
}

void SearchResultPageView::OnHidden() {
  // Hide the search results page when it is behind search box to avoid focus
  // being moved onto suggested apps when zero state is enabled.
  AppListPage::OnHidden();
  dialog_controller_->Reset(false);
  SetVisible(false);

  contents_view()->GetSearchBoxView()->OnResultContainerVisibilityChanged(
      false);
}

void SearchResultPageView::OnShown() {
  AppListPage::OnShown();

  dialog_controller_->Reset(true);

  contents_view()->GetSearchBoxView()->OnResultContainerVisibilityChanged(
      ShouldShowSearchResultView());
}

void SearchResultPageView::UpdatePageOpacityForState(AppListState state,
                                                     float search_box_opacity) {
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
  bounding_rect.Inset(
      gfx::Insets::TLBR(0, 0, kSearchResultPageMinimumBottomMargin, 0));

  gfx::Rect preferred_bounds =
      gfx::Rect(search_box_bounds.origin(),
                gfx::Size(search_box_bounds.width(),
                          CalculatePreferredSize({}).height()));
  preferred_bounds.Intersect(bounding_rect);

  return preferred_bounds;
}

void SearchResultPageView::OnAnimationStarted(AppListState from_state,
                                              AppListState to_state) {
  if (from_state != AppListState::kStateSearchResults &&
      to_state != AppListState::kStateSearchResults) {
    return;
  }

  SearchResultsState to_result_state;
  if (to_state == AppListState::kStateApps) {
    to_result_state = SearchResultsState::kClosed;
  } else {
    to_result_state = ShouldShowSearchResultView()
                          ? SearchResultsState::kExpanded
                          : SearchResultsState::kActive;
  }

  AnimateToSearchResultsState(to_result_state);
}

gfx::Size SearchResultPageView::GetPreferredSearchBoxSize() const {
  auto* iph_view = search_view_->search_box_view()->GetIphView();
  const int iph_height = iph_view ? iph_view->GetPreferredSize().height() : 0;

  return gfx::Size(kWidth, kActiveSearchBoxHeight + iph_height);
}

BEGIN_METADATA(SearchResultPageView)
END_METADATA

}  // namespace ash

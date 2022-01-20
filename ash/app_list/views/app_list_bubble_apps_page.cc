// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_apps_page.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/views/app_list_reorder_undo_container_view.h"
#include "ash/app_list/views/app_list_view_util.h"
#include "ash/app_list/views/continue_section_view.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/constants/ash_features.h"
#include "ash/controls/rounded_scroll_bar.h"
#include "ash/controls/scroll_view_gradient_helper.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/style/color_provider.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

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

// A slide animation's duration.
constexpr base::TimeDelta kSlideAnimationDuration = base::Milliseconds(250);

// A slide animation's tween type.
constexpr gfx::Tween::Type kSlideAnimationTweenType =
    gfx::Tween::LINEAR_OUT_SLOW_IN;

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

  AppListModelProvider::Get()->AddObserver(this);

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

  // Scroll view will have a gradient mask layer.
  scroll_view_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  // When animations are enabled the gradient helper is created in the animation
  // end callback.
  if (!features::IsProductivityLauncherAnimationEnabled()) {
    gradient_helper_ = std::make_unique<ScrollViewGradientHelper>(scroll_view_);
    // Layout() updates the gradient zone, since the gradient helper needs to
    // know the bounds of the scroll view and contents view.
  }

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
          view_delegate, kContinueColumnCount, /*tablet_mode=*/false));
  // Observe changes in continue section visibility, to keep separator
  // visibility in sync.
  continue_section_->AddObserver(this);

  // Recent apps row.
  recent_apps_ = scroll_contents->AddChildView(
      std::make_unique<RecentAppsView>(this, view_delegate));
  recent_apps_->UpdateAppListConfig(app_list_config);
  // Observe changes in continue section visibility, to keep separator
  // visibility in sync.
  recent_apps_->AddObserver(this);

  // Horizontal separator.
  separator_ =
      scroll_contents->AddChildView(std::make_unique<views::Separator>());
  separator_->SetBorder(views::CreateEmptyBorder(kSeparatorInsets));
  separator_->SetColor(ColorProvider::Get()->GetContentLayerColor(
      ColorProvider::ContentLayerType::kSeparatorColor));

  // Add a empty container view. A toast view should be added to
  // `reorder_undo_container_` when the app list starts temporary sorting.
  if (features::IsLauncherAppSortEnabled()) {
    reorder_undo_container_ = scroll_contents->AddChildView(
        std::make_unique<AppListReorderUndoContainerView>(
            /*tablet_mode=*/false));
  }

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
  AppListModel* const model = AppListModelProvider::Get()->model();
  scrollable_apps_grid_view_->SetModel(model);
  scrollable_apps_grid_view_->SetItemList(model->top_level_item_list());
  scrollable_apps_grid_view_->ResetForShowApps();
  // Ensure the grid fills the remaining space in the bubble so that icons can
  // be dropped beneath the last row.
  layout->SetFlexForView(scrollable_apps_grid_view_, 1);

  scroll_view_->SetContents(std::move(scroll_contents));

  UpdateSuggestions();
}

AppListBubbleAppsPage::~AppListBubbleAppsPage() {
  AppListModelProvider::Get()->RemoveObserver(this);
  continue_section_->RemoveObserver(this);
  recent_apps_->RemoveObserver(this);
}

void AppListBubbleAppsPage::UpdateSuggestions() {
  recent_apps_->ShowResults(AppListModelProvider::Get()->search_model(),
                            AppListModelProvider::Get()->model());
  continue_section_->UpdateSuggestionTasks();
  UpdateSeparatorVisibility();
}

void AppListBubbleAppsPage::AnimateShowLauncher() {
  DCHECK(GetVisible());

  // The animation relies on the correct positions of views, so force layout.
  if (needs_layout())
    Layout();
  DCHECK(!needs_layout());

  // This part of the animation has a longer duration than the bubble part
  // handled in AppListBubbleView, so track overall smoothness here.
  ui::AnimationThroughputReporter reporter(
      scrollable_apps_grid_view_->layer()->GetAnimator(),
      metrics_util::ForSmoothness(base::BindRepeating([](int value) {
        base::UmaHistogramPercentage(
            "Apps.ClamshellLauncher.AnimationSmoothness.OpenAppsPage", value);
      })));

  // Animate the views. Each section is initially offset down, then slides up
  // into its final position. If a section isn't visible, skip it. The further
  // down the section, the greater its initial offset. This code uses multiple
  // animations because views::AnimationBuilder doesn't have a good way to
  // build a single animation with conditional parts. https://crbug.com/1266020
  constexpr int kSectionOffset = 20;
  int vertical_offset = 0;
  if (continue_section_->GetTasksSuggestionsCount() > 0) {
    vertical_offset += kSectionOffset;
    SlideViewIntoPosition(continue_section_, vertical_offset);
  }
  if (recent_apps_->GetItemViewCount() > 0) {
    vertical_offset += kSectionOffset;
    SlideViewIntoPosition(recent_apps_, vertical_offset);
  }
  if (separator_->GetVisible()) {
    // The separator is not offset; it animates next to the view above it.
    SlideViewIntoPosition(separator_, vertical_offset);
  }

  // The apps grid is always visible.
  vertical_offset += kSectionOffset;
  // Use a special cleanup callback to show the gradient mask at the end of the
  // animation. No need to use SlideViewIntoPosition() because this view always
  // has a layer.
  StartSlideInAnimation(
      scrollable_apps_grid_view_, vertical_offset, kSlideAnimationDuration,
      kSlideAnimationTweenType,
      base::BindRepeating(&AppListBubbleAppsPage::OnAppsGridViewAnimationEnded,
                          weak_factory_.GetWeakPtr()));
}

void AppListBubbleAppsPage::AnimateHideLauncher() {
  // Remove the gradient mask from the scroll view to improve performance.
  gradient_helper_.reset();
}

void AppListBubbleAppsPage::AnimateShowPage() {
  SetVisible(true);

  // If skipping animations, just update visibility.
  if (!features::IsProductivityLauncherAnimationEnabled() ||
      ui::ScopedAnimationDurationScaleMode::is_zero()) {
    return;
  }

  // TODO(https://crbug.com/1286590): Add ui::AnimationThroughputReporter and
  // tests.

  // Scroll contents has a layer, so animate that.
  views::View* scroll_contents = scroll_view_->contents();
  DCHECK(scroll_contents->layer());
  DCHECK_EQ(scroll_contents->layer()->type(), ui::LAYER_TEXTURED);

  gfx::Transform translate_down;
  constexpr int kVerticalOffset = 40;
  translate_down.Translate(0, kVerticalOffset);

  // Position: Down 40 -> 0, duration 250ms, ease (0.00, 0.00, 0.20, 1.00)
  // Opacity: 0% -> 100%, delay 50ms, duration 100ms
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetOpacity(scroll_contents, 0.f)
      .SetTransform(scroll_contents, translate_down)
      .Then()
      .SetDuration(base::Milliseconds(250))
      .SetTransform(scroll_contents, gfx::Transform(),
                    gfx::Tween::LINEAR_OUT_SLOW_IN)
      .At(base::Milliseconds(50))
      .SetDuration(base::Milliseconds(100))
      .SetOpacity(scroll_contents, 1.f);
}

void AppListBubbleAppsPage::AnimateHidePage() {
  // If skipping animations, just update visibility.
  if (!features::IsProductivityLauncherAnimationEnabled() ||
      ui::ScopedAnimationDurationScaleMode::is_zero()) {
    SetVisible(false);
    return;
  }

  // TODO(https://crbug.com/1286590): Add ui::AnimationThroughputReporter and
  // tests.

  // Update view visibility when the animation is done.
  auto set_visible_false = base::BindRepeating(
      [](base::WeakPtr<AppListBubbleAppsPage> self) {
        if (!self)
          return;
        self->SetVisible(false);
        ui::Layer* layer = self->scroll_view()->contents()->layer();
        layer->SetOpacity(1.f);
        layer->SetTransform(gfx::Transform());
      },
      weak_factory_.GetWeakPtr());

  // Scroll contents has a layer, so animate that.
  views::View* scroll_contents = scroll_view_->contents();
  DCHECK(scroll_contents->layer());
  DCHECK_EQ(scroll_contents->layer()->type(), ui::LAYER_TEXTURED);

  // The animation spec says 40 dips down over 250ms, but the opacity animation
  // renders the view invisible after 50ms, so animate the visible fraction.
  gfx::Transform translate_down;
  constexpr int kVerticalOffset = 40 * 250 / 50;
  translate_down.Translate(0, kVerticalOffset);

  // Opacity: 100% -> 0%, duration 50ms
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(set_visible_false)
      .OnAborted(set_visible_false)
      .Once()
      .SetDuration(base::Milliseconds(50))
      .SetOpacity(scroll_contents, 0.f)
      .SetTransform(scroll_contents, translate_down);
}

void AppListBubbleAppsPage::ResetScrollPosition() {
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(), 0);
}

void AppListBubbleAppsPage::AbortAllAnimations() {
  auto abort_animations = [](views::View* view) {
    if (view->layer())
      view->layer()->GetAnimator()->AbortAllAnimations();
  };
  abort_animations(continue_section_);
  abort_animations(recent_apps_);
  abort_animations(separator_);
  abort_animations(scrollable_apps_grid_view_);
}

void AppListBubbleAppsPage::DisableFocusForShowingActiveFolder(bool disabled) {
  continue_section_->DisableFocusForShowingActiveFolder(disabled);
  recent_apps_->DisableFocusForShowingActiveFolder(disabled);
  scrollable_apps_grid_view_->DisableFocusForShowingActiveFolder(disabled);
}

void AppListBubbleAppsPage::UpdateForNewSortingOrder(
    const absl::optional<AppListSortOrder>& new_order,
    bool animate,
    base::OnceClosure update_position_closure) {
  DCHECK(features::IsLauncherAppSortEnabled());
  DCHECK_EQ(animate, !update_position_closure.is_null());

  if (!animate) {
    // Reordering is not required so update the undo toast and return early.
    reorder_undo_container_->OnTemporarySortOrderChanged(new_order);
    return;
  }

  update_position_closure_ = std::move(update_position_closure);
  scrollable_apps_grid_view_->FadeOutVisibleItemsForReorder(base::BindRepeating(
      &AppListBubbleAppsPage::OnAppsGridViewFadeOutAnimationEneded,
      weak_factory_.GetWeakPtr(), new_order));
}

void AppListBubbleAppsPage::Layout() {
  views::View::Layout();
  if (gradient_helper_)
    gradient_helper_->UpdateGradientZone();
}

void AppListBubbleAppsPage::VisibilityChanged(views::View* starting_from,
                                              bool is_visible) {
  if (!is_visible)
    scrollable_apps_grid_view_->CancelDragWithNoDropAnimation();
}

void AppListBubbleAppsPage::OnActiveAppListModelsChanged(
    AppListModel* model,
    SearchModel* search_model) {
  scrollable_apps_grid_view_->SetModel(model);
  scrollable_apps_grid_view_->SetItemList(model->top_level_item_list());

  recent_apps_->ShowResults(search_model, model);
}

void AppListBubbleAppsPage::OnViewVisibilityChanged(
    views::View* observed_view,
    views::View* starting_view) {
  if (starting_view == continue_section_ || starting_view == recent_apps_)
    UpdateSeparatorVisibility();
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

ui::Layer* AppListBubbleAppsPage::GetPageAnimationLayerForTest() {
  return scroll_view_->contents()->layer();
}

void AppListBubbleAppsPage::UpdateSeparatorVisibility() {
  separator_->SetVisible(recent_apps_->GetItemViewCount() > 0 ||
                         continue_section_->GetTasksSuggestionsCount() > 0);
}

void AppListBubbleAppsPage::DestroyLayerForView(views::View* view) {
  // This function is not static so it can be bound with a weak pointer.
  view->DestroyLayer();
}

void AppListBubbleAppsPage::OnAppsGridViewAnimationEnded() {
  // If the window is destroyed during an animation the animation will end, but
  // there's no need to build the gradient mask layer.
  if (GetWidget()->GetNativeWindow()->is_destroying())
    return;

  // Set up fade in/fade out gradients at top/bottom of scroll view. Wait until
  // the end of the show animation because the animation performs better without
  // the gradient mask layer.
  gradient_helper_ = std::make_unique<ScrollViewGradientHelper>(scroll_view_);
  gradient_helper_->UpdateGradientZone();
}

void AppListBubbleAppsPage::OnAppsGridViewFadeOutAnimationEneded(
    const absl::optional<AppListSortOrder>& new_order,
    bool aborted) {
  // Update item positions after the fade out animation but before the fade in
  // animation. NOTE: `update_position_closure_` can be empty in some edge
  // cases. For example, the app list is set with a new order denoted by Order
  // A. Then before the fade out animation is completed, the app list order is
  // reset with the old value. In this case, `update_position_closure_` for
  // Order A is never called. As a result, the closure for resetting the order
  // is empty.
  // Also update item positions only when the fade out animation ends normally.
  // Because a fade out animation is aborted when:
  // (1) Another reorder animation starts, or
  // (2) The apps grid's view model updates due to the reasons such as app
  // installation or model reset.
  // It is meaningless to update item positions in either case.
  if (update_position_closure_ && !aborted)
    std::move(update_position_closure_).Run();

  // Record the undo toast's visibility before update.
  const bool old_toast_visible = reorder_undo_container_->is_toast_visible();

  reorder_undo_container_->OnTemporarySortOrderChanged(new_order);

  // Skip the fade in animation if the fade out animation is aborted.
  if (aborted)
    return;

  // When the undo toast's visibility changes, the apps grid's bounds should
  // change. Meanwhile, the fade in animation relies on the apps grid's bounds
  // to calculate visible items. Therefore trigger layout before starting the
  // fade in animation.
  if (old_toast_visible != reorder_undo_container_->is_toast_visible())
    Layout();

  scrollable_apps_grid_view_->FadeInVisibleItemsForReorder();
}

void AppListBubbleAppsPage::SlideViewIntoPosition(views::View* view,
                                                  int vertical_offset) {
  // Animation spec:
  //
  // Y Position: Down (offset) → End position
  // Duration: 250ms
  // Ease: (0.00, 0.00, 0.20, 1.00)

  const bool create_layer = PrepareForLayerAnimation(view);

  // If we created a layer for the view, undo that when the animation ends.
  // The underlying views don't expose weak pointers directly, so use a weak
  // pointer to this view, which owns its children.
  auto cleanup = create_layer ? base::BindRepeating(
                                    &AppListBubbleAppsPage::DestroyLayerForView,
                                    weak_factory_.GetWeakPtr(), view)
                              : base::DoNothing();
  StartSlideInAnimation(view, vertical_offset, kSlideAnimationDuration,
                        kSlideAnimationTweenType, cleanup);
}

BEGIN_METADATA(AppListBubbleAppsPage, views::View)
END_METADATA

}  // namespace ash

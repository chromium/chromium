// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_apps_page.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/views/app_list_keyboard_controller.h"
#include "ash/app_list/views/app_list_nudge_controller.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/app_list/views/app_list_toast_view.h"
#include "ash/app_list/views/app_list_view_util.h"
#include "ash/app_list/views/continue_section_view.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/controls/rounded_scroll_bar.h"
#include "ash/controls/scroll_view_gradient_helper.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

using views::BoxLayout;

namespace ash {

namespace {

constexpr int kContinueColumnCount = 2;

// Insets for the vertical scroll bar. The bottom is pushed up slightly to keep
// the scroll bar from being clipped by the rounded corners.
constexpr auto kVerticalScrollInsets = gfx::Insets::TLBR(1, 0, 16, 1);

// The padding between different sections within the apps page. Also used for
// interior apps page container margin.
constexpr int kVerticalPaddingBetweenSections = 16;

// Label container padding in DIPs.
constexpr auto kContinueLabelContainerPadding = gfx::Insets::TLBR(0, 16, 0, 16);

// The horizontal interior margin for the apps page container - i.e. the margin
// between the apps page bounds and the page content.
constexpr int kHorizontalInteriorMargin = 16;

// The size of the scroll view gradient.
constexpr int kScrollViewGradientSize = 16;

// Insets for the continue section. These insets are required to make the
// suggestion icons visually align with the icons in the apps grid.
constexpr auto kContinueSectionInsets = gfx::Insets::VH(0, 4);

// Insets for the separator between the continue section and apps.
constexpr auto kSeparatorInsets = gfx::Insets::VH(0, 16);

// Delay for the show page transform and opacity animations.
constexpr base::TimeDelta kShowPageAnimationDelay = base::Milliseconds(50);

// The spec says "Down 40 -> 0, duration 250ms" with no delay, but the opacity
// animation has a 50ms delay that causes the first 50ms to be invisible. Just
// animate the 200ms visible part, which is 32 dips. This ensures the search
// page hide animation doesn't play at the same time as the apps page show
// animation.
constexpr int kShowPageAnimationVerticalOffset = 32;
constexpr base::TimeDelta kShowPageAnimationTransformDuration =
    base::Milliseconds(200);

// Duration of the show page opacity animation.
constexpr base::TimeDelta kShowPageAnimationOpacityDuration =
    base::Milliseconds(100);

// A view that runs a click callback when clicked or tapped.
class ClickableView : public views::View {
  METADATA_HEADER(ClickableView, views::View)

 public:
  explicit ClickableView(base::RepeatingClosure click_callback)
      : click_callback_(click_callback) {}
  ~ClickableView() override = default;

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    views::View::OnMousePressed(event);
    // Return true so this object will receive a mouse released event.
    return true;
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    views::View::OnMouseReleased(event);
    click_callback_.Run();
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    views::View::OnGestureEvent(event);
    if (event->type() == ui::EventType::kGestureTap) {
      event->SetHandled();
      click_callback_.Run();
    }
  }

 private:
  base::RepeatingClosure click_callback_;
};

BEGIN_METADATA(ClickableView)
END_METADATA

}  // namespace

AppListBubbleAppsPage::AppListBubbleAppsPage(
    AppListViewDelegate* view_delegate,
    AppListConfig* app_list_config,
    AppListA11yAnnouncer* a11y_announcer,
    AppListFolderController* folder_controller,
    SearchBoxView* search_box)
    : view_delegate_(view_delegate),
      search_box_(search_box),
      app_list_keyboard_controller_(
          std::make_unique<AppListKeyboardController>(this)),
      app_list_nudge_controller_(std::make_unique<AppListNudgeController>()) {
  DCHECK(view_delegate);
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
  scroll_view_->SetBackgroundColor(std::nullopt);
  // Arrow keys are used to select app icons.
  scroll_view_->SetAllowKeyboardScrolling(false);

  // Scroll view will have a gradient mask layer, and is animated during
  // hide/show.
  scroll_view_->SetPaintToLayer();
  scroll_view_->layer()->SetFillsBoundsOpaquely(false);

  // Set up scroll bars.
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  auto vertical_scroll = std::make_unique<RoundedScrollBar>(
      views::ScrollBar::Orientation::kVertical);
  vertical_scroll->SetInsets(kVerticalScrollInsets);
  vertical_scroll->SetSnapBackOnDragOutside(false);
  scroll_bar_ = vertical_scroll.get();
  scroll_view_->SetVerticalScrollBar(std::move(vertical_scroll));

  auto scroll_contents = std::make_unique<views::View>();
  auto* layout = scroll_contents->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kVerticalPaddingBetweenSections,
                      kHorizontalInteriorMargin),
      kVerticalPaddingBetweenSections));
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStretch);

  // The "Continue where you left off" label is in a container that is a child
  // of this view.
  InitContinueLabelContainer(scroll_contents.get());

  // Continue section row.
  continue_section_ = scroll_contents->AddChildView(
      std::make_unique<ContinueSectionView>(view_delegate, kContinueColumnCount,
                                            /*tablet_mode=*/false));
  continue_section_->SetBorder(
      views::CreateEmptyBorder(kContinueSectionInsets));
  continue_section_->SetNudgeController(app_list_nudge_controller_.get());
  // Decrease the between-sections spacing so the continue label is closer to
  // the continue tasks section.
  continue_section_->SetProperty(views::kMarginsKey,
                                 gfx::Insets::TLBR(-14, 0, 0, 0));

  // Observe changes in continue section visibility, to keep separator
  // visibility in sync.
  continue_section_->AddObserver(this);

  // Recent apps row.
  recent_apps_ = scroll_contents->AddChildView(std::make_unique<RecentAppsView>(
      app_list_keyboard_controller_.get(), view_delegate));
  recent_apps_->UpdateAppListConfig(app_list_config);
  // Observe changes in continue section visibility, to keep separator
  // visibility in sync.
  recent_apps_->AddObserver(this);

  // Horizontal separator.
  separator_ =
      scroll_contents->AddChildView(std::make_unique<views::Separator>());
  separator_->SetBorder(views::CreateEmptyBorder(kSeparatorInsets));
  separator_->SetColorId(cros_tokens::kCrosSysSeparator);

  // Add a empty container view. A toast view should be added to
  // `toast_container_` when the app list starts temporary sorting.
  toast_container_ =
      scroll_contents->AddChildView(std::make_unique<AppListToastContainerView>(
          app_list_nudge_controller_.get(), app_list_keyboard_controller_.get(),
          a11y_announcer, view_delegate,
          /*delegate=*/this,
          /*tablet_mode=*/false));

  // All apps section.
  scrollable_apps_grid_view_ =
      scroll_contents->AddChildView(std::make_unique<ScrollableAppsGridView>(
          a11y_announcer, view_delegate,
          /*folder_delegate=*/nullptr, scroll_view_, folder_controller,
          app_list_keyboard_controller_.get()));
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
  UpdateContinueSectionVisibility();

  on_contents_scrolled_subscription_ =
      scroll_view_->AddContentsScrolledCallback(base::BindRepeating(
          &AppListBubbleAppsPage::OnPageScrolled, base::Unretained(this)));
}

AppListBubbleAppsPage::~AppListBubbleAppsPage() {
  AppListModelProvider::Get()->RemoveObserver(this);
  continue_section_->RemoveObserver(this);
  recent_apps_->RemoveObserver(this);
}

void AppListBubbleAppsPage::UpdateSuggestions() {
  recent_apps_->SetModels(AppListModelProvider::Get()->search_model(),
                          AppListModelProvider::Get()->model());
  continue_section_->UpdateSuggestionTasks();
  UpdateSeparatorVisibility();
}

void AppListBubbleAppsPage::AnimateShowLauncher(bool is_side_shelf) {
  DCHECK(GetVisible());

  // Don't show the scroll bar due to thumb bounds changes. There's enough
  // visual movement going on during the animation.
  scroll_bar_->SetShowOnThumbBoundsChanged(false);

  // The animation relies on the correct positions of views, so force layout.
  if (needs_layout())
    DeprecatedLayoutImmediately();
  DCHECK(!needs_layout());

  // This part of the animation has a longer duration than the bubble part
  // handled in AppListBubbleView, so track overall smoothness here.
  ui::AnimationThroughputReporter reporter(
      scrollable_apps_grid_view_->layer()->GetAnimator(),
      metrics_util::ForSmoothnessV3(base::BindRepeating([](int value) {
        // This histogram name is used in Tast tests. Do not rename.
        base::UmaHistogramPercentage(
            "Apps.ClamshellLauncher.AnimationSmoothness.OpenAppsPage", value);
      })));

  // Side-shelf uses faster animations.
  const base::TimeDelta slide_duration =
      is_side_shelf ? base::Milliseconds(150) : base::Milliseconds(250);
  const gfx::Tween::Type tween_type = gfx::Tween::LINEAR_OUT_SLOW_IN;

  // Animate the views. Each section is initially offset down, then slides up
  // into its final position. For side shelf, each section is initially offset
  // up, then it slides down. If a section isn't visible, skip it. The further
  // down the section, the greater its initial offset. This code uses multiple
  // animations because views::AnimationBuilder doesn't have a good way to
  // build a single animation with conditional parts. https://crbug.com/1266020
  const int section_offset = is_side_shelf ? -20 : 20;
  int vertical_offset = 0;
  const bool animate_continue_label_container =
      continue_label_container_ && continue_label_container_->GetVisible();
  if (animate_continue_label_container) {
    vertical_offset += section_offset;
    SlideViewIntoPosition(continue_label_container_, vertical_offset,
                          slide_duration, tween_type);
  }
  if (continue_section_->GetVisible() &&
      continue_section_->GetTasksSuggestionsCount() > 0) {
    // Only offset if this is the top section, otherwise animate next to the
    // continue label container above.
    if (!animate_continue_label_container)
      vertical_offset += section_offset;
    SlideViewIntoPosition(continue_section_, vertical_offset, slide_duration,
                          tween_type);
  }
  if (recent_apps_->GetVisible() && recent_apps_->GetItemViewCount() > 0) {
    vertical_offset += section_offset;
    SlideViewIntoPosition(recent_apps_, vertical_offset, slide_duration,
                          tween_type);
  }
  if (separator_->GetVisible()) {
    // The separator is not offset; it animates next to the view above it.
    SlideViewIntoPosition(separator_, vertical_offset, slide_duration,
                          tween_type);
  }
  if (toast_container_ && toast_container_->IsToastVisible()) {
    vertical_offset += section_offset;
    SlideViewIntoPosition(toast_container_, vertical_offset, slide_duration,
                          tween_type);
  }

  // The apps grid is always visible.
  vertical_offset += section_offset;
  // Use a special cleanup callback to show the gradient mask at the end of the
  // animation. No need to use SlideViewIntoPosition() because this view always
  // has a layer.

  // Set up fade in/fade out gradients at top/bottom of scroll view.
  gradient_helper_ = std::make_unique<ScrollViewGradientHelper>(
      scroll_view_, kScrollViewGradientSize);
  gradient_helper_->UpdateGradientMask();

  StartSlideInAnimation(
      scrollable_apps_grid_view_, vertical_offset, slide_duration, tween_type,
      base::BindRepeating(&AppListBubbleAppsPage::OnAppsGridViewAnimationEnded,
                          weak_factory_.GetWeakPtr()));
}

void AppListBubbleAppsPage::PrepareForHideLauncher() {
  // Remove the gradient mask from the scroll view to improve performance.
  gradient_helper_.reset();
  scrollable_apps_grid_view_->EndDrag(/*cancel=*/true);
}

void AppListBubbleAppsPage::AnimateShowPage() {
  // If skipping animations, just update visibility.
  if (ui::ScopedAnimationDurationScaleMode::is_zero()) {
    SetVisible(true);
    return;
  }

  // Ensure any in-progress animations have their cleanup callbacks called.
  // Note that this might call SetVisible(false) from the hide animation.
  AbortAllAnimations();

  // Ensure the view is visible.
  SetVisible(true);

  ui::Layer* scroll_view_layer = scroll_view_->layer();
  DCHECK(scroll_view_layer);
  DCHECK_EQ(scroll_view_layer->type(), ui::LAYER_TEXTURED);

  ui::AnimationThroughputReporter reporter(
      scroll_view_layer->GetAnimator(),
      metrics_util::ForSmoothnessV3(base::BindRepeating([](int value) {
        base::UmaHistogramPercentage(
            "Apps.ClamshellLauncher.AnimationSmoothness.ShowAppsPage", value);
      })));

  gfx::Transform translate_down;
  translate_down.Translate(0, kShowPageAnimationVerticalOffset);

  // Update view visibility when the animation is done. Needed to ensure
  // the view has the correct opacity and transform when the animation is
  // aborted.
  auto set_visible_true = base::BindRepeating(
      [](base::WeakPtr<AppListBubbleAppsPage> self) {
        if (!self)
          return;
        self->SetVisible(true);
        ui::Layer* layer = self->scroll_view()->layer();
        layer->SetOpacity(1.f);
        layer->SetTransform(gfx::Transform());
      },
      weak_factory_.GetWeakPtr());

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(set_visible_true)
      .OnAborted(set_visible_true)
      .Once()
      .SetOpacity(scroll_view_layer, 0.f)
      .SetTransform(scroll_view_layer, translate_down)
      .At(kShowPageAnimationDelay)
      .SetDuration(kShowPageAnimationTransformDuration)
      .SetTransform(scroll_view_layer, gfx::Transform(),
                    gfx::Tween::LINEAR_OUT_SLOW_IN)
      .At(kShowPageAnimationDelay)
      .SetDuration(kShowPageAnimationOpacityDuration)
      .SetOpacity(scroll_view_layer, 1.f);
}

void AppListBubbleAppsPage::AnimateHidePage() {
  // If skipping animations, just update visibility.
  if (ui::ScopedAnimationDurationScaleMode::is_zero()) {
    SetVisible(false);
    return;
  }

  scrollable_apps_grid_view_->CancelDragWithNoDropAnimation();

  // Update view visibility when the animation is done.
  auto set_visible_false = base::BindRepeating(
      [](base::WeakPtr<AppListBubbleAppsPage> self) {
        if (!self)
          return;
        self->SetVisible(false);
        ui::Layer* layer = self->scroll_view()->layer();
        layer->SetOpacity(1.f);
        layer->SetTransform(gfx::Transform());
      },
      weak_factory_.GetWeakPtr());

  ui::Layer* scroll_view_layer = scroll_view_->layer();
  DCHECK(scroll_view_layer);
  DCHECK_EQ(scroll_view_layer->type(), ui::LAYER_TEXTURED);

  ui::AnimationThroughputReporter reporter(
      scroll_view_layer->GetAnimator(),
      metrics_util::ForSmoothnessV3(base::BindRepeating([](int value) {
        base::UmaHistogramPercentage(
            "Apps.ClamshellLauncher.AnimationSmoothness.HideAppsPage", value);
      })));

  // The animation spec says 40 dips down over 250ms, but the opacity animation
  // renders the view invisible after 50ms, so animate the visible fraction.
  gfx::Transform translate_down;
  constexpr int kVerticalOffset = 40 * 50 / 250;
  translate_down.Translate(0, kVerticalOffset);

  // Opacity: 100% -> 0%, duration 50ms
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(set_visible_false)
      .OnAborted(set_visible_false)
      .Once()
      .SetDuration(base::Milliseconds(50))
      .SetOpacity(scroll_view_layer, 0.f)
      .SetTransform(scroll_view_layer, translate_down);
}

void AppListBubbleAppsPage::ResetScrollPosition() {
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(), 0);
}

void AppListBubbleAppsPage::AbortAllAnimations() {
  auto abort_animations = [](views::View* view) {
    if (view->layer())
      view->layer()->GetAnimator()->AbortAllAnimations();
  };
  abort_animations(scroll_view_);
  abort_animations(continue_section_);
  abort_animations(recent_apps_);
  abort_animations(separator_);
  if (toast_container_)
    abort_animations(toast_container_);
  abort_animations(scrollable_apps_grid_view_);
}

void AppListBubbleAppsPage::DisableFocusForShowingActiveFolder(bool disabled) {
  toggle_continue_section_button_->SetEnabled(!disabled);
  // Prevent container items from being accessed by ChromeVox.
  SetViewIgnoredForAccessibility(continue_label_container_, disabled);

  continue_section_->DisableFocusForShowingActiveFolder(disabled);
  recent_apps_->DisableFocusForShowingActiveFolder(disabled);
  if (toast_container_)
    toast_container_->DisableFocusForShowingActiveFolder(disabled);
  scrollable_apps_grid_view_->DisableFocusForShowingActiveFolder(disabled);
}

void AppListBubbleAppsPage::UpdateForNewSortingOrder(
    const std::optional<AppListSortOrder>& new_order,
    bool animate,
    base::OnceClosure update_position_closure,
    base::OnceClosure animation_done_closure) {
  DCHECK_EQ(animate, !update_position_closure.is_null());
  DCHECK(!animation_done_closure || animate);

  // A11y announcements must happen before animations, otherwise the undo
  // guidance is spoken first because focus moves immediately to the undo button
  // on the toast. Note that when `new_order` is null, `animate` was set to true
  // only if the sort was reverted.
  if (new_order) {
    if (*new_order != AppListSortOrder::kAlphabeticalEphemeralAppFirst)
      toast_container_->AnnounceSortOrder(*new_order);
  } else if (animate) {
    toast_container_->AnnounceUndoSort();
  }

  if (!animate) {
    // Reordering is not required so update the undo toast and return early.
    app_list_nudge_controller_->OnTemporarySortOrderChanged(new_order);
    toast_container_->OnTemporarySortOrderChanged(new_order);
    HandleFocusAfterSort();
    return;
  }

  // Abort the old reorder animation if any before closure update to avoid data
  // races on closures.
  scrollable_apps_grid_view_->MaybeAbortWholeGridAnimation();
  DCHECK(!update_position_closure_);
  update_position_closure_ = std::move(update_position_closure);
  DCHECK(!reorder_animation_done_closure_);
  reorder_animation_done_closure_ = std::move(animation_done_closure);

  views::AnimationBuilder animation_builder =
      scrollable_apps_grid_view_->FadeOutVisibleItemsForReorder(
          base::BindRepeating(
              &AppListBubbleAppsPage::OnAppsGridViewFadeOutAnimationEnded,
              weak_factory_.GetWeakPtr(), new_order));

  // Configure the toast fade out animation if the toast is going to be hidden.
  const bool current_toast_visible = toast_container_->IsToastVisible();
  const bool target_toast_visible =
      toast_container_->GetVisibilityForSortOrder(new_order);
  if (current_toast_visible && !target_toast_visible) {
    // Because `toast_container_` does not have a layer before the fade in
    // animation, create one.
    DCHECK(!toast_container_->layer());
    toast_container_->SetPaintToLayer();
    toast_container_->layer()->SetFillsBoundsOpaquely(false);

    animation_builder.GetCurrentSequence().SetOpacity(toast_container_->layer(),
                                                      0.f);
  }
}

bool AppListBubbleAppsPage::MaybeScrollToShowToast() {
  gfx::Point toast_origin;
  views::View::ConvertPointToTarget(toast_container_, scroll_view_->contents(),
                                    &toast_origin);
  const gfx::Rect toast_bounds_in_scroll_view =
      gfx::Rect(toast_origin, toast_container_->size());

  // Do not scroll if the toast is already fully shown.
  if (scroll_view_->GetVisibleRect().Contains(toast_bounds_in_scroll_view))
    return false;

  const int scroll_offset = separator_->GetVisible() ? separator_->y() : 0;
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                 scroll_offset);

  return true;
}

void AppListBubbleAppsPage::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);
  if (gradient_helper_)
    gradient_helper_->UpdateGradientMask();
}

void AppListBubbleAppsPage::VisibilityChanged(views::View* starting_from,
                                              bool is_visible) {
  // Cancel any in progress drag without running drop animation if the bubble
  // apps page is hiding.
  if (!is_visible) {
    scrollable_apps_grid_view_->CancelDragWithNoDropAnimation();
  }

  // Updates the visibility state in toast container.
  AppListToastContainerView::VisibilityState state =
      is_visible ? AppListToastContainerView::VisibilityState::kShown
                 : AppListToastContainerView::VisibilityState::kHidden;
  toast_container_->UpdateVisibilityState(state);

  // Check if the reorder nudge view needs update if the bubble apps page is
  // showing.
  if (is_visible) {
    toast_container_->MaybeUpdateReorderNudgeView();
  }
}

void AppListBubbleAppsPage::OnBoundsChanged(const gfx::Rect& old_bounds) {
  // Toast container, and continue section may contain toasts with multiline
  // labels, whose preferred height will depend on the apps page bounds (in
  // particular, the amount of horizontal space available to lay out labels).
  // Propagate the amount of available width for toasts before layout starts, so
  // the toast views can correctly calculate their preferred size during the
  // ensuing layout pass (otherwise, the preferred toast size may change as
  // result of the layout).
  toast_container_->ConfigureLayoutForAvailableWidth(
      bounds().width() - 2 * kHorizontalInteriorMargin);
  continue_section_->ConfigureLayoutForAvailableWidth(
      bounds().width() - 2 * kHorizontalInteriorMargin -
      kContinueSectionInsets.width());
}

void AppListBubbleAppsPage::OnActiveAppListModelsChanged(
    AppListModel* model,
    SearchModel* search_model) {
  scrollable_apps_grid_view_->SetModel(model);
  scrollable_apps_grid_view_->SetItemList(model->top_level_item_list());

  recent_apps_->SetModels(search_model, model);
}

void AppListBubbleAppsPage::OnViewVisibilityChanged(
    views::View* observed_view,
    views::View* starting_view) {
  if (starting_view == continue_section_ || starting_view == recent_apps_)
    UpdateSeparatorVisibility();
}

void AppListBubbleAppsPage::OnNudgeRemoved() {
  const gfx::Rect current_grid_bounds = scrollable_apps_grid_view_->bounds();

  if (needs_layout())
    DeprecatedLayoutImmediately();

  const gfx::Rect target_grid_bounds = scrollable_apps_grid_view_->bounds();
  const int offset = current_grid_bounds.y() - target_grid_bounds.y();

  // With the nudge gone, animate the apps grid up to its new target location.
  StartSlideInAnimation(scrollable_apps_grid_view_, offset,
                        base::Milliseconds(300),
                        gfx::Tween::ACCEL_40_DECEL_100_3, base::DoNothing());
}

ContinueSectionView* AppListBubbleAppsPage::GetContinueSectionView() {
  return continue_section_;
}

RecentAppsView* AppListBubbleAppsPage::GetRecentAppsView() {
  return recent_apps_;
}

AppListToastContainerView* AppListBubbleAppsPage::GetToastContainerView() {
  return toast_container_;
}

AppsGridView* AppListBubbleAppsPage::GetAppsGridView() {
  return scrollable_apps_grid_view_;
}

ui::Layer* AppListBubbleAppsPage::GetPageAnimationLayerForTest() {
  // Animating the `scroll_view_`'s content layer can have its transform
  // animations interrupted when the content layer's transforms get set due to
  // rtl specific transforms in ScrollView code. Use the `scroll_view_` layer
  // for animations to avoid this.
  return scroll_view_->layer();
}

////////////////////////////////////////////////////////////////////////////////
// private:

void AppListBubbleAppsPage::InitContinueLabelContainer(
    views::View* scroll_contents) {
  // The entire container view is clickable/tappable. The view is not focusable,
  // but the toggle button is focusable, and that satisfies the user's need for
  // an element with keyboard or accessibility focus.
  continue_label_container_ =
      scroll_contents->AddChildView(std::make_unique<ClickableView>(
          base::BindRepeating(&AppListBubbleAppsPage::OnToggleContinueSection,
                              base::Unretained(this))));
  continue_label_container_->SetBorder(
      views::CreateEmptyBorder(kContinueLabelContainerPadding));

  auto* layout = continue_label_container_->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal));
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kCenter);

  continue_label_ =
      continue_label_container_->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_ASH_LAUNCHER_CONTINUE_SECTION_LABEL)));
  bubble_utils::ApplyStyle(continue_label_, TypographyToken::kCrosAnnotation1,
                           cros_tokens::kCrosSysOnSurfaceVariant);
  continue_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Button should be right aligned, so flex label to fill empty space.
  layout->SetFlexForView(continue_label_, 1);

  // The toggle button is clickable/tappable in addition to the container view.
  // This ensures ink drop ripple effects play when the button is clicked.
  toggle_continue_section_button_ =
      continue_label_container_->AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(&AppListBubbleAppsPage::OnToggleContinueSection,
                              base::Unretained(this)),
          IconButton::Type::kSmallFloating, &kChevronUpIcon,
          /*is_togglable=*/false,
          /*has_border=*/false));
  // See ButtonFocusSkipper in app_list_bubble_view.cc for focus handling.
}

void AppListBubbleAppsPage::UpdateContinueSectionVisibility() {
  // The continue section view and recent apps view manage their own visibility
  // internally.
  continue_section_->UpdateElementsVisibility();
  recent_apps_->UpdateVisibility();
  UpdateContinueLabelContainer();
  UpdateSeparatorVisibility();
}

void AppListBubbleAppsPage::OnPageScrolled() {
  // Do not log anything if the contents are not scrollable.
  if (scroll_view_->GetVisibleRect().height() >=
      scroll_view_->contents()->height()) {
    return;
  }

  if (scroll_view_->GetVisibleRect().bottom() ==
      scroll_view_->contents()->bounds().bottom()) {
    RecordLauncherWorkflowMetrics(
        AppListUserAction::kNavigatedToBottomOfAppList,
        /*is_tablet_mode = */ false, std::nullopt);
  }
}

void AppListBubbleAppsPage::UpdateContinueLabelContainer() {
  if (!continue_label_container_)
    return;

  // If there are no suggested tasks and no recent apps, it doesn't make sense
  // to show the continue label. This can happen for brand-new users.
  continue_label_container_->SetVisible(
      continue_section_->GetTasksSuggestionsCount() > 0 ||
      recent_apps_->GetItemViewCount() > 0);

  // Update the toggle continue section button tooltip and image.
  bool is_hidden = view_delegate_->ShouldHideContinueSection();
  toggle_continue_section_button_->SetTooltipText(l10n_util::GetStringUTF16(
      is_hidden ? IDS_ASH_LAUNCHER_SHOW_CONTINUE_SECTION_TOOLTIP
                : IDS_ASH_LAUNCHER_HIDE_CONTINUE_SECTION_TOOLTIP));
  toggle_continue_section_button_->SetVectorIcon(is_hidden ? kChevronDownIcon
                                                           : kChevronUpIcon);
}

void AppListBubbleAppsPage::UpdateSeparatorVisibility() {
  // The separator only hides if the user has the continue section shown but
  // there are no suggested tasks and no apps. This is rare.
  separator_->SetVisible(view_delegate_->ShouldHideContinueSection() ||
                         recent_apps_->GetVisible() ||
                         continue_section_->GetVisible());
}

void AppListBubbleAppsPage::DestroyLayerForView(views::View* view) {
  // This function is not static so it can be bound with a weak pointer.
  view->DestroyLayer();
}

void AppListBubbleAppsPage::OnAppsGridViewAnimationEnded() {
  // Show the scroll bar for keyboard-driven scroll position changes.
  scroll_bar_->SetShowOnThumbBoundsChanged(true);
}

void AppListBubbleAppsPage::HandleFocusAfterSort() {
  // As the sort update on AppListBubbleAppsPage can be called in both clamshell
  // mode and tablet mode, return early if it's currently in tablet mode because
  // the AppListBubbleAppsPage isn't visible.
  if (view_delegate_->IsInTabletMode())
    return;

  // Focusing toast button may show the tooltip anchored on the button - make
  // sure the toast button bounds are correctly set before tooltip is shown.
  if (GetWidget()) {
    GetWidget()->LayoutRootViewIfNecessary();
  }

  // If the sort is done and the toast is visible and not fading out, request
  // the focus on the undo button on the toast. Otherwise request the focus on
  // the search box.
  if (toast_container_->IsToastVisible()) {
    toast_container_->toast_view()->toast_button()->RequestFocus();
  } else {
    search_box_->search_box()->RequestFocus();
  }
}

void AppListBubbleAppsPage::OnAppsGridViewFadeOutAnimationEnded(
    const std::optional<AppListSortOrder>& new_order,
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
  const bool old_toast_visible = toast_container_->IsToastVisible();

  toast_container_->OnTemporarySortOrderChanged(new_order);
  HandleFocusAfterSort();
  const bool target_toast_visible = toast_container_->IsToastVisible();

  // If there is a layer created for fading out `toast_container_`, destroy
  // the layer when the fade out animation ends. NOTE: when the reorder toast
  // is faded out, it should not be faded in along with the apps grid fade in
  // animation. Therefore destroy the layer when the fade out animation ends.
  if (toast_container_->layer()) {
    DCHECK(!target_toast_visible);
    toast_container_->DestroyLayer();
  }

  // Skip the fade in animation if the fade out animation is aborted.
  if (aborted) {
    OnReorderAnimationEnded();
    return;
  }

  const bool toast_visibility_change =
      (old_toast_visible != target_toast_visible);

  // When the undo toast's visibility changes, the apps grid's bounds should
  // change. Meanwhile, the fade in animation relies on the apps grid's bounds
  // to calculate visible items. Therefore trigger layout before starting the
  // fade in animation.
  if (toast_visibility_change)
    DeprecatedLayoutImmediately();

  // Ensure to scroll before triggering apps grid fade in animation so that
  // the bubble apps page's layout is ready.
  const bool scroll_performed = MaybeScrollToShowToast();

  views::AnimationBuilder animation_builder =
      scrollable_apps_grid_view_->FadeInVisibleItemsForReorder(
          base::BindRepeating(
              &AppListBubbleAppsPage::OnAppsGridViewFadeInAnimationEnded,
              weak_factory_.GetWeakPtr()));

  // Fade in the undo toast when:
  // (1) The toast's visibility becomes true from false, or
  // (2) The apps page is scrolled to show the toast.
  const bool should_fade_in_toast =
      target_toast_visible && (scroll_performed || toast_visibility_change);

  if (!should_fade_in_toast)
    return;

  // Because `toast_container_` does not have a layer before the fade in
  // animation, create one.
  DCHECK(!toast_container_->layer());
  toast_container_->SetPaintToLayer();
  toast_container_->layer()->SetFillsBoundsOpaquely(false);

  // Hide the undo toast instantly before starting the toast fade in animation.
  toast_container_->layer()->SetOpacity(0.f);

  animation_builder.GetCurrentSequence().SetOpacity(
      toast_container_->layer(), 1.f, gfx::Tween::ACCEL_5_70_DECEL_90);
}

void AppListBubbleAppsPage::OnAppsGridViewFadeInAnimationEnded(bool aborted) {
  // Destroy the layer created for the layer animation.
  toast_container_->DestroyLayer();

  OnReorderAnimationEnded();
}

void AppListBubbleAppsPage::OnReorderAnimationEnded() {
  update_position_closure_.Reset();

  if (reorder_animation_done_closure_)
    std::move(reorder_animation_done_closure_).Run();
}

void AppListBubbleAppsPage::SlideViewIntoPosition(views::View* view,
                                                  int vertical_offset,
                                                  base::TimeDelta duration,
                                                  gfx::Tween::Type tween_type) {
  // Animation spec:
  //
  // Y Position: Down (offset) → End position
  // Ease: (0.00, 0.00, 0.20, 1.00)

  const bool create_layer = PrepareForLayerAnimation(view);

  // If we created a layer for the view, undo that when the animation ends.
  // The underlying views don't expose weak pointers directly, so use a weak
  // pointer to this view, which owns its children.
  auto cleanup = create_layer ? base::BindRepeating(
                                    &AppListBubbleAppsPage::DestroyLayerForView,
                                    weak_factory_.GetWeakPtr(), view)
                              : base::DoNothing();
  StartSlideInAnimation(view, vertical_offset, duration, tween_type, cleanup);
}

void AppListBubbleAppsPage::FadeInContinueSectionView(views::View* view) {
  const bool create_layer = PrepareForLayerAnimation(view);

  // If we created a layer for the view, undo that when the animation ends.
  // The underlying views don't expose weak pointers directly, so use a weak
  // pointer to this view, which owns its children.
  auto cleanup = create_layer ? base::BindRepeating(
                                    &AppListBubbleAppsPage::DestroyLayerForView,
                                    weak_factory_.GetWeakPtr(), view)
                              : base::DoNothing();

  view->layer()->SetOpacity(0.0f);

  // The animation has a delay to give the separator and apps grid time to
  // partially slide out of the way.
  views::AnimationBuilder()
      .SetPreemptionStrategy(ui::LayerAnimator::PreemptionStrategy::
                                 IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(cleanup)
      .OnAborted(cleanup)
      .Once()
      .At(base::Milliseconds(100))
      .SetOpacity(view->layer(), 1.0f)
      .SetDuration(base::Milliseconds(200));
}

void AppListBubbleAppsPage::OnToggleContinueSection() {
  const int separator_initial_y = separator_->y();

  // Toggle the section visibility.
  bool should_hide = !view_delegate_->ShouldHideContinueSection();
  view_delegate_->SetHideContinueSection(should_hide);
  // AppListControllerImpl will trigger UpdateContinueSectionVisibility().

  // Layout will change the position of the separator and apps grid based on the
  // visibility of the continue section view and recent apps.
  if (needs_layout())
    DeprecatedLayoutImmediately();

  // The vertical offset for slide animations is the difference in separator
  // position from before layout versus its position now.
  const int vertical_offset = separator_initial_y - separator_->y();
  const base::TimeDelta duration = base::Milliseconds(300);
  const gfx::Tween::Type tween_type = gfx::Tween::ACCEL_LIN_DECEL_100_3;

  if (should_hide) {
    // Don't try to fade out the continue section and recent apps because the
    // view is already invisible. UX is OK with these sections not animating.

    // The separator and apps grid slide up.
    DCHECK_GE(vertical_offset, 0);
    SlideViewIntoPosition(separator_, vertical_offset, duration, tween_type);
    SlideViewIntoPosition(scrollable_apps_grid_view_, vertical_offset, duration,
                          tween_type);
  } else {
    // The continue section and recent apps fade in.
    FadeInContinueSectionView(continue_section_);
    FadeInContinueSectionView(recent_apps_);

    // The separator and apps grid slide down.
    DCHECK_LE(vertical_offset, 0);
    SlideViewIntoPosition(separator_, vertical_offset, duration, tween_type);
    SlideViewIntoPosition(scrollable_apps_grid_view_, vertical_offset, duration,
                          tween_type);
  }
}

void AppListBubbleAppsPage::RecordAboveTheFoldMetrics() {
  std::vector<std::string> apps_above_the_fold = {};
  std::vector<std::string> apps_below_the_fold = {};
  for (size_t i = 0; i < scrollable_apps_grid_view_->view_model()->view_size();
       ++i) {
    AppListItemView* child_view =
        scrollable_apps_grid_view_->view_model()->view_at(i);
    if (scrollable_apps_grid_view_->IsAboveTheFold(child_view)) {
      apps_above_the_fold.push_back(child_view->item()->id());
    } else {
      apps_below_the_fold.push_back(child_view->item()->id());
    }
  }
  view_delegate_->RecordAppsDefaultVisibility(
      std::move(apps_above_the_fold), std::move(apps_below_the_fold),
      /*is_apps_collections_page=*/false);
}

BEGIN_METADATA(AppListBubbleAppsPage)
END_METADATA

}  // namespace ash

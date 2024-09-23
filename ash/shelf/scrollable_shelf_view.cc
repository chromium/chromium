// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/scrollable_shelf_view.h"

#include <algorithm>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/screen_util.h"
#include "ash/shelf/scrollable_shelf_constants.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_tooltip_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/desks/desk_button/desk_button_container.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/view_targeter_delegate.h"

namespace ash {

namespace {

// Returns the display id for the display that shows the shelf for |view|.
int64_t GetDisplayIdForView(const views::View* view) {
  aura::Window* window = view->GetWidget()->GetNativeWindow();
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
}

void ReportSmoothness(bool tablet_mode, bool launcher_visible, int smoothness) {
  base::UmaHistogramPercentage(
      scrollable_shelf_constants::kAnimationSmoothnessHistogram, smoothness);
  if (tablet_mode) {
    if (launcher_visible) {
      base::UmaHistogramPercentage(
          scrollable_shelf_constants::
              kAnimationSmoothnessTabletLauncherVisibleHistogram,
          smoothness);
    } else {
      base::UmaHistogramPercentage(
          scrollable_shelf_constants::
              kAnimationSmoothnessTabletLauncherHiddenHistogram,
          smoothness);
    }
  } else {
    if (launcher_visible) {
      base::UmaHistogramPercentage(
          scrollable_shelf_constants::
              kAnimationSmoothnessClamshellLauncherVisibleHistogram,
          smoothness);
    } else {
      base::UmaHistogramPercentage(
          scrollable_shelf_constants::
              kAnimationSmoothnessClamshellLauncherHiddenHistogram,
          smoothness);
    }
  }
}

gfx::Insets GetMirroredInsets(const gfx::Insets& insets) {
  return gfx::Insets::TLBR(insets.top(), insets.right(), insets.bottom(),
                           insets.left());
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ScrollableShelfArrowView

class ScrollableShelfView::ScrollableShelfArrowView
    : public ScrollArrowView,
      public views::ViewTargeterDelegate {
  METADATA_HEADER(ScrollableShelfArrowView, ScrollArrowView)

 public:
  explicit ScrollableShelfArrowView(ArrowType arrow_type,
                                    bool is_horizontal_alignment,
                                    ShelfView* shelf_view,
                                    ShelfButtonDelegate* shelf_button_delegate)
      : ScrollArrowView(arrow_type,
                        is_horizontal_alignment,
                        shelf_view->shelf(),
                        shelf_button_delegate),
        shelf_(shelf_view->shelf()) {
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    set_context_menu_controller(shelf_view);

    // When the spoken feedback is enabled, scrollable shelf should ensure that
    // the hidden icon which receives the accessibility focus shows through
    // scroll animation. So the arrow button is not useful for the spoken
    // feedback users. The spoken feedback should ignore the arrow button.
    GetViewAccessibility().SetIsIgnored(/*value=*/true);
  }
  ~ScrollableShelfArrowView() override = default;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    DCHECK_EQ(target, this);
    const gfx::Rect bounds = gfx::Rect(size());

    // Calculates the tapping area. Note that tapping area is bigger than the
    // arrow button's bounds.
    gfx::Rect tap_rect(
        shelf_->PrimaryAxisValue(
            scrollable_shelf_constants::kArrowButtonTapAreaHorizontal,
            shelf_->hotseat_widget()->GetHotseatSize()),
        shelf_->PrimaryAxisValue(
            shelf_->hotseat_widget()->GetHotseatSize(),
            scrollable_shelf_constants::kArrowButtonTapAreaHorizontal));
    tap_rect -= gfx::Vector2d((tap_rect.width() - bounds.width()) / 2,
                              (tap_rect.height() - bounds.height()) / 2);
    DCHECK(tap_rect.Contains(bounds));

    return tap_rect.Intersects(rect);
  }

  // Make ScrollRectToVisible a no-op because ScrollableShelfArrowView is
  // always visible/invisible depending on the layout strategy at fixed
  // locations. So it does not need to be scrolled to show.
  // TODO (andrewxu): Moves all of functions related with scrolling into
  // ScrollableShelfContainerView. Then erase this empty function.
  void ScrollRectToVisible(const gfx::Rect& rect) override {}

 private:
  const raw_ptr<Shelf> shelf_;
};

BEGIN_METADATA(ScrollableShelfView, ScrollableShelfArrowView)
END_METADATA

////////////////////////////////////////////////////////////////////////////////
// ScopedActiveInkDropCountImpl

class ScrollableShelfView::ScopedActiveInkDropCountImpl
    : public ScrollableShelfView::ScopedActiveInkDropCount {
 public:
  explicit ScopedActiveInkDropCountImpl(
      base::RepeatingCallback<void(bool)> callback)
      : on_active_ink_drop_change_callback_(callback) {
    on_active_ink_drop_change_callback_.Run(/*increase=*/true);
  }

  ~ScopedActiveInkDropCountImpl() override {
    on_active_ink_drop_change_callback_.Run(/*increase=*/false);
  }

  ScopedActiveInkDropCountImpl(const ScopedActiveInkDropCountImpl& rhs) =
      delete;
  ScopedActiveInkDropCountImpl& operator=(
      const ScopedActiveInkDropCountImpl& rhs) = delete;

 private:
  base::RepeatingCallback<void(bool)> on_active_ink_drop_change_callback_;
};

////////////////////////////////////////////////////////////////////////////////
// ScrollableShelfContainerView

class ScrollableShelfContainerView : public ShelfContainerView,
                                     public views::ViewTargeterDelegate {
  METADATA_HEADER(ScrollableShelfContainerView, ShelfContainerView)

 public:
  explicit ScrollableShelfContainerView(
      ScrollableShelfView* scrollable_shelf_view)
      : ShelfContainerView(scrollable_shelf_view->shelf_view()),
        scrollable_shelf_view_(scrollable_shelf_view) {
    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  }

  ScrollableShelfContainerView(const ScrollableShelfContainerView&) = delete;
  ScrollableShelfContainerView& operator=(const ScrollableShelfContainerView&) =
      delete;

  ~ScrollableShelfContainerView() override = default;

  // ShelfContainerView:
  void TranslateShelfView(const gfx::Vector2dF& offset) override;

 private:
  // views::View:
  void Layout(PassKey) override;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  raw_ptr<ScrollableShelfView> scrollable_shelf_view_ = nullptr;
};

void ScrollableShelfContainerView::TranslateShelfView(
    const gfx::Vector2dF& offset) {
  ShelfContainerView::TranslateShelfView(
      scrollable_shelf_view_->ShouldAdaptToRTL() ? -offset : offset);
}

void ScrollableShelfContainerView::Layout(PassKey) {
  // Should not use ShelfView::GetPreferredSize in replace of
  // CalculateIdealSize. Because ShelfView::CalculatePreferredSize relies on the
  // bounds of app icon. Meanwhile, the icon's bounds may be updated by
  // animation.
  const gfx::Rect ideal_bounds = gfx::Rect(CalculatePreferredSize({}));

  const gfx::Rect local_bounds = GetLocalBounds();
  gfx::Rect shelf_view_bounds =
      local_bounds.Contains(ideal_bounds) ? local_bounds : ideal_bounds;

  if (shelf_view_->shelf()->IsHorizontalAlignment())
    shelf_view_bounds.set_x(ShelfConfig::Get()->GetAppIconEndPadding());
  else
    shelf_view_bounds.set_y(ShelfConfig::Get()->GetAppIconEndPadding());

  shelf_view_->SetBoundsRect(shelf_view_bounds);
  shelf_view_->shelf()
      ->shelf_layout_manager()
      ->HandleScrollableShelfContainerBoundsChange();
}

bool ScrollableShelfContainerView::DoesIntersectRect(
    const views::View* target,
    const gfx::Rect& rect) const {
  // This view's layer is clipped. So the view should only handle the events
  // within the area after cilp.
  gfx::RectF bounds(scrollable_shelf_view_->visible_space());
  views::View::ConvertRectToTarget(scrollable_shelf_view_, this, &bounds);
  return ToEnclosedRect(bounds).Contains(rect);
}

BEGIN_METADATA(ScrollableShelfContainerView)
END_METADATA

////////////////////////////////////////////////////////////////////////////////
// ScrollableShelfFocusSearch

class ScrollableShelfFocusSearch : public views::FocusSearch {
 public:
  explicit ScrollableShelfFocusSearch(
      ScrollableShelfView* scrollable_shelf_view)
      : FocusSearch(/*root=*/nullptr,
                    /*cycle=*/true,
                    /*accessibility_mode=*/true),
        scrollable_shelf_view_(scrollable_shelf_view) {}

  ScrollableShelfFocusSearch(const ScrollableShelfFocusSearch&) = delete;
  ScrollableShelfFocusSearch& operator=(const ScrollableShelfFocusSearch&) =
      delete;

  ~ScrollableShelfFocusSearch() override = default;

  // views::FocusSearch
  views::View* FindNextFocusableView(
      views::View* starting_view,
      FocusSearch::SearchDirection search_direction,
      FocusSearch::TraversalDirection traversal_direction,
      FocusSearch::StartingViewPolicy check_starting_view,
      FocusSearch::AnchoredDialogPolicy can_go_into_anchored_dialog,
      views::FocusTraversable** focus_traversable,
      views::View** focus_traversable_view) override {
    std::vector<views::View*> focusable_views;
    ShelfView* shelf_view = scrollable_shelf_view_->shelf_view();

    for (int i : shelf_view->visible_views_indices())
      focusable_views.push_back(shelf_view->view_model()->view_at(i));

    int start_index = 0;
    for (size_t i = 0; i < focusable_views.size(); ++i) {
      if (focusable_views[i] == starting_view) {
        start_index = i;
        break;
      }
    }

    int new_index =
        start_index +
        (search_direction == FocusSearch::SearchDirection::kBackwards ? -1 : 1);

    // Scrolls to the new page if the focused shelf item is not tappable
    // on the current page.
    if (new_index < 0) {
      new_index = focusable_views.size() - 1;
    } else if (static_cast<size_t>(new_index) >= focusable_views.size()) {
      new_index = 0;
    } else if (static_cast<size_t>(new_index) <
               scrollable_shelf_view_->first_tappable_app_index()) {
      scrollable_shelf_view_->ScrollToNewPage(/*forward=*/false);
    } else if (static_cast<size_t>(new_index) >
               scrollable_shelf_view_->last_tappable_app_index()) {
      scrollable_shelf_view_->ScrollToNewPage(/*forward=*/true);
    }

    return focusable_views[new_index];
  }

 private:
  raw_ptr<ScrollableShelfView> scrollable_shelf_view_ = nullptr;
};

////////////////////////////////////////////////////////////////////////////////
// ScrollableShelfView

ScrollableShelfView::ScrollableShelfView(ShelfModel* model, Shelf* shelf)
    : shelf_view_(new ShelfView(model,
                                shelf,
                                /*drag_and_drop_host=*/this,
                                /*shelf_button_delegate=*/this)),
      page_flip_time_threshold_(
          scrollable_shelf_constants::kShelfPageFlipDelay) {
  Shell::Get()->AddShellObserver(this);
  ShelfConfig::Get()->AddObserver(this);
  set_allow_deactivate_on_esc(true);
}

ScrollableShelfView::~ScrollableShelfView() {
  ShelfConfig::Get()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  GetShelf()->tooltip()->set_shelf_tooltip_delegate(nullptr);
}

void ScrollableShelfView::Init() {
  // Although there is no animation for ScrollableShelfView, a layer is still
  // needed. Otherwise, the child view without its own layer will be painted on
  // RootView which is beneath |translucent_background_| in ShelfWidget.
  // As a result, the child view will not show.
  SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  layer()->SetFillsBoundsOpaquely(false);

  // Initialize the shelf container view.
  // Note that |shelf_container_view_| should be under the arrow buttons. It
  // ensures that the arrow button receives the tapping events which happen
  // within the overlapping zone between the arrow button's tapping area and
  // the bounds of |shelf_container_view_|.
  shelf_container_view_ =
      AddChildView(std::make_unique<ScrollableShelfContainerView>(this));
  shelf_container_view_->Initialize();

  // Initialize the left arrow button.
  left_arrow_ = AddChildView(std::make_unique<ScrollableShelfArrowView>(
      ScrollArrowView::kLeft, GetShelf()->IsHorizontalAlignment(), shelf_view_,
      this));

  // Initialize the right arrow button.
  right_arrow_ = AddChildView(std::make_unique<ScrollableShelfArrowView>(
      ScrollArrowView::kRight, GetShelf()->IsHorizontalAlignment(), shelf_view_,
      this));

  focus_search_ = std::make_unique<ScrollableShelfFocusSearch>(this);

  GetShelf()->tooltip()->set_shelf_tooltip_delegate(this);

  set_context_menu_controller(this);

  // Initializes |shelf_view_| after scrollable shelf view's children are
  // initialized.
  shelf_view_->Init(focus_search_.get());
}

void ScrollableShelfView::OnFocusRingActivationChanged(bool activated) {
  if (activated) {
    focus_ring_activated_ = true;
    SetPaneFocusAndFocusDefault();
    force_show_hotseat_resetter_ =
        GetShelf()->shelf_widget()->ForceShowHotseatInTabletMode();
  } else {
    // Shows the gradient shader when the focus ring is disabled.
    focus_ring_activated_ = false;
    if (force_show_hotseat_resetter_)
      force_show_hotseat_resetter_.RunAndReset();
  }

  MaybeUpdateGradientZone();
}

void ScrollableShelfView::ScrollToNewPage(bool forward) {
  const float offset = CalculatePageScrollingOffset(forward, layout_strategy_);
  if (GetShelf()->IsHorizontalAlignment())
    ScrollByXOffset(offset, /*animating=*/true);
  else
    ScrollByYOffset(offset, /*animating=*/true);
}

views::FocusSearch* ScrollableShelfView::GetFocusSearch() {
  return focus_search_.get();
}

views::FocusTraversable* ScrollableShelfView::GetFocusTraversableParent() {
  return parent()->GetFocusTraversable();
}

views::View* ScrollableShelfView::GetFocusTraversableParentView() {
  return this;
}

views::View* ScrollableShelfView::GetDefaultFocusableChild() {
  // Adapts |scroll_offset_| to show the view properly right after the focus
  // ring is enabled.

  if (default_last_focusable_child_) {
    ScrollToMainOffset(CalculateScrollUpperBound(GetSpaceForIcons()),
                       /*animating=*/true);
    return FindLastFocusableChild();
  }

  ScrollToMainOffset(/*target_offset=*/0.f, /*animating=*/true);
  return FindFirstFocusableChild();
}

gfx::Rect ScrollableShelfView::GetHotseatBackgroundBounds() const {
  return available_space_;
}

bool ScrollableShelfView::ShouldAdaptToRTL() const {
  return base::i18n::IsRTL() && GetShelf()->IsHorizontalAlignment();
}

bool ScrollableShelfView::NeedUpdateToTargetBounds() const {
  return GetAvailableLocalBounds(/*use_target_bounds=*/true) !=
         GetAvailableLocalBounds(/*use_target_bounds=*/false);
}

gfx::Rect ScrollableShelfView::GetTargetScreenBoundsOfItemIcon(
    const ShelfID& id) const {
  const int item_index_in_model = shelf_view_->model()->ItemIndexByID(id);

  // Return a dummy value if the item specified by `id` does not exist in the
  // shelf model.
  // TODO(crbug.com/40057927): it is a quick fixing. We should
  // investigate the root cause.
  if (item_index_in_model < 0)
    return gfx::Rect();

  // Calculates the available space for child views based on the target bounds.
  // To ease coding, we use the variables before mirroring in computation.
  const gfx::Insets target_edge_padding_RTL_mirrored =
      CalculateMirroredEdgePadding(/*use_target_bounds=*/true);
  const gfx::Insets target_edge_padding_before_RTL_mirror =
      ShouldAdaptToRTL() ? GetMirroredInsets(target_edge_padding_RTL_mirrored)
                         : target_edge_padding_RTL_mirrored;
  gfx::Rect target_space_before_RTL_mirror =
      GetAvailableLocalBounds(/*use_target_bounds=*/true);
  target_space_before_RTL_mirror.Inset(target_edge_padding_before_RTL_mirror);

  const gfx::Insets current_edge_padding_RTL_mirrored = edge_padding_insets_;
  const gfx::Insets current_edge_padding_before_RTL_mirror =
      ShouldAdaptToRTL() ? GetMirroredInsets(current_edge_padding_RTL_mirrored)
                         : current_edge_padding_RTL_mirrored;
  gfx::Rect icon_bounds =
      shelf_view_->view_model()->ideal_bounds(item_index_in_model);
  icon_bounds.Offset(target_edge_padding_before_RTL_mirror.left() -
                         current_edge_padding_before_RTL_mirror.left(),
                     0);

  // Transforms |icon_bounds| from shelf view's coordinates to scrollable shelf
  // view's coordinates manually.
  const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();
  const int shelf_view_offset = ShelfConfig::Get()->GetAppIconEndPadding();
  const int shelf_view_container_offset =
      is_horizontal_alignment ? shelf_container_view_->bounds().x()
                              : shelf_container_view_->bounds().y();
  const int target_scroll_offset = CalculateScrollOffsetForTargetAvailableSpace(
      target_space_before_RTL_mirror);
  const int delta =
      -target_scroll_offset + shelf_view_container_offset + shelf_view_offset;
  const gfx::Vector2d bounds_offset = is_horizontal_alignment
                                          ? gfx::Vector2d(delta, 0)
                                          : gfx::Vector2d(0, delta);
  icon_bounds.Offset(bounds_offset);

  // If the icon is invisible under the target view bounds, replaces the actual
  // icon's bounds with the rectangle centering on the edge of |target_space|.
  const gfx::Point icon_bounds_center = icon_bounds.CenterPoint();
  if (icon_bounds_center.x() > target_space_before_RTL_mirror.right()) {
    icon_bounds.Offset(
        target_space_before_RTL_mirror.right_center().OffsetFromOrigin() -
        icon_bounds_center.OffsetFromOrigin());
  } else if (icon_bounds_center.x() < target_space_before_RTL_mirror.x()) {
    icon_bounds.Offset(
        target_space_before_RTL_mirror.left_center().OffsetFromOrigin() -
        icon_bounds_center.OffsetFromOrigin());
  }

  // Hotseat's target bounds may differ from the actual bounds. So it has to
  // transform the bounds manually from view's local coordinates to screen.
  // Notes that the target bounds stored in shelf layout manager are adapted to
  // RTL already while |icon_bounds| are not adjusted to RTL yet.
  gfx::Rect hotseat_bounds_in_screen =
      GetShelf()->hotseat_widget()->GetTargetBounds();
  if (ShouldAdaptToRTL()) {
    // One simple way for transformation under RTL is: (1) Transforms hotseat
    // target bounds from RTL to LTR. (2) Calculates the icon's bounds in screen
    // under LTR. (3) Transforms the icon's bounds to RTL.
    gfx::Rect display_bounds =
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(GetWidget()->GetNativeView())
            .bounds();
    hotseat_bounds_in_screen.set_x(display_bounds.right() -
                                   hotseat_bounds_in_screen.right());
    icon_bounds.Offset(hotseat_bounds_in_screen.OffsetFromOrigin());
    icon_bounds.set_x(display_bounds.right() - icon_bounds.right());
  } else {
    icon_bounds.Offset(hotseat_bounds_in_screen.OffsetFromOrigin());
  }

  return icon_bounds;
}

bool ScrollableShelfView::RequiresScrollingForItemSize(
    const gfx::Size& target_size,
    int button_size) const {
  const gfx::Size icons_preferred_size =
      shelf_container_view_->CalculateIdealSize(button_size);
  return !CanFitAllAppsWithoutScrolling(target_size, icons_preferred_size);
}

void ScrollableShelfView::SetEdgePaddingInsets(
    const gfx::Insets& padding_insets) {
  edge_padding_insets_ = padding_insets;
  shelf_view_->LayoutIfAppIconsOffsetUpdates();
}

gfx::Insets ScrollableShelfView::CalculateMirroredEdgePadding(
    bool use_target_bounds) const {
  // Tries display centering strategy.
  const gfx::Insets display_centering_edge_padding =
      CalculateMirroredPaddingForDisplayCentering(use_target_bounds);
  if (!display_centering_edge_padding.IsEmpty()) {
    // Returns early if the value is legal.
    return display_centering_edge_padding;
  }

  const int icons_size =
      shelf_view_->GetSizeOfAppButtons(shelf_view_->number_of_visible_apps(),
                                       shelf_view_->GetButtonSize()) +
      2 * ShelfConfig::Get()->GetAppIconEndPadding();

  const gfx::Rect available_local_bounds =
      GetAvailableLocalBounds(use_target_bounds);
  const int available_size_for_app_icons = GetShelf()->PrimaryAxisValue(
      available_local_bounds.width(), available_local_bounds.height());

  int gap = CanFitAllAppsWithoutScrolling(available_local_bounds.size(),
                                          CalculatePreferredSize({}))
                ? available_size_for_app_icons - icons_size
                : 0;  // overflow

  // Calculates the paddings before/after the visible area of scrollable shelf.
  // |after_padding| being zero ensures that the available space after the
  // visible area is filled first.
  const int before_padding = gap;
  const int after_padding = 0;

  gfx::Insets padding_insets;
  if (GetShelf()->IsHorizontalAlignment()) {
    padding_insets = gfx::Insets::TLBR(0, before_padding, 0, after_padding);
    if (ShouldAdaptToRTL())
      padding_insets = GetMirroredInsets(padding_insets);
  } else {
    padding_insets = gfx::Insets::TLBR(before_padding, 0, after_padding, 0);
  }

  return padding_insets;
}

bool ScrollableShelfView::CalculateShelfOverflowForAvailableLength(
    int available_length) const {
  return available_length < CalculateShelfIconsPreferredLength();
}

views::View* ScrollableShelfView::GetShelfContainerViewForTest() {
  return shelf_container_view_;
}

bool ScrollableShelfView::ShouldAdjustForTest() const {
  return CalculateAdjustmentOffset(CalculateMainAxisScrollDistance(),
                                   layout_strategy_, GetSpaceForIcons());
}

void ScrollableShelfView::SetTestObserver(TestObserver* test_observer) {
  DCHECK(!(test_observer && test_observer_));

  test_observer_ = test_observer;
}

bool ScrollableShelfView::IsAnyCornerButtonInkDropActivatedForTest() const {
  return activated_corner_buttons_ > 0;
}

float ScrollableShelfView::GetScrollUpperBoundForTest() const {
  return CalculateScrollUpperBound(GetSpaceForIcons());
}

bool ScrollableShelfView::IsPageFlipTimerBusyForTest() const {
  return page_flip_timer_.IsRunning();
}

int ScrollableShelfView::GetSumOfButtonSizeAndSpacing() const {
  return shelf_view_->GetButtonSize() + ShelfConfig::Get()->button_spacing();
}

int ScrollableShelfView::GetGestureDragThreshold() const {
  return shelf_view_->GetButtonSize() / 2;
}

float ScrollableShelfView::CalculateScrollUpperBound(
    int available_space_for_icons) const {
  if (layout_strategy_ == kNotShowArrowButtons)
    return 0.f;

  return std::max(
      0, CalculateShelfIconsPreferredLength() - available_space_for_icons);
}

float ScrollableShelfView::CalculateClampedScrollOffset(
    float scroll,
    int available_space_for_icons) const {
  const float scroll_upper_bound =
      CalculateScrollUpperBound(available_space_for_icons);
  scroll = std::clamp(scroll, 0.0f, scroll_upper_bound);
  return scroll;
}

void ScrollableShelfView::StartShelfScrollAnimation(float scroll_distance) {
  const gfx::Vector2dF scroll_offset_before_update = scroll_offset_;
  UpdateScrollOffset(scroll_distance);

  if (scroll_offset_before_update == scroll_offset_)
    return;

  StopObservingImplicitAnimations();

  during_scroll_animation_ = true;
  MaybeUpdateGradientZone();

  // In tablet mode, if the target layout only has one arrow button, enable the
  // rounded corners of the shelf container layer in order to cut off the icons
  // outside of the hotseat background.
  const bool one_arrow_in_target_state =
      (layout_strategy_ == LayoutStrategy::kShowLeftArrowButton ||
       layout_strategy_ == LayoutStrategy::kShowRightArrowButton);
  if (one_arrow_in_target_state)
    EnableShelfRoundedCorners(/*enable=*/true);

  ui::ScopedLayerAnimationSettings animation_settings(
      shelf_view_->layer()->GetAnimator());
  animation_settings.SetTweenType(gfx::Tween::EASE_OUT);
  animation_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
  animation_settings.AddObserver(this);

  ui::AnimationThroughputReporter reporter(
      animation_settings.GetAnimator(),
      metrics_util::ForSmoothnessV3(
          base::BindRepeating(&ReportSmoothness, Shell::Get()->IsInTabletMode(),
                              Shell::Get()->app_list_controller()->IsVisible(
                                  GetDisplayIdForView(this)))));

  shelf_container_view_->TranslateShelfView(scroll_offset_);
}

ScrollableShelfView::LayoutStrategy
ScrollableShelfView::CalculateLayoutStrategy(float scroll_distance_on_main_axis,
                                             int available_length) const {
  if (available_length >= CalculateShelfIconsPreferredLength()) {
    return kNotShowArrowButtons;
  }

  if (scroll_distance_on_main_axis == 0.f) {
    // No invisible shelf buttons at the left side. So hide the left button.
    return kShowRightArrowButton;
  }

  if (scroll_distance_on_main_axis ==
      CalculateScrollUpperBound(available_length)) {
    // If there is no invisible shelf button at the right side, hide the right
    // button.
    return kShowLeftArrowButton;
  }

  // There are invisible shelf buttons at both sides. So show two buttons.
  return kShowButtons;
}

Shelf* ScrollableShelfView::GetShelf() {
  return const_cast<Shelf*>(
      const_cast<const ScrollableShelfView*>(this)->GetShelf());
}

const Shelf* ScrollableShelfView::GetShelf() const {
  return shelf_view_->shelf();
}

gfx::Size ScrollableShelfView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return shelf_container_view_->GetPreferredSize(available_size);
}

void ScrollableShelfView::Layout(PassKey) {
  gfx::Rect shelf_container_bounds = gfx::Rect(size());

  // Transpose and layout as if it is horizontal.
  const bool is_horizontal = GetShelf()->IsHorizontalAlignment();
  if (!is_horizontal)
    shelf_container_bounds.Transpose();

  gfx::Size arrow_button_size(scrollable_shelf_constants::kArrowButtonSize,
                              shelf_container_bounds.height());
  gfx::Size arrow_button_group_size(
      scrollable_shelf_constants::kArrowButtonGroupWidth,
      shelf_container_bounds.height());

  // The bounds of |left_arrow_| and |right_arrow_| are in the
  // ScrollableShelfView's local coordinates.
  gfx::Rect left_arrow_bounds;
  gfx::Rect right_arrow_bounds;

  int before_padding;
  if (ShouldAdaptToRTL()) {
    before_padding = edge_padding_insets_.right();
  } else {
    before_padding = is_horizontal ? edge_padding_insets_.left()
                                   : edge_padding_insets_.top();
  }

  int after_padding;
  if (ShouldAdaptToRTL()) {
    after_padding = edge_padding_insets_.left();
  } else {
    after_padding = is_horizontal ? edge_padding_insets_.right()
                                  : edge_padding_insets_.bottom();
  }

  // Calculates the bounds of the left arrow button. If the left arrow button
  // should not show, |left_arrow_bounds| should be empty.
  if (layout_strategy_ == kShowLeftArrowButton ||
      layout_strategy_ == kShowButtons) {
    gfx::Point left_arrow_start_point(shelf_container_bounds.x(), 0);
    left_arrow_bounds =
        gfx::Rect(left_arrow_start_point, arrow_button_group_size);
    left_arrow_bounds.Offset(before_padding, 0);
    left_arrow_bounds.Inset(gfx::Insets::TLBR(
        0, scrollable_shelf_constants::kArrowButtonEndPadding, 0,
        scrollable_shelf_constants::kDistanceToArrowButton));
    left_arrow_bounds.ClampToCenteredSize(arrow_button_size);
  }

  if (layout_strategy_ == kShowRightArrowButton ||
      layout_strategy_ == kShowButtons) {
    gfx::Point right_arrow_start_point(
        shelf_container_bounds.right() - after_padding -
            scrollable_shelf_constants::kArrowButtonGroupWidth,
        0);
    right_arrow_bounds =
        gfx::Rect(right_arrow_start_point, arrow_button_group_size);
    right_arrow_bounds.Inset(gfx::Insets::TLBR(
        0, scrollable_shelf_constants::kDistanceToArrowButton, 0,
        scrollable_shelf_constants::kArrowButtonEndPadding));
    right_arrow_bounds.ClampToCenteredSize(arrow_button_size);
  }

  // Adjust the bounds when not showing in the horizontal
  // alignment.tShelf()->IsHorizontalAlignment()) {
  if (!is_horizontal) {
    left_arrow_bounds.Transpose();
    right_arrow_bounds.Transpose();
    shelf_container_bounds.Transpose();
  }

  // Layout |left_arrow_| if it should show.
  left_arrow_->SetVisible(!left_arrow_bounds.IsEmpty());
  left_arrow_->SetBoundsRect(left_arrow_bounds);

  // Layout |right_arrow_| if it should show.
  right_arrow_->SetVisible(!right_arrow_bounds.IsEmpty());
  right_arrow_->SetBoundsRect(right_arrow_bounds);

  MaybeUpdateGradientZone();

  // Layout |shelf_container_view_|.
  shelf_container_view_->SetBoundsRect(shelf_container_bounds);

  EnableLayerClipOnShelfContainerView(ShouldEnableLayerClip());
}

void ScrollableShelfView::ChildPreferredSizeChanged(views::View* child) {
  // Add/remove a shelf icon may change the layout strategy.
  UpdateAvailableSpaceAndScroll();
  shelf_container_view_->TranslateShelfView(scroll_offset_);
  DeprecatedLayoutImmediately();
}

void ScrollableShelfView::OnScrollEvent(ui::ScrollEvent* event) {
  if (event->finger_count() != 2)
    return;
  if (ShouldDelegateScrollToShelf(*event)) {
    GetShelf()->ProcessScrollEvent(event);
    event->StopPropagation();
  }
}

void ScrollableShelfView::OnMouseEvent(ui::MouseEvent* event) {
  if (event->IsMouseWheelEvent()) {
    HandleMouseWheelEvent(event->AsMouseWheelEvent());
    return;
  }

  // The mouse event's location may be outside of ShelfView but within the
  // bounds of the ScrollableShelfView. Meanwhile, ScrollableShelfView should
  // handle the mouse event consistently with ShelfView. To achieve this,
  // we simply redirect |event| to ShelfView.
  gfx::Point location_in_shelf_view = event->location();
  View::ConvertPointToTarget(this, shelf_view_, &location_in_shelf_view);
  event->set_location(location_in_shelf_view);
  shelf_view_->OnMouseEvent(event);
}

void ScrollableShelfView::OnGestureEvent(ui::GestureEvent* event) {
  if (ShouldHandleGestures(*event) && ProcessGestureEvent(*event)) {
    // |event| is consumed by ScrollableShelfView.
    event->SetHandled();
  } else if (shelf_view_->HandleGestureEvent(event)) {
    // |event| is consumed by ShelfView.
    event->StopPropagation();
  } else if (event->type() == ui::EventType::kGestureScrollBegin) {
    // |event| is consumed by neither ScrollableShelfView nor ShelfView. So the
    // gesture end event will not be propagated to this view. Then we need to
    // reset the class members related with scroll status explicitly.
    ResetScrollStatus();
  }
}

void ScrollableShelfView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  GetViewAccessibility().SetNextFocus(GetShelf()->GetStatusAreaWidget());
  GetViewAccessibility().SetPreviousFocus(
      GetShelf()->shelf_widget()->navigation_widget());
}

void ScrollableShelfView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  const gfx::Insets old_edge_padding_insets = edge_padding_insets_;
  const gfx::Vector2dF old_scroll_offset = scroll_offset_;

  // The changed view bounds may lead to update on the available space.
  UpdateAvailableSpaceAndScroll();

  // Relayout shelf items if the preferred padding changed.
  if (old_edge_padding_insets != edge_padding_insets_)
    shelf_view_->OnBoundsChanged(shelf_view_->GetBoundsInScreen());

  // Avoids calling AdjustOffset() when the scrollable shelf view is
  // under scroll along the main axis. Otherwise, animation will conflict with
  // scroll gesture. Meanwhile, translates the shelf view
  // if AdjustOffset() returns false since when AdjustOffset() returns true,
  // shelf view is scrolled by animation.
  const bool should_translate_shelf_view =
      scroll_status_ == kAlongMainAxisScroll || !AdjustOffset();

  if (should_translate_shelf_view && old_scroll_offset != scroll_offset_)
    shelf_container_view_->TranslateShelfView(scroll_offset_);
}

void ScrollableShelfView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.parent != shelf_view_.get()) {
    return;
  }

  shelf_view_->UpdateShelfItemViewsVisibility();

  // When app scaling state needs update, hotseat bounds should change. Then
  // it is not meaningful to do further work in the current view bounds. So
  // returns early.
  if (GetShelf()->hotseat_widget()->UpdateTargetHotseatDensityIfNeeded())
    return;

  const gfx::Vector2dF old_scroll_offset = scroll_offset_;

  // Adding/removing an icon may change the padding then affect the available
  // space.
  UpdateAvailableSpaceAndScroll();

  if (old_scroll_offset != scroll_offset_)
    shelf_container_view_->TranslateShelfView(scroll_offset_);
}

void ScrollableShelfView::ScrollRectToVisible(const gfx::Rect& rect) {
  // Transform |rect| to local view coordinates taking |scroll_offset_| into
  // consideration.
  const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();
  gfx::Rect rect_after_adjustment = rect;
  if (is_horizontal_alignment)
    rect_after_adjustment.Offset(-scroll_offset_.x(), 0);
  else
    rect_after_adjustment.Offset(0, -scroll_offset_.y());

  // Notes that |rect| is not mirrored under RTL while |visible_space_| has been
  // mirrored. It is easier for coding if we mirror |visible_space_| back and
  // then do the calculation.
  const gfx::Rect visible_space_without_RTL = GetMirroredRect(visible_space_);

  // |rect_after_adjustment| is already shown completely. So scroll is not
  // needed.
  if (visible_space_without_RTL.Contains(rect_after_adjustment)) {
    AdjustOffset();
    return;
  }

  const float original_offset = CalculateMainAxisScrollDistance();

  // |forward| indicates the scroll direction.
  const bool forward =
      is_horizontal_alignment
          ? rect_after_adjustment.right() > visible_space_without_RTL.right()
          : rect_after_adjustment.bottom() > visible_space_without_RTL.bottom();

  // Scrolling |shelf_view_| has the following side-effects:
  // (1) May change the layout strategy.
  // (2) May change the visible space.
  // (3) Must change the scrolling offset.
  // (4) Must change |rect_after_adjustment|'s coordinates after adjusting the
  // scroll.
  LayoutStrategy layout_strategy_after_scroll = layout_strategy_;
  float main_axis_offset_after_scroll = original_offset;
  gfx::Rect visible_space_after_scroll = visible_space_without_RTL;
  gfx::Rect rect_after_scroll = rect_after_adjustment;

  // In each iteration, it scrolls |shelf_view_| to the neighboring page.
  // Terminating the loop iteration if:
  // (1) Find the suitable page which shows |rect| completely.
  // (2) Cannot scroll |shelf_view_| anymore (it may happen with ChromeVox
  // enabled).
  while (!visible_space_after_scroll.Contains(rect_after_scroll)) {
    int page_scroll_distance =
        CalculatePageScrollingOffset(forward, layout_strategy_after_scroll);

    // Breaking the while loop if it cannot scroll anymore.
    if (!page_scroll_distance)
      break;

    main_axis_offset_after_scroll = CalculateTargetOffsetAfterScroll(
        main_axis_offset_after_scroll, page_scroll_distance);
    layout_strategy_after_scroll = CalculateLayoutStrategy(
        main_axis_offset_after_scroll, GetSpaceForIcons());
    visible_space_after_scroll =
        GetMirroredRect(CalculateVisibleSpace(layout_strategy_after_scroll));
    rect_after_scroll = rect_after_adjustment;
    const int offset_delta = main_axis_offset_after_scroll - original_offset;
    if (is_horizontal_alignment)
      rect_after_scroll.Offset(-offset_delta, 0);
    else
      rect_after_scroll.Offset(0, -offset_delta);
  }

  if (!visible_space_after_scroll.Contains(rect_after_scroll))
    return;

  ScrollToMainOffset(main_axis_offset_after_scroll, /*animating=*/true);
}

std::unique_ptr<ui::Layer> ScrollableShelfView::RecreateLayer() {
  layer()->SetGradientMask(gfx::LinearGradient::GetEmpty());
  return views::View::RecreateLayer();
}

void ScrollableShelfView::OnShelfButtonAboutToRequestFocusFromTabTraversal(
    ShelfButton* button,
    bool reverse) {
  if ((button == left_arrow_) || (button == right_arrow_))
    return;

  shelf_view_->OnShelfButtonAboutToRequestFocusFromTabTraversal(button,
                                                                reverse);
  ShelfWidget* shelf_widget = GetShelf()->shelf_widget();
  // In tablet mode, when the hotseat is not extended but one of the buttons
  // gets focused, it should update the visibility of the hotseat.
  if (Shell::Get()->IsInTabletMode() &&
      !shelf_widget->hotseat_widget()->IsExtended()) {
    shelf_widget->shelf_layout_manager()->UpdateVisibilityState(
        /*force_layout=*/false);
  }
}

void ScrollableShelfView::ButtonPressed(views::Button* sender,
                                        const ui::Event& event,
                                        views::InkDrop* ink_drop) {
  if ((sender == left_arrow_) || (sender == right_arrow_)) {
    ScrollToNewPage(sender == right_arrow_);
    return;
  }

  shelf_view_->ButtonPressed(sender, event, ink_drop);
}

void ScrollableShelfView::HandleAccessibleActionScrollToMakeVisible(
    ShelfButton* button) {
  // Scrollable shelf can only be hidden in tablet mode.
  GetShelf()->hotseat_widget()->set_manually_extended(true);
  GetShelf()->shelf_widget()->shelf_layout_manager()->UpdateVisibilityState(
      /*force_layout=*/false);
}

void ScrollableShelfView::OnButtonWillBeRemoved() {
  const int view_size_before_removal = shelf_view_->view_model()->view_size();
  DCHECK_GT(view_size_before_removal, 0);

  // Ensure `last_tappable_app_index_` to be valid after removal. Normally
  // `last_tappable_app_index_` updates when the shelf button is removed. But
  // button removal could be performed at the end of the button fade out
  // animation, which means that incorrect `last_tappable_app_index_` could be
  // accessed during the animation. To handle this issue, update
  // `last_tappable_app_index_` before removal finishes.
  // The code block also covers the edge case that the only shelf item is going
  // to be removed, i.e. `view_size_before_removal_` is one. In this case,
  // both `first_tappable_app_index_` and `last_tappable_app_index_` are reset
  // to invalid values (see https://crbug.com/1300561).
  if (view_size_before_removal < 2) {
    last_tappable_app_index_ = std::nullopt;
  } else {
    last_tappable_app_index_ = std::min(
        last_tappable_app_index_,
        std::make_optional(static_cast<size_t>(view_size_before_removal - 2)));
  }
  first_tappable_app_index_ =
      std::min(first_tappable_app_index_, last_tappable_app_index_);
}

void ScrollableShelfView::OnAppButtonActivated(const ShelfButton* button) {
  ScrollRectToVisible(button->bounds());
}

std::unique_ptr<ScrollableShelfView::ScopedActiveInkDropCount>
ScrollableShelfView::CreateScopedActiveInkDropCount(const ShelfButton* sender) {
  if (!ShouldCountActivatedInkDrop(sender))
    return nullptr;

  return std::make_unique<ScopedActiveInkDropCountImpl>(
      base::BindRepeating(&ScrollableShelfView::OnActiveInkDropChange,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ScrollableShelfView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // |point| is in screen coordinates. So it does not need to transform.
  shelf_view_->ShowContextMenuForViewImpl(shelf_view_, point, source_type);
}

void ScrollableShelfView::OnShelfAlignmentChanged(
    aura::Window* root_window,
    ShelfAlignment old_alignment) {
  const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();
  left_arrow_->set_is_horizontal_alignment(is_horizontal_alignment);
  right_arrow_->set_is_horizontal_alignment(is_horizontal_alignment);
  scroll_offset_ = gfx::Vector2dF();
  ScrollToMainOffset(CalculateMainAxisScrollDistance(), /*animating=*/false);
  DeprecatedLayoutImmediately();
}

void ScrollableShelfView::OnShelfConfigUpdated() {
  UpdateAvailableSpaceAndScroll();
  shelf_view_->OnShelfConfigUpdated();
}

bool ScrollableShelfView::ShouldShowTooltipForView(
    const views::View* view) const {
  if (!view || !view->parent())
    return false;

  if (view == left_arrow_ || view == right_arrow_)
    return true;

  // TODO(b/288898225): Move shelf tooltip manager delegate implementation
  // outside of `ScrollableShelfView` now that it deals with views outside the
  // `ScrollableShelfView`.
  if (DeskButtonWidget* desk_button_widget = GetShelf()->desk_button_widget()) {
    DeskButtonContainer* desk_button_container =
        desk_button_widget->GetDeskButtonContainer();
    if (view->parent() == desk_button_container && view->GetEnabled() &&
        !desk_button_container->GetTitleForView(view).empty()) {
      return true;
    }
  }

  if (view->parent() != shelf_view_)
    return false;

  // The shelf item corresponding to |view| may have been removed from the
  // model.
  if (!shelf_view_->ShouldShowTooltipForChildView(view))
    return false;

  const gfx::Rect screen_bounds = view->GetBoundsInScreen();
  gfx::Rect visible_bounds_in_screen = visible_space_;
  views::View::ConvertRectToScreen(this, &visible_bounds_in_screen);

  return visible_bounds_in_screen.Contains(screen_bounds);
}

bool ScrollableShelfView::ShouldHideTooltip(const gfx::Point& cursor_location,
                                            views::View* delegate_view) const {
  if (DeskButtonWidget* desk_button_widget = GetShelf()->desk_button_widget()) {
    DeskButtonContainer* desk_button_container =
        desk_button_widget->GetDeskButtonContainer();
    if (delegate_view == desk_button_container) {
      return !desk_button_container->GetLocalBounds().Contains(cursor_location);
    }
  }

  if ((ShouldShowLeftArrow() &&
       left_arrow_->GetMirroredBounds().Contains(cursor_location)) ||
      (ShouldShowRightArrow() &&
       right_arrow_->GetMirroredBounds().Contains(cursor_location))) {
    return false;
  }

  // Should hide the tooltip if |cursor_location| is not in |visible_space_|.
  if (!visible_space_.Contains(cursor_location))
    return true;

  gfx::Point location_in_shelf_view = cursor_location;
  views::View::ConvertPointToTarget(this, shelf_view_, &location_in_shelf_view);
  return shelf_view_->ShouldHideTooltip(location_in_shelf_view, delegate_view);
}

const std::vector<aura::Window*> ScrollableShelfView::GetOpenWindowsForView(
    views::View* view) {
  if (!view || view->parent() != shelf_view_)
    return std::vector<aura::Window*>();

  return shelf_view_->GetOpenWindowsForView(view);
}

std::u16string ScrollableShelfView::GetTitleForView(
    const views::View* view) const {
  if (!view || !view->parent())
    return std::u16string();

  if (view->parent() == shelf_view_)
    return shelf_view_->GetTitleForView(view);

  if (DeskButtonWidget* desk_button_widget = GetShelf()->desk_button_widget()) {
    DeskButtonContainer* desk_button_container =
        desk_button_widget->GetDeskButtonContainer();
    if (view->parent() == desk_button_container) {
      return desk_button_container->GetTitleForView(view);
    }
  }

  if (view == left_arrow_)
    return l10n_util::GetStringUTF16(IDS_SHELF_PREVIOUS);

  if (view == right_arrow_)
    return l10n_util::GetStringUTF16(IDS_SHELF_NEXT);

  return std::u16string();
}

views::View* ScrollableShelfView::GetViewForEvent(const ui::Event& event) {
  if (event.target() == GetWidget()->GetNativeWindow())
    return this;

  if (DeskButtonWidget* desk_button_widget = GetShelf()->desk_button_widget()) {
    if (event.target() == desk_button_widget->GetNativeWindow()) {
      return desk_button_widget->GetDeskButtonContainer();
    }
  }

  return nullptr;
}

void ScrollableShelfView::ScheduleScrollForItemDragIfNeeded(
    const gfx::Rect& item_bounds_in_screen) {
  gfx::Rect visible_space_in_screen = visible_space_;
  views::View::ConvertRectToScreen(this, &visible_space_in_screen);

  drag_item_bounds_in_screen_.emplace(item_bounds_in_screen);
  if (AreBoundsWithinVisibleSpace(*drag_item_bounds_in_screen_)) {
    page_flip_timer_.AbandonAndStop();
    return;
  }

  if (!page_flip_timer_.IsRunning()) {
    page_flip_timer_.Start(FROM_HERE, page_flip_time_threshold_, this,
                           &ScrollableShelfView::OnPageFlipTimer);
  }
}
void ScrollableShelfView::CancelScrollForItemDrag() {
  drag_item_bounds_in_screen_.reset();
  if (page_flip_timer_.IsRunning())
    page_flip_timer_.AbandonAndStop();
}

void ScrollableShelfView::OnImplicitAnimationsCompleted() {
  during_scroll_animation_ = false;
  DeprecatedLayoutImmediately();

  EnableShelfRoundedCorners(/*enable=*/false);

  if (scroll_status_ != kAlongMainAxisScroll)
    UpdateTappableIconIndices();

  // Notifies ChromeVox of the changed location at the end of animation.
  shelf_view_->NotifyAccessibilityEvent(ax::mojom::Event::kLocationChanged,
                                        /*send_native_event=*/true);

  if (!drag_item_bounds_in_screen_ ||
      AreBoundsWithinVisibleSpace(*drag_item_bounds_in_screen_)) {
    return;
  }

  // Keep scrolling if the dragged shelf item is outside of the visible space.
  page_flip_timer_.Start(FROM_HERE, page_flip_time_threshold_, this,
                         &ScrollableShelfView::OnPageFlipTimer);
}

bool ScrollableShelfView::ShouldShowLeftArrow() const {
  return (layout_strategy_ == kShowLeftArrowButton) ||
         (layout_strategy_ == kShowButtons);
}

bool ScrollableShelfView::ShouldShowRightArrow() const {
  return (layout_strategy_ == kShowRightArrowButton) ||
         (layout_strategy_ == kShowButtons);
}

gfx::Rect ScrollableShelfView::GetAvailableLocalBounds(
    bool use_target_bounds) const {
  return use_target_bounds
             ? gfx::Rect(GetShelf()->hotseat_widget()->GetTargetBounds().size())
             : GetLocalBounds();
}

gfx::Insets ScrollableShelfView::CalculateMirroredPaddingForDisplayCentering(
    bool use_target_bounds) const {
  const int icons_size =
      shelf_view_->GetSizeOfAppButtons(shelf_view_->number_of_visible_apps(),
                                       shelf_view_->GetButtonSize()) +
      2 * ShelfConfig::Get()->GetAppIconEndPadding();
  const gfx::Rect display_bounds =
      screen_util::GetDisplayBoundsWithShelf(GetWidget()->GetNativeWindow());
  const int display_size_primary = GetShelf()->PrimaryAxisValue(
      display_bounds.width(), display_bounds.height());
  const int gap = (display_size_primary - icons_size) / 2;

  // Calculates paddings in view coordinates.
  const gfx::Rect screen_bounds =
      use_target_bounds ? GetShelf()->hotseat_widget()->GetTargetBounds()
                        : GetBoundsInScreen();
  int before_padding =
      gap - GetShelf()->PrimaryAxisValue(
                ShouldAdaptToRTL()
                    ? display_bounds.right() - screen_bounds.right()
                    : screen_bounds.x() - display_bounds.x(),
                screen_bounds.y() - display_bounds.y());
  int after_padding =
      gap - GetShelf()->PrimaryAxisValue(
                ShouldAdaptToRTL()
                    ? screen_bounds.x() - display_bounds.x()
                    : display_bounds.right() - screen_bounds.right(),
                display_bounds.bottom() - screen_bounds.bottom());

  // Checks whether there is enough space to ensure |base_padding_|. Returns
  // empty insets if not.
  if (before_padding < 0 || after_padding < 0)
    return gfx::Insets();

  gfx::Insets padding_insets;
  if (GetShelf()->IsHorizontalAlignment()) {
    padding_insets = gfx::Insets::TLBR(0, before_padding, 0, after_padding);
    if (ShouldAdaptToRTL())
      padding_insets = GetMirroredInsets(padding_insets);
  } else {
    padding_insets = gfx::Insets::TLBR(before_padding, 0, after_padding, 0);
  }

  return padding_insets;
}

bool ScrollableShelfView::ShouldHandleGestures(const ui::GestureEvent& event) {
  // ScrollableShelfView only handles the gesture scrolling along the main axis.
  // For other gesture events, including the scrolling across the main axis,
  // they are handled by ShelfView.

  if (scroll_status_ == kNotInScroll && !event.IsScrollGestureEvent())
    return false;

  if (event.type() == ui::EventType::kGestureScrollBegin) {
    CHECK_EQ(scroll_status_, kNotInScroll);

    float main_offset = event.details().scroll_x_hint();
    float cross_offset = event.details().scroll_y_hint();
    if (!GetShelf()->IsHorizontalAlignment())
      std::swap(main_offset, cross_offset);

    if (std::abs(main_offset) < std::abs(cross_offset)) {
      scroll_status_ = kAcrossMainAxisScroll;
    } else if (layout_strategy_ != kNotShowArrowButtons) {
      // Note that if the scrollable shelf is not in overflow mode, scroll along
      // the main axis should not make any UI differences. Do not handle scroll
      // in this scenario.
      scroll_status_ = kAlongMainAxisScroll;
    }
  }

  bool should_handle_gestures = scroll_status_ == kAlongMainAxisScroll;

  if (scroll_status_ == kAlongMainAxisScroll &&
      event.type() == ui::EventType::kGestureScrollBegin) {
    scroll_offset_before_main_axis_scrolling_ = scroll_offset_;
    layout_strategy_before_main_axis_scrolling_ = layout_strategy_;

    // The change in |scroll_status_| may lead to update on the gradient zone.
    MaybeUpdateGradientZone();
  }

  if (event.type() == ui::EventType::kGestureEnd) {
    ResetScrollStatus();
  }

  return should_handle_gestures;
}

void ScrollableShelfView::ResetScrollStatus() {
  scroll_status_ = kNotInScroll;
  scroll_offset_before_main_axis_scrolling_ = gfx::Vector2dF();
  layout_strategy_before_main_axis_scrolling_ = kNotShowArrowButtons;

  // The change in |scroll_status_| may lead to update on the gradient zone.
  MaybeUpdateGradientZone();
}

bool ScrollableShelfView::ProcessGestureEvent(const ui::GestureEvent& event) {
  if (layout_strategy_ == kNotShowArrowButtons)
    return true;

  // Handle scroll-related events, but don't do anything special for begin and
  // end.
  if (event.type() == ui::EventType::kGestureScrollBegin) {
    DCHECK(!presentation_time_recorder_);
    if (Shell::Get()->IsInTabletMode()) {
      if (Shell::Get()->app_list_controller()->IsVisible(
              GetDisplayIdForView(this))) {
        presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
            GetWidget()->GetCompositor(),
            scrollable_shelf_constants::
                kScrollDraggingTabletLauncherVisibleHistogram,
            scrollable_shelf_constants::
                kScrollDraggingTabletLauncherVisibleMaxLatencyHistogram);
      } else {
        presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
            GetWidget()->GetCompositor(),
            scrollable_shelf_constants::
                kScrollDraggingTabletLauncherHiddenHistogram,
            scrollable_shelf_constants::
                kScrollDraggingTabletLauncherHiddenMaxLatencyHistogram);
      }
    } else {
      if (Shell::Get()->app_list_controller()->IsVisible(
              GetDisplayIdForView(this))) {
        presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
            GetWidget()->GetCompositor(),
            scrollable_shelf_constants::
                kScrollDraggingClamshellLauncherVisibleHistogram,
            scrollable_shelf_constants::
                kScrollDraggingClamshellLauncherVisibleMaxLatencyHistogram);
      } else {
        presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
            GetWidget()->GetCompositor(),
            scrollable_shelf_constants::
                kScrollDraggingClamshellLauncherHiddenHistogram,
            scrollable_shelf_constants::
                kScrollDraggingClamshellLauncherHiddenMaxLatencyHistogram);
      }
    }
    return true;
  }

  if (event.type() == ui::EventType::kGestureEnd) {
    // Do not reset |presentation_time_recorder_| in
    // ui::EventType::kGestureScrollEnd event because it may not exist due to
    // gesture fling.
    presentation_time_recorder_.reset();

    // The type of scrolling offset is float to ensure that ScrollableShelfView
    // is responsive to slow gesture scrolling. However, after offset
    // adjustment, the scrolling offset should be floored.
    scroll_offset_ = gfx::ToFlooredVector2d(scroll_offset_);

    // If the scroll animation is created, tappable icon indices are updated
    // at the end of animation.
    if (!AdjustOffset() && !during_scroll_animation_)
      UpdateTappableIconIndices();
    return true;
  }

  if (event.type() == ui::EventType::kScrollFlingStart) {
    const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();
    if (!ShouldHandleScroll(gfx::Vector2dF(event.details().velocity_x(),
                                           event.details().velocity_y()),
                            /*is_gesture_fling=*/true)) {
      return false;
    }

    int scroll_velocity = is_horizontal_alignment
                              ? event.details().velocity_x()
                              : event.details().velocity_y();
    if (ShouldAdaptToRTL())
      scroll_velocity = -scroll_velocity;
    float page_scrolling_offset = CalculatePageScrollingOffset(
        scroll_velocity < 0, layout_strategy_before_main_axis_scrolling_);

    // Only starts animation when scroll distance is greater than zero.
    if (std::fabs(page_scrolling_offset) > 0.f) {
      ScrollToMainOffset((is_horizontal_alignment
                              ? scroll_offset_before_main_axis_scrolling_.x()
                              : scroll_offset_before_main_axis_scrolling_.y()) +
                             page_scrolling_offset,
                         /*animating=*/true);
    }

    return true;
  }

  if (event.type() != ui::EventType::kGestureScrollUpdate) {
    return false;
  }

  float scroll_delta = 0.f;
  const bool is_horizontal = GetShelf()->IsHorizontalAlignment();
  if (is_horizontal) {
    scroll_delta = -event.details().scroll_x();
    scroll_delta = ShouldAdaptToRTL() ? -scroll_delta : scroll_delta;
  } else {
    scroll_delta = -event.details().scroll_y();
  }

  // Return early if scrollable shelf cannot be scrolled anymore because it has
  // reached to the end.
  const float current_scroll_offset = CalculateMainAxisScrollDistance();
  if ((current_scroll_offset == 0.f && scroll_delta <= 0.f) ||
      (current_scroll_offset == CalculateScrollUpperBound(GetSpaceForIcons()) &&
       scroll_delta >= 0.f)) {
    return true;
  }

  DCHECK(presentation_time_recorder_);
  presentation_time_recorder_->RequestNext();

  if (is_horizontal)
    ScrollByXOffset(scroll_delta, /*animate=*/false);
  else
    ScrollByYOffset(scroll_delta, /*animate=*/false);

  return true;
}

void ScrollableShelfView::HandleMouseWheelEvent(ui::MouseWheelEvent* event) {
  // Note that the scrolling from touchpad is propagated as mouse wheel event.
  // Let the shelf handle mouse wheel events over the empty area of the shelf
  // view, as these events would be ignored by the scrollable shelf view.
  gfx::Point location_in_shelf_view = event->location();
  View::ConvertPointToTarget(this, shelf_view_, &location_in_shelf_view);
  if (!shelf_view_->LocationInsideVisibleShelfItemBounds(
          location_in_shelf_view)) {
    GetShelf()->ProcessMouseWheelEvent(event);
    return;
  }

  if (!ShouldHandleScroll(gfx::Vector2dF(event->x_offset(), event->y_offset()),
                          /*is_gesture_fling=*/false)) {
    return;
  }

  event->SetHandled();

  // Scrolling the mouse wheel may create multiple mouse wheel events at the
  // same time. If the scrollable shelf view is during scrolling animation at
  // this moment, do not handle the mouse wheel event.
  if (shelf_view_->layer()->GetAnimator()->is_animating())
    return;

  if (GetShelf()->IsHorizontalAlignment()) {
    const float x_offset = event->x_offset();
    const float y_offset = event->y_offset();
    // If the shelf is bottom aligned, we can scroll over the shelf contents if
    // the scroll is horizontal or vertical (in the case of a mousewheel
    // scroll). We take the biggest offset difference of the vertical and
    // horizontal components to determine the offset to scroll over the
    // contents.
    float max_absolute_offset =
        abs(x_offset) > abs(y_offset) ? x_offset : y_offset;
    ScrollByXOffset(
        CalculatePageScrollingOffset(max_absolute_offset < 0, layout_strategy_),
        /*animating=*/true);
  } else {
    ScrollByYOffset(
        CalculatePageScrollingOffset(event->y_offset() < 0, layout_strategy_),
        /*animating=*/true);
  }
}

void ScrollableShelfView::ScrollByXOffset(float x_offset, bool animating) {
  ScrollToMainOffset(scroll_offset_.x() + x_offset, animating);
}

void ScrollableShelfView::ScrollByYOffset(float y_offset, bool animating) {
  ScrollToMainOffset(scroll_offset_.y() + y_offset, animating);
}

void ScrollableShelfView::ScrollToMainOffset(float target_offset,
                                             bool animating) {
  if (animating) {
    StartShelfScrollAnimation(target_offset);
  } else {
    UpdateScrollOffset(target_offset);
    shelf_container_view_->TranslateShelfView(scroll_offset_);
  }
}

float ScrollableShelfView::CalculatePageScrollingOffset(
    bool forward,
    LayoutStrategy layout_strategy) const {
  // Returns zero if inputs are invalid.
  const bool invalid = (layout_strategy == kNotShowArrowButtons) ||
                       (layout_strategy == kShowLeftArrowButton && forward) ||
                       (layout_strategy == kShowRightArrowButton && !forward);
  if (invalid)
    return 0;

  float offset = CalculatePageScrollingOffsetInAbs(layout_strategy);

  if (!forward)
    offset = -offset;

  return offset;
}

float ScrollableShelfView::CalculatePageScrollingOffsetInAbs(
    LayoutStrategy layout_strategy) const {
  // Implement the arrow button handler in the same way with the gesture
  // scrolling. The key is to calculate the suitable scroll distance.

  float offset = 0.f;

  // The available space for icons excluding the area taken by arrow button(s).
  int space_excluding_arrow;

  const int space_needed_for_button = GetSumOfButtonSizeAndSpacing();

  if (layout_strategy == kShowRightArrowButton) {
    space_excluding_arrow =
        GetSpaceForIcons() - scrollable_shelf_constants::kArrowButtonGroupWidth;

    // After scrolling, the left arrow button will show. Adapts the offset
    // to the extra arrow button.
    const int offset_for_extra_arrow =
        scrollable_shelf_constants::kArrowButtonGroupWidth -
        ShelfConfig::Get()->GetAppIconEndPadding();

    const int mod = space_excluding_arrow % space_needed_for_button;
    offset = space_excluding_arrow - mod - offset_for_extra_arrow;
  } else if (layout_strategy == kShowButtons ||
             layout_strategy == kShowLeftArrowButton) {
    space_excluding_arrow =
        GetSpaceForIcons() -
        2 * scrollable_shelf_constants::kArrowButtonGroupWidth;
    const int mod = space_excluding_arrow % space_needed_for_button;
    offset = space_excluding_arrow - mod;

    // Layout of kShowLeftArrowButton can be regarded as the layout of
    // kShowButtons with extra offset.
    if (layout_strategy == kShowLeftArrowButton) {
      const int extra_offset =
          -ShelfConfig::Get()->button_spacing() -
          (GetSpaceForIcons() -
           scrollable_shelf_constants::kArrowButtonGroupWidth) %
              space_needed_for_button +
          ShelfConfig::Get()->GetAppIconEndPadding();
      offset += extra_offset;
    }
  }

  // Ensure the return value to be non-negative. Note that if the screen is too
  // small (usually on the Linux emulator), `offset` may be negative.
  return std::fmax(offset, 0.f);
}

float ScrollableShelfView::CalculateTargetOffsetAfterScroll(
    float start_offset,
    float scroll_distance) const {
  float target_offset = start_offset;

  target_offset += scroll_distance;
  target_offset =
      CalculateClampedScrollOffset(target_offset, GetSpaceForIcons());
  LayoutStrategy layout_strategy_after_scroll =
      CalculateLayoutStrategy(target_offset, GetSpaceForIcons());
  target_offset = CalculateScrollDistanceAfterAdjustment(
      target_offset, layout_strategy_after_scroll);

  return target_offset;
}

void ScrollableShelfView::CalculateHorizontalGradient(
    gfx::LinearGradient* gradient_mask) {
  auto get_clamped = [](int position, int total) -> float {
    return std::clamp(static_cast<float>(position) / total, 0.f, 1.f);
  };

  // Do not add gradient if visible width is too narrow.
  if (visible_space_.right() <
      visible_space_.x() +
          2 * scrollable_shelf_constants::kGradientZoneLength) {
    return;
  }

  float gradient_start, gradient_end;

  const bool rtl = ShouldAdaptToRTL();

  // Horizontal linear gradient, from left to right
  gradient_mask->set_angle(0);

  // If true, create a gradient area that fades in the shelf app buttons at
  // the beginning
  const bool show_fade_in =
      rtl ? should_show_end_gradient_zone_ : should_show_start_gradient_zone_;
  if (show_fade_in) {
    gradient_start = get_clamped((visible_space_.x() - 1), width());
    gradient_end = get_clamped(
        (visible_space_.x() + scrollable_shelf_constants::kGradientZoneLength),
        width());

    // When the scroll arrow button shows, `gradient_start` is greater than 0.
    // Ensure that the area in the range [0, gradient_start) has an opaque
    // opacity so that the scroll arrow button is visible.
    if (gradient_start > 0) {
      gradient_mask->AddStep(0, /*alpha=*/255);
      gradient_mask->AddStep(get_clamped((visible_space_.x() - 2), width()),
                             255);
    }
    gradient_mask->AddStep(gradient_start, 0);
    gradient_mask->AddStep(gradient_end, 255);
  }

  // If true, create a gradient area that fades out the shelf app buttons at
  // the end
  bool show_fade_out =
      rtl ? should_show_start_gradient_zone_ : should_show_end_gradient_zone_;
  if (show_fade_out) {
    gradient_start =
        get_clamped((visible_space_.right() -
                     scrollable_shelf_constants::kGradientZoneLength),
                    width());
    gradient_end = get_clamped((visible_space_.right() + 1), width());
    gradient_mask->AddStep(gradient_start, /*alpha=*/255);
    gradient_mask->AddStep(gradient_end, 0);

    // When the scroll arrow button shows, `gradient_end` is less than 1.
    // Ensure that the area in the range (gradient_end, 1] has an opaque
    // opacity so that the scroll arrow button is visible.
    if (gradient_end < 1) {
      gradient_mask->AddStep(get_clamped((visible_space_.right() + 2), width()),
                             255);
      gradient_mask->AddStep(1, 255);
    }
  }
}

void ScrollableShelfView::CalculateVerticalGradient(
    gfx::LinearGradient* gradient_mask) {
  auto get_clamped = [](int position, int total) -> float {
    return std::clamp(static_cast<float>(position) / total, 0.f, 1.f);
  };

  // Do not add gradient if visible height is too small.
  if (visible_space_.bottom() <
      visible_space_.y() +
          2 * scrollable_shelf_constants::kGradientZoneLength) {
    return;
  }

  float gradient_start, gradient_end;

  DCHECK(!ShouldAdaptToRTL());

  // Vertical gradient from top to bottom.
  gradient_mask->set_angle(-90);

  if (should_show_start_gradient_zone_) {
    gradient_start = get_clamped((visible_space_.y() - 1), height());
    gradient_end = get_clamped(
        (visible_space_.y() + scrollable_shelf_constants::kGradientZoneLength),
        height());

    // When the scroll arrow button shows, `gradient_start` is greater than 0.
    // Ensure that the area in the range [0, gradient_start) has an opaque
    // opacity so that the scroll arrow button is visible.
    if (gradient_start > 0) {
      gradient_mask->AddStep(0, /*alpha=*/255);
      gradient_mask->AddStep(get_clamped((visible_space_.y() - 2), height()),
                             255);
    }
    gradient_mask->AddStep(gradient_start, 0);
    gradient_mask->AddStep(gradient_end, 255);
  }

  if (should_show_end_gradient_zone_) {
    gradient_start =
        get_clamped((visible_space_.bottom() -
                     scrollable_shelf_constants::kGradientZoneLength),
                    height());
    gradient_end = get_clamped((visible_space_.bottom() + 1), height());
    gradient_mask->AddStep(gradient_start,
                           /*alpha=*/255);
    gradient_mask->AddStep(gradient_end, 0);

    // When the scroll arrow button shows, `gradient_end` is less than 1.
    // Ensure that the area in the range (gradient_end, 1] has an opaque
    // opacity so that the scroll arrow button is visible.
    if (gradient_end < 1) {
      gradient_mask->AddStep(
          get_clamped((visible_space_.bottom() + 2), height()), 255);
      gradient_mask->AddStep(1, 255);
    }
  }
}

void ScrollableShelfView::UpdateGradientMask() {
  // There is no visible shelf app buttons so return early
  if (bounds().IsEmpty() || visible_space_.IsEmpty())
    return;

  gfx::LinearGradient gradient_mask;

  if (GetShelf()->IsHorizontalAlignment()) {
    CalculateHorizontalGradient(&gradient_mask);
  } else {
    CalculateVerticalGradient(&gradient_mask);
  }

  // Return if the gradients do not change.
  if (gradient_mask == layer()->gradient_mask())
    return;

  layer()->SetGradientMask(gradient_mask);
}

void ScrollableShelfView::UpdateGradientZoneState() {
  // The gradient zone is not painted when the focus ring shows in order to
  // display the focus ring correctly.
  if (focus_ring_activated_) {
    should_show_start_gradient_zone_ = false;
    should_show_end_gradient_zone_ = false;
    return;
  }

  if (during_scroll_animation_) {
    should_show_start_gradient_zone_ = true;
    should_show_end_gradient_zone_ = true;
    return;
  }

  should_show_start_gradient_zone_ = layout_strategy_ == kShowLeftArrowButton ||
                                     (layout_strategy_ == kShowButtons &&
                                      scroll_status_ == kAlongMainAxisScroll);
  should_show_end_gradient_zone_ = ShouldShowRightArrow();
}

void ScrollableShelfView::MaybeUpdateGradientZone() {
  if (!ShouldApplyMaskLayerGradientZone())
    return;

  // Fade zones should be updated if:
  // (1) Fade zone's visibility changes.
  // (2) Fade zone should show and the arrow button's location changes.
  UpdateGradientZoneState();
  UpdateGradientMask();
}

bool ScrollableShelfView::ShouldApplyMaskLayerGradientZone() const {
  return layout_strategy_ != LayoutStrategy::kNotShowArrowButtons;
}

float ScrollableShelfView::GetActualScrollOffset(
    float main_axis_scroll_distance,
    LayoutStrategy layout_strategy) const {
  return (layout_strategy == kShowButtons ||
          layout_strategy == kShowLeftArrowButton)
             ? (main_axis_scroll_distance +
                scrollable_shelf_constants::kArrowButtonGroupWidth -
                ShelfConfig::Get()->GetAppIconEndPadding())
             : main_axis_scroll_distance;
}

void ScrollableShelfView::UpdateTappableIconIndices() {
  // Scrollable shelf should be not under the scroll along the main axis, which
  // means that the decimal part of the main scroll offset should be zero.
  DCHECK(scroll_status_ != kAlongMainAxisScroll);

  // The value returned by CalculateMainAxisScrollDistance() can be casted into
  // an integer without losing precision since the decimal part is zero.
  const auto tappable_indices = CalculateTappableIconIndices(
      layout_strategy_, CalculateMainAxisScrollDistance());
  first_tappable_app_index_ = tappable_indices.first;
  last_tappable_app_index_ = tappable_indices.second;
}

std::pair<std::optional<size_t>, std::optional<size_t>>
ScrollableShelfView::CalculateTappableIconIndices(
    ScrollableShelfView::LayoutStrategy layout_strategy,
    int scroll_distance_on_main_axis) const {
  const auto& visible_views_indices = shelf_view_->visible_views_indices();

  if (visible_views_indices.empty() || visible_space_.IsEmpty())
    return {std::nullopt, std::nullopt};

  if (layout_strategy == ScrollableShelfView::kNotShowArrowButtons) {
    return {visible_views_indices.front(), visible_views_indices.back()};
  }

  const int visible_size = GetShelf()->IsHorizontalAlignment()
                               ? visible_space_.width()
                               : visible_space_.height();

  const int space_needed_for_button = GetSumOfButtonSizeAndSpacing();

  // Note that some apps may have their |ShelfAppButton| views hidden, when they
  // are on an inactive desk. Therefore, the indices of tappable apps may not be
  // contiguous, so we need to map from a visible view index back to an app
  // index. The below are indices into the |visible_views_indices| vector.
  size_t first_visible_view_index;
  size_t last_visible_view_index;
  if (layout_strategy == kShowRightArrowButton ||
      layout_strategy == kShowButtons) {
    first_visible_view_index =
        scroll_distance_on_main_axis / space_needed_for_button +
        (layout_strategy == kShowButtons ? 1 : 0);
    last_visible_view_index =
        first_visible_view_index + visible_size / space_needed_for_button;

    const int end_of_last_visible_view =
        last_visible_view_index * space_needed_for_button +
        shelf_view_->GetButtonSize() - scroll_distance_on_main_axis;

    // It is very rare but |visible_size| may be smaller than
    // |space_needed_for_button| as reported in https://crbug.com/1094363.
    if (end_of_last_visible_view > visible_size &&
        last_visible_view_index > first_visible_view_index) {
      last_visible_view_index--;
    }
  } else {
    DCHECK_EQ(layout_strategy, kShowLeftArrowButton);
    last_visible_view_index = visible_views_indices.size() - 1;

    // In fuzz tests, `visible_size` may be smaller than
    // `space_needed_for_button` although it never happens on real devices.
    first_visible_view_index =
        visible_size >= space_needed_for_button
            ? last_visible_view_index - visible_size / space_needed_for_button +
                  1
            : last_visible_view_index;
  }

  DCHECK_LT(first_visible_view_index, visible_views_indices.size());
  DCHECK_LT(last_visible_view_index, visible_views_indices.size());

  // Ensure that each visible view index is within the bounds of
  // `visible_views_indices`.
  // TODO(b/268401797): Rewrite CalculateTappableIconIndices() as a more
  // thorough fix for out of bound indices.
  first_visible_view_index =
      std::clamp(first_visible_view_index, static_cast<size_t>(0),
                 visible_views_indices.size() - 1);
  last_visible_view_index =
      std::clamp(last_visible_view_index, first_visible_view_index,
                 visible_views_indices.size() - 1);

  return {visible_views_indices[first_visible_view_index],
          visible_views_indices[last_visible_view_index]};
}

views::View* ScrollableShelfView::FindFirstFocusableChild() {
  return shelf_view_->FindFirstFocusableChild();
}

views::View* ScrollableShelfView::FindLastFocusableChild() {
  return shelf_view_->FindLastFocusableChild();
}

int ScrollableShelfView::GetSpaceForIcons() const {
  return GetShelf()->IsHorizontalAlignment() ? available_space_.width()
                                             : available_space_.height();
}

bool ScrollableShelfView::CanFitAllAppsWithoutScrolling(
    const gfx::Size& available_size,
    const gfx::Size& icons_preferred_size) const {
  const int available_length =
      (GetShelf()->IsHorizontalAlignment() ? available_size.width()
                                           : available_size.height());

  int preferred_length = GetShelf()->IsHorizontalAlignment()
                             ? icons_preferred_size.width()
                             : icons_preferred_size.height();
  preferred_length += 2 * ShelfConfig::Get()->GetAppIconEndPadding();

  return available_length >= preferred_length;
}

bool ScrollableShelfView::ShouldHandleScroll(const gfx::Vector2dF& offset,
                                             bool is_gesture_scrolling) const {
  // When the shelf is aligned at the bottom, a horizontal mousewheel scroll may
  // also be handled by the ScrollableShelf if the offset along the main axis is
  // 0. This case is mainly triggered by an event generated in the MouseWheel,
  // but not in the touchpad, as touchpads events are caught on ScrollEvent.
  // If there is an x component to the scroll, consider this instead of the y
  // axis because the horizontal scroll could move the scrollable shelf.
  const float main_axis_offset =
      GetShelf()->IsHorizontalAlignment() && offset.x() != 0 ? offset.x()
                                                             : offset.y();

  const int threshold =
      is_gesture_scrolling
          ? scrollable_shelf_constants::kGestureFlingVelocityThreshold
          : scrollable_shelf_constants::kScrollOffsetThreshold;
  return abs(main_axis_offset) > threshold;
}

bool ScrollableShelfView::AdjustOffset() {
  const float offset = CalculateAdjustmentOffset(
      CalculateMainAxisScrollDistance(), layout_strategy_, GetSpaceForIcons());

  // Returns early when it does not need to adjust the shelf view's location.
  if (!offset)
    return false;

  if (GetShelf()->IsHorizontalAlignment())
    ScrollByXOffset(offset, /*animate=*/true);
  else
    ScrollByYOffset(offset, /*animate=*/true);

  return true;
}

float ScrollableShelfView::CalculateAdjustmentOffset(
    int main_axis_scroll_distance,
    LayoutStrategy layout_strategy,
    int available_space_for_icons) const {
  // Scrollable shelf should be not under the scroll along the main axis, which
  // means that the decimal part of the main scroll offset should be zero.
  DCHECK(scroll_status_ != kAlongMainAxisScroll);

  // Returns early when it does not need to adjust the shelf view's location.
  if (layout_strategy == kNotShowArrowButtons ||
      main_axis_scroll_distance >=
          CalculateScrollUpperBound(available_space_for_icons)) {
    return 0;
  }

  // Because the decimal part of the scroll offset is zero, it is meaningful
  // to use modulo operation here.
  const int remainder = static_cast<int>(GetActualScrollOffset(
                            main_axis_scroll_distance, layout_strategy)) %
                        GetSumOfButtonSizeAndSpacing();
  int offset = remainder > GetGestureDragThreshold()
                   ? GetSumOfButtonSizeAndSpacing() - remainder
                   : -remainder;

  return offset;
}

int ScrollableShelfView::CalculateScrollDistanceAfterAdjustment(
    int main_axis_scroll_distance,
    LayoutStrategy layout_strategy) const {
  return main_axis_scroll_distance +
         CalculateAdjustmentOffset(main_axis_scroll_distance, layout_strategy,
                                   GetSpaceForIcons());
}

void ScrollableShelfView::UpdateAvailableSpace() {
  if (!is_padding_configured_externally_) {
    edge_padding_insets_ =
        CalculateMirroredEdgePadding(/*use_target_bounds=*/false);
  }

  available_space_ = GetLocalBounds();
  available_space_.Inset(edge_padding_insets_);

  // The hotseat uses |available_space_| to determine where to show its
  // background, so notify it when it is recalculated.
  if (HotseatWidget::ShouldShowHotseatBackground())
    GetShelf()->hotseat_widget()->UpdateTranslucentBackground();
}

gfx::Rect ScrollableShelfView::CalculateVisibleSpace(
    LayoutStrategy layout_strategy) const {
  const bool in_tablet_mode = Shell::Get()->IsInTabletMode();
  if (layout_strategy == kNotShowArrowButtons && !in_tablet_mode)
    return GetAvailableLocalBounds(/*use_target_bounds=*/false);

  const bool should_show_left_arrow =
      (layout_strategy == kShowLeftArrowButton) ||
      (layout_strategy == kShowButtons);
  const bool should_show_right_arrow =
      (layout_strategy == kShowRightArrowButton) ||
      (layout_strategy == kShowButtons);

  const int before_padding =
      (should_show_left_arrow
           ? scrollable_shelf_constants::kArrowButtonGroupWidth
           : 0);
  const int after_padding =
      (should_show_right_arrow
           ? scrollable_shelf_constants::kArrowButtonGroupWidth
           : 0);

  gfx::Insets visible_space_insets;
  if (ShouldAdaptToRTL()) {
    visible_space_insets =
        gfx::Insets::TLBR(0, after_padding, 0, before_padding);
  } else {
    visible_space_insets =
        GetShelf()->IsHorizontalAlignment()
            ? gfx::Insets::TLBR(0, before_padding, 0, after_padding)
            : gfx::Insets::TLBR(before_padding, 0, after_padding, 0);
  }
  visible_space_insets -= CalculateRipplePaddingInsets();

  gfx::Rect visible_space = available_space_;
  visible_space.Inset(visible_space_insets);

  return visible_space;
}

gfx::Insets ScrollableShelfView::CalculateRipplePaddingInsets() const {
  // Indicates whether it is in tablet mode with hotseat enabled.
  const bool in_tablet_mode = display::Screen::GetScreen()->InTabletMode();

  const int ripple_padding =
      ShelfConfig::Get()->scrollable_shelf_ripple_padding();
  const int before_padding =
      (in_tablet_mode && !ShouldShowLeftArrow()) ? 0 : ripple_padding;
  const int after_padding =
      (in_tablet_mode && !ShouldShowRightArrow()) ? 0 : ripple_padding;

  if (ShouldAdaptToRTL())
    return gfx::Insets::TLBR(0, after_padding, 0, before_padding);

  return GetShelf()->IsHorizontalAlignment()
             ? gfx::Insets::TLBR(0, before_padding, 0, after_padding)
             : gfx::Insets::TLBR(before_padding, 0, after_padding, 0);
}

gfx::RoundedCornersF
ScrollableShelfView::CalculateShelfContainerRoundedCorners() const {
  if (!display::Screen::GetScreen()->InTabletMode()) {
    return gfx::RoundedCornersF();
  }

  const bool is_horizontal_alignment = GetShelf()->IsHorizontalAlignment();
  const float radius = (is_horizontal_alignment ? height() : width()) / 2.f;

  int upper_left = ShouldShowLeftArrow() ? 0 : radius;

  int upper_right;
  if (is_horizontal_alignment)
    upper_right = ShouldShowRightArrow() ? 0 : radius;
  else
    upper_right = ShouldShowLeftArrow() ? 0 : radius;

  int lower_right = ShouldShowRightArrow() ? 0 : radius;

  int lower_left;
  if (is_horizontal_alignment)
    lower_left = ShouldShowLeftArrow() ? 0 : radius;
  else
    lower_left = ShouldShowRightArrow() ? 0 : radius;

  if (ShouldAdaptToRTL()) {
    std::swap(upper_left, upper_right);
    std::swap(lower_left, lower_right);
  }

  return gfx::RoundedCornersF(upper_left, upper_right, lower_right, lower_left);
}

void ScrollableShelfView::OnPageFlipTimer() {
  gfx::Rect visible_space_in_screen = visible_space_;
  views::View::ConvertRectToScreen(this, &visible_space_in_screen);

  // Calculates the page scrolling direction based on the drag item bounds and
  // the bounds of the visible space.
  bool should_scroll_to_next;
  if (ShouldAdaptToRTL()) {
    should_scroll_to_next =
        drag_item_bounds_in_screen_->x() < visible_space_in_screen.x();
  } else {
    should_scroll_to_next = GetShelf()->IsHorizontalAlignment()
                                ? drag_item_bounds_in_screen_->right() >
                                      visible_space_in_screen.right()
                                : drag_item_bounds_in_screen_->bottom() >
                                      visible_space_in_screen.bottom();
  }

  ScrollToNewPage(/*forward=*/should_scroll_to_next);

  if (test_observer_)
    test_observer_->OnPageFlipTimerFired();
}

bool ScrollableShelfView::AreBoundsWithinVisibleSpace(
    const gfx::Rect& bounds_in_screen) const {
  if (bounds_in_screen.IsEmpty())
    return false;

  gfx::Rect visible_space_in_screen = visible_space_;
  views::View::ConvertRectToScreen(this, &visible_space_in_screen);

  if (GetShelf()->IsHorizontalAlignment()) {
    return bounds_in_screen.x() >= visible_space_in_screen.x() &&
           bounds_in_screen.right() <= visible_space_in_screen.right();
  }

  return bounds_in_screen.y() >= visible_space_in_screen.y() &&
         bounds_in_screen.bottom() <= visible_space_in_screen.bottom();
}

bool ScrollableShelfView::ShouldDelegateScrollToShelf(
    const ui::ScrollEvent& event) const {
  // When the shelf is not aligned in the bottom, the events should be
  // propagated and handled as MouseWheel events.

  if (event.type() != ui::EventType::kScroll) {
    return false;
  }

  const float main_offset =
      GetShelf()->IsHorizontalAlignment() ? event.x_offset() : event.y_offset();
  const float cross_offset =
      GetShelf()->IsHorizontalAlignment() ? event.y_offset() : event.x_offset();

  // We only delegate to the shelf scroll events across the main axis,
  // otherwise, let them propagate and be handled as MouseWheel Events.
  return std::abs(main_offset) < std::abs(cross_offset);
}

float ScrollableShelfView::CalculateMainAxisScrollDistance() const {
  return GetShelf()->IsHorizontalAlignment() ? scroll_offset_.x()
                                             : scroll_offset_.y();
}

void ScrollableShelfView::UpdateScrollOffset(float target_offset) {
  target_offset =
      CalculateClampedScrollOffset(target_offset, GetSpaceForIcons());

  if (GetShelf()->IsHorizontalAlignment())
    scroll_offset_.set_x(target_offset);
  else
    scroll_offset_.set_y(target_offset);

  // Calculating the layout strategy relies on |scroll_offset_|.
  LayoutStrategy new_strategy = CalculateLayoutStrategy(
      CalculateMainAxisScrollDistance(), GetSpaceForIcons());

  const bool strategy_needs_update = (layout_strategy_ != new_strategy);
  if (strategy_needs_update) {
    layout_strategy_ = new_strategy;
    const bool has_gradient_zone = layer()->HasGradientMask();
    const bool should_have_gradient_zone = ShouldApplyMaskLayerGradientZone();
    if (has_gradient_zone && !should_have_gradient_zone) {
      layer()->SetGradientMask(gfx::LinearGradient::GetEmpty());
    }
    InvalidateLayout();
  }

  visible_space_ = CalculateVisibleSpace(layout_strategy_);

  if (scroll_status_ != kAlongMainAxisScroll)
    UpdateTappableIconIndices();
}

void ScrollableShelfView::UpdateAvailableSpaceAndScroll() {
  UpdateAvailableSpace();
  UpdateScrollOffset(CalculateMainAxisScrollDistance());
}

int ScrollableShelfView::CalculateScrollOffsetForTargetAvailableSpace(
    const gfx::Rect& target_space) const {
  // Ensures that the scroll offset is legal under the updated available space.
  const int available_space_for_icons =
      GetShelf()->PrimaryAxisValue(target_space.width(), target_space.height());
  int target_scroll_offset = CalculateClampedScrollOffset(
      CalculateMainAxisScrollDistance(), available_space_for_icons);

  // Calculates the layout strategy based on the new scroll offset.
  LayoutStrategy new_strategy =
      CalculateLayoutStrategy(target_scroll_offset, available_space_for_icons);

  // Adjusts the scroll offset with the new strategy.
  target_scroll_offset += CalculateAdjustmentOffset(
      target_scroll_offset, new_strategy, available_space_for_icons);

  return target_scroll_offset;
}

bool ScrollableShelfView::ShouldCountActivatedInkDrop(
    const views::View* sender) const {
  bool should_count = false;

  // When scrolling shelf by gestures, the shelf icon's ink drop ripple may be
  // activated accidentally. So ignore the ink drop activity during animation.
  if (during_scroll_animation_)
    return should_count;

  if (!first_tappable_app_index_.has_value() ||
      !last_tappable_app_index_.has_value()) {
    // Verify that `first_tappable_app_index_` and `last_tappable_app_index_`
    // are both illegal. In that case, return early.
    DCHECK(first_tappable_app_index_ == last_tappable_app_index_);
    return false;
  }

  // The ink drop needs to be clipped only if |sender| is the app at one of the
  // corners of the shelf. This happens if it is either the first or the last
  // tappable app and no arrow is showing on its side.
  if (shelf_view_->view_model()->view_at(first_tappable_app_index_.value()) ==
      sender) {
    should_count = !(layout_strategy_ == kShowButtons ||
                     layout_strategy_ == kShowLeftArrowButton);
  } else if (shelf_view_->view_model()->view_at(
                 last_tappable_app_index_.value()) == sender) {
    should_count = !(layout_strategy_ == kShowButtons ||
                     layout_strategy_ == kShowRightArrowButton);
  }

  return should_count;
}

void ScrollableShelfView::EnableShelfRoundedCorners(bool enable) {
  // Only enable shelf rounded corners in tablet mode. Note that we allow
  // disabling rounded corners in clamshell. Because when switching to clamshell
  // from tablet, this method may be called after tablet mode ends.
  if (enable && !display::Screen::GetScreen()->InTabletMode()) {
    return;
  }

  ui::Layer* layer = shelf_container_view_->layer();

  const bool has_rounded_corners = !(layer->rounded_corner_radii().IsEmpty());
  if (enable == has_rounded_corners)
    return;

  // In non-overflow mode, only apply layer clip on |shelf_container_view_|
  // when the ripple ring of the first/last shelf icon shows.
  // Note that |layout_strategy_| may update while EnableShelfRoundedCorners()
  // is not called. For example, the first icon's context menu shows, then the
  // system notification makes shelf view enter overflow mode. So
  // |layer_clip_in_non_overflow_| updates regardless of |layout_strategy_|.
  layer_clip_in_non_overflow_ = enable;

  if (layout_strategy_ == kNotShowArrowButtons)
    EnableLayerClipOnShelfContainerView(layer_clip_in_non_overflow_);

  layer->SetRoundedCornerRadius(enable ? CalculateShelfContainerRoundedCorners()
                                       : gfx::RoundedCornersF());

  if (!layer->is_fast_rounded_corner())
    layer->SetIsFastRoundedCorner(/*enable=*/true);
}

void ScrollableShelfView::OnActiveInkDropChange(bool increase) {
  if (increase)
    ++activated_corner_buttons_;
  else
    --activated_corner_buttons_;

  // When long pressing icons, sometimes there are more ripple animations
  // pending over others buttons. Only activate rounded corners when at least
  // one button needs them.
  // NOTE: `last_tappable_app_index_` is used to compute whether a button is
  // at the corner or not. Meanwhile, `last_tappable_app_index_` could update
  // before the button fade out animation ends. As a result, in edge cases
  // `activated_corner_buttons_` could be greater than 2.
  CHECK_GE(activated_corner_buttons_, 0);
  EnableShelfRoundedCorners(activated_corner_buttons_ > 0);
}

bool ScrollableShelfView::ShouldEnableLayerClip() const {
  // Always use layer clip in overflow mode.
  if (layout_strategy_ != LayoutStrategy::kNotShowArrowButtons)
    return true;

  // In clamshell, only use layer clip in overflow mode.
  if (!display::Screen::GetScreen()->InTabletMode()) {
    return false;
  }

  // In tablet mode, whether using layer clip in non-overflow mode depends on
  // |layer_clip_in_non_overflow_|.
  return layer_clip_in_non_overflow_;
}

void ScrollableShelfView::EnableLayerClipOnShelfContainerView(bool enable) {
  if (!enable) {
    shelf_container_view_->layer()->SetClipRect(gfx::Rect());
    return;
  }

  // |visible_space_| is in local coordinates. It should be transformed into
  // |shelf_container_view_|'s coordinates for layer clip.
  gfx::RectF visible_space_in_shelf_container_coordinates(visible_space_);
  views::View::ConvertRectToTarget(
      this, shelf_container_view_,
      &visible_space_in_shelf_container_coordinates);
  shelf_container_view_->layer()->SetClipRect(
      gfx::ToEnclosedRect(visible_space_in_shelf_container_coordinates));
}

int ScrollableShelfView::CalculateShelfIconsPreferredLength() const {
  const gfx::Size shelf_preferred_size(
      shelf_container_view_->GetPreferredSize());
  const int preferred_length =
      (GetShelf()->IsHorizontalAlignment() ? shelf_preferred_size.width()
                                           : shelf_preferred_size.height());
  return preferred_length + 2 * ShelfConfig::Get()->GetAppIconEndPadding();
}

BEGIN_METADATA(ScrollableShelfView)
END_METADATA

}  // namespace ash

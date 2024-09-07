// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle/window_cycle_view.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/system_shadow.h"
#include "ash/style/tab_slider.h"
#include "ash/style/tab_slider_button.h"
#include "ash/utility/occlusion_tracker_pauser.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_cycle/window_cycle_item_view.h"
#include "ash/wm/window_cycle/window_cycle_list.h"
#include "ash/wm/window_mini_view.h"
#include "ash/wm/wm_constants.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Shield rounded corner radius.
constexpr int kBackgroundCornerRadius = 16;

// Shield horizontal inset.
constexpr int kBackgroundHorizontalInsetDp = 40;

// Vertical padding between the alt-tab bandshield and the window previews.
constexpr int kInsideBorderVerticalPaddingDp = 60;

// Padding between the alt-tab bandshield and the tab slider container.
constexpr int kMirrorContainerVerticalPaddingDp = 24;

// Padding between the window previews within the alt-tab bandshield.
constexpr int kBetweenChildPaddingDp = 12;

// Padding between the tab slider button and the tab slider container.
constexpr int kTabSliderContainerVerticalPaddingDp = 32;

// The font size of "No recent items" string when there's no window in the
// window cycle list.
constexpr int kNoRecentItemsLabelFontSizeDp = 14;

// The UMA histogram that logs smoothness of the fade-in animation.
constexpr char kShowAnimationSmoothness[] =
    "Ash.WindowCycleView.AnimationSmoothness.Show";

// The UMA histogram that logs smoothness of the window container animation.
constexpr char kContainerAnimationSmoothness[] =
    "Ash.WindowCycleView.AnimationSmoothness.Container";

// Duration of the window cycle UI fade in animation.
constexpr base::TimeDelta kFadeInDuration = base::Milliseconds(100);

// Duration of the window cycle elements slide animation.
constexpr base::TimeDelta kContainerSlideDuration = base::Milliseconds(120);

// Duration of the window cycle scale animation when a user toggles alt-tab
// modes.
constexpr base::TimeDelta kToggleModeScaleDuration = base::Milliseconds(150);

constexpr base::TimeDelta kOcclusionTrackerPauseTimeout =
    base::Milliseconds(300);

// Builds the item view for window cycling for the given `window` with the
// correct parent. If the given `window` is a free-form window, the direct
// parent will be `mirror_container`. For `window` that belongs to a snap group,
// however, a `GroupContainerCycleView` will be added. If `same_app_only` is
// true, `GroupContainerCycleView` will only be created if both the windows in
// snap group belongs to the same app.
WindowMiniViewBase* BuildAndConfigureCycleView(
    aura::Window* window,
    views::View* mirror_container,
    std::vector<raw_ptr<WindowMiniViewBase, VectorExperimental>>& cycle_views,
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows,
    const bool same_app_only) {
  if (auto* snap_group_controller = SnapGroupController::Get()) {
    if (auto* snap_group =
            snap_group_controller->GetSnapGroupForGivenWindow(window)) {
      if (!same_app_only ||
          (same_app_only && base::Contains(windows, snap_group->window1()) &&
           base::Contains(windows, snap_group->window2()))) {
        // Create `GroupContainerCycleView` if `window` is physically left / top
        // snapped, which adds two child views subsequently. Skip adding
        // `GroupContainerCycleView` if `window` is secondary snapped since the
        // corresponding container view has been built.
        return window == snap_group->GetPhysicallyLeftOrTopWindow()
                   ? mirror_container->AddChildView(
                         std::make_unique<GroupContainerCycleView>(snap_group))
                   : nullptr;
      }
    }
  }

  // `mirror_container_` owns `view`. The `preview_view_` in `view` will use
  // trilinear filtering in InitLayerOwner().
  return mirror_container->AddChildView(
      std::make_unique<WindowCycleItemView>(window));
}

}  // namespace

WindowCycleView::WindowCycleView(aura::Window* root_window,
                                 const WindowList& windows,
                                 const bool same_app_only)
    : root_window_(root_window), same_app_only_(same_app_only) {
  const bool is_interactive_alt_tab_mode_allowed =
      Shell::Get()->window_cycle_controller()->IsInteractiveAltTabModeAllowed();

  DCHECK(!windows.empty() || is_interactive_alt_tab_mode_allowed);
  // Start the occlusion tracker pauser. It's used to increase smoothness for
  // the fade in but we also create windows here which may occlude other
  // windows.
  Shell::Get()->occlusion_tracker_pauser()->PauseUntilAnimationsEnd(
      kOcclusionTrackerPauseTimeout);

  // The layer for `this` is responsible for showing background blur and fade
  // and clip animations.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetName("WindowCycleView");
  layer()->SetMasksToBounds(true);
  if (features::IsBackgroundBlurEnabled()) {
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  }

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysScrim2, kBackgroundCornerRadius));
  SetBorder(std::make_unique<views::HighlightBorder>(
      kBackgroundCornerRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));

  // `mirror_container_` may be larger than `this`. In this case, it will be
  // shifted along the x-axis when the user tabs through. It is a container
  // for the previews and has no rendered content.
  mirror_container_ = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetPaintToLayer(ui::LAYER_NOT_DRAWN)
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetInsideBorderInsets(gfx::Insets::TLBR(
              is_interactive_alt_tab_mode_allowed
                  ? kMirrorContainerVerticalPaddingDp
                  : kInsideBorderVerticalPaddingDp,
              WindowCycleView::kInsideBorderHorizontalPaddingDp,
              kInsideBorderVerticalPaddingDp,
              WindowCycleView::kInsideBorderHorizontalPaddingDp))
          .SetBetweenChildSpacing(kBetweenChildPaddingDp)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
          .Build());
  mirror_container_->AddObserver(this);
  mirror_container_->layer()->SetName("WindowCycleView/MirrorContainer");

  if (is_interactive_alt_tab_mode_allowed) {
    tab_slider_ = AddChildView(std::make_unique<TabSlider>(/*max_tab_num=*/2));
    all_desks_tab_slider_button_ =
        tab_slider_->AddButton(std::make_unique<LabelSliderButton>(
            base::BindRepeating(
                &WindowCycleController::OnModeChanged,
                base::Unretained(Shell::Get()->window_cycle_controller()),
                /*per_desk=*/false,
                WindowCycleController::ModeSwitchSource::kClick),
            l10n_util::GetStringUTF16(IDS_ASH_ALT_TAB_ALL_DESKS_MODE)));
    current_desk_tab_slider_button_ =
        tab_slider_->AddButton(std::make_unique<LabelSliderButton>(
            base::BindRepeating(
                &WindowCycleController::OnModeChanged,
                base::Unretained(Shell::Get()->window_cycle_controller()),
                /*per_desk=*/true,
                WindowCycleController::ModeSwitchSource::kClick),
            l10n_util::GetStringUTF16(IDS_ASH_ALT_TAB_CURRENT_DESK_MODE)));

    auto* tab_slider_selector_view = tab_slider_->GetSelectorView();
    // Configure the focus ring for the tab slider selector view.
    views::FocusRing::Install(tab_slider_selector_view);
    auto* focus_ring = views::FocusRing::Get(tab_slider_selector_view);
    focus_ring->SetOutsetFocusRingDisabled(true);
    focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);
    const float halo_inset = focus_ring->GetHaloThickness() / 2.f + 2;
    focus_ring->SetHaloInset(-halo_inset);
    // Set a pill shaped (fully rounded rect) highlight path to focus ring.
    focus_ring->SetPathGenerator(
        std::make_unique<views::PillHighlightPathGenerator>());
    focus_ring->SetHasFocusPredicate(base::BindRepeating(
        [](const WindowCycleView* cycle_view, const views::View* view) {
          return cycle_view->IsTabSliderFocused();
        },
        base::Unretained(this)));

    const bool per_desk =
        Shell::Get()->window_cycle_controller()->IsAltTabPerActiveDesk();
    current_desk_tab_slider_button_->SetSelected(per_desk);
    all_desks_tab_slider_button_->SetSelected(!per_desk);

    no_recent_items_label_ = AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_ASH_OVERVIEW_NO_RECENT_ITEMS)));
    no_recent_items_label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    no_recent_items_label_->SetVerticalAlignment(gfx::ALIGN_MIDDLE);

    no_recent_items_label_->SetEnabledColorId(kColorAshIconColorSecondary);
    no_recent_items_label_->SetFontList(
        no_recent_items_label_->font_list()
            .DeriveWithSizeDelta(
                kNoRecentItemsLabelFontSizeDp -
                no_recent_items_label_->font_list().GetFontSize())
            .DeriveWithWeight(gfx::Font::Weight::NORMAL));
    no_recent_items_label_->SetVisible(windows.empty());
    no_recent_items_label_->SetPreferredSize(gfx::Size(
        tab_slider_->GetPreferredSize().width() +
            2 * WindowCycleView::kInsideBorderHorizontalPaddingDp,
        WindowCycleItemView::kFixedPreviewHeightDp +
            kWindowMiniViewHeaderHeight + kMirrorContainerVerticalPaddingDp +
            kInsideBorderVerticalPaddingDp + 8));
  }

  for (aura::Window* window : windows) {
    if (auto* view = BuildAndConfigureCycleView(
            window, mirror_container_, cycle_views_, windows, same_app_only)) {
      cycle_views_.push_back(view);
      no_previews_list_.push_back(view);
    }
  }

  // The insets in the `WindowCycleItemView` are coming from its border, which
  // paints the focus ring around the view when it is focused. Exclude the
  // insets such that the spacing between the contents of the views rather
  // than the views themselves is `kBetweenChildPaddingDp`.
  const gfx::Insets cycle_item_insets =
      cycle_views_.empty() ? gfx::Insets() : cycle_views_.front()->GetInsets();
  mirror_container_->SetBetweenChildSpacing(kBetweenChildPaddingDp -
                                            cycle_item_insets.width());

  shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
      this, SystemShadow::Type::kElevation4);
  shadow_->SetRoundedCornerRadius(kBackgroundCornerRadius);
}

WindowCycleView::~WindowCycleView() = default;

void WindowCycleView::ScaleCycleView(const gfx::Rect& screen_bounds) {
  auto* layer_animator = layer()->GetAnimator();
  if (layer_animator->is_animating()) {
    // There is an existing scaling animation occurring. To accurately get the
    // new bounds for the next layout, we must abort the ongoing animation so
    // `this` will set the previous bounds of the widget and clear the clip
    // rect.
    layer_animator->AbortAllAnimations();
  }

  // `screen_bounds` is in screen coords so store it in local coordinates in
  // `new_bounds`.
  gfx::Rect old_bounds = GetLocalBounds();
  gfx::Rect new_bounds = gfx::Rect(screen_bounds.size());

  if (old_bounds == new_bounds)
    return;

  if (new_bounds.width() >= old_bounds.width()) {
    // In this case, the cycle view is growing. To achieve the scaling
    // animation we set the widget bounds immediately and scale the clipping
    // rect of `this`'s layer from where the `old_bounds` would be in the
    // new local coordinates.
    GetWidget()->SetBounds(screen_bounds);
    old_bounds +=
        gfx::Vector2d((new_bounds.width() - old_bounds.width()) / 2, 0);
  } else {
    // In this case, the cycle view is shrinking. To achieve the scaling
    // animation, we first scale the clipping rect and defer updating the
    // widget's bounds to when the animation is complete. If we instantly
    // laid out, then it wouldn't appear as though the background is
    // shrinking.
    new_bounds +=
        gfx::Vector2d((old_bounds.width() - new_bounds.width()) / 2, 0);
    defer_widget_bounds_update_ = true;
  }

  // Hide the shadow while animating because the clip rect animation clips away
  // visible portions of `this` while the shadow remains the size of `this`.
  shadow_->GetLayer()->SetVisible(false);

  layer()->SetClipRect(old_bounds);
  ui::ScopedLayerAnimationSettings settings(layer_animator);
  settings.SetTransitionDuration(kToggleModeScaleDuration);
  settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN_2);
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.AddObserver(this);
  layer()->SetClipRect(new_bounds);
}

gfx::Rect WindowCycleView::GetTargetBounds() const {
  // The widget is sized clamped to the screen bounds. Its child, the mirror
  // container which is parent to all the previews may be larger than the
  // widget as some previews will be offscreen. When `cycle_view_` does layout
  // the mirror container will be slid back and forth depending on the target
  // window.
  gfx::Rect widget_rect = root_window_->GetBoundsInScreen();
  widget_rect.ClampToCenteredSize(GetPreferredSize());
  return widget_rect;
}

void WindowCycleView::UpdateWindows(const WindowList& windows) {
  const bool no_windows = windows.empty();
  const bool is_interactive_alt_tab_mode_allowed =
      Shell::Get()->window_cycle_controller()->IsInteractiveAltTabModeAllowed();

  if (is_interactive_alt_tab_mode_allowed) {
    DCHECK(no_recent_items_label_);
    no_recent_items_label_->SetVisible(no_windows);
  }

  if (no_windows)
    return;

  for (aura::Window* window : windows) {
    if (auto* view = BuildAndConfigureCycleView(
            window, mirror_container_, cycle_views_, windows, same_app_only_)) {
      cycle_views_.push_back(view);
      no_previews_list_.push_back(view);
    }
  }

  // If there was an ongoing drag session, it's now been completed so reset
  // `horizontal_distance_dragged_`.
  horizontal_distance_dragged_ = 0.f;

  gfx::Rect widget_rect = GetTargetBounds();
  if (is_interactive_alt_tab_mode_allowed)
    ScaleCycleView(widget_rect);
  else
    GetWidget()->SetBounds(widget_rect);

  SetTargetWindow(windows[0]);
  ScrollToWindow(windows[0]);
}

void WindowCycleView::FadeInLayer() {
  DCHECK(GetWidget());

  layer()->SetOpacity(0.f);
  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.SetTransitionDuration(kFadeInDuration);
  settings.AddObserver(this);
  settings.CacheRenderSurface();
  ui::AnimationThroughputReporter reporter(
      settings.GetAnimator(),
      metrics_util::ForSmoothnessV3(base::BindRepeating([](int smoothness) {
        UMA_HISTOGRAM_PERCENTAGE(kShowAnimationSmoothness, smoothness);
      })));

  layer()->SetOpacity(1.f);
}

void WindowCycleView::ScrollToWindow(aura::Window* target) {
  current_window_ = target;

  // If there was an ongoing drag session, it's now been completed so reset
  // |`horizontal_distance_dragged_`.
  horizontal_distance_dragged_ = 0.f;

  if (GetWidget())
    DeprecatedLayoutImmediately();
}

void WindowCycleView::SetTargetWindow(aura::Window* new_target) {
  // Hide the focus border of the previous target window and show the focus
  // border of the new one.
  if (target_window_) {
    if (auto* view = GetCycleViewForWindow(target_window_)) {
      view->ClearFocusSelection();
    }
  }

  target_window_ = new_target;
  if (auto* view = GetCycleViewForWindow(target_window_)) {
    view->SetSelectedWindowForFocus(target_window_);
  }

  // Focus the target window if the user is not currently switching the mode
  // while ChromeVox is on.
  // During the mode switch, we prevent ChromeVox auto-announce the window
  // title from the focus and send our custom string to announce both window
  // title and the selected mode together
  // (see `WindowCycleController::OnModeChanged`).
  auto* a11y_controller = Shell::Get()->accessibility_controller();
  auto* window_cycle_controller = Shell::Get()->window_cycle_controller();
  const bool chromevox_enabled = a11y_controller->spoken_feedback().enabled();
  const bool is_switching_mode = window_cycle_controller->IsSwitchingMode();
  if (!target_window_ || (chromevox_enabled && is_switching_mode)) {
    return;
  }

  auto* cycle_view = GetCycleViewForWindow(target_window_);
  CHECK(cycle_view);
  if (GetWidget()) {
    cycle_view->RequestFocus();
  } else {
    SetInitiallyFocusedView(cycle_view);
    // When alt-tab mode selection is available, announce via ChromeVox the
    // current mode and the directional cue for mode switching.
    if (window_cycle_controller->IsInteractiveAltTabModeAllowed()) {
      a11y_controller->TriggerAccessibilityAlertWithMessage(
          l10n_util::GetStringUTF8(
              window_cycle_controller->IsAltTabPerActiveDesk()
                  ? IDS_ASH_ALT_TAB_FOCUS_CURRENT_DESK_MODE
                  : IDS_ASH_ALT_TAB_FOCUS_ALL_DESKS_MODE));
    }
  }
}

void WindowCycleView::HandleWindowDestruction(aura::Window* destroying_window,
                                              aura::Window* new_target) {
  WindowMiniViewBase* preview = GetCycleViewForWindow(destroying_window);
  CHECK(preview);
  views::View* parent = preview->parent();
  CHECK_EQ(mirror_container_, parent);

  if (preview->TryRemovingChildItem(destroying_window) == 0) {
    // With no remaining child mini views contained in `preview`, we need to
    // remove `preview` and clean up the `preview` in `cycle_views_` and
    // `no_previews_list_`.
    std::erase(cycle_views_, preview);
    std::erase(no_previews_list_, preview);
    parent->RemoveChildViewT(preview);
  }
  // With one of its children now gone, we must re-layout `mirror_container_`.
  // This must happen before `ScrollToWindow()` to make sure our own `Layout()`
  // works correctly when it's calculating highlight bounds.
  parent->DeprecatedLayoutImmediately();
  SetTargetWindow(new_target);
  ScrollToWindow(new_target);
}

void WindowCycleView::DestroyContents() {
  is_destroying_ = true;
  cycle_views_.clear();
  no_previews_list_.clear();
  target_window_ = nullptr;
  current_window_ = nullptr;
  mirror_container_ = nullptr;
  no_recent_items_label_ = nullptr;
  tab_slider_ = nullptr;
  all_desks_tab_slider_button_ = nullptr;
  current_desk_tab_slider_button_ = nullptr;
  defer_widget_bounds_update_ = false;
  RemoveAllChildViews();
  OnFlingEnd();
}

void WindowCycleView::Drag(float delta_x) {
  horizontal_distance_dragged_ += delta_x;
  DeprecatedLayoutImmediately();
}

void WindowCycleView::StartFling(float velocity_x) {
  fling_handler_ = std::make_unique<WmFlingHandler>(
      gfx::Vector2dF(velocity_x, 0),
      GetWidget()->GetNativeWindow()->GetRootWindow(),
      base::BindRepeating(&WindowCycleView::OnFlingStep,
                          base::Unretained(this)),
      base::BindRepeating(&WindowCycleView::OnFlingEnd,
                          base::Unretained(this)));
}

bool WindowCycleView::OnFlingStep(float offset) {
  DCHECK(fling_handler_);
  horizontal_distance_dragged_ += offset;
  DeprecatedLayoutImmediately();
  return true;
}

void WindowCycleView::OnFlingEnd() {
  fling_handler_.reset();
}

void WindowCycleView::SetFocusTabSlider(bool focus) {
  DCHECK(tab_slider_);
  if (focus == is_tab_slider_focused_) {
    return;
  }

  is_tab_slider_focused_ = focus;
  views::FocusRing::Get(tab_slider_->GetSelectorView())->SchedulePaint();
}

bool WindowCycleView::IsTabSliderFocused() const {
  DCHECK(tab_slider_);
  return is_tab_slider_focused_;
}

aura::Window* WindowCycleView::GetWindowAtPoint(
    const gfx::Point& screen_point) {
  for (const ash::WindowMiniViewBase* view : cycle_views_) {
    if (auto* window = view->GetWindowAtPoint(screen_point)) {
      return window;
    }
  }
  return nullptr;
}

void WindowCycleView::OnModePrefsChanged() {
  const bool per_desk =
      Shell::Get()->window_cycle_controller()->IsAltTabPerActiveDesk();

  current_desk_tab_slider_button_->SetSelected(per_desk);
  all_desks_tab_slider_button_->SetSelected(!per_desk);
}

bool WindowCycleView::IsEventInTabSliderContainer(
    const gfx::Point& screen_point) const {
  return tab_slider_ && tab_slider_->GetBoundsInScreen().Contains(screen_point);
}

int WindowCycleView::CalculateMaxWidth() const {
  return root_window_->GetBoundsInScreen().size().width() -
         2 * kBackgroundHorizontalInsetDp;
}

gfx::Size WindowCycleView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size = GetContentContainerBounds().size();
  // `mirror_container_` can have window list that overflow out of the
  // screen, but the window cycle view with a bandshield, cropping the
  // overflow window list, should remain within the specified horizontal
  // insets of the screen width.
  const int max_width = CalculateMaxWidth();
  size.set_width(std::min(size.width(), max_width));
  if (Shell::Get()
          ->window_cycle_controller()
          ->IsInteractiveAltTabModeAllowed()) {
    CHECK(tab_slider_);
    // `mirror_container_` can have window list with width smaller the tab
    // slider's width. The padding should be 64px from the tab slider.
    const int min_width = tab_slider_->GetPreferredSize().width() +
                          2 * WindowCycleView::kInsideBorderHorizontalPaddingDp;
    size.set_width(std::max(size.width(), min_width));
    size.Enlarge(0, tab_slider_->GetPreferredSize().height() +
                        kTabSliderContainerVerticalPaddingDp);
  }
  return size;
}

void WindowCycleView::Layout(PassKey) {
  if (is_destroying_)
    return;

  const bool is_interactive_alt_tab_mode_allowed =
      Shell::Get()->window_cycle_controller()->IsInteractiveAltTabModeAllowed();
  if (bounds().IsEmpty() || (!is_interactive_alt_tab_mode_allowed &&
                             (!target_window_ || !current_window_))) {
    return;
  }

  const bool first_layout = mirror_container_->bounds().IsEmpty();
  // If `mirror_container_` has not yet been laid out, we must lay it and
  // its descendants out so that the calculations based on `target_view`
  // work properly.
  if (first_layout) {
    mirror_container_->SizeToPreferredSize();
    layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF{kBackgroundCornerRadius});
  }

  gfx::RectF target_bounds;
  if (current_window_ || !is_interactive_alt_tab_mode_allowed) {
    views::View* target_view = GetCycleViewForWindow(current_window_);
    target_bounds = gfx::RectF(target_view->GetLocalBounds());
    views::View::ConvertRectToTarget(target_view, mirror_container_,
                                     &target_bounds);
  } else {
    CHECK(no_recent_items_label_);
    target_bounds = gfx::RectF(no_recent_items_label_->bounds());
  }

  // Content container represents the mirror container with >=1 windows or
  // no-recent-items label when there is no window to be shown.
  gfx::Rect content_container_bounds = GetContentContainerBounds();

  // Case one: the container is narrower than the screen. Center the
  // container.
  int x_offset = (width() - content_container_bounds.width()) / 2;
  if (x_offset < 0) {
    // Case two: the container is wider than the screen. Center the target
    // view by moving the list just enough to ensure the target view is in
    // the center. Additionally, offset by however much the user has dragged.
    x_offset = width() / 2 - mirror_container_->GetMirroredXInView(
                                 target_bounds.CenterPoint().x());

    // However, the container must span the screen, i.e. the maximum x is 0
    // and the minimum for its right boundary is the width of the screen.
    int minimum_x = width() - content_container_bounds.width();
    x_offset = std::clamp(x_offset, minimum_x, 0);

    // If the user has dragged, offset the container based on how much they
    // have dragged. Cap `horizontal_distance_dragged_` based on the available
    // distance from the container to the left and right boundaries.
    float clamped_horizontal_distance_dragged = std::clamp(
        horizontal_distance_dragged_, static_cast<float>(minimum_x - x_offset),
        static_cast<float>(-x_offset));
    if (horizontal_distance_dragged_ != clamped_horizontal_distance_dragged)
      OnFlingEnd();

    horizontal_distance_dragged_ = clamped_horizontal_distance_dragged;
    x_offset += horizontal_distance_dragged_;
  }
  content_container_bounds.set_x(x_offset);

  // Layout a tab slider if there is more than one desk.
  if (is_interactive_alt_tab_mode_allowed) {
    CHECK(tab_slider_);
    CHECK(no_recent_items_label_);
    // Layout the tab slider.
    const gfx::Size tab_slider_size = tab_slider_->GetPreferredSize();
    const gfx::Rect tab_slider_mirror_container_bounds(
        (width() - tab_slider_size.width()) / 2,
        kTabSliderContainerVerticalPaddingDp, tab_slider_size.width(),
        tab_slider_size.height());
    tab_slider_->SetBoundsRect(tab_slider_mirror_container_bounds);

    // Move window cycle container down.
    content_container_bounds.set_y(tab_slider_->y() + tab_slider_->height());

    // Unlike the bounds of scrollable mirror container, the bounds of label
    // should not overflow out of the screen.
    const gfx::Rect no_recent_item_bounds_(
        std::max(0, content_container_bounds.x()), content_container_bounds.y(),
        std::min(width(), content_container_bounds.width()),
        content_container_bounds.height());
    no_recent_items_label_->SetBoundsRect(no_recent_item_bounds_);
  }

  // Enable animations only after the first layout pass. If `this` is animating
  // or `defer_widget_bounds_update_`, don't animate as well since the cycle
  // view is already being animated or just finished animating for mode switch.
  std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
  std::optional<ui::AnimationThroughputReporter> reporter;
  if (!first_layout && !this->layer()->GetAnimator()->is_animating() &&
      !defer_widget_bounds_update_ &&
      mirror_container_->bounds() != content_container_bounds) {
    settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
        mirror_container_->layer()->GetAnimator());
    settings->SetTransitionDuration(kContainerSlideDuration);
    reporter.emplace(
        settings->GetAnimator(),
        metrics_util::ForSmoothnessV3(base::BindRepeating([](int smoothness) {
          // Reports animation metrics when the mirror container, which holds
          // all the preview views slides along the x-axis. This can happen
          // while tabbing through windows, if the window cycle ui spans the
          // length of the display.
          UMA_HISTOGRAM_PERCENTAGE(kContainerAnimationSmoothness, smoothness);
        })));
  }
  mirror_container_->SetBoundsRect(content_container_bounds);
}

void WindowCycleView::OnImplicitAnimationsCompleted() {
  layer()->SetClipRect(gfx::Rect());
  if (defer_widget_bounds_update_) {
    // This triggers layout, so reset `defer_widget_bounds_update_` after
    // calling `SetBounds()` to prevent the mirror container from animating.
    GetWidget()->SetBounds(GetTargetBounds());
    defer_widget_bounds_update_ = false;
  }

  shadow_->GetLayer()->SetVisible(true);
}

gfx::Rect WindowCycleView::GetContentContainerBounds() const {
  const bool empty_mirror_container = mirror_container_->children().empty();
  if (empty_mirror_container && no_recent_items_label_)
    return gfx::Rect(no_recent_items_label_->GetPreferredSize(
        views::SizeBounds(no_recent_items_label_->width(), {})));
  return gfx::Rect(mirror_container_->GetPreferredSize());
}

WindowMiniViewBase* WindowCycleView::GetCycleViewForWindow(
    aura::Window* window) const {
  for (ash::WindowMiniViewBase* view : cycle_views_) {
    if (view->Contains(window)) {
      return view;
    }
  }
  return nullptr;
}

void WindowCycleView::OnViewBoundsChanged(views::View* observed_view) {
  CHECK_EQ(mirror_container_.get(), observed_view);
  // If an element in `no_previews_list_` is onscreen (its bounds in `this`
  // coordinates intersects `this`), create the rest of its elements and
  // remove it from the set.
  const gfx::RectF local_bounds(GetLocalBounds());
  for (auto it = no_previews_list_.begin(); it != no_previews_list_.end();) {
    WindowMiniViewBase* view = *it;
    gfx::RectF bounds(view->GetLocalBounds());
    views::View::ConvertRectToTarget(view, this, &bounds);
    if (bounds.Intersects(local_bounds)) {
      view->SetShowPreview(true);
      view->RefreshItemVisuals();
      it = no_previews_list_.erase(it);
    } else {
      ++it;
    }
  }
}

BEGIN_METADATA(WindowCycleView)
END_METADATA

}  // namespace ash

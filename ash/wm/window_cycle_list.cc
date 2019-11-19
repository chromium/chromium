// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_list.h"

#include <map>
#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_mini_view.h"
#include "ash/wm/window_preview_view.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/numerics/ranges.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

bool g_disable_initial_delay = false;

// The color of the window thumbnail backdrop and window cycle highlight - white
// at 14% opacity.
constexpr SkColor kHighlightAndBackdropColor = SkColorSetA(SK_ColorWHITE, 0x24);

// Used for the shield (black background).
constexpr float kBackgroundCornerRadius = 4.f;

// Corner radius applied to the alt-tab selector border.
constexpr gfx::RoundedCornersF kWindowSelectionCornerRadii{9};

// All previews are the same height (this is achieved via a combination of
// scaling and padding).
constexpr int kFixedPreviewHeightDp = 256;

// The min and max width for preview size are in relation to the fixed height.
constexpr int kMinPreviewWidthDp = kFixedPreviewHeightDp / 2;
constexpr int kMaxPreviewWidthDp = kFixedPreviewHeightDp * 2;

// Padding between the alt-tab bandshield and the window previews.
constexpr int kInsideBorderHorizontalPaddingDp = 64;
constexpr int kInsideBorderVerticalPaddingDp = 60;

// Padding between the window previews within the alt-tab bandshield.
constexpr int kBetweenChildPaddingDp = 10;

// The alt-tab cycler widget is not activatable (except when ChromeVox is on),
// so we use WindowTargeter to send input events to the widget.
class CustomWindowTargeter : public aura::WindowTargeter {
 public:
  explicit CustomWindowTargeter(aura::Window* tab_cycler)
      : tab_cycler_(tab_cycler) {}
  ~CustomWindowTargeter() override = default;

  // aura::WindowTargeter
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override {
    if (event->IsLocatedEvent())
      return aura::WindowTargeter::FindTargetForEvent(root, event);
    return tab_cycler_;
  }

 private:
  aura::Window* tab_cycler_;

  DISALLOW_COPY_AND_ASSIGN(CustomWindowTargeter);
};

}  // namespace

// This view represents a single aura::Window by displaying a title and a
// thumbnail of the window's contents.
class WindowCycleItemView : public WindowMiniView {
 public:
  explicit WindowCycleItemView(aura::Window* window)
      : WindowMiniView(window, /*views_should_paint_to_layers=*/false) {
    SetShowPreview(/*show=*/true);
    SetFocusBehavior(FocusBehavior::ALWAYS);
  }
  ~WindowCycleItemView() override = default;

 private:
  // WindowMiniView:
  // Returns the size for the preview view, scaled to fit within the max bounds.
  // Scaling is always 1:1 and we only scale down, never up.
  gfx::Size GetPreviewViewSize() const override {
    gfx::Size preview_pref_size = preview_view()->GetPreferredSize();
    if (preview_pref_size.width() > kMaxPreviewWidthDp ||
        preview_pref_size.height() > kFixedPreviewHeightDp) {
      const float scale =
          std::min(kMaxPreviewWidthDp / float{preview_pref_size.width()},
                   kFixedPreviewHeightDp / float{preview_pref_size.height()});
      preview_pref_size =
          gfx::ScaleToFlooredSize(preview_pref_size, scale, scale);
    }

    return preview_pref_size;
  }

  // views::View:
  void Layout() override {
    WindowMiniView::Layout();

    // Show the backdrop if the preview view does not take up all the bounds
    // allocated for it.
    gfx::Rect preview_max_bounds = GetLocalBounds();
    preview_max_bounds.Subtract(GetHeaderBounds());
    const gfx::Rect preview_area_bounds = preview_view()->bounds();
    SetBackdropVisibility(preview_max_bounds.size() !=
                          preview_area_bounds.size());
  }

  gfx::Size CalculatePreferredSize() const override {
    gfx::Size size = GetSizeForPreviewArea();
    const int header_height = title_label()->GetPreferredSize().height();
    size.Enlarge(0, header_height);
    return size;
  }

  // Returns the size for the entire preview area (preview view and additional
  // padding). All previews will be the same height, so if the preview view
  // isn't tall enough we will add top and bottom padding. Previews can range
  // in width from half to double of |kFixedPreviewHeightDp|. Again, padding
  // will be added to the sides to achieve this if the preview is too narrow.
  gfx::Size GetSizeForPreviewArea() const {
    gfx::Size preview_size = GetPreviewViewSize();

    // All previews are the same height (this may add padding on top and
    // bottom).
    preview_size.set_height(kFixedPreviewHeightDp);

    // Previews should never be narrower than half or wider than double their
    // fixed height.
    preview_size.set_width(base::ClampToRange(
        preview_size.width(), kMinPreviewWidthDp, kMaxPreviewWidthDp));

    return preview_size;
  }

  DISALLOW_COPY_AND_ASSIGN(WindowCycleItemView);
};

// A view that shows a collection of windows the user can tab through.
class WindowCycleView : public views::WidgetDelegateView {
 public:
  explicit WindowCycleView(const WindowCycleList::WindowList& windows)
      : mirror_container_(new views::View()),
        highlight_view_(new views::View()),
        target_window_(nullptr) {
    DCHECK(!windows.empty());
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetMasksToBounds(true);
    layer()->SetOpacity(0.0);
    {
      ui::ScopedLayerAnimationSettings animate_fade(layer()->GetAnimator());
      animate_fade.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(100));
      layer()->SetOpacity(1.0);
    }

    auto layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        gfx::Insets(kInsideBorderVerticalPaddingDp,
                    kInsideBorderHorizontalPaddingDp),
        kBetweenChildPaddingDp);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);
    mirror_container_->SetLayoutManager(std::move(layout));
    mirror_container_->SetPaintToLayer();
    mirror_container_->layer()->SetFillsBoundsOpaquely(false);

    for (auto* window : windows) {
      // |mirror_container_| owns |view|. The |preview_view_| in |view| will
      // use trilinear filtering in InitLayerOwner().
      views::View* view = new WindowCycleItemView(window);
      window_view_map_[window] = view;
      mirror_container_->AddChildView(view);
    }

    highlight_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    highlight_view_->layer()->SetRoundedCornerRadius(
        kWindowSelectionCornerRadii);
    highlight_view_->layer()->SetColor(kHighlightAndBackdropColor);
    highlight_view_->layer()->SetFillsBoundsOpaquely(false);

    AddChildView(highlight_view_);
    AddChildView(mirror_container_);
  }

  ~WindowCycleView() override = default;

  void SetTargetWindow(aura::Window* target) {
    target_window_ = target;
    if (GetWidget()) {
      Layout();
      if (target_window_)
        window_view_map_[target_window_]->RequestFocus();
    }
  }

  void HandleWindowDestruction(aura::Window* destroying_window,
                               aura::Window* new_target) {
    auto view_iter = window_view_map_.find(destroying_window);
    views::View* preview = view_iter->second;
    views::View* parent = preview->parent();
    DCHECK_EQ(mirror_container_, parent);
    window_view_map_.erase(view_iter);
    delete preview;
    // With one of its children now gone, we must re-layout
    // |mirror_container_|. This must happen before SetTargetWindow() to make
    // sure our own Layout() works correctly when it's calculating highlight
    // bounds.
    parent->Layout();
    SetTargetWindow(new_target);
  }

  void DestroyContents() {
    window_view_map_.clear();
    target_window_ = nullptr;
    RemoveAllChildViews(true);
  }

  // views::WidgetDelegateView overrides:
  gfx::Size CalculatePreferredSize() const override {
    return mirror_container_->GetPreferredSize();
  }

  void Layout() override {
    if (!target_window_ || bounds().IsEmpty())
      return;

    bool first_layout = mirror_container_->bounds().IsEmpty();
    // If |mirror_container_| has not yet been laid out, we must lay it and
    // its descendants out so that the calculations based on |target_view|
    // work properly.
    if (first_layout)
      mirror_container_->SizeToPreferredSize();

    views::View* target_view = window_view_map_[target_window_];
    gfx::RectF target_bounds(target_view->GetLocalBounds());
    views::View::ConvertRectToTarget(target_view, mirror_container_,
                                     &target_bounds);
    gfx::Rect container_bounds(mirror_container_->GetPreferredSize());
    // Case one: the container is narrower than the screen. Center the
    // container.
    int x_offset = (width() - container_bounds.width()) / 2;
    if (x_offset < 0) {
      // Case two: the container is wider than the screen. Center the target
      // view by moving the list just enough to ensure the target view is in
      // the center.
      x_offset = width() / 2 - mirror_container_->GetMirroredXInView(
                                   target_bounds.CenterPoint().x());

      // However, the container must span the screen, i.e. the maximum x is 0
      // and the minimum for its right boundary is the width of the screen.
      x_offset = std::min(x_offset, 0);
      x_offset = std::max(x_offset, width() - container_bounds.width());
    }
    container_bounds.set_x(x_offset);
    mirror_container_->SetBoundsRect(container_bounds);

    // Calculate the target preview's bounds relative to |this|.
    views::View::ConvertRectToTarget(mirror_container_, this, &target_bounds);
    const int kHighlightPaddingDip = 5;
    target_bounds.Inset(gfx::InsetsF(-kHighlightPaddingDip));
    target_bounds.set_x(
        GetMirroredXWithWidthInView(target_bounds.x(), target_bounds.width()));
    highlight_view_->SetBoundsRect(gfx::ToEnclosingRect(target_bounds));

    // Enable animations only after the first Layout() pass.
    if (first_layout) {
      // The preview list animates bounds changes (other animatable properties
      // never change).
      mirror_container_->layer()->SetAnimator(
          ui::LayerAnimator::CreateImplicitAnimator());
      // The selection highlight also animates all bounds changes and never
      // changes other animatable properties.
      highlight_view_->layer()->SetAnimator(
          ui::LayerAnimator::CreateImplicitAnimator());
    }
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    // We can't set a bg on the mirror container itself because the highlight
    // view needs to be on top of the bg but behind the target windows.
    const gfx::RectF shield_bounds(mirror_container_->bounds());
    cc::PaintFlags flags;
    flags.setColor(SkColorSetA(SK_ColorBLACK, 0xE6));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    float corner_radius = 0.f;
    if (shield_bounds.width() < width()) {
      flags.setAntiAlias(true);
      corner_radius = kBackgroundCornerRadius;
    }
    canvas->DrawRoundRect(shield_bounds, corner_radius, flags);
  }

  View* GetInitiallyFocusedView() override {
    return window_view_map_[target_window_];
  }

  aura::Window* target_window() { return target_window_; }

 private:
  std::map<aura::Window*, views::View*> window_view_map_;
  views::View* mirror_container_;
  views::View* highlight_view_;
  aura::Window* target_window_;

  DISALLOW_COPY_AND_ASSIGN(WindowCycleView);
};

WindowCycleList::WindowCycleList(const WindowList& windows)
    : windows_(windows) {
  if (!ShouldShowUi())
    Shell::Get()->mru_window_tracker()->SetIgnoreActivations(true);

  for (auto* window : windows_)
    window->AddObserver(this);

  if (ShouldShowUi()) {
    if (g_disable_initial_delay) {
      InitWindowCycleView();
    } else {
      show_ui_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(150),
                           this, &WindowCycleList::InitWindowCycleView);
    }
  }
}

WindowCycleList::~WindowCycleList() {
  if (!ShouldShowUi())
    Shell::Get()->mru_window_tracker()->SetIgnoreActivations(false);

  for (auto* window : windows_)
    window->RemoveObserver(this);

  if (cycle_ui_widget_)
    cycle_ui_widget_->Close();

  // |this| is responsible for notifying |cycle_view_| when windows are
  // destroyed. Since |this| is going away, clobber |cycle_view_|. Otherwise
  // there will be a race where a window closes after now but before the
  // Widget::Close() call above actually destroys |cycle_view_|. See
  // crbug.com/681207
  if (cycle_view_)
    cycle_view_->DestroyContents();

  // While the cycler widget is shown, the windows listed in the cycler is
  // marked as force-visible and don't contribute to occlusion. In order to
  // work occlusion calculation properly, we need to activate a window after
  // the widget has been destroyed. See b/138914552.
  if (!windows_.empty() && user_did_accept_) {
    auto* target_window = windows_[current_index_];
    SelectWindow(target_window);
  }
}

void WindowCycleList::Step(WindowCycleController::Direction direction) {
  if (windows_.empty())
    return;

  // When there is only one window, we should give feedback to the user. If
  // the window is minimized, we should also show it.
  if (windows_.size() == 1) {
    ::wm::AnimateWindow(windows_[0], ::wm::WINDOW_ANIMATION_TYPE_BOUNCE);
    SelectWindow(windows_[0]);
    return;
  }

  DCHECK(static_cast<size_t>(current_index_) < windows_.size());

  if (!cycle_view_ && current_index_ == 0) {
    // Special case the situation where we're cycling forward but the MRU
    // window is not active. This occurs when all windows are minimized. The
    // starting window should be the first one rather than the second.
    if (direction == WindowCycleController::FORWARD &&
        !wm::IsActiveWindow(windows_[0]))
      current_index_ = -1;
  }

  // We're in a valid cycle, so step forward or backward.
  current_index_ += direction == WindowCycleController::FORWARD ? 1 : -1;

  // Wrap to window list size.
  current_index_ = (current_index_ + windows_.size()) % windows_.size();
  DCHECK(windows_[current_index_]);

  if (ShouldShowUi()) {
    if (current_index_ > 1)
      InitWindowCycleView();

    if (cycle_view_)
      cycle_view_->SetTargetWindow(windows_[current_index_]);
  }
}

// static
void WindowCycleList::DisableInitialDelayForTesting() {
  g_disable_initial_delay = true;
}

void WindowCycleList::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);

  WindowList::iterator i = std::find(windows_.begin(), windows_.end(), window);
  // TODO(oshima): Change this back to DCHECK once crbug.com/483491 is fixed.
  CHECK(i != windows_.end());
  int removed_index = static_cast<int>(i - windows_.begin());
  windows_.erase(i);
  if (current_index_ > removed_index ||
      current_index_ == static_cast<int>(windows_.size())) {
    current_index_--;
  }

  if (cycle_view_) {
    auto* new_target_window =
        windows_.empty() ? nullptr : windows_[current_index_];
    cycle_view_->HandleWindowDestruction(window, new_target_window);
    if (windows_.empty()) {
      // This deletes us.
      Shell::Get()->window_cycle_controller()->CancelCycling();
      return;
    }
  }
}

void WindowCycleList::OnDisplayMetricsChanged(const display::Display& display,
                                              uint32_t changed_metrics) {
  if (cycle_ui_widget_ &&
      display.id() ==
          display::Screen::GetScreen()
              ->GetDisplayNearestWindow(cycle_ui_widget_->GetNativeWindow())
              .id() &&
      (changed_metrics & (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_ROTATION))) {
    Shell::Get()->window_cycle_controller()->CancelCycling();
    // |this| is deleted.
    return;
  }
}

bool WindowCycleList::ShouldShowUi() {
  return windows_.size() > 1;
}

void WindowCycleList::InitWindowCycleView() {
  if (cycle_view_)
    return;

  cycle_view_ = new WindowCycleView(windows_);
  cycle_view_->SetTargetWindow(windows_[current_index_]);

  // We need to activate the widget if ChromeVox is enabled as ChromeVox relies
  // on activation.
  const bool spoken_feedback_enabled =
      Shell::Get()->accessibility_controller()->spoken_feedback_enabled();

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.delegate = cycle_view_;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  // Don't let the alt-tab cycler be activatable. This lets the currently
  // activated window continue to be in the foreground. This may affect
  // things such as video automatically pausing/playing.
  if (!spoken_feedback_enabled)
    params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.accept_events = true;
  params.name = "WindowCycleList (Alt+Tab)";
  // TODO(estade): make sure nothing untoward happens when the lock screen
  // or a system modal dialog is shown.
  aura::Window* root_window = Shell::GetRootWindowForNewWindows();
  params.parent = root_window->GetChildById(kShellWindowId_OverlayContainer);
  gfx::Rect widget_rect = display::Screen::GetScreen()
                              ->GetDisplayNearestWindow(root_window)
                              .bounds();
  const int widget_height = cycle_view_->GetPreferredSize().height();
  widget_rect.set_y(widget_rect.y() +
                    (widget_rect.height() - widget_height) / 2);
  widget_rect.set_height(widget_height);
  params.bounds = widget_rect;
  widget->Init(std::move(params));

  screen_observer_.Add(display::Screen::GetScreen());
  widget->Show();
  cycle_ui_widget_ = widget;

  // Since this window is not activated, grab events.
  if (!spoken_feedback_enabled) {
    window_targeter_ = std::make_unique<aura::ScopedWindowTargeter>(
        widget->GetNativeWindow()->GetRootWindow(),
        std::make_unique<CustomWindowTargeter>(widget->GetNativeWindow()));
  }
  // Close the app list, if it's open.
  Shell::Get()->app_list_controller()->DismissAppList();
}

void WindowCycleList::SelectWindow(aura::Window* window) {
  // If the list has only one window, the window can be selected twice (in
  // Step() and the destructor). This causes ARC PIP windows to be restored
  // twice, which leads to a wrong window state.
  if (window_selected_)
    return;

  window->Show();
  auto* window_state = WindowState::Get(window);
  if (window_util::IsArcPipWindow(window))
    window_state->Restore();
  else
    window_state->Activate();

  window_selected_ = true;
}

}  // namespace ash

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_list.h"

#include <map>
#include <string>
#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/frame_throttler/frame_throttling_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/window_mini_view.h"
#include "ash/wm/window_preview_view.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/ranges.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

bool g_disable_initial_delay = false;

// Shield rounded corner radius
constexpr gfx::RoundedCornersF kBackgroundCornerRadius{4.f};

// Shield background blur sigma.
constexpr float kBackgroundBlurSigma =
    static_cast<float>(AshColorProvider::LayerBlurSigma::kBlurDefault);

// Quality of the shield background blur.
constexpr float kBackgroundBlurQuality = 0.33f;

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

// The UMA histogram that logs smoothness of the fade-in animation.
constexpr char kShowAnimationSmoothness[] =
    "Ash.WindowCycleView.AnimationSmoothness.Show";
// The UMA histogram that logs smoothness of the window container animation.
constexpr char kContainerAnimationSmoothness[] =
    "Ash.WindowCycleView.AnimationSmoothness.Container";

// Delay before the UI fade in animation starts. This is so users can switch
// quickly between windows without bringing up the UI.
constexpr base::TimeDelta kShowDelayDuration =
    base::TimeDelta::FromMilliseconds(150);

// Duration of the window cycle UI fade in animation.
constexpr base::TimeDelta kFadeInDuration =
    base::TimeDelta::FromMilliseconds(100);

// Duration of the window cycle elements slide animation.
constexpr base::TimeDelta kContainerSlideDuration =
    base::TimeDelta::FromMilliseconds(120);

// The alt-tab cycler widget is not activatable (except when ChromeVox is on),
// so we use WindowTargeter to send input events to the widget.
class CustomWindowTargeter : public aura::WindowTargeter {
 public:
  explicit CustomWindowTargeter(aura::Window* tab_cycler)
      : tab_cycler_(tab_cycler) {}
  CustomWindowTargeter(const CustomWindowTargeter&) = delete;
  CustomWindowTargeter& operator=(const CustomWindowTargeter&) = delete;
  ~CustomWindowTargeter() override = default;

  // aura::WindowTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override {
    if (event->IsLocatedEvent())
      return aura::WindowTargeter::FindTargetForEvent(root, event);
    return tab_cycler_;
  }

 private:
  aura::Window* tab_cycler_;
};

}  // namespace

// This view represents a single aura::Window by displaying a title and a
// thumbnail of the window's contents.
class WindowCycleItemView : public WindowMiniView {
 public:
  explicit WindowCycleItemView(aura::Window* window) : WindowMiniView(window) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetNotifyEnterExitOnChild(true);
  }
  WindowCycleItemView(const WindowCycleItemView&) = delete;
  WindowCycleItemView& operator=(const WindowCycleItemView&) = delete;
  ~WindowCycleItemView() override = default;

  // Shows the preview and icon. For performance reasons, these are not created
  // on construction. This should be called at most one time during the lifetime
  // of |this|.
  void ShowPreview() {
    DCHECK(!preview_view());

    UpdateIconView();
    SetShowPreview(/*show=*/true);
    UpdatePreviewRoundedCorners(/*show=*/true);
  }

  // views::View:
  void OnMouseEntered(const ui::MouseEvent& event) override {
    Shell::Get()->window_cycle_controller()->SetFocusedWindow(source_window());
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    Shell::Get()->window_cycle_controller()->SetFocusedWindow(source_window());
    Shell::Get()->window_cycle_controller()->CompleteCycling();
    return true;
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    switch (event->type()) {
      case ui::ET_GESTURE_TAP:
      case ui::ET_GESTURE_DOUBLE_TAP:
      case ui::ET_GESTURE_TAP_DOWN:
      case ui::ET_GESTURE_TAP_UNCONFIRMED:
      case ui::ET_GESTURE_LONG_PRESS:
      case ui::ET_GESTURE_LONG_TAP:
      case ui::ET_GESTURE_TWO_FINGER_TAP: {
        WindowCycleController* controller =
            Shell::Get()->window_cycle_controller();
        controller->SetFocusedWindow(source_window());
        break;
      }
      default:
        break;
    }
  }

 private:
  // WindowMiniView:
  gfx::Size GetPreviewViewSize() const override {
    // When the preview is not shown, do an estimate of the expected size.
    // |this| will not be visible anyways, and will get corrected once
    // ShowPreview() is called.
    if (!preview_view()) {
      gfx::SizeF source_size(source_window()->bounds().size());
      // Windows may have no size in tests.
      if (source_size.IsEmpty())
        return gfx::Size();
      const float aspect_ratio = source_size.width() / source_size.height();
      return gfx::Size(kFixedPreviewHeightDp * aspect_ratio,
                       kFixedPreviewHeightDp);
    }

    // Returns the size for the preview view, scaled to fit within the max
    // bounds. Scaling is always 1:1 and we only scale down, never up.
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

    if (!preview_view())
      return;

    // Show the backdrop if the preview view does not take up all the bounds
    // allocated for it.
    gfx::Rect preview_max_bounds = GetContentsBounds();
    preview_max_bounds.Subtract(GetHeaderBounds());
    const gfx::Rect preview_area_bounds = preview_view()->bounds();
    SetBackdropVisibility(preview_max_bounds.size() !=
                          preview_area_bounds.size());
  }

  gfx::Size CalculatePreferredSize() const override {
    // Previews can range in width from half to double of
    // |kFixedPreviewHeightDp|. Padding will be added to the sides to achieve
    // this if the preview is too narrow.
    gfx::Size preview_size = GetPreviewViewSize();

    // All previews are the same height (this may add padding on top and
    // bottom).
    preview_size.set_height(kFixedPreviewHeightDp);

    // Previews should never be narrower than half or wider than double their
    // fixed height.
    preview_size.set_width(base::ClampToRange(
        preview_size.width(), kMinPreviewWidthDp, kMaxPreviewWidthDp));

    const int margin = GetInsets().width();
    preview_size.Enlarge(margin, margin + WindowMiniView::kHeaderHeightDp);
    return preview_size;
  }
};

// A view that shows a collection of windows the user can tab through.
class WindowCycleView : public views::WidgetDelegateView,
                        public ui::ImplicitAnimationObserver {
 public:
  explicit WindowCycleView(const WindowCycleList::WindowList& windows) {
    DCHECK(!windows.empty());

    // Start the occlusion tracker pauser. It's used to increase smoothness for
    // the fade in but we also create windows here which may occlude other
    // windows.
    occlusion_tracker_pauser_ =
        std::make_unique<aura::WindowOcclusionTracker::ScopedPause>();

    // The layer for |this| is responsible for showing color, background blur
    // and fading in.
    SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    ui::Layer* layer = this->layer();
    SkColor background_color = AshColorProvider::Get()->GetBaseLayerColor(
        AshColorProvider::BaseLayerType::kTransparent80);
    layer->SetColor(background_color);
    layer->SetBackgroundBlur(kBackgroundBlurSigma);
    layer->SetBackdropFilterQuality(kBackgroundBlurQuality);
    layer->SetName("WindowCycleView");

    // |mirror_container_| may be larger than |this|. In this case, it will be
    // shifted along the x-axis when the user tabs through. It is a container
    // for the previews and has no rendered content.
    mirror_container_ = AddChildView(std::make_unique<views::View>());
    mirror_container_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
    mirror_container_->layer()->SetName("WindowCycleView/MirrorContainer");
    views::BoxLayout* layout =
        mirror_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            gfx::Insets(kInsideBorderVerticalPaddingDp,
                        kInsideBorderHorizontalPaddingDp),
            kBetweenChildPaddingDp));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);

    for (auto* window : windows) {
      // |mirror_container_| owns |view|. The |preview_view_| in |view| will
      // use trilinear filtering in InitLayerOwner().
      auto* view = mirror_container_->AddChildView(
          std::make_unique<WindowCycleItemView>(window));
      window_view_map_[window] = view;

      no_previews_set_.insert(view);
    }

    // The insets in the WindowCycleItemView are coming from its border, which
    // paints the focus ring around the view when it is highlighted. Exclude the
    // insets such that the spacing between the contents of the views rather
    // than the views themselves is |kBetweenChildPaddingDp|.
    const gfx::Insets cycle_item_insets =
        window_view_map_.begin()->second->GetInsets();
    layout->set_between_child_spacing(kBetweenChildPaddingDp -
                                      cycle_item_insets.width());
  }
  WindowCycleView(const WindowCycleView&) = delete;
  WindowCycleView& operator=(const WindowCycleView&) = delete;
  ~WindowCycleView() override = default;

  void UpdateWindows(const WindowCycleList::WindowList& windows) {
    for (auto* window : windows) {
      auto* view = mirror_container_->AddChildView(
          std::make_unique<WindowCycleItemView>(window));
      window_view_map_[window] = view;

      no_previews_set_.insert(view);
    }

    // Resize the widget.
    aura::Window* root_window = Shell::GetRootWindowForNewWindows();
    gfx::Rect widget_rect = root_window->GetBoundsInScreen();
    widget_rect.ClampToCenteredSize(GetPreferredSize());
    GetWidget()->SetBounds(widget_rect);

    SetTargetWindow(windows[0]);
    ScrollToWindow(windows[0]);
  }

  void FadeInLayer() {
    DCHECK(GetWidget());

    layer()->SetOpacity(0.f);
    ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
    settings.SetTransitionDuration(kFadeInDuration);
    settings.AddObserver(this);
    settings.CacheRenderSurface();
    ui::AnimationThroughputReporter reporter(
        settings.GetAnimator(),
        metrics_util::ForSmoothness(base::BindRepeating([](int smoothness) {
          UMA_HISTOGRAM_PERCENTAGE(kShowAnimationSmoothness, smoothness);
        })));

    layer()->SetOpacity(1.f);
  }

  void ScrollToWindow(aura::Window* target) {
    current_window_ = target;

    if (GetWidget()) {
      Layout();
    }
  }

  void SetTargetWindow(aura::Window* target) {
    // Hide the focus border of the previous target window and show the focus
    // border of the new one.
    if (target_window_) {
      auto target_it = window_view_map_.find(target_window_);
      if (target_it != window_view_map_.end())
        target_it->second->UpdateBorderState(/*show=*/false);
    }
    target_window_ = target;
    auto target_it = window_view_map_.find(target_window_);
    if (target_it != window_view_map_.end())
      target_it->second->UpdateBorderState(/*show=*/true);

    if (GetWidget() && target_window_)
      window_view_map_[target_window_]->RequestFocus();
  }

  void HandleWindowDestruction(aura::Window* destroying_window,
                               aura::Window* new_target) {
    auto view_iter = window_view_map_.find(destroying_window);
    WindowCycleItemView* preview = view_iter->second;
    views::View* parent = preview->parent();
    DCHECK_EQ(mirror_container_, parent);
    window_view_map_.erase(view_iter);
    no_previews_set_.erase(preview);
    delete preview;

    // With one of its children now gone, we must re-layout
    // |mirror_container_|. This must happen before ScrollToWindow() to make
    // sure our own Layout() works correctly when it's calculating highlight
    // bounds.
    parent->Layout();
    SetTargetWindow(new_target);
    ScrollToWindow(new_target);
  }

  void DestroyContents() {
    window_view_map_.clear();
    no_previews_set_.clear();
    target_window_ = nullptr;
    current_window_ = nullptr;
    RemoveAllChildViews(true);
  }

  // views::WidgetDelegateView:
  gfx::Size CalculatePreferredSize() const override {
    return mirror_container_->GetPreferredSize();
  }

  void Layout() override {
    if (!target_window_ || !current_window_ || bounds().IsEmpty())
      return;

    const bool first_layout = mirror_container_->bounds().IsEmpty();
    // If |mirror_container_| has not yet been laid out, we must lay it and
    // its descendants out so that the calculations based on |target_view|
    // work properly.
    if (first_layout) {
      mirror_container_->SizeToPreferredSize();
      // Give rounded corners to our layer if it takes up less space than the
      // width of the screen (our width will match |mirror_container_|'s if the
      // widget's width is less than that of the screen).
      if (mirror_container_->GetPreferredSize().width() <= width())
        layer()->SetRoundedCornerRadius(kBackgroundCornerRadius);
    }

    views::View* target_view = window_view_map_[current_window_];
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

    // Enable animations only after the first Layout() pass.
    std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
    base::Optional<ui::AnimationThroughputReporter> reporter;
    if (!first_layout) {
      settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
          mirror_container_->layer()->GetAnimator());
      settings->SetTransitionDuration(kContainerSlideDuration);
      reporter.emplace(
          settings->GetAnimator(),
          metrics_util::ForSmoothness(base::BindRepeating([](int smoothness) {
            // Reports animation metrics when the mirror container, which holds
            // all the preview views slides along the x-axis. This can happen
            // while tabbing through windows, if the window cycle ui spans the
            // length of the display.
            UMA_HISTOGRAM_PERCENTAGE(kContainerAnimationSmoothness, smoothness);
          })));
    }
    mirror_container_->SetBoundsRect(container_bounds);

    // If an element in |no_previews_set_| is no onscreen (its bounds in |this|
    // coordinates intersects |this|), create the rest of its elements and
    // remove it from the set.
    const gfx::RectF local_bounds(GetLocalBounds());
    for (auto it = no_previews_set_.begin(); it != no_previews_set_.end();) {
      WindowCycleItemView* view = *it;
      gfx::RectF bounds(view->GetLocalBounds());
      views::View::ConvertRectToTarget(view, this, &bounds);
      if (bounds.Intersects(local_bounds)) {
        view->ShowPreview();
        it = no_previews_set_.erase(it);
      } else {
        ++it;
      }
    }
  }

  aura::Window* GetTargetWindow() { return target_window_; }

  View* GetInitiallyFocusedView() override {
    return window_view_map_[target_window_];
  }

  const views::View::Views& GetPreviewViewsForTesting() const {
    return mirror_container_->children();
  }

  const aura::Window* GetTargetWindowForTesting() const {
    return target_window_;
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    occlusion_tracker_pauser_.reset();
  }

 private:
  std::map<aura::Window*, WindowCycleItemView*> window_view_map_;
  views::View* mirror_container_ = nullptr;

  // The |target_window_| is the window that has the focus ring. When the user
  // completes cycling the |target_window_| is activated.
  aura::Window* target_window_ = nullptr;

  // The |current_window_| is the window that the window cycle list uses to
  // determine the layout and positioning of the list's items. If this window's
  // preview can equally divide the list it is centered, otherwise it is
  // off-center.
  aura::Window* current_window_ = nullptr;

  // Set which contains items which have been created but have some of their
  // performance heavy elements not created yet. These elements will be created
  // once onscreen to improve fade in performance, then removed from this set.
  base::flat_set<WindowCycleItemView*> no_previews_set_;

  // Used for preventng occlusion state computations for the duration of the
  // fade in animation.
  std::unique_ptr<aura::WindowOcclusionTracker::ScopedPause>
      occlusion_tracker_pauser_;
};

WindowCycleList::WindowCycleList(const WindowList& windows)
    : windows_(windows) {
  if (!ShouldShowUi())
    Shell::Get()->mru_window_tracker()->SetIgnoreActivations(true);

  for (auto* window : windows_)
    window->AddObserver(this);

  if (ShouldShowUi()) {
    // Disable the tab scrubber so three finger scrolling doesn't scrub tabs as
    // well.
    Shell::Get()->shell_delegate()->SetTabScrubberEnabled(false);

    if (g_disable_initial_delay) {
      InitWindowCycleView();
    } else {
      show_ui_timer_.Start(FROM_HERE, kShowDelayDuration, this,
                           &WindowCycleList::InitWindowCycleView);
    }
  }
}

WindowCycleList::~WindowCycleList() {
  if (!ShouldShowUi())
    Shell::Get()->mru_window_tracker()->SetIgnoreActivations(false);

  Shell::Get()->shell_delegate()->SetTabScrubberEnabled(true);

  for (auto* window : windows_)
    window->RemoveObserver(this);

  if (cycle_ui_widget_)
    cycle_ui_widget_->Close();

  // Store the target window before |cycle_view_| is destroyed.
  aura::Window* target_window = nullptr;

  // |this| is responsible for notifying |cycle_view_| when windows are
  // destroyed. Since |this| is going away, clobber |cycle_view_|. Otherwise
  // there will be a race where a window closes after now but before the
  // Widget::Close() call above actually destroys |cycle_view_|. See
  // crbug.com/681207
  if (cycle_view_) {
    target_window = cycle_view_->GetTargetWindow();
    cycle_view_->DestroyContents();
  }

  // While the cycler widget is shown, the windows listed in the cycler is
  // marked as force-visible and don't contribute to occlusion. In order to
  // work occlusion calculation properly, we need to activate a window after
  // the widget has been destroyed. See b/138914552.
  if (!windows_.empty() && user_did_accept_) {
    if (!target_window)
      target_window = windows_[current_index_];
    SelectWindow(target_window);
  }
  Shell::Get()->frame_throttling_controller()->EndThrottling();
}

void WindowCycleList::ReplaceWindows(const WindowList& windows) {
  if (windows.empty())
    return;

  RemoveAllWindows();
  windows_ = windows;

  for (auto* new_window : windows_)
    new_window->AddObserver(this);

  if (ShouldShowUi() && cycle_view_)
    cycle_view_->UpdateWindows(windows_);
}

void WindowCycleList::Step(WindowCycleController::Direction direction) {
  if (windows_.empty())
    return;

  // If the position of the window cycle list is out-of-sync with the currently
  // selected item, scroll to the selected item and then step.
  if (cycle_view_) {
    aura::Window* selected_window = cycle_view_->GetTargetWindow();
    Scroll(GetIndexOfWindow(selected_window) - current_index_);
  }

  const int offset = direction == WindowCycleController::FORWARD ? 1 : -1;
  SetFocusedWindow(windows_[GetOffsettedWindowIndex(offset)]);
  Scroll(offset);
}

void WindowCycleList::ScrollInDirection(
    WindowCycleController::Direction direction) {
  if (windows_.empty())
    return;

  const int offset = direction == WindowCycleController::FORWARD ? 1 : -1;
  Scroll(offset);
}

void WindowCycleList::SetFocusedWindow(aura::Window* window) {
  if (windows_.empty())
    return;

  if (ShouldShowUi() && cycle_view_)
    cycle_view_->SetTargetWindow(windows_[GetIndexOfWindow(window)]);
}

bool WindowCycleList::IsEventInCycleView(ui::LocatedEvent* event) {
  if (!cycle_view_)
    return false;

  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::Window* event_root = target->GetRootWindow();
  gfx::Point event_screen_point = event->root_location();
  wm::ConvertPointToScreen(event_root, &event_screen_point);
  return cycle_view_->GetBoundsInScreen().Contains(event_screen_point);
}

bool WindowCycleList::ShouldShowUi() {
  return windows_.size() > 1u;
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

void WindowCycleList::RemoveAllWindows() {
  for (auto* window : windows_) {
    window->RemoveObserver(this);

    if (cycle_view_)
      cycle_view_->HandleWindowDestruction(window, nullptr);
  }

  windows_.clear();
  current_index_ = 0;
  window_selected_ = false;
}

void WindowCycleList::InitWindowCycleView() {
  if (cycle_view_)
    return;

  cycle_view_ = new WindowCycleView(windows_);
  cycle_view_->SetTargetWindow(windows_[current_index_]);
  cycle_view_->ScrollToWindow(windows_[current_index_]);

  // We need to activate the widget if ChromeVox is enabled as ChromeVox
  // relies on activation.
  const bool spoken_feedback_enabled =
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled();

  views::Widget* widget = new views::Widget();
  views::Widget::InitParams params;
  params.delegate = cycle_view_;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.layer_type = ui::LAYER_NOT_DRAWN;

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

  // The widget is sized clamped to the screen bounds. Its child, the mirror
  // container which is parent to all the previews may be larger than the widget
  // as some previews will be offscreen. In Layout() of |cyclev_view_| the
  // mirror container will be slid back and forth depending on the target
  // window.
  gfx::Rect widget_rect = root_window->GetBoundsInScreen();
  widget_rect.ClampToCenteredSize(cycle_view_->GetPreferredSize());
  params.bounds = widget_rect;

  screen_observer_.Add(display::Screen::GetScreen());
  widget->Init(std::move(params));
  widget->Show();
  cycle_view_->FadeInLayer();
  cycle_ui_widget_ = widget;

  // Since this window is not activated, grab events.
  if (!spoken_feedback_enabled) {
    window_targeter_ = std::make_unique<aura::ScopedWindowTargeter>(
        widget->GetNativeWindow()->GetRootWindow(),
        std::make_unique<CustomWindowTargeter>(widget->GetNativeWindow()));
  }
  // Close the app list, if it's open in clamshell mode.
  if (!Shell::Get()->tablet_mode_controller()->InTabletMode())
    Shell::Get()->app_list_controller()->DismissAppList();

  Shell::Get()->frame_throttling_controller()->StartThrottling(windows_);
}

void WindowCycleList::SelectWindow(aura::Window* window) {
  // If the list has only one window, the window can be selected twice (in
  // Scroll() and the destructor). This causes ARC PIP windows to be restored
  // twice, which leads to a wrong window state.
  if (window_selected_)
    return;

  if (window->GetProperty(kPipOriginalWindowKey)) {
    window_util::ExpandArcPipWindow();
  } else {
    window->Show();
    WindowState::Get(window)->Activate();
  }

  window_selected_ = true;
}

void WindowCycleList::Scroll(int offset) {
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
    if (offset == 1 && !wm::IsActiveWindow(windows_[0]))
      current_index_ = -1;
  }

  current_index_ = GetOffsettedWindowIndex(offset);

  if (ShouldShowUi()) {
    if (current_index_ > 1)
      InitWindowCycleView();

    if (cycle_view_)
      cycle_view_->ScrollToWindow(windows_[current_index_]);
  }
}

int WindowCycleList::GetIndexOfWindow(aura::Window* window) const {
  auto target_window = std::find(windows_.begin(), windows_.end(), window);
  DCHECK(target_window != windows_.end());
  return std::distance(windows_.begin(), target_window);
}

int WindowCycleList::GetOffsettedWindowIndex(int offset) const {
  DCHECK(!windows_.empty());

  const int offsetted_index =
      (current_index_ + offset + windows_.size()) % windows_.size();
  DCHECK(windows_[offsetted_index]);

  return offsetted_index;
}

const views::View::Views& WindowCycleList::GetWindowCycleItemViewsForTesting()
    const {
  return cycle_view_->GetPreviewViewsForTesting();
}

const aura::Window* WindowCycleList::GetTargetWindowForTesting() const {
  return cycle_view_->GetTargetWindowForTesting();
}

}  // namespace ash

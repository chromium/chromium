// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_highlight_controller.h"

#include "ash/magnifier/docked_magnifier_controller_impl.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/new_desk_button.h"
#include "ash/wm/overview/cleanup_animation_observer.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_delegate.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_properties.h"

namespace ash {

namespace {

// The color and opacity of the overview highlight.
constexpr SkColor kHighlightColor = SkColorSetARGB(36, 255, 255, 255);

// Corner radius and shadow applied to the overview highlight.
constexpr gfx::RoundedCornersF kHighlightCornerRadii{9};
constexpr int kHighlightShadowElevation = 24;

aura::Window* GetWindowForView(views::View* view) {
  DCHECK(view->GetWidget());
  return view->GetWidget()->GetNativeWindow();
}

bool HasSameRootWindow(views::View* view1, views::View* view2) {
  return GetWindowForView(view1)->GetRootWindow() ==
         GetWindowForView(view2)->GetRootWindow();
}

bool ShouldCreateHighlight(
    OverviewHighlightController::OverviewHighlightableView* previous_view,
    OverviewHighlightController::OverviewHighlightableView*
        view_to_be_highlighted,
    bool reverse) {
  if (!previous_view)
    return true;

  DCHECK(view_to_be_highlighted);
  // Recreate it if the new highlight is on a different root.
  if (!HasSameRootWindow(previous_view->GetView(),
                         view_to_be_highlighted->GetView())) {
    return true;
  }

  // Recreate it if the new highlight is on a different row.
  if (previous_view->GetView()->GetBoundsInScreen().y() !=
      view_to_be_highlighted->GetView()->GetBoundsInScreen().y()) {
    return true;
  }

  // Recreate it if we are going from the first item in the same row to the last
  // item, or from the last item to the first item.
  const int previous_x = previous_view->GetView()->GetBoundsInScreen().x();
  const int current_x =
      view_to_be_highlighted->GetView()->GetBoundsInScreen().x();
  return reverse != (current_x < previous_x);
}

}  // namespace

// Widget which contains two children, a solid rounded corner layer which
// represents the highlight, and a shadow. Done this way instead of a single
// layer, because the rounded corner will clip out the shadow, and we want to
// avoid animating two separate layers.
class OverviewHighlightController::HighlightWidget : public views::Widget {
 public:
  HighlightWidget(aura::Window* root_window,
                  const gfx::Rect& bounds_in_screen,
                  const gfx::RoundedCornersF& rounded_corners)
      : root_window_(root_window) {
    DCHECK(root_window->IsRootWindow());

    views::Widget::InitParams params;
    params.type = views::Widget::InitParams::TYPE_POPUP;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    params.layer_type = ui::LAYER_NOT_DRAWN;
    params.accept_events = false;
    params.parent =
        root_window->GetChildById(kShellWindowId_WallpaperContainer);
    params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
    set_focus_on_creation(false);
    Init(std::move(params));

    aura::Window* widget_window = GetNativeWindow();
    // Disable the "bounce in" animation when showing the window.
    ::wm::SetWindowVisibilityAnimationTransition(widget_window,
                                                 ::wm::ANIMATE_NONE);
    // Set the opacity to 0 initial so we can fade it in.
    SetOpacity(0.f);
    Show();

    gfx::Rect bounds = bounds_in_screen;
    wm::ConvertRectFromScreen(root_window_, &bounds);

    widget_window->SetBounds(bounds);
    widget_window->SetName("OverviewModeHighlight");

    // Add the shadow.
    shadow_layer_ = new ui::Shadow();
    shadow_layer_->Init(kHighlightShadowElevation);
    shadow_layer_->SetContentBounds(gfx::Rect(bounds.size()));
    shadow_layer_->layer()->SetVisible(true);
    widget_window->layer()->SetMasksToBounds(false);
    widget_window->layer()->Add(shadow_layer_->layer());

    // Add rounded corner solid color layer.
    color_layer_ = new ui::Layer(ui::LAYER_SOLID_COLOR);
    color_layer_->SetColor(kHighlightColor);
    color_layer_->SetRoundedCornerRadius(rounded_corners);
    color_layer_->SetVisible(true);
    color_layer_->SetBounds(gfx::Rect(bounds.size()));
    widget_window->layer()->Add(color_layer_);
  }

  ~HighlightWidget() override = default;

  // Set the bounds of |this|, and also manually sets the bounds of the
  // children, because there is no masks to bounds.
  void SetWidgetBoundsInScreen(const gfx::Rect& bounds) {
    gfx::Rect bounds_in_root = bounds;
    wm::ConvertRectFromScreen(root_window_, &bounds_in_root);
    SetBounds(bounds_in_root);
    const gfx::Rect child_bounds(bounds_in_root.size());
    shadow_layer_->SetContentBounds(child_bounds);
    color_layer_->SetBounds(child_bounds);
  }

 private:
  aura::Window* root_window_;

  ui::Shadow* shadow_layer_ = nullptr;
  ui::Layer* color_layer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(HighlightWidget);
};

gfx::RoundedCornersF
OverviewHighlightController::OverviewHighlightableView::GetRoundedCornersRadii()
    const {
  return kHighlightCornerRadii;
}

bool OverviewHighlightController::OverviewHighlightableView::
    OnViewHighlighted() {
  return false;
}

void OverviewHighlightController::OverviewHighlightableView::
    OnViewUnhighlighted() {}

bool OverviewHighlightController::OverviewHighlightableView::
    IsViewHighlighted() {
  auto* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  DCHECK(overview_session);
  return overview_session->highlight_controller()->highlighted_view_ == this;
}

gfx::Point OverviewHighlightController::OverviewHighlightableView::
    GetMagnifierFocusPointInScreen() {
  return GetHighlightBoundsInScreen().CenterPoint();
}

// -----------------------------------------------------------------------------
// OverviewHighlightController::TestApi

OverviewHighlightController::TestApi::TestApi(
    OverviewHighlightController* highlight_controller)
    : highlight_controller_(highlight_controller) {}

OverviewHighlightController::TestApi::~TestApi() = default;

gfx::Rect OverviewHighlightController::TestApi::GetHighlightBoundsInScreen()
    const {
  if (!GetHighlightWidget())
    return gfx::Rect();
  return GetHighlightWidget()->GetNativeWindow()->GetBoundsInScreen();
}

OverviewHighlightController::OverviewHighlightableView*
OverviewHighlightController::TestApi::GetHighlightView() const {
  return highlight_controller_->highlighted_view_;
}

OverviewHighlightController::HighlightWidget*
OverviewHighlightController::TestApi::GetHighlightWidget() const {
  return highlight_controller_->highlight_widget_.get();
}

// -----------------------------------------------------------------------------
// OverviewHighlightController

OverviewHighlightController::OverviewHighlightController(
    OverviewSession* overview_session)
    : overview_session_(overview_session) {}

OverviewHighlightController::~OverviewHighlightController() = default;

void OverviewHighlightController::MoveHighlight(bool reverse) {
  const std::vector<OverviewHighlightableView*> traversable_views =
      GetTraversableViews();
  const int count = int{traversable_views.size()};

  // |count| can be zero when there are no overview items and no desk views (eg.
  // "No recent items" or PIP windows are shown but they aren't traversable).
  if (count == 0)
    return;

  int index = 0;
  if (!highlighted_view_) {
    // Pick up where we left off if |deleted_index_| has a value.
    if (deleted_index_) {
      index = *deleted_index_ >= count ? count - 1 : *deleted_index_;
      deleted_index_.reset();
    } else if (reverse) {
      index = count - 1;
    }
  } else {
    auto it = std::find(traversable_views.begin(), traversable_views.end(),
                        highlighted_view_);
    DCHECK(it != traversable_views.end());
    const int current_index = std::distance(traversable_views.begin(), it);
    DCHECK_GE(current_index, 0);
    index = (((reverse ? -1 : 1) + current_index) + count) % count;
  }

  UpdateFocusWidget(traversable_views[index], reverse);
}

void OverviewHighlightController::OnViewDestroyingOrDisabling(
    OverviewHighlightableView* view) {
  DCHECK(view);
  if (view != highlighted_view_)
    return;

  const std::vector<OverviewHighlightableView*> traversable_views =
      GetTraversableViews();
  const auto it = std::find(traversable_views.begin(), traversable_views.end(),
                            highlighted_view_);
  DCHECK(it != traversable_views.end());
  const int current_index = std::distance(traversable_views.begin(), it);
  DCHECK_GE(current_index, 0);
  deleted_index_ = base::make_optional(current_index);
  highlight_widget_.reset();
  highlighted_view_->OnViewUnhighlighted();
  highlighted_view_ = nullptr;
}

void OverviewHighlightController::SetFocusHighlightVisibility(bool visible) {
  if (!highlight_widget_)
    return;

  if (visible)
    highlight_widget_->Show();
  else
    highlight_widget_->Hide();
}

bool OverviewHighlightController::IsFocusHighlightVisible() const {
  return highlight_widget_ && highlight_widget_->IsVisible();
}

bool OverviewHighlightController::MaybeActivateHighlightedView() {
  if (!highlighted_view_)
    return false;

  highlighted_view_->MaybeActivateHighlightedView();
  return true;
}

bool OverviewHighlightController::MaybeCloseHighlightedView() {
  if (!highlighted_view_)
    return false;

  highlighted_view_->MaybeCloseHighlightedView();
  return true;
}

OverviewItem* OverviewHighlightController::GetHighlightedItem() const {
  if (!highlighted_view_)
    return nullptr;

  for (auto& grid : overview_session_->grid_list()) {
    for (auto& item : grid->window_list()) {
      if (highlighted_view_->GetView() == item->overview_item_view())
        return item.get();
    }
  }

  return nullptr;
}

void OverviewHighlightController::ClearTabDragHighlight() {
  tab_drag_widget_.reset();
}

void OverviewHighlightController::UpdateTabDragHighlight(
    aura::Window* root_window,
    const gfx::Rect& bounds_in_screen) {
  DCHECK(root_window);
  DCHECK(!bounds_in_screen.IsEmpty());
  if (tab_drag_widget_) {
    tab_drag_widget_->SetWidgetBoundsInScreen(bounds_in_screen);
    return;
  }
  tab_drag_widget_ = std::make_unique<HighlightWidget>(
      root_window, bounds_in_screen, kHighlightCornerRadii);
  tab_drag_widget_->SetOpacity(1.f);
}

bool OverviewHighlightController::IsTabDragHighlightVisible() const {
  return !!tab_drag_widget_;
}

void OverviewHighlightController::OnWindowsRepositioned(
    aura::Window* root_window) {
  if (!highlight_widget_)
    return;

  aura::Window* highlight_window = highlight_widget_->GetNativeWindow();
  if (root_window != highlight_window->GetRootWindow())
    return;

  DCHECK(highlighted_view_);
  highlight_widget_->SetWidgetBoundsInScreen(
      highlighted_view_->GetHighlightBoundsInScreen());
}

std::vector<OverviewHighlightController::OverviewHighlightableView*>
OverviewHighlightController::GetTraversableViews() const {
  std::vector<OverviewHighlightableView*> traversable_views;
  traversable_views.reserve(overview_session_->num_items() +
                            (desks_util::kMaxNumberOfDesks + 1) *
                                Shell::Get()->GetAllRootWindows().size());
  for (auto& grid : overview_session_->grid_list()) {
    auto* bar_view = grid->desks_bar_view();
    if (bar_view) {
      // The desk items are always traversable from left to right, even in RTL
      // languages.
      for (const auto& mini_view : bar_view->mini_views())
        traversable_views.push_back(mini_view.get());

      if (bar_view->new_desk_button()->GetEnabled())
        traversable_views.push_back(bar_view->new_desk_button());
    }

    for (auto& item : grid->window_list())
      traversable_views.push_back(item->overview_item_view());
  }
  return traversable_views;
}

void OverviewHighlightController::UpdateFocusWidget(
    OverviewHighlightableView* view_to_be_highlighted,
    bool reverse) {
  if (highlighted_view_ == view_to_be_highlighted)
    return;

  OverviewHighlightableView* previous_view = highlighted_view_;
  highlighted_view_ = view_to_be_highlighted;

  // Perform accessibility related tasks.
  highlighted_view_->GetView()->NotifyAccessibilityEvent(
      ax::mojom::Event::kSelection, true);
  // Note that both magnifiers are mutually exclusive. The overview "focus"
  // works differently from regular focusing so we need to update the magnifier
  // manually here.
  DockedMagnifierControllerImpl* docked_magnifier =
      Shell::Get()->docked_magnifier_controller();
  MagnificationController* fullscreen_magnifier =
      Shell::Get()->magnification_controller();
  const gfx::Point point_of_interest =
      highlighted_view_->GetMagnifierFocusPointInScreen();
  if (docked_magnifier->GetEnabled())
    docked_magnifier->CenterOnPoint(point_of_interest);
  else if (fullscreen_magnifier->IsEnabled())
    fullscreen_magnifier->CenterOnPoint(point_of_interest);

  if (previous_view)
    previous_view->OnViewUnhighlighted();

  const bool create_highlight =
      ShouldCreateHighlight(previous_view, highlighted_view_, reverse);
  // If the highlight exists and we need to recreate it, slide the old one out.
  if (highlight_widget_ && create_highlight) {
    aura::Window* old_highlight_window = highlight_widget_->GetNativeWindow();
    int highlight_width = old_highlight_window->bounds().width();
    gfx::Vector2dF translation(highlight_width * (reverse ? -1.f : 1.f), 0.f);
    gfx::Transform transform = old_highlight_window->transform();
    transform.Translate(translation);

    ScopedOverviewAnimationSettings settings(
        OVERVIEW_ANIMATION_SELECTION_WINDOW, old_highlight_window);
    // CleanupAnimationObserver will delete itself and the widget when the
    // motion animation is complete. Ownership over the observer is passed to
    // the overview_session_->delegate() which has longer lifetime so that
    // animations can continue even after the overview session is shut down.
    auto observer = std::make_unique<CleanupAnimationObserver>(
        std::move(highlight_widget_));
    settings.AddObserver(observer.get());
    overview_session_->delegate()->AddExitAnimationObserver(
        std::move(observer));
    old_highlight_window->layer()->SetOpacity(0.f);
    old_highlight_window->SetTransform(transform);
  }

  if (highlighted_view_->OnViewHighlighted())
    return;

  gfx::Rect target_screen_bounds =
      highlighted_view_->GetHighlightBoundsInScreen();
  if (!highlight_widget_) {
    // Offset the bounds slightly to create a slide in animation.
    gfx::Rect initial_bounds = target_screen_bounds;
    initial_bounds.Offset(target_screen_bounds.width() * (reverse ? 1 : -1), 0);
    highlight_widget_ = std::make_unique<HighlightWidget>(
        GetWindowForView(highlighted_view_->GetView())->GetRootWindow(),
        initial_bounds, highlighted_view_->GetRoundedCornersRadii());
  }

  // Move the highlight to the target.
  aura::Window* highlight_window = highlight_widget_->GetNativeWindow();
  gfx::RectF previous_bounds =
      gfx::RectF(highlight_window->GetBoundsInScreen());
  highlight_widget_->SetWidgetBoundsInScreen(target_screen_bounds);
  const gfx::RectF current_bounds = gfx::RectF(target_screen_bounds);
  highlight_window->SetTransform(
      gfx::TransformBetweenRects(current_bounds, previous_bounds));
  ScopedOverviewAnimationSettings settings(OVERVIEW_ANIMATION_SELECTION_WINDOW,
                                           highlight_window);
  highlight_window->SetTransform(gfx::Transform());
  highlight_widget_->SetOpacity(1.f);
}

}  // namespace ash

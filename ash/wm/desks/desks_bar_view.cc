// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_bar_view.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_mini_view_animations.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/new_desk_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "base/stl_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/events/event_observer.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/event_monitor.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

constexpr int kBarHeightInCompactLayout = 64;
constexpr int kUseCompactLayoutWidthThreshold = 600;

// In the non-compact layout, this is the height allocated for elements other
// than the desk preview (e.g. the DeskNameView, and the vertical paddings).
constexpr int kNonPreviewAllocatedHeight = 55;

// The local Y coordinate of the mini views in both non-compact and compact
// layouts respectively.
constexpr int kMiniViewsY = 16;
constexpr int kMiniViewsYCompact = 8;

// New desk button layout constants.
constexpr int kButtonRightMargin = 36;
constexpr int kIconAndTextHorizontalPadding = 16;
constexpr int kIconAndTextVerticalPadding = 8;

// Spacing between mini views.
constexpr int kMiniViewsSpacing = 12;

gfx::Rect GetGestureEventScreenRect(const ui::Event& event) {
  DCHECK(event.IsGestureEvent());
  return event.AsGestureEvent()->details().bounding_box();
}

OverviewHighlightController* GetHighlightController() {
  auto* overview_controller = Shell::Get()->overview_controller();
  DCHECK(overview_controller->InOverviewSession());
  return overview_controller->overview_session()->highlight_controller();
}

}  // namespace

// -----------------------------------------------------------------------------
// DeskBarHoverObserver:

class DeskBarHoverObserver : public ui::EventObserver {
 public:
  DeskBarHoverObserver(DesksBarView* owner, aura::Window* widget_window)
      : owner_(owner),
        event_monitor_(views::EventMonitor::CreateWindowMonitor(
            this,
            widget_window,
            {ui::ET_MOUSE_PRESSED, ui::ET_MOUSE_DRAGGED, ui::ET_MOUSE_RELEASED,
             ui::ET_MOUSE_MOVED, ui::ET_MOUSE_ENTERED, ui::ET_MOUSE_EXITED,
             ui::ET_GESTURE_LONG_PRESS, ui::ET_GESTURE_LONG_TAP,
             ui::ET_GESTURE_TAP, ui::ET_GESTURE_TAP_DOWN})) {}

  ~DeskBarHoverObserver() override = default;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    switch (event.type()) {
      case ui::ET_MOUSE_PRESSED:
      case ui::ET_MOUSE_DRAGGED:
      case ui::ET_MOUSE_RELEASED:
      case ui::ET_MOUSE_MOVED:
      case ui::ET_MOUSE_ENTERED:
      case ui::ET_MOUSE_EXITED:
        owner_->OnHoverStateMayHaveChanged();
        break;

      case ui::ET_GESTURE_LONG_PRESS:
      case ui::ET_GESTURE_LONG_TAP:
        owner_->OnGestureTap(GetGestureEventScreenRect(event),
                             /*is_long_gesture=*/true);
        break;

      case ui::ET_GESTURE_TAP:
      case ui::ET_GESTURE_TAP_DOWN:
        owner_->OnGestureTap(GetGestureEventScreenRect(event),
                             /*is_long_gesture=*/false);
        break;

      default:
        NOTREACHED();
        break;
    }
  }

 private:
  DesksBarView* owner_;

  std::unique_ptr<views::EventMonitor> event_monitor_;

  DISALLOW_COPY_AND_ASSIGN(DeskBarHoverObserver);
};

// -----------------------------------------------------------------------------
// DesksBarView:

DesksBarView::DesksBarView(OverviewGrid* overview_grid)
    : background_view_(new views::View),
      new_desk_button_(new NewDeskButton(this)),
      overview_grid_(overview_grid) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  background_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  background_view_->layer()->SetFillsBoundsOpaquely(false);
  background_view_->layer()->SetColor(
      AshColorProvider::Get()->GetShieldLayerColor(
          AshColorProvider::ShieldLayerType::kShield80));

  AddChildView(background_view_);
  AddChildView(new_desk_button_);

  DesksController::Get()->AddObserver(this);
}

DesksBarView::~DesksBarView() {
  DesksController::Get()->RemoveObserver(this);
}

// static
int DesksBarView::GetBarHeightForWidth(aura::Window* root,
                                       const DesksBarView* desks_bar_view,
                                       int width) {
  if (width <= kUseCompactLayoutWidthThreshold ||
      (desks_bar_view && width <= desks_bar_view->min_width_to_fit_contents_)) {
    return kBarHeightInCompactLayout;
  }

  return DeskPreviewView::GetHeight(root, /*compact=*/false) +
         kNonPreviewAllocatedHeight;
}

// static
std::unique_ptr<views::Widget> DesksBarView::CreateDesksWidget(
    aura::Window* root,
    const gfx::Rect& bounds) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_YES;
  params.accept_events = true;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  // This widget will be parented to the currently-active desk container on
  // |root|.
  params.context = root;
  params.bounds = bounds;
  params.name = "VirtualDesksWidget";

  // Even though this widget exists on the active desk container, it should not
  // show up in the MRU list, and it should not be mirrored in the desks
  // mini_views.
  params.init_properties_container.SetProperty(kExcludeInMruKey, true);
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  widget->Init(std::move(params));

  auto* window = widget->GetNativeWindow();
  window->set_id(kShellWindowId_DesksBarWindow);
  ::wm::SetWindowVisibilityAnimationTransition(window, ::wm::ANIMATE_NONE);

  return widget;
}

void DesksBarView::Init() {
  UpdateNewMiniViews(/*animate=*/false);
  hover_observer_ = std::make_unique<DeskBarHoverObserver>(
      this, GetWidget()->GetNativeWindow());
}

bool DesksBarView::IsDeskNameBeingModified() const {
  if (!GetWidget()->IsActive())
    return false;

  for (auto* mini_view : mini_views_) {
    if (mini_view->IsDeskNameBeingModified())
      return true;
  }
  return false;
}

float DesksBarView::GetOnHoverWindowSizeScaleFactor() const {
  return float{height()} / overview_grid_->root_window()->bounds().height();
}

void DesksBarView::OnHoverStateMayHaveChanged() {
  for (auto* mini_view : mini_views_)
    mini_view->UpdateCloseButtonVisibility();
}

void DesksBarView::OnGestureTap(const gfx::Rect& screen_rect,
                                bool is_long_gesture) {
  for (auto* mini_view : mini_views_)
    mini_view->OnWidgetGestureTap(screen_rect, is_long_gesture);
}

void DesksBarView::SetDragDetails(const gfx::Point& screen_location,
                                  bool dragged_item_over_bar) {
  last_dragged_item_screen_location_ = screen_location;
  const bool old_dragged_item_over_bar = dragged_item_over_bar_;
  dragged_item_over_bar_ = dragged_item_over_bar;

  if (!old_dragged_item_over_bar && !dragged_item_over_bar)
    return;

  for (auto* mini_view : mini_views_)
    mini_view->UpdateBorderColor();
}

const char* DesksBarView::GetClassName() const {
  return "DesksBarView";
}

void DesksBarView::Layout() {
  background_view_->SetBoundsRect(bounds());

  const bool compact = UsesCompactLayout();
  new_desk_button_->SetLabelVisible(!compact);
  gfx::Size new_desk_button_size = new_desk_button_->GetPreferredSize();
  if (compact) {
    new_desk_button_size.Enlarge(2 * kIconAndTextVerticalPadding,
                                 2 * kIconAndTextVerticalPadding);
  } else {
    new_desk_button_size.Enlarge(2 * kIconAndTextHorizontalPadding,
                                 2 * kIconAndTextVerticalPadding);
  }

  const gfx::Rect button_bounds{
      bounds().right() - new_desk_button_size.width() - kButtonRightMargin,
      (bounds().height() - new_desk_button_size.height()) / 2,
      new_desk_button_size.width(), new_desk_button_size.height()};
  new_desk_button_->SetBoundsRect(button_bounds);

  if (mini_views_.empty())
    return;

  const gfx::Size mini_view_size = mini_views_[0]->GetPreferredSize();
  const int total_width =
      mini_views_.size() * (mini_view_size.width() + kMiniViewsSpacing) -
      kMiniViewsSpacing;

  int x = (width() - total_width) / 2;
  const int y = compact ? kMiniViewsYCompact : kMiniViewsY;
  for (auto* mini_view : mini_views_) {
    mini_view->SetBoundsRect(gfx::Rect(gfx::Point(x, y), mini_view_size));
    x += (mini_view_size.width() + kMiniViewsSpacing);
  }
}

bool DesksBarView::OnMousePressed(const ui::MouseEvent& event) {
  DeskNameView::CommitChanges(GetWidget());
  return false;
}

void DesksBarView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_LONG_PRESS:
    case ui::ET_GESTURE_LONG_TAP:
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_TAP_DOWN:
      DeskNameView::CommitChanges(GetWidget());
      break;

    default:
      break;
  }
}

bool DesksBarView::UsesCompactLayout() const {
  return width() <= kUseCompactLayoutWidthThreshold ||
         width() <= min_width_to_fit_contents_;
}

void DesksBarView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  if (sender == new_desk_button_)
    new_desk_button_->OnButtonPressed();
}

void DesksBarView::OnDeskAdded(const Desk* desk) {
  DeskNameView::CommitChanges(GetWidget());
  UpdateNewMiniViews(/*animate=*/true);
}

void DesksBarView::OnDeskRemoved(const Desk* desk) {
  DeskNameView::CommitChanges(GetWidget());
  auto iter = std::find_if(
      mini_views_.begin(), mini_views_.end(),
      [desk](DeskMiniView* mini_view) { return desk == mini_view->desk(); });

  DCHECK(iter != mini_views_.end());

  // Let the highlight controller know the view is destroying before it is
  // removed from the collection because it needs to know the index of the mini
  // view, or the desk name view (if either is currently highlighted) relative
  // to other traversable views.
  auto* highlight_controller = GetHighlightController();
  // The order here matters, we call it first on the desk_name_view since it
  // comes later in the highlight order (See documentation of
  // OnViewDestroyingOrDisabling()).
  highlight_controller->OnViewDestroyingOrDisabling((*iter)->desk_name_view());
  highlight_controller->OnViewDestroyingOrDisabling(*iter);

  const int begin_x = GetFirstMiniViewXOffset();
  // Remove the mini view from the list now. And remove it from its parent
  // after the animation is done.
  DeskMiniView* removed_mini_view = *iter;
  auto partition_iter = mini_views_.erase(iter);

  UpdateMinimumWidthToFitContents();
  overview_grid_->OnDesksChanged();
  new_desk_button_->UpdateButtonState();

  for (auto* mini_view : mini_views_)
    mini_view->UpdateCloseButtonVisibility();

  PerformRemoveDeskMiniViewAnimation(
      removed_mini_view,
      std::vector<DeskMiniView*>(mini_views_.begin(), partition_iter),
      std::vector<DeskMiniView*>(partition_iter, mini_views_.end()),
      begin_x - GetFirstMiniViewXOffset());
}

void DesksBarView::OnDeskActivationChanged(const Desk* activated,
                                           const Desk* deactivated) {
  for (auto* mini_view : mini_views_) {
    const Desk* desk = mini_view->desk();
    if (desk == activated || desk == deactivated)
      mini_view->UpdateBorderColor();
  }
}

void DesksBarView::OnDeskSwitchAnimationLaunching() {}

void DesksBarView::OnDeskSwitchAnimationFinished() {}

void DesksBarView::UpdateNewMiniViews(bool animate) {
  const auto& desks = DesksController::Get()->desks();
  if (desks.size() < 2) {
    // We do not show mini_views when we have a single desk.
    DCHECK(mini_views_.empty());

    // The bar background is initially translated off the screen.
    gfx::Transform translate;
    translate.Translate(0, -height());
    background_view_->layer()->SetTransform(translate);
    background_view_->layer()->SetOpacity(0);

    return;
  }

  // This should not be called when a desk is removed.
  DCHECK_LE(mini_views_.size(), desks.size());

  const bool first_time_mini_views = mini_views_.empty();
  const int begin_x = GetFirstMiniViewXOffset();
  std::vector<DeskMiniView*> new_mini_views;

  aura::Window* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  DCHECK(root_window);
  for (const auto& desk : desks) {
    if (!FindMiniViewForDesk(desk.get())) {
      DeskMiniView* mini_view = AddChildView(
          std::make_unique<DeskMiniView>(this, root_window, desk.get()));
      mini_views_.push_back(mini_view);
      new_mini_views.push_back(mini_view);
    }
  }

  UpdateMinimumWidthToFitContents();
  overview_grid_->OnDesksChanged();

  if (!animate)
    return;

  PerformNewDeskMiniViewAnimation(this, new_mini_views,
                                  begin_x - GetFirstMiniViewXOffset(),
                                  first_time_mini_views);
}

DeskMiniView* DesksBarView::FindMiniViewForDesk(const Desk* desk) const {
  for (auto* mini_view : mini_views_) {
    if (mini_view->desk() == desk)
      return mini_view;
  }

  return nullptr;
}

int DesksBarView::GetFirstMiniViewXOffset() const {
  return mini_views_.empty() ? bounds().CenterPoint().x()
                             : mini_views_[0]->bounds().x();
}

void DesksBarView::UpdateMinimumWidthToFitContents() {
  int button_width = new_desk_button_->GetMinSize(/*compact=*/false).width();
  button_width += 2 * kIconAndTextHorizontalPadding;
  button_width += kButtonRightMargin;

  if (mini_views_.empty()) {
    min_width_to_fit_contents_ = button_width;
    return;
  }

  const int mini_view_width = mini_views_[0]->GetMinWidthForDefaultLayout();
  const int total_mini_views_width =
      mini_views_.size() * (mini_view_width + kMiniViewsSpacing) -
      kMiniViewsSpacing;

  min_width_to_fit_contents_ = total_mini_views_width + button_width * 2;
}

}  // namespace ash

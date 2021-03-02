// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_bar_view.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/desk_drag_proxy.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_mini_view_animations.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/expanded_state_new_desk_button.h"
#include "ash/wm/desks/new_desk_button.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "base/stl_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/events/event_observer.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

constexpr int kBarHeightInCompactLayout = 64;
constexpr int kUseCompactLayoutWidthThreshold = 600;

// In the non-compact layout, this is the height allocated for elements other
// than the desk preview (e.g. the DeskNameView, and the vertical paddings).
// Note, the vertical paddings should exclude the preview border's insets.
constexpr int kNonPreviewAllocatedHeight = 48;

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

// Spacing between zero state default desk button and new desk button.
constexpr int kZeroStateButtonSpacing = 8;

// The local Y coordinate of the zero state desk buttons.
constexpr int kZeroStateY = 6;

gfx::Rect GetGestureEventScreenRect(const ui::Event& event) {
  DCHECK(event.IsGestureEvent());
  return event.AsGestureEvent()->details().bounding_box();
}

OverviewHighlightController* GetHighlightController() {
  auto* overview_controller = Shell::Get()->overview_controller();
  DCHECK(overview_controller->InOverviewSession());
  return overview_controller->overview_session()->highlight_controller();
}

int DetermineMoveIndex(const std::vector<DeskMiniView*>& views,
                       int old_index,
                       int location_screen_x) {
  DCHECK_GE(old_index, 0);

  const int views_size = static_cast<int>(views.size());
  DCHECK_LT(old_index, views_size);

  for (int new_index = 0; new_index < views_size; new_index++) {
    // Note that we cannot directly use |GetBoundsInScreen|. Because we may
    // perform animation (transform) on mini views. The bounds gotten from
    // |GetBoundsInScreen| may be the intermediate bounds during animation.
    // Therefore, we transfer a mini view's origin from its parent level to
    // avoid the influence of its own transform.
    auto* view = views[new_index];
    gfx::Point center_in_screen = view->bounds().CenterPoint();
    views::View::ConvertPointToScreen(view->parent(), &center_in_screen);

    if (location_screen_x < center_in_screen.x())
      return new_index;
  }

  return views_size - 1;
}

int GetSpaceBetweenMiniViews(DeskMiniView* mini_view) {
  return kMiniViewsSpacing - mini_view->GetPreviewBorderInsets().width();
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
// DesksBarLayout:

// TODO(minch): Remove this layout manager once the kBento feature is fully
// launched and becomes the default.
// Layout manager for the classic desks bar.
class DesksBarLayout : public views::LayoutManager {
 public:
  DesksBarLayout(views::View* background_view, NewDeskButton* new_desk_button)
      : background_view_(background_view), new_desk_button_(new_desk_button) {}
  DesksBarLayout(const DesksBarLayout&) = delete;
  DesksBarLayout& operator=(const DesksBarLayout&) = delete;
  ~DesksBarLayout() override = default;

  // views::LayoutManager:
  void Layout(views::View* host) override {
    auto* desks_bar_view = static_cast<DesksBarView*>(host);
    const bool compact = desks_bar_view->UsesCompactLayout();
    const gfx::Rect bounds = desks_bar_view->bounds();
    background_view_->SetBoundsRect(bounds);

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
        bounds.right() - new_desk_button_size.width() - kButtonRightMargin,
        (bounds.height() - new_desk_button_size.height()) / 2,
        new_desk_button_size.width(), new_desk_button_size.height()};
    new_desk_button_->SetBoundsRect(button_bounds);

    const std::vector<DeskMiniView*>& mini_views = desks_bar_view->mini_views();
    if (mini_views.empty())
      return;

    const gfx::Size mini_view_size = mini_views[0]->GetPreferredSize();
    const int mini_view_spacing = GetSpaceBetweenMiniViews(mini_views[0]);
    const int total_width =
        mini_views.size() * (mini_view_size.width() + mini_view_spacing) -
        mini_view_spacing;

    int x = (bounds.width() - total_width) / 2;
    int y = compact ? kMiniViewsYCompact : kMiniViewsY;
    y -= mini_views[0]->GetPreviewBorderInsets().top();
    for (auto* mini_view : mini_views) {
      mini_view->SetBoundsRect(gfx::Rect(gfx::Point(x, y), mini_view_size));
      x += (mini_view_size.width() + mini_view_spacing);
    }
  }

  // views::LayoutManager:
  gfx::Size GetPreferredSize(const views::View* host) const override {
    DCHECK(host);
    return host->bounds().size();
  }

 private:
  views::View* background_view_;    // Not owned.
  NewDeskButton* new_desk_button_;  // Not owned.
};

// -----------------------------------------------------------------------------
// BentoDesksBarLayout:

// TODO(minch): Remove this layout manager and move the layout code back to
// DesksBarView::Layout() once the kBento feature is launched and becomes
// stable.
// Layout manager for desks bar of Bento. The difference from DesksBarLayout is
// that there is no compact layout in Bento. And contents can be layout outside
// of the bar if the total contents' width exceeds the width of the desks bar.
class BentoDesksBarLayout : public views::LayoutManager {
 public:
  BentoDesksBarLayout(DesksBarView* bar_view) : bar_view_(bar_view) {}
  BentoDesksBarLayout(const BentoDesksBarLayout&) = delete;
  BentoDesksBarLayout& operator=(const BentoDesksBarLayout&) = delete;
  ~BentoDesksBarLayout() override = default;

  // views::LayoutManager:
  void Layout(views::View* host) override {
    const gfx::Rect desks_bar_bounds = bar_view_->bounds();
    if (bar_view_->IsZeroState()) {
      host->SetBoundsRect(desks_bar_bounds);
      auto* zero_state_default_desk_button =
          bar_view_->zero_state_default_desk_button();
      const gfx::Size zero_state_default_desk_button_size =
          zero_state_default_desk_button->GetPreferredSize();

      auto* zero_state_new_desk_button =
          bar_view_->zero_state_new_desk_button();
      const gfx::Size zero_state_new_desk_button_size =
          zero_state_new_desk_button->GetPreferredSize();

      const int content_width = zero_state_default_desk_button_size.width() +
                                kZeroStateButtonSpacing +
                                zero_state_new_desk_button_size.width();
      zero_state_default_desk_button->SetBoundsRect(
          gfx::Rect(gfx::Point((desks_bar_bounds.width() - content_width) / 2,
                               kZeroStateY),
                    zero_state_default_desk_button_size));
      // Update this button's text since it may changes while removing a desk
      // and going back to the zero state.
      zero_state_default_desk_button->UpdateLabelText();
      // Make sure these two buttons are always visible while in zero state bar
      // since they are invisible in expanded state bar.
      zero_state_default_desk_button->SetVisible(true);
      zero_state_new_desk_button->SetVisible(true);
      zero_state_new_desk_button->SetBoundsRect(gfx::Rect(
          gfx::Point(zero_state_default_desk_button->bounds().right() +
                         kZeroStateButtonSpacing,
                     kZeroStateY),
          zero_state_new_desk_button_size));
      return;
    }

    const std::vector<DeskMiniView*>& mini_views = bar_view_->mini_views();
    if (mini_views.empty())
      return;

    gfx::Size mini_view_size = mini_views[0]->GetPreferredSize();
    const int mini_view_spacing = GetSpaceBetweenMiniViews(mini_views[0]);
    // The new desk button in the expaneded bar view has the same size as mini
    // view.
    int content_width =
        (mini_views.size() + 1) * (mini_view_size.width() + mini_view_spacing) -
        mini_view_spacing;
    width_ = std::max(desks_bar_bounds.width(), content_width);

    // Update the size of the |host|, which is |scroll_view_contents_| here.
    // This is done to make sure its size can be updated on mini views' adding
    // or removing, then |scroll_view_| will know whether the contents need to
    // be scolled or not.
    host->SetSize(gfx::Size(width_, desks_bar_bounds.height()));

    int x = (width_ - content_width) / 2;
    const int y = kMiniViewsY - mini_views[0]->GetPreviewBorderInsets().top();
    for (auto* mini_view : mini_views) {
      mini_view->SetBoundsRect(gfx::Rect(gfx::Point(x, y), mini_view_size));
      x += (mini_view_size.width() + mini_view_spacing);
    }
    bar_view_->expanded_state_new_desk_button()->SetBoundsRect(
        gfx::Rect(gfx::Point(x, y), mini_view_size));
  }

  // views::LayoutManager:
  gfx::Size GetPreferredSize(const views::View* host) const override {
    return gfx::Size(width_, bar_view_->bounds().height());
  }

 private:
  DesksBarView* bar_view_;  // Not owned.

  // Width of the scroll view. It is the contents' preferred width if it exceeds
  // the desks bar view's width or just the desks bar view's width if not.
  int width_ = 0;
};

// -----------------------------------------------------------------------------
// DesksBarView:

DesksBarView::DesksBarView(OverviewGrid* overview_grid)
    : background_view_(new views::View),
      overview_grid_(overview_grid) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  background_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  background_view_->layer()->SetFillsBoundsOpaquely(false);

  AddChildView(background_view_);

  if (features::IsBentoEnabled()) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
    scroll_view_->SetBackgroundColor(base::nullopt);
    scroll_view_->SetDrawOverflowIndicator(false);
    scroll_view_->SetHorizontalScrollBarMode(
        views::ScrollView::ScrollBarMode::kHiddenButEnabled);
    scroll_view_->SetTreatAllScrollEventsAsHorizontal(true);

    scroll_view_contents_ =
        scroll_view_->SetContents(std::make_unique<views::View>());
    expanded_state_new_desk_button_ = scroll_view_contents_->AddChildView(
        std::make_unique<ExpandedStateNewDeskButton>(this));
    zero_state_default_desk_button_ = scroll_view_contents_->AddChildView(
        std::make_unique<ZeroStateDefaultDeskButton>(this));
    zero_state_new_desk_button_ = scroll_view_contents_->AddChildView(
        std::make_unique<ZeroStateNewDeskButton>());
    scroll_view_contents_->SetLayoutManager(
        std::make_unique<BentoDesksBarLayout>(this));
  } else {
    new_desk_button_ = AddChildView(std::make_unique<NewDeskButton>());
    SetLayoutManager(
        std::make_unique<DesksBarLayout>(background_view_, new_desk_button_));
  }

  DesksController::Get()->AddObserver(this);
}

DesksBarView::~DesksBarView() {
  DesksController::Get()->RemoveObserver(this);
  if (drag_view_)
    EndDragDesk(drag_view_, /*end_by_user=*/false);
}

// static
int DesksBarView::GetBarHeightForWidth(aura::Window* root,
                                       const DesksBarView* desks_bar_view,
                                       int width) {
  if (!features::IsBentoEnabled() &&
      (width <= kUseCompactLayoutWidthThreshold ||
       (desks_bar_view &&
        width <= desks_bar_view->min_width_to_fit_contents_))) {
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
  UpdateNewMiniViews(/*initializing_bar_view=*/true,
                     /*expanding_bar_view=*/false);
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

int DesksBarView::GetMiniViewIndex(const DeskMiniView* mini_view) const {
  auto begin_iter = mini_views_.cbegin();
  auto end_iter = mini_views_.cend();
  auto iter = std::find(begin_iter, end_iter, mini_view);

  if (iter == end_iter)
    return -1;

  return std::distance(begin_iter, iter);
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

bool DesksBarView::IsZeroState() const {
  return features::IsBentoEnabled() && mini_views_.empty() &&
         DesksController::Get()->desks().size() == 1;
}

void DesksBarView::HandleStartDragEvent(DeskMiniView* mini_view,
                                        const ui::LocatedEvent& event) {
  DeskNameView::CommitChanges(GetWidget());

  gfx::PointF location = event.target()->GetScreenLocationF(event);
  StartDragDesk(mini_view, location);
}

bool DesksBarView::HandleDragEvent(DeskMiniView* mini_view,
                                   const ui::LocatedEvent& event) {
  gfx::PointF location = event.target()->GetScreenLocationF(event);
  return ContinueDragDesk(mini_view, location);
}

bool DesksBarView::HandleReleaseEvent(DeskMiniView* mini_view,
                                      const ui::LocatedEvent& event) {
  return EndDragDesk(mini_view, /*end_by_user=*/true);
}

void DesksBarView::StartDragDesk(DeskMiniView* mini_view,
                                 const gfx::PointF& location_in_screen) {
  // If another view is being dragged, then end the drag.
  if (drag_view_)
    EndDragDesk(drag_view_, /*end_by_user=*/false);

  drag_view_ = mini_view;

  gfx::PointF preview_origin_in_screen(
      drag_view_->GetPreviewBoundsInScreen().origin());
  gfx::Vector2dF drag_origin_offset =
      location_in_screen - preview_origin_in_screen;

  // Hide the dragged mini view.
  drag_view_->layer()->SetOpacity(0.0f);

  // Create a drag proxy for the dragged desk.
  drag_proxy_ =
      std::make_unique<DeskDragProxy>(this, drag_view_, drag_origin_offset);
  drag_proxy_->ScaleAndMoveTo(location_in_screen);

  Shell::Get()->cursor_manager()->SetCursor(ui::mojom::CursorType::kGrabbing);
}

bool DesksBarView::ContinueDragDesk(DeskMiniView* mini_view,
                                    const gfx::PointF& location_in_screen) {
  if (!drag_view_ || mini_view != drag_view_)
    return false;

  drag_proxy_->DragTo(location_in_screen);

  const auto drag_view_iter =
      std::find(mini_views_.cbegin(), mini_views_.cend(), drag_view_);
  DCHECK(drag_view_iter != mini_views_.cend());

  int old_index = drag_view_iter - mini_views_.cbegin();

  gfx::Point drag_pos_in_screen = drag_proxy_->GetPositionInScreen();
  gfx::Rect bar_bounds = scroll_view_contents_->GetBoundsInScreen();
  float cursor_y = location_in_screen.y();

  // Determine the target location for the desk to be reordered. If the cursor
  // is outside the desks bar, then the dragged desk will be moved to the end.
  // Otherwise, the position is determined by the drag proxy's location.
  int new_index =
      (cursor_y < bar_bounds.origin().y() || cursor_y > bar_bounds.bottom())
          ? mini_views_.size() - 1
          : DetermineMoveIndex(mini_views_, old_index, drag_pos_in_screen.x());

  if (old_index != new_index)
    Shell::Get()->desks_controller()->ReorderDesk(old_index, new_index);

  return true;
}

bool DesksBarView::EndDragDesk(DeskMiniView* mini_view, bool end_by_user) {
  if (!drag_view_ || mini_view != drag_view_)
    return false;

  // Update default desk names after dropping.
  Shell::Get()->desks_controller()->UpdateDesksDefaultNames();
  Shell::Get()->cursor_manager()->SetCursor(ui::mojom::CursorType::kPointer);

  // If the reordering is ended by the user (release the drag), perform the
  // snapping back animation. Otherwise, directly finalize the drag.
  if (end_by_user)
    drag_proxy_->SnapBackToDragView();
  else
    FinalizeDragDesk();

  return true;
}

void DesksBarView::FinalizeDragDesk() {
  if (drag_view_) {
    drag_view_->layer()->SetOpacity(1.0f);
    drag_view_ = nullptr;
  }
  drag_proxy_.reset();
}

bool DesksBarView::IsDraggingDesk() const {
  return drag_view_ != nullptr;
}

const char* DesksBarView::GetClassName() const {
  return "DesksBarView";
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

void DesksBarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  DCHECK_EQ(ui::LAYER_SOLID_COLOR, background_view_->layer()->type());
  background_view_->layer()->SetColor(
      AshColorProvider::Get()->GetShieldLayerColor(
          AshColorProvider::ShieldLayerType::kShield80));
}

bool DesksBarView::UsesCompactLayout() const {
  if (features::IsBentoEnabled())
    return false;

  return width() <= kUseCompactLayoutWidthThreshold ||
         width() <= min_width_to_fit_contents_;
}

void DesksBarView::OnDeskAdded(const Desk* desk) {
  DeskNameView::CommitChanges(GetWidget());
  const bool is_expanding_bar_view =
      features::IsBentoEnabled() && zero_state_new_desk_button_->GetVisible();
  UpdateNewMiniViews(/*initializing_bar_view=*/false, is_expanding_bar_view);
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
  const bool is_bento_enabled = features::IsBentoEnabled();
  if (is_bento_enabled)
    expanded_state_new_desk_button_->UpdateButtonState();
  else
    new_desk_button_->UpdateButtonState();

  for (auto* mini_view : mini_views_)
    mini_view->UpdateCloseButtonVisibility();

  // Switch to zero state if there is a single desk after removing.
  if (is_bento_enabled && mini_views_.size() == 1) {
    std::vector<DeskMiniView*> removed_mini_views;
    removed_mini_views.push_back(removed_mini_view);
    removed_mini_views.push_back(mini_views_[0]);
    mini_views_.clear();
    // Keep current layout until the animation is completed since the animation
    // for going back to zero state is based on the expanded bar's current
    // layout.
    PerformExpandedStateToZeroStateMiniViewAnimation(this, removed_mini_views);
    return;
  }
  overview_grid_->OnDesksChanged();
  PerformRemoveDeskMiniViewAnimation(
      removed_mini_view,
      std::vector<DeskMiniView*>(mini_views_.begin(), partition_iter),
      std::vector<DeskMiniView*>(partition_iter, mini_views_.end()),
      expanded_state_new_desk_button_, begin_x - GetFirstMiniViewXOffset());
}

void DesksBarView::OnDeskReordered(int old_index, int new_index) {
  desks_util::ReorderItem(mini_views_, old_index, new_index);

  // Update the order of child views.
  auto* reordered_view = mini_views_[new_index];
  reordered_view->parent()->ReorderChildView(reordered_view, new_index);

  overview_grid_->OnDesksChanged();

  // Call the animation function after reorder the mini views.
  PerformReorderDeskMiniViewAnimation(old_index, new_index, mini_views_);
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

void DesksBarView::UpdateNewMiniViews(bool initializing_bar_view,
                                      bool expanding_bar_view) {
  const bool is_bento_enabled = features::IsBentoEnabled();
  const auto& desks = DesksController::Get()->desks();
  if (is_bento_enabled) {
    if (initializing_bar_view)
      UpdateBentoDeskButtonsVisibility();
    if (IsZeroState() && !expanding_bar_view) {
      gfx::Transform transform;
      transform.Translate(0, -(height() - kZeroStateBarHeight));
      background_view_->layer()->SetTransform(transform);
      return;
    }
  } else if (desks.size() < 2) {
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
      DeskMiniView* mini_view = AddMiniViewAsChild(
          std::make_unique<DeskMiniView>(this, root_window, desk.get()));
      mini_views_.push_back(mini_view);
      new_mini_views.push_back(mini_view);
    }
  }

  if (is_bento_enabled && !initializing_bar_view) {
    // If Bento is enabled, focus on the newly created name view to encourge
    // users to rename their desks.
    auto* newly_added_name_view = mini_views_.back()->desk_name_view();
    newly_added_name_view->RequestFocus();

    // Set |newly_added_name_view|'s accessible name to the default desk name
    // since its text is cleared.
    newly_added_name_view->SetAccessibleName(
        DesksController::GetDeskDefaultName(desks.size() - 1));

    auto* highlight_controller = GetHighlightController();
    if (highlight_controller->IsFocusHighlightVisible())
      highlight_controller->MoveHighlightToView(newly_added_name_view);
  }

  UpdateMinimumWidthToFitContents();
  overview_grid_->OnDesksChanged();

  if (expanding_bar_view) {
    UpdateBentoDeskButtonsVisibility();
    PerformZeroStateToExpandedStateMiniViewAnimation(this);
    return;
  }

  if (initializing_bar_view)
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
  if (features::IsBentoEnabled())
    return;

  int button_width = new_desk_button_->GetMinSize(/*compact=*/false).width();
  button_width += 2 * kIconAndTextHorizontalPadding;
  button_width += kButtonRightMargin;

  if (mini_views_.empty()) {
    min_width_to_fit_contents_ = button_width;
    return;
  }

  const int mini_view_width = mini_views_[0]->GetMinWidthForDefaultLayout();
  const int mini_view_spacing = GetSpaceBetweenMiniViews(mini_views_[0]);
  const int total_mini_views_width =
      mini_views_.size() * (mini_view_width + mini_view_spacing) -
      mini_view_spacing;

  min_width_to_fit_contents_ = total_mini_views_width + button_width * 2;
}

DeskMiniView* DesksBarView::AddMiniViewAsChild(
    std::unique_ptr<DeskMiniView> mini_view) {
  return features::IsBentoEnabled()
             ? scroll_view_contents_->AddChildView(std::move(mini_view))
             : AddChildView(std::move(mini_view));
}

void DesksBarView::UpdateBentoDeskButtonsVisibility() {
  DCHECK(features::IsBentoEnabled());
  const bool is_zero_state = IsZeroState();
  zero_state_default_desk_button_->SetVisible(is_zero_state);
  zero_state_new_desk_button_->SetVisible(is_zero_state);
  expanded_state_new_desk_button_->SetVisible(!is_zero_state);
}

}  // namespace ash

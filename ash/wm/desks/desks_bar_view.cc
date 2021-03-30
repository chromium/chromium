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
#include "ash/shelf/gradient_layer_delegate.h"
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
#include "ash/wm/desks/scroll_arrow_button.h"
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
#include "ui/views/event_monitor.h"
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

// The minimum horizontal padding of the scroll view. This is set to make sure
// there is enough space for the scroll buttons.
constexpr int kScrollViewMinimumHorizontalPadding = 32;

constexpr int kScrollButtonWidth = 36;

constexpr int kGradientZoneLength = 40;

// The duration of scrolling one page.
constexpr base::TimeDelta kBarScrollDuration =
    base::TimeDelta::FromMilliseconds(250);

gfx::Rect GetGestureEventScreenRect(const ui::Event& event) {
  DCHECK(event.IsGestureEvent());
  return event.AsGestureEvent()->details().bounding_box();
}

OverviewHighlightController* GetHighlightController() {
  auto* overview_controller = Shell::Get()->overview_controller();
  DCHECK(overview_controller->InOverviewSession());
  return overview_controller->overview_session()->highlight_controller();
}

int GetSpaceBetweenMiniViews(DeskMiniView* mini_view) {
  return kMiniViewsSpacing - mini_view->GetPreviewBorderInsets().width();
}

// Translate the |background_view| by |y| on y-coordinate.
void TranslateTheBackgroundView(views::View* background_view, float y) {
  gfx::Transform transform;
  transform.Translate(0, y);
  background_view->layer()->SetTransform(transform);
}

// Initialize a scoped layer animation settings for scroll view contents.
void InitScrollContentsAnimationSettings(
    ui::ScopedLayerAnimationSettings& settings) {
  settings.SetTransitionDuration(kBarScrollDuration);
  settings.SetTweenType(gfx::Tween::ACCEL_20_DECEL_60);
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
// Layout manager for desks bar of Bento. This will lay out the direct children
// of the DesksBarView. E.g, |background_view_|, |scroll_view_| and scroll
// buttons. All the other contents that are the children of |scroll_view_| will
// be laid out by BentoDesksBarScrollViewLayout.
class BentoDesksBarLayout : public views::LayoutManager {
 public:
  BentoDesksBarLayout(DesksBarView* bar_view) : bar_view_(bar_view) {}
  BentoDesksBarLayout(const BentoDesksBarLayout&) = delete;
  BentoDesksBarLayout& operator=(const BentoDesksBarLayout&) = delete;
  ~BentoDesksBarLayout() override = default;

  // views::LayoutManager:
  void Layout(views::View* host) override {
    const gfx::Rect bar_bounds = bar_view_->bounds();
    bar_view_->background_view()->SetBoundsRect(bar_bounds);
    // Scroll buttons are kept |kScrollViewMinimumHorizontalPadding| away from
    // the edge of the scroll view. So the horizontal padding of the scroll view
    // is set to guarantee enough space for the scroll buttons.
    const gfx::Insets insets = bar_view_->overview_grid_->GetGridInsets();
    DCHECK(insets.left() == insets.right());
    const int horizontal_padding =
        std::max(kScrollViewMinimumHorizontalPadding, insets.left());
    bar_view_->left_scroll_button_->SetBounds(
        horizontal_padding - kScrollViewMinimumHorizontalPadding,
        bar_bounds.y(), kScrollButtonWidth, bar_bounds.height());
    bar_view_->right_scroll_button_->SetBounds(
        bar_bounds.right() - horizontal_padding -
            (kScrollButtonWidth - kScrollViewMinimumHorizontalPadding),
        bar_bounds.y(), kScrollButtonWidth, bar_bounds.height());

    gfx::Rect scroll_bounds = bar_bounds;
    // Align with the overview grid in horizontal, so only horizontal insets are
    // needed here.
    scroll_bounds.Inset(horizontal_padding, 0);
    bar_view_->scroll_view_->SetBoundsRect(scroll_bounds);

    // Clip the contents that are outside of the |scroll_view_|'s bounds.
    bar_view_->scroll_view_->layer()->SetMasksToBounds(true);
    bar_view_->UpdateScrollButtonsVisibility();
    bar_view_->UpdateGradientZone();

    bar_view_->scroll_view_->Layout();
  }

  gfx::Size GetPreferredSize(const views::View* host) const override {
    return bar_view_->bounds().size();
  }

 private:
  DesksBarView* bar_view_;  // Not owned.
};

// -----------------------------------------------------------------------------
// BentoDesksBarScrollViewLayout:

// In Bento, all the desks bar contents except the background view are added to
// be the children of the |scroll_view_| to support scrollable desks bar.
// BentoDesksBarScrollViewLayout will help lay out the contents of the
// |scroll_view_|. There is no compact layout in Bento and contents that be
// scrolled to outside of the scroll view will be clipped.
class BentoDesksBarScrollViewLayout : public views::LayoutManager {
 public:
  BentoDesksBarScrollViewLayout(DesksBarView* bar_view) : bar_view_(bar_view) {}
  BentoDesksBarScrollViewLayout(const BentoDesksBarScrollViewLayout&) = delete;
  BentoDesksBarScrollViewLayout& operator=(
      const BentoDesksBarScrollViewLayout&) = delete;
  ~BentoDesksBarScrollViewLayout() override = default;

  // views::LayoutManager:
  void Layout(views::View* host) override {
    const gfx::Rect scroll_bounds = bar_view_->scroll_view_->bounds();
    // |host| here is |scroll_view_contents_|.
    if (bar_view_->IsZeroState()) {
      host->SetBoundsRect(scroll_bounds);
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
      zero_state_default_desk_button->SetBoundsRect(gfx::Rect(
          gfx::Point((scroll_bounds.width() - content_width) / 2, kZeroStateY),
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

      // Keep the background view's translation updated while the height of bar
      // changes. E.g, it could happens while zooming in/out the bar. Note, the
      // background view is only translated while in zero state.
      TranslateTheBackgroundView(
          bar_view_->background_view(),
          -(scroll_bounds.height() - DesksBarView::kZeroStateBarHeight));
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
    width_ = std::max(scroll_bounds.width(), content_width);

    // Update the size of the |host|, which is |scroll_view_contents_| here.
    // This is done to make sure its size can be updated on mini views' adding
    // or removing, then |scroll_view_| will know whether the contents need to
    // be scolled or not.
    host->SetSize(gfx::Size(width_, scroll_bounds.height()));

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
    SetLayoutManager(std::make_unique<BentoDesksBarLayout>(this));
    scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
    scroll_view_->SetPaintToLayer();
    scroll_view_->layer()->SetFillsBoundsOpaquely(false);
    scroll_view_->SetBackgroundColor(base::nullopt);
    scroll_view_->SetDrawOverflowIndicator(false);
    scroll_view_->SetHorizontalScrollBarMode(
        views::ScrollView::ScrollBarMode::kHiddenButEnabled);
    scroll_view_->SetTreatAllScrollEventsAsHorizontal(true);

    left_scroll_button_ = AddChildView(std::make_unique<ScrollArrowButton>(
        base::BindRepeating(&DesksBarView::ScrollToPreviousPage,
                            base::Unretained(this)),
        /*is_left_arrow=*/true, this));
    right_scroll_button_ = AddChildView(std::make_unique<ScrollArrowButton>(
        base::BindRepeating(&DesksBarView::ScrollToNextPage,
                            base::Unretained(this)),
        /*is_left_arrow=*/false, this));

    scroll_view_contents_ =
        scroll_view_->SetContents(std::make_unique<views::View>());
    // Make the scroll content view animable by painting to a layer.
    scroll_view_contents_->SetPaintToLayer();
    expanded_state_new_desk_button_ = scroll_view_contents_->AddChildView(
        std::make_unique<ExpandedStateNewDeskButton>(this));
    zero_state_default_desk_button_ = scroll_view_contents_->AddChildView(
        std::make_unique<ZeroStateDefaultDeskButton>(this));
    zero_state_new_desk_button_ = scroll_view_contents_->AddChildView(
        std::make_unique<ZeroStateNewDeskButton>());
    scroll_view_contents_->SetLayoutManager(
        std::make_unique<BentoDesksBarScrollViewLayout>(this));

    gradient_layer_delegate_ = std::make_unique<GradientLayerDelegate>();
    scroll_view_->layer()->SetMaskLayer(gradient_layer_delegate_->layer());

    scroll_view_->AddScrollViewObserver(this);
  } else {
    new_desk_button_ = AddChildView(std::make_unique<NewDeskButton>());
    SetLayoutManager(
        std::make_unique<DesksBarLayout>(background_view_, new_desk_button_));
  }

  DesksController::Get()->AddObserver(this);
}

DesksBarView::~DesksBarView() {
  DesksController::Get()->RemoveObserver(this);
  if (features::IsBentoEnabled())
    scroll_view_->RemoveScrollViewObserver(this);
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

void DesksBarView::HandlePressEvent(DeskMiniView* mini_view,
                                    const ui::LocatedEvent& event) {
  DeskNameView::CommitChanges(GetWidget());

  gfx::PointF location = event.target()->GetScreenLocationF(event);
  InitDragDesk(mini_view, location);
}

void DesksBarView::HandleLongPressEvent(DeskMiniView* mini_view,
                                        const ui::LocatedEvent& event) {
  DeskNameView::CommitChanges(GetWidget());

  // Initialize and start drag.
  gfx::PointF location = event.target()->GetScreenLocationF(event);
  InitDragDesk(mini_view, location);
  StartDragDesk(mini_view, location);
}

void DesksBarView::HandleDragEvent(DeskMiniView* mini_view,
                                   const ui::LocatedEvent& event) {
  // Do not perform drag if drag proxy is not initialized.
  if (!drag_proxy_)
    return;

  gfx::PointF location = event.target()->GetScreenLocationF(event);

  // If the drag proxy is initialized, start the drag. If the drag started,
  // continue drag.
  switch (drag_proxy_->state()) {
    case DeskDragProxy::State::kInitialized:
      StartDragDesk(mini_view, location);
      break;
    case DeskDragProxy::State::kStarted:
      ContinueDragDesk(mini_view, location);
      break;
    default:
      NOTREACHED();
  }
}

bool DesksBarView::HandleReleaseEvent(DeskMiniView* mini_view,
                                      const ui::LocatedEvent& event) {
  // Do not end drag if the proxy is not initialized.
  if (!drag_proxy_)
    return false;

  // If the drag didn't start, finalize the drag. Otherwise, end the drag and
  // snap back the desk.
  switch (drag_proxy_->state()) {
    case DeskDragProxy::State::kInitialized:
      FinalizeDragDesk();
      return false;
    case DeskDragProxy::State::kStarted:
      EndDragDesk(mini_view, /*end_by_user=*/true);
      break;
    default:
      NOTREACHED();
  }
  return true;
}

void DesksBarView::InitDragDesk(DeskMiniView* mini_view,
                                const gfx::PointF& location_in_screen) {
  // If another view is being dragged, then end the drag.
  if (drag_view_)
    EndDragDesk(drag_view_, /*end_by_user=*/false);

  drag_view_ = mini_view;

  gfx::PointF preview_origin_in_screen(
      drag_view_->GetPreviewBoundsInScreen().origin());
  const float init_offset_x =
      location_in_screen.x() - preview_origin_in_screen.x();

  // Create a drag proxy for the dragged desk.
  drag_proxy_ =
      std::make_unique<DeskDragProxy>(this, drag_view_, init_offset_x);
}

void DesksBarView::StartDragDesk(DeskMiniView* mini_view,
                                 const gfx::PointF& location_in_screen) {
  DCHECK(drag_view_);
  DCHECK(drag_proxy_);
  DCHECK_EQ(mini_view, drag_view_);

  // Hide the dragged mini view.
  drag_view_->layer()->SetOpacity(0.0f);

  // Create a drag proxy widget, scale it up and move its x-coordinate according
  // to the x of |location_in_screen|.
  drag_proxy_->InitAndScaleAndMoveToX(location_in_screen.x());

  Shell::Get()->cursor_manager()->SetCursor(ui::mojom::CursorType::kGrabbing);
}

void DesksBarView::ContinueDragDesk(DeskMiniView* mini_view,
                                    const gfx::PointF& location_in_screen) {
  DCHECK(drag_view_);
  DCHECK(drag_proxy_);
  DCHECK_EQ(mini_view, drag_view_);

  drag_proxy_->DragToX(location_in_screen.x());

  // Check if the desk is on the scroll arrow buttons. Do not determine move
  // index while scrolling, since the positions of the desks on bar keep varying
  // during this process.
  if (MaybeScrollByDraggedDesk())
    return;

  const auto drag_view_iter =
      std::find(mini_views_.cbegin(), mini_views_.cend(), drag_view_);
  DCHECK(drag_view_iter != mini_views_.cend());

  const int old_index = drag_view_iter - mini_views_.cbegin();

  const int drag_pos_screen_x = drag_proxy_->GetBoundsInScreen().origin().x();

  // Determine the target location for the desk to be reordered.
  const int new_index = DetermineMoveIndex(drag_pos_screen_x);

  if (old_index != new_index)
    Shell::Get()->desks_controller()->ReorderDesk(old_index, new_index);
}

void DesksBarView::EndDragDesk(DeskMiniView* mini_view, bool end_by_user) {
  DCHECK(drag_view_);
  DCHECK(drag_proxy_);
  DCHECK_EQ(mini_view, drag_view_);

  // Update default desk names after dropping.
  Shell::Get()->desks_controller()->UpdateDesksDefaultNames();
  Shell::Get()->cursor_manager()->SetCursor(ui::mojom::CursorType::kPointer);

  // Stop scroll even if the desk is on the scroll arrow buttons.
  left_scroll_button_->OnDeskHoverEnd();
  right_scroll_button_->OnDeskHoverEnd();

  // If the reordering is ended by the user (release the drag), perform the
  // snapping back animation and scroll the bar to target position. If current
  // drag is ended due to the start of a new drag or the end of the overview,
  // directly finalize current drag.
  if (end_by_user) {
    ScrollToShowMiniViewIfNecessary(drag_view_);
    drag_proxy_->SnapBackToDragView();
  } else {
    FinalizeDragDesk();
  }
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

void DesksBarView::OnContentsScrolled() {
  UpdateScrollButtonsVisibility();
}

void DesksBarView::UpdateNewMiniViews(bool initializing_bar_view,
                                      bool expanding_bar_view) {
  const bool is_bento_enabled = features::IsBentoEnabled();
  const auto& desks = DesksController::Get()->desks();
  if (is_bento_enabled) {
    if (initializing_bar_view)
      UpdateBentoDeskButtonsVisibility();
    if (IsZeroState() && !expanding_bar_view)
      return;
  } else if (desks.size() < 2) {
    // We do not show mini_views when we have a single desk.
    DCHECK(mini_views_.empty());

    // The bar background is initially translated off the screen.
    TranslateTheBackgroundView(background_view_, -height());
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

void DesksBarView::ScrollToShowMiniViewIfNecessary(
    const DeskMiniView* mini_view) {
  DCHECK(base::Contains(mini_views_, mini_view));
  const gfx::Rect visible_bounds = scroll_view_->GetVisibleRect();
  const gfx::Rect mini_view_bounds = mini_view->bounds();
  const bool beyond_left = mini_view_bounds.x() < visible_bounds.x();
  const bool beyond_right = mini_view_bounds.right() > visible_bounds.right();
  auto* scroll_bar = scroll_view_->horizontal_scroll_bar();
  if (beyond_left) {
    scroll_view_->ScrollToPosition(
        scroll_bar, mini_view_bounds.right() - scroll_view_->bounds().width());
  } else if (beyond_right) {
    scroll_view_->ScrollToPosition(scroll_bar, mini_view_bounds.x());
  }
}

int DesksBarView::DetermineMoveIndex(int location_screen_x) const {
  const int views_size = static_cast<int>(mini_views_.size());

  // We find the target position according to the x-axis coordinate of the
  // desks' center positions in screen in ascending order. Therefore, if the
  // desks bar is mirrored, check from right to left, otherwise check from left
  // to right.
  const bool mirrored = GetMirrored();
  const int start_index = mirrored ? views_size - 1 : 0;
  const int end_index = mirrored ? -1 : views_size;
  const int iter_step = mirrored ? -1 : 1;

  for (int new_index = start_index; new_index != end_index;
       new_index += iter_step) {
    auto* mini_view = mini_views_[new_index];

    // Note that we cannot directly use |GetBoundsInScreen|. Because we may
    // perform animation (transform) on mini views. The bounds gotten from
    // |GetBoundsInScreen| may be the intermediate bounds during animation.
    // Therefore, we transfer a mini view's origin from its parent level to
    // avoid the influence of its own transform.
    gfx::Point center_screen_pos = mini_view->GetMirroredBounds().CenterPoint();
    views::View::ConvertPointToScreen(mini_view->parent(), &center_screen_pos);
    if (location_screen_x < center_screen_pos.x())
      return new_index;
  }

  return end_index - iter_step;
}

bool DesksBarView::MaybeScrollByDraggedDesk() {
  DCHECK(drag_proxy_);

  const gfx::Rect proxy_bounds = drag_proxy_->GetBoundsInScreen();

  // If the desk proxy overlaps a scroll button, scroll the bar in the
  // corresponding direction.
  for (auto* scroll_button : {left_scroll_button_, right_scroll_button_}) {
    if (scroll_button->GetVisible() &&
        proxy_bounds.Intersects(scroll_button->GetBoundsInScreen())) {
      scroll_button->OnDeskHoverStart();
      return true;
    }
    scroll_button->OnDeskHoverEnd();
  }

  return false;
}

DeskMiniView* DesksBarView::FindMiniViewForDesk(const Desk* desk) const {
  for (auto* mini_view : mini_views_) {
    if (mini_view->desk() == desk)
      return mini_view;
  }

  return nullptr;
}

int DesksBarView::GetFirstMiniViewXOffset() const {
  // GetMirroredX is used here to make sure the removing and adding a desk
  // transform is correct while in RTL layout.
  return mini_views_.empty() ? bounds().CenterPoint().x()
                             : mini_views_[0]->GetMirroredX();
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

void DesksBarView::UpdateScrollButtonsVisibility() {
  const gfx::Rect visible_bounds = scroll_view_->GetVisibleRect();
  const bool left_visible = visible_bounds.x() > 0;
  const bool right_visible =
      visible_bounds.right() < scroll_view_contents_->bounds().width();
  left_scroll_button_->SetVisible(left_visible);
  right_scroll_button_->SetVisible(right_visible);
}

void DesksBarView::UpdateGradientZone() {
  const bool is_rtl = base::i18n::IsRTL();
  const bool is_left_scroll_button_visible = left_scroll_button_->GetVisible();
  const bool is_right_scroll_button_visible =
      right_scroll_button_->GetVisible();
  const bool is_left_visible_only =
      is_left_scroll_button_visible && !is_right_scroll_button_visible;
  const bool is_right_visible_only =
      !is_left_scroll_button_visible && is_right_scroll_button_visible;

  // Only showing the gradient while scrolled to the start or end position of
  // the scroll view.
  const bool should_show_start_gradient =
      is_rtl ? is_right_visible_only : is_left_visible_only;
  const bool should_show_end_gradient =
      is_rtl ? is_left_visible_only : is_right_visible_only;

  // The bounds of the start and end gradient will be the same regardless it is
  // LTR or RTL layout. While the |left_scroll_button_| will be changed from
  // left to right and |right_scroll_button_| will be changed from right to left
  // if it is RTL layout.
  const gfx::Rect bounds = scroll_view_->bounds();
  gfx::Rect start_gradient_bounds, end_gradient_bounds;
  if (should_show_start_gradient) {
    start_gradient_bounds =
        gfx::Rect(0, 0, kGradientZoneLength, bounds.height());
  }
  if (should_show_end_gradient) {
    end_gradient_bounds = gfx::Rect(bounds.width() - kGradientZoneLength, 0,
                                    kGradientZoneLength, bounds.height());
  }
  const GradientLayerDelegate::FadeZone start_gradient_zone = {
      start_gradient_bounds,
      /*fade_in=*/true,
      /*is_horizontal=*/true};
  const GradientLayerDelegate::FadeZone end_gradient_zone = {
      end_gradient_bounds,
      /*fade_in=*/false,
      /*is_horizonal=*/true};
  gradient_layer_delegate_->set_start_fade_zone(start_gradient_zone);
  gradient_layer_delegate_->set_end_fade_zone(end_gradient_zone);
  gradient_layer_delegate_->layer()->SetBounds(scroll_view_->layer()->bounds());
}

void DesksBarView::ScrollToPreviousPage() {
  ui::ScopedLayerAnimationSettings settings(
      scroll_view_contents_->layer()->GetAnimator());
  InitScrollContentsAnimationSettings(settings);
  scroll_view_->ScrollToPosition(
      scroll_view_->horizontal_scroll_bar(),
      scroll_view_->GetVisibleRect().x() - scroll_view_->width());
}

void DesksBarView::ScrollToNextPage() {
  ui::ScopedLayerAnimationSettings settings(
      scroll_view_contents_->layer()->GetAnimator());
  InitScrollContentsAnimationSettings(settings);
  scroll_view_->ScrollToPosition(
      scroll_view_->horizontal_scroll_bar(),
      scroll_view_->GetVisibleRect().x() + scroll_view_->width());
}

}  // namespace ash

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/multi_window_resize_controller.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_metrics.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "base/containers/adapters.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

// Delay before hiding the `resize_widget_`.
constexpr base::TimeDelta kHideDelay = base::Milliseconds(500);

// Padding from the bottom/right edge the resize widget is shown at.
const int kResizeWidgetPadding = 15;

// The size of the resize widget.
constexpr int kLongSide = 64;
constexpr int kShortSide = 52;

// Returns the widget init params needed to create the resize widget.
views::Widget::InitParams CreateWidgetParams(aura::Window* parent_window,
                                             const std::string& widget_name) {
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.parent = parent_window;
  params.name = widget_name;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  return params;
}

gfx::PointF ConvertPointFromScreen(aura::Window* window,
                                   const gfx::PointF& point) {
  gfx::PointF result(point);
  ::wm::ConvertPointFromScreen(window, &result);
  return result;
}

gfx::Point ConvertPointToTarget(aura::Window* source,
                                aura::Window* target,
                                const gfx::Point& point) {
  gfx::Point result(point);
  aura::Window::ConvertPointToTarget(source, target, &result);
  return result;
}

gfx::Rect ConvertRectToScreen(aura::Window* source, const gfx::Rect& rect) {
  gfx::Rect result(rect);
  ::wm::ConvertRectToScreen(source, &result);
  return result;
}

bool ContainsX(aura::Window* window, int x) {
  return x >= 0 && x <= window->bounds().width();
}

bool ContainsScreenX(aura::Window* window, int x_in_screen) {
  gfx::PointF window_loc =
      ConvertPointFromScreen(window, gfx::PointF(x_in_screen, 0));
  return ContainsX(window, window_loc.x());
}

bool ContainsY(aura::Window* window, int y) {
  return y >= 0 && y <= window->bounds().height();
}

bool ContainsScreenY(aura::Window* window, int y_in_screen) {
  gfx::PointF window_loc =
      ConvertPointFromScreen(window, gfx::PointF(0, y_in_screen));
  return ContainsY(window, window_loc.y());
}

// Returns true if `p` is on the edge `edge_want` of `window`.
bool PointOnWindowEdge(aura::Window* window,
                       int edge_want,
                       const gfx::Point& p) {
  switch (edge_want) {
    case HTLEFT:
      return ContainsY(window, p.y()) && p.x() == 0;
    case HTRIGHT:
      return ContainsY(window, p.y()) && p.x() == window->bounds().width();
    case HTTOP:
      return ContainsX(window, p.x()) && p.y() == 0;
    case HTBOTTOM:
      return ContainsX(window, p.x()) && p.y() == window->bounds().height();
    default:
      NOTREACHED();
  }
}

bool Intersects(int x1, int max_1, int x2, int max_2) {
  return x2 <= max_1 && max_2 > x1;
}

}  // namespace

// -----------------------------------------------------------------------------
// ResizeView:
// View contained in the widget. Passes along mouse events to the
// MultiWindowResizeController so that it can start/stop the resize loop.

class MultiWindowResizeController::ResizeView : public views::View {
 public:
  ResizeView(MultiWindowResizeController* controller, Direction direction)
      : controller_(controller), direction_(direction) {}

  ResizeView(const ResizeView&) = delete;
  ResizeView& operator=(const ResizeView&) = delete;
  ~ResizeView() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    const bool vert = direction_ == Direction::kLeftRight;
    return gfx::Size(vert ? kLongSide : kShortSide,
                     vert ? kShortSide : kLongSide);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setColor(
        GetColorProvider()->GetColor(cros_tokens::kCrosSysSystemBaseElevated));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);

    canvas->DrawPath(GeneratePath(GetLocalBounds()), flags);

    // Paint the chevron icons.
    constexpr int kIconSize = 20;
    constexpr int kHalfLong = kLongSide / 2;

    // Paint the left / up chevron icon.
    canvas->Save();
    int long_offset = (kHalfLong - kIconSize) / 2;
    int short_offset = (kShortSide - kIconSize) / 2;
    canvas->Translate(direction_ == Direction::kLeftRight
                          ? gfx::Vector2d(long_offset, short_offset)
                          : gfx::Vector2d(short_offset, long_offset));
    gfx::PaintVectorIcon(
        canvas,
        direction_ == Direction::kLeftRight ? kOverflowShelfLeftIcon
                                            : kChevronUpSmallIcon,
        kIconSize,
        GetColorProvider()->GetColor(cros_tokens::kCrosSysOnSurface));
    canvas->Restore();

    // Paint the right / down chevron icon.
    canvas->Save();
    long_offset = kHalfLong + (kHalfLong - kIconSize) / 2;
    canvas->Translate(direction_ == Direction::kLeftRight
                          ? gfx::Vector2d(long_offset, short_offset)
                          : gfx::Vector2d(short_offset, long_offset));
    gfx::PaintVectorIcon(
        canvas,
        direction_ == Direction::kLeftRight ? kOverflowShelfRightIcon
                                            : kChevronDownSmallIcon,
        kIconSize,
        GetColorProvider()->GetColor(cros_tokens::kCrosSysOnSurface));
    canvas->Restore();
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    gfx::Point location(event.location());
    views::View::ConvertPointToScreen(this, &location);
    controller_->StartResize(gfx::PointF(location));
    return true;
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    gfx::Point location(event.location());
    views::View::ConvertPointToScreen(this, &location);
    controller_->Resize(gfx::PointF(location), event.flags());
    return true;
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    controller_->CompleteResize();
  }

  void OnMouseCaptureLost() override { controller_->CancelResize(); }

  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override {
    int component = (direction_ == Direction::kLeftRight) ? HTRIGHT : HTBOTTOM;
    return ::wm::CompoundEventFilter::CursorForWindowComponent(component);
  }

 private:
  raw_ptr<MultiWindowResizeController> controller_;
  const Direction direction_;

  SkPath GeneratePath(const gfx::Rect& bounds) {
    //           /\
    //      ----    ----
    //    /              \
    //    \              /
    //      ----    ----
    //           \/
    //
    // Generate the path for the shape above when `direction_` is
    // `Direction::kLeftRight`. If the `direction_` is `Direction::kTopBottom`,
    // generate the path for the shape above with 90 degree rotated.

    static constexpr int kLargeCurveRadius = 16;
    static constexpr int kSmallCurveRadius = 10;

    // The resize shape is symmetric horizontally and vertically, hence only
    // need to manually generate the path for the quarter and then flip twice;
    const gfx::RectF quarter_bounds(bounds.x(), bounds.y(), bounds.width() / 2,
                                    bounds.height() / 2);
    SkPath path;
    if (direction_ == Direction::kLeftRight) {
      //           /|
      //      ----  |
      //    / ____  |

      // Generate the path for the quarter of the resize shape which looks like
      // the shape above, starting from left bottom to the right top and then
      // back to the left bottom.
      path.moveTo(quarter_bounds.x(), quarter_bounds.bottom());
      path.arcTo(
          quarter_bounds.x(), quarter_bounds.bottom() - kLargeCurveRadius,
          quarter_bounds.x() + kLargeCurveRadius,
          quarter_bounds.bottom() - kLargeCurveRadius, kLargeCurveRadius);
      path.lineTo(quarter_bounds.right() - kSmallCurveRadius,
                  quarter_bounds.bottom() - kLargeCurveRadius);
      path.arcTo(quarter_bounds.right(),
                 quarter_bounds.bottom() - kLargeCurveRadius,
                 quarter_bounds.right(), quarter_bounds.y(), kSmallCurveRadius);
      path.lineTo(quarter_bounds.right(), quarter_bounds.bottom());
    } else {
      // Similar to the way when `direction_` is `Direction::kLeftRight`,
      // starting from the right top to the left bottom and then back to the
      // right top.
      path.moveTo(quarter_bounds.right(), quarter_bounds.y());
      path.arcTo(quarter_bounds.right() - kLargeCurveRadius, quarter_bounds.y(),
                 quarter_bounds.right() - kLargeCurveRadius,
                 quarter_bounds.y() + kLargeCurveRadius, kLargeCurveRadius);
      path.lineTo(quarter_bounds.right() - kLargeCurveRadius,
                  quarter_bounds.bottom() - kSmallCurveRadius);
      path.arcTo(quarter_bounds.right() - kLargeCurveRadius,
                 quarter_bounds.bottom(), quarter_bounds.x(),
                 quarter_bounds.bottom(), kSmallCurveRadius);
      path.lineTo(quarter_bounds.right(), quarter_bounds.bottom());
    }
    path.close();

    // Flip vertically and horizontally and vertically to get the full path.
    SkMatrix flip;
    flip.setScale(1, -1, quarter_bounds.width(), quarter_bounds.height());
    path.addPath(path, flip);
    flip.setScale(-1, 1, quarter_bounds.width(), quarter_bounds.height());
    path.addPath(path, flip);

    return path;
  }
};

// -----------------------------------------------------------------------------
// ResizeMouseWatcherHost:
// MouseWatcherHost implementation for MultiWindowResizeController. Forwards
// Contains() to MultiWindowResizeController.

class MultiWindowResizeController::ResizeMouseWatcherHost
    : public views::MouseWatcherHost {
 public:
  explicit ResizeMouseWatcherHost(MultiWindowResizeController* host)
      : host_(host) {}

  ResizeMouseWatcherHost(const ResizeMouseWatcherHost&) = delete;
  ResizeMouseWatcherHost& operator=(const ResizeMouseWatcherHost&) = delete;
  ~ResizeMouseWatcherHost() override = default;

  // views::MouseWatcherHost:
  bool Contains(const gfx::Point& point_in_screen, EventType type) override {
    return (type == EventType::kPress)
               ? host_->IsOverResizeWidget(point_in_screen)
               : host_->IsOverWindows(point_in_screen);
  }

 private:
  raw_ptr<MultiWindowResizeController> host_;
};

MultiWindowResizeController::ResizeWindows::ResizeWindows()
    : direction(Direction::kTopBottom) {}

MultiWindowResizeController::ResizeWindows::ResizeWindows(
    const ResizeWindows& other) = default;

MultiWindowResizeController::ResizeWindows::~ResizeWindows() = default;

bool MultiWindowResizeController::ResizeWindows::Equals(
    const ResizeWindows& other) const {
  return window1 == other.window1 && window2 == other.window2 &&
         direction == other.direction;
}

// -----------------------------------------------------------------------------
// MultiWindowResizeController:

MultiWindowResizeController::MultiWindowResizeController() {
  Shell::Get()->overview_controller()->AddObserver(this);
}

MultiWindowResizeController::~MultiWindowResizeController() {
  if (Shell::Get()->overview_controller()) {
    Shell::Get()->overview_controller()->RemoveObserver(this);
  }

  ResetResizer();
}

void MultiWindowResizeController::Show(aura::Window* window,
                                       int component,
                                       const gfx::Point& point_in_window) {
  // When the resize widget is showing we ignore Show() requests. Instead we
  // only care about mouse movements from MouseWatcher. This is necessary as
  // WorkspaceEventHandler only sees mouse movements over the windows, not all
  // windows or over the desktop.
  if (resize_widget_)
    return;

  ResizeWindows windows(DetermineWindows(window, component, point_in_window));
  if (IsShowing() && windows_.Equals(windows))
    return;

  Hide();
  if (!windows.is_valid()) {
    windows_ = ResizeWindows();
    return;
  }

  windows_ = windows;
  StartObserving(windows_.window1);
  StartObserving(windows_.window2);
  show_location_in_parent_ =
      ConvertPointToTarget(window, window->parent(), point_in_window);
  show_timer_.Start(FROM_HERE, kShowDelay, this,
                    &MultiWindowResizeController::ShowIfValidMouseLocation);
}

void MultiWindowResizeController::MouseMovedOutOfHost() {
  Hide();
}

void MultiWindowResizeController::OnWindowPropertyChanged(aura::Window* window,
                                                          const void* key,
                                                          intptr_t old) {
  // If the window is now non-resizeable, make sure the resizer is not showing.
  if ((window->GetProperty(aura::client::kResizeBehaviorKey) &
       aura::client::kResizeBehaviorCanResize) == 0)
    ResetResizer();
}

void MultiWindowResizeController::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  // OnWindowVisibilityChanged() is fired not only for observed windows but
  // also its descendants (and ancestors), but multi-window resizing should keep
  // running even if the resized window’s child window gets hidden. So here, we
  // only handles events for windows being resized (i.e. observed windows).
  if (!IsObserving(window))
    return;

  if (!visible)
    ResetResizer();
}

void MultiWindowResizeController::OnWindowDestroying(aura::Window* window) {
  ResetResizer();
}

void MultiWindowResizeController::OnPostWindowStateTypeChange(
    WindowState* window_state,
    chromeos::WindowStateType old_type) {
  if (window_state->IsMaximized() || window_state->IsFullscreen() ||
      window_state->IsMinimized()) {
    ResetResizer();
  }
}

void MultiWindowResizeController::OnOverviewModeStarting() {
  // Hide resizing UI when entering overview.
  Shell::Get()->resize_shadow_controller()->HideAllShadows();
  ResetResizer();
}

void MultiWindowResizeController::OnOverviewModeEndingAnimationComplete(
    bool canceled) {
  if (canceled) {
    return;
  }

  // Show shadow for the resizer after exiting overview.
  Shell::Get()->resize_shadow_controller()->TryShowAllShadows();
}

MultiWindowResizeController::ResizeWindows
MultiWindowResizeController::DetermineWindowsFromScreenPoint(
    aura::Window* window) const {
  gfx::Point mouse_location(
      display::Screen::GetScreen()->GetCursorScreenPoint());
  wm::ConvertPointFromScreen(window, &mouse_location);
  const int component =
      window_util::GetNonClientComponent(window, mouse_location);
  return DetermineWindows(window, component, mouse_location);
}

void MultiWindowResizeController::CreateMouseWatcher() {
  mouse_watcher_ = std::make_unique<views::MouseWatcher>(
      std::make_unique<ResizeMouseWatcherHost>(this), this);
  mouse_watcher_->set_notify_on_exit_time(kHideDelay);
  DCHECK(resize_widget_);
  mouse_watcher_->Start(resize_widget_->GetNativeWindow());
}

MultiWindowResizeController::ResizeWindows
MultiWindowResizeController::DetermineWindows(aura::Window* window,
                                              int window_component,
                                              const gfx::Point& point) const {
  ResizeWindows result;

  // Check if the window is non-resizeable.
  if ((window->GetProperty(aura::client::kResizeBehaviorKey) &
       aura::client::kResizeBehaviorCanResize) == 0) {
    return result;
  }

  gfx::Point point_in_parent =
      ConvertPointToTarget(window, window->parent(), point);
  switch (window_component) {
    case HTRIGHT:
      result.direction = Direction::kLeftRight;
      result.window1 = window;
      result.window2 = FindWindowByEdge(
          window, HTLEFT, window->bounds().right(), point_in_parent.y());
      break;
    case HTLEFT:
      result.direction = Direction::kLeftRight;
      result.window1 = FindWindowByEdge(window, HTRIGHT, window->bounds().x(),
                                        point_in_parent.y());
      result.window2 = window;
      break;
    case HTTOP:
      result.direction = Direction::kTopBottom;
      result.window1 = FindWindowByEdge(window, HTBOTTOM, point_in_parent.x(),
                                        window->bounds().y());
      result.window2 = window;
      break;
    case HTBOTTOM:
      result.direction = Direction::kTopBottom;
      result.window1 = window;
      result.window2 = FindWindowByEdge(window, HTTOP, point_in_parent.x(),
                                        window->bounds().bottom());
      break;
    default:
      break;
  }
  return result;
}

aura::Window* MultiWindowResizeController::FindWindowByEdge(
    aura::Window* window_to_ignore,
    int edge_want,
    int x_in_parent,
    int y_in_parent) const {
  aura::Window* parent = window_to_ignore->parent();
  const aura::Window::Windows& windows = parent->children();
  for (aura::Window* window : base::Reversed(windows)) {
    if (window == window_to_ignore || !window->IsVisible())
      continue;

    // Ignore windows without a non-client area.
    if (!window->delegate())
      continue;

    // Return the window if it is resizeable and the wanted edge has the point.
    if ((window->GetProperty(aura::client::kResizeBehaviorKey) &
         aura::client::kResizeBehaviorCanResize) != 0 &&
        PointOnWindowEdge(
            window, edge_want,
            ConvertPointToTarget(parent, window,
                                 gfx::Point(x_in_parent, y_in_parent)))) {
      return window;
    }

    // Having determined that the window is not a suitable return value, if it
    // contains the point, then it is obscuring that point on any remaining
    // window that also contains the point.
    if (window->bounds().Contains(x_in_parent, y_in_parent))
      return nullptr;
  }
  return nullptr;
}

aura::Window* MultiWindowResizeController::FindWindowTouching(
    aura::Window* window,
    Direction direction) const {
  int right = window->bounds().right();
  int bottom = window->bounds().bottom();
  aura::Window* parent = window->parent();
  const aura::Window::Windows& windows = parent->children();
  for (aura::Window* other : base::Reversed(windows)) {
    if (other == window || !other->IsVisible())
      continue;
    switch (direction) {
      case Direction::kTopBottom:
        if (other->bounds().y() == bottom &&
            Intersects(other->bounds().x(), other->bounds().right(),
                       window->bounds().x(), window->bounds().right())) {
          return other;
        }
        break;
      case Direction::kLeftRight:
        if (other->bounds().x() == right &&
            Intersects(other->bounds().y(), other->bounds().bottom(),
                       window->bounds().y(), window->bounds().bottom())) {
          return other;
        }
        break;
      default:
        NOTREACHED();
    }
  }
  return nullptr;
}

void MultiWindowResizeController::FindWindowsTouching(
    aura::Window* start,
    Direction direction,
    aura::Window::Windows* others) const {
  while (start) {
    start = FindWindowTouching(start, direction);
    if (start)
      others->push_back(start);
  }
}

void MultiWindowResizeController::StartObserving(aura::Window* window) {
  window_observations_.AddObservation(window);
  window_state_observations_.AddObservation(WindowState::Get(window));
}

void MultiWindowResizeController::StopObserving(aura::Window* window) {
  window_observations_.RemoveObservation(window);
  window_state_observations_.RemoveObservation(WindowState::Get(window));
}

bool MultiWindowResizeController::IsObserving(aura::Window* window) const {
  return window_observations_.IsObservingSource(window);
}

void MultiWindowResizeController::ShowIfValidMouseLocation() {
  if (DetermineWindowsFromScreenPoint(windows_.window1).Equals(windows_) ||
      DetermineWindowsFromScreenPoint(windows_.window2).Equals(windows_)) {
    ShowNow();
  } else {
    Hide();
  }
}

void MultiWindowResizeController::ShowNow() {
  DCHECK(!resize_widget_.get());
  DCHECK(windows_.is_valid());
  show_timer_.Stop();
  aura::Window* window1 = windows_.window1;
  aura::Window* window2 = windows_.window2;

  resize_widget_ = std::make_unique<views::Widget>();
  resize_widget_->set_focus_on_creation(false);
  aura::Window* parent_window = window1->GetRootWindow()->GetChildById(
      kShellWindowId_AlwaysOnTopContainer);
  resize_widget_->Init(CreateWidgetParams(
      parent_window, /*widget_name=*/"MultiWindowResizeController"));

  ::wm::SetWindowVisibilityAnimationType(
      resize_widget_->GetNativeWindow(),
      ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  resize_widget_->SetContentsView(
      std::make_unique<ResizeView>(this, windows_.direction));
  gfx::Rect resize_widget_bounds =
      CalculateResizeWidgetBounds(gfx::PointF(show_location_in_parent_));
  resize_widget_show_bounds_in_screen_ =
      ConvertRectToScreen(window1->parent(), resize_widget_bounds);
  resize_widget_->SetBounds(resize_widget_show_bounds_in_screen_);
  resize_widget_->Show();

  base::RecordAction(base::UserMetricsAction(kMultiWindowResizerShow));
  base::UmaHistogramBoolean(kMultiWindowResizerShowHistogramName, true);

  if (WindowState::Get(window1)->IsSnapped() &&
      WindowState::Get(window2)->IsSnapped()) {
    base::RecordAction(
        base::UserMetricsAction(kMultiWindowResizerShowTwoWindowsSnapped));
    base::UmaHistogramBoolean(
        kMultiWindowResizerShowTwoWindowsSnappedHistogramName, true);
  }

  CreateMouseWatcher();
}

bool MultiWindowResizeController::IsShowing() const {
  return resize_widget_.get() || show_timer_.IsRunning();
}

void MultiWindowResizeController::Hide() {
  // Ignore `Hide` while actively resizing.
  if (window_resizer_) {
    return;
  }

  if (windows_.window1) {
    StopObserving(windows_.window1);
    windows_.window1 = nullptr;
  }
  if (windows_.window2) {
    StopObserving(windows_.window2);
    windows_.window2 = nullptr;
  }

  show_timer_.Stop();

  if (!resize_widget_) {
    return;
  }

  for (aura::Window* window : windows_.other_windows) {
    StopObserving(window);
  }

  mouse_watcher_.reset();
  resize_widget_.reset();
  windows_ = ResizeWindows();
}

void MultiWindowResizeController::ResetResizer() {
  // Have to explicitly reset the WindowResizer, otherwise Hide() does nothing.
  window_resizer_.reset();
  Hide();
}

void MultiWindowResizeController::StartResize(
    const gfx::PointF& location_in_screen) {
  DCHECK(!window_resizer_.get());
  DCHECK(windows_.is_valid());
  gfx::PointF location_in_parent =
      ConvertPointFromScreen(windows_.window2->parent(), location_in_screen);
  aura::Window::Windows windows;
  windows.push_back(windows_.window2.get());
  DCHECK(windows_.other_windows.empty());
  FindWindowsTouching(windows_.window2, windows_.direction,
                      &windows_.other_windows);

  for (aura::Window* other_window : windows_.other_windows) {
    StartObserving(other_window);
    windows.push_back(other_window);
  }

  int component =
      windows_.direction == Direction::kLeftRight ? HTRIGHT : HTBOTTOM;
  WindowState* window_state = WindowState::Get(windows_.window1);
  window_state->CreateDragDetails(location_in_parent, component,
                                  ::wm::WINDOW_MOVE_SOURCE_MOUSE);
  window_resizer_ = WorkspaceWindowResizer::Create(window_state, windows);

  // Do not hide the resize widget while a drag is active.
  mouse_watcher_.reset();
  base::RecordAction(base::UserMetricsAction(kMultiWindowResizerClick));
  base::UmaHistogramBoolean(kMultiWindowResizerClickHistogramName, true);

  if (WindowState::Get(windows_.window1)->IsSnapped() &&
      WindowState::Get(windows_.window2)->IsSnapped()) {
    base::RecordAction(
        base::UserMetricsAction(kMultiWindowResizerClickTwoWindowsSnapped));
    base::UmaHistogramBoolean(
        kMultiWindowResizerClickTwoWindowsSnappedHistogramName, true);
  }
}

void MultiWindowResizeController::Resize(const gfx::PointF& location_in_screen,
                                         int event_flags) {
  gfx::PointF location_in_parent =
      ConvertPointFromScreen(windows_.window1->parent(), location_in_screen);
  window_resizer_->Drag(location_in_parent, event_flags);
  gfx::Rect bounds =
      ConvertRectToScreen(windows_.window1->parent(),
                          CalculateResizeWidgetBounds(location_in_parent));

  if (windows_.direction == Direction::kLeftRight) {
    bounds.set_y(resize_widget_show_bounds_in_screen_.y());
  } else {
    bounds.set_x(resize_widget_show_bounds_in_screen_.x());
  }

  resize_widget_->SetBounds(bounds);
}

void MultiWindowResizeController::CompleteResize() {
  window_resizer_->CompleteDrag();
  WindowState::Get(window_resizer_->GetTarget())->DeleteDragDetails();
  window_resizer_.reset();

  // Mouse may still be over resizer, if not hide.
  gfx::Point screen_loc = display::Screen::GetScreen()->GetCursorScreenPoint();
  if (!resize_widget_->GetWindowBoundsInScreen().Contains(screen_loc)) {
    Hide();
  } else {
    // If the mouse is over the resizer we need to remove observers on any of
    // the `other_windows`. If we start another resize we'll recalculate the
    // `other_windows` and invoke AddObserver() as necessary.
    for (aura::Window* other_window : windows_.other_windows) {
      StopObserving(other_window);
    }

    windows_.other_windows.clear();
    CreateMouseWatcher();
  }
}

void MultiWindowResizeController::CancelResize() {
  // Happens if window was destroyed and we nuked the WindowResizer.
  if (!window_resizer_) {
    return;
  }

  window_resizer_->RevertDrag();
  WindowState::Get(window_resizer_->GetTarget())->DeleteDragDetails();
  ResetResizer();
}

gfx::Rect MultiWindowResizeController::CalculateResizeWidgetBounds(
    const gfx::PointF& location_in_parent) const {
  gfx::Size pref = resize_widget_->GetContentsView()->GetPreferredSize();
  int x = 0, y = 0;
  if (windows_.direction == Direction::kLeftRight) {
    x = windows_.window1->bounds().right() - pref.width() / 2;
    y = location_in_parent.y() + kResizeWidgetPadding;
    if (y + pref.height() / 2 > windows_.window1->bounds().bottom() &&
        y + pref.height() / 2 > windows_.window2->bounds().bottom()) {
      y = location_in_parent.y() - kResizeWidgetPadding - pref.height();
    }
  } else {
    x = location_in_parent.x() + kResizeWidgetPadding;
    if (x + pref.height() / 2 > windows_.window1->bounds().right() &&
        x + pref.height() / 2 > windows_.window2->bounds().right()) {
      x = location_in_parent.x() - kResizeWidgetPadding - pref.width();
    }
    y = windows_.window1->bounds().bottom() - pref.height() / 2;
  }
  return gfx::Rect(x, y, pref.width(), pref.height());
}

bool MultiWindowResizeController::IsOverResizeWidget(
    const gfx::Point& location_in_screen) const {
  return resize_widget_->GetWindowBoundsInScreen().Contains(location_in_screen);
}

bool MultiWindowResizeController::IsOverWindows(
    const gfx::Point& location_in_screen) const {
  if (IsOverResizeWidget(location_in_screen)) {
    return true;
  }

  if (windows_.direction == Direction::kTopBottom) {
    if (!ContainsScreenX(windows_.window1, location_in_screen.x()) ||
        !ContainsScreenX(windows_.window2, location_in_screen.x())) {
      return false;
    }
  } else {
    if (!ContainsScreenY(windows_.window1, location_in_screen.y()) ||
        !ContainsScreenY(windows_.window2, location_in_screen.y())) {
      return false;
    }
  }

  // Check whether `location_in_screen` is in the event target's resize region.
  // This is tricky because a window's resize region can extend outside a
  // window's bounds.
  aura::Window* target = RootWindowController::ForWindow(windows_.window1)
                             ->FindEventTarget(location_in_screen);
  if (target == windows_.window1) {
    return IsOverComponent(
        windows_.window1, location_in_screen,
        windows_.direction == Direction::kTopBottom ? HTBOTTOM : HTRIGHT);
  }
  if (target == windows_.window2) {
    return IsOverComponent(
        windows_.window2, location_in_screen,
        windows_.direction == Direction::kTopBottom ? HTTOP : HTLEFT);
  }
  return false;
}

bool MultiWindowResizeController::IsOverComponent(
    aura::Window* window,
    const gfx::Point& location_in_screen,
    int component) const {
  gfx::Point window_loc(location_in_screen);
  ::wm::ConvertPointFromScreen(window, &window_loc);
  return window_util::GetNonClientComponent(window, window_loc) == component;
}

}  // namespace ash

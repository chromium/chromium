// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/overview/rounded_rect_view.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/overview/window_selector_delegate.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/switchable_windows.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// The amount of padding surrounding the text in the text filtering textbox.
constexpr int kTextFilterHorizontalPadding = 6;

// The height of the text filtering textbox.
constexpr int kTextFilterHeight = 32;

// The margin at the bottom to make sure the text filter layer is hidden.
// This is needed because positioning the text filter directly touching the top
// edge of the screen still allows the shadow to peek through.
constexpr int kTextFieldBottomMargin = 2;

// Distance from top of overview to the top of text filtering textbox as a
// proportion of the total overview area.
constexpr float kTextFilterTopScreenProportion = 0.02f;

// Width of the text filter area.
constexpr int kTextFilterWidth = 280;

// The font delta used for text filtering textbox.
constexpr int kTextFilterFontDelta = 1;

// The color of the text and its background in the text filtering textbox.
constexpr SkColor kTextFilterTextColor = SkColorSetARGB(0xFF, 0x3C, 0x40, 0x43);
constexpr SkColor kTextFilterBackgroundColor = SK_ColorWHITE;

// The color or search icon.
constexpr SkColor kTextFilterIconColor = SkColorSetARGB(138, 0, 0, 0);

// The size of search icon.
constexpr int kTextFilterIconSize = 20;

// The radius used for the rounded corners on the text filtering textbox.
constexpr int kTextFilterCornerRadius = 16;

// A comparator for locating a selector item for a given root.
struct WindowSelectorItemForRoot {
  explicit WindowSelectorItemForRoot(const aura::Window* root)
      : root_window(root) {}

  bool operator()(WindowSelectorItem* item) const {
    return item->root_window() == root_window;
  }

  const aura::Window* root_window;
};

// Triggers a shelf visibility update on all root window controllers.
void UpdateShelfVisibility() {
  for (aura::Window* root : Shell::GetAllRootWindows())
    Shelf::ForWindow(root)->UpdateVisibilityState();
}

// Returns the bounds for the overview window grid according to the split view
// state. If split view mode is active, the overview window should open on the
// opposite side of the default snap window. If |divider_changed| is true, maybe
// clamp the bounds to a minimum size and shift the bounds offscreen.
gfx::Rect GetGridBoundsInScreen(aura::Window* root_window,
                                bool divider_changed) {
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  const gfx::Rect work_area =
      split_view_controller->GetDisplayWorkAreaBoundsInScreen(root_window);
  if (!split_view_controller->IsSplitViewModeActive())
    return work_area;

  SplitViewController::SnapPosition opposite_position =
      (split_view_controller->default_snap_position() ==
       SplitViewController::LEFT)
          ? SplitViewController::RIGHT
          : SplitViewController::LEFT;
  gfx::Rect bounds = split_view_controller->GetSnappedWindowBoundsInScreen(
      root_window, opposite_position);
  if (!divider_changed)
    return bounds;

  const bool landscape =
      split_view_controller->IsCurrentScreenOrientationLandscape();
  const int min_length =
      (landscape ? work_area.width() : work_area.height()) / 3;
  if ((landscape ? bounds.width() : bounds.height()) > min_length)
    return bounds;

  // Helper function which shifts and clamps |out_bounds|. Handles the
  // orientation and whether |opposite_position| is physically on the left
  // or top of the screen.
  auto shift_bounds = [&min_length, &landscape](bool left_or_top,
                                                gfx::Rect* out_bounds) {
    // If we are shifting to the left or top we need to update the origin as
    // well.
    if (left_or_top) {
      if (landscape) {
        out_bounds->set_x(out_bounds->x() - (min_length - out_bounds->width()));
      } else {
        out_bounds->set_y(out_bounds->y() -
                          (min_length - out_bounds->height()));
      }
    }

    if (landscape)
      out_bounds->set_width(min_length);
    else
      out_bounds->set_height(min_length);
  };

  // Shift the opposite direction when |primary| is false because the physical
  // location will not be aligned with |opposite_position|.
  const bool primary =
      split_view_controller->IsCurrentScreenOrientationPrimary();
  if (opposite_position == SplitViewController::LEFT)
    shift_bounds(primary, &bounds);
  else
    shift_bounds(!primary, &bounds);

  return bounds;
}

gfx::Rect GetTextFilterPosition(aura::Window* root_window) {
  const gfx::Rect total_bounds =
      GetGridBoundsInScreen(root_window, /*divider_changed=*/false);
  return gfx::Rect(
      total_bounds.x() +
          0.5 * (total_bounds.width() -
                 std::min(kTextFilterWidth, total_bounds.width())),
      total_bounds.y() + total_bounds.height() * kTextFilterTopScreenProportion,
      std::min(kTextFilterWidth, total_bounds.width()), kTextFilterHeight);
}

// Initializes the text filter on the top of the main root window and requests
// focus on its textfield. Uses |image| to place an icon to the left of the text
// field.
views::Widget* CreateTextFilter(views::TextfieldController* controller,
                                aura::Window* root_window,
                                const gfx::ImageSkia& image,
                                int* text_filter_bottom) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = true;
  params.bounds = GetTextFilterPosition(root_window);
  params.name = "OverviewModeTextFilter";
  *text_filter_bottom = params.bounds.bottom() + kTextFieldBottomMargin;
  params.parent = root_window->GetChildById(kShellWindowId_StatusContainer);
  widget->Init(params);

  // Use |container| to specify the padding surrounding the text and to give
  // the textfield rounded corners.
  views::View* container =
      new RoundedRectView(kTextFilterCornerRadius, kTextFilterBackgroundColor);
  const gfx::FontList& font_list =
      views::Textfield::GetDefaultFontList().Derive(
          kTextFilterFontDelta, gfx::Font::FontStyle::NORMAL,
          gfx::Font::Weight::NORMAL);
  const int text_height = std::max(kTextFilterIconSize, font_list.GetHeight());
  DCHECK(text_height);
  const int vertical_padding = (params.bounds.height() - text_height) / 2;
  auto* layout = container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal,
      gfx::Insets(vertical_padding, kTextFilterHorizontalPadding,
                  vertical_padding, kTextFilterCornerRadius),
      kTextFilterHorizontalPadding));

  views::Textfield* textfield = new views::Textfield();
  textfield->set_controller(controller);
  textfield->SetBorder(views::NullBorder());
  textfield->SetBackgroundColor(kTextFilterBackgroundColor);
  textfield->SetTextColor(kTextFilterTextColor);
  textfield->SetFontList(font_list);
  textfield->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_ASH_WINDOW_SELECTOR_INPUT_FILTER_ACCESSIBLE_NAME));

  views::ImageView* image_view = new views::ImageView();
  image_view->SetImage(image);

  container->AddChildView(image_view);
  container->AddChildView(textfield);
  layout->SetFlexForView(textfield, 1);
  widget->SetContentsView(container);

  // The textfield initially contains no text, so shift its position to be
  // outside the visible bounds of the screen.
  gfx::Transform transform;
  transform.Translate(0, -(*text_filter_bottom));
  aura::Window* text_filter_widget_window = widget->GetNativeWindow();
  text_filter_widget_window->layer()->SetOpacity(0);
  text_filter_widget_window->SetTransform(transform);
  widget->Show();
  textfield->RequestFocus();

  return widget;
}

// Gets the window that's currently being dragged in |root_window|. Returns
// nullptr if there is no such window.
aura::Window* GetDraggedWindow(
    const aura::Window* root_window,
    const std::vector<aura::Window*>& mru_window_list) {
  for (auto* window : mru_window_list) {
    if (wm::GetWindowState(window)->is_dragged() &&
        window->GetRootWindow() == root_window) {
      return window;
    }
  }
  return nullptr;
}

}  // namespace

// static
bool WindowSelector::IsSelectable(const aura::Window* window) {
  auto* window_state = wm::GetWindowState(window);
  return window_state->IsUserPositionable() && !window_state->IsPip();
}

WindowSelector::WindowSelector(WindowSelectorDelegate* delegate)
    : delegate_(delegate),
      restore_focus_window_(wm::GetFocusedWindow()),
      overview_start_time_(base::Time::Now()) {
  DCHECK(delegate_);
}

WindowSelector::~WindowSelector() {
  DCHECK(observed_windows_.empty());
  // Don't delete |window_drag_controller_| yet since the stack might be still
  // using it.
  if (window_drag_controller_) {
    window_drag_controller_->ResetWindowSelector();
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
        FROM_HERE, window_drag_controller_.release());
  }
}

// NOTE: The work done in Init() is not done in the constructor because it may
// cause other, unrelated classes, to make indirect method calls on a partially
// constructed object.
void WindowSelector::Init(const WindowList& windows,
                          const WindowList& hide_windows) {
  hide_overview_windows_ =
      std::make_unique<ScopedHideOverviewWindows>(std::move(hide_windows));
  if (restore_focus_window_)
    restore_focus_window_->AddObserver(this);

  if (SplitViewController::ShouldAllowSplitView())
    split_view_drag_indicators_ = std::make_unique<SplitViewDragIndicators>();

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  std::sort(root_windows.begin(), root_windows.end(),
            [](const aura::Window* a, const aura::Window* b) {
              // Since we don't know if windows are vertically or horizontally
              // oriented we use both x and y position. This may be confusing
              // if you have 3 or more monitors which are not strictly
              // horizontal or vertical but that case is not yet supported.
              return (a->GetBoundsInScreen().x() + a->GetBoundsInScreen().y()) <
                     (b->GetBoundsInScreen().x() + b->GetBoundsInScreen().y());
            });

  for (auto* root : root_windows) {
    // Observed switchable containers for newly created windows on all root
    // windows.
    for (size_t i = 0; i < wm::kSwitchableWindowContainerIdsLength; ++i) {
      aura::Window* container =
          root->GetChildById(wm::kSwitchableWindowContainerIds[i]);
      container->AddObserver(this);
      observed_windows_.insert(container);
    }

    std::unique_ptr<WindowGrid> grid(
        new WindowGrid(root, windows, this,
                       GetGridBoundsInScreen(root, /*divider_changed=*/false)));
    num_items_ += grid->size();
    grid_list_.push_back(std::move(grid));
  }

  {
    // The calls to WindowGrid::PrepareForOverview() and CreateTextFilter(...)
    // requires some LayoutManagers to perform layouts so that windows are
    // correctly visible and properly animated in overview mode. Otherwise
    // these layouts should be suppressed during overview mode so they don't
    // conflict with overview mode animations.

    WindowList mru_window_list =
        Shell::Get()->mru_window_tracker()->BuildMruWindowList();

    // Do not call PrepareForOverview until all items are added to window_list_
    // as we don't want to cause any window updates until all windows in
    // overview are observed. See http://crbug.com/384495.
    for (std::unique_ptr<WindowGrid>& window_grid : grid_list_) {
      window_grid->PrepareForOverview();

      // Do not animate if there is any window that is being dragged in the
      // grid.
      if (enter_exit_overview_type_ == EnterExitOverviewType::kWindowDragged) {
        if (!mru_window_list.empty())
          DCHECK(GetDraggedWindow(window_grid->root_window(), mru_window_list));
        window_grid->PositionWindows(/*animate=*/false);
      } else if (enter_exit_overview_type_ ==
                 EnterExitOverviewType::kWindowsMinimized) {
        window_grid->PositionWindows(/*animate=*/false);
        window_grid->SlideWindowsIn();
      } else {
        // EnterExitOverviewType::kSwipeFromShelf is an exit only type, so it
        // should not appear here.
        DCHECK_NE(enter_exit_overview_type_,
                  EnterExitOverviewType::kSwipeFromShelf);
        window_grid->CalculateWindowListAnimationStates(
            /*selected_item=*/nullptr, OverviewTransition::kEnter);
        window_grid->PositionWindows(/*animate=*/true, /*ignore_item=*/nullptr,
                                     OverviewTransition::kEnter);
      }
    }

    // Image used for text filter textfield.
    gfx::ImageSkia search_image =
        gfx::CreateVectorIcon(kOverviewTextFilterSearchIcon,
                              kTextFilterIconSize, kTextFilterIconColor);

    aura::Window* root_window = Shell::GetPrimaryRootWindow();
    text_filter_widget_.reset(CreateTextFilter(this, root_window, search_image,
                                               &text_filter_bottom_));
  }

  UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.Items", num_items_);

  Shell::Get()->activation_client()->AddObserver(this);
  Shell::Get()->split_view_controller()->AddObserver(this);

  display::Screen::GetScreen()->AddObserver(this);
  base::RecordAction(base::UserMetricsAction("WindowSelector_Overview"));
  // Send an a11y alert.
  Shell::Get()->accessibility_controller()->TriggerAccessibilityAlert(
      mojom::AccessibilityAlert::WINDOW_OVERVIEW_MODE_ENTERED);

  UpdateShelfVisibility();
}

// NOTE: The work done in Shutdown() is not done in the destructor because it
// may cause other, unrelated classes, to make indirect calls to
// restoring_minimized_windows() on a partially destructed object.
void WindowSelector::Shutdown() {
  // Stop observing screen metrics changes first to avoid auto-positioning
  // windows in response to work area changes from window activation.
  display::Screen::GetScreen()->RemoveObserver(this);

  // Stop observing split view state changes before restoring window focus.
  // Otherwise the activation of the window triggers OnSplitViewStateChanged()
  // that will call into this function again.
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  split_view_controller->RemoveObserver(this);

  size_t remaining_items = 0;
  for (std::unique_ptr<WindowGrid>& window_grid : grid_list_) {
    // During shutdown, do not animate all windows in overview if we need to
    // animate the snapped window.
    if (window_grid->should_animate_when_exiting()) {
      window_grid->CalculateWindowListAnimationStates(
          selected_item_ && selected_item_->window_grid() == window_grid.get()
              ? selected_item_
              : nullptr,
          OverviewTransition::kExit);
    }
    for (const auto& window_selector_item : window_grid->window_list())
      window_selector_item->RestoreWindow(/*reset_transform=*/true);
    remaining_items += window_grid->size();
  }

  // Setting focus after restoring windows' state avoids unnecessary animations.
  // No need to restore if we are sliding to the home launcher screen, as all
  // windows will be minimized.
  ResetFocusRestoreWindow(enter_exit_overview_type_ ==
                          EnterExitOverviewType::kNormal);
  RemoveAllObservers();

  for (std::unique_ptr<WindowGrid>& window_grid : grid_list_)
    window_grid->Shutdown();

  DCHECK(num_items_ >= remaining_items);
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.OverviewClosedItems",
                           num_items_ - remaining_items);
  UMA_HISTOGRAM_MEDIUM_TIMES("Ash.WindowSelector.TimeInOverview",
                             base::Time::Now() - overview_start_time_);

  // Record metrics related to text filtering.
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.TextFilteringStringLength",
                           text_filter_string_length_);
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.TextFilteringTextfieldCleared",
                           num_times_textfield_cleared_);
  if (text_filter_string_length_) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Ash.WindowSelector.TimeInOverviewWithTextFiltering",
        base::Time::Now() - overview_start_time_);
    UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.ItemsWhenTextFilteringUsed",
                             remaining_items);
  }

  // Clearing the window list resets the ignored_by_shelf flag on the windows.
  grid_list_.clear();
  UpdateShelfVisibility();
}

void WindowSelector::CancelSelection() {
  delegate_->OnSelectionEnded();
}

void WindowSelector::OnGridEmpty(WindowGrid* grid) {
  // TODO(crbug.com/881089): Speculative fix based on the crash stack, needs
  // confirming.
  if (IsShuttingDown())
    return;

  size_t index = 0;
  // If there are no longer any items on any of the grids, shutdown,
  // otherwise the empty grids will remain blurred but will have no items.
  if (IsEmpty()) {
    // Shutdown all grids if no grids have any items and split view mode is
    // not active. Set |index| to -1 so that it does not attempt to select any
    // items.
    index = -1;
    if (!Shell::Get()->IsSplitViewModeActive()) {
      for (const auto& grid : grid_list_)
        grid->Shutdown();
      grid_list_.clear();
    }
  } else {
    for (auto iter = grid_list_.begin(); iter != grid_list_.end(); ++iter) {
      if (grid == (*iter).get()) {
        index = iter - grid_list_.begin();
        break;
      }
    }
  }
  if (index > 0 && selected_grid_index_ >= index) {
    // If the grid closed is not the one with the selected item, we do not need
    // to move the selected item.
    if (selected_grid_index_ == index)
      --selected_grid_index_;
    // If the grid which became empty was the one with the selected window, we
    // need to select a window on the newly selected grid.
    if (selected_grid_index_ == index - 1)
      Move(LEFT, true);
  }
  if (grid_list_.empty())
    CancelSelection();
  else
    PositionWindows(/*animate=*/false);
}

void WindowSelector::IncrementSelection(int increment) {
  const Direction direction =
      increment > 0 ? WindowSelector::RIGHT : WindowSelector::LEFT;
  for (int step = 0; step < abs(increment); ++step)
    Move(direction, true);
}

bool WindowSelector::AcceptSelection() {
  if (!grid_list_[selected_grid_index_]->is_selecting())
    return false;
  SelectWindow(grid_list_[selected_grid_index_]->SelectedWindow());
  return true;
}

void WindowSelector::SelectWindow(WindowSelectorItem* item) {
  aura::Window* window = item->GetWindow();
  aura::Window::Windows window_list =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList();
  if (!window_list.empty()) {
    // Record WindowSelector_ActiveWindowChanged if the user is selecting a
    // window other than the window that was active prior to entering overview
    // mode (i.e., the window at the front of the MRU list).
    if (window_list[0] != window) {
      base::RecordAction(
          base::UserMetricsAction("WindowSelector_ActiveWindowChanged"));
      Shell::Get()->metrics()->task_switch_metrics_recorder().OnTaskSwitch(
          TaskSwitchSource::OVERVIEW_MODE);
    }
    const auto it = std::find(window_list.begin(), window_list.end(), window);
    if (it != window_list.end()) {
      // Record 1-based index so that selecting a top MRU window will record 1.
      UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.SelectionDepth",
                               1 + it - window_list.begin());
    }
  }
  item->EnsureVisible();
  wm::GetWindowState(window)->Activate();
}

void WindowSelector::SetBoundsForWindowGridsInScreenIgnoringWindow(
    const gfx::Rect& bounds,
    WindowSelectorItem* ignored_item) {
  for (std::unique_ptr<WindowGrid>& grid : grid_list_)
    grid->SetBoundsAndUpdatePositionsIgnoringWindow(bounds, ignored_item);
}

void WindowSelector::SetSplitViewDragIndicatorsIndicatorState(
    IndicatorState indicator_state,
    const gfx::Point& event_location) {
  DCHECK(split_view_drag_indicators_);
  split_view_drag_indicators_->SetIndicatorState(indicator_state,
                                                 event_location);
}

WindowGrid* WindowSelector::GetGridWithRootWindow(aura::Window* root_window) {
  for (std::unique_ptr<WindowGrid>& grid : grid_list_) {
    if (grid->root_window() == root_window)
      return grid.get();
  }

  return nullptr;
}

void WindowSelector::AddItem(aura::Window* window,
                             bool reposition,
                             bool animate) {
  // Early exit if a grid already contains |window|.
  WindowGrid* grid = GetGridWithRootWindow(window->GetRootWindow());
  if (!grid || grid->GetWindowSelectorItemContaining(window))
    return;

  grid->AddItem(window, reposition, animate);
  ++num_items_;

  // Transfer focus from |window| to the text widget, to match the behavior of
  // entering overview mode in the beginning.
  wm::ActivateWindow(GetTextFilterWidgetWindow());
}

void WindowSelector::RemoveWindowSelectorItem(WindowSelectorItem* item,
                                              bool reposition) {
  if (item->GetWindow()->HasObserver(this)) {
    item->GetWindow()->RemoveObserver(this);
    observed_windows_.erase(item->GetWindow());
    if (item->GetWindow() == restore_focus_window_)
      restore_focus_window_ = nullptr;
  }

  // Remove |item| from the corresponding grid.
  for (std::unique_ptr<WindowGrid>& grid : grid_list_) {
    if (grid->GetWindowSelectorItemContaining(item->GetWindow())) {
      grid->RemoveItem(item, reposition);
      --num_items_;
      break;
    }
  }
}

void WindowSelector::InitiateDrag(WindowSelectorItem* item,
                                  const gfx::Point& location_in_screen) {
  window_drag_controller_.reset(new OverviewWindowDragController(this));
  window_drag_controller_->InitiateDrag(item, location_in_screen);

  for (std::unique_ptr<WindowGrid>& grid : grid_list_)
    grid->OnSelectorItemDragStarted(item);
}

void WindowSelector::Drag(WindowSelectorItem* item,
                          const gfx::Point& location_in_screen) {
  DCHECK(window_drag_controller_);
  DCHECK_EQ(item, window_drag_controller_->item());
  window_drag_controller_->Drag(location_in_screen);
}

void WindowSelector::CompleteDrag(WindowSelectorItem* item,
                                  const gfx::Point& location_in_screen) {
  DCHECK(window_drag_controller_);
  DCHECK_EQ(item, window_drag_controller_->item());
  window_drag_controller_->CompleteDrag(location_in_screen);

  for (std::unique_ptr<WindowGrid>& grid : grid_list_)
    grid->OnSelectorItemDragEnded();
}

void WindowSelector::StartSplitViewDragMode(
    const gfx::Point& location_in_screen) {
  window_drag_controller_->StartSplitViewDragMode(location_in_screen);
}

void WindowSelector::Fling(WindowSelectorItem* item,
                           const gfx::Point& location_in_screen,
                           float velocity_x,
                           float velocity_y) {
  // Its possible a fling event is not paired with a tap down event. Ignore
  // these flings.
  if (!window_drag_controller_ || item != window_drag_controller_->item())
    return;

  window_drag_controller_->Fling(location_in_screen, velocity_x, velocity_y);
  for (std::unique_ptr<WindowGrid>& grid : grid_list_)
    grid->OnSelectorItemDragEnded();
}

void WindowSelector::ActivateDraggedWindow() {
  window_drag_controller_->ActivateDraggedWindow();
}

void WindowSelector::ResetDraggedWindowGesture() {
  window_drag_controller_->ResetGesture();
}

void WindowSelector::OnWindowDragStarted(aura::Window* dragged_window,
                                         bool animate) {
  WindowGrid* target_grid =
      GetGridWithRootWindow(dragged_window->GetRootWindow());
  if (!target_grid)
    return;
  target_grid->OnWindowDragStarted(dragged_window, animate);
}

void WindowSelector::OnWindowDragContinued(aura::Window* dragged_window,
                                           const gfx::Point& location_in_screen,
                                           IndicatorState indicator_state) {
  WindowGrid* target_grid =
      GetGridWithRootWindow(dragged_window->GetRootWindow());
  if (!target_grid)
    return;
  target_grid->OnWindowDragContinued(dragged_window, location_in_screen,
                                     indicator_state);
}
void WindowSelector::OnWindowDragEnded(aura::Window* dragged_window,
                                       const gfx::Point& location_in_screen,
                                       bool should_drop_window_into_overview) {
  WindowGrid* target_grid =
      GetGridWithRootWindow(dragged_window->GetRootWindow());
  if (!target_grid)
    return;
  target_grid->OnWindowDragEnded(dragged_window, location_in_screen,
                                 should_drop_window_into_overview);
}

void WindowSelector::PositionWindows(bool animate,
                                     WindowSelectorItem* ignored_item) {
  for (std::unique_ptr<WindowGrid>& grid : grid_list_)
    grid->PositionWindows(animate, ignored_item);
}

bool WindowSelector::IsShuttingDown() const {
  return Shell::Get()->window_selector_controller()->is_shutting_down();
}

bool WindowSelector::ShouldAnimateWallpaper(aura::Window* root_window) {
  // Find the grid associated with |root_window|.
  WindowGrid* grid = GetGridWithRootWindow(root_window);
  if (!grid)
    return false;

  // If one of the windows covers the workspace, we do not need to animate.
  for (const auto& selector_item : grid->window_list()) {
    if (CanCoverAvailableWorkspace(selector_item->GetWindow()))
      return false;
  }

  return true;
}

bool WindowSelector::IsWindowInOverview(const aura::Window* window) {
  for (const std::unique_ptr<WindowGrid>& grid : grid_list_) {
    if (grid->GetWindowSelectorItemContaining(window))
      return true;
  }
  return false;
}

void WindowSelector::SetWindowListNotAnimatedWhenExiting(
    aura::Window* root_window) {
  // Find the grid accociated with |root_window|.
  WindowGrid* grid = GetGridWithRootWindow(root_window);
  if (grid)
    grid->SetWindowListNotAnimatedWhenExiting();
}

void WindowSelector::UpdateGridAtLocationYPositionAndOpacity(
    int64_t display_id,
    int new_y,
    float opacity,
    const gfx::Rect& work_area,
    UpdateAnimationSettingsCallback callback) {
  WindowGrid* grid = GetGridWithRootWindow(
      ash::Shell::Get()->GetRootWindowForDisplayId(display_id));
  if (grid)
    grid->UpdateYPositionAndOpacity(new_y, opacity, work_area, callback);
}

void WindowSelector::UpdateMaskAndShadow(bool show) {
  for (auto& grid : grid_list_) {
    for (auto& window : grid->window_list())
      window->UpdateMaskAndShadow(show);
  }
}

void WindowSelector::OnDisplayRemoved(const display::Display& display) {
  // TODO(flackr): Keep window selection active on remaining displays.
  CancelSelection();
}

void WindowSelector::OnDisplayMetricsChanged(const display::Display& display,
                                             uint32_t metrics) {
  // For metrics changes that happen when the split view mode is active, the
  // display bounds will be adjusted in OnSplitViewDividerPositionChanged().
  if (Shell::Get()->IsSplitViewModeActive())
    return;
  OnDisplayBoundsChanged();
}

void WindowSelector::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  // Only care about newly added children of |observed_windows_|.
  if (!observed_windows_.count(params.receiver) ||
      !observed_windows_.count(params.new_parent)) {
    return;
  }

  aura::Window* new_window = params.target;
  if (!IsSelectable(new_window))
    return;

  // If the new window is added when splitscreen is active, do nothing.
  // SplitViewController will do the right thing to snap the window or end
  // overview mode.
  if (Shell::Get()->IsSplitViewModeActive() &&
      new_window->GetRootWindow() == Shell::Get()
                                         ->split_view_controller()
                                         ->GetDefaultSnappedWindow()
                                         ->GetRootWindow()) {
    return;
  }

  for (size_t i = 0; i < wm::kSwitchableWindowContainerIdsLength; ++i) {
    if (new_window->parent()->id() == wm::kSwitchableWindowContainerIds[i] &&
        !::wm::GetTransientParent(new_window)) {
      // The new window is in one of the switchable containers, abort overview.
      CancelSelection();
      return;
    }
  }
}

void WindowSelector::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  observed_windows_.erase(window);
  if (window == restore_focus_window_)
    restore_focus_window_ = nullptr;
}

void WindowSelector::OnWindowActivated(ActivationReason reason,
                                       aura::Window* gained_active,
                                       aura::Window* lost_active) {
  if (ignore_activations_ || !gained_active ||
      gained_active == GetTextFilterWidgetWindow()) {
    return;
  }

  auto* grid = GetGridWithRootWindow(gained_active->GetRootWindow());
  if (!grid)
    return;
  const auto& windows = grid->window_list();

  auto iter = std::find_if(
      windows.begin(), windows.end(),
      [gained_active](const std::unique_ptr<WindowSelectorItem>& window) {
        return window->Contains(gained_active);
      });

  if (iter == windows.end() && showing_text_filter_ &&
      lost_active == GetTextFilterWidgetWindow()) {
    return;
  }

  // Do not cancel overview mode if the window activation was caused by
  // snapping window to one side of the screen.
  if (Shell::Get()->IsSplitViewModeActive())
    return;

  // Do not cancel overview mode if the window activation was caused while
  // dragging overview mode offscreen.
  if (IsSlidingOutOverviewFromShelf())
    return;

  // Don't restore focus on exit if a window was just activated.
  ResetFocusRestoreWindow(false);
  if (iter != windows.end())
    selected_item_ = iter->get();
  CancelSelection();
}

void WindowSelector::OnAttemptToReactivateWindow(aura::Window* request_active,
                                                 aura::Window* actual_active) {
  OnWindowActivated(ActivationReason::ACTIVATION_CLIENT, request_active,
                    actual_active);
}

void WindowSelector::ContentsChanged(views::Textfield* sender,
                                     const base::string16& new_contents) {
  // If the user enters underline mode via CTRL+SHIFT+U, ContentsChanged
  // will get called after shutdown has started. Prevent anything from
  // happening if shutdown has started (grids have been cleared).
  if (grid_list_.size() < 1)
    return;

  text_filter_string_length_ = new_contents.length();
  if (!text_filter_string_length_)
    num_times_textfield_cleared_++;

  bool should_show_text_filter = !new_contents.empty();
  if (showing_text_filter_ != should_show_text_filter) {
    aura::Window* text_filter_widget_window = GetTextFilterWidgetWindow();
    ui::ScopedLayerAnimationSettings animation_settings(
        text_filter_widget_window->layer()->GetAnimator());
    animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    animation_settings.SetTweenType(showing_text_filter_
                                        ? gfx::Tween::FAST_OUT_LINEAR_IN
                                        : gfx::Tween::LINEAR_OUT_SLOW_IN);

    gfx::Transform transform;
    if (should_show_text_filter) {
      transform.Translate(0, 0);
      text_filter_widget_window->layer()->SetOpacity(1);
    } else {
      transform.Translate(0, -text_filter_bottom_);
      text_filter_widget_window->layer()->SetOpacity(0);
    }

    text_filter_widget_window->SetTransform(transform);
    showing_text_filter_ = should_show_text_filter;
  }
  for (std::unique_ptr<WindowGrid>& grid : grid_list_)
    grid->FilterItems(new_contents);

  // If the selection widget is not active and the filter string is not empty,
  // execute a Move() command so that it shows up on the first undimmed item.
  if (grid_list_[selected_grid_index_]->is_selecting() || new_contents.empty())
    return;
  Move(WindowSelector::RIGHT, false);
}

bool WindowSelector::HandleKeyEvent(views::Textfield* sender,
                                    const ui::KeyEvent& key_event) {
  // Do not do anything with the events if none of the window grids have windows
  // in them.
  if (IsEmpty())
    return true;

  if (key_event.type() != ui::ET_KEY_PRESSED)
    return false;

  switch (key_event.key_code()) {
    case ui::VKEY_BROWSER_BACK:
      FALLTHROUGH;
    case ui::VKEY_ESCAPE:
      CancelSelection();
      break;
    case ui::VKEY_UP:
      num_key_presses_++;
      Move(WindowSelector::UP, true);
      break;
    case ui::VKEY_DOWN:
      num_key_presses_++;
      Move(WindowSelector::DOWN, true);
      break;
    case ui::VKEY_RIGHT:
    case ui::VKEY_TAB:
      if (key_event.key_code() == ui::VKEY_RIGHT ||
          !(key_event.flags() & ui::EF_SHIFT_DOWN)) {
        num_key_presses_++;
        Move(WindowSelector::RIGHT, true);
        break;
      }
      FALLTHROUGH;
    case ui::VKEY_LEFT:
      num_key_presses_++;
      Move(WindowSelector::LEFT, true);
      break;
    case ui::VKEY_W:
      if (!(key_event.flags() & ui::EF_CONTROL_DOWN) ||
          !grid_list_[selected_grid_index_]->is_selecting()) {
        // Allow the textfield to handle 'W' key when not used with Ctrl.
        return false;
      }
      base::RecordAction(
          base::UserMetricsAction("WindowSelector_OverviewCloseKey"));
      grid_list_[selected_grid_index_]->SelectedWindow()->CloseWindow();
      break;
    case ui::VKEY_RETURN:
      // Ignore if no item is selected.
      if (!grid_list_[selected_grid_index_]->is_selecting())
        return false;
      UMA_HISTOGRAM_COUNTS_100("Ash.WindowSelector.ArrowKeyPresses",
                               num_key_presses_);
      UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.WindowSelector.KeyPressesOverItemsRatio",
                                  (num_key_presses_ * 100) / num_items_, 1, 300,
                                  30);
      base::RecordAction(
          base::UserMetricsAction("WindowSelector_OverviewEnterKey"));
      SelectWindow(grid_list_[selected_grid_index_]->SelectedWindow());
      break;
    default:
      // Not a key we are interested in, allow the textfield to handle it.
      return false;
  }
  return true;
}

void WindowSelector::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  const bool unsnappable_window_activated =
      state == SplitViewController::NO_SNAP &&
      Shell::Get()->split_view_controller()->end_reason() ==
          SplitViewController::EndReason::kUnsnappableWindowActivated;

  if (state != SplitViewController::NO_SNAP || unsnappable_window_activated) {
    // Do not restore focus if a window was just snapped and activated or
    // splitview mode is ended by activating an unsnappable window.
    ResetFocusRestoreWindow(false);
  }

  if (state == SplitViewController::BOTH_SNAPPED ||
      unsnappable_window_activated) {
    // If two windows were snapped to both sides of the screen or an unsnappable
    // window was just activated, end overview mode.
    CancelSelection();
  } else {
    // Otherwise adjust the overview window grid bounds if overview mode is
    // active at the moment.
    OnDisplayBoundsChanged();
    for (auto& grid : grid_list_)
      grid->UpdateCannotSnapWarningVisibility();
  }
}

void WindowSelector::OnSplitViewDividerPositionChanged() {
  DCHECK(Shell::Get()->IsSplitViewModeActive());
  // Re-calculate the bounds for the window grids and position all the windows.
  for (std::unique_ptr<WindowGrid>& grid : grid_list_) {
    grid->SetBoundsAndUpdatePositions(
        GetGridBoundsInScreen(const_cast<aura::Window*>(grid->root_window()),
                              /*divider_changed=*/true));
  }
  PositionWindows(/*animate=*/false);
  RepositionTextFilterOnDisplayMetricsChange();
}

aura::Window* WindowSelector::GetTextFilterWidgetWindow() {
  return text_filter_widget_->GetNativeWindow();
}

void WindowSelector::RepositionTextFilterOnDisplayMetricsChange() {
  const gfx::Rect rect = GetTextFilterPosition(Shell::GetPrimaryRootWindow());
  text_filter_bottom_ = rect.bottom() + kTextFieldBottomMargin;
  text_filter_widget_->SetBounds(rect);

  gfx::Transform transform;
  transform.Translate(
      0, text_filter_string_length_ == 0 ? -text_filter_bottom_ : 0);
  aura::Window* text_filter_window = GetTextFilterWidgetWindow();
  text_filter_window->layer()->SetOpacity(text_filter_string_length_ == 0 ? 0
                                                                          : 1);
  text_filter_window->SetTransform(transform);
}

void WindowSelector::ResetFocusRestoreWindow(bool focus) {
  if (!restore_focus_window_)
    return;
  if (focus) {
    base::AutoReset<bool> restoring_focus(&ignore_activations_, true);
    wm::ActivateWindow(restore_focus_window_);
  }
  // If the window is in the observed_windows_ list it needs to continue to be
  // observed.
  if (observed_windows_.find(restore_focus_window_) ==
      observed_windows_.end()) {
    restore_focus_window_->RemoveObserver(this);
  }
  restore_focus_window_ = nullptr;
}

void WindowSelector::Move(Direction direction, bool animate) {
  // Direction to move if moving past the end of a display.
  int display_direction = (direction == RIGHT || direction == DOWN) ? 1 : -1;

  // If this is the first move and it's going backwards, start on the last
  // display.
  if (display_direction == -1 && !grid_list_.empty() &&
      !grid_list_[selected_grid_index_]->is_selecting()) {
    selected_grid_index_ = grid_list_.size() - 1;
  }

  // Keep calling Move() on the grids until one of them reports no overflow or
  // we made a full cycle on all the grids.
  for (size_t i = 0; i <= grid_list_.size() &&
                     grid_list_[selected_grid_index_]->Move(direction, animate);
       i++) {
    selected_grid_index_ =
        (selected_grid_index_ + display_direction + grid_list_.size()) %
        grid_list_.size();
  }
}

void WindowSelector::RemoveAllObservers() {
  for (auto* window : observed_windows_)
    window->RemoveObserver(this);
  observed_windows_.clear();

  Shell::Get()->activation_client()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
  if (restore_focus_window_)
    restore_focus_window_->RemoveObserver(this);
}

void WindowSelector::OnDisplayBoundsChanged() {
  // Re-calculate the bounds for the window grids and position all the windows.
  for (std::unique_ptr<WindowGrid>& grid : grid_list_) {
    grid->SetBoundsAndUpdatePositions(
        GetGridBoundsInScreen(const_cast<aura::Window*>(grid->root_window()),
                              /*divider_changed=*/false));
  }
  PositionWindows(/*animate=*/false);
  RepositionTextFilterOnDisplayMetricsChange();
  if (split_view_drag_indicators_)
    split_view_drag_indicators_->OnDisplayBoundsChanged();
}

bool WindowSelector::IsEmpty() {
  for (const auto& grid : grid_list_) {
    if (!grid->empty())
      return false;
  }
  return true;
}

}  // namespace ash

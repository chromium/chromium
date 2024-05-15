// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_focus_cycler.h"

#include "ash/shell.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

views::View* GetFirstOrLastFocusableView(views::Widget* widget, bool reverse) {
  views::View* view = widget->GetFocusManager()->GetNextFocusableView(
      /*starting_view=*/nullptr, widget, reverse, /*dont_loop=*/false);
  CHECK(view);
  return view;
}

// Determines whether we should rotate focus to the next widget. We rotate focus
// if we are forward tabbing and the current focused view is the last focusable
// view of the widget, or if we are reverse tabbing and the current focused view
// is the first focusable view of the widget.
bool ShouldRotateFocus(views::View* current_focused_view, bool reverse) {
  views::Widget* widget = current_focused_view->GetWidget();
  return current_focused_view == GetFirstOrLastFocusableView(widget, !reverse);
}

int AdvanceIndex(int previous_index, int size, bool reverse) {
  if (reverse) {
    return previous_index == 0 ? (size - 1) : (previous_index - 1);
  }
  return previous_index == (size - 1) ? 0 : (previous_index + 1);
}

}  // namespace

OverviewFocusCycler::OverviewFocusCycler(OverviewSession* overview_session)
    : overview_session_(overview_session) {}

OverviewFocusCycler::~OverviewFocusCycler() = default;

void OverviewFocusCycler::MoveFocus(bool reverse) {
  views::View* focused_view = GetOverviewFocusedView();
  if (focused_view && !ShouldRotateFocus(focused_view, reverse)) {
    // If we don't need to rotate focus to the next widget, let the focus
    // manager advance focus.
    focused_view->GetWidget()->GetFocusManager()->AdvanceFocus(reverse);
    return;
  }

  const std::vector<views::Widget*> widgets = GetTraversableWidgets();
  // `widgets` can be empty when there are only non traversable overview widgets
  // shown (ex. "No recent items" label).
  if (widgets.empty()) {
    return;
  }

  // If there is no current focused view request either the last focusable view
  // of the last widget in the traversal or the first focusable view of the
  // first widget, depending on `reverse`.
  if (!focused_view) {
    views::Widget* widget = reverse ? widgets.back() : widgets.front();
    GetFirstOrLastFocusableView(widget, reverse)->RequestFocus();
    return;
  }

  auto it = base::ranges::find(widgets, focused_view->GetWidget());
  CHECK(it != widgets.end());

  const int previous_index = std::distance(widgets.begin(), it);
  const int size = static_cast<int>(widgets.size());

  // Focus the last focusable view of the previous widget if `reverse`, or the
  // first focusable view of the next widget otherwise.
  const int next_index = AdvanceIndex(previous_index, size, reverse);
  GetFirstOrLastFocusableView(widgets[next_index], reverse)->RequestFocus();
}

views::View* OverviewFocusCycler::GetOverviewFocusedView() {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window) {
    return nullptr;
  }

  if (!active_window->GetProperty(kOverviewUiKey)) {
    return nullptr;
  }

  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(active_window);
  if (!widget) {
    return nullptr;
  }

  return widget->GetFocusManager()->GetFocusedView();
}

std::vector<views::Widget*> OverviewFocusCycler::GetTraversableWidgets() const {
  std::vector<views::Widget*> traversable_widgets;

  auto maybe_add_widget = [&traversable_widgets](views::Widget* widget) {
    if (!widget || !widget->CanActivate()) {
      return;
    }

    // Skip this widget if it has no focusable views. (i.e. Saved desks library
    // with all saved desks deleted.)
    if (!widget->GetFocusManager()->GetNextFocusableView(
            /*starting_view=*/nullptr, widget, /*reverse=*/false,
            /*dont_loop=*/false)) {
      return;
    }

    traversable_widgets.push_back(widget);
  };

  // TODO(http://b/325335020): Handle multidisplay focus.
  OverviewGrid* primary_grid =
      overview_session_->GetGridWithRootWindow(Shell::GetPrimaryRootWindow());
  maybe_add_widget(primary_grid->pine_widget());
  maybe_add_widget(primary_grid->feedback_widget());
  maybe_add_widget(primary_grid->birch_bar_widget());
  maybe_add_widget(primary_grid->saved_desk_library_widget());
  return traversable_widgets;
}

}  // namespace ash

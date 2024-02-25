// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/focus_cycler.h"

#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/ranges/algorithm.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

bool HasFocusableWindow() {
  return !Shell::Get()
              ->mru_window_tracker()
              ->BuildMruWindowList(kActiveDesk)
              .empty();
}

}  // namespace

FocusCycler::FocusCycler() : widget_activating_(nullptr) {}

FocusCycler::~FocusCycler() = default;

void FocusCycler::AddWidget(views::Widget* widget) {
  widgets_.push_back(widget);
}

void FocusCycler::RemoveWidget(views::Widget* widget) {
  auto iter = base::ranges::find(widgets_, widget);
  if (iter != widgets_.end())
    widgets_.erase(iter);
}

void FocusCycler::RotateFocus(Direction direction, bool move_onto_next_widget) {
  if (aura::Window* window = window_util::GetActiveWindow(); window) {
    views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
    // First try to rotate focus within the active widget. If that succeeds,
    // we're done.
    if (widget && !move_onto_next_widget &&
        widget->GetFocusManager()->RotatePaneFocus(
            direction == BACKWARD ? views::FocusManager::Direction::kBackward
                                  : views::FocusManager::Direction::kForward,
            views::FocusManager::FocusCycleWrapping::kDisabled)) {
      return;
    }
  }

  const bool has_window = HasFocusableWindow();
  int index = 0;
  int count = static_cast<int>(widgets_.size());
  int browser_index = has_window ? count : -1;

  for (; index < count; ++index) {
    if (widgets_[index]->IsActive())
      break;
  }

  int start_index = index;

  if (has_window)
    ++count;

  for (;;) {
    if (direction == FORWARD)
      index = (index + 1) % count;
    else
      index = ((index - 1) + count) % count;

    // Ensure that we don't loop more than once.
    if (index == start_index)
      break;

    if (index == browser_index) {
      // Activate the most recently active browser window.
      MruWindowTracker::WindowList mru_windows(
          Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk));
      if (mru_windows.empty())
        break;
      auto* window = mru_windows.front().get();
      WindowState::Get(window)->Activate();
      views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
      if (!widget)
        break;
      views::FocusManager* focus_manager = widget->GetFocusManager();
      focus_manager->ClearFocus();
      focus_manager->RotatePaneFocus(
          direction == BACKWARD ? views::FocusManager::Direction::kBackward
                                : views::FocusManager::Direction::kForward,
          views::FocusManager::FocusCycleWrapping::kEnabled);
      break;
    } else {
      if (FocusWidget(widgets_[index]))
        break;
    }
  }
}

bool FocusCycler::FocusWidget(views::Widget* widget) {
  // If the target is PIP window, temporarily make it activatable.
  if (WindowState::Get(widget->GetNativeWindow())->IsPip())
    widget->widget_delegate()->SetCanActivate(true);

  // Note: It is not necessary to set the focus directly to the pane since that
  // will be taken care of by the widget activation.
  widget_activating_ = widget;
  widget->Activate();
  widget_activating_ = nullptr;
  return widget->IsActive();
}

views::Widget* FocusCycler::FindWidget(
    base::RepeatingCallback<bool(views::Widget*)> callback) {
  for (views::Widget* widget : widgets_) {
    if (callback.Run(widget))
      return widget;
  }
  return nullptr;
}

}  // namespace ash

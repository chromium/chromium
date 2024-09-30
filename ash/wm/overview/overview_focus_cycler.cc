// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_focus_cycler.h"

#include "ash/shell.h"
#include "ash/style/rounded_label_widget.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

namespace {

// Returns true if any child is focusable in `view`'s tree.
bool IsViewFocusable(const views::View* view, bool for_accessibility) {
  CHECK(view);

  // A regular focusable view is focusable for ChromeVox as well.
  if (view->IsFocusable()) {
    return true;
  }

  if (for_accessibility &&
      view->GetViewAccessibility().IsAccessibilityFocusable()) {
    return true;
  }

  // If any of the children are focusable we are done.
  for (const views::View* child : view->children()) {
    if (IsViewFocusable(child, for_accessibility)) {
      return true;
    }
  }

  return false;
}

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

// Class that temporary makes a widget activatable so that we can focus it. This
// is meant to be used while keyboard traversing through overview item widgets.
// These widgets are not activatable normally for both historical reasons, and
// to prevent activation change while mouse dragging.
class ScopedActivatable : public views::WidgetObserver {
 public:
  explicit ScopedActivatable(views::Widget* widget) {
    views::WidgetDelegate* delegate = widget->widget_delegate();
    if (!delegate->CanActivate()) {
      observation_.Observe(widget);
      delegate->SetCanActivate(true);
    }
  }
  ScopedActivatable(const ScopedActivatable&) = delete;
  ScopedActivatable& operator=(const ScopedActivatable&) = delete;
  ~ScopedActivatable() override {
    if (observation_.IsObserving()) {
      observation_.GetSource()->widget_delegate()->SetCanActivate(false);
    }
  }

  void OnWidgetDestroying(views::Widget* widget) override {
    observation_.Reset();
  }

  void OnWidgetActivationChanged(views::Widget* widget, bool active) override {
    if (!active) {
      return;
    }

    // If an overview item received focus, we need to restack the original
    // window above the overview item widget, otherwise the overview backdrop
    // would end up covering the original window.
    auto* item_view =
        views::AsViewClass<OverviewItemView>(widget->GetContentsView());
    if (!item_view) {
      return;
    }

    OverviewItemBase* item = item_view->overview_item();
    if (!item) {
      return;
    }

    aura::Window* parent = widget->GetNativeWindow()->parent();
    if (parent == item->GetWindow()->parent()) {
      parent->StackChildAbove(item->GetWindow(), widget->GetNativeWindow());
    }
  }

 private:
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
};

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

  const std::vector<views::Widget*> widgets =
      GetTraversableWidgets(/*for_accessibility=*/false);
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
    ScopedActivatable scoped_activatable(widget);
    GetFirstOrLastFocusableView(widget, reverse)->RequestFocus();
    return;
  }

  auto it = base::ranges::find(widgets, focused_view->GetWidget());
  CHECK(it != widgets.end());

  const int previous_index = std::distance(widgets.begin(), it);
  const int size = static_cast<int>(widgets.size());

  // Jump to the desk removal toast if it exists. We introduce special logic
  // here since it's not an overview UI.
  if ((reverse && previous_index == 0) ||
      (!reverse && previous_index == size - 1)) {
    const bool ignore_activations = overview_session_->ignore_activations();
    overview_session_->set_ignore_activations(true);
    const bool focused_toast =
        DesksController::Get()->RequestFocusOnUndoDeskRemovalToast();
    overview_session_->set_ignore_activations(ignore_activations);
    if (focused_toast) {
      return;
    }
  }

  // Focus the last focusable view of the previous widget if `reverse`, or the
  // first focusable view of the next widget otherwise.
  const int next_index = AdvanceIndex(previous_index, size, reverse);
  ScopedActivatable scoped_activatable(widgets[next_index]);
  GetFirstOrLastFocusableView(widgets[next_index], reverse)->RequestFocus();
}

bool OverviewFocusCycler::AcceptSelection() {
  views::View* focused_view = GetOverviewFocusedView();
  if (!focused_view) {
    return false;
  }

  if (auto* preview_view = views::AsViewClass<DeskPreviewView>(focused_view)) {
    preview_view->AcceptSelection();
    return true;
  }

  if (auto* item_view = views::AsViewClass<OverviewItemView>(focused_view)) {
    item_view->AcceptSelection(overview_session_);
    return true;
  }

  return false;
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

void OverviewFocusCycler::UpdateAccessibilityFocus() {
  const std::vector<views::Widget*> a11y_widgets =
      GetTraversableWidgets(/*for_accessibility=*/true);
  if (a11y_widgets.empty()) {
    return;
  }

  auto get_view_a11y = [&a11y_widgets](int index) -> views::ViewAccessibility& {
    return a11y_widgets[index]->GetContentsView()->GetViewAccessibility();
  };

  // If there is only one widget left, clear the focus overrides so that they
  // do not point to deleted objects.
  if (a11y_widgets.size() == 1) {
    get_view_a11y(/*index=*/0).SetPreviousFocus(nullptr);
    get_view_a11y(/*index=*/0).SetNextFocus(nullptr);
    a11y_widgets[0]->GetContentsView()->NotifyAccessibilityEvent(
        ax::mojom::Event::kTreeChanged, true);
    return;
  }

  int size = a11y_widgets.size();
  for (int i = 0; i < size; ++i) {
    int previous_index = (i + size - 1) % size;
    int next_index = (i + 1) % size;
    get_view_a11y(i).SetPreviousFocus(a11y_widgets[previous_index]);
    get_view_a11y(i).SetNextFocus(a11y_widgets[next_index]);
    a11y_widgets[i]->GetContentsView()->NotifyAccessibilityEvent(
        ax::mojom::Event::kTreeChanged, true);
  }
}

std::vector<views::Widget*> OverviewFocusCycler::GetTraversableWidgets(
    bool for_accessibility) const {
  std::vector<views::Widget*> traversable_widgets;
  traversable_widgets.reserve(40);  // Conservative default.

  auto maybe_add_widget = [for_accessibility,
                           &traversable_widgets](views::Widget* widget) {
    // The desks bar is invisible when in splitscreen. This is because of the
    // high cost of creating and destroying it.
    if (!widget || !widget->IsVisible() ||
        widget->GetNativeWindow()->layer()->GetTargetOpacity() == 0.f) {
      return;
    }

    // Focus is tied to activation except in ChromeVox where labels and other
    // normally unfocusable elements can be ChromeVox focused.
    if (!for_accessibility && !widget->CanActivate() &&
        !widget->GetNativeWindow()->GetProperty(kIsOverviewItemKey)) {
      return;
    }

    // Skip this widget if it has no focusable views. (i.e. Saved desks library
    // with all saved desks deleted or saved desk button container with all
    // buttons disabled.)
    if (!IsViewFocusable(widget->GetContentsView(), for_accessibility)) {
      return;
    }

    traversable_widgets.push_back(widget);
  };

  maybe_add_widget(overview_session_->overview_focus_widget());

  for (const auto& grid : overview_session_->grid_list()) {
    for (const auto& item : grid->item_list()) {
      // There may be two widgets if the item is a snap group item.
      for (views::Widget* item_widget : item->GetFocusableWidgets()) {
        maybe_add_widget(item_widget);
      }
    }
    maybe_add_widget(grid->saved_desk_library_widget());
    maybe_add_widget(grid->desks_widget());
    maybe_add_widget(grid->save_desk_button_container_widget());
    maybe_add_widget(grid->informed_restore_widget());
    maybe_add_widget(grid->birch_bar_widget());
    maybe_add_widget(grid->split_view_setup_widget());
    maybe_add_widget(grid->no_windows_widget());
  }
  return traversable_widgets;
}

}  // namespace ash

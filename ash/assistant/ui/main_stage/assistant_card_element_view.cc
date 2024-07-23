// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_card_element_view.h"

#include <memory>
#include <tuple>

#include "ash/assistant/model/ui/assistant_card_element.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/main_stage/assistant_ui_element_view_animator.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

using assistant::util::DeepLinkParam;
using assistant::util::DeepLinkType;

constexpr char kAssistantCardElementHistogram[] =
    "Ash.Assistant.AnimationSmoothness.CardElement";

// Helpers ---------------------------------------------------------------------

void CreateAndSendMouseClick(aura::WindowTreeHost* host,
                             const gfx::Point& location_in_pixels) {
  ui::MouseEvent press_event(ui::EventType::kMousePressed, location_in_pixels,
                             location_in_pixels, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);

  // Send an kMousePressed event.
  ui::EventDispatchDetails details =
      host->GetEventSink()->OnEventFromSource(&press_event);

  if (details.dispatcher_destroyed)
    return;

  ui::MouseEvent release_event(ui::EventType::kMouseReleased,
                               location_in_pixels, location_in_pixels,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);

  // Send an EventType::kMouseReleased event.
  std::ignore = host->GetEventSink()->OnEventFromSource(&release_event);
}

}  // namespace

AssistantCardElementView::AssistantCardElementView(
    AssistantViewDelegate* delegate,
    const AssistantCardElement* card_element)
    : delegate_(delegate), card_element_(card_element) {
  InitLayout();

  // We observe contents_view() to receive events pertaining to the underlying
  // WebContents including focus change and suppressed navigation events.
  contents_view_->AddObserver(this);
}

AssistantCardElementView::~AssistantCardElementView() {
  contents_view_->RemoveObserver(this);
}

ui::Layer* AssistantCardElementView::GetLayerForAnimating() {
  // native_view() can be nullptr if this runs in unit test.
  if (!native_view())
    return nullptr;

  return native_view()->layer();
}

std::string AssistantCardElementView::ToStringForTesting() const {
  return card_element_->html();
}

void AssistantCardElementView::AddedToWidget() {
  // native_view() can be nullptr if this runs in unit test.
  if (!native_view())
    return;

  aura::Window* const top_level_window = native_view()->GetToplevelWindow();

  // Find the window for the Assistant card.
  aura::Window* window = native_view();
  while (window->parent() != top_level_window)
    window = window->parent();

  // The Assistant card window will consume all events that enter it. This
  // prevents us from being able to scroll the native view hierarchy
  // vertically. As such, we need to prevent the Assistant card window from
  // receiving events it doesn't need. It needs mouse click events for
  // handling links.
  window->SetProperty(assistant::ui::kOnlyAllowMouseClickEvents, true);
}

void AssistantCardElementView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantCardElementView::OnGestureEvent(ui::GestureEvent* event) {
  // We need to route GESTURE_TAP events to our Assistant card because links
  // should be tappable. The Assistant card window will not receive gesture
  // events so we convert the gesture into analogous mouse events.
  if (event->type() != ui::EventType::kGestureTap) {
    views::View::OnGestureEvent(event);
    return;
  }

  // Consume the original event.
  event->StopPropagation();
  event->SetHandled();

  aura::Window* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();

  // Get the appropriate event location in pixels.
  gfx::Point location_in_pixels = event->location();
  ConvertPointToScreen(this, &location_in_pixels);
  aura::WindowTreeHost* host = root_window->GetHost();
  host->ConvertDIPToPixels(&location_in_pixels);

  wm::CursorManager* cursor_manager = delegate_->GetCursorManager();

  // We want to prevent the cursor from changing its visibility during our
  // mouse events because we are actually handling a gesture. To accomplish
  // this, we cache the cursor's visibility and lock it in its current state.
  const bool visible = cursor_manager->IsCursorVisible();
  cursor_manager->LockCursor();

  CreateAndSendMouseClick(host, location_in_pixels);

  // Restore the original cursor visibility that may have changed during our
  // sequence of mouse events. This change would not have been perceivable to
  // the user since it occurred within our lock.
  if (visible)
    cursor_manager->ShowCursor();
  else
    cursor_manager->HideCursor();

  // Release our cursor lock.
  cursor_manager->UnlockCursor();
}

void AssistantCardElementView::ScrollRectToVisible(const gfx::Rect& rect) {
  // We expect this method is called outside this class to show its contents
  // bounds. Inside this class, should call views::View::ScrollRectToVisible()
  // to show the focused node in the web contents.
  DCHECK(rect == GetContentsBounds());

  // When this view is focused, View::Focus() calls ScrollViewToVisible(), which
  // calls ScrollRectToVisible().  But we don't want that call to do anything,
  // since the true focused item is not this view but a node in the contained
  // web contents.  That will be scrolled into view by FocusedNodeChanged()
  // below, so just no-op here.
  if (focused_node_rect_.IsEmpty())
    return;

  // Make the focused node visible.
  views::View::ScrollRectToVisible(focused_node_rect_);
}

void AssistantCardElementView::DidSuppressNavigation(
    const GURL& url,
    WindowOpenDisposition disposition,
    bool from_user_gesture) {
  // We delegate navigation to the AssistantController so that it can apply
  // special handling to deep links.
  AssistantController::Get()->OpenUrl(url);
}

void AssistantCardElementView::DidChangeFocusedNode(
    const gfx::Rect& node_bounds_in_screen) {
  // TODO(b/143985066): Card has element with empty bounds, e.g. the line break.
  if (node_bounds_in_screen.IsEmpty())
    return;

  gfx::Point origin = node_bounds_in_screen.origin();
  ConvertPointFromScreen(this, &origin);
  focused_node_rect_ = gfx::Rect(origin, node_bounds_in_screen.size());
  views::View::ScrollRectToVisible(focused_node_rect_);
}

void AssistantCardElementView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Contents view.
  contents_view_ =
      AddChildView(const_cast<AssistantCardElement*>(card_element_.get())
                       ->MoveContentsView());
}

std::unique_ptr<ElementAnimator> AssistantCardElementView::CreateAnimator() {
  return std::make_unique<AssistantUiElementViewAnimator>(
      this, kAssistantCardElementHistogram);
}

BEGIN_METADATA(AssistantCardElementView)
END_METADATA

}  // namespace ash

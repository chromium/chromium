// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/legacy_desk_bar_view.h"

#include <iterator>
#include <utility>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/utility/haptics_util.h"
#include "ash/wm/desks/cros_next_desk_icon_button.h"
#include "ash/wm/desks/desk_action_view.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_drag_proxy.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_mini_view_animations.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/scroll_arrow_button.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "ash/wm/desks/templates/saved_desk_presenter.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event_observer.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/event_monitor.h"
#include "ui/views/highlight_border.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

constexpr int kMiniViewsY = 16;

// Spacing between mini views.
constexpr int kMiniViewsSpacing = 12;

// Spacing between zero state default desk button and new desk button.
constexpr int kZeroStateButtonSpacing = 8;

// The local Y coordinate of the zero state desk buttons.
constexpr int kZeroStateY = 6;

constexpr int kDeskIconButtonAndLabelSpacing = 8;

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
  DeskBarHoverObserver(LegacyDeskBarView* owner, aura::Window* widget_window)
      : owner_(owner),
        event_monitor_(views::EventMonitor::CreateWindowMonitor(
            this,
            widget_window,
            {ui::ET_MOUSE_PRESSED, ui::ET_MOUSE_DRAGGED, ui::ET_MOUSE_RELEASED,
             ui::ET_MOUSE_MOVED, ui::ET_MOUSE_ENTERED, ui::ET_MOUSE_EXITED,
             ui::ET_GESTURE_LONG_PRESS, ui::ET_GESTURE_LONG_TAP,
             ui::ET_GESTURE_TAP, ui::ET_GESTURE_TAP_DOWN})) {}

  DeskBarHoverObserver(const DeskBarHoverObserver&) = delete;
  DeskBarHoverObserver& operator=(const DeskBarHoverObserver&) = delete;

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
  raw_ptr<LegacyDeskBarView, ExperimentalAsh> owner_;

  std::unique_ptr<views::EventMonitor> event_monitor_;
};

// -----------------------------------------------------------------------------
// DesksBarScrollViewLayout:

// All the desk bar contents except the background view are added to
// be the children of the `scroll_view_` to support scrollable desk bar.
// `DesksBarScrollViewLayout` will help lay out the contents of the
// `scroll_view_`.
class DesksBarScrollViewLayout : public views::LayoutManager {
 public:
  explicit DesksBarScrollViewLayout(LegacyDeskBarView* bar_view)
      : bar_view_(bar_view) {}
  DesksBarScrollViewLayout(const DesksBarScrollViewLayout&) = delete;
  DesksBarScrollViewLayout& operator=(const DesksBarScrollViewLayout&) = delete;
  ~DesksBarScrollViewLayout() override = default;

  void LayoutInternal(views::View* host) {
    const gfx::Rect scroll_bounds = bar_view_->scroll_view_->bounds();

    // `host` here is `scroll_view_contents_`.
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

      // The presenter is shutdown early in the overview destruction process to
      // prevent calls to the model. Some animations on the desk bar may still
      // call this function past shutdown start. In this case we just continue
      // as if the saved desks UI should be hidden.
      OverviewSession* session = bar_view_->overview_grid()->overview_session();
      const bool should_show_saved_desk_library =
          saved_desk_util::IsSavedDesksEnabled() && session &&
          !session->is_shutting_down() &&
          session->saved_desk_presenter()->should_show_saved_desk_library();
      auto* zero_state_library_button = bar_view_->zero_state_library_button();
      const gfx::Size zero_state_library_button_size =
          should_show_saved_desk_library
              ? zero_state_library_button->GetPreferredSize()
              : gfx::Size();
      const int width_for_zero_state_library_button =
          should_show_saved_desk_library
              ? zero_state_library_button_size.width() + kZeroStateButtonSpacing
              : 0;

      const int content_width = zero_state_default_desk_button_size.width() +
                                kZeroStateButtonSpacing +
                                zero_state_new_desk_button_size.width() +
                                width_for_zero_state_library_button;
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

      if (zero_state_library_button) {
        zero_state_library_button->SetBoundsRect(
            gfx::Rect(gfx::Point(zero_state_new_desk_button->bounds().right() +
                                     kZeroStateButtonSpacing,
                                 kZeroStateY),
                      zero_state_library_button_size));
        zero_state_library_button->SetVisible(should_show_saved_desk_library);
      }
      return;
    }

    std::vector<DeskMiniView*> mini_views = bar_view_->mini_views();
    if (mini_views.empty()) {
      return;
    }
    // When RTL is enabled, we still want desks to be laid our in LTR, to match
    // the spatial order of desks. Therefore, we reverse the order of the mini
    // views before laying them out.
    if (base::i18n::IsRTL()) {
      base::ranges::reverse(mini_views);
    }

    auto* expanded_state_library_button =
        bar_view_->expanded_state_library_button();
    const bool expanded_state_library_button_visible =
        expanded_state_library_button &&
        expanded_state_library_button->GetVisible();

    gfx::Size mini_view_size = mini_views[0]->GetPreferredSize();

    // The new desk button and library button in the expanded bar view has the
    // same size as mini view.
    const int num_items = static_cast<int>(mini_views.size()) +
                          (expanded_state_library_button_visible ? 2 : 1);

    // Content width is sum of the width of all views, and plus the spacing
    // between the views, the focus ring's thickness and padding on each sides.
    const int content_width =
        num_items * (mini_view_size.width() + kMiniViewsSpacing) -
        kMiniViewsSpacing +
        kDeskBarDeskPreviewViewFocusRingThicknessAndPadding * 2;
    width_ = std::max(scroll_bounds.width(), content_width);

    // Update the size of the |host|, which is |scroll_view_contents_| here.
    // This is done to make sure its size can be updated on mini views' adding
    // or removing, then |scroll_view_| will know whether the contents need to
    // be scolled or not.
    host->SetSize(gfx::Size(width_, scroll_bounds.height()));

    // The x of the first mini view should include the focus ring thickness and
    // padding into consideration, otherwise the focus ring won't be drawn on
    // the left side of the first mini view.
    int x = (width_ - content_width) / 2 +
            kDeskBarDeskPreviewViewFocusRingThicknessAndPadding;
    const int y = kMiniViewsY - mini_views[0]->GetPreviewBorderInsets().top();
    for (auto* mini_view : mini_views) {
      mini_view->SetBoundsRect(gfx::Rect(gfx::Point(x, y), mini_view_size));
      x += (mini_view_size.width() + kMiniViewsSpacing);
    }
    bar_view_->expanded_state_new_desk_button()->SetBoundsRect(
        gfx::Rect(gfx::Point(x, y), mini_view_size));

    if (expanded_state_library_button) {
      x += (mini_view_size.width() + kMiniViewsSpacing);
      expanded_state_library_button->SetBoundsRect(
          gfx::Rect(gfx::Point(x, y), mini_view_size));
    }
  }

  // Layout the label which is shown below the desk icon button when the button
  // is at active state.
  void LayoutDeskIconButtonLabel(views::Label* label,
                                 const gfx::Rect& icon_button_bounds,
                                 DeskNameView* desk_name_view,
                                 int label_text_id) {
    label->SetText(gfx::ElideText(
        l10n_util::GetStringUTF16(label_text_id), gfx::FontList(),
        icon_button_bounds.width() - desk_name_view->GetInsets().width(),
        gfx::ELIDE_TAIL));

    const gfx::Size button_label_size = label->GetPreferredSize();

    label->SetBoundsRect(gfx::Rect(
        gfx::Point(
            icon_button_bounds.x() +
                ((icon_button_bounds.width() - button_label_size.width()) / 2),
            icon_button_bounds.bottom() + kDeskIconButtonAndLabelSpacing),
        gfx::Size(button_label_size.width(), desk_name_view->height())));
  }

  // TODO(conniekxu): After CrOS Next is launched, remove function
  // `LayoutInternal`, and move this to Layout.
  void LayoutInternalCrOSNext(views::View* host) {
    const gfx::Rect scroll_bounds = bar_view_->scroll_view_->bounds();

    auto* new_desk_button_label = bar_view_->new_desk_button_label();
    auto* library_button_label = bar_view_->library_button_label();

    // `host` here is `scroll_view_contents_`.
    if (bar_view_->IsZeroState()) {
      host->SetBoundsRect(scroll_bounds);

      new_desk_button_label->SetVisible(false);
      library_button_label->SetVisible(false);

      auto* default_desk_button = bar_view_->default_desk_button();
      const gfx::Size default_desk_button_size =
          default_desk_button->GetPreferredSize();

      auto* new_desk_button = bar_view_->new_desk_button();
      const gfx::Size new_desk_button_size =
          new_desk_button->GetPreferredSize();

      // The presenter is shutdown early in the overview destruction process to
      // prevent calls to the model. Some animations on the desk bar may still
      // call this function past shutdown start. In this case we just continue
      // as if the saved desks UI should be hidden.
      OverviewSession* session = bar_view_->overview_grid()->overview_session();
      const bool should_show_saved_desk_library =
          saved_desk_util::IsSavedDesksEnabled() && session &&
          !session->is_shutting_down() &&
          session->saved_desk_presenter()->should_show_saved_desk_library();
      auto* library_button = bar_view_->library_button();
      const gfx::Size library_button_size =
          should_show_saved_desk_library ? library_button->GetPreferredSize()
                                         : gfx::Size();
      const int width_for_library_button =
          should_show_saved_desk_library
              ? library_button_size.width() + kZeroStateButtonSpacing
              : 0;

      const int content_width =
          default_desk_button_size.width() + kZeroStateButtonSpacing +
          new_desk_button_size.width() + width_for_library_button;
      default_desk_button->SetBoundsRect(gfx::Rect(
          gfx::Point((scroll_bounds.width() - content_width) / 2, kZeroStateY),
          default_desk_button_size));
      // Update this button's text since it may changes while removing a desk
      // and going back to the zero state.
      default_desk_button->UpdateLabelText();
      // Make sure default desk button is always visible while in zero state
      // bar.
      default_desk_button->SetVisible(true);
      new_desk_button->SetBoundsRect(gfx::Rect(
          gfx::Point(
              default_desk_button->bounds().right() + kZeroStateButtonSpacing,
              kZeroStateY),
          new_desk_button_size));

      if (library_button) {
        library_button->SetBoundsRect(gfx::Rect(
            gfx::Point(
                new_desk_button->bounds().right() + kZeroStateButtonSpacing,
                kZeroStateY),
            library_button_size));
        library_button->SetVisible(should_show_saved_desk_library);
      }
      return;
    }

    std::vector<DeskMiniView*> mini_views = bar_view_->mini_views();
    if (mini_views.empty()) {
      return;
    }
    // When RTL is enabled, we still want desks to be laid our in LTR, to match
    // the spatial order of desks. Therefore, we reverse the order of the mini
    // views before laying them out.
    if (base::i18n::IsRTL()) {
      base::ranges::reverse(mini_views);
    }

    auto* library_button = bar_view_->library_button();
    const bool library_button_visible =
        library_button && library_button->GetVisible();
    gfx::Size library_button_size = library_button->GetPreferredSize();

    gfx::Size mini_view_size = mini_views[0]->GetPreferredSize();

    auto* new_desk_button = bar_view_->new_desk_button();
    gfx::Size new_desk_button_size = new_desk_button->GetPreferredSize();

    // Content width is sum of the width of all views, and plus the spacing
    // between the views, the focus ring's thickness and padding on each sides.
    const int content_width =
        mini_views.size() * (mini_view_size.width() + kMiniViewsSpacing) +
        (new_desk_button_size.width() + kMiniViewsSpacing) +
        (library_button_visible ? 1 : 0) *
            (library_button_size.width() + kMiniViewsSpacing) -
        kMiniViewsSpacing +
        kDeskBarDeskPreviewViewFocusRingThicknessAndPadding * 2;
    width_ = std::max(scroll_bounds.width(), content_width);

    // Update the size of the `host`, which is `scroll_view_contents_` here.
    // This is done to make sure its size can be updated on mini views' adding
    // or removing, then `scroll_view_` will know whether the contents need to
    // be scolled or not.
    host->SetSize(gfx::Size(width_, scroll_bounds.height()));

    // The x of the first mini view should include the focus ring thickness and
    // padding into consideration, otherwise the focus ring won't be drawn on
    // the left side of the first mini view.
    int x = (width_ - content_width) / 2 +
            kDeskBarDeskPreviewViewFocusRingThicknessAndPadding;
    const int y = kMiniViewsY - mini_views[0]->GetPreviewBorderInsets().top();
    for (auto* mini_view : mini_views) {
      mini_view->SetBoundsRect(gfx::Rect(gfx::Point(x, y), mini_view_size));
      x += (mini_view_size.width() + kMiniViewsSpacing);
    }

    const gfx::Rect new_desk_button_bounds(
        gfx::Rect(gfx::Point(x, y), new_desk_button_size));
    new_desk_button->SetBoundsRect(new_desk_button_bounds);

    auto* desk_name_view = mini_views[0]->desk_name_view();

    LayoutDeskIconButtonLabel(new_desk_button_label, new_desk_button_bounds,
                              desk_name_view, IDS_ASH_DESKS_NEW_DESK_BUTTON);
    new_desk_button_label->SetVisible(new_desk_button->state() ==
                                      CrOSNextDeskIconButton::State::kActive);

    if (library_button) {
      x += (new_desk_button_size.width() + kMiniViewsSpacing);
      const gfx::Rect library_button_bounds(
          gfx::Rect(gfx::Point(x, y), library_button_size));
      library_button->SetBoundsRect(library_button_bounds);
      LayoutDeskIconButtonLabel(
          library_button_label, library_button_bounds, desk_name_view,
          /*label_text_id=*/
          saved_desk_util::AreDesksTemplatesEnabled()
              ? IDS_ASH_DESKS_TEMPLATES_DESKS_BAR_BUTTON_LIBRARY
              : IDS_ASH_DESKS_TEMPLATES_DESKS_BAR_BUTTON_SAVED_FOR_LATER);
      library_button_label->SetVisible(library_button->state() ==
                                       CrOSNextDeskIconButton::State::kActive);
    }
  }

  // views::LayoutManager:
  void Layout(views::View* host) override {
    if (chromeos::features::IsJellyrollEnabled()) {
      LayoutInternalCrOSNext(host);
    } else {
      LayoutInternal(host);
    }
  }

  // views::LayoutManager:
  gfx::Size GetPreferredSize(const views::View* host) const override {
    return gfx::Size(width_, bar_view_->bounds().height());
  }

 private:
  raw_ptr<LegacyDeskBarView, ExperimentalAsh> bar_view_;  // Not owned.

  // Width of the scroll view. It is the contents' preferred width if it exceeds
  // the desk bar view's width or just the desk bar view's width if not.
  int width_ = 0;
};

// -----------------------------------------------------------------------------
// LegacyDeskBarView:

LegacyDeskBarView::LegacyDeskBarView(OverviewGrid* overview_grid)
    : DeskBarViewBase(overview_grid->root_window(),
                      DeskBarViewBase::Type::kOverview) {
  overview_grid_ = overview_grid;

  // TODO(b/278946142): Migrate `DesksBarScrollViewLayout` to use base class.
  scroll_view_contents_->SetLayoutManager(
      std::make_unique<DesksBarScrollViewLayout>(this));

  DesksController::Get()->AddObserver(this);
}

LegacyDeskBarView::~LegacyDeskBarView() {
  DesksController::Get()->RemoveObserver(this);
  if (drag_view_) {
    EndDragDesk(drag_view_, /*end_by_user=*/false);
  }
}

void LegacyDeskBarView::Init() {
  DeskBarViewBase::Init();

  // TODO(b/278946144): Migrate `DeskBarHoverObserver` to use base class.
  hover_observer_ = std::make_unique<DeskBarHoverObserver>(
      this, GetWidget()->GetNativeWindow());
}

void LegacyDeskBarView::OnHoverStateMayHaveChanged() {
  for (auto* mini_view : mini_views_) {
    mini_view->UpdateDeskButtonVisibility();
  }
}

void LegacyDeskBarView::OnGestureTap(const gfx::Rect& screen_rect,
                                     bool is_long_gesture) {
  for (auto* mini_view : mini_views_) {
    mini_view->OnWidgetGestureTap(screen_rect, is_long_gesture);
  }
}

void LegacyDeskBarView::SetDragDetails(const gfx::Point& screen_location,
                                       bool dragged_item_over_bar) {
  last_dragged_item_screen_location_ = screen_location;
  const bool old_dragged_item_over_bar = dragged_item_over_bar_;
  dragged_item_over_bar_ = dragged_item_over_bar;

  if (!old_dragged_item_over_bar && !dragged_item_over_bar) {
    return;
  }

  for (auto* mini_view : mini_views_) {
    mini_view->UpdateFocusColor();
  }

  if (DesksController::Get()->CanCreateDesks()) {
    if (chromeos::features::IsJellyrollEnabled()) {
      new_desk_button_->UpdateFocusState();
    } else {
      expanded_state_new_desk_button()->UpdateFocusColor();
    }
  }
}

void LegacyDeskBarView::HandlePressEvent(DeskMiniView* mini_view,
                                         const ui::LocatedEvent& event) {
  if (mini_view->is_animating_to_remove()) {
    return;
  }

  DeskNameView::CommitChanges(GetWidget());

  if (ui::EventTarget* target = event.target()) {
    gfx::PointF location = target->GetScreenLocationF(event);
    InitDragDesk(mini_view, location);
  }
}

void LegacyDeskBarView::HandleLongPressEvent(DeskMiniView* mini_view,
                                             const ui::LocatedEvent& event) {
  if (mini_view->is_animating_to_remove()) {
    return;
  }

  DeskNameView::CommitChanges(GetWidget());

  // Initialize and start drag.
  gfx::PointF location = event.target()->GetScreenLocationF(event);
  InitDragDesk(mini_view, location);
  StartDragDesk(mini_view, location, event.IsMouseEvent());

  mini_view->OpenContextMenu(ui::MENU_SOURCE_LONG_PRESS);
}

void LegacyDeskBarView::HandleDragEvent(DeskMiniView* mini_view,
                                        const ui::LocatedEvent& event) {
  // Do not perform drag if drag proxy is not initialized, or the mini view is
  // animating to be removed.
  if (!drag_proxy_ || mini_view->is_animating_to_remove()) {
    return;
  }

  mini_view->MaybeCloseContextMenu();

  gfx::PointF location = event.target()->GetScreenLocationF(event);

  // If the drag proxy is initialized, start the drag. If the drag started,
  // continue drag.
  switch (drag_proxy_->state()) {
    case DeskDragProxy::State::kInitialized:
      StartDragDesk(mini_view, location, event.IsMouseEvent());
      break;
    case DeskDragProxy::State::kStarted:
      ContinueDragDesk(mini_view, location);
      break;
    default:
      NOTREACHED();
  }
}

bool LegacyDeskBarView::HandleReleaseEvent(DeskMiniView* mini_view,
                                           const ui::LocatedEvent& event) {
  // Do not end drag if the proxy is not initialized, or the mini view is
  // animating to be removed.
  if (!drag_proxy_ || mini_view->is_animating_to_remove()) {
    return false;
  }

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

void LegacyDeskBarView::InitDragDesk(DeskMiniView* mini_view,
                                     const gfx::PointF& location_in_screen) {
  DCHECK(!mini_view->is_animating_to_remove());

  // If another view is being dragged, then end the drag.
  if (drag_view_) {
    EndDragDesk(drag_view_, /*end_by_user=*/false);
  }

  drag_view_ = mini_view;

  gfx::PointF preview_origin_in_screen(
      drag_view_->GetPreviewBoundsInScreen().origin());
  const float init_offset_x =
      location_in_screen.x() - preview_origin_in_screen.x();

  // Create a drag proxy for the dragged desk.
  drag_proxy_ =
      std::make_unique<DeskDragProxy>(this, drag_view_, init_offset_x);
}

void LegacyDeskBarView::StartDragDesk(DeskMiniView* mini_view,
                                      const gfx::PointF& location_in_screen,
                                      bool is_mouse_dragging) {
  DCHECK(drag_view_);
  DCHECK(drag_proxy_);
  DCHECK_EQ(mini_view, drag_view_);
  DCHECK(!mini_view->is_animating_to_remove());

  // Hide the dragged mini view.
  drag_view_->layer()->SetOpacity(0.0f);

  // Create a drag proxy widget, scale it up and move its x-coordinate according
  // to the x of |location_in_screen|.
  drag_proxy_->InitAndScaleAndMoveToX(location_in_screen.x());

  Shell::Get()->cursor_manager()->SetCursor(ui::mojom::CursorType::kGrabbing);

  // Fire a haptic event if necessary.
  if (is_mouse_dragging) {
    haptics_util::PlayHapticTouchpadEffect(
        ui::HapticTouchpadEffect::kTick,
        ui::HapticTouchpadEffectStrength::kMedium);
  }
}

void LegacyDeskBarView::ContinueDragDesk(
    DeskMiniView* mini_view,
    const gfx::PointF& location_in_screen) {
  DCHECK(drag_view_);
  DCHECK(drag_proxy_);
  DCHECK_EQ(mini_view, drag_view_);
  DCHECK(!mini_view->is_animating_to_remove());

  drag_proxy_->DragToX(location_in_screen.x());

  // Check if the desk is on the scroll arrow buttons. Do not determine move
  // index while scrolling, since the positions of the desks on bar keep varying
  // during this process.
  if (MaybeScrollByDraggedDesk()) {
    return;
  }

  const auto drag_view_iter = base::ranges::find(mini_views_, drag_view_);
  DCHECK(drag_view_iter != mini_views_.cend());

  const int old_index = drag_view_iter - mini_views_.cbegin();

  const int drag_pos_screen_x = drag_proxy_->GetBoundsInScreen().origin().x();

  // Determine the target location for the desk to be reordered.
  const int new_index = DetermineMoveIndex(drag_pos_screen_x);

  if (old_index != new_index) {
    Shell::Get()->desks_controller()->ReorderDesk(old_index, new_index);
  }
}

void LegacyDeskBarView::EndDragDesk(DeskMiniView* mini_view, bool end_by_user) {
  DCHECK(drag_view_);
  DCHECK(drag_proxy_);
  DCHECK_EQ(mini_view, drag_view_);
  DCHECK(!mini_view->is_animating_to_remove());

  // Update default desk names after dropping.
  Shell::Get()->desks_controller()->UpdateDesksDefaultNames();
  Shell::Get()->cursor_manager()->SetCursor(ui::mojom::CursorType::kPointer);

  // We update combine desks tooltips here to reflect the updated desk default
  // names.
  MaybeUpdateCombineDesksTooltips();

  // Stop scroll even if the desk is on the scroll arrow buttons.
  left_scroll_button_->OnDeskHoverEnd();
  right_scroll_button_->OnDeskHoverEnd();

  // If the reordering is ended by the user (release the drag), perform the
  // snapping back animation and scroll the bar to target position. If current
  // drag is ended due to the start of a new drag or the end of the overview,
  // directly finalize current drag.
  if (end_by_user) {
    ScrollToShowViewIfNecessary(drag_view_);
    drag_proxy_->SnapBackToDragView();
  } else {
    FinalizeDragDesk();
  }
}

void LegacyDeskBarView::FinalizeDragDesk() {
  if (drag_view_) {
    drag_view_->layer()->SetOpacity(1.0f);
    drag_view_ = nullptr;
  }
  drag_proxy_.reset();
}

const char* LegacyDeskBarView::GetClassName() const {
  return "LegacyDeskBarView";
}

void LegacyDeskBarView::OnDeskAdded(const Desk* desk) {
  DeskNameView::CommitChanges(GetWidget());

  if (chromeos::features::IsJellyrollEnabled()) {
    const bool is_expanding_bar_view =
        new_desk_button_->state() == CrOSNextDeskIconButton::State::kZero;
    UpdateNewMiniViews(/*initializing_bar_view=*/false, is_expanding_bar_view);
    MaybeUpdateCombineDesksTooltips();
    if (!DesksController::Get()->CanCreateDesks()) {
      new_desk_button_->SetEnabled(/*enabled=*/false);
    }
  } else {
    const bool is_expanding_bar_view =
        zero_state_new_desk_button_->GetVisible();
    UpdateNewMiniViews(/*initializing_bar_view=*/false, is_expanding_bar_view);
    MaybeUpdateCombineDesksTooltips();

    if (!DesksController::Get()->CanCreateDesks()) {
      expanded_state_new_desk_button_->SetButtonState(/*enabled=*/false);
    }
  }
}

void LegacyDeskBarView::OnDeskRemoved(const Desk* desk) {
  DeskNameView::CommitChanges(GetWidget());
  auto iter = base::ranges::find(mini_views_, desk, &DeskMiniView::desk);

  // There are cases where a desk may be removed before the `desks_bar_view`
  // finishes initializing (i.e. removed on a separate root window before the
  // overview starting animation completes). In those cases, that mini_view
  // would not exist and the bar view will already be in the correct state so we
  // do not need to update the UI (https://crbug.com/1346154).
  if (iter == mini_views_.end()) {
    return;
  }

  // Let the highlight controller know the view is destroying before it is
  // removed from the collection because it needs to know the index of the mini
  // view, or the desk name view (if either is currently highlighted) relative
  // to other traversable views.
  auto* highlight_controller = GetHighlightController();
  // The order here matters, we call it first on the desk_name_view since it
  // comes later in the highlight order (See documentation of
  // OnViewDestroyingOrDisabling()).
  highlight_controller->OnViewDestroyingOrDisabling((*iter)->desk_name_view());
  highlight_controller->OnViewDestroyingOrDisabling((*iter)->desk_preview());

  if (chromeos::features::IsJellyrollEnabled()) {
    new_desk_button_->SetEnabled(/*enabled=*/true);
  } else {
    expanded_state_new_desk_button_->SetButtonState(/*enabled=*/true);
  }

  for (auto* mini_view : mini_views_) {
    mini_view->UpdateDeskButtonVisibility();
  }

  // If Jellyroll is not enabled, switch to zero state if there will be one desk
  // after removal, unless we are viewing the saved desk library.
  if (!chromeos::features::IsJellyrollEnabled() && mini_views_.size() == 2u &&
      !overview_grid_->IsShowingSavedDeskLibrary()) {
    SwitchToZeroState();
    return;
  }

  const int begin_x = GetFirstMiniViewXOffset();
  // Remove the mini view from the list now. And remove it from its parent
  // after the animation is done.
  DeskMiniView* removed_mini_view = *iter;
  auto partition_iter = mini_views_.erase(iter);

  // End dragging desk if remove a dragged desk.
  if (drag_view_ == removed_mini_view) {
    EndDragDesk(removed_mini_view, /*end_by_user=*/false);
  }

  Layout();
  PerformRemoveDeskMiniViewAnimation(
      this, removed_mini_view,
      std::vector<DeskMiniView*>(mini_views_.begin(), partition_iter),
      std::vector<DeskMiniView*>(partition_iter, mini_views_.end()),
      begin_x - GetFirstMiniViewXOffset());

  MaybeUpdateCombineDesksTooltips();
}

void LegacyDeskBarView::OnDeskReordered(int old_index, int new_index) {
  desks_util::ReorderItem(mini_views_, old_index, new_index);

  // Update the order of child views.
  auto* reordered_view = mini_views_[new_index];
  reordered_view->parent()->ReorderChildView(reordered_view, new_index);
  reordered_view->parent()->NotifyAccessibilityEvent(
      ax::mojom::Event::kTreeChanged, true);

  Layout();

  // Call the animation function after reorder the mini views.
  PerformReorderDeskMiniViewAnimation(old_index, new_index, mini_views_);
  MaybeUpdateCombineDesksTooltips();
}

void LegacyDeskBarView::OnDeskActivationChanged(const Desk* activated,
                                                const Desk* deactivated) {
  for (auto* mini_view : mini_views_) {
    const Desk* desk = mini_view->desk();
    if (desk == activated || desk == deactivated) {
      mini_view->UpdateFocusColor();
    }
  }
}

void LegacyDeskBarView::OnDeskNameChanged(const Desk* desk,
                                          const std::u16string& new_name) {
  MaybeUpdateCombineDesksTooltips();
}

void LegacyDeskBarView::UpdateNewMiniViews(bool initializing_bar_view,
                                           bool expanding_bar_view) {
  const auto& desks = DesksController::Get()->desks();
  if (initializing_bar_view) {
    UpdateDeskButtonsVisibility();
  }
  if (IsZeroState() && !expanding_bar_view) {
    return;
  }

  // This should not be called when a desk is removed.
  DCHECK_LE(mini_views_.size(), desks.size());

  const int begin_x = GetFirstMiniViewXOffset();
  aura::Window* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  DCHECK(root_window);

  // New mini views can be added at any index, so we need to iterate through and
  // insert new mini views in a position in `mini_views_` that corresponds to
  // their index in the `DeskController`'s list of desks.
  int mini_view_index = 0;
  std::vector<DeskMiniView*> new_mini_views;
  for (const auto& desk : desks) {
    if (!FindMiniViewForDesk(desk.get())) {
      DeskMiniView* mini_view = scroll_view_contents_->AddChildViewAt(
          std::make_unique<DeskMiniView>(this, root_window, desk.get()),
          mini_view_index);
      mini_views_.insert(mini_views_.begin() + mini_view_index, mini_view);
      new_mini_views.push_back(mini_view);
    }
    ++mini_view_index;
  }

  if (expanding_bar_view) {
    SwitchToExpandedState();
    return;
  }

  if (chromeos::features::IsJellyrollEnabled()) {
    if (new_desk_button_->state() == CrOSNextDeskIconButton::State::kActive) {
      // Make sure the new desk button is updated to expanded state from the
      // active state. This can happen when dropping the window on the new desk
      // button.
      new_desk_button_->UpdateState(CrOSNextDeskIconButton::State::kExpanded);
    }
  }

  Layout();

  if (initializing_bar_view) {
    return;
  }

  // We need to compile lists of the mini views on either side of the new mini
  // views so that they can be moved to make room for the new mini views in the
  // desk bar.
  auto left_partition_iter =
      base::ranges::find(mini_views_, new_mini_views.front());
  auto right_partition_iter =
      std::next(base::ranges::find(mini_views_, new_mini_views.back()));

  // A vector between `left_partition_iter` and `right_partition_iter` should be
  // the same as `new_mini_views` if they were added correctly.
  DCHECK(std::vector<DeskMiniView*>(left_partition_iter,
                                    right_partition_iter) == new_mini_views);

  PerformNewDeskMiniViewAnimation(
      this, new_mini_views,
      std::vector<DeskMiniView*>(mini_views_.begin(), left_partition_iter),
      std::vector<DeskMiniView*>(right_partition_iter, mini_views_.end()),
      begin_x - GetFirstMiniViewXOffset());
}

void LegacyDeskBarView::SwitchToZeroState() {
  DCHECK(!chromeos::features::IsJellyrollEnabled());

  state_ = DeskBarViewBase::State::kZero;

  // In zero state, if the only desk is being dragged, we should end dragging.
  // Because the dragged desk's mini view is removed, the mouse released or
  // gesture ended events cannot be received. |drag_view_| will keep the stale
  // reference of removed mini view and |drag_proxy_| will not be reset.
  if (drag_view_) {
    EndDragDesk(drag_view_, /*end_by_user=*/false);
  }

  std::vector<DeskMiniView*> removed_mini_views = mini_views_;
  mini_views_.clear();

  auto* highlight_controller = GetHighlightController();
  OverviewHighlightableView* view = highlight_controller->highlighted_view();
  // Reset the highlight if it is highlighted on a descendant of `this`.
  if (view && Contains(view->GetView())) {
    highlight_controller->ResetHighlightedView();
  }

  // Keep current layout until the animation is completed since the animation
  // for going back to zero state is based on the expanded bar's current
  // layout.
  PerformExpandedStateToZeroStateMiniViewAnimation(this, removed_mini_views);
}

void LegacyDeskBarView::SwitchToExpandedState() {
  state_ = DeskBarViewBase::State::kExpanded;

  UpdateDeskButtonsVisibility();
  if (chromeos::features::IsJellyrollEnabled()) {
    PerformZeroStateToExpandedStateMiniViewAnimationCrOSNext(this);
  } else {
    PerformZeroStateToExpandedStateMiniViewAnimation(this);
  }
}

bool LegacyDeskBarView::MaybeScrollByDraggedDesk() {
  DCHECK(drag_proxy_);

  const gfx::Rect proxy_bounds = drag_proxy_->GetBoundsInScreen();

  // If the desk proxy overlaps a scroll button, scroll the bar in the
  // corresponding direction.
  for (ScrollArrowButton* scroll_button : {
           left_scroll_button_,
           right_scroll_button_,
       }) {
    if (scroll_button->GetVisible() &&
        proxy_bounds.Intersects(scroll_button->GetBoundsInScreen())) {
      scroll_button->OnDeskHoverStart();
      return true;
    }
    scroll_button->OnDeskHoverEnd();
  }

  return false;
}

void LegacyDeskBarView::UpdateDeskIconButtonState(
    CrOSNextDeskIconButton* button,
    CrOSNextDeskIconButton::State target_state) {
  DCHECK(chromeos::features::IsJellyrollEnabled());
  DCHECK_NE(target_state, CrOSNextDeskIconButton::State::kZero);

  if (button->state() == target_state) {
    return;
  }

  const int begin_x = GetFirstMiniViewXOffset();
  gfx::Rect current_bounds = button->GetBoundsInScreen();

  DeskBarViewBase::UpdateDeskIconButtonState(button, target_state);

  gfx::RectF target_bounds = gfx::RectF(new_desk_button_->GetBoundsInScreen());
  gfx::Transform scale_transform;
  const int shift_x = begin_x - GetFirstMiniViewXOffset();
  scale_transform.Translate(shift_x, 0);
  scale_transform.Scale(current_bounds.width() / target_bounds.width(),
                        current_bounds.height() / target_bounds.height());

  // TODO(b/278946302): Migrate desk mini view animations to use base class.
  PerformDeskIconButtonScaleAnimationCrOSNext(button, this, scale_transform,
                                              shift_x);
}

}  // namespace ash

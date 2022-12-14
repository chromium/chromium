// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_bar_view.h"

#include <iterator>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/pill_button.h"
#include "ash/utility/haptics_util.h"
#include "ash/wm/desks/desk_action_view.h"
#include "ash/wm/desks/desk_drag_proxy.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_mini_view_animations.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/persistent_desks_bar/persistent_desks_bar_button.h"
#include "ash/wm/desks/persistent_desks_bar/persistent_desks_bar_controller.h"
#include "ash/wm/desks/scroll_arrow_button.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "ash/wm/desks/templates/saved_desk_presenter.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event_observer.h"
#include "ui/events/types/event_type.h"
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

// In the non-compact layout, this is the height allocated for elements other
// than the desk preview (e.g. the DeskNameView, and the vertical paddings).
// Note, the vertical paddings should exclude the preview border's insets.
constexpr int kNonPreviewAllocatedHeight = 48;

constexpr int kMiniViewsY = 16;

// Spacing between mini views.
constexpr int kMiniViewsSpacing = 12;

// Location of the "up next" button for glanceables.
constexpr int kUpNextX = 4;

// Spacing between zero state default desk button and new desk button.
constexpr int kZeroStateButtonSpacing = 8;

// The local Y coordinate of the zero state desk buttons.
constexpr int kZeroStateY = 6;

// The minimum horizontal padding of the scroll view. This is set to make sure
// there is enough space for the scroll buttons.
constexpr int kScrollViewMinimumHorizontalPadding = 32;

constexpr int kScrollButtonWidth = 36;

constexpr int kGradientZoneLength = 40;

constexpr int kVerticalDotsButtonVerticalPadding = 8;
constexpr int kVerticalDotsButtonRightPadding = 8;

constexpr int kDeskPreviewViewFocusRingThicknessAndPadding = 4;

// The duration of scrolling one page.
constexpr base::TimeDelta kBarScrollDuration = base::Milliseconds(250);

gfx::Rect GetGestureEventScreenRect(const ui::Event& event) {
  DCHECK(event.IsGestureEvent());
  return event.AsGestureEvent()->details().bounding_box();
}

OverviewHighlightController* GetHighlightController() {
  auto* overview_controller = Shell::Get()->overview_controller();
  DCHECK(overview_controller->InOverviewSession());
  return overview_controller->overview_session()->highlight_controller();
}

// Initialize a scoped layer animation settings for scroll view contents.
void InitScrollContentsAnimationSettings(
    ui::ScopedLayerAnimationSettings& settings) {
  settings.SetTransitionDuration(kBarScrollDuration);
  settings.SetTweenType(gfx::Tween::ACCEL_20_DECEL_60);
}

// Checks whether there are any external keyboards.
bool HasExternalKeyboard() {
  for (const ui::InputDevice& device :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (device.type != ui::InputDeviceType::INPUT_DEVICE_INTERNAL)
      return true;
  }
  return false;
}

// Callback for click/tap on the "Up next" button for glanceables.
void OnUpNextButtonPressed() {
  Shell::Get()->overview_controller()->EndOverview(
      OverviewEndAction::kShowGlanceables,
      OverviewEnterExitType::kImmediateExit);
  Shell::Get()->glanceables_controller()->ShowFromOverview();
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
  DesksBarView* owner_;

  std::unique_ptr<views::EventMonitor> event_monitor_;
};

// -----------------------------------------------------------------------------
// DesksBarScrollViewLayout:

// All the desks bar contents except the background view are added to
// be the children of the |scroll_view_| to support scrollable desks bar.
// DesksBarScrollViewLayout will help lay out the contents of the
// |scroll_view_|.
class DesksBarScrollViewLayout : public views::LayoutManager {
 public:
  DesksBarScrollViewLayout(DesksBarView* bar_view) : bar_view_(bar_view) {}
  DesksBarScrollViewLayout(const DesksBarScrollViewLayout&) = delete;
  DesksBarScrollViewLayout& operator=(const DesksBarScrollViewLayout&) = delete;
  ~DesksBarScrollViewLayout() override = default;

  // views::LayoutManager:
  void Layout(views::View* host) override {
    const gfx::Rect scroll_bounds = bar_view_->scroll_view_->bounds();

    // The glanceables UI goes on the left edge regardless of zero state or
    // expanded state.
    // TODO(crbug.com/1353119): Real layout once we have specs for both modes.
    auto* up_next_button = bar_view_->up_next_button();
    if (up_next_button) {
      const gfx::Size size = up_next_button->GetPreferredSize();
      const int y = (scroll_bounds.height() / 2) - (size.height() / 2);
      up_next_button->SetBounds(kUpNextX, y, size.width(), size.height());
    }

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

      // The presenter is shutdown early in the overview destruction process to
      // prevent calls to the model. Some animations on the desks bar may still
      // call this function past shutdown start. In this case we just continue
      // as if the saved desks UI should be hidden.
      OverviewSession* session = bar_view_->overview_grid()->overview_session();
      const bool should_show_saved_desk_library =
          saved_desk_util::IsSavedDesksEnabled() && session &&
          !session->is_shutting_down() &&
          session->saved_desk_presenter()->should_show_saved_desk_library();
      auto* zero_state_desks_templates_button =
          bar_view_->zero_state_desks_templates_button();
      const gfx::Size zero_state_desks_templates_button_size =
          should_show_saved_desk_library
              ? zero_state_desks_templates_button->GetPreferredSize()
              : gfx::Size();
      const int width_for_zero_state_desks_templates_button =
          should_show_saved_desk_library
              ? zero_state_desks_templates_button_size.width() +
                    kZeroStateButtonSpacing
              : 0;

      const int content_width = zero_state_default_desk_button_size.width() +
                                kZeroStateButtonSpacing +
                                zero_state_new_desk_button_size.width() +
                                width_for_zero_state_desks_templates_button;
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

      if (zero_state_desks_templates_button) {
        zero_state_desks_templates_button->SetBoundsRect(
            gfx::Rect(gfx::Point(zero_state_new_desk_button->bounds().right() +
                                     kZeroStateButtonSpacing,
                                 kZeroStateY),
                      zero_state_desks_templates_button_size));
        zero_state_desks_templates_button->SetVisible(
            should_show_saved_desk_library);
      }
      return;
    }

    std::vector<DeskMiniView*> mini_views = bar_view_->mini_views();
    if (mini_views.empty())
      return;
    // When RTL is enabled, we still want desks to be laid our in LTR, to match
    // the spatial order of desks. Therefore, we reverse the order of the mini
    // views before laying them out.
    if (base::i18n::IsRTL())
      base::ranges::reverse(mini_views);

    auto* expanded_state_desks_templates_button =
        bar_view_->expanded_state_desks_templates_button();
    const bool expanded_state_desks_templates_button_visible =
        expanded_state_desks_templates_button &&
        expanded_state_desks_templates_button->GetVisible();

    gfx::Size mini_view_size = mini_views[0]->GetPreferredSize();

    // The new desk button and template button in the expanded bar view has the
    // same size as mini view.
    const int num_items =
        static_cast<int>(mini_views.size()) +
        (expanded_state_desks_templates_button_visible ? 2 : 1);

    // Content width is sum of the width of all views, and plus the spacing
    // between the views, the focus ring's thickness and padding on each sides.
    const int content_width =
        num_items * (mini_view_size.width() + kMiniViewsSpacing) -
        kMiniViewsSpacing + kDeskPreviewViewFocusRingThicknessAndPadding * 2;
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
            kDeskPreviewViewFocusRingThicknessAndPadding;
    const int y = kMiniViewsY - mini_views[0]->GetPreviewBorderInsets().top();
    for (auto* mini_view : mini_views) {
      mini_view->SetBoundsRect(gfx::Rect(gfx::Point(x, y), mini_view_size));
      x += (mini_view_size.width() + kMiniViewsSpacing);
    }
    bar_view_->expanded_state_new_desk_button()->SetBoundsRect(
        gfx::Rect(gfx::Point(x, y), mini_view_size));

    if (expanded_state_desks_templates_button) {
      x += (mini_view_size.width() + kMiniViewsSpacing);
      expanded_state_desks_templates_button->SetBoundsRect(
          gfx::Rect(gfx::Point(x, y), mini_view_size));
    }
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
    : overview_grid_(overview_grid) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  if (features::IsDarkLightModeEnabled()) {
    SetBorder(std::make_unique<views::HighlightBorder>(
        /*corner_radius=*/0, views::HighlightBorder::Type::kHighlightBorder2,
        /*use_light_colors=*/false));
  }

  SetBackground(views::CreateThemedSolidBackground(kColorAshShieldAndBase80));
  // Use layer scrolling so that the contents will paint on top of the parent,
  // which uses SetPaintToLayer()
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->SetPaintToLayer();
  scroll_view_->layer()->SetFillsBoundsOpaquely(false);
  scroll_view_->SetBackgroundColor(absl::nullopt);
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

  if (PersistentDesksBarController::ShouldPersistentDesksBarBeVisible()) {
    vertical_dots_button_ =
        AddChildView(std::make_unique<PersistentDesksBarVerticalDotsButton>());
    vertical_dots_button_->SetPaintToLayer();
    vertical_dots_button_->layer()->SetFillsBoundsOpaquely(false);
  }

  // Make the scroll content view animatable by painting to a layer.
  scroll_view_contents_ =
      scroll_view_->SetContents(std::make_unique<views::View>());
  scroll_view_contents_->SetPaintToLayer();

  if (features::AreGlanceablesEnabled() &&
      Shell::Get()->session_controller()->IsUserPrimary()) {
    up_next_button_ =
        scroll_view_contents_->AddChildView(std::make_unique<PillButton>(
            base::BindRepeating(&OnUpNextButtonPressed),
            l10n_util::GetStringUTF16(IDS_GLANCEABLES_UP_NEXT)));
  }

  expanded_state_new_desk_button_ = scroll_view_contents_->AddChildView(
      std::make_unique<ExpandedDesksBarButton>(
          this, &kDesksNewDeskButtonIcon,
          l10n_util::GetStringUTF16(IDS_ASH_DESKS_NEW_DESK_BUTTON),
          /*initially_enabled=*/DesksController::Get()->CanCreateDesks(),
          base::BindRepeating(&DesksBarView::OnNewDeskButtonPressed,
                              base::Unretained(this),
                              DesksCreationRemovalSource::kButton)));

  zero_state_default_desk_button_ = scroll_view_contents_->AddChildView(
      std::make_unique<ZeroStateDefaultDeskButton>(this));
  zero_state_new_desk_button_ =
      scroll_view_contents_->AddChildView(std::make_unique<ZeroStateIconButton>(
          &kDesksNewDeskButtonIcon,
          l10n_util::GetStringUTF16(IDS_ASH_DESKS_NEW_DESK_BUTTON),
          base::BindRepeating(&DesksBarView::OnNewDeskButtonPressed,
                              base::Unretained(this),
                              DesksCreationRemovalSource::kButton)));
  if (saved_desk_util::IsSavedDesksEnabled()) {
    int button_text_id = IDS_ASH_DESKS_TEMPLATES_DESKS_BAR_BUTTON_LIBRARY;
    if (!saved_desk_util::AreDesksTemplatesEnabled())
      button_text_id = IDS_ASH_DESKS_TEMPLATES_DESKS_BAR_BUTTON_SAVED_FOR_LATER;

    expanded_state_desks_templates_button_ =
        scroll_view_contents_->AddChildView(
            std::make_unique<ExpandedDesksBarButton>(
                this, &kDesksTemplatesIcon,
                l10n_util::GetStringUTF16(button_text_id),
                /*initially_enabled=*/true,
                base::BindRepeating(&DesksBarView::OnLibraryButtonPressed,
                                    base::Unretained(this))));
    zero_state_desks_templates_button_ = scroll_view_contents_->AddChildView(
        std::make_unique<ZeroStateIconButton>(
            &kDesksTemplatesIcon, l10n_util::GetStringUTF16(button_text_id),
            base::BindRepeating(&DesksBarView::OnLibraryButtonPressed,
                                base::Unretained(this))));
  }
  scroll_view_contents_->SetLayoutManager(
      std::make_unique<DesksBarScrollViewLayout>(this));

  on_contents_scrolled_subscription_ =
      scroll_view_->AddContentsScrolledCallback(base::BindRepeating(
          &DesksBarView::OnContentsScrolled, base::Unretained(this)));
  on_contents_scroll_ended_subscription_ =
      scroll_view_->AddContentsScrollEndedCallback(base::BindRepeating(
          &DesksBarView::OnContentsScrollEnded, base::Unretained(this)));

  DesksController::Get()->AddObserver(this);
}

DesksBarView::~DesksBarView() {
  DesksController::Get()->RemoveObserver(this);
  if (drag_view_)
    EndDragDesk(drag_view_, /*end_by_user=*/false);
}

// static
constexpr int DesksBarView::kZeroStateBarHeight;

// static
int DesksBarView::GetExpandedBarHeight(aura::Window* root) {
  return DeskPreviewView::GetHeight(root) + kNonPreviewAllocatedHeight;
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
  params.activatable = views::Widget::InitParams::Activatable::kYes;
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
  window->SetId(kShellWindowId_DesksBarWindow);
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

int DesksBarView::GetMiniViewIndex(const DeskMiniView* mini_view) const {
  auto iter = base::ranges::find(mini_views_, mini_view);
  return (iter == mini_views_.cend())
             ? -1
             : std::distance(mini_views_.cbegin(), iter);
}

void DesksBarView::OnHoverStateMayHaveChanged() {
  for (auto* mini_view : mini_views_)
    mini_view->UpdateDeskButtonVisibility();
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
    mini_view->UpdateFocusColor();

  if (features::IsDragWindowToNewDeskEnabled() &&
      DesksController::Get()->CanCreateDesks()) {
    expanded_state_new_desk_button()->UpdateFocusColor();
  }
}

bool DesksBarView::IsZeroState() const {
  return mini_views_.empty() && DesksController::Get()->desks().size() == 1;
}

void DesksBarView::HandlePressEvent(DeskMiniView* mini_view,
                                    const ui::LocatedEvent& event) {
  if (mini_view->is_animating_to_remove())
    return;

  DeskNameView::CommitChanges(GetWidget());

  if (ui::EventTarget* target = event.target()) {
    gfx::PointF location = target->GetScreenLocationF(event);
    InitDragDesk(mini_view, location);
  }
}

void DesksBarView::HandleLongPressEvent(DeskMiniView* mini_view,
                                        const ui::LocatedEvent& event) {
  if (mini_view->is_animating_to_remove())
    return;

  DeskNameView::CommitChanges(GetWidget());

  // Initialize and start drag.
  gfx::PointF location = event.target()->GetScreenLocationF(event);
  InitDragDesk(mini_view, location);
  StartDragDesk(mini_view, location, event.IsMouseEvent());

  if (features::IsDesksCloseAllEnabled())
    mini_view->OpenContextMenu(ui::MENU_SOURCE_LONG_PRESS);
}

void DesksBarView::HandleDragEvent(DeskMiniView* mini_view,
                                   const ui::LocatedEvent& event) {
  // Do not perform drag if drag proxy is not initialized, or the mini view is
  // animating to be removed.
  if (!drag_proxy_ || mini_view->is_animating_to_remove())
    return;

  if (features::IsDesksCloseAllEnabled())
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

bool DesksBarView::HandleReleaseEvent(DeskMiniView* mini_view,
                                      const ui::LocatedEvent& event) {
  // Do not end drag if the proxy is not initialized, or the mini view is
  // animating to be removed.
  if (!drag_proxy_ || mini_view->is_animating_to_remove())
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
  DCHECK(!mini_view->is_animating_to_remove());

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

void DesksBarView::ContinueDragDesk(DeskMiniView* mini_view,
                                    const gfx::PointF& location_in_screen) {
  DCHECK(drag_view_);
  DCHECK(drag_proxy_);
  DCHECK_EQ(mini_view, drag_view_);
  DCHECK(!mini_view->is_animating_to_remove());

  drag_proxy_->DragToX(location_in_screen.x());

  // Check if the desk is on the scroll arrow buttons. Do not determine move
  // index while scrolling, since the positions of the desks on bar keep varying
  // during this process.
  if (MaybeScrollByDraggedDesk())
    return;

  const auto drag_view_iter = base::ranges::find(mini_views_, drag_view_);
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

void DesksBarView::OnSavedDeskLibraryHidden() {
  if (mini_views_.size() == 1u)
    SwitchToZeroState();
}

const char* DesksBarView::GetClassName() const {
  return "DesksBarView";
}

void DesksBarView::Layout() {
  if (is_bounds_animation_on_going_)
    return;

  // Scroll buttons are kept |kScrollViewMinimumHorizontalPadding| away from
  // the edge of the scroll view. So the horizontal padding of the scroll view
  // is set to guarantee enough space for the scroll buttons.
  const gfx::Insets insets = overview_grid_->GetGridInsets();
  DCHECK(insets.left() == insets.right());
  const int horizontal_padding =
      std::max(kScrollViewMinimumHorizontalPadding, insets.left());
  left_scroll_button_->SetBounds(
      horizontal_padding - kScrollViewMinimumHorizontalPadding, bounds().y(),
      kScrollButtonWidth, bounds().height());
  right_scroll_button_->SetBounds(
      bounds().right() - horizontal_padding -
          (kScrollButtonWidth - kScrollViewMinimumHorizontalPadding),
      bounds().y(), kScrollButtonWidth, bounds().height());

  if (vertical_dots_button_) {
    const gfx::Size vertical_dots_button_size =
        vertical_dots_button_->GetPreferredSize();
    vertical_dots_button_->SetBoundsRect(gfx::Rect(
        gfx::Point(bounds().right() - vertical_dots_button_size.width() -
                       kVerticalDotsButtonRightPadding,
                   bounds().y() + kVerticalDotsButtonVerticalPadding),
        vertical_dots_button_size));
  }

  gfx::Rect scroll_bounds = bounds();
  // Align with the overview grid in horizontal, so only horizontal insets are
  // needed here.
  scroll_bounds.Inset(gfx::Insets::VH(0, horizontal_padding));
  scroll_view_->SetBoundsRect(scroll_bounds);

  // Clip the contents that are outside of the |scroll_view_|'s bounds.
  scroll_view_->layer()->SetMasksToBounds(true);
  scroll_view_->Layout();

  UpdateScrollButtonsVisibility();
  UpdateGradientMask();
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

void DesksBarView::OnDeskAdded(const Desk* desk) {
  DeskNameView::CommitChanges(GetWidget());
  const bool is_expanding_bar_view = zero_state_new_desk_button_->GetVisible();
  UpdateNewMiniViews(/*initializing_bar_view=*/false, is_expanding_bar_view);
  MaybeUpdateCombineDesksTooltips();

  if (!DesksController::Get()->CanCreateDesks())
    expanded_state_new_desk_button_->SetButtonState(/*enabled=*/false);
}

void DesksBarView::OnDeskRemoved(const Desk* desk) {
  DeskNameView::CommitChanges(GetWidget());
  auto iter = base::ranges::find(mini_views_, desk, &DeskMiniView::desk);

  // There are cases where a desk may be removed before the `desks_bar_view`
  // finishes initializing (i.e. removed on a separate root window before the
  // overview starting animation completes). In those cases, that mini_view
  // would not exist and the bar view will already be in the correct state so we
  // do not need to update the UI (https://crbug.com/1346154).
  if (iter == mini_views_.end())
    return;

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

  expanded_state_new_desk_button_->SetButtonState(/*enabled=*/true);

  for (auto* mini_view : mini_views_)
    mini_view->UpdateDeskButtonVisibility();

  // Switch to zero state, which happens if there would be one desk after
  // removal, unless we are viewing the saved desk library.
  if (mini_views_.size() == 2u &&
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
  if (drag_view_ == removed_mini_view)
    EndDragDesk(removed_mini_view, /*end_by_user=*/false);

  Layout();
  PerformRemoveDeskMiniViewAnimation(
      this, removed_mini_view,
      std::vector<DeskMiniView*>(mini_views_.begin(), partition_iter),
      std::vector<DeskMiniView*>(partition_iter, mini_views_.end()),
      expanded_state_new_desk_button_, expanded_state_desks_templates_button_,
      begin_x - GetFirstMiniViewXOffset());

  MaybeUpdateCombineDesksTooltips();
}

void DesksBarView::OnDeskReordered(int old_index, int new_index) {
  desks_util::ReorderItem(mini_views_, old_index, new_index);

  // Update the order of child views.
  auto* reordered_view = mini_views_[new_index];
  reordered_view->parent()->ReorderChildView(reordered_view, new_index);

  Layout();

  // Call the animation function after reorder the mini views.
  PerformReorderDeskMiniViewAnimation(old_index, new_index, mini_views_);
  MaybeUpdateCombineDesksTooltips();
}

void DesksBarView::OnDeskActivationChanged(const Desk* activated,
                                           const Desk* deactivated) {
  for (auto* mini_view : mini_views_) {
    const Desk* desk = mini_view->desk();
    if (desk == activated || desk == deactivated)
      mini_view->UpdateFocusColor();
  }
}

void DesksBarView::OnDeskNameChanged(const Desk* desk,
                                     const std::u16string& new_name) {
  MaybeUpdateCombineDesksTooltips();
}

void DesksBarView::UpdateNewMiniViews(bool initializing_bar_view,
                                      bool expanding_bar_view) {
  const auto& desks = DesksController::Get()->desks();
  if (initializing_bar_view)
    UpdateDeskButtonsVisibility();
  if (IsZeroState() && !expanding_bar_view)
    return;
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
    UpdateDeskButtonsVisibility();
    PerformZeroStateToExpandedStateMiniViewAnimation(this);
    return;
  }

  Layout();

  if (initializing_bar_view)
    return;

  // We need to compile lists of the mini views on either side of the new mini
  // views so that they can be moved to make room for the new mini views in the
  // desks bar.
  auto left_partition_iter =
      base::ranges::find(mini_views_, new_mini_views.front());
  auto right_partition_iter =
      std::next(base::ranges::find(mini_views_, new_mini_views.back()));

  // A vector between `left_partition_iter` and `right_partition_iter` should be
  // the same as `new_mini_views` if they were added correctly.
  DCHECK(std::vector<DeskMiniView*>(left_partition_iter,
                                    right_partition_iter) == new_mini_views);

  PerformNewDeskMiniViewAnimation(
      new_mini_views,
      std::vector<DeskMiniView*>(mini_views_.begin(), left_partition_iter),
      std::vector<DeskMiniView*>(right_partition_iter, mini_views_.end()),
      expanded_state_new_desk_button_, expanded_state_desks_templates_button_,
      begin_x - GetFirstMiniViewXOffset());
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

void DesksBarView::OnNewDeskButtonPressed(
    DesksCreationRemovalSource desks_creation_removal_source) {
  auto* controller = DesksController::Get();
  if (!controller->CanCreateDesks())
    return;
  controller->NewDesk(desks_creation_removal_source);
  NudgeDeskName(mini_views_.size() - 1);
}

void DesksBarView::UpdateButtonsForSavedDeskGrid() {
  if (IsZeroState() || !saved_desk_util::IsSavedDesksEnabled())
    return;

  FindMiniViewForDesk(Shell::Get()->desks_controller()->active_desk())
      ->UpdateFocusColor();
  expanded_state_desks_templates_button_->set_active(
      overview_grid_->IsShowingSavedDeskLibrary());
  expanded_state_desks_templates_button_->UpdateFocusColor();
}

void DesksBarView::UpdateDeskButtonsVisibility() {
  const bool is_zero_state = IsZeroState();
  zero_state_default_desk_button_->SetVisible(is_zero_state);
  zero_state_new_desk_button_->SetVisible(is_zero_state);
  expanded_state_new_desk_button_->SetVisible(!is_zero_state);
  if (vertical_dots_button_)
    vertical_dots_button_->SetVisible(!is_zero_state);

  UpdateLibraryButtonVisibility();
}

void DesksBarView::UpdateLibraryButtonVisibility() {
  if (!saved_desk_util::IsSavedDesksEnabled())
    return;

  const bool should_show_ui = overview_grid_->overview_session()
                                  ->saved_desk_presenter()
                                  ->should_show_saved_desk_library();
  const bool is_zero_state = IsZeroState();

  zero_state_desks_templates_button_->SetVisible(should_show_ui &&
                                                 is_zero_state);
  expanded_state_desks_templates_button_->SetVisible(should_show_ui &&
                                                     !is_zero_state);

  // Removes the button from the tabbing order if it becomes invisible.
  auto* highlight_controller = GetHighlightController();
  if (!zero_state_desks_templates_button_->GetVisible()) {
    highlight_controller->OnViewDestroyingOrDisabling(
        zero_state_desks_templates_button_);
  }
  if (!expanded_state_desks_templates_button_->GetVisible()) {
    highlight_controller->OnViewDestroyingOrDisabling(
        expanded_state_desks_templates_button_->GetInnerButton());
  }

  const int begin_x = GetFirstMiniViewXOffset();
  Layout();

  if (mini_views_.empty())
    return;

  // The mini views and new desk button are already laid out in the earlier
  // `Layout()` call. This call shifts the transforms of the mini views and new
  // desk button and then animates to the identity transform.
  PerformLibraryButtonVisibilityAnimation(
      mini_views_,
      is_zero_state
          ? static_cast<views::View*>(zero_state_new_desk_button_)
          : static_cast<views::View*>(expanded_state_new_desk_button_),
      begin_x - GetFirstMiniViewXOffset());
}

DeskMiniView* DesksBarView::FindMiniViewForDesk(const Desk* desk) const {
  for (auto* mini_view : mini_views_) {
    if (mini_view->desk() == desk)
      return mini_view;
  }

  return nullptr;
}

void DesksBarView::SwitchToZeroState() {
  // Hiding the button immediately instead of the ends of the animation while
  // switching from expanded state to zero state.
  if (vertical_dots_button_)
    vertical_dots_button_->SetVisible(false);

  // In zero state, if the only desk is being dragged, we should end dragging.
  // Because the dragged desk's mini view is removed, the mouse released or
  // gesture ended events cannot be received. |drag_view_| will keep the stale
  // reference of removed mini view and |drag_proxy_| will not be reset.
  if (drag_view_)
    EndDragDesk(drag_view_, /*end_by_user=*/false);

  std::vector<DeskMiniView*> removed_mini_views = mini_views_;
  mini_views_.clear();

  auto* highlight_controller = GetHighlightController();
  OverviewHighlightableView* view = highlight_controller->highlighted_view();
  // Reset the highlight if it is highlighted on a descendant of `this`.
  if (view && Contains(view->GetView()))
    highlight_controller->ResetHighlightedView();

  // Keep current layout until the animation is completed since the animation
  // for going back to zero state is based on the expanded bar's current
  // layout.
  PerformExpandedStateToZeroStateMiniViewAnimation(this, removed_mini_views);
}

int DesksBarView::DetermineMoveIndex(int location_screen_x) const {
  const int views_size = static_cast<int>(mini_views_.size());

  // We find the target position according to the x-axis coordinate of the
  // desks' center positions in screen in ascending order.
  for (int new_index = 0; new_index != views_size - 1; ++new_index) {
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

  return views_size - 1;
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

int DesksBarView::GetFirstMiniViewXOffset() const {
  // GetMirroredX is used here to make sure the removing and adding a desk
  // transform is correct while in RTL layout.
  return mini_views_.empty() ? bounds().CenterPoint().x()
                             : mini_views_[0]->GetMirroredX();
}

void DesksBarView::UpdateScrollButtonsVisibility() {
  const gfx::Rect visible_bounds = scroll_view_->GetVisibleRect();
  left_scroll_button_->SetVisible(visible_bounds.x() > 0);
  right_scroll_button_->SetVisible(visible_bounds.right() <
                                   scroll_view_contents_->bounds().width());
}

void DesksBarView::UpdateGradientMask() {
  const bool is_rtl = base::i18n::IsRTL();
  const bool is_left_scroll_button_visible = left_scroll_button_->GetVisible();
  const bool is_right_scroll_button_visible =
      right_scroll_button_->GetVisible();
  const bool is_left_visible_only =
      is_left_scroll_button_visible && !is_right_scroll_button_visible;

  bool should_show_start_gradient = false;
  bool should_show_end_gradient = false;
  // Show the both sides gradients during scroll if the corresponding scroll
  // button is visible. Otherwise, show the start/end gradient only in last page
  // and show the end/start gradient if there are contents beyond the right/left
  // side of the visible bounds with LTR/RTL layout.
  if (scroll_view_->is_scrolling()) {
    should_show_start_gradient =
        is_rtl ? is_right_scroll_button_visible : is_left_scroll_button_visible;
    should_show_end_gradient =
        is_rtl ? is_left_scroll_button_visible : is_right_scroll_button_visible;
  } else {
    should_show_start_gradient =
        is_rtl ? is_right_scroll_button_visible : is_left_visible_only;
    should_show_end_gradient =
        is_rtl ? is_left_visible_only : is_right_scroll_button_visible;
  }

  // The bounds of the start and end gradient will be the same regardless it is
  // LTR or RTL layout. While the |left_scroll_button_| will be changed from
  // left to right and |right_scroll_button_| will be changed from right to left
  // if it is RTL layout.

  // Horizontal linear gradient, from left to right.
  gfx::LinearGradient gradient_mask(/*angle=*/0);

  // Fraction of layer width that gradient will be applied to.
  const float fade_position =
      should_show_start_gradient || should_show_end_gradient
          ? static_cast<float>(kGradientZoneLength) /
                scroll_view_->bounds().width()
          : 0;

  // Left fade in section.
  if (should_show_start_gradient) {
    gradient_mask.AddStep(/*fraction=*/0, /*alpha=*/0);
    gradient_mask.AddStep(fade_position, 255);
  }
  // Right fade out section.
  if (should_show_end_gradient) {
    gradient_mask.AddStep((1 - fade_position), 255);
    gradient_mask.AddStep(1, 0);
  }

  scroll_view_->layer()->SetGradientMask(gradient_mask);
  scroll_view_->SchedulePaint();
}

void DesksBarView::ScrollToPreviousPage() {
  ui::ScopedLayerAnimationSettings settings(
      scroll_view_contents_->layer()->GetAnimator());
  InitScrollContentsAnimationSettings(settings);
  scroll_view_->ScrollToPosition(
      scroll_view_->horizontal_scroll_bar(),
      GetAdjustedUncroppedScrollPosition(scroll_view_->GetVisibleRect().x() -
                                         scroll_view_->width()));
}

void DesksBarView::ScrollToNextPage() {
  ui::ScopedLayerAnimationSettings settings(
      scroll_view_contents_->layer()->GetAnimator());
  InitScrollContentsAnimationSettings(settings);
  scroll_view_->ScrollToPosition(
      scroll_view_->horizontal_scroll_bar(),
      GetAdjustedUncroppedScrollPosition(scroll_view_->GetVisibleRect().x() +
                                         scroll_view_->width()));
}

int DesksBarView::GetAdjustedUncroppedScrollPosition(int position) const {
  // Let the ScrollView handle it if the given |position| is invalid or it can't
  // be adjusted.
  if (position <= 0 || position >= scroll_view_contents_->bounds().width() -
                                       scroll_view_->width()) {
    return position;
  }

  int adjusted_position = position;
  int i = 0;
  gfx::Rect mini_view_bounds;
  const int mini_views_size = static_cast<int>(mini_views_.size());
  for (; i < mini_views_size; i++) {
    mini_view_bounds = mini_views_[i]->bounds();

    // Return early if there is no desk preview cropped at the start position.
    if (mini_view_bounds.x() >= position)
      return position - kDeskPreviewViewFocusRingThicknessAndPadding;

    if (mini_view_bounds.x() < position && mini_view_bounds.right() > position)
      break;
  }

  DCHECK_LT(i, mini_views_size);
  if ((position - mini_view_bounds.x()) < mini_view_bounds.width() / 2) {
    adjusted_position = mini_view_bounds.x();
  } else {
    adjusted_position = mini_view_bounds.right();
    if (i + 1 < mini_views_size)
      adjusted_position = mini_views_[i + 1]->bounds().x();
  }
  return adjusted_position - kDeskPreviewViewFocusRingThicknessAndPadding;
}

void DesksBarView::OnLibraryButtonPressed() {
  RecordLoadSavedDeskLibraryHistogram();
  if (IsDeskNameBeingModified())
    DeskNameView::CommitChanges(GetWidget());
  overview_grid_->overview_session()->ShowSavedDeskLibrary(
      base::GUID(), /*saved_desk_name=*/u"",
      GetWidget()->GetNativeWindow()->GetRootWindow());
}

void DesksBarView::MaybeUpdateCombineDesksTooltips() {
  if (!features::IsDesksCloseAllEnabled())
    return;

  for (auto* mini_view : mini_views_) {
    mini_view->desk_action_view()->UpdateCombineDesksTooltip(
        DesksController::Get()->GetCombineDesksTargetName(mini_view->desk()));
  }
}

void DesksBarView::OnContentsScrolled() {
  UpdateScrollButtonsVisibility();
  UpdateGradientMask();
}

void DesksBarView::OnContentsScrollEnded() {
  const gfx::Rect visible_bounds = scroll_view_->GetVisibleRect();
  const int current_position = visible_bounds.x();
  const int adjusted_position =
      GetAdjustedUncroppedScrollPosition(current_position);
  if (current_position != adjusted_position) {
    scroll_view_->ScrollToPosition(scroll_view_->horizontal_scroll_bar(),
                                   adjusted_position);
  }
  UpdateGradientMask();
}

void DesksBarView::NudgeDeskName(int desk_index) {
  DCHECK_LT(desk_index, static_cast<int>(mini_views_.size()));

  auto* name_view = mini_views_[desk_index]->desk_name_view();
  name_view->RequestFocus();

  // Set `name_view`'s accessible name to the default desk name since its text
  // is cleared.
  if (name_view->GetAccessibleName().empty()) {
    name_view->SetAccessibleName(
        DesksController::GetDeskDefaultName(desk_index));
  }

  UpdateOverviewHighlightForFocus(name_view);

  // If we're in tablet mode and there are no external keyboards, open up the
  // virtual keyboard.
  if (Shell::Get()->tablet_mode_controller()->InTabletMode() &&
      !HasExternalKeyboard()) {
    keyboard::KeyboardUIController::Get()->ShowKeyboard(/*lock=*/false);
  }
}

}  // namespace ash

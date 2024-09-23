// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/wide_frame_view.h"
#include <memory>

#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "chromeos/ui/frame/header_view.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/caption_button_layout_constants.h"

namespace ash {
namespace {

using ::chromeos::ImmersiveFullscreenController;

class WideFrameTargeter : public aura::WindowTargeter {
 public:
  explicit WideFrameTargeter(chromeos::HeaderView* header_view)
      : header_view_(header_view) {}

  WideFrameTargeter(const WideFrameTargeter&) = delete;
  WideFrameTargeter& operator=(const WideFrameTargeter&) = delete;

  ~WideFrameTargeter() override = default;

  // aura::WindowTargeter:
  bool GetHitTestRects(aura::Window* target,
                       gfx::Rect* hit_test_rect_mouse,
                       gfx::Rect* hit_test_rect_touch) const override {
    if (header_view_->in_immersive_mode() && !header_view_->is_revealed()) {
      aura::Window* source = header_view_->GetWidget()->GetNativeWindow();
      *hit_test_rect_mouse = source->bounds();
      aura::Window::ConvertRectToTarget(source, target->parent(),
                                        hit_test_rect_mouse);
      hit_test_rect_mouse->set_y(target->bounds().y());
      hit_test_rect_mouse->set_height(1);
      hit_test_rect_touch->SetRect(0, 0, 0, 0);
      return true;
    }
    return aura::WindowTargeter::GetHitTestRects(target, hit_test_rect_mouse,
                                                 hit_test_rect_touch);
  }

 private:
  raw_ptr<chromeos::HeaderView> header_view_;
};

}  // namespace

// static
gfx::Rect WideFrameView::GetFrameBounds(views::Widget* target) {
  static const int kFrameHeight =
      views::GetCaptionButtonLayoutSize(
          views::CaptionButtonLayoutSize::kNonBrowserCaption)
          .height();
  aura::Window* target_window = target->GetNativeWindow();
  gfx::Rect bounds =
      screen_util::GetIdealBoundsForMaximizedOrFullscreenOrPinnedState(
          target_window);

  bounds.set_height(kFrameHeight);
  return bounds;
}

void WideFrameView::Init(ImmersiveFullscreenController* controller) {
  DCHECK(target_);
  controller->Init(this, target_, header_view_);
}

void WideFrameView::SetCaptionButtonModel(
    std::unique_ptr<chromeos::CaptionButtonModel> model) {
  header_view_->caption_button_container()->SetModel(std::move(model));
  header_view_->UpdateCaptionButtons();
}

WideFrameView::WideFrameView(views::Widget* target)
    : target_(target),
      frame_context_menu_controller_(
          std::make_unique<FrameContextMenuController>(target_, this)) {
  // WideFrameView is owned by its client, not by Views.
  SetOwnedByWidget(false);

  aura::Window* target_window = target->GetNativeWindow();
  target_window->AddObserver(this);
  // Use the HeaderView itself as a frame view because WideFrameView is
  // is the frame only.
  header_view_ = AddChildView(
      std::make_unique<chromeos::HeaderView>(target, /*frame view=*/nullptr));
  header_view_->Init();
  GetTargetHeaderView()->SetShouldPaintHeader(false);
  header_view_->set_context_menu_controller(
      frame_context_menu_controller_.get());

  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.delegate = this;
  params.bounds = GetFrameBounds(target);
  params.name = "WideFrameView";
  params.parent = target->GetNativeWindow();
  // Setup Opacity Control.
  // WideFrame should be used only when the rounded corner is not necessary.
  params.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  widget_ = std::move(widget);

  aura::Window* window = widget_->GetNativeWindow();
  // Overview normally clips the caption container which exists on the same
  // window. But this WideFrameView exists as a separate window, which we hide
  // in overview using the `kHideInOverviewKey` property. However, we still want
  // to show it in the desks mini_views.
  window->SetProperty(kHideInOverviewKey, true);
  window->SetProperty(kForceVisibleInMiniViewKey, true);
  window->SetEventTargeter(std::make_unique<WideFrameTargeter>(header_view()));
  set_owned_by_client();
  WindowState::Get(window)->set_allow_set_bounds_direct(true);

  paint_as_active_subscription_ =
      target_->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &WideFrameView::PaintAsActiveChanged, base::Unretained(this)));
  PaintAsActiveChanged();
}

WideFrameView::~WideFrameView() {
  header_view_->set_context_menu_controller(nullptr);
  if (widget_)
    widget_->CloseNow();
  if (target_) {
    chromeos::HeaderView* target_header_view = GetTargetHeaderView();
    target_header_view->SetShouldPaintHeader(true);
    target_header_view->GetFrameHeader()->UpdateFrameHeaderKey();
    target_->GetNativeWindow()->RemoveObserver(this);
  }
}

void WideFrameView::Layout(PassKey) {
  int onscreen_height = header_view_->GetPreferredOnScreenHeight();
  if (onscreen_height == 0 || !GetVisible()) {
    header_view_->SetVisible(false);
  } else {
    const int height = header_view_->GetPreferredHeight();
    header_view_->SetBounds(0, onscreen_height - height, width(), height);
    header_view_->SetVisible(true);
  }
}

void WideFrameView::OnMouseEvent(ui::MouseEvent* event) {
  if (event->IsOnlyLeftMouseButton()) {
    if ((event->flags() & ui::EF_IS_DOUBLE_CLICK)) {
      base::RecordAction(
          base::UserMetricsAction("Caption_ClickTogglesMaximize"));
      const WMEvent wm_event(WM_EVENT_TOGGLE_MAXIMIZE_CAPTION);
      WindowState::Get(target_->GetNativeWindow())->OnWMEvent(&wm_event);
    }
    event->SetHandled();
  }
}

void WideFrameView::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  target_ = nullptr;
}

void WideFrameView::OnDisplayMetricsChanged(const display::Display& display,
                                            uint32_t changed_metrics) {
  aura::Window* target_window = target_->GetNativeWindow();
  // The target window is detached from the tree when the window is being moved
  // to another desks, or another display, and it may trigger the display
  // metrics change if the target window is in fullscreen. Do not try to layout
  // in this case. The it'll be layout when the window is added back.
  // (crbug.com/1267128)
  if (!target_window->GetRootWindow())
    return;

  display::Screen* screen = display::Screen::GetScreen();
  if (screen->GetDisplayNearestWindow(target_->GetNativeWindow()).id() !=
          display.id() ||
      !widget_) {
    return;
  }
  DCHECK(target_);

  // Ignore if this is called when the targe window state is nor proper
  // state. This can happen when switching the state to other states, in which
  // case, the wide frame will be removed.
  if (!WindowState::Get(target_window)->IsMaximizedOrFullscreenOrPinned()) {
    return;
  }

  GetWidget()->SetBounds(GetFrameBounds(target_));
}

void WideFrameView::OnImmersiveRevealStarted() {
  header_view_->OnImmersiveRevealStarted();
}

void WideFrameView::OnImmersiveRevealEnded() {
  header_view_->OnImmersiveRevealEnded();
}

void WideFrameView::OnImmersiveFullscreenEntered() {
  header_view_->OnImmersiveFullscreenEntered();
  widget_->GetNativeWindow()->SetTransparent(true);
  if (target_)
    GetTargetHeaderView()->OnImmersiveFullscreenEntered();
}

void WideFrameView::OnImmersiveFullscreenExited() {
  header_view_->OnImmersiveFullscreenExited();
  widget_->GetNativeWindow()->SetTransparent(false);
  if (target_)
    GetTargetHeaderView()->OnImmersiveFullscreenExited();
  DeprecatedLayoutImmediately();
}

void WideFrameView::SetVisibleFraction(double visible_fraction) {
  header_view_->SetVisibleFraction(visible_fraction);
}

std::vector<gfx::Rect> WideFrameView::GetVisibleBoundsInScreen() const {
  return header_view_->GetVisibleBoundsInScreen();
}

bool WideFrameView::ShouldShowContextMenu(
    View* source,
    const gfx::Point& screen_coords_point) {
  gfx::Point point_in_header_coords(screen_coords_point);
  views::View::ConvertPointToTarget(this, header_view_,
                                    &point_in_header_coords);
  return header_view_->HitTestRect(
      gfx::Rect(point_in_header_coords, gfx::Size(1, 1)));
}

chromeos::HeaderView* WideFrameView::GetTargetHeaderView() {
  auto* frame_view = static_cast<NonClientFrameViewAsh*>(
      target_->non_client_view()->frame_view());
  return frame_view->GetHeaderView();
}

void WideFrameView::PaintAsActiveChanged() {
  header_view_->GetFrameHeader()->SetPaintAsActive(
      target_->ShouldPaintAsActive());
}

}  // namespace ash

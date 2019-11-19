// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/ime_mode_indicator_view.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

// Minimum size of inner contents in pixel.
// 43 is the designed size including the default margin (6 * 2).
const int kMinSize = 31;

// After this duration in msec, the mode inicator will be fading out.
const int kShowingDuration = 500;

class ModeIndicatorFrameView : public views::BubbleFrameView {
 public:
  explicit ModeIndicatorFrameView()
      : views::BubbleFrameView(gfx::Insets(), gfx::Insets()) {}
  ~ModeIndicatorFrameView() override {}

 private:
  // views::BubbleFrameView overrides:
  gfx::Rect GetAvailableScreenBounds(const gfx::Rect& rect) const override {
    return display::Screen::GetScreen()
        ->GetDisplayNearestPoint(rect.CenterPoint())
        .bounds();
  }

  DISALLOW_COPY_AND_ASSIGN(ModeIndicatorFrameView);
};

}  // namespace

ImeModeIndicatorView::ImeModeIndicatorView(const gfx::Rect& cursor_bounds,
                                           const base::string16& label)
    : cursor_bounds_(cursor_bounds), label_view_(new views::Label(label)) {
  SetCanActivate(false);
  set_accept_events(false);
  set_shadow(views::BubbleBorder::BIG_SHADOW);
  SetArrow(views::BubbleBorder::TOP_CENTER);
}

ImeModeIndicatorView::~ImeModeIndicatorView() = default;

void ImeModeIndicatorView::ShowAndFadeOut() {
  ::wm::SetWindowVisibilityAnimationTransition(GetWidget()->GetNativeView(),
                                               ::wm::ANIMATE_HIDE);
  GetWidget()->Show();
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kShowingDuration),
               GetWidget(), &views::Widget::Close);
}

void ImeModeIndicatorView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  aura::Window* window = window_util::GetActiveWindow();
  if (window) {  // Null check for tests.
    params->parent = Shell::GetContainer(window->GetRootWindow(),
                                         kShellWindowId_SettingBubbleContainer);
  } else {
    params->parent = Shell::GetPrimaryRootWindow();
  }
}

gfx::Size ImeModeIndicatorView::CalculatePreferredSize() const {
  gfx::Size size = label_view_->GetPreferredSize();
  size.SetToMax(gfx::Size(kMinSize, kMinSize));
  return size;
}

const char* ImeModeIndicatorView::GetClassName() const {
  return "ImeModeIndicatorView";
}

int ImeModeIndicatorView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void ImeModeIndicatorView::Init() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(label_view_);

  SetAnchorRect(cursor_bounds_);
}

views::NonClientFrameView* ImeModeIndicatorView::CreateNonClientFrameView(
    views::Widget* widget) {
  views::BubbleFrameView* frame = new ModeIndicatorFrameView();
  // arrow adjustment in BubbleDialogDelegateView is unnecessary because arrow
  // of this bubble is always center.
  frame->SetBubbleBorder(std::unique_ptr<views::BubbleBorder>(
      new views::BubbleBorder(arrow(), GetShadow(), color())));
  return frame;
}

}  // namespace ash

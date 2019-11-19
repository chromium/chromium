// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell/example_factory.h"
#include "ash/shell/example_session_controller_client.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace shell {

class LockView : public views::WidgetDelegateView,
                 public views::ButtonListener {
 public:
  LockView() : text_(new views::Label(base::ASCIIToUTF16("LOCKED!"))) {
    text_->SetEnabledColor(SK_ColorRED);
    AddChildView(text_);
    unlock_button_ = AddChildView(
        views::MdTextButton::Create(this, base::ASCIIToUTF16("Unlock")));
  }
  ~LockView() override = default;

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(500, 400);
  }

 private:
  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    canvas->FillRect(GetLocalBounds(), SK_ColorYELLOW);
  }

  void Layout() override {
    gfx::Rect bounds = GetLocalBounds();
    gfx::Size ts = text_->GetPreferredSize();
    text_->SetBoundsRect(gfx::Rect((bounds.width() - ts.width()) / 2,
                                   (bounds.height() - ts.height()) / 2,
                                   ts.width(), ts.height()));

    gfx::Size ps = unlock_button_->GetPreferredSize();
    bounds.set_y(bounds.bottom() - ps.height() - 5);
    bounds.set_x((bounds.width() - ps.width()) / 2);
    bounds.set_size(ps);
    unlock_button_->SetBoundsRect(bounds);
  }

  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override {
    if (details.is_add && details.child == this)
      unlock_button_->RequestFocus();
  }

  // Overridden from views::WidgetDelegateView:
  void WindowClosing() override {
    ExampleSessionControllerClient::Get()->UnlockScreen();
  }

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK(sender == unlock_button_);
    GetWidget()->Close();
  }

  views::Label* text_;
  views::Button* unlock_button_;

  DISALLOW_COPY_AND_ASSIGN(LockView);
};

void CreateLockScreen() {
  LockView* lock_view = new LockView;
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  gfx::Size ps = lock_view->GetPreferredSize();

  gfx::Size root_window_size = Shell::GetPrimaryRootWindow()->bounds().size();
  params.bounds = gfx::Rect((root_window_size.width() - ps.width()) / 2,
                            (root_window_size.height() - ps.height()) / 2,
                            ps.width(), ps.height());
  params.delegate = lock_view;
  params.parent = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                      kShellWindowId_LockScreenContainer);
  widget->Init(std::move(params));
  widget->Show();
  widget->GetNativeView()->SetName("LockView");
  widget->GetNativeView()->Focus();

  // TODO: it shouldn't be necessary to invoke UpdateTooltip() here.
  Shell::Get()->tooltip_controller()->UpdateTooltip(widget->GetNativeView());
}

}  // namespace shell
}  // namespace ash

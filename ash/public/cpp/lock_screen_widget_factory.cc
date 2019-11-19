// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/lock_screen_widget_factory.h"

#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

class LockScreenWidgetDelegate : public views::WidgetDelegate {
 public:
  explicit LockScreenWidgetDelegate(views::Widget* widget) : widget_(widget) {
    DCHECK(widget_);
  }
  ~LockScreenWidgetDelegate() override = default;

  // views::WidgetDelegate:
  views::View* GetInitiallyFocusedView() override {
    return widget_->GetContentsView();
  }
  views::Widget* GetWidget() override { return widget_; }
  const views::Widget* GetWidget() const override { return widget_; }
  void DeleteDelegate() override { delete this; }

 private:
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenWidgetDelegate);
};

}  // namespace

std::unique_ptr<views::Widget> CreateLockScreenWidget(aura::Window* parent) {
  std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  // Owned by Widget.
  params.delegate = new LockScreenWidgetDelegate(widget.get());
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = parent;
  widget->Init(std::move(params));
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  return widget;
}

}  // namespace ash

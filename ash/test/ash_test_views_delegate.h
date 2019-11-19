// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_VIEWS_DELEGATE_H_
#define ASH_TEST_ASH_TEST_VIEWS_DELEGATE_H_

#include "base/macros.h"
#include "ui/views/test/test_views_delegate.h"

namespace ash {

// Ash specific ViewsDelegate. In addition to creating a TestWebContents this
// parents widget with no parent/context to the shell. This is created by
// default AshTestHelper.
class AshTestViewsDelegate : public views::TestViewsDelegate {
 public:
  AshTestViewsDelegate();
  ~AshTestViewsDelegate() override;

  // Overriden from TestViewsDelegate.
  void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) override;
  views::TestViewsDelegate::ProcessMenuAcceleratorResult
  ProcessAcceleratorWhileMenuShowing(
      const ui::Accelerator& accelerator) override;

  void set_close_menu_accelerator(const ui::Accelerator& accelerator) {
    close_menu_accelerator_ = accelerator;
  }

 private:
  // ProcessAcceleratorWhileMenuShowing returns CLOSE_MENU if passed accelerator
  // matches with this.
  ui::Accelerator close_menu_accelerator_;

  DISALLOW_COPY_AND_ASSIGN(AshTestViewsDelegate);
};

}  // namespace ash

#endif  // ASH_TEST_ASH_TEST_VIEWS_DELEGATE_H_

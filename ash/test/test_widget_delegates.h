// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_WIDGET_DELEGATES_H_
#define ASH_TEST_TEST_WIDGET_DELEGATES_H_

#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/test/test_widget_builder.h"

namespace views {

class WidgetDelegate;

}  // namespace views

namespace ash {

// A bubble dialog delegates that centers itself to its anchor widget.
class CenteredBubbleDialogModelHost : public views::BubbleDialogModelHost {
 public:
  CenteredBubbleDialogModelHost(views::Widget* anchor,
                                const gfx::Size& size,
                                bool close_on_deactivate);
  CenteredBubbleDialogModelHost(const CenteredBubbleDialogModelHost&) = delete;
  CenteredBubbleDialogModelHost& operator=(
      const CenteredBubbleDialogModelHost&) = delete;
  ~CenteredBubbleDialogModelHost() override = default;

 private:
  gfx::Rect GetDesiredBounds() const;

  gfx::Size size_;
};

// Creates a test widget delegate that
// 1) makes the window resizable, maximizable and minimizale.
// 2) creates an ash's window frame.
views::WidgetDelegate* CreateTestWidgetBuilderDelegate();

// Creates a test widget builder with a above delegate.
views::test::TestWidgetBuilder CreateWidgetBuilderWithDelegate(
    views::test::WidgetBuilderParams params = {});

}  // namespace ash

#endif  // ASH_TEST_TEST_WIDGET_DELEGATES_H_

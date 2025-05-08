// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_WIDGET_DELEGATES_H_
#define ASH_TEST_TEST_WIDGET_DELEGATES_H_

#include "ui/views/bubble/bubble_dialog_model_host.h"

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

}  // namespace ash

#endif  // ASH_TEST_TEST_WIDGET_DELEGATES_H_

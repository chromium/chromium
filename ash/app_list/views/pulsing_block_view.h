// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_PULSING_BLOCK_VIEW_H_
#define ASH_APP_LIST_VIEWS_PULSING_BLOCK_VIEW_H_

#include "base/compiler_specific.h"
#include "base/timer/timer.h"
#include "ui/views/view.h"

namespace gfx {
class Size;
}

namespace ash {

// PulsingBlockView shows a pulsing white block via layer animation.
class PulsingBlockView : public views::View {
 public:
  // Constructs a PulsingBlockView of |size|. Starts the pulsing animation after
  // a |animation_delay|.
  PulsingBlockView(const gfx::Size& size, base::TimeDelta animation_delay);

  PulsingBlockView(const PulsingBlockView&) = delete;
  PulsingBlockView& operator=(const PulsingBlockView&) = delete;

  ~PulsingBlockView() override;

  // views::View:
  const char* GetClassName() const override;
  void OnThemeChanged() override;

  // Schedules the animation again from the beginning.
  void ResetAnimation();

 private:
  void OnStartDelayTimer();

  // views::View overrides:
  void OnPaint(gfx::Canvas* canvas) override;

  base::OneShotTimer start_delay_timer_;

  views::View* background_color_view_ = nullptr;
  const gfx::Size block_size_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_PULSING_BLOCK_VIEW_H_

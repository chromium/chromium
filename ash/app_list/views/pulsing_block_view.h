// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_PULSING_BLOCK_VIEW_H_
#define ASH_APP_LIST_VIEWS_PULSING_BLOCK_VIEW_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/views/view.h"

namespace gfx {
class Size;
}

namespace ash {

// PulsingBlockView shows a pulsing white block via layer animation.
class PulsingBlockView : public views::View {
 public:
  // Constructs a PulsingBlockView of |size|. If |start_delay| is true,
  // starts the pulsing animation after a random delay.
  PulsingBlockView(const gfx::Size& size, bool start_delay);
  ~PulsingBlockView() override;

  // views::View:
  const char* GetClassName() const override;

 private:
  void OnStartDelayTimer();

  // views::View overrides:
  void OnPaint(gfx::Canvas* canvas) override;

  base::OneShotTimer start_delay_timer_;

  DISALLOW_COPY_AND_ASSIGN(PulsingBlockView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_PULSING_BLOCK_VIEW_H_

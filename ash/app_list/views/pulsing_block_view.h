// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_PULSING_BLOCK_VIEW_H_
#define ASH_APP_LIST_VIEWS_PULSING_BLOCK_VIEW_H_

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace gfx {
class Size;
}

namespace ash {

// PulsingBlockView shows a pulsing white circle via layer animation.
class PulsingBlockView : public views::View {
  METADATA_HEADER(PulsingBlockView, views::View)

 public:
  // Constructs a PulsingBlockView of |size|. Starts the pulsing animation after
  // a |animation_delay|.
  PulsingBlockView(const gfx::Size& size,
                   base::TimeDelta animation_delay,
                   float corner_radius);

  PulsingBlockView(const PulsingBlockView&) = delete;
  PulsingBlockView& operator=(const PulsingBlockView&) = delete;

  ~PulsingBlockView() override;

  // views::View:
  void OnThemeChanged() override;

  // Returns true if the view has a layer animator attached and is currently
  // running.
  bool IsAnimating();

  // Starts the animation by immediately firing `start_delay_timer`. Returns
  // false if the timer was not running.
  bool FireAnimationTimerForTest();

 private:
  void OnStartDelayTimer();

  base::OneShotTimer start_delay_timer_;

  raw_ptr<views::View> background_color_view_ = nullptr;

  const gfx::Size block_size_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_PULSING_BLOCK_VIEW_H_

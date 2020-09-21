// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_UNIFIED_MEDIA_CONTROLS_CONTAINER_H_
#define ASH_SYSTEM_MEDIA_UNIFIED_MEDIA_CONTROLS_CONTAINER_H_

#include "ui/views/view.h"

namespace ash {

// Container view of UnifiedMediaControlsView. This manages the
// visibility and expanded amount of the entire media controls view.
class UnifiedMediaControlsContainer : public views::View {
 public:
  UnifiedMediaControlsContainer();
  ~UnifiedMediaControlsContainer() override = default;

  void SetShouldShowMediaControls(bool should_show);
  void SetExpandedAmount(double expanded_amount);
  int GetExpandedHeight() const;

  // views::View
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;

 private:
  double expanded_amount_;

  bool should_show_media_controls_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_UNIFIED_MEDIA_CONTROLS_CONTAINER_H_

// Copyright 2020 The Chromium Authors
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

  // Set |should_show_media_controls_|.
  void SetShouldShowMediaControls(bool should_show);

  // Show media controls if necessary. Returns true if media controls
  // will be shown.
  bool MaybeShowMediaControls();

  // Set |expanded_amount_| and hide/show media controls correspondingly.
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

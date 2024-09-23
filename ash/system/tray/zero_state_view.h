// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_ZERO_STATE_VIEW_H_
#define ASH_SYSTEM_TRAY_ZERO_STATE_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

// A generic zero-state view that displays an illustration, a title, and a
// subtitle. This view will be used in the TrayDetailedView for Bluetooth,
// Network, Cast, etc when the detailed view has no available options to display
// to the user.
class ASH_EXPORT ZeroStateView : public views::View {
  METADATA_HEADER(ZeroStateView, views::View)
 public:
  ZeroStateView(const int image_id, const int title_id, const int subtitle_id);

  ZeroStateView(const ZeroStateView&) = delete;
  ZeroStateView& operator=(const ZeroStateView&) = delete;

  ~ZeroStateView() override = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_ZERO_STATE_VIEW_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_HUD_HEADER_VIEW_H_
#define ASH_HUD_DISPLAY_HUD_HEADER_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {
namespace hud_display {

class HUDDisplayView;
class HUDTabStrip;

// HUDHeaderView renders header (with buttons and tabs) of the HUD.
class HUDHeaderView : public views::BoxLayoutView {
  METADATA_HEADER(HUDHeaderView, views::BoxLayoutView)

 public:
  explicit HUDHeaderView(HUDDisplayView* hud);

  HUDHeaderView(const HUDHeaderView&) = delete;
  HUDHeaderView& operator=(const HUDHeaderView&) = delete;

  ~HUDHeaderView() override;

  HUDTabStrip* tab_strip() { return tab_strip_; }

 private:
  raw_ptr<HUDTabStrip> tab_strip_ = nullptr;  // not owned
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_HUD_HEADER_VIEW_H_

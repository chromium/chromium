// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLOOM_BLOOM_TRAY_H_
#define ASH_SYSTEM_BLOOM_BLOOM_TRAY_H_

#include "ash/system/tray/tray_background_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace views {
class ImageView;
}

namespace ash {

class BloomTray : public TrayBackgroundView {
 public:
  METADATA_HEADER(BloomTray);

  explicit BloomTray(Shelf* shelf);
  BloomTray(const BloomTray&) = delete;
  BloomTray& operator=(const BloomTray&) = delete;
  ~BloomTray() override;

  // TrayBackgroundView:
  void Initialize() override;
  base::string16 GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;
  bool PerformAction(const ui::Event& event) override;

 private:
  void SetIcon();
  void SetTooltipText();

  // Weak pointer, will be parented by TrayContainer for its lifetime.
  views::ImageView* const icon_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLOOM_BLOOM_TRAY_H_

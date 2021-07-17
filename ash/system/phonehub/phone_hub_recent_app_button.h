// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_RECENT_APP_BUTTON_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_RECENT_APP_BUTTON_H_

#include "ash/ash_export.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

// A recent app button containing an |AppMetadata|.
class ASH_EXPORT PhoneHubRecentAppButton : public views::ImageButton {
 public:
  PhoneHubRecentAppButton();
  ~PhoneHubRecentAppButton() override;
  PhoneHubRecentAppButton(PhoneHubRecentAppButton&) = delete;
  PhoneHubRecentAppButton operator=(PhoneHubRecentAppButton&) = delete;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  // views::ImageButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;
  void OnThemeChanged() override;

 private:
  void ButtonPressed();
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_RECENT_APP_BUTTON_H_

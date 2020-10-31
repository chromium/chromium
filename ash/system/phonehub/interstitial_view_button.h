// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_INTERSTITIAL_VIEW_BUTTON_H_
#define ASH_SYSTEM_PHONEHUB_INTERSTITIAL_VIEW_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/system/unified/rounded_label_button.h"
#include "ui/views/controls/button/button.h"

namespace ash {

// Button to be shown on Phone Hub interstital views. It's focusable by default
// and shares common setup with system menu buttons. It can have a gray rounded
// rectangle background if |paint_background| is true.
class ASH_EXPORT InterstitialViewButton : public RoundedLabelButton {
 public:
  METADATA_HEADER(InterstitialViewButton);

  InterstitialViewButton(Button::PressedCallback callback,
                         const base::string16& text,
                         bool paint_background);
  InterstitialViewButton(const InterstitialViewButton&) = delete;
  InterstitialViewButton& operator=(const InterstitialViewButton&) = delete;
  ~InterstitialViewButton() override;

  // views::RoundedLabelButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  // True if the button needs a gray rectangle background.
  bool paint_background_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_INTERSTITIAL_VIEW_BUTTON_H_

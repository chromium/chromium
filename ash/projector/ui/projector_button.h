// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_UI_PROJECTOR_BUTTON_H_
#define ASH_PROJECTOR_UI_PROJECTOR_BUTTON_H_

#include <string>

#include "ash/ash_export.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

// Projector button base class, which handles common styles (rounded background,
// size, etc) and ink drop. The button is also toggleable. This is used by
// |ProjectorColorButton|, |ProjectorImageButton|, etc.
class ASH_EXPORT ProjectorButton : public views::ToggleImageButton {
  METADATA_HEADER(ProjectorButton, views::ToggleImageButton)

 public:
  const int kProjectorButtonSize = 32;
  const int kProjectorButtonBorderSize = 1;

  ProjectorButton(views::Button::PressedCallback callback,
                  const std::u16string& name);
  ProjectorButton(const ProjectorButton&) = delete;
  ProjectorButton& operator=(const ProjectorButton&) = delete;
  ~ProjectorButton() override = default;

  // views::ToggleImageButton:
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;
};

}  // namespace ash

#endif  // ASH_PROJECTOR_UI_PROJECTOR_BUTTON_H_

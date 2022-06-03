// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/ui/projector_color_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"

namespace ash {

ProjectorColorButton::ProjectorColorButton(
    views::Button::PressedCallback callback,
    SkColor color,
    int size,
    float radius,
    const std::u16string& name)
    : ProjectorButton(callback, name) {
  // Add the color view.
  auto* color_view = AddChildView(std::make_unique<View>());
  color_view->SetBounds((kProjectorButtonSize - size) / 2,
                        (kProjectorButtonSize - size) / 2, size, size);
  color_view->SetBackground(CreateBackgroundFromPainter(
      views::Painter::CreateSolidRoundRectPainter(color, radius)));
}

}  // namespace ash

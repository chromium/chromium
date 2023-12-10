// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/ui/projector_color_button.h"

#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"

namespace ash {

ProjectorColorButton::ProjectorColorButton(
    views::Button::PressedCallback callback,
    SkColor color,
    int size,
    float radius,
    const std::u16string& name)
    : ProjectorButton(std::move(callback), name), color_(color), size_(size) {}

void ProjectorColorButton::PaintButtonContents(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(color_);
  const gfx::RectF bounds(GetContentsBounds());
  canvas->DrawCircle(bounds.CenterPoint(), (kProjectorButtonSize - size_) / 2,
                     flags);
}

BEGIN_METADATA(ProjectorColorButton)
END_METADATA

}  // namespace ash

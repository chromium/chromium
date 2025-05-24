// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/lobster/lobster_text_input_context.h"

#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

LobsterTextInputContext::LobsterTextInputContext(
    ui::TextInputType text_input_type,
    const gfx::Rect& caret_bounds,
    bool can_insert_image)
    : text_input_type(text_input_type),
      caret_bounds(caret_bounds),
      support_image_insertion(can_insert_image) {}

LobsterTextInputContext::LobsterTextInputContext()
    : LobsterTextInputContext(
          /*text_input_type=*/ui::TextInputType::TEXT_INPUT_TYPE_NONE,
          /*caret_bounds=*/gfx::Rect(),
          /*can_insert_image=*/false) {}

LobsterTextInputContext::~LobsterTextInputContext() = default;

}  // namespace ash

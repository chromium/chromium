// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOBSTER_LOBSTER_TEXT_INPUT_CONTEXT_H_
#define ASH_PUBLIC_CPP_LOBSTER_LOBSTER_TEXT_INPUT_CONTEXT_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

struct ASH_PUBLIC_EXPORT LobsterTextInputContext {
  ui::TextInputType text_input_type;
  gfx::Rect caret_bounds;
  bool support_image_insertion;

  LobsterTextInputContext(ui::TextInputType text_input_type,
                          const gfx::Rect& caret_bounds,
                          bool can_insert_image);
  LobsterTextInputContext();
  ~LobsterTextInputContext();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOBSTER_LOBSTER_TEXT_INPUT_CONTEXT_H_

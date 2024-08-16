// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/palette_ids.h"
#include "base/notreached.h"

namespace ash {

PaletteTrayOptions PaletteToolIdToPaletteTrayOptions(PaletteToolId tool_id) {
  switch (tool_id) {
    case PaletteToolId::NONE:
      return PALETTE_OPTIONS_COUNT;
    case PaletteToolId::CREATE_NOTE:
      return PALETTE_NEW_NOTE;
    case PaletteToolId::LASER_POINTER:
      return PALETTE_LASER_POINTER;
    case PaletteToolId::MAGNIFY:
      return PALETTE_MAGNIFY;
    case PaletteToolId::ENTER_CAPTURE_MODE:
      return PALETTE_ENTER_CAPTURE_MODE;
    case PaletteToolId::MARKER_MODE:
      return PALETTE_MARKER_MODE;
  }

  NOTREACHED();
}

PaletteModeCancelType PaletteToolIdToPaletteModeCancelType(
    PaletteToolId tool_id,
    bool is_switched) {
  PaletteModeCancelType type = PALETTE_MODE_CANCEL_TYPE_COUNT;
  if (tool_id == PaletteToolId::LASER_POINTER) {
    return is_switched ? PALETTE_MODE_LASER_POINTER_SWITCHED
                       : PALETTE_MODE_LASER_POINTER_CANCELLED;
  } else if (tool_id == PaletteToolId::MAGNIFY) {
    return is_switched ? PALETTE_MODE_MAGNIFY_SWITCHED
                       : PALETTE_MODE_MAGNIFY_CANCELLED;
  }
  return type;
}

}  // namespace ash

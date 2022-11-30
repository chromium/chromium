// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_HUD_PROPERTIES_H_
#define ASH_HUD_DISPLAY_HUD_PROPERTIES_H_

#include "ash/ash_export.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"

namespace ash {
namespace hud_display {

// Marks view as Click event handler to make clicks on the other area drag the
// whole HUD.
extern const ui::ClassProperty<HitTestCompat>* const kHUDClickHandler;

}  // namespace hud_display
}  // namespace ash

DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_EXPORT, HitTestCompat)

#endif  // ASH_HUD_DISPLAY_HUD_PROPERTIES_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/hud_properties.h"

namespace ash {
namespace hud_display {

DEFINE_UI_CLASS_PROPERTY_KEY(HitTestCompat, kHUDClickHandler, HTNOWHERE)

}  // namespace hud_display
}  // namespace ash

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(ASH_EXPORT, HitTestCompat)

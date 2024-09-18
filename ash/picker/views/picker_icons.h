// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_ICONS_H_
#define ASH_PICKER_VIEWS_PICKER_ICONS_H_

#include "ash/ash_export.h"
#include "ash/picker/picker_category.h"
#include "ash/resources/vector_icons/vector_icons.h"

namespace ui {
class ImageModel;
}

namespace ash {

ui::ImageModel GetIconForPickerCategory(PickerCategory category);

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_ICONS_H_

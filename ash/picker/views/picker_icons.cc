// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_icons.h"

#include "ash/public/cpp/picker/picker_category.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {
namespace {

const gfx::VectorIcon& GetVectorIconForPickerCategory(PickerCategory category) {
  switch (category) {
    case PickerCategory::kEditorWrite:
      // TODO: b/322926823 - Use correct icons.
      return kPencilIcon;
    case PickerCategory::kEditorRewrite:
      // TODO: b/322926823 - Use correct icons.
      return kPencilIcon;
    case PickerCategory::kExpressions:
      return kPickerEmojiIcon;
    case PickerCategory::kLinks:
      return kPickerBrowsingHistoryIcon;
    case PickerCategory::kClipboard:
      return kPickerClipboardIcon;
    case PickerCategory::kDriveFiles:
      return kPickerDriveFilesIcon;
    case PickerCategory::kLocalFiles:
      return kPickerLocalFilesIcon;
    case PickerCategory::kDatesTimes:
      return kPickerCalendarIcon;
    case PickerCategory::kUnitsMaths:
      return kPickerUnitsMathsIcon;
  }
}

}  // namespace

ui::ImageModel GetIconForPickerCategory(PickerCategory category) {
  return ui::ImageModel::FromVectorIcon(
      GetVectorIconForPickerCategory(category), cros_tokens::kCrosSysOnSurface);
}

}  // namespace ash

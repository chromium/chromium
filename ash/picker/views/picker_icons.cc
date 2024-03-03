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
    case PickerCategory::kEmojis:
      return kPickerEmojiIcon;
    case PickerCategory::kSymbols:
      return kPickerSymbolIcon;
    case PickerCategory::kEmoticons:
      return kPickerEmoticonIcon;
    case PickerCategory::kGifs:
      return kPickerGifIcon;
    case PickerCategory::kOpenTabs:
      return kPickerOpenTabIcon;
    case PickerCategory::kBrowsingHistory:
      return kPickerBrowsingHistoryIcon;
    case PickerCategory::kBookmarks:
      return kPickerBookmarkIcon;
    case PickerCategory::kDriveFiles:
      // TODO: b/327492842 - Use correct icons.
      return kFolderIcon;
    case PickerCategory::kLocalFiles:
      // TODO: b/327492842 - Use correct icons.
      return kFolderIcon;
  }
}

}  // namespace

ui::ImageModel GetIconForPickerCategory(PickerCategory category) {
  return ui::ImageModel::FromVectorIcon(
      GetVectorIconForPickerCategory(category), cros_tokens::kCrosSysOnSurface);
}

}  // namespace ash

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_icons.h"

#include "ash/quick_insert/quick_insert_category.h"
#include "build/branding_buildflags.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/vector_icon_types.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/icons/vector_icons.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {
namespace {

const gfx::VectorIcon& GetVectorIconForQuickInsertCategory(
    QuickInsertCategory category) {
  switch (category) {
    case QuickInsertCategory::kEditorWrite:
      // TODO: b/322926823 - Use correct icons.
      return kPencilIcon;
    case QuickInsertCategory::kEditorRewrite:
      // TODO: b/322926823 - Use correct icons.
      return kPencilIcon;
    case QuickInsertCategory::kLobsterWithNoSelectedText:
    case QuickInsertCategory::kLobsterWithSelectedText:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return kLobsterIcon;
#else
      return kPencilIcon;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case QuickInsertCategory::kEmojisGifs:
    case QuickInsertCategory::kEmojis:
      return kQuickInsertEmojiIcon;
    case QuickInsertCategory::kLinks:
      return kQuickInsertBrowsingHistoryIcon;
    case QuickInsertCategory::kClipboard:
      return kQuickInsertClipboardIcon;
    case QuickInsertCategory::kDriveFiles:
      return kQuickInsertDriveFilesIcon;
    case QuickInsertCategory::kLocalFiles:
      return kFilesAppIcon;
    case QuickInsertCategory::kDatesTimes:
      return kQuickInsertCalendarIcon;
    case QuickInsertCategory::kUnitsMaths:
      return kQuickInsertUnitsMathsIcon;
  }
}

}  // namespace

ui::ImageModel GetIconForQuickInsertCategory(QuickInsertCategory category) {
  return ui::ImageModel::FromVectorIcon(
      GetVectorIconForQuickInsertCategory(category),
      cros_tokens::kCrosSysOnSurface);
}

}  // namespace ash

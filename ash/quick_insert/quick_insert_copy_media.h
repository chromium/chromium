// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_QUICK_INSERT_COPY_MEDIA_H_
#define ASH_QUICK_INSERT_QUICK_INSERT_COPY_MEDIA_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/quick_insert/quick_insert_rich_media.h"

namespace ui {
class ClipboardData;
}

namespace ash {

struct ASH_EXPORT QuickInsertClipboardDataOptions {
  bool links_should_use_title = false;
};

ASH_EXPORT std::unique_ptr<ui::ClipboardData> ClipboardDataFromMedia(
    const QuickInsertRichMedia& media,
    const QuickInsertClipboardDataOptions& options);

// Copies rich media into the clipboard.
ASH_EXPORT void CopyMediaToClipboard(const QuickInsertRichMedia& media);

}  // namespace ash

#endif  // ASH_QUICK_INSERT_QUICK_INSERT_COPY_MEDIA_H_

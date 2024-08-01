// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_COPY_MEDIA_H_
#define ASH_PICKER_PICKER_COPY_MEDIA_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/picker/picker_rich_media.h"

namespace ui {
class ClipboardData;
}

namespace ash {

struct ASH_EXPORT PickerClipboardDataOptions {
  bool links_should_use_title = false;
};

ASH_EXPORT std::unique_ptr<ui::ClipboardData> ClipboardDataFromMedia(
    const PickerRichMedia& media,
    const PickerClipboardDataOptions& options);

// Copies rich media into the clipboard.
ASH_EXPORT void CopyMediaToClipboard(const PickerRichMedia& media);

}  // namespace ash

#endif  // ASH_PICKER_PICKER_COPY_MEDIA_H_

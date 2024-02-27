// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_COPY_MEDIA_H_
#define ASH_PICKER_PICKER_COPY_MEDIA_H_

#include <string_view>

#include "ash/ash_export.h"
#include "ash/picker/picker_rich_media.h"

namespace ash {

// Copies rich media into the clipboard.
ASH_EXPORT void CopyMediaToClipboard(const PickerRichMedia& media);

}  // namespace ash

#endif  // ASH_PICKER_PICKER_COPY_MEDIA_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_COPY_MEDIA_H_
#define ASH_PICKER_PICKER_COPY_MEDIA_H_

#include <string_view>

#include "ash/ash_export.h"

class GURL;

namespace ash {

// Copies a GIF into the clipboard.
// TODO: b/322928125 - Take a PickerInsertMediaRequest::MediaData instead.
ASH_EXPORT void CopyGifMediaToClipboard(
    const GURL& url,
    std::u16string_view content_description);

}  // namespace ash

#endif  // ASH_PICKER_PICKER_COPY_MEDIA_H_

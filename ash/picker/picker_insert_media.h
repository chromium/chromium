// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_INSERT_MEDIA_H_
#define ASH_PICKER_PICKER_INSERT_MEDIA_H_

#include "ash/ash_export.h"
#include "ash/picker/picker_rich_media.h"

namespace ui {
class TextInputClient;
}  // namespace ui

namespace ash {

// Inserts `media` into `client`.
// Returns whether the insertion was successful.
[[nodiscard]] ASH_EXPORT bool InsertMediaToInputField(
    PickerRichMedia media,
    ui::TextInputClient* client);

}  // namespace ash

#endif  // ASH_PICKER_PICKER_INSERT_MEDIA_REQUEST_H_

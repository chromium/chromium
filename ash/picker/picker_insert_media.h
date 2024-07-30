// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_INSERT_MEDIA_H_
#define ASH_PICKER_PICKER_INSERT_MEDIA_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/picker/picker_rich_media.h"
#include "base/functional/callback_forward.h"

namespace ui {
class TextInputClient;
}  // namespace ui

namespace ash {

struct PickerWebPasteTarget;

// Returns whether the `client` supports inserting `media`.
ASH_EXPORT bool InputFieldSupportsInsertingMedia(const PickerRichMedia& media,
                                                 ui::TextInputClient& client);

enum class ASH_EXPORT InsertMediaResult {
  kSuccess,
  kUnsupported,
  kNotFound,
};

using WebPasteTargetCallback =
    base::OnceCallback<std::optional<PickerWebPasteTarget>()>;
using OnInsertMediaCompleteCallback =
    base::OnceCallback<void(InsertMediaResult)>;

// Inserts `media` into `client` asynchronously.
// `callback` is called with whether the insertion was successful.
ASH_EXPORT void InsertMediaToInputField(
    PickerRichMedia media,
    ui::TextInputClient& client,
    WebPasteTargetCallback get_web_paste_target,
    OnInsertMediaCompleteCallback callback);

}  // namespace ash

#endif  // ASH_PICKER_PICKER_INSERT_MEDIA_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_RICH_MEDIA_H_
#define ASH_PICKER_PICKER_RICH_MEDIA_H_

#include <string>
#include <string_view>
#include <variant>

#include "ash/ash_export.h"
#include "url/gurl.h"

namespace ash {

struct ASH_EXPORT PickerTextMedia {
  std::u16string text;

  explicit PickerTextMedia(std::u16string text);
  explicit PickerTextMedia(std::string_view text);
};

struct ASH_EXPORT PickerImageMedia {
  GURL url;

  explicit PickerImageMedia(GURL url);
};

struct ASH_EXPORT PickerLinkMedia {
  GURL url;

  explicit PickerLinkMedia(GURL url);
};

// Rich media that can be inserted or copied, such as text and images.
using PickerRichMedia =
    std::variant<PickerTextMedia, PickerImageMedia, PickerLinkMedia>;

}  // namespace ash

#endif  // ASH_PICKER_PICKER_INSERT_MEDIA_REQUEST_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_RICH_MEDIA_H_
#define ASH_PICKER_PICKER_RICH_MEDIA_H_

#include <string>
#include <string_view>
#include <variant>

#include "ash/ash_export.h"
#include "base/files/file_path.h"
#include "url/gurl.h"

namespace ash {

struct ASH_EXPORT PickerTextMedia {
  std::u16string text;

  explicit PickerTextMedia(std::u16string text);
  explicit PickerTextMedia(std::string_view text);
};

struct ASH_EXPORT PickerLinkMedia {
  GURL url;

  explicit PickerLinkMedia(GURL url);
};

struct ASH_EXPORT PickerLocalFileMedia {
  base::FilePath path;

  explicit PickerLocalFileMedia(base::FilePath path);
};

// Rich media that can be inserted or copied, such as text and images.
using PickerRichMedia =
    std::variant<PickerTextMedia, PickerLinkMedia, PickerLocalFileMedia>;

}  // namespace ash

#endif  // ASH_PICKER_PICKER_RICH_MEDIA_H_

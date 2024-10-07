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
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {

struct ASH_EXPORT PickerTextMedia {
  std::u16string text;

  explicit PickerTextMedia(std::u16string text);
  explicit PickerTextMedia(std::string_view text);
};

struct ASH_EXPORT PickerImageMedia {
  GURL url;
  // `dimensions` is std::nullopt if it's unknown.
  std::optional<gfx::Size> dimensions;
  std::u16string content_description;

  explicit PickerImageMedia(base::span<const uint8_t> png);

  explicit PickerImageMedia(GURL url,
                            std::optional<gfx::Size> dimensions = std::nullopt,
                            std::u16string content_description = u"");
};

struct ASH_EXPORT PickerLinkMedia {
  GURL url;
  std::string title;

  explicit PickerLinkMedia(GURL url, std::string title);
};

struct ASH_EXPORT PickerLocalFileMedia {
  base::FilePath path;

  explicit PickerLocalFileMedia(base::FilePath path);
};

// Rich media that can be inserted or copied, such as text and images.
using PickerRichMedia = std::variant<PickerTextMedia,
                                     PickerImageMedia,
                                     PickerLinkMedia,
                                     PickerLocalFileMedia>;

}  // namespace ash

#endif  // ASH_PICKER_PICKER_RICH_MEDIA_H_

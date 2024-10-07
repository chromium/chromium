// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_rich_media.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "url/gurl.h"

namespace ash {

PickerTextMedia::PickerTextMedia(std::u16string text) : text(std::move(text)) {}

PickerTextMedia::PickerTextMedia(std::string_view text)
    : PickerTextMedia(base::UTF8ToUTF16(text)) {}

PickerImageMedia::PickerImageMedia(base::span<const uint8_t> png)
    : url(GURL(
          base::StrCat({"data:image/png;base64,", base::Base64Encode(png)}))) {}

PickerImageMedia::PickerImageMedia(GURL url,
                                   std::optional<gfx::Size> dimensions,
                                   std::u16string content_description)
    : url(std::move(url)),
      dimensions(dimensions),
      content_description(std::move(content_description)) {}

PickerLinkMedia::PickerLinkMedia(GURL url, std::string title)
    : url(std::move(url)), title(std::move(title)) {}

PickerLocalFileMedia::PickerLocalFileMedia(base::FilePath path)
    : path(std::move(path)) {}

}  // namespace ash

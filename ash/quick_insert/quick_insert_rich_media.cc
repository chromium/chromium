// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/quick_insert_rich_media.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "url/gurl.h"

namespace ash {

QuickInsertTextMedia::QuickInsertTextMedia(std::u16string text)
    : text(std::move(text)) {}

QuickInsertTextMedia::QuickInsertTextMedia(std::string_view text)
    : QuickInsertTextMedia(base::UTF8ToUTF16(text)) {}

QuickInsertImageMedia::QuickInsertImageMedia(base::span<const uint8_t> png)
    : url(GURL(
          base::StrCat({"data:image/png;base64,", base::Base64Encode(png)}))) {}

QuickInsertImageMedia::QuickInsertImageMedia(
    GURL url,
    std::optional<gfx::Size> dimensions,
    std::u16string content_description)
    : url(std::move(url)),
      dimensions(dimensions),
      content_description(std::move(content_description)) {}

QuickInsertLinkMedia::QuickInsertLinkMedia(GURL url, std::string title)
    : url(std::move(url)), title(std::move(title)) {}

QuickInsertLocalFileMedia::QuickInsertLocalFileMedia(base::FilePath path)
    : path(std::move(path)) {}

}  // namespace ash

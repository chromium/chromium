// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_QUICK_INSERT_RICH_MEDIA_H_
#define ASH_QUICK_INSERT_QUICK_INSERT_RICH_MEDIA_H_

#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "ash/ash_export.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {

struct ASH_EXPORT QuickInsertTextMedia {
  std::u16string text;

  explicit QuickInsertTextMedia(std::u16string text);
  explicit QuickInsertTextMedia(std::string_view text);
};

struct ASH_EXPORT QuickInsertImageMedia {
  GURL url;
  // `dimensions` is std::nullopt if it's unknown.
  std::optional<gfx::Size> dimensions;
  std::u16string content_description;

  explicit QuickInsertImageMedia(base::span<const uint8_t> png);

  explicit QuickInsertImageMedia(
      GURL url,
      std::optional<gfx::Size> dimensions = std::nullopt,
      std::u16string content_description = u"");
};

struct ASH_EXPORT QuickInsertLinkMedia {
  GURL url;
  std::string title;

  explicit QuickInsertLinkMedia(GURL url, std::string title);
};

struct ASH_EXPORT QuickInsertLocalFileMedia {
  base::FilePath path;

  explicit QuickInsertLocalFileMedia(base::FilePath path);
};

// Rich media that can be inserted or copied, such as text and images.
using QuickInsertRichMedia = std::variant<QuickInsertTextMedia,
                                          QuickInsertImageMedia,
                                          QuickInsertLinkMedia,
                                          QuickInsertLocalFileMedia>;

}  // namespace ash

#endif  // ASH_QUICK_INSERT_QUICK_INSERT_RICH_MEDIA_H_

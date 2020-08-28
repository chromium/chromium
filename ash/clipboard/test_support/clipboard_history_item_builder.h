// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_TEST_SUPPORT_CLIPBOARD_HISTORY_ITEM_BUILDER_H_
#define ASH_CLIPBOARD_TEST_SUPPORT_CLIPBOARD_HISTORY_ITEM_BUILDER_H_

#include <string>

#include "ash/ash_export.h"
#include "base/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace ui {
enum class ClipboardInternalFormat;
}  // namespace ui

namespace ash {

class ClipboardHistoryItem;

// A builder for constructing `ClipboardHistoryItem`s in testing.
class ASH_EXPORT ClipboardHistoryItemBuilder {
 public:
  ClipboardHistoryItemBuilder();
  ClipboardHistoryItemBuilder(const ClipboardHistoryItemBuilder&) = delete;
  ClipboardHistoryItemBuilder& operator=(const ClipboardHistoryItemBuilder&) =
      delete;
  ~ClipboardHistoryItemBuilder();

  // Constructs a `ClipboardHistoryItem` from only explicitly set data.
  ClipboardHistoryItem Build() const;

  // Clears all data.
  ClipboardHistoryItemBuilder& Clear();

  // Sets data of the specified `format` to a hardcoded value.
  ClipboardHistoryItemBuilder& SetFormat(ui::ClipboardInternalFormat format);

  // Clears data of the specified `format`.
  ClipboardHistoryItemBuilder& ClearFormat(ui::ClipboardInternalFormat format);

  // Sets/clears `text_` data.
  ClipboardHistoryItemBuilder& SetText(const std::string&);
  ClipboardHistoryItemBuilder& ClearText();

  // Sets/clears `markup_` data.
  ClipboardHistoryItemBuilder& SetMarkup(const std::string& markup);
  ClipboardHistoryItemBuilder& ClearMarkup();

  // Sets/clears `svg_` data.
  ClipboardHistoryItemBuilder& SetSvg(const std::string& svg);
  ClipboardHistoryItemBuilder& ClearSvg();

  // Sets/clears `rtf_` data.
  ClipboardHistoryItemBuilder& SetRtf(const std::string& rtf);
  ClipboardHistoryItemBuilder& ClearRtf();

  // Sets/clears `bookmark_title_` data.
  ClipboardHistoryItemBuilder& SetBookmarkTitle(
      const std::string& bookmark_title);
  ClipboardHistoryItemBuilder& ClearBookmarkTitle();

  // Sets/clears `bitmap_` data.
  ClipboardHistoryItemBuilder& SetBitmap(const SkBitmap& bitmap);
  ClipboardHistoryItemBuilder& ClearBitmap();

  // Sets/clears `custom_format_` and `custom_data_` data.
  ClipboardHistoryItemBuilder& SetCustomData(const std::string& custom_format,
                                             const std::string& custom_data);
  ClipboardHistoryItemBuilder& ClearCustomData();

  // Sets/clears file system data.
  // NOTE: File system data is a special type of custom data.
  ClipboardHistoryItemBuilder& SetFileSystemData(
      const std::initializer_list<std::string>& source_list);

  // Sets/clears `web_smart_paste_` data.
  ClipboardHistoryItemBuilder& SetWebSmartPaste(bool web_smart_paste);
  ClipboardHistoryItemBuilder& ClearWebSmartPaste();

 private:
  // `ui::ClipboardData` formats.
  base::Optional<std::string> text_;
  base::Optional<std::string> markup_;
  base::Optional<std::string> svg_;
  base::Optional<std::string> rtf_;
  base::Optional<std::string> bookmark_title_;
  base::Optional<SkBitmap> bitmap_;
  base::Optional<std::string> custom_format_;
  base::Optional<std::string> custom_data_;
  base::Optional<bool> web_smart_paste_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_TEST_SUPPORT_CLIPBOARD_HISTORY_ITEM_BUILDER_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_TEST_SUPPORT_CLIPBOARD_HISTORY_ITEM_BUILDER_H_
#define ASH_CLIPBOARD_TEST_SUPPORT_CLIPBOARD_HISTORY_ITEM_BUILDER_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "base/memory/ref_counted_memory.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/file_info.h"

namespace ui {
class ClipboardData;
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

  // Constructs a `ClipboardData` with the requested formats populated.
  ui::ClipboardData BuildData() const;

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

  // Sets/clears `filenames_` data.
  ClipboardHistoryItemBuilder& SetFilenames(
      std::vector<ui::FileInfo> filenames);
  ClipboardHistoryItemBuilder& ClearFilenames();

  // Sets/clears `bookmark_title_` data.
  ClipboardHistoryItemBuilder& SetBookmarkTitle(
      const std::string& bookmark_title);
  ClipboardHistoryItemBuilder& ClearBookmarkTitle();

  // Sets/clears `png_` data.
  ClipboardHistoryItemBuilder& SetPng(std::vector<uint8_t> png);
  ClipboardHistoryItemBuilder& SetPng(
      const scoped_refptr<base::RefCountedMemory>& png);
  ClipboardHistoryItemBuilder& ClearPng();

  // Sets/clears `custom_format_` and `custom_data_` data.
  ClipboardHistoryItemBuilder& SetCustomData(
      const ui::ClipboardFormatType& custom_format,
      const std::string& custom_data);
  ClipboardHistoryItemBuilder& ClearCustomData();

  // Sets/clears file system data.
  // NOTE: File system data is a special type of custom data.
  ClipboardHistoryItemBuilder& SetFileSystemData(
      const std::initializer_list<std::u16string>& source_list);

  // Sets/clears `web_smart_paste_` data.
  ClipboardHistoryItemBuilder& SetWebSmartPaste(bool web_smart_paste);
  ClipboardHistoryItemBuilder& ClearWebSmartPaste();

 private:
  // `ui::ClipboardData` formats.
  std::optional<std::string> text_;
  std::optional<std::string> markup_;
  std::optional<std::string> svg_;
  std::optional<std::string> rtf_;
  std::vector<ui::FileInfo> filenames_;
  std::optional<std::string> bookmark_title_;
  std::optional<std::vector<uint8_t>> png_;
  std::optional<ui::ClipboardFormatType> custom_format_;
  std::optional<std::string> custom_data_;
  std::optional<bool> web_smart_paste_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_TEST_SUPPORT_CLIPBOARD_HISTORY_ITEM_BUILDER_H_

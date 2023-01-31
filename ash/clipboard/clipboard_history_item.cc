// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_item.h"

#include "ash/clipboard/clipboard_history_util.h"

namespace ash {

namespace {

ClipboardHistoryItem::DisplayFormat CalculateDisplayFormat(
    ui::ClipboardInternalFormat main_format,
    const ui::ClipboardData& data) {
  switch (main_format) {
    case ui::ClipboardInternalFormat::kPng:
      return ClipboardHistoryItem::DisplayFormat::kPng;
    case ui::ClipboardInternalFormat::kHtml:
      if ((data.markup_data().find("<img") == std::string::npos) &&
          (data.markup_data().find("<table") == std::string::npos)) {
        return ClipboardHistoryItem::DisplayFormat::kText;
      }
      return ClipboardHistoryItem::DisplayFormat::kHtml;
    case ui::ClipboardInternalFormat::kText:
    case ui::ClipboardInternalFormat::kSvg:
    case ui::ClipboardInternalFormat::kRtf:
    case ui::ClipboardInternalFormat::kBookmark:
    case ui::ClipboardInternalFormat::kWeb:
      return ClipboardHistoryItem::DisplayFormat::kText;
    case ui::ClipboardInternalFormat::kFilenames:
      return ClipboardHistoryItem::DisplayFormat::kFile;
    case ui::ClipboardInternalFormat::kCustom:
      return clipboard_history_util::ContainsFileSystemData(data)
                 ? ClipboardHistoryItem::DisplayFormat::kFile
                 : ClipboardHistoryItem::DisplayFormat::kText;
  }
}

}  // namespace

ClipboardHistoryItem::ClipboardHistoryItem(ui::ClipboardData data)
    : id_(base::UnguessableToken::Create()),
      data_(std::move(data)),
      main_format_(clipboard_history_util::CalculateMainFormat(data_).value()),
      display_format_(CalculateDisplayFormat(main_format_, data_)),
      time_copied_(base::Time::Now()) {}

ClipboardHistoryItem::ClipboardHistoryItem(const ClipboardHistoryItem&) =
    default;

ClipboardHistoryItem::ClipboardHistoryItem(ClipboardHistoryItem&&) = default;

ClipboardHistoryItem::~ClipboardHistoryItem() = default;

ui::ClipboardData ClipboardHistoryItem::ReplaceEquivalentData(
    ui::ClipboardData&& new_data) {
  DCHECK(data_ == new_data);
  time_copied_ = base::Time::Now();
  // If work has already been done to encode an image belonging to both data
  // instances, make sure it is not lost.
  if (data_.maybe_png() && !new_data.maybe_png())
    new_data.SetPngDataAfterEncoding(*data_.maybe_png());
  return std::exchange(data_, std::move(new_data));
}

}  // namespace ash

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_util.h"

#include <array>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/custom_data_helper.h"

namespace ash {
namespace ClipboardHistoryUtil {

namespace {

constexpr char kFileSystemSourcesType[] = "fs/sources";

// The array of formats in order of decreasing priority.
constexpr ui::ClipboardInternalFormat kPrioritizedFormats[] = {
    ui::ClipboardInternalFormat::kBitmap,   ui::ClipboardInternalFormat::kHtml,
    ui::ClipboardInternalFormat::kText,     ui::ClipboardInternalFormat::kRtf,
    ui::ClipboardInternalFormat::kBookmark, ui::ClipboardInternalFormat::kWeb,
    ui::ClipboardInternalFormat::kCustom};

}  // namespace

base::Optional<ui::ClipboardInternalFormat> CalculateMainFormat(
    const ui::ClipboardData& data) {
  for (const auto& format : kPrioritizedFormats) {
    if (ContainsFormat(data, format))
      return format;
  }
  return base::nullopt;
}

bool ContainsFormat(const ui::ClipboardData& data,
                    ui::ClipboardInternalFormat format) {
  return data.format() & static_cast<int>(format);
}

bool ContainsFileSystemData(const ui::ClipboardData& data) {
  return !GetFileSystemSources(data).empty();
}

base::string16 GetFileSystemSources(const ui::ClipboardData& data) {
  if (!ContainsFormat(data, ui::ClipboardInternalFormat::kCustom))
    return base::string16();

  // Attempt to read file system sources in the custom data.
  base::string16 sources;
  ui::ReadCustomDataForType(
      data.custom_data_data().c_str(), data.custom_data_data().size(),
      base::UTF8ToUTF16(kFileSystemSourcesType), &sources);

  return sources;
}

bool IsSupported(const ui::ClipboardData& data) {
  const base::Optional<ui::ClipboardInternalFormat> format =
      CalculateMainFormat(data);

  // Empty `data` is not supported.
  if (!format.has_value())
    return false;

  // The only supported type of custom data is file system data.
  if (format.value() == ui::ClipboardInternalFormat::kCustom)
    return ContainsFileSystemData(data);

  return true;
}

}  // namespace ClipboardHistoryUtil
}  // namespace ash

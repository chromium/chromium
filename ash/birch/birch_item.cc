// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_item.h"

#include <sstream>
#include <string>

#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"

namespace ash {

BirchItem::BirchItem(const std::string& title) : title(title) {}

BirchItem::~BirchItem() = default;

BirchFileItem::BirchFileItem(const base::FilePath& file_path,
                             const std::optional<base::Time>& timestamp)
    : BirchItem(file_path.BaseName().value()),
      file_path(file_path),
      timestamp(timestamp) {}

BirchFileItem::~BirchFileItem() = default;

std::string BirchFileItem::ToString() const {
  std::stringstream ss;
  ss << "title: " << title << ", file_path:" << file_path;
  if (timestamp.has_value()) {
    ss << ", timestamp: "
       << base::UTF16ToUTF8(
              base::TimeFormatShortDateAndTime(timestamp.value()));
  }
  return ss.str();
}

}  // namespace ash

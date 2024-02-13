// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_item.h"

#include <sstream>
#include <string>

#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"

namespace ash {

BirchItem::BirchItem(const std::u16string& title, ui::ImageModel icon)
    : title(title), icon(std::move(icon)) {}

BirchItem::BirchItem(const BirchItem&) = default;

BirchItem::~BirchItem() = default;

BirchFileItem::BirchFileItem(const base::FilePath& file_path,
                             const std::optional<base::Time>& timestamp)
    : BirchItem(base::UTF8ToUTF16(file_path.BaseName().value()),
                ui::ImageModel()),
      file_path(file_path),
      timestamp(timestamp) {}

BirchFileItem::~BirchFileItem() = default;

const char* BirchFileItem::GetItemType() const {
  return kItemType;
}

std::string BirchFileItem::ToString() const {
  std::stringstream ss;
  ss << "File item : {title: " << base::UTF16ToUTF8(title)
     << ", file_path:" << file_path;
  if (timestamp.has_value()) {
    ss << ", timestamp: "
       << base::UTF16ToUTF8(
              base::TimeFormatShortDateAndTime(timestamp.value()));
  }
  ss << "}";
  return ss.str();
}

BirchCalendarItem::BirchCalendarItem(const std::u16string& title,
                                     const GURL& icon_url,
                                     const base::Time& start_time,
                                     const base::Time& end_time)
    : BirchItem(title, ui::ImageModel()),
      icon_url(icon_url),
      start_time(start_time),
      end_time(end_time) {}

BirchCalendarItem::~BirchCalendarItem() = default;

const char* BirchCalendarItem::GetItemType() const {
  return kItemType;
}

std::string BirchCalendarItem::ToString() const {
  std::stringstream ss;
  using base::UTF16ToUTF8;
  ss << "Calendar item: {title: " << UTF16ToUTF8(title)
     << ", icon_url: " << icon_url.spec()
     << ", start: " << UTF16ToUTF8(base::TimeFormatShortDateAndTime(start_time))
     << ", end: " << UTF16ToUTF8(base::TimeFormatShortDateAndTime(end_time))
     << "}";
  return ss.str();
}

BirchWeatherItem::BirchWeatherItem(const std::u16string& weather_description,
                                   const std::u16string& temperature,
                                   ui::ImageModel icon)
    : BirchItem(weather_description, std::move(icon)),
      temperature(temperature) {}

BirchWeatherItem::~BirchWeatherItem() = default;

const char* BirchWeatherItem::GetItemType() const {
  return kItemType;
}

std::string BirchWeatherItem::ToString() const {
  std::stringstream ss;
  ss << "Weather item: {title: " << base::UTF16ToUTF8(title)
     << ", temperature:" << base::UTF16ToUTF8(temperature) << "}";
  return ss.str();
}

BirchTabItem::BirchTabItem(const std::u16string& title,
                           const GURL& url,
                           const base::Time& timestamp,
                           const GURL& favicon_url,
                           const std::string& session_name)
    : BirchItem(title, ui::ImageModel()),
      url(url),
      timestamp(timestamp),
      favicon_url(favicon_url),
      session_name(session_name) {}

BirchTabItem::BirchTabItem(BirchTabItem&&) = default;

BirchTabItem::BirchTabItem(const BirchTabItem&) = default;

BirchTabItem::~BirchTabItem() = default;

const char* BirchTabItem::GetItemType() const {
  return kItemType;
}

std::string BirchTabItem::ToString() const {
  std::stringstream ss;
  ss << "title: " << base::UTF16ToUTF8(title) << ", url:" << url
     << ", timestamp:" << timestamp << ", favicon_url:" << favicon_url
     << ", session_name:" << session_name;
  return ss.str();
}

}  // namespace ash

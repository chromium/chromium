// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_ITEM_H_
#define ASH_BIRCH_BIRCH_ITEM_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"

namespace ash {

// The base item which is stored by the birch model.
struct ASH_EXPORT BirchItem {
  BirchItem(const std::u16string& title, const ui::ImageModel icon);
  BirchItem(BirchItem&&) = default;
  BirchItem(const BirchItem&);
  BirchItem& operator=(const BirchItem&);
  virtual ~BirchItem();
  bool operator==(const BirchItem& rhs) const = default;

  const std::u16string title;
  const ui::ImageModel icon;
  virtual const char* GetItemType() const = 0;
};

// A birch item which contains calendar event information.
struct ASH_EXPORT BirchCalendarItem : public BirchItem {
  BirchCalendarItem(const std::u16string& title,
                    const GURL& icon_url,
                    const base::Time& start_time,
                    const base::Time& end_time);
  BirchCalendarItem(BirchCalendarItem&&) = default;
  BirchCalendarItem(const BirchCalendarItem&) = default;
  BirchCalendarItem& operator=(const BirchCalendarItem&) = delete;
  bool operator==(const BirchCalendarItem& rhs) const = default;
  ~BirchCalendarItem() override;

  static constexpr char kItemType[] = "CalendarItem";

  // BirchItem:
  const char* GetItemType() const override;

  // For debugging.
  std::string ToString() const;

  const GURL icon_url;
  const base::Time start_time;
  const base::Time end_time;
};

// A birch item which contains file path and time information.
struct ASH_EXPORT BirchFileItem : public BirchItem {
  BirchFileItem(const base::FilePath& file_path,
                const std::optional<base::Time>& timestamp);
  BirchFileItem(BirchFileItem&&) = default;
  BirchFileItem(const BirchFileItem&) = default;
  BirchFileItem& operator=(const BirchFileItem&) = delete;
  bool operator==(const BirchFileItem& rhs) const = default;
  ~BirchFileItem() override;

  const base::FilePath file_path;
  const std::optional<base::Time> timestamp;

  static constexpr char kItemType[] = "FileItem";

  const char* GetItemType() const override;

  // Intended for debugging.
  std::string ToString() const;
};

// A birch item which contains tab and session information.
struct ASH_EXPORT BirchTabItem : public BirchItem {
  BirchTabItem(const std::u16string& title,
               const GURL& url,
               const base::Time& timestamp,
               const GURL& favicon_url,
               const std::string& session_name);
  BirchTabItem(BirchTabItem&&);
  BirchTabItem(const BirchTabItem&);
  BirchTabItem& operator=(const BirchTabItem&);
  bool operator==(const BirchTabItem& rhs) const = default;
  ~BirchTabItem() override;

  const GURL url;
  const base::Time timestamp;
  const GURL favicon_url;
  const std::string session_name;

  static constexpr char kItemType[] = "TabItem";

  const char* GetItemType() const override;

  // Intended for debugging.
  std::string ToString() const;
};

struct ASH_EXPORT BirchWeatherItem : public BirchItem {
  BirchWeatherItem(const std::u16string& weather_description,
                   const std::u16string& temperature,
                   ui::ImageModel icon);
  BirchWeatherItem(BirchWeatherItem&&) = default;
  BirchWeatherItem(const BirchWeatherItem&) = default;
  BirchWeatherItem& operator=(const BirchWeatherItem&) = delete;
  bool operator==(const BirchWeatherItem& rhs) const = default;
  ~BirchWeatherItem() override;

  const std::u16string temperature;

  static constexpr char kItemType[] = "WeatherItem";

  const char* GetItemType() const override;

  // Intended for debugging.
  std::string ToString() const;
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_ITEM_H_

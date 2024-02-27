// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_ITEM_H_
#define ASH_BIRCH_BIRCH_ITEM_H_

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
  BirchItem(BirchItem&&);
  BirchItem& operator=(BirchItem&&);
  BirchItem(const BirchItem&);
  BirchItem& operator=(const BirchItem&);
  virtual ~BirchItem();
  bool operator==(const BirchItem& rhs) const;

  std::u16string title;
  ui::ImageModel icon;
  float ranking;  // Lower is better.

  virtual const char* GetItemType() const = 0;

  // Print the item to a string for debugging. The format is not stable.
  virtual std::string ToString() const = 0;
};

// A birch item which contains calendar event information.
struct ASH_EXPORT BirchCalendarItem : public BirchItem {
  explicit BirchCalendarItem(const std::u16string& title);
  BirchCalendarItem(BirchCalendarItem&&);
  BirchCalendarItem(const BirchCalendarItem&);
  BirchCalendarItem& operator=(const BirchCalendarItem&);
  ~BirchCalendarItem() override;

  static constexpr char kItemType[] = "CalendarItem";

  // BirchItem:
  const char* GetItemType() const override;
  std::string ToString() const override;

  GURL icon_url;
  base::Time start_time;
  base::Time end_time;
  // Video conferencing URL (e.g. Google Meet).
  GURL conference_url;
};

// An attachment (e.g. a file attached to a calendar event). Represented as a
// separate BirchItem from the calendar event because the UI shows attachments
// separately (and ranks them independently).
struct ASH_EXPORT BirchAttachmentItem : public BirchItem {
  explicit BirchAttachmentItem(const std::u16string& title);
  BirchAttachmentItem(BirchAttachmentItem&&);
  BirchAttachmentItem& operator=(BirchAttachmentItem&&);
  BirchAttachmentItem(const BirchAttachmentItem&);
  BirchAttachmentItem& operator=(const BirchAttachmentItem&);
  ~BirchAttachmentItem() override;

  static constexpr char kItemType[] = "AttachmentItem";

  // BirchItem:
  const char* GetItemType() const override;
  std::string ToString() const override;

  GURL file_url;          // Link to the file.
  GURL icon_url;          // Link to the file's icon's art asset.
  base::Time start_time;  // Start time of the event (used for ranking).
  base::Time end_time;    // End time of the event (used for ranking).
};

// A birch item which contains file path and time information.
struct ASH_EXPORT BirchFileItem : public BirchItem {
  BirchFileItem(const base::FilePath& file_path, base::Time timestamp);
  BirchFileItem(BirchFileItem&&);
  BirchFileItem(const BirchFileItem&);
  BirchFileItem& operator=(const BirchFileItem&);
  bool operator==(const BirchFileItem& rhs) const;
  ~BirchFileItem() override;

  base::FilePath file_path;
  base::Time timestamp;

  static constexpr char kItemType[] = "FileItem";

  // BirchItem:
  const char* GetItemType() const override;
  std::string ToString() const override;
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
  bool operator==(const BirchTabItem& rhs) const;
  ~BirchTabItem() override;

  GURL url;
  base::Time timestamp;
  GURL favicon_url;
  std::string session_name;

  static constexpr char kItemType[] = "TabItem";

  // BirchItem:
  const char* GetItemType() const override;
  std::string ToString() const override;
};

struct ASH_EXPORT BirchWeatherItem : public BirchItem {
  BirchWeatherItem(const std::u16string& weather_description,
                   const std::u16string& temperature,
                   ui::ImageModel icon);
  BirchWeatherItem(BirchWeatherItem&&);
  BirchWeatherItem(const BirchWeatherItem&);
  BirchWeatherItem& operator=(const BirchWeatherItem&);
  bool operator==(const BirchWeatherItem& rhs) const;
  ~BirchWeatherItem() override;

  std::u16string temperature;

  static constexpr char kItemType[] = "WeatherItem";

  // BirchItem:
  const char* GetItemType() const override;
  std::string ToString() const override;
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_ITEM_H_

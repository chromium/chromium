// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BIRCH_BIRCH_ITEM_H_
#define ASH_BIRCH_BIRCH_ITEM_H_

#include <string>

#include "ash/ash_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"

namespace ash {

// These values are used in metrics and should not be reordered or deleted.
enum class BirchItemType {
  kTest = 0,          // Internal type used for testing.
  kCalendar = 1,      // Calendar event.
  kAttachment = 2,    // File attachment from calendar event.
  kFile = 3,          // File suggestion e.g. Google Drive file.
  kTab = 4,           // Recent tab from other device.
  kWeather = 5,       // Weather conditions.
  kReleaseNotes = 6,  // Release notes from recent OS update.
  kMaxValue = kReleaseNotes,
};

// The base item which is stored by the birch model.
class ASH_EXPORT BirchItem {
 public:
  BirchItem(const std::u16string& title, const std::u16string& subtitle);
  BirchItem(BirchItem&&);
  BirchItem& operator=(BirchItem&&);
  BirchItem(const BirchItem&);
  BirchItem& operator=(const BirchItem&);
  virtual ~BirchItem();
  bool operator==(const BirchItem& rhs) const;

  virtual BirchItemType GetType() const = 0;

  // Print the item to a string for debugging. The format is not stable.
  virtual std::string ToString() const = 0;

  // Perform the action associated with this item (e.g. open a document).
  virtual void PerformAction() = 0;

  // Performs the secondary action associated with this item, if the action has
  // a secondary action. When the secondary action is available,
  // `secondary_action()` will be set to the user-friendly secondary action
  // name.
  virtual void PerformSecondaryAction() = 0;

  // Loads the icon for this image. This may invoke the callback immediately
  // (e.g. with a local icon) or there may be a delay for a network fetch.
  using LoadIconCallback = base::OnceCallback<void(const ui::ImageModel&)>;
  virtual void LoadIcon(LoadIconCallback callback) const = 0;

  // Records metrics when the user takes an action on the item (e.g. clicks or
  // taps on it).
  void RecordActionMetrics();

  const std::u16string& title() const { return title_; }
  const std::u16string& subtitle() const { return subtitle_; }

  void set_ranking(float ranking) { ranking_ = ranking; }
  float ranking() const { return ranking_; }

  const std::optional<std::u16string> secondary_action() const {
    return secondary_action_;
  }

  static void set_action_count_for_test(int value) { action_count_ = value; }

 protected:
  void set_secondary_action(const std::u16string& action_name) {
    secondary_action_ = action_name;
  }

 private:
  // The title to be displayed in birch chip UI.
  std::u16string title_;

  // The subtitle to be displayed in birch chip UI.
  std::u16string subtitle_;

  // If the item has a secondary action (e.g. Joining a meeting for Calendar),
  // the name of the action to display in the UI.
  std::optional<std::u16string> secondary_action_;

  float ranking_;  // Lower is better.

  // Clicks or taps on birch chips, across this login session. Used for metrics.
  static int action_count_;
};

// A birch item which contains calendar event information.
class ASH_EXPORT BirchCalendarItem : public BirchItem {
 public:
  BirchCalendarItem(const std::u16string& title,
                    const base::Time& start_time,
                    const base::Time& end_time,
                    const GURL& calendar_url,
                    const GURL& conference_url,
                    const std::string& event_id,
                    const bool all_day_event);
  BirchCalendarItem(BirchCalendarItem&&);
  BirchCalendarItem(const BirchCalendarItem&);
  BirchCalendarItem& operator=(const BirchCalendarItem&);
  ~BirchCalendarItem() override;

  // BirchItem:
  BirchItemType GetType() const override;
  std::string ToString() const override;
  void PerformAction() override;
  void PerformSecondaryAction() override;
  void LoadIcon(LoadIconCallback callback) const override;

  const base::Time& start_time() const { return start_time_; }
  const base::Time& end_time() const { return end_time_; }
  const GURL& calendar_url() const { return calendar_url_; }
  const GURL& conference_url() const { return conference_url_; }
  const std::string& event_id() const { return event_id_; }

 private:
  static std::u16string GetSubtitle(base::Time start_time,
                                    base::Time end_time,
                                    bool all_day_event);

  // Returns a string like "10:00 AM - 10:30 AM".
  static std::u16string GetStartEndString(base::Time start_time,
                                          base::Time end_time);

  // Returns true if the "Join" button should be shown (i.e. the event has a
  // conference URL and the event is ongoing or happening soon).
  bool ShouldShowJoinButton() const;

  base::Time start_time_;
  base::Time end_time_;
  // Link to the event in the Google Calendar UI.
  GURL calendar_url_;
  // Video conferencing URL (e.g. Google Meet).
  GURL conference_url_;
  std::string event_id_;
};

// An attachment (e.g. a file attached to a calendar event). Represented as a
// separate BirchItem from the calendar event because the UI shows attachments
// separately (and ranks them independently).
class ASH_EXPORT BirchAttachmentItem : public BirchItem {
 public:
  explicit BirchAttachmentItem(const std::u16string& title,
                               const GURL& file_url,
                               const GURL& icon_url,
                               const base::Time& start_time,
                               const base::Time& end_time,
                               const std::string& file_id);
  BirchAttachmentItem(BirchAttachmentItem&&);
  BirchAttachmentItem& operator=(BirchAttachmentItem&&);
  BirchAttachmentItem(const BirchAttachmentItem&);
  BirchAttachmentItem& operator=(const BirchAttachmentItem&);
  ~BirchAttachmentItem() override;

  // BirchItem:
  BirchItemType GetType() const override;
  std::string ToString() const override;
  void PerformAction() override;
  void PerformSecondaryAction() override;
  void LoadIcon(LoadIconCallback callback) const override;

  const GURL& file_url() const { return file_url_; }
  const GURL& icon_url() const { return icon_url_; }
  const base::Time& start_time() const { return start_time_; }
  const base::Time& end_time() const { return end_time_; }
  const std::string& file_id() const { return file_id_; }

 private:
  static std::u16string GetSubtitle(base::Time start_time, base::Time end_time);

  GURL file_url_;          // Link to the file.
  GURL icon_url_;          // Link to the file's icon's art asset.
  base::Time start_time_;  // Start time of the event (used for ranking).
  base::Time end_time_;    // End time of the event (used for ranking).
  std::string file_id_;    // ID of the file.
};

// A birch item which contains file path and time information.
class ASH_EXPORT BirchFileItem : public BirchItem {
 public:
  BirchFileItem(const base::FilePath& file_path,
                const std::u16string& justification,
                base::Time timestamp,
                const std::string& file_id,
                const std::string& icon_url);
  BirchFileItem(BirchFileItem&&);
  BirchFileItem(const BirchFileItem&);
  BirchFileItem& operator=(const BirchFileItem&);
  bool operator==(const BirchFileItem& rhs) const;
  ~BirchFileItem() override;

  // BirchItem:
  BirchItemType GetType() const override;
  std::string ToString() const override;
  void PerformAction() override;
  void PerformSecondaryAction() override;
  void LoadIcon(LoadIconCallback callback) const override;

  const base::Time& timestamp() const { return timestamp_; }
  const std::string& file_id() const { return file_id_; }
  const std::string& icon_url() const { return icon_url_; }

 private:
  static std::u16string GetTitle(const base::FilePath& file_path);

  // A unique file id which is used to identify file type items, specifically
  // BirchFileItem and BirchAttachmentItem.
  std::string file_id_;
  std::string icon_url_;
  base::FilePath file_path_;
  base::Time timestamp_;
};

// A birch item which contains tab and session information.
class ASH_EXPORT BirchTabItem : public BirchItem {
 public:
  enum class DeviceFormFactor { kDesktop, kPhone, kTablet };

  BirchTabItem(const std::u16string& title,
               const GURL& url,
               const base::Time& timestamp,
               const GURL& favicon_url,
               const std::string& session_name,
               const DeviceFormFactor& form_factor);
  BirchTabItem(BirchTabItem&&);
  BirchTabItem(const BirchTabItem&);
  BirchTabItem& operator=(const BirchTabItem&);
  bool operator==(const BirchTabItem& rhs) const;
  ~BirchTabItem() override;

  // BirchItem:
  BirchItemType GetType() const override;
  std::string ToString() const override;
  void PerformAction() override;
  void PerformSecondaryAction() override;
  void LoadIcon(LoadIconCallback callback) const override;

  const GURL& url() const { return url_; }
  const base::Time& timestamp() const { return timestamp_; }
  const std::string& session_name() const { return session_name_; }
  DeviceFormFactor form_factor() const { return form_factor_; }

 private:
  static std::u16string GetSubtitle(const std::string& session_name,
                                    base::Time timestamp);

  GURL url_;
  base::Time timestamp_;
  GURL favicon_url_;
  std::string session_name_;
  DeviceFormFactor form_factor_;
};

class ASH_EXPORT BirchWeatherItem : public BirchItem {
 public:
  BirchWeatherItem(const std::u16string& weather_description,
                   const std::u16string& temperature,
                   ui::ImageModel icon);
  BirchWeatherItem(BirchWeatherItem&&);
  BirchWeatherItem(const BirchWeatherItem&);
  BirchWeatherItem& operator=(const BirchWeatherItem&);
  bool operator==(const BirchWeatherItem& rhs) const;
  ~BirchWeatherItem() override;

  // BirchItem:
  BirchItemType GetType() const override;
  std::string ToString() const override;
  void PerformAction() override;
  void PerformSecondaryAction() override;
  void LoadIcon(LoadIconCallback callback) const override;

  const std::u16string& temperature() const { return temperature_; }

 private:
  std::u16string temperature_;
  ui::ImageModel icon_;
};

struct ASH_EXPORT BirchReleaseNotesItem : public BirchItem {
  BirchReleaseNotesItem(const std::u16string& release_notes_title,
                        const std::u16string& release_notes_text,
                        const GURL& url,
                        base::Time first_seen);
  BirchReleaseNotesItem(BirchReleaseNotesItem&&) = default;
  BirchReleaseNotesItem(const BirchReleaseNotesItem&) = default;
  BirchReleaseNotesItem& operator=(const BirchReleaseNotesItem&) = default;
  bool operator==(const BirchReleaseNotesItem& rhs) const = default;
  ~BirchReleaseNotesItem() override;

  // BirchItem:
  BirchItemType GetType() const override;
  std::string ToString() const override;
  void PerformAction() override;
  void PerformSecondaryAction() override;
  void LoadIcon(LoadIconCallback callback) const override;

  const base::Time& first_seen() const { return first_seen_; }
  const GURL& url() const { return url_; }

 private:
  // The text to display in the suggestions.
  std::u16string release_notes_text_;

  // The URL that gets launched when the user clicks on the release notes birch
  // item.
  GURL url_;

  // The timestamp when the user first sees this item.
  base::Time first_seen_;
};

}  // namespace ash

#endif  // ASH_BIRCH_BIRCH_ITEM_H_

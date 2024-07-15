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
#include "base/unguessable_token.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"

class PrefRegistrySimple;

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
  kSelfShare = 7,     // Tabs shared to self from ChromeSync API.
  kMostVisited = 8,   // Most frequently visited URLs.
  kLastActive = 9,    // Last active URL.
  kLostMedia = 10,
  kMaxValue = kLostMedia,
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

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

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
  // The bool is true if the icon load was successful.
  using LoadIconCallback =
      base::OnceCallback<void(const ui::ImageModel&, bool)>;
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
  // Used for ranking calendar items, `kAccepted` has the highest priority.
  enum class ResponseStatus {
    kAccepted = 0,
    kTentative = 1,
    kNeedsAction = 2,
    kDeclined = 3
  };

  BirchCalendarItem(const std::u16string& title,
                    const base::Time& start_time,
                    const base::Time& end_time,
                    const GURL& calendar_url,
                    const GURL& conference_url,
                    const std::string& event_id,
                    const bool all_day_event,
                    ResponseStatus response_status = ResponseStatus::kAccepted);
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
  bool all_day_event() const { return all_day_event_; }
  const GURL& calendar_url() const { return calendar_url_; }
  const GURL& conference_url() const { return conference_url_; }
  const std::string& event_id() const { return event_id_; }
  const ResponseStatus& response_status() const { return response_status_; }

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
  bool all_day_event_;
  // Link to the event in the Google Calendar UI.
  GURL calendar_url_;
  // Video conferencing URL (e.g. Google Meet).
  GURL conference_url_;
  std::string event_id_;
  // The user's current response to this calendar event.
  ResponseStatus response_status_;
};

// An attachment (e.g. a file attached to a calendar event). Represented as a
// separate BirchItem from the calendar event because the UI shows attachments
// separately (and ranks them independently).
class ASH_EXPORT BirchAttachmentItem : public BirchItem {
 public:
  BirchAttachmentItem(const std::u16string& title,
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
  const base::FilePath& file_path() const { return file_path_; }

 private:
  static std::u16string GetTitle(const base::FilePath& file_path);

  // A unique file id which is used to identify file type items, specifically
  // BirchFileItem and BirchAttachmentItem.
  std::string file_id_;
  std::string icon_url_;
  // For Google Drive documents the path looks like:
  // /media/fuse/drivefs-48de6bc248c2f6d8e809521347ef6190/root/Test doc.gdoc
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
               const DeviceFormFactor& form_factor,
               const ui::ImageModel& backup_icon);
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
  ui::ImageModel backup_icon_;
};

// A birch item for the last active URL.
class ASH_EXPORT BirchLastActiveItem : public BirchItem {
 public:
  BirchLastActiveItem(const std::u16string& title,
                      const GURL& url,
                      base::Time last_visit,
                      ui::ImageModel icon);
  BirchLastActiveItem(BirchLastActiveItem&&);
  BirchLastActiveItem(const BirchLastActiveItem&);
  BirchLastActiveItem& operator=(const BirchLastActiveItem&);
  bool operator==(const BirchLastActiveItem& rhs) const;
  ~BirchLastActiveItem() override;

  // BirchItem:
  BirchItemType GetType() const override;
  std::string ToString() const override;
  void PerformAction() override;
  void PerformSecondaryAction() override;
  void LoadIcon(LoadIconCallback callback) const override;

  const GURL& url() const { return url_; }

 private:
  static std::u16string GetSubtitle(base::Time last_visit);

  GURL url_;
  ui::ImageModel icon_;
};

// A birch item for a most-frequently-visited URL.
class ASH_EXPORT BirchMostVisitedItem : public BirchItem {
 public:
  BirchMostVisitedItem(const std::u16string& title,
                       const GURL& url,
                       ui::ImageModel icon);
  BirchMostVisitedItem(BirchMostVisitedItem&&);
  BirchMostVisitedItem(const BirchMostVisitedItem&);
  BirchMostVisitedItem& operator=(const BirchMostVisitedItem&);
  bool operator==(const BirchMostVisitedItem& rhs) const;
  ~BirchMostVisitedItem() override;

  // BirchItem:
  BirchItemType GetType() const override;
  std::string ToString() const override;
  void PerformAction() override;
  void PerformSecondaryAction() override;
  void LoadIcon(LoadIconCallback callback) const override;

  const GURL& url() const { return url_; }

 private:
  static std::u16string GetSubtitle();

  GURL url_;
  ui::ImageModel icon_;
};

// A birch item which contains tabs shared to self information.
class ASH_EXPORT BirchSelfShareItem : public BirchItem {
 public:
  BirchSelfShareItem(const std::u16string& guid,
                     const std::u16string& title,
                     const GURL& url,
                     const base::Time& shared_time,
                     const std::u16string& device_name,
                     const ui::ImageModel& backup_icon,
                     base::RepeatingClosure activation_callback);
  BirchSelfShareItem(BirchSelfShareItem&&);
  BirchSelfShareItem(const BirchSelfShareItem&);
  BirchSelfShareItem& operator=(const BirchSelfShareItem&);
  bool operator==(const BirchSelfShareItem& rhs) const;
  ~BirchSelfShareItem() override;

  // BirchItem:
  BirchItemType GetType() const override;
  std::string ToString() const override;
  void PerformAction() override;
  void PerformSecondaryAction() override;
  void LoadIcon(LoadIconCallback callback) const override;

  const std::u16string& guid() const { return guid_; }
  const base::Time& shared_time() const { return shared_time_; }
  const GURL& url() const { return url_; }

 private:
  static std::u16string GetSubtitle(const std::u16string& device_name,
                                    base::Time shared_time);

  std::u16string guid_;
  GURL url_;
  base::Time shared_time_;
  ui::ImageModel backup_icon_;
  // `activation_callback_` is triggered when the item is clicked by the user,
  // calling `OnItemPressed()` in `BirchSelfShareProvider` to mark the
  // corresponding `SendTabToSelfEntry` as opened.
  base::RepeatingClosure activation_callback_;
};

// A birch item which contains information about a tab that is currently playing
// media.
class ASH_EXPORT BirchLostMediaItem : public BirchItem {
 public:
  BirchLostMediaItem(const GURL& source_url,
                     const std::u16string& media_title,
                     bool is_video_conference_tab,
                     const ui::ImageModel& backup_icon,
                     base::RepeatingClosure activation_callback);
  BirchLostMediaItem(BirchLostMediaItem&&);
  BirchLostMediaItem(const BirchLostMediaItem&);
  BirchLostMediaItem& operator=(const BirchLostMediaItem&);
  bool operator==(const BirchLostMediaItem& rhs) const;
  ~BirchLostMediaItem() override;

  // BirchItem:
  BirchItemType GetType() const override;
  std::string ToString() const override;
  void PerformAction() override;
  void PerformSecondaryAction() override;
  void LoadIcon(LoadIconCallback callback) const override;

  const GURL& source_url() const { return source_url_; }
  const std::u16string& media_title() const { return media_title_; }
  bool is_video_conference_tab() const { return is_video_conference_tab_; }

 private:
  static std::u16string GetSubtitle(bool is_video_conference_tab);

  GURL source_url_;
  std::u16string media_title_;
  bool is_video_conference_tab_;
  ui::ImageModel backup_icon_;
  base::RepeatingClosure activation_callback_;
};

class ASH_EXPORT BirchWeatherItem : public BirchItem {
 public:
  BirchWeatherItem(const std::u16string& weather_description,
                   float temp_f,
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

  float temp_f() const { return temp_f_; }

 private:
  static std::u16string GetSubtitle(float temp_f);

  float temp_f_;
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

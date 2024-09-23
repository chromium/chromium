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
// If you are adding to this enum, please keep in sync with
// tools/metrics/histograms/metadata/ash/enums.xml as well as metrics in
// tools/metrics/histograms/metadata/ash/histograms.xml
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
  kLostMedia = 10,    // Tab that is currently playing media.
  kCoral = 11,        // Coral provider.
  kMaxValue = kCoral,
};

// These values are used to determine which secondary icon to load for the items
// that contain secondary icons.
enum class SecondaryIconType {
  kTabFromDesktop,            // Type that links to desktop icon.
  kTabFromPhone,              // Type that links to phone/portrait icon.
  kTabFromTablet,             // Type that links to tablet/landscape icon.
  kTabFromUnknown,            // Type that links to question-mark icon.
  kLostMediaAudio,            // Type that links to audio icon.
  kLostMediaVideo,            // Type that links to media icon.
  kLostMediaVideoConference,  // Type that links to video conference icon.
  kNoIcon,                    // Type where we will not load a secondary icon.
  kMaxValue = kNoIcon,
};

// These values are used to determine the types of chip add-ons which is an
// additional UI component like the join button of calendar item.
enum class BirchAddonType {
  kNone,         // No add-ons.
  kButton,       // A button with an action, e,g. the calendar join button.
  kCoralButton,  // A special button for coral, has a different UI and brings up
                 // a new UI on click.
  kWeatherTempLabelF,  // A label for weather temperature in Fahrenheit.
  kWeatherTempLabelC,  // A label for weather temperature in Celsius.
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
  // `is_post_login` is true if the chip was in a post-login overview session as
  // opposed to an in-session overview session.
  virtual void PerformAction(bool is_post_login) = 0;

  // Loads the icon for this image. This may invoke the callback immediately
  // (e.g. with a local icon) or there may be a delay for a network fetch.
  // The `SecondaryIconType` passed to `BirchChipButton` allows the view to set
  // a corresponding secondary icon image.
  using LoadIconCallback =
      base::OnceCallback<void(const ui::ImageModel&, SecondaryIconType)>;
  virtual void LoadIcon(LoadIconCallback callback) const = 0;

  // Records metrics when the user takes an action on the item (e.g. clicks or
  // taps on it).
  void RecordActionMetrics();

  virtual std::u16string GetAccessibleName() const;

  // Performs the action associated with the add-on of this item (e.g. joining a
  // meeting for Calendar). When the add-on action is available, `addon_label()`
  // will be set to the user-friendly action name.
  virtual void PerformAddonAction();
  virtual BirchAddonType GetAddonType() const;
  virtual std::u16string GetAddonAccessibleName() const;

  const std::u16string& title() const { return title_; }
  const std::u16string& subtitle() const { return subtitle_; }

  void set_ranking(float ranking) { ranking_ = ranking; }
  float ranking() const { return ranking_; }

  std::optional<std::u16string> addon_label() const { return addon_label_; }

  static void set_action_count_for_test(int value) { action_count_ = value; }

 protected:
  void set_addon_label(const std::u16string& addon_label) {
    addon_label_ = addon_label;
  }

 private:
  // The title to be displayed in birch chip UI.
  std::u16string title_;

  // The subtitle to be displayed in birch chip UI.
  std::u16string subtitle_;

  // The label for add-on component of the chip, e.g. "Join" on calendar join
  // button.
  std::optional<std::u16string> addon_label_;

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
  void PerformAction(bool is_post_login) override;
  void PerformAddonAction() override;
  void LoadIcon(LoadIconCallback callback) const override;
  BirchAddonType GetAddonType() const override;
  std::u16string GetAddonAccessibleName() const override;

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
  void PerformAction(bool is_post_login) override;
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
                const std::optional<std::string>& title,
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
  void PerformAction(bool is_post_login) override;
  void LoadIcon(LoadIconCallback callback) const override;

  const base::Time& timestamp() const { return timestamp_; }
  const std::string& file_id() const { return file_id_; }
  const std::string& icon_url() const { return icon_url_; }
  const base::FilePath& file_path() const { return file_path_; }

 private:
  static std::u16string GetTitle(const base::FilePath& file_path,
                                 const std::optional<std::string>& title);

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
               const DeviceFormFactor& form_factor);
  BirchTabItem(BirchTabItem&&);
  BirchTabItem(const BirchTabItem&);
  BirchTabItem& operator=(const BirchTabItem&);
  bool operator==(const BirchTabItem& rhs) const;
  ~BirchTabItem() override;

  // BirchItem:
  BirchItemType GetType() const override;
  std::string ToString() const override;
  void PerformAction(bool is_post_login) override;
  void LoadIcon(LoadIconCallback callback) const override;

  const GURL& url() const { return url_; }
  const base::Time& timestamp() const { return timestamp_; }
  const std::string& session_name() const { return session_name_; }
  DeviceFormFactor form_factor() const { return form_factor_; }
  const SecondaryIconType& secondary_icon_type() const {
    return secondary_icon_type_;
  }

 private:
  static std::u16string GetSubtitle(const std::string& session_name,
                                    base::Time timestamp);

  GURL url_;
  base::Time timestamp_;
  GURL favicon_url_;
  std::string session_name_;
  DeviceFormFactor form_factor_;
  SecondaryIconType secondary_icon_type_;
};

// A birch item for the last active URL.
class ASH_EXPORT BirchLastActiveItem : public BirchItem {
 public:
  BirchLastActiveItem(const std::u16string& title,
                      const GURL& page_url,
                      base::Time last_visit);
  BirchLastActiveItem(BirchLastActiveItem&&);
  BirchLastActiveItem(const BirchLastActiveItem&);
  BirchLastActiveItem& operator=(const BirchLastActiveItem&);
  bool operator==(const BirchLastActiveItem& rhs) const;
  ~BirchLastActiveItem() override;

  // BirchItem:
  BirchItemType GetType() const override;
  std::string ToString() const override;
  void PerformAction(bool is_post_login) override;
  void LoadIcon(LoadIconCallback callback) const override;

  const GURL& page_url() const { return page_url_; }

 private:
  static std::u16string GetSubtitle(base::Time last_visit);

  GURL page_url_;
};

// A birch item for a most-frequently-visited URL.
class ASH_EXPORT BirchMostVisitedItem : public BirchItem {
 public:
  BirchMostVisitedItem(const std::u16string& title, const GURL& page_url);
  BirchMostVisitedItem(BirchMostVisitedItem&&);
  BirchMostVisitedItem(const BirchMostVisitedItem&);
  BirchMostVisitedItem& operator=(const BirchMostVisitedItem&);
  bool operator==(const BirchMostVisitedItem& rhs) const;
  ~BirchMostVisitedItem() override;

  // BirchItem:
  BirchItemType GetType() const override;
  std::string ToString() const override;
  void PerformAction(bool is_post_login) override;
  void LoadIcon(LoadIconCallback callback) const override;

  const GURL& page_url() const { return page_url_; }

 private:
  static std::u16string GetSubtitle();

  GURL page_url_;
};

// A birch item which contains tabs shared to self information.
class ASH_EXPORT BirchSelfShareItem : public BirchItem {
 public:
  BirchSelfShareItem(const std::u16string& guid,
                     const std::u16string& title,
                     const GURL& url,
                     const base::Time& shared_time,
                     const std::u16string& device_name,
                     const SecondaryIconType& secondary_icon_type,
                     base::RepeatingClosure activation_callback);
  BirchSelfShareItem(BirchSelfShareItem&&);
  BirchSelfShareItem(const BirchSelfShareItem&);
  BirchSelfShareItem& operator=(const BirchSelfShareItem&);
  bool operator==(const BirchSelfShareItem& rhs) const;
  ~BirchSelfShareItem() override;

  // BirchItem:
  BirchItemType GetType() const override;
  std::string ToString() const override;
  void PerformAction(bool is_post_login) override;
  void LoadIcon(LoadIconCallback callback) const override;

  const std::u16string& guid() const { return guid_; }
  const base::Time& shared_time() const { return shared_time_; }
  const GURL& url() const { return url_; }
  const SecondaryIconType& secondary_icon_type() const {
    return secondary_icon_type_;
  }

 private:
  static std::u16string GetSubtitle(const std::u16string& device_name,
                                    base::Time shared_time);

  std::u16string guid_;
  GURL url_;
  base::Time shared_time_;
  SecondaryIconType secondary_icon_type_;
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
                     const std::optional<ui::ImageModel>& backup_icon,
                     const SecondaryIconType& secondary_icon_type,
                     base::RepeatingClosure activation_callback);
  BirchLostMediaItem(BirchLostMediaItem&&);
  BirchLostMediaItem(const BirchLostMediaItem&);
  BirchLostMediaItem& operator=(const BirchLostMediaItem&);
  bool operator==(const BirchLostMediaItem& rhs) const;
  ~BirchLostMediaItem() override;

  // BirchItem:
  BirchItemType GetType() const override;
  std::string ToString() const override;
  void PerformAction(bool is_post_login) override;
  void LoadIcon(LoadIconCallback callback) const override;

  const GURL& source_url() const { return source_url_; }
  const std::u16string& media_title() const { return media_title_; }
  const SecondaryIconType& secondary_icon_type() const {
    return secondary_icon_type_;
  }

 private:
  static std::u16string GetSubtitle(SecondaryIconType type);

  GURL source_url_;
  std::u16string media_title_;
  std::optional<ui::ImageModel> backup_icon_;
  SecondaryIconType secondary_icon_type_;
  base::RepeatingClosure activation_callback_;
};

class ASH_EXPORT BirchWeatherItem : public BirchItem {
 public:
  BirchWeatherItem(const std::u16string& weather_description,
                   float temp_f,
                   const GURL& icon_url);
  BirchWeatherItem(BirchWeatherItem&&);
  BirchWeatherItem(const BirchWeatherItem&);
  BirchWeatherItem& operator=(const BirchWeatherItem&);
  bool operator==(const BirchWeatherItem& rhs) const;
  ~BirchWeatherItem() override;

  // BirchItem:
  BirchItemType GetType() const override;
  std::string ToString() const override;
  void PerformAction(bool is_post_login) override;
  void LoadIcon(LoadIconCallback callback) const override;
  std::u16string GetAccessibleName() const override;
  void PerformAddonAction() override;
  BirchAddonType GetAddonType() const override;

  float temp_f() const { return temp_f_; }

 private:
  static int GetTemperature(float temp_f);
  static bool UseCelsius();

  float temp_f_;
  GURL icon_url_;
};

class ASH_EXPORT BirchReleaseNotesItem : public BirchItem {
 public:
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
  void PerformAction(bool is_post_login) override;
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

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_item.h"

#include <limits>
#include <sstream>
#include <string>

#include "ash/birch/birch_icon_cache.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "components/prefs/pref_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {
namespace {

constexpr net::NetworkTrafficAnnotationTag kIconDownloaderTrafficTag =
    net::DefineNetworkTrafficAnnotation("glanceables_icon_downloader", R"(
        semantics {
          sender: "Post-login glanceables"
          description:
            "Downloads icons for suggestion chip buttons for activities the "
            "user might want to perform after login or from overview mode "
            "(e.g. view a calendar event or open a file)."
          trigger: "User logs in to device or enters overview mode."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
          user_data {
            type: NONE
          }
          internal {
            contacts {
              email: "chromeos-launcher@google.com"
            }
          }
          last_reviewed: "2024-05-29"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature can be enabled/disabled by the user in the "
            "suggestion chip button context menu."
          chrome_policy {
            ContextualGoogleIntegrationsEnabled {
              ContextualGoogleIntegrationsEnabled: false
            }
          }
        })");

// Handles when an `image` is downloaded, by converting it to a ui::ImageModel
// and running `callback`.
void OnImageDownloaded(const GURL& url,
                       base::OnceCallback<void(const ui::ImageModel&)> callback,
                       const gfx::ImageSkia& image) {
  // Add the image to the cache.
  Shell::Get()->birch_model()->icon_cache()->Put(url.spec(), image);
  std::move(callback).Run(ui::ImageModel::FromImageSkia(image));
}

// Downloads an image from `url` and invokes `callback` with the image. If the
// `url` is invalid, invokes `callback` with an empty image.
void DownloadImageFromUrl(
    const GURL& url,
    base::OnceCallback<void(const ui::ImageModel&)> callback) {
  if (!url.is_valid()) {
    std::move(callback).Run(ui::ImageModel());
    return;
  }

  // Look for the icon in the cache.
  gfx::ImageSkia icon =
      Shell::Get()->birch_model()->icon_cache()->Get(url.spec());
  if (!icon.isNull()) {
    // Use the cached icon.
    std::move(callback).Run(ui::ImageModel::FromImageSkia(icon));
    return;
  }

  // Download the icon.
  const UserSession* active_user_session =
      Shell::Get()->session_controller()->GetUserSession(0);
  CHECK(active_user_session);

  ImageDownloader::Get()->Download(
      url, kIconDownloaderTrafficTag, active_user_session->user_info.account_id,
      base::BindOnce(&OnImageDownloaded, url, std::move(callback)));
}

// Returns the pref service to use for Birch item prefs.
PrefService* GetPrefService() {
  if (!Shell::HasInstance()) {
    CHECK_IS_TEST();
    return nullptr;
  }
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

}  // namespace

int BirchItem::action_count_ = 0;

BirchItem::BirchItem(const std::u16string& title,
                     const std::u16string& subtitle)
    : title_(title),
      subtitle_(subtitle),
      ranking_(std::numeric_limits<float>::max()) {}

BirchItem::BirchItem(BirchItem&&) = default;

BirchItem& BirchItem::operator=(BirchItem&&) = default;

BirchItem::BirchItem(const BirchItem&) = default;

BirchItem& BirchItem::operator=(const BirchItem&) = default;

BirchItem::~BirchItem() = default;

bool BirchItem::operator==(const BirchItem& rhs) const = default;

// static
void BirchItem::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kBirchUseCelsius, false);
}

void BirchItem::RecordActionMetrics() {
  // Record that the whole bar was activated.
  base::UmaHistogramBoolean("Ash.Birch.Bar.Activate", true);
  // Record which chip type was activated.
  base::UmaHistogramEnumeration("Ash.Birch.Chip.Activate", GetType());
  // Record the ranking of the activated chip.
  base::UmaHistogramCounts100("Ash.Birch.Chip.ActivatedRanking",
                              static_cast<int>(ranking()));
  // Record the types of the first 3 actions in a session.
  ++action_count_;
  if (action_count_ == 1) {
    base::UmaHistogramEnumeration("Ash.Birch.Chip.ActivateFirst", GetType());
  } else if (action_count_ == 2) {
    base::UmaHistogramEnumeration("Ash.Birch.Chip.ActivateSecond", GetType());
  } else if (action_count_ == 3) {
    base::UmaHistogramEnumeration("Ash.Birch.Chip.ActivateThird", GetType());
  }
}

////////////////////////////////////////////////////////////////////////////////

BirchCalendarItem::BirchCalendarItem(const std::u16string& title,
                                     const base::Time& start_time,
                                     const base::Time& end_time,
                                     const GURL& calendar_url,
                                     const GURL& conference_url,
                                     const std::string& event_id,
                                     const bool all_day_event)
    : BirchItem(title, GetSubtitle(start_time, end_time, all_day_event)),
      start_time_(start_time),
      end_time_(end_time),
      all_day_event_(all_day_event),
      calendar_url_(calendar_url),
      conference_url_(conference_url),
      event_id_(event_id) {
  if (ShouldShowJoinButton()) {
    set_secondary_action(
        l10n_util::GetStringUTF16(IDS_ASH_BIRCH_CALENDAR_JOIN_BUTTON));
  }
}

BirchCalendarItem::BirchCalendarItem(BirchCalendarItem&&) = default;

BirchCalendarItem::BirchCalendarItem(const BirchCalendarItem&) = default;

BirchCalendarItem& BirchCalendarItem::operator=(const BirchCalendarItem&) =
    default;

BirchCalendarItem::~BirchCalendarItem() = default;

BirchItemType BirchCalendarItem::GetType() const {
  return BirchItemType::kCalendar;
}

std::string BirchCalendarItem::ToString() const {
  std::stringstream ss;
  using base::UTF16ToUTF8;
  ss << "Calendar item: {ranking: " << ranking()
     << ", title: " << UTF16ToUTF8(title()) << ", start: "
     << UTF16ToUTF8(base::TimeFormatShortDateAndTime(start_time_))
     << ", end: " << UTF16ToUTF8(base::TimeFormatShortDateAndTime(end_time_))
     << ", conference_url: " << conference_url_.spec()
     << ", event_id: " << event_id_ << "}";
  return ss.str();
}

void BirchCalendarItem::PerformAction() {
  if (!calendar_url_.is_valid()) {
    LOG(ERROR) << "No valid URL for calendar item";
    return;
  }
  RecordActionMetrics();
  NewWindowDelegate::GetPrimary()->OpenUrl(
      calendar_url_, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void BirchCalendarItem::PerformSecondaryAction() {
  if (!conference_url_.is_valid()) {
    LOG(ERROR) << "No conference URL for calendar item";
    return;
  }
  // TODO(jamescook): Decide if we want differerent metrics for secondary
  // actions.
  RecordActionMetrics();
  NewWindowDelegate::GetPrimary()->OpenUrl(
      conference_url_, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void BirchCalendarItem::LoadIcon(LoadIconCallback callback) const {
  std::move(callback).Run(ui::ImageModel::FromVectorIcon(kCalendarEventIcon));
}

// static
std::u16string BirchCalendarItem::GetSubtitle(base::Time start_time,
                                              base::Time end_time,
                                              bool all_day_event) {
  base::Time now = base::Time::Now();
  if (start_time < now && now < end_time) {
    // This event is set to last all day.
    if (all_day_event) {
      return l10n_util::GetStringUTF16(IDS_ASH_BIRCH_CALENDAR_ALL_DAY);
    }

    // This is an ongoing event. Return "Now · Ends 11:20 AM".
    return l10n_util::GetStringFUTF16(IDS_ASH_BIRCH_CALENDAR_ONGOING_SUBTITLE,
                                      base::TimeFormatTimeOfDay(end_time));
  }
  if (start_time < now + base::Minutes(30)) {
    // This event is starting soon. Return "In 5 mins · 10:00 AM - 10:30 AM".
    int minutes = (start_time - now).InMinutes();
    return l10n_util::GetPluralStringFUTF16(IDS_ASH_BIRCH_CALENDAR_MINUTES,
                                            minutes) +
           u" · " + GetStartEndString(start_time, end_time);
  }
  if (now.LocalMidnight() + base::Days(1) < start_time) {
    // This event starts tomorrow. We don't show events more than 1 day in the
    // future, so we don't need to worry about days other than "tomorrow".
    // Return "Tomorrow · 10:00 AM - 11:30 AM"
    return l10n_util::GetStringUTF16(IDS_ASH_BIRCH_CALENDAR_TOMORROW) + u" · " +
           GetStartEndString(start_time, end_time);
  }
  // Otherwise return "10:00 AM - 11:30 AM".
  return GetStartEndString(start_time, end_time);
}

// static
std::u16string BirchCalendarItem::GetStartEndString(base::Time start_time,
                                                    base::Time end_time) {
  // Return "10:00 AM - 10:30 AM".
  return base::TimeFormatTimeOfDay(start_time) + u" - " +
         base::TimeFormatTimeOfDay(end_time);
}

bool BirchCalendarItem::ShouldShowJoinButton() const {
  if (!conference_url_.is_valid()) {
    return false;
  }
  // Only show "Join" if the meeting is starting soon or happening right now.
  base::Time start_adjusted = start_time_ - base::Minutes(5);
  base::Time now = base::Time::Now();
  return start_adjusted < now && now < end_time_;
}

////////////////////////////////////////////////////////////////////////////////

BirchAttachmentItem::BirchAttachmentItem(const std::u16string& title,
                                         const GURL& file_url,
                                         const GURL& icon_url,
                                         const base::Time& start_time,
                                         const base::Time& end_time,
                                         const std::string& file_id)
    : BirchItem(title, GetSubtitle(start_time, end_time)),
      file_url_(file_url),
      icon_url_(icon_url),
      start_time_(start_time),
      end_time_(end_time),
      file_id_(file_id) {}

BirchAttachmentItem::BirchAttachmentItem(BirchAttachmentItem&&) = default;

BirchAttachmentItem& BirchAttachmentItem::operator=(BirchAttachmentItem&&) =
    default;

BirchAttachmentItem::BirchAttachmentItem(const BirchAttachmentItem&) = default;

BirchAttachmentItem& BirchAttachmentItem::operator=(
    const BirchAttachmentItem&) = default;

BirchAttachmentItem::~BirchAttachmentItem() = default;

BirchItemType BirchAttachmentItem::GetType() const {
  return BirchItemType::kAttachment;
}

std::string BirchAttachmentItem::ToString() const {
  std::stringstream ss;
  using base::UTF16ToUTF8;
  ss << "Attachment item: {ranking: " << ranking()
     << ", title: " << UTF16ToUTF8(title())
     << ", file_url: " << file_url_.spec() << ", icon_url: " << icon_url_.spec()
     << ", start: "
     << UTF16ToUTF8(base::TimeFormatShortDateAndTime(start_time_))
     << ", end: " << UTF16ToUTF8(base::TimeFormatShortDateAndTime(end_time_))
     << ", file_id: " << file_id_ << "}";
  return ss.str();
}

void BirchAttachmentItem::PerformAction() {
  if (!file_url_.is_valid()) {
    LOG(ERROR) << "No valid URL for attachment item";
  }
  RecordActionMetrics();
  NewWindowDelegate::GetPrimary()->OpenUrl(
      file_url_, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void BirchAttachmentItem::PerformSecondaryAction() {
  NOTREACHED_IN_MIGRATION();
}

void BirchAttachmentItem::LoadIcon(LoadIconCallback callback) const {
  DownloadImageFromUrl(icon_url_, std::move(callback));
}

// static
std::u16string BirchAttachmentItem::GetSubtitle(base::Time start_time,
                                                base::Time end_time) {
  base::Time now = base::Time::Now();
  if (start_time < now && now < end_time) {
    // This event is happening now.
    return l10n_util::GetStringUTF16(
        IDS_ASH_BIRCH_CALENDAR_ATTACHMENT_NOW_SUBTITLE);
  }
  // This event will happen in the future.
  return l10n_util::GetStringUTF16(
      IDS_ASH_BIRCH_CALENDAR_ATTACHMENT_UPCOMING_SUBTITLE);
}

////////////////////////////////////////////////////////////////////////////////

BirchFileItem::BirchFileItem(const base::FilePath& file_path,
                             const std::u16string& justification,
                             base::Time timestamp,
                             const std::string& file_id,
                             const std::string& icon_url)
    : BirchItem(GetTitle(file_path), justification),
      file_id_(file_id),
      icon_url_(icon_url),
      file_path_(file_path),
      timestamp_(timestamp) {}

BirchFileItem::BirchFileItem(BirchFileItem&&) = default;

BirchFileItem::BirchFileItem(const BirchFileItem&) = default;

BirchFileItem& BirchFileItem::operator=(const BirchFileItem&) = default;

bool BirchFileItem::operator==(const BirchFileItem& rhs) const = default;

BirchFileItem::~BirchFileItem() = default;

BirchItemType BirchFileItem::GetType() const {
  return BirchItemType::kFile;
}

std::string BirchFileItem::ToString() const {
  std::stringstream ss;
  ss << "File item : {ranking: " << ranking()
     << ", title: " << base::UTF16ToUTF8(title())
     << ", file_path:" << file_path_ << ", timestamp: "
     << base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(timestamp_))
     << ", file_id: " << file_id_ << "}" << ", icon_url: " << icon_url_;
  return ss.str();
}

void BirchFileItem::PerformAction() {
  RecordActionMetrics();
  NewWindowDelegate::GetPrimary()->OpenFile(file_path_);
}

void BirchFileItem::PerformSecondaryAction() {
  NOTREACHED_IN_MIGRATION();
}

void BirchFileItem::LoadIcon(LoadIconCallback callback) const {
  DownloadImageFromUrl(GURL(icon_url_), std::move(callback));
}

// static
std::u16string BirchFileItem::GetTitle(const base::FilePath& file_path) {
  // Convert "/path/to/foo.txt" into just "foo".
  std::string filename = file_path.BaseName().RemoveExtension().value();
  return base::UTF8ToUTF16(filename);
}

////////////////////////////////////////////////////////////////////////////////

BirchWeatherItem::BirchWeatherItem(const std::u16string& weather_description,
                                   float temp_f,
                                   ui::ImageModel icon)
    : BirchItem(weather_description, GetSubtitle(temp_f)),
      temp_f_(temp_f),
      icon_(std::move(icon)) {}

BirchWeatherItem::BirchWeatherItem(BirchWeatherItem&&) = default;

BirchWeatherItem::BirchWeatherItem(const BirchWeatherItem&) = default;

BirchWeatherItem& BirchWeatherItem::operator=(const BirchWeatherItem&) =
    default;

bool BirchWeatherItem::operator==(const BirchWeatherItem& rhs) const = default;

BirchWeatherItem::~BirchWeatherItem() = default;

BirchItemType BirchWeatherItem::GetType() const {
  return BirchItemType::kWeather;
}

std::string BirchWeatherItem::ToString() const {
  std::stringstream ss;
  ss << "Weather item: {ranking: " << ranking()
     << ", title : " << base::UTF16ToUTF8(title()) << ", temp_f:" << temp_f_
     << "}";
  return ss.str();
}

void BirchWeatherItem::PerformAction() {
  RecordActionMetrics();
  // TODO(jamescook): Localize the query string.
  GURL url("https://google.com/search?q=weather");
  NewWindowDelegate::GetPrimary()->OpenUrl(
      url, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void BirchWeatherItem::PerformSecondaryAction() {
  NOTREACHED_IN_MIGRATION();
}

void BirchWeatherItem::LoadIcon(LoadIconCallback callback) const {
  std::move(callback).Run(icon_);
}

// static
std::u16string BirchWeatherItem::GetSubtitle(float temp_f) {
  // Tests may not have a pref service.
  bool use_celsius = false;
  PrefService* pref_service = GetPrefService();
  if (pref_service) {
    use_celsius = pref_service->GetBoolean(prefs::kBirchUseCelsius);
  } else {
    CHECK_IS_TEST();
  }
  return use_celsius
             ? l10n_util::GetStringFUTF16Int(
                   IDS_ASH_AMBIENT_MODE_WEATHER_TEMPERATURE_IN_CELSIUS,
                   static_cast<int>((temp_f - 32) * 5 / 9))
             : l10n_util::GetStringFUTF16Int(
                   IDS_ASH_AMBIENT_MODE_WEATHER_TEMPERATURE_IN_FAHRENHEIT,
                   static_cast<int>(temp_f));
}

////////////////////////////////////////////////////////////////////////////////

BirchTabItem::BirchTabItem(const std::u16string& title,
                           const GURL& url,
                           const base::Time& timestamp,
                           const GURL& favicon_url,
                           const std::string& session_name,
                           const DeviceFormFactor& form_factor)
    : BirchItem(title, GetSubtitle(session_name, timestamp)),
      url_(url),
      timestamp_(timestamp),
      favicon_url_(favicon_url),
      session_name_(session_name),
      form_factor_(form_factor) {}

BirchTabItem::BirchTabItem(BirchTabItem&&) = default;

BirchTabItem::BirchTabItem(const BirchTabItem&) = default;

BirchTabItem& BirchTabItem::operator=(const BirchTabItem&) = default;

bool BirchTabItem::operator==(const BirchTabItem& rhs) const = default;

BirchTabItem::~BirchTabItem() = default;

BirchItemType BirchTabItem::GetType() const {
  return BirchItemType::kTab;
}

std::string BirchTabItem::ToString() const {
  std::stringstream ss;
  ss << "Tab item: {ranking: " << ranking()
     << ", title: " << base::UTF16ToUTF8(title()) << ", url:" << url_
     << ", timestamp:" << timestamp_ << ", favicon_url:" << favicon_url_
     << ", session_name:" << session_name_
     << ", form_factor:" << static_cast<int>(form_factor_) << "}";
  return ss.str();
}

void BirchTabItem::PerformAction() {
  if (!url_.is_valid()) {
    LOG(ERROR) << "No valid URL for tab item";
    return;
  }
  RecordActionMetrics();
  NewWindowDelegate::GetPrimary()->OpenUrl(
      url_, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kSwitchToTab);
}

void BirchTabItem::PerformSecondaryAction() {
  NOTREACHED_IN_MIGRATION();
}

void BirchTabItem::LoadIcon(LoadIconCallback callback) const {
  DownloadImageFromUrl(favicon_url_, std::move(callback));
}

// static
std::u16string BirchTabItem::GetSubtitle(const std::string& session_name,
                                         base::Time timestamp) {
  std::u16string prefix;
  if (timestamp < base::Time::Now().LocalMidnight()) {
    // Builds the string "Yesterday". We only show tabs within the last 24 hours
    // so we don't need to worry about days before yesterday.
    prefix =
        l10n_util::GetStringUTF16(IDS_ASH_BIRCH_RECENT_TAB_SUBTITLE_YESTERDAY);
  } else {
    // Builds a string like "12 hours ago". We only show tabs within the last
    // 24 hours so we don't need to worry about a day count.
    int hours = (base::Time::Now() - timestamp).InHours();
    prefix = l10n_util::GetPluralStringFUTF16(
        IDS_ASH_BIRCH_RECENT_TAB_SUBTITLE_PREFIX, hours);
  }

  // Builds a string like "From Chromebook".
  std::u16string suffix =
      l10n_util::GetStringFUTF16(IDS_ASH_BIRCH_RECENT_TAB_SUBTITLE_SUFFIX,
                                 base::UTF8ToUTF16(session_name));
  return prefix + u" · " + suffix;
}

////////////////////////////////////////////////////////////////////////////////

BirchLastActiveItem::BirchLastActiveItem(const std::u16string& title,
                                         const GURL& url,
                                         base::Time last_visit,
                                         ui::ImageModel icon)
    : BirchItem(title, GetSubtitle(last_visit)), url_(url), icon_(icon) {}

BirchLastActiveItem::BirchLastActiveItem(BirchLastActiveItem&&) = default;

BirchLastActiveItem::BirchLastActiveItem(const BirchLastActiveItem&) = default;

BirchLastActiveItem& BirchLastActiveItem::operator=(
    const BirchLastActiveItem&) = default;

bool BirchLastActiveItem::operator==(const BirchLastActiveItem& rhs) const =
    default;

BirchLastActiveItem::~BirchLastActiveItem() = default;

BirchItemType BirchLastActiveItem::GetType() const {
  return BirchItemType::kLastActive;
}

std::string BirchLastActiveItem::ToString() const {
  std::stringstream ss;
  ss << "Last active item: {ranking: " << ranking()
     << ", Title: " << base::UTF16ToUTF8(title()) << ", URL: " << url_;
  return ss.str();
}

void BirchLastActiveItem::PerformAction() {
  if (!url_.is_valid()) {
    LOG(ERROR) << "No valid URL for last active item";
    return;
  }
  RecordActionMetrics();
  NewWindowDelegate::GetPrimary()->OpenUrl(
      url_, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kSwitchToTab);
}

void BirchLastActiveItem::PerformSecondaryAction() {
  NOTREACHED_IN_MIGRATION();
}

void BirchLastActiveItem::LoadIcon(LoadIconCallback callback) const {
  std::move(callback).Run(icon_);
}

// static
std::u16string BirchLastActiveItem::GetSubtitle(base::Time last_visit) {
  std::u16string prefix;
  if (last_visit < base::Time::Now().LocalMidnight() - base::Days(1)) {
    // If the last visit was before yesterday, show "X days ago".
    int days = (base::Time::Now() - last_visit).InDays();
    prefix = l10n_util::GetPluralStringFUTF16(
        IDS_ASH_BIRCH_LAST_ACTIVE_SUBTITLE_DAYS_AGO, days);
  } else if (last_visit < base::Time::Now().LocalMidnight()) {
    // If the last visit was yesterday show "Yesterday", which is a common case
    // in the mornings.
    prefix =
        l10n_util::GetStringUTF16(IDS_ASH_BIRCH_LAST_ACTIVE_SUBTITLE_YESTERDAY);
  } else {
    // Builds a string like "12 hours ago".
    int hours = (base::Time::Now() - last_visit).InHours();
    prefix = l10n_util::GetPluralStringFUTF16(
        IDS_ASH_BIRCH_LAST_ACTIVE_SUBTITLE_PREFIX, hours);
  }

  // Builds a string like "Continue browsing".
  std::u16string suffix =
      l10n_util::GetStringUTF16(IDS_ASH_BIRCH_LAST_ACTIVE_SUBTITLE_SUFFIX);
  return prefix + u" · " + suffix;
}

////////////////////////////////////////////////////////////////////////////////

BirchMostVisitedItem::BirchMostVisitedItem(const std::u16string& title,
                                           const GURL& url,
                                           ui::ImageModel icon)
    : BirchItem(title, GetSubtitle()), url_(url), icon_(icon) {}

BirchMostVisitedItem::BirchMostVisitedItem(BirchMostVisitedItem&&) = default;

BirchMostVisitedItem::BirchMostVisitedItem(const BirchMostVisitedItem&) =
    default;

BirchMostVisitedItem& BirchMostVisitedItem::operator=(
    const BirchMostVisitedItem&) = default;

bool BirchMostVisitedItem::operator==(const BirchMostVisitedItem& rhs) const =
    default;

BirchMostVisitedItem::~BirchMostVisitedItem() = default;

BirchItemType BirchMostVisitedItem::GetType() const {
  return BirchItemType::kMostVisited;
}

std::string BirchMostVisitedItem::ToString() const {
  std::stringstream ss;
  ss << "Most Visited item: {ranking: " << ranking()
     << ", Title: " << base::UTF16ToUTF8(title()) << ", URL: " << url_;
  return ss.str();
}

void BirchMostVisitedItem::PerformAction() {
  if (!url_.is_valid()) {
    LOG(ERROR) << "No valid URL for most visited item";
    return;
  }
  RecordActionMetrics();
  NewWindowDelegate::GetPrimary()->OpenUrl(
      url_, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kSwitchToTab);
}

void BirchMostVisitedItem::PerformSecondaryAction() {
  NOTREACHED_IN_MIGRATION();
}

void BirchMostVisitedItem::LoadIcon(LoadIconCallback callback) const {
  std::move(callback).Run(icon_);
}

// static
std::u16string BirchMostVisitedItem::GetSubtitle() {
  return l10n_util::GetStringUTF16(IDS_ASH_BIRCH_MOST_VISITED_SUBTITLE);
}

////////////////////////////////////////////////////////////////////////////////

BirchSelfShareItem::BirchSelfShareItem(const std::u16string& guid,
                                       const std::u16string& title,
                                       const GURL& url,
                                       const base::Time& shared_time,
                                       const std::u16string& device_name,
                                       const GURL& favicon_url,
                                       base::RepeatingClosure callback)
    : BirchItem(title, GetSubtitle(device_name, shared_time)),
      guid_(guid),
      url_(url),
      shared_time_(shared_time),
      favicon_url_(favicon_url),
      activation_callback_(std::move(callback)) {}

BirchSelfShareItem::BirchSelfShareItem(BirchSelfShareItem&&) = default;

BirchSelfShareItem::BirchSelfShareItem(const BirchSelfShareItem&) = default;

BirchSelfShareItem& BirchSelfShareItem::operator=(const BirchSelfShareItem&) =
    default;

bool BirchSelfShareItem::operator==(const BirchSelfShareItem& rhs) const =
    default;

BirchSelfShareItem::~BirchSelfShareItem() = default;

BirchItemType BirchSelfShareItem::GetType() const {
  return BirchItemType::kSelfShare;
}

std::string BirchSelfShareItem::ToString() const {
  std::stringstream ss;
  ss << "Self Share item: {ranking: " << ranking()
     << ", Title: " << base::UTF16ToUTF8(title())
     << ", Device Name: " << base::UTF16ToUTF8(subtitle())
     << ", GUID: " << guid_ << ", Shared Time: " << shared_time_
     << ", URL: " << url_ << ", Favicon URL: " << favicon_url_;
  return ss.str();
}

void BirchSelfShareItem::PerformAction() {
  if (!url_.is_valid()) {
    LOG(ERROR) << "No valid URL for self "
                  "share item";
    return;
  }
  if (activation_callback_) {
    activation_callback_.Run();
  }
  RecordActionMetrics();
  NewWindowDelegate::GetPrimary()->OpenUrl(
      url_, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kSwitchToTab);
}

void BirchSelfShareItem::PerformSecondaryAction() {
  NOTREACHED_IN_MIGRATION();
}

void BirchSelfShareItem::LoadIcon(LoadIconCallback callback) const {
  // TODO(b/333412417): Set a generic website icon if empty image result.
  DownloadImageFromUrl(favicon_url_, std::move(callback));
}

// static
std::u16string BirchSelfShareItem::GetSubtitle(
    const std::u16string& device_name,
    base::Time shared_time) {
  std::u16string prefix;
  if (shared_time < base::Time::Now().LocalMidnight()) {
    // Builds the string "Yesterday". We only show tabs within the last 24 hours
    // so we don't need to worry about days before yesterday.
    prefix =
        l10n_util::GetStringUTF16(IDS_ASH_BIRCH_SELF_SHARE_SUBTITLE_YESTERDAY);
  } else {
    // Builds a string like "12 hours ago". We only show tabs within the last
    // 24 hours so we don't need to worry about a day count.
    const int hours = (base::Time::Now() - shared_time).InHours();
    prefix = l10n_util::GetPluralStringFUTF16(
        IDS_ASH_BIRCH_SELF_SHARE_SUBTITLE_PREFIX, hours);
  }

  // Builds a string like "Sent from Chromebook".
  std::u16string suffix = l10n_util::GetStringFUTF16(
      IDS_ASH_BIRCH_SELF_SHARE_SUBTITLE_SUFFIX, device_name);
  return prefix + u" · " + suffix;
}

////////////////////////////////////////////////////////////////////////////////

BirchReleaseNotesItem::BirchReleaseNotesItem(
    const std::u16string& release_notes_title,
    const std::u16string& release_notes_text,
    const GURL& url,
    const base::Time first_seen)
    : BirchItem(release_notes_title, release_notes_text),
      url_(url),
      first_seen_(first_seen) {}

BirchReleaseNotesItem::~BirchReleaseNotesItem() = default;

BirchItemType BirchReleaseNotesItem::GetType() const {
  return BirchItemType::kReleaseNotes;
}

std::string BirchReleaseNotesItem::ToString() const {
  std::stringstream ss;
  ss << "release_notes_title: " << base::UTF16ToUTF8(title())
     << ", release_notes_text:" << base::UTF16ToUTF8(subtitle())
     << ", url:" << url_ << ", ranking: " << ranking()
     << ", first seen: " << first_seen_;
  return ss.str();
}

void BirchReleaseNotesItem::PerformAction() {
  if (!url_.is_valid()) {
    LOG(ERROR) << "No valid URL for release notes item";
    return;
  }
  RecordActionMetrics();
  NewWindowDelegate::GetPrimary()->OpenUrl(
      url_, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void BirchReleaseNotesItem::PerformSecondaryAction() {
  NOTREACHED_IN_MIGRATION();
}

void BirchReleaseNotesItem::LoadIcon(LoadIconCallback callback) const {
  std::move(callback).Run(
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_BIRCH_RELEASE_NOTES_ICON));
}

}  // namespace ash

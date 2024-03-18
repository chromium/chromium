// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_item.h"

#include <limits>
#include <sstream>
#include <string>

#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"

namespace ash {
namespace {

// Handles when an `image` is downloaded, by converting it to a ui::ImageModel
// and running `callback`.
void OnImageDownloaded(base::OnceCallback<void(const ui::ImageModel&)> callback,
                       const gfx::ImageSkia& image) {
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

  const UserSession* active_user_session =
      Shell::Get()->session_controller()->GetUserSession(0);
  CHECK(active_user_session);

  // TODO(b/328088919): Update MISSING_TRAFFIC_ANNOTATION with real annotation.
  ImageDownloader::Get()->Download(
      url, MISSING_TRAFFIC_ANNOTATION,
      active_user_session->user_info.account_id,
      base::BindOnce(&OnImageDownloaded, std::move(callback)));
}

}  // namespace

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

////////////////////////////////////////////////////////////////////////////////

BirchCalendarItem::BirchCalendarItem(const std::u16string& title,
                                     const base::Time& start_time,
                                     const base::Time& end_time,
                                     const GURL& calendar_url,
                                     const GURL& conference_url)
    : BirchItem(title, GetSubtitle(start_time, end_time)),
      start_time_(start_time),
      end_time_(end_time),
      calendar_url_(calendar_url),
      conference_url_(conference_url) {
  if (conference_url_.is_valid()) {
    set_secondary_action(
        l10n_util::GetStringUTF16(IDS_ASH_BIRCH_CALENDAR_JOIN_BUTTON));
  }
}

BirchCalendarItem::BirchCalendarItem(BirchCalendarItem&&) = default;

BirchCalendarItem::BirchCalendarItem(const BirchCalendarItem&) = default;

BirchCalendarItem& BirchCalendarItem::operator=(const BirchCalendarItem&) =
    default;

BirchCalendarItem::~BirchCalendarItem() = default;

const char* BirchCalendarItem::GetItemType() const {
  return kItemType;
}

std::string BirchCalendarItem::ToString() const {
  std::stringstream ss;
  using base::UTF16ToUTF8;
  ss << "Calendar item: {ranking: " << ranking()
     << ", title: " << UTF16ToUTF8(title()) << ", start: "
     << UTF16ToUTF8(base::TimeFormatShortDateAndTime(start_time_))
     << ", end: " << UTF16ToUTF8(base::TimeFormatShortDateAndTime(end_time_))
     << ", conference_url: " << conference_url_.spec() << "}";
  return ss.str();
}

void BirchCalendarItem::PerformAction() {
  if (!calendar_url_.is_valid()) {
    LOG(ERROR) << "No valid URL for calendar item";
    return;
  }

  NewWindowDelegate::GetInstance()->OpenUrl(
      calendar_url_, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void BirchCalendarItem::PerformSecondaryAction() {
  if (!conference_url_.is_valid()) {
    LOG(ERROR) << "No conference URL for calendar item";
    return;
  }

  NewWindowDelegate::GetInstance()->OpenUrl(
      conference_url_, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void BirchCalendarItem::LoadIcon(LoadIconCallback callback) const {
  std::move(callback).Run(ui::ImageModel::FromVectorIcon(kCalendarEventIcon));
}

// static
std::u16string BirchCalendarItem::GetSubtitle(base::Time start_time,
                                              base::Time end_time) {
  return base::TimeFormatTimeOfDay(start_time) + u" - " +
         base::TimeFormatTimeOfDay(end_time);
}

////////////////////////////////////////////////////////////////////////////////

BirchAttachmentItem::BirchAttachmentItem(const std::u16string& title,
                                         const GURL& file_url,
                                         const GURL& icon_url,
                                         const base::Time& start_time,
                                         const base::Time& end_time)
    : BirchItem(title, GetSubtitle()),
      file_url_(file_url),
      icon_url_(icon_url),
      start_time_(start_time),
      end_time_(end_time) {}

BirchAttachmentItem::BirchAttachmentItem(BirchAttachmentItem&&) = default;

BirchAttachmentItem& BirchAttachmentItem::operator=(BirchAttachmentItem&&) =
    default;

BirchAttachmentItem::BirchAttachmentItem(const BirchAttachmentItem&) = default;

BirchAttachmentItem& BirchAttachmentItem::operator=(
    const BirchAttachmentItem&) = default;

BirchAttachmentItem::~BirchAttachmentItem() = default;

const char* BirchAttachmentItem::GetItemType() const {
  return kItemType;
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
     << "}";
  return ss.str();
}

void BirchAttachmentItem::PerformAction() {
  if (!file_url_.is_valid()) {
    LOG(ERROR) << "No valid URL for attachment item";
  }
  NewWindowDelegate::GetInstance()->OpenUrl(
      file_url_, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void BirchAttachmentItem::PerformSecondaryAction() {
  NOTREACHED();
}

void BirchAttachmentItem::LoadIcon(LoadIconCallback callback) const {
  DownloadImageFromUrl(icon_url_, std::move(callback));
}

// static
std::u16string BirchAttachmentItem::GetSubtitle() {
  return l10n_util::GetStringUTF16(IDS_ASH_BIRCH_CALENDAR_ATTACHMENT_SUBTITLE);
}

////////////////////////////////////////////////////////////////////////////////

BirchFileItem::BirchFileItem(const base::FilePath& file_path,
                             const std::u16string& justification,
                             base::Time timestamp)
    : BirchItem(GetTitle(file_path), justification),
      file_path_(file_path),
      timestamp_(timestamp) {}

BirchFileItem::BirchFileItem(BirchFileItem&&) = default;

BirchFileItem::BirchFileItem(const BirchFileItem&) = default;

BirchFileItem& BirchFileItem::operator=(const BirchFileItem&) = default;

bool BirchFileItem::operator==(const BirchFileItem& rhs) const = default;

BirchFileItem::~BirchFileItem() = default;

const char* BirchFileItem::GetItemType() const {
  return kItemType;
}

std::string BirchFileItem::ToString() const {
  std::stringstream ss;
  ss << "File item : {ranking: " << ranking()
     << ", title: " << base::UTF16ToUTF8(title())
     << ", file_path:" << file_path_ << ", timestamp: "
     << base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(timestamp_)) << "}";
  return ss.str();
}

void BirchFileItem::PerformAction() {
  NewWindowDelegate::GetInstance()->OpenFile(file_path_);
}

void BirchFileItem::PerformSecondaryAction() {
  NOTREACHED();
}

void BirchFileItem::LoadIcon(LoadIconCallback callback) const {
  const gfx::VectorIcon& icon = chromeos::GetIconForPath(file_path_);
  bool dark_mode = DarkLightModeController::Get()->IsDarkModeEnabled();
  SkColor color = chromeos::GetIconColorForPath(file_path_, dark_mode);
  std::move(callback).Run(ui::ImageModel::FromVectorIcon(icon, color));
}

// static
std::u16string BirchFileItem::GetTitle(const base::FilePath& file_path) {
  // Convert "/path/to/foo.txt" into just "foo".
  std::string filename = file_path.BaseName().RemoveExtension().value();
  return base::UTF8ToUTF16(filename);
}

////////////////////////////////////////////////////////////////////////////////

BirchWeatherItem::BirchWeatherItem(const std::u16string& weather_description,
                                   const std::u16string& temperature,
                                   ui::ImageModel icon)
    : BirchItem(weather_description, temperature),
      temperature_(temperature),
      icon_(std::move(icon)) {}

BirchWeatherItem::BirchWeatherItem(BirchWeatherItem&&) = default;

BirchWeatherItem::BirchWeatherItem(const BirchWeatherItem&) = default;

BirchWeatherItem& BirchWeatherItem::operator=(const BirchWeatherItem&) =
    default;

bool BirchWeatherItem::operator==(const BirchWeatherItem& rhs) const = default;

BirchWeatherItem::~BirchWeatherItem() = default;

const char* BirchWeatherItem::GetItemType() const {
  return kItemType;
}

std::string BirchWeatherItem::ToString() const {
  std::stringstream ss;
  ss << "Weather item: {ranking: " << ranking()
     << ", title : " << base::UTF16ToUTF8(title())
     << ", temperature:" << base::UTF16ToUTF8(temperature_) << "}";
  return ss.str();
}

void BirchWeatherItem::PerformAction() {
  // TODO(jamescook): Localize the query string.
  GURL url("https://google.com/search?q=weather");
  NewWindowDelegate::GetInstance()->OpenUrl(
      url, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void BirchWeatherItem::PerformSecondaryAction() {
  NOTREACHED();
}

void BirchWeatherItem::LoadIcon(LoadIconCallback callback) const {
  std::move(callback).Run(icon_);
}

////////////////////////////////////////////////////////////////////////////////

BirchTabItem::BirchTabItem(const std::u16string& title,
                           const GURL& url,
                           const base::Time& timestamp,
                           const GURL& favicon_url,
                           const std::string& session_name,
                           const DeviceFormFactor& form_factor)
    : BirchItem(title, GetSubtitle(session_name)),
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

const char* BirchTabItem::GetItemType() const {
  return kItemType;
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
  NewWindowDelegate::GetInstance()->OpenUrl(
      url_, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void BirchTabItem::PerformSecondaryAction() {
  NOTREACHED();
}

void BirchTabItem::LoadIcon(LoadIconCallback callback) const {
  DownloadImageFromUrl(favicon_url_, std::move(callback));
}

// static
std::u16string BirchTabItem::GetSubtitle(const std::string& session_name) {
  // Builds a string like "From Chromebook".
  return l10n_util::GetStringFUTF16(IDS_ASH_BIRCH_RECENT_TAB_SUBTITLE,
                                    base::UTF8ToUTF16(session_name));
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

const char* BirchReleaseNotesItem::GetItemType() const {
  return kItemType;
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

  NewWindowDelegate::GetInstance()->OpenUrl(
      url_, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void BirchReleaseNotesItem::PerformSecondaryAction() {
  NOTREACHED();
}

void BirchReleaseNotesItem::LoadIcon(LoadIconCallback callback) const {
  LOG(ERROR) << "Not implemented";
}

}  // namespace ash

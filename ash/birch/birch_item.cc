// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_item.h"

#include <limits>
#include <sstream>
#include <string>

#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
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

BirchItem::BirchItem(const std::u16string& title)
    : title(title), ranking(std::numeric_limits<float>::max()) {}

BirchItem::BirchItem(BirchItem&&) = default;

BirchItem& BirchItem::operator=(BirchItem&&) = default;

BirchItem::BirchItem(const BirchItem&) = default;

BirchItem& BirchItem::operator=(const BirchItem&) = default;

BirchItem::~BirchItem() = default;

bool BirchItem::operator==(const BirchItem& rhs) const = default;

////////////////////////////////////////////////////////////////////////////////

BirchCalendarItem::BirchCalendarItem(const std::u16string& title)
    : BirchItem(title) {}

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
  ss << "Calendar item: {ranking: " << ranking
     << ", title: " << UTF16ToUTF8(title)
     << ", start: " << UTF16ToUTF8(base::TimeFormatShortDateAndTime(start_time))
     << ", end: " << UTF16ToUTF8(base::TimeFormatShortDateAndTime(end_time))
     << ", conference_url: " << conference_url.spec() << "}";
  return ss.str();
}

void BirchCalendarItem::PerformAction() {
  GURL url;
  // Prefer the video conference URL if one is available. Otherwise open the
  // calendar event on Google Calendar.
  if (conference_url.is_valid()) {
    url = conference_url;
  } else if (calendar_url.is_valid()) {
    url = calendar_url;
  } else {
    LOG(ERROR) << "No valid URL for calendar item";
    return;
  }
  NewWindowDelegate::GetInstance()->OpenUrl(
      url, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void BirchCalendarItem::LoadIcon(LoadIconCallback callback) {
  // TODO(jamescook): Supply a static icon. The Google Calendar API does not
  // provide an icon for calendar events.
  std::move(callback).Run(ui::ImageModel());
}

////////////////////////////////////////////////////////////////////////////////

BirchAttachmentItem::BirchAttachmentItem(const std::u16string& title)
    : BirchItem(title) {}

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
  ss << "Attachment item: {ranking: " << ranking
     << ", title: " << UTF16ToUTF8(title) << ", file_url: " << file_url.spec()
     << ", icon_url: " << icon_url.spec()
     << ", start: " << UTF16ToUTF8(base::TimeFormatShortDateAndTime(start_time))
     << ", end: " << UTF16ToUTF8(base::TimeFormatShortDateAndTime(end_time))
     << "}";
  return ss.str();
}

void BirchAttachmentItem::PerformAction() {
  if (!file_url.is_valid()) {
    LOG(ERROR) << "No valid URL for attachment item";
  }
  NewWindowDelegate::GetInstance()->OpenUrl(
      file_url, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void BirchAttachmentItem::LoadIcon(LoadIconCallback callback) {
  DownloadImageFromUrl(icon_url, std::move(callback));
}

////////////////////////////////////////////////////////////////////////////////

BirchFileItem::BirchFileItem(const base::FilePath& file_path,
                             base::Time timestamp)
    : BirchItem(base::UTF8ToUTF16(file_path.BaseName().value())),
      file_path(file_path),
      timestamp(timestamp) {}

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
  ss << "File item : {ranking: " << ranking
     << ", title: " << base::UTF16ToUTF8(title) << ", file_path:" << file_path
     << ", timestamp: "
     << base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(timestamp)) << "}";
  return ss.str();
}

void BirchFileItem::PerformAction() {
  NewWindowDelegate::GetInstance()->OpenFile(file_path);
}

void BirchFileItem::LoadIcon(LoadIconCallback callback) {
  std::move(callback).Run(
      ui::ImageModel::FromVectorIcon(chromeos::GetIconForPath(file_path)));
}

////////////////////////////////////////////////////////////////////////////////

BirchWeatherItem::BirchWeatherItem(const std::u16string& weather_description,
                                   const std::u16string& temperature,
                                   ui::ImageModel icon)
    : BirchItem(weather_description),
      temperature(temperature),
      icon(std::move(icon)) {}

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
  ss << "Weather item: {ranking: " << ranking
     << ", title : " << base::UTF16ToUTF8(title)
     << ", temperature:" << base::UTF16ToUTF8(temperature) << "}";
  return ss.str();
}

void BirchWeatherItem::PerformAction() {
  // TODO(jamescook): Localize the query string.
  GURL url("https://google.com/search?q=weather");
  NewWindowDelegate::GetInstance()->OpenUrl(
      url, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void BirchWeatherItem::LoadIcon(LoadIconCallback callback) {
  std::move(callback).Run(icon);
}

////////////////////////////////////////////////////////////////////////////////

BirchTabItem::BirchTabItem(const std::u16string& title,
                           const GURL& url,
                           const base::Time& timestamp,
                           const GURL& favicon_url,
                           const std::string& session_name,
                           const DeviceFormFactor& form_factor)
    : BirchItem(title),
      url(url),
      timestamp(timestamp),
      favicon_url(favicon_url),
      session_name(session_name),
      form_factor(form_factor) {}

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
  ss << "Tab item: {ranking: " << ranking
     << ", title: " << base::UTF16ToUTF8(title) << ", url:" << url
     << ", timestamp:" << timestamp << ", favicon_url:" << favicon_url
     << ", session_name:" << session_name
     << ", form_factor:" << static_cast<int>(form_factor) << "}";
  return ss.str();
}

void BirchTabItem::PerformAction() {
  if (!url.is_valid()) {
    LOG(ERROR) << "No valid URL for tab item";
    return;
  }
  NewWindowDelegate::GetInstance()->OpenUrl(
      url, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void BirchTabItem::LoadIcon(LoadIconCallback callback) {
  DownloadImageFromUrl(favicon_url, std::move(callback));
}

}  // namespace ash

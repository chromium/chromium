// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr personalization_app::mojom::TopicSource kTopicSource =
    personalization_app::mojom::TopicSource::kGooglePhotos;

constexpr AmbientModeTemperatureUnit kTemperatureUnit =
    AmbientModeTemperatureUnit::kCelsius;

constexpr char kFakeUrl[] = "chrome://ambient";

constexpr char kFakeDetails[] = "fake-photo-attribution";

constexpr std::array<const char*, 2> kFakeBackupPhotoUrls = {
    "http://fake-backup-photo-1.com", "http://fake-backup-photo-2.com"};

AmbientSettings CreateFakeSettings() {
  AmbientSettings settings;
  settings.topic_source = kTopicSource;
  settings.temperature_unit = kTemperatureUnit;

  ArtSetting art_setting0;
  art_setting0.album_id = "0";
  art_setting0.enabled = true;
  art_setting0.title = "art0";
  art_setting0.visible = true;
  settings.art_settings.emplace_back(art_setting0);

  ArtSetting art_setting1;
  art_setting1.album_id = "1";
  art_setting1.enabled = false;
  art_setting1.title = "art1";
  art_setting1.visible = true;
  settings.art_settings.emplace_back(art_setting1);

  ArtSetting hidden_setting;
  hidden_setting.album_id = "2";
  hidden_setting.enabled = false;
  hidden_setting.title = "hidden";
  hidden_setting.visible = false;
  settings.art_settings.emplace_back(hidden_setting);

  settings.selected_album_ids = {"1"};
  return settings;
}

PersonalAlbums CreateFakeAlbums() {
  PersonalAlbums albums;
  PersonalAlbum album0;
  album0.album_id = "0";
  album0.album_name = "album0";
  albums.albums.emplace_back(std::move(album0));

  PersonalAlbum album1;
  album1.album_id = "1";
  album1.album_name = "album1";
  albums.albums.emplace_back(std::move(album1));

  return albums;
}

}  // namespace

FakeAmbientBackendControllerImpl::FakeAmbientBackendControllerImpl() = default;
FakeAmbientBackendControllerImpl::~FakeAmbientBackendControllerImpl() = default;

void FakeAmbientBackendControllerImpl::FetchScreenUpdateInfo(
    int num_topics,
    bool show_pair_personal_portraits,
    const gfx::Size& screen_size,
    OnScreenUpdateInfoFetchedCallback callback) {
  ash::ScreenUpdate update;

  if (custom_topic_generator_) {
    update.next_topics = custom_topic_generator_.Run(num_topics, screen_size);
  } else {
    int num_topics_to_return =
        custom_num_topics_to_return_.has_value()
            ? std::min(custom_num_topics_to_return_.value(), num_topics)
            : num_topics;
    for (int i = 0; i < num_topics_to_return; ++i) {
      ash::AmbientModeTopic topic;
      topic.url = kFakeUrl;
      topic.details = kFakeDetails;
      topic.is_portrait = is_portrait_;
      if ((show_pair_personal_portraits &&
           topic_type_ == ::ambient::kPersonal && is_portrait_) ||
          has_related_image_) {
        topic.related_image_url = kFakeUrl;
      }
      topic.topic_type = topic_type_;

      update.next_topics.emplace_back(topic);
    }
  }

  // Only respond weather info when there is no active weather testing.
  if (!weather_info_) {
    ash::WeatherInfo weather_info;
    weather_info.temp_f = .0f;
    weather_info.condition_icon_url = kFakeUrl;
    weather_info.show_celsius = true;
    update.weather_info = weather_info;
  }

  // Pretend to respond asynchronously.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), update));
}

void FakeAmbientBackendControllerImpl::FetchPreviewImages(
    const gfx::Size& preview_size,
    OnPreviewImagesFetchedCallback callback) {
  std::vector<GURL> urls = {GURL(kFakeUrl)};
  // Pretend to respond asynchronously.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), urls));
}

void FakeAmbientBackendControllerImpl::UpdateSettings(
    const AmbientSettings settings,
    UpdateSettingsCallback callback) {
  // |show_weather| should always be set to true.
  DCHECK(settings.show_weather);
  current_temperature_unit_ = settings.temperature_unit;
  if (update_auto_reply_.has_value()) {
    std::move(callback).Run(update_auto_reply_.value(), settings);
    return;
  }
  pending_update_callback_ = std::move(callback);
  pending_settings_ = settings;
}

void FakeAmbientBackendControllerImpl::FetchSettingsAndAlbums(
    int banner_width,
    int banner_height,
    int num_albums,
    OnSettingsAndAlbumsFetchedCallback callback) {
  pending_fetch_settings_albums_callback_ = std::move(callback);
}

void FakeAmbientBackendControllerImpl::FetchWeather(
    std::optional<std::string> weather_client_id,
    FetchWeatherCallback callback) {
  ++fetch_weather_count_;
  weather_client_id_ = weather_client_id;
  if (run_fetch_weather_callback_) {
    std::move(callback).Run(weather_info_);
  }
}

const std::array<const char*, 2>&
FakeAmbientBackendControllerImpl::GetBackupPhotoUrls() const {
  return kFakeBackupPhotoUrls;
}

std::array<const char*, 2>
FakeAmbientBackendControllerImpl::GetTimeOfDayVideoPreviewImageUrls(
    AmbientVideo video) const {
  return kFakeBackupPhotoUrls;
}

const char* FakeAmbientBackendControllerImpl::GetPromoBannerUrl() const {
  return kFakeUrl;
}

const char* FakeAmbientBackendControllerImpl::GetTimeOfDayProductName() const {
  return "Product Name";
}

void FakeAmbientBackendControllerImpl::ReplyFetchSettingsAndAlbums(
    bool success,
    const std::optional<AmbientSettings>& settings) {
  if (!pending_fetch_settings_albums_callback_)
    return;

  if (success) {
    std::move(pending_fetch_settings_albums_callback_)
        .Run(settings.value_or(CreateFakeSettings()), CreateFakeAlbums());
  } else {
    std::move(pending_fetch_settings_albums_callback_)
        .Run(/*settings=*/std::nullopt, PersonalAlbums());
  }
}

void FakeAmbientBackendControllerImpl::SetFetchScreenUpdateInfoResponseSize(
    int num_topics_to_return) {
  DCHECK_GE(num_topics_to_return, 0);
  custom_num_topics_to_return_.emplace(num_topics_to_return);
}

bool FakeAmbientBackendControllerImpl::IsFetchSettingsAndAlbumsPending() const {
  return !pending_fetch_settings_albums_callback_.is_null();
}

void FakeAmbientBackendControllerImpl::ReplyUpdateSettings(bool success) {
  if (!pending_update_callback_)
    return;

  std::move(pending_update_callback_).Run(success, pending_settings_);
}

bool FakeAmbientBackendControllerImpl::IsUpdateSettingsPending() const {
  return !pending_update_callback_.is_null();
}

void FakeAmbientBackendControllerImpl::EnableUpdateSettingsAutoReply(
    bool success) {
  update_auto_reply_.emplace(success);
}

void FakeAmbientBackendControllerImpl::SetWeatherInfo(
    std::optional<WeatherInfo> info) {
  weather_info_ = std::move(info);
}

void FakeAmbientBackendControllerImpl::SetPhotoOrientation(bool portrait) {
  is_portrait_ = portrait;
}

void FakeAmbientBackendControllerImpl::SetPhotoTopicType(
    ::ambient::TopicType topic_type) {
  has_related_image_ = false;
  topic_type_ = topic_type;
}

}  // namespace ash

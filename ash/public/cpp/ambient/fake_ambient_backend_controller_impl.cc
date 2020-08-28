// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"

#include <utility>

#include "base/callback.h"
#include "base/optional.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace ash {

namespace {

constexpr AmbientModeTopicSource kTopicSource =
    AmbientModeTopicSource::kGooglePhotos;

constexpr AmbientModeTemperatureUnit kTemperatureUnit =
    AmbientModeTemperatureUnit::kCelsius;

constexpr char kFakeUrl[] = "chrome://ambient";

constexpr char kFakeDetails[] = "fake-photo-attribution";

AmbientSettings CreateFakeSettings() {
  AmbientSettings settings;
  settings.topic_source = kTopicSource;
  settings.temperature_unit = kTemperatureUnit;

  ArtSetting art_setting0;
  art_setting0.album_id = "0";
  art_setting0.enabled = true;
  art_setting0.title = "art0";
  settings.art_settings.emplace_back(art_setting0);

  ArtSetting art_setting1;
  art_setting1.album_id = "1";
  art_setting1.enabled = false;
  art_setting1.title = "art1";
  settings.art_settings.emplace_back(art_setting1);

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
    OnScreenUpdateInfoFetchedCallback callback) {
  ash::AmbientModeTopic topic;
  topic.url = kFakeUrl;
  topic.details = kFakeDetails;

  ash::WeatherInfo weather_info;
  weather_info.temp_f = .0f;
  weather_info.condition_icon_url = kFakeUrl;
  weather_info.show_celsius = true;

  ash::ScreenUpdate update;
  update.next_topics.emplace_back(topic);
  update.weather_info = weather_info;

  // Pretend to respond asynchronously.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), update));
}

void FakeAmbientBackendControllerImpl::InitSettings(
    UpdateSettingsCallback callback) {
  // Post task to simulate an async response.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), /*success=*/true));
}

void FakeAmbientBackendControllerImpl::GetSettings(
    GetSettingsCallback callback) {
  // Pretend to respond asynchronously.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), CreateFakeSettings()));
}

void FakeAmbientBackendControllerImpl::UpdateSettings(
    const AmbientSettings& settings,
    UpdateSettingsCallback callback) {
  // Pretend to respond asynchronously.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), /*success=*/true));
}

void FakeAmbientBackendControllerImpl::FetchSettingPreview(
    int preview_width,
    int preview_height,
    OnSettingPreviewFetchedCallback callback) {
  std::vector<std::string> urls = {kFakeUrl};
  // Pretend to respond asynchronously.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), urls));
}

void FakeAmbientBackendControllerImpl::FetchPersonalAlbums(
    int banner_width,
    int banner_height,
    int num_albums,
    const std::string& resume_token,
    OnPersonalAlbumsFetchedCallback callback) {
  // Pretend to respond asynchronously.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), CreateFakeAlbums()));
}

void FakeAmbientBackendControllerImpl::FetchSettingsAndAlbums(
    int banner_width,
    int banner_height,
    int num_albums,
    OnSettingsAndAlbumsFetchedCallback callback) {
  // Pretend to respond asynchronously.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), CreateFakeSettings(),
                                CreateFakeAlbums()));
}

void FakeAmbientBackendControllerImpl::SetPhotoRefreshInterval(
    base::TimeDelta interval) {
  NOTIMPLEMENTED();
}

}  // namespace ash

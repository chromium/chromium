// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_FAKE_AMBIENT_BACKEND_CONTROLLER_IMPL_H_
#define ASH_PUBLIC_CPP_AMBIENT_FAKE_AMBIENT_BACKEND_CONTROLLER_IMPL_H_

#include <array>
#include <optional>
#include <utility>
#include <vector>

#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

// A fake implementation of AmbientBackendController.
class ASH_PUBLIC_EXPORT FakeAmbientBackendControllerImpl
    : public AmbientBackendController {
 public:
  FakeAmbientBackendControllerImpl();
  ~FakeAmbientBackendControllerImpl() override;

  // AmbientBackendController:
  void FetchScreenUpdateInfo(
      int num_topics,
      bool show_pair_personal_portraits,
      const gfx::Size& screen_size,
      OnScreenUpdateInfoFetchedCallback callback) override;
  void FetchPreviewImages(const gfx::Size& preview_size,
                          OnPreviewImagesFetchedCallback callback) override;
  void UpdateSettings(const AmbientSettings settings,
                      UpdateSettingsCallback callback) override;
  void FetchSettingsAndAlbums(
      int banner_width,
      int banner_height,
      int num_albums,
      OnSettingsAndAlbumsFetchedCallback callback) override;
  void FetchWeather(std::optional<std::string> weather_client_id,
                    FetchWeatherCallback callback) override;
  const std::array<const char*, 2>& GetBackupPhotoUrls() const override;
  std::array<const char*, 2> GetTimeOfDayVideoPreviewImageUrls(
      AmbientVideo video) const override;
  const char* GetPromoBannerUrl() const override;
  const char* GetTimeOfDayProductName() const override;

  // Simulate to reply the request of FetchSettingsAndAlbums().
  // If |success| is true, will return fake data.
  // If |success| is false, will return null |settings| data.
  // If |settings| contains a value, that will be used as the argument to
  // the pending callback.
  void ReplyFetchSettingsAndAlbums(
      bool success,
      const std::optional<AmbientSettings>& settings = std::nullopt);

  // Simulates the reply for FetchScreenUpdateInfo(). All future calls to
  // FetchScreenUpdateInfo() will return the number of topics specified by
  // |num_topics_to_return|, bounded by the |num_topics| argument requested in
  // FetchScreenUpdateInfo().
  void SetFetchScreenUpdateInfoResponseSize(int num_topics_to_return);

  // Whether there is a pending FetchSettingsAndAlbums() request.
  bool IsFetchSettingsAndAlbumsPending() const;

  // Simulate to reply the request of UpdateSettings() with |success|.
  void ReplyUpdateSettings(bool success);

  // Whether there is a pending UpdateSettings() request.
  bool IsUpdateSettingsPending() const;

  // Will automatically reply to all future UpdateSettings() calls with
  // |success|.
  void EnableUpdateSettingsAutoReply(bool success);

  // Sets the weather info that will be returned in subsequent calls to
  // `FetchWeather`.
  void SetWeatherInfo(std::optional<WeatherInfo> info);

  void SetPhotoOrientation(bool portrait);

  void SetPhotoTopicType(::ambient::TopicType topic_type);

  // Gives the test total control over the topics returned by the backend.
  // If a generator is set, it takes priority over all other topic settings
  // above, and the topics it generates are returned verbatim to the client.
  using TopicGeneratorCallback = base::RepeatingCallback<std::vector<
      AmbientModeTopic>(int num_topics, const gfx::Size& screen_size)>;
  void set_custom_topic_generator(
      TopicGeneratorCallback custom_topic_generator) {
    custom_topic_generator_ = std::move(custom_topic_generator);
  }

  // The latest temperature unit received via |UpdateSettings()|. Defaults to
  // |kCelsius| if |UpdateSettings()| has not been called.
  AmbientModeTemperatureUnit current_temperature_unit() const {
    return current_temperature_unit_;
  }

  int fetch_weather_count() const { return fetch_weather_count_; }

  void set_run_fetch_weather_callback(bool value) {
    run_fetch_weather_callback_ = value;
  }

  // The latest `weather_client_id` passed to `FetchWeather()`.
  std::optional<std::string> weather_client_id() const {
    return weather_client_id_;
  }

 private:
  OnSettingsAndAlbumsFetchedCallback pending_fetch_settings_albums_callback_;

  UpdateSettingsCallback pending_update_callback_;

  AmbientSettings pending_settings_;

  std::optional<bool> update_auto_reply_;

  std::optional<WeatherInfo> weather_info_;

  bool is_portrait_ = false;

  bool has_related_image_ = true;

  ::ambient::TopicType topic_type_ = ::ambient::TopicType::kCulturalInstitute;

  std::optional<int> custom_num_topics_to_return_;

  TopicGeneratorCallback custom_topic_generator_;

  AmbientModeTemperatureUnit current_temperature_unit_ =
      AmbientModeTemperatureUnit::kCelsius;

  int fetch_weather_count_ = 0;
  bool run_fetch_weather_callback_ = true;
  std::optional<std::string> weather_client_id_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_FAKE_AMBIENT_BACKEND_CONTROLLER_IMPL_H_

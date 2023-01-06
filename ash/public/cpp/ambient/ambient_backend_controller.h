// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_AMBIENT_BACKEND_CONTROLLER_H_
#define ASH_PUBLIC_CPP_AMBIENT_AMBIENT_BACKEND_CONTROLLER_H_

#include <array>
#include <string>
#include <vector>

#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {

// AmbientModeTopic contains the information we need for rendering photo frame
// for Ambient Mode. Corresponding to the |backdrop::ScreenUpdate::Topic| proto.
struct ASH_PUBLIC_EXPORT AmbientModeTopic {
  AmbientModeTopic();
  AmbientModeTopic(const AmbientModeTopic&);
  AmbientModeTopic& operator=(const AmbientModeTopic&);
  AmbientModeTopic(AmbientModeTopic&&);
  AmbientModeTopic& operator=(AmbientModeTopic&&);
  ~AmbientModeTopic();

  // Details, i.e. the attribution, to be displayed for the current photo on
  // ambient.
  std::string details;

  // Image url.
  std::string url;

  std::string related_image_url;

  std::string related_details;

  ::ambient::TopicType topic_type = ::ambient::TopicType::kOther;

  // Whether the original image is portrait or not. Cannot use aspect ratio of
  // the fetched image to determine it because the fetched image could be
  // cropped.
  bool is_portrait = false;
};

// WeatherInfo contains the weather information we need for rendering a
// glanceable weather content on Ambient Mode. Corresponding to the
// |backdrop::WeatherInfo| proto.
struct ASH_PUBLIC_EXPORT WeatherInfo {
  WeatherInfo();
  WeatherInfo(const WeatherInfo&);
  WeatherInfo& operator=(const WeatherInfo&);
  ~WeatherInfo();

  // The url of the weather condition icon image.
  absl::optional<std::string> condition_icon_url;

  // Weather temperature in Fahrenheit.
  absl::optional<float> temp_f;

  // If the temperature should be displayed in celsius. Conversion must happen
  // before the value in temp_f is displayed.
  bool show_celsius = false;
};

// Trimmed-down version of |backdrop::ScreenUpdate| proto from the backdrop
// server. It contains necessary information we need to render photo frame and
// glancible weather card in Ambient Mode.
struct ASH_PUBLIC_EXPORT ScreenUpdate {
  ScreenUpdate();
  ScreenUpdate(const ScreenUpdate&);
  ScreenUpdate& operator=(const ScreenUpdate&);
  ~ScreenUpdate();

  // A list of |Topic| (size >= 0).
  std::vector<AmbientModeTopic> next_topics;

  // Weather information with weather condition icon and temperature in
  // Fahrenheit. Will be a null-opt if:
  // 1. The weather setting was disabled in the request, or
  // 2. Fatal errors, such as response parsing failure, happened during the
  // process, and a default |ScreenUpdate| instance was returned to indicate
  // the error.
  absl::optional<WeatherInfo> weather_info;
};

// Interface to manage ambient mode backend.
class ASH_PUBLIC_EXPORT AmbientBackendController {
 public:
  using OnScreenUpdateInfoFetchedCallback =
      base::OnceCallback<void(const ScreenUpdate&)>;
  using GetSettingsCallback =
      base::OnceCallback<void(const absl::optional<AmbientSettings>& settings)>;
  using UpdateSettingsCallback = base::OnceCallback<void(bool success)>;
  using OnSettingPreviewFetchedCallback =
      base::OnceCallback<void(const std::vector<std::string>& preview_urls)>;
  using OnPersonalAlbumsFetchedCallback =
      base::OnceCallback<void(PersonalAlbums)>;
  // TODO(wutao): Make |settings| move only.
  using OnSettingsAndAlbumsFetchedCallback =
      base::OnceCallback<void(const absl::optional<AmbientSettings>& settings,
                              PersonalAlbums personal_albums)>;
  using FetchWeatherCallback =
      base::OnceCallback<void(const absl::optional<WeatherInfo>& weather_info)>;

  using GetGooglePhotosAlbumsPreviewCallback =
      base::OnceCallback<void(const std::vector<GURL>& preview_urls)>;

  static AmbientBackendController* Get();

  AmbientBackendController();
  AmbientBackendController(const AmbientBackendController&) = delete;
  AmbientBackendController& operator=(const AmbientBackendController&) = delete;
  virtual ~AmbientBackendController();

  // Sends request to retrieve |num_topics| of |ScreenUpdate| from the backdrop
  // server with the specified |screen_size|.
  //
  // |show_pair_personal_portraits|: Whether IMAX should serve paired or single
  // personal portrait photos returned by the Photos backend. Ignored for
  // non-personal topic types.
  //
  // Upon completion, |callback| is run with the parsed |ScreenUpdate|. If any
  // errors happened during the process, e.g. failed to fetch access token, a
  // default instance will be returned.
  virtual void FetchScreenUpdateInfo(
      int num_topics,
      bool show_pair_personal_portraits,
      const gfx::Size& screen_size,
      OnScreenUpdateInfoFetchedCallback callback) = 0;

  // Get ambient mode Settings from server.
  virtual void GetSettings(GetSettingsCallback callback) = 0;

  // Update ambient mode Settings to server.
  virtual void UpdateSettings(const AmbientSettings& settings,
                              UpdateSettingsCallback callback) = 0;

  virtual void FetchPersonalAlbums(int banner_width,
                                   int banner_height,
                                   int num_albums,
                                   const std::string& resume_token,
                                   OnPersonalAlbumsFetchedCallback) = 0;

  // Fetch the Settings and albums as one API.
  virtual void FetchSettingsAndAlbums(int banner_width,
                                      int banner_height,
                                      int num_albums,
                                      OnSettingsAndAlbumsFetchedCallback) = 0;

  // Fetch the weather information.
  virtual void FetchWeather(FetchWeatherCallback) = 0;

  virtual void GetGooglePhotosAlbumsPreview(
      const std::vector<std::string>& album_ids,
      int preview_width,
      int preview_height,
      int num_previews,
      GetGooglePhotosAlbumsPreviewCallback callback) = 0;

  // Get stock photo urls to cache in advance in case Ambient mode is started
  // without internet access.
  virtual const std::array<const char*, 2>& GetBackupPhotoUrls() const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_AMBIENT_BACKEND_CONTROLLER_H_

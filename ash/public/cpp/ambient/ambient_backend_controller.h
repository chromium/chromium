// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_AMBIENT_BACKEND_CONTROLLER_H_
#define ASH_PUBLIC_CPP_AMBIENT_AMBIENT_BACKEND_CONTROLLER_H_

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "ash/constants/ambient_video.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/public/cpp/ambient/weather_info.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"
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
  std::optional<WeatherInfo> weather_info;
};

// Interface to manage ambient mode backend.
class ASH_PUBLIC_EXPORT AmbientBackendController {
 public:
  using OnScreenUpdateInfoFetchedCallback =
      base::OnceCallback<void(const ScreenUpdate&)>;
  using OnPreviewImagesFetchedCallback =
      base::OnceCallback<void(const std::vector<GURL>& preview_urls)>;
  using UpdateSettingsCallback =
      base::OnceCallback<void(bool success, const AmbientSettings& settings)>;
  // TODO(wutao): Make |settings| move only.
  using OnSettingsAndAlbumsFetchedCallback =
      base::OnceCallback<void(const std::optional<AmbientSettings>& settings,
                              PersonalAlbums personal_albums)>;
  using FetchWeatherCallback =
      base::OnceCallback<void(const std::optional<WeatherInfo>& weather_info)>;

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

  virtual void FetchPreviewImages(const gfx::Size& preview_size,
                                  OnPreviewImagesFetchedCallback callback) = 0;

  // Update ambient mode Settings to server.
  virtual void UpdateSettings(const AmbientSettings settings,
                              UpdateSettingsCallback callback) = 0;

  // Fetch the Settings and albums as one API.
  virtual void FetchSettingsAndAlbums(int banner_width,
                                      int banner_height,
                                      int num_albums,
                                      OnSettingsAndAlbumsFetchedCallback) = 0;

  // Fetch the weather information.
  // `weather_client_id` - the weather client ID that should be passed to the
  // weather request, use nullopt to use the default weather client ID (used
  // for ambient mode).
  virtual void FetchWeather(std::optional<std::string> weather_client_id,
                            FetchWeatherCallback callback) = 0;

  // Get stock photo urls to cache in advance in case Ambient mode is started
  // without internet access.
  virtual const std::array<const char*, 2>& GetBackupPhotoUrls() const = 0;

  // Returns the preview image urls for the video screen saver.
  virtual std::array<const char*, 2> GetTimeOfDayVideoPreviewImageUrls(
      AmbientVideo video) const = 0;

  // Returns the promo banner url to highlight time-of-day wallpapers and screen
  // saver feature.
  virtual const char* GetPromoBannerUrl() const = 0;

  // Returns the product name that features the exclusive time of day wallpapers
  // and screen savers.
  virtual const char* GetTimeOfDayProductName() const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_AMBIENT_BACKEND_CONTROLLER_H_

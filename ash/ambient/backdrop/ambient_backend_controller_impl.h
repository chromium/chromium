// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_BACKDROP_AMBIENT_BACKEND_CONTROLLER_IMPL_H_
#define ASH_AMBIENT_BACKDROP_AMBIENT_BACKEND_CONTROLLER_IMPL_H_

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/assistant/internal/ambient/backdrop_client_config.h"

namespace ash {

class BackdropURLLoader;
// The Backdrop client implementation of AmbientBackendController.
class AmbientBackendControllerImpl : public AmbientBackendController {
 public:
  AmbientBackendControllerImpl();
  ~AmbientBackendControllerImpl() override;

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

 private:
  using BackdropClientConfig = chromeos::ambient::BackdropClientConfig;
  using GetSettingsCallback =
      base::OnceCallback<void(const std::optional<AmbientSettings>& settings)>;
  using OnPersonalAlbumsFetchedCallback =
      base::OnceCallback<void(PersonalAlbums)>;

  void RequestAccessToken(AmbientClient::GetAccessTokenCallback callback);

  void FetchScreenUpdateInfoInternal(int num_topics,
                                     bool show_pair_personal_portraits,
                                     const gfx::Size& screen_size,
                                     OnScreenUpdateInfoFetchedCallback callback,
                                     const std::string& gaia_id,
                                     const std::string& access_token);

  void OnScreenUpdateInfoFetched(
      OnScreenUpdateInfoFetchedCallback callback,
      std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
      std::unique_ptr<std::string> response);

  void GetSettings(GetSettingsCallback callback);
  void StartToGetSettings(GetSettingsCallback callback,
                          const std::string& gaia_id,
                          const std::string& access_token);

  void OnGetSettings(GetSettingsCallback callback,
                     std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
                     std::unique_ptr<std::string> response);

  void StartToUpdateSettings(const AmbientSettings& settings,
                             UpdateSettingsCallback callback,
                             const std::string& gaia_id,
                             const std::string& access_token);

  void OnUpdateSettings(UpdateSettingsCallback callback,
                        const AmbientSettings& settings,
                        std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
                        std::unique_ptr<std::string> response);

  void FetchPersonalAlbums(int banner_width,
                           int banner_height,
                           int num_albums,
                           const std::string& resume_token,
                           OnPersonalAlbumsFetchedCallback callback);

  void FetchPersonalAlbumsInternal(int banner_width,
                                   int banner_height,
                                   int num_albums,
                                   const std::string& resume_token,
                                   OnPersonalAlbumsFetchedCallback callback,
                                   const std::string& gaia_id,
                                   const std::string& access_token);

  void OnPersonalAlbumsFetched(
      OnPersonalAlbumsFetchedCallback callback,
      std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
      std::unique_ptr<std::string> response);

  void OnSettingsFetched(base::RepeatingClosure on_done,
                         const std::optional<ash::AmbientSettings>& settings);

  void OnAlbumsFetched(base::RepeatingClosure on_done,
                       ash::PersonalAlbums personal_albums);

  void OnSettingsAndAlbumsFetched(OnSettingsAndAlbumsFetchedCallback callback);

  // Temporary store for FetchSettingsAndAlbums() when |GetSettingsCallback|
  // called. |settings_| will be std::nullopt if server returns with error.
  std::optional<ash::AmbientSettings> settings_;

  // Temporary store for FetchSettingsAndAlbums() when
  // |OnPersonalAlbumsFetchedCallback| called. |personal_albums_| will contains
  // empty values if server returns with error.
  ash::PersonalAlbums personal_albums_;

  BackdropClientConfig backdrop_client_config_;

  base::WeakPtrFactory<AmbientBackendControllerImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_BACKDROP_AMBIENT_BACKEND_CONTROLLER_IMPL_H_

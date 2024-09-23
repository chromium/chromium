// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_AMBIENT_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_AMBIENT_PROVIDER_IMPL_H_

#include <optional>

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/personalization_app_ambient_provider.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/backoff_entry.h"
#include "url/gurl.h"

class Profile;

namespace ash::personalization_app {

class PersonalizationAppAmbientProviderImpl
    : public PersonalizationAppAmbientProvider,
      public AmbientUiModelObserver {
 public:
  explicit PersonalizationAppAmbientProviderImpl(content::WebUI* web_ui);

  PersonalizationAppAmbientProviderImpl(
      const PersonalizationAppAmbientProviderImpl&) = delete;
  PersonalizationAppAmbientProviderImpl& operator=(
      const PersonalizationAppAmbientProviderImpl&) = delete;

  ~PersonalizationAppAmbientProviderImpl() override;

  void BindInterface(
      mojo::PendingReceiver<ash::personalization_app::mojom::AmbientProvider>
          receiver) override;

  // AmbientUiModelObserver:
  void OnAmbientUiVisibilityChanged(AmbientUiVisibility visibility) override;

  // ash::personalization_app::mojom:AmbientProvider:
  void IsAmbientModeEnabled(IsAmbientModeEnabledCallback callback) override;
  void SetAmbientObserver(
      mojo::PendingRemote<ash::personalization_app::mojom::AmbientObserver>
          observer) override;
  void SetAmbientModeEnabled(bool enabled) override;
  void SetAmbientTheme(mojom::AmbientTheme ambient_theme) override;
  void SetScreenSaverDuration(int minutes) override;
  void SetTopicSource(mojom::TopicSource topic_source) override;
  void SetTemperatureUnit(
      ash::AmbientModeTemperatureUnit temperature_unit) override;
  void SetAlbumSelected(const std::string& id,
                        mojom::TopicSource topic_source,
                        bool selected) override;
  void SetPageViewed() override;
  void FetchSettingsAndAlbums() override;
  void StartScreenSaverPreview() override;
  void ShouldShowTimeOfDayBanner(
      ShouldShowTimeOfDayBannerCallback callback) override;
  void HandleTimeOfDayBannerDismissed() override;
  void IsGeolocationEnabledForSystemServices(
      IsGeolocationEnabledForSystemServicesCallback callback) override;
  void IsGeolocationUserModifiable(
      IsGeolocationUserModifiableCallback callback) override;
  void EnableGeolocationForSystemServices() override;

  // Notify WebUI the latest values.
  void OnAmbientModeEnabledChanged();
  void OnAmbientUiSettingsChanged();
  void OnScreenSaverDurationChanged();
  void OnTemperatureUnitChanged();
  void OnTopicSourceChanged();
  void OnAlbumsChanged();
  void OnRecentHighlightsPreviewsChanged();

 private:
  friend class PersonalizationAppAmbientProviderImplTest;

  bool IsAmbientModeEnabled();

  bool IsGeolocationEnabledForSystemServices();
  bool IsGeolocationUserModifiable();

  // Notify webUI the current state of system geolocation permission.
  void NotifyGeolocationPermissionChanged();

  AmbientUiSettings GetCurrentUiSettings() const;

  // Update the local `settings_` to server.
  void UpdateSettings();

  // Called when the settings is updated.
  // `success` is true when update successfully.
  void OnUpdateSettings(bool success, const AmbientSettings& settings);

  void OnSettingsAndAlbumsFetched(
      const std::optional<ash::AmbientSettings>& settings,
      ash::PersonalAlbums personal_albums);

  // The `settings_` could be stale when the albums in Google Photos changes.
  // Prune the `selected_album_id` which does not exist any more.
  // Populate albums with selected info which will be shown on Settings UI.
  void SyncSettingsAndAlbums();

  // Update topic source if needed.
  void MaybeUpdateTopicSource(mojom::TopicSource topic_source);

  void FetchPreviewImages();
  void OnPreviewsFetched(const std::vector<GURL>& preview_urls);

  ash::PersonalAlbum* FindPersonalAlbumById(const std::string& album_id);

  ash::ArtSetting* FindArtAlbumById(const std::string& album_id);

  // Reset local settings to start a new session.
  void ResetLocalSettings();

  // Not necessarily the same as `settings_.topic_source`. The `topic_source` in
  // `settings_` should never be `kVideo` since it reflects what the server
  // stores, and the server does not know about video. Note it's important to
  // leave `settings_` untouched while the video theme is active so that the
  // user's exact `AmbientSettings` can be restored when switching back to a
  // non-video theme (ex: slideshow).
  mojom::TopicSource GetCurrentTopicSource() const;

  void BroadcastAmbientModeEnabledStatus(bool enabled);

  mojo::Receiver<ash::personalization_app::mojom::AmbientProvider>
      ambient_receiver_{this};

  mojo::Remote<ash::personalization_app::mojom::AmbientObserver>
      ambient_observer_remote_;

  raw_ptr<Profile> const profile_ = nullptr;

  PrefChangeRegistrar pref_change_registrar_;

  // Backoff retries for `FetchSettingsAndAlbums()`.
  net::BackoffEntry fetch_settings_retry_backoff_;

  // Backoff retries for `UpdateSettings()`.
  net::BackoffEntry update_settings_retry_backoff_;

  // Local settings which may contain changes from WebUI but have not sent to
  // server. Only one `UpdateSettings()` at a time.
  std::optional<ash::AmbientSettings> settings_;

  // The cached settings from the server. Should be the same as the server side.
  // This value will be updated when `RequestSettingsAndAlbums()` and
  // `UpdateSettings()` return successfully.
  // If `UpdateSettings()` fails, will restore to this value.
  std::optional<ash::AmbientSettings> cached_settings_;

  ash::PersonalAlbums personal_albums_;

  // Whether the Settings updating is ongoing.
  bool is_updating_backend_ = false;

  // Whether to update previews when `UpdateSettings()` returns successfully.
  bool needs_update_previews_ = false;

  // A flag to record if the user has seen the ambient mode page.
  bool page_viewed_ = false;

  base::ScopedObservation<AmbientUiModel, AmbientUiModelObserver>
      ambient_ui_model_observer_{this};

  base::WeakPtrFactory<PersonalizationAppAmbientProviderImpl>
      write_weak_factory_{this};
  base::WeakPtrFactory<PersonalizationAppAmbientProviderImpl>
      read_weak_factory_{this};
  base::WeakPtrFactory<PersonalizationAppAmbientProviderImpl>
      previews_weak_factory_{this};
};

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_AMBIENT_PROVIDER_IMPL_H_

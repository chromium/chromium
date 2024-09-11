// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_ambient_provider_impl.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/metrics/ambient_metrics.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/constants/ambient_video.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/controls/contextual_tooltip.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/shell.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/mojom/personalization_app_mojom_traits.h"
#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/ambient_video_albums.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_manager.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_manager_factory.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_metrics.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/base/backoff_entry.h"
#include "personalization_app_ambient_provider_impl.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

namespace ash::personalization_app {

namespace {

// The max possible preview image container is 460x290.
// Double the fetched image W/H to stay sharp when scaled down.
constexpr int kBannerWidthPx = 920;
constexpr int kBannerHeightPx = 580;

constexpr int kMaxRetries = 3;

constexpr net::BackoffEntry::Policy kRetryBackoffPolicy = {
    0,          // Number of initial errors to ignore.
    500,        // Initial delay in ms.
    2.0,        // Factor by which the waiting time will be multiplied.
    0.2,        // Fuzzing percentage.
    60 * 1000,  // Maximum delay in ms.
    -1,         // Never discard the entry.
    true,       // Use initial delay.
};

DurationOption GetDurationMetrics(int minutes) {
  switch (minutes) {
    case (0): {
      return DurationOption::kForever;
    }
    case (5): {
      return DurationOption::kFiveMin;
    }
    case (10): {
      return DurationOption::kTenMin;
    }
    case (30): {
      return DurationOption::kThirtyMin;
    }
    case (60): {
      return DurationOption::kOneHour;
    }
    default: {
      return DurationOption::kError;
    }
  }
}

}  // namespace

PersonalizationAppAmbientProviderImpl::PersonalizationAppAmbientProviderImpl(
    content::WebUI* web_ui)
    : profile_(Profile::FromWebUI(web_ui)),
      fetch_settings_retry_backoff_(&kRetryBackoffPolicy),
      update_settings_retry_backoff_(&kRetryBackoffPolicy) {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      ash::ambient::prefs::kAmbientModeEnabled,
      base::BindRepeating(
          &PersonalizationAppAmbientProviderImpl::OnAmbientModeEnabledChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      ash::ambient::prefs::kAmbientUiSettings,
      base::BindRepeating(
          &PersonalizationAppAmbientProviderImpl::OnAmbientUiSettingsChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      ash::prefs::kUserGeolocationAccessLevel,
      base::BindRepeating(&PersonalizationAppAmbientProviderImpl::
                              NotifyGeolocationPermissionChanged,
                          base::Unretained(this)));
  ambient_ui_model_observer_.Observe(
      Shell::Get()->ambient_controller()->ambient_ui_model());
}

PersonalizationAppAmbientProviderImpl::
    ~PersonalizationAppAmbientProviderImpl() {
  if (page_viewed_) {
    ::ash::personalization_app::PersonalizationAppManagerFactory::
        GetForBrowserContext(profile_)
            ->MaybeStartHatsTimer(
                ::ash::personalization_app::HatsSurveyType::kScreensaver);
  }
}

void PersonalizationAppAmbientProviderImpl::BindInterface(
    mojo::PendingReceiver<ash::personalization_app::mojom::AmbientProvider>
        receiver) {
  ambient_receiver_.reset();
  ambient_receiver_.Bind(std::move(receiver));
}

void PersonalizationAppAmbientProviderImpl::IsAmbientModeEnabled(
    IsAmbientModeEnabledCallback callback) {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  std::move(callback).Run(
      pref_service->GetBoolean(ash::ambient::prefs::kAmbientModeEnabled));
}

bool PersonalizationAppAmbientProviderImpl::
    IsGeolocationEnabledForSystemServices() {
  PrefService* pref_service = profile_->GetPrefs();
  CHECK(pref_service);
  const auto access_level = static_cast<GeolocationAccessLevel>(
      pref_service->GetInteger(prefs::kUserGeolocationAccessLevel));

  switch (access_level) {
    case ash::GeolocationAccessLevel::kAllowed:
    case ash::GeolocationAccessLevel::kOnlyAllowedForSystem:
      return true;
    case ash::GeolocationAccessLevel::kDisallowed:
      return false;
  }
}

bool PersonalizationAppAmbientProviderImpl::IsGeolocationUserModifiable() {
  PrefService* pref_service = profile_->GetPrefs();
  CHECK(pref_service);
  return pref_service->IsUserModifiablePreference(
      prefs::kUserGeolocationAccessLevel);
}

void PersonalizationAppAmbientProviderImpl::
    NotifyGeolocationPermissionChanged() {
  if (!ambient_observer_remote_.is_bound()) {
    return;
  }

  ambient_observer_remote_->OnGeolocationPermissionForSystemServicesChanged(
      IsGeolocationEnabledForSystemServices(), IsGeolocationUserModifiable());
}

void PersonalizationAppAmbientProviderImpl::SetAmbientObserver(
    mojo::PendingRemote<ash::personalization_app::mojom::AmbientObserver>
        observer) {
  if (!AmbientClient::Get() || !AmbientClient::Get()->IsAmbientModeAllowed()) {
    ambient_receiver_.ReportBadMessage(
        "Ambient observer set when ambient is not allowed");
    return;
  }
  // May already be bound if user refreshes page.
  ambient_observer_remote_.reset();
  ambient_observer_remote_.Bind(std::move(observer));

  // Call it once to get the current ambient mode enabled status.
  BroadcastAmbientModeEnabledStatus(IsAmbientModeEnabled());

  // Call it once to get the current ambient ui settings.
  OnAmbientUiSettingsChanged();

  // Call it once to get the current ambient duration settings.
  OnScreenSaverDurationChanged();

  ResetLocalSettings();
}

void PersonalizationAppAmbientProviderImpl::SetAmbientModeEnabled(
    bool enabled) {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  pref_service->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled, enabled);
}

void PersonalizationAppAmbientProviderImpl::SetAmbientTheme(
    mojom::AmbientTheme to_theme) {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  LogAmbientModeTheme(to_theme);
  AmbientUiSettings orig_settings = GetCurrentUiSettings();
  mojom::AmbientTheme from_theme = orig_settings.theme();
  if (from_theme == to_theme) {
    return;
  }

  // Attempt to retrieve the previously selected video. If not, fallback to the
  // default video. Only applicable when target theme is `AmbientTheme::kVideo`.
  AmbientVideo video = orig_settings.video().value_or(kDefaultAmbientVideo);
  if (to_theme == mojom::AmbientTheme::kVideo) {
    LogAmbientModeVideo(video);
  }
  AmbientUiSettings(to_theme, video).WriteToPrefService(*pref_service);

  // `kVideo` theme is special and automatically means a switch to the `kVideo`
  // topic source. None of the other topic sources are possible with this theme.
  //
  // If `settings_` is null, the next call to `FetchSettingsAndAlbums()` will
  // broadcast the `OnTopicSourceChanged()` call that's being done here.
  if (settings_ && (to_theme == mojom::AmbientTheme::kVideo ||
                    from_theme == mojom::AmbientTheme::kVideo)) {
    OnTopicSourceChanged();
  }
}

void PersonalizationAppAmbientProviderImpl::SetTopicSource(
    mojom::TopicSource topic_source) {
  mojom::AmbientTheme current_theme = GetCurrentUiSettings().theme();
  // The presence of the `kVideo` theme in pref automatically means the `kVideo`
  // topic source is active. `settings_` should be kept as the server's view of
  // the user's ambient settings, and `SetAmbientTheme(kVideo)` already
  // broadcasts an `OnTopicSourceChanged()`, so there's no work to do here.
  if (current_theme == mojom::AmbientTheme::kVideo) {
    if (topic_source != mojom::TopicSource::kVideo) {
      LOG(ERROR) << "Cannot set topic source to "
                 << static_cast<int>(topic_source) << " for video theme";
    }
    return;
  }

  if (topic_source == mojom::TopicSource::kVideo) {
    LOG(ERROR) << "Video topic source does not apply to theme "
               << ambient::util::AmbientThemeToString(current_theme);
    return;
  }

  // If this is an Art gallery album page, will select art gallery topic source.
  if (topic_source == mojom::TopicSource::kArtGallery) {
    MaybeUpdateTopicSource(topic_source);
    return;
  }

  // If this is a Google Photos album page, will
  // 1. Select art gallery topic source if no albums or no album is selected.
  if (settings_->selected_album_ids.empty()) {
    MaybeUpdateTopicSource(mojom::TopicSource::kArtGallery);
    return;
  }

  // 2. Select Google Photos topic source if at least one album is selected.
  MaybeUpdateTopicSource(mojom::TopicSource::kGooglePhotos);
}

void PersonalizationAppAmbientProviderImpl::SetScreenSaverDuration(
    int minutes) {
  Shell::Get()->ambient_controller()->SetScreenSaverDuration(minutes);
  OnScreenSaverDurationChanged();
  LogAmbientModeScreenSaverDuration(GetDurationMetrics(minutes));
}

void PersonalizationAppAmbientProviderImpl::SetTemperatureUnit(
    ash::AmbientModeTemperatureUnit temperature_unit) {
  if (settings_->temperature_unit != temperature_unit) {
    settings_->temperature_unit = temperature_unit;
    UpdateSettings();
    OnTemperatureUnitChanged();
  }
}

void PersonalizationAppAmbientProviderImpl::SetAlbumSelected(
    const std::string& id,
    mojom::TopicSource topic_source,
    bool selected) {
  switch (topic_source) {
    case (mojom::TopicSource::kGooglePhotos): {
      ash::PersonalAlbum* target_personal_album = FindPersonalAlbumById(id);
      if (!target_personal_album) {
        ambient_receiver_.ReportBadMessage("Invalid album id.");
        return;
      }
      target_personal_album->selected = selected;

      // For Google Photos, we will populate the |selected_album_ids| with IDs
      // of selected albums.
      settings_->selected_album_ids.clear();
      for (const auto& personal_album : personal_albums_.albums) {
        if (personal_album.selected) {
          settings_->selected_album_ids.push_back(personal_album.album_id);
        }
      }

      // Update topic source based on selections.
      if (settings_->selected_album_ids.empty()) {
        settings_->topic_source = mojom::TopicSource::kArtGallery;
      } else {
        settings_->topic_source = mojom::TopicSource::kGooglePhotos;
      }

      ash::ambient::RecordAmbientModeTotalNumberOfAlbums(
          personal_albums_.albums.size());
      ash::ambient::RecordAmbientModeSelectedNumberOfAlbums(
          settings_->selected_album_ids.size());
      break;
    }
    case (mojom::TopicSource::kArtGallery): {
      // For Art gallery, we set the corresponding setting to be enabled or not
      // based on the selections.
      auto* art_setting = FindArtAlbumById(id);
      if (!art_setting || !art_setting->visible) {
        ambient_receiver_.ReportBadMessage("Invalid album id.");
        return;
      }
      art_setting->enabled = selected;
      break;
    }
    case mojom::TopicSource::kVideo:
      if (!selected) {
        DVLOG(4) << "Exactly one video must be selected at all times. Setting "
                    "the desired video to selected==true automatically "
                    "unselects all other videos.";
        return;
      }
      std::optional<AmbientVideo> video = FindAmbientVideoByAlbumId(id);
      if (!video) {
        ambient_receiver_.ReportBadMessage("Invalid album id.");
        return;
      }
      // Even if the current `AmbientTheme` is not `kVideo`, pref storage can
      // still be updated with the requested video, and it will be applied
      // if/when the user selects the video theme later.
      PrefService* pref_service = profile_->GetPrefs();
      DCHECK(pref_service);
      AmbientUiSettings(GetCurrentUiSettings().theme(), *video)
          .WriteToPrefService(*pref_service);
      LogAmbientModeVideo(*video);
      break;
  }

  UpdateSettings();
  OnTopicSourceChanged();
  OnAlbumsChanged();
}

void PersonalizationAppAmbientProviderImpl::SetPageViewed() {
  page_viewed_ = true;
}

void PersonalizationAppAmbientProviderImpl::FetchSettingsAndAlbums() {
  // If there is an ongoing update, do not fetch. If update succeeds, it will
  // update the UI with the new settings. If update fails, it will restore
  // previous settings and update UI.
  if (is_updating_backend_) {
    return;
  }

  ash::AmbientBackendController::Get()->FetchSettingsAndAlbums(
      kBannerWidthPx, kBannerHeightPx, /*num_albums=*/100,
      base::BindOnce(
          &PersonalizationAppAmbientProviderImpl::OnSettingsAndAlbumsFetched,
          read_weak_factory_.GetWeakPtr()));
}

void PersonalizationAppAmbientProviderImpl::OnAmbientModeEnabledChanged() {
  const bool enabled = IsAmbientModeEnabled();
  if (enabled) {
    // The usage metrics for the user's `AmbientUiSettings` should be
    // incremented whenever ambient mode is enabled. They should not be
    // incremented though every time the hub is simply opened.
    AmbientUiSettings current_ui_settings = GetCurrentUiSettings();
    LogAmbientModeTheme(current_ui_settings.theme());
    if (current_ui_settings.theme() == mojom::AmbientTheme::kVideo) {
      LogAmbientModeVideo(*current_ui_settings.video());
    }
  }
  BroadcastAmbientModeEnabledStatus(enabled);
  if (!enabled) {
    // UpdateSettings assumes that ambient is enabled. Since ambient is now
    // disabled, cancel any requests to update settings.
    write_weak_factory_.InvalidateWeakPtrs();
    is_updating_backend_ = false;
  }
}

void PersonalizationAppAmbientProviderImpl::BroadcastAmbientModeEnabledStatus(
    bool enabled) {
  if (ambient_observer_remote_.is_bound()) {
    ambient_observer_remote_->OnAmbientModeEnabledChanged(enabled);
  }

  // Call |UpdateSettings| when Ambient mode is enabled to make sure that
  // settings are properly synced to the server even if the user never touches
  // the other controls. Please see b/177456397.
  if (settings_ && enabled) {
    UpdateSettings();
  }
}

void PersonalizationAppAmbientProviderImpl::OnAmbientUiSettingsChanged() {
  if (!ambient_observer_remote_.is_bound()) {
    return;
  }

  ambient_observer_remote_->OnAmbientThemeChanged(
      GetCurrentUiSettings().theme());
}

void PersonalizationAppAmbientProviderImpl::OnScreenSaverDurationChanged() {
  if (!ambient_observer_remote_.is_bound()) {
    return;
  }

  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  int duration_minutes = pref_service->GetInteger(
      ambient::prefs::kAmbientModeRunningDurationMinutes);
  CHECK(duration_minutes >= 0);
  ambient_observer_remote_->OnScreenSaverDurationChanged(duration_minutes);
}

void PersonalizationAppAmbientProviderImpl::OnTemperatureUnitChanged() {
  if (!ambient_observer_remote_.is_bound()) {
    return;
  }

  ambient_observer_remote_->OnTemperatureUnitChanged(
      settings_->temperature_unit);
}

void PersonalizationAppAmbientProviderImpl::OnTopicSourceChanged() {
  if (!ambient_observer_remote_.is_bound()) {
    return;
  }

  // Empty the WebUI store so it doesn't show the previously selected albums'
  // previews.
  OnPreviewsFetched(std::vector<GURL>());
  if (is_updating_backend_) {
    // Once settings updated, fetch preview images.
    needs_update_previews_ = true;
  } else {
    // Fetch preview images if settings have been updated.
    FetchPreviewImages();
  }

  ambient_observer_remote_->OnTopicSourceChanged(GetCurrentTopicSource());
}

void PersonalizationAppAmbientProviderImpl::OnAlbumsChanged() {
  if (!ambient_observer_remote_.is_bound()) {
    return;
  }

  std::vector<ash::personalization_app::mojom::AmbientModeAlbumPtr> albums;
  // Google photos:
  for (const auto& personal_album : personal_albums_.albums) {
    // `url` will be updated when preview image is downloaded.
    ash::personalization_app::mojom::AmbientModeAlbumPtr album =
        ash::personalization_app::mojom::AmbientModeAlbum::New();
    album->id = personal_album.album_id;
    album->checked = personal_album.selected;
    album->title = personal_album.album_name;
    album->description = personal_album.description;
    album->number_of_photos = personal_album.number_of_photos;
    album->url = GURL(personal_album.banner_image_url);
    album->topic_source = mojom::TopicSource::kGooglePhotos;
    albums.emplace_back(std::move(album));
  }

  // Art gallery:
  for (const auto& setting : settings_->art_settings) {
    if (!setting.visible) {
      continue;
    }

    // `url` will be updated when preview image is downloaded.
    ash::personalization_app::mojom::AmbientModeAlbumPtr album =
        ash::personalization_app::mojom::AmbientModeAlbum::New();
    album->id = setting.album_id;
    album->checked = setting.enabled;
    album->title = setting.title;
    album->description = setting.description;
    album->url = GURL(setting.preview_image_url);
    album->topic_source = mojom::TopicSource::kArtGallery;
    albums.emplace_back(std::move(album));
  }

  // Video:
  AppendAmbientVideoAlbums(
      /*currently_selected_video*/ GetCurrentUiSettings().video().value_or(
          kDefaultAmbientVideo),
      albums);

  ambient_observer_remote_->OnAlbumsChanged(std::move(albums));
}

void PersonalizationAppAmbientProviderImpl::
    OnRecentHighlightsPreviewsChanged() {
  NOTIMPLEMENTED();
}

bool PersonalizationAppAmbientProviderImpl::IsAmbientModeEnabled() {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  return pref_service->GetBoolean(ash::ambient::prefs::kAmbientModeEnabled);
}

AmbientUiSettings PersonalizationAppAmbientProviderImpl::GetCurrentUiSettings()
    const {
  PrefService* pref_service = profile_->GetPrefs();
  DCHECK(pref_service);
  return AmbientUiSettings::ReadFromPrefService(*pref_service);
}

void PersonalizationAppAmbientProviderImpl::UpdateSettings() {
  DCHECK(IsAmbientModeEnabled())
      << "Ambient mode must be enabled to update settings";
  DCHECK(settings_);
  DCHECK_NE(settings_->topic_source, mojom::TopicSource::kVideo)
      << "Ambient backend is not aware of the video topic source";

  // Prevent fetch settings callback changing `settings_` and `personal_albums_`
  // while updating.
  read_weak_factory_.InvalidateWeakPtrs();
  // Cancel in-flight write requests, as this newer update will overwrite them.
  write_weak_factory_.InvalidateWeakPtrs();

  is_updating_backend_ = true;

  // Explicitly set show_weather to true to force server to respond with
  // weather information. See: b/158630188.
  settings_->show_weather = true;

  ash::AmbientBackendController::Get()->UpdateSettings(
      *settings_,
      base::BindOnce(&PersonalizationAppAmbientProviderImpl::OnUpdateSettings,
                     write_weak_factory_.GetWeakPtr()));
}

void PersonalizationAppAmbientProviderImpl::OnUpdateSettings(
    bool success,
    const AmbientSettings& settings) {
  is_updating_backend_ = false;

  if (success) {
    update_settings_retry_backoff_.Reset();
    OnSettingsAndAlbumsFetched(settings, std::move(personal_albums_));
    // The request to fetch preview images came in during |UpdateSettings|. Call
    // it now that updating has finished.
    if (needs_update_previews_) {
      FetchPreviewImages();
    }
  } else {
    update_settings_retry_backoff_.InformOfRequest(/*succeeded=*/false);
    // When the update fails, send a retry or revert to cached settings.
    if (update_settings_retry_backoff_.failure_count() <= kMaxRetries) {
      const base::TimeDelta kDelay =
          update_settings_retry_backoff_.GetTimeUntilRelease();
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PersonalizationAppAmbientProviderImpl::UpdateSettings,
                         write_weak_factory_.GetWeakPtr()),
          kDelay);
    } else {
      OnSettingsAndAlbumsFetched(cached_settings_, std::move(personal_albums_));
    }
  }
}

void PersonalizationAppAmbientProviderImpl::OnSettingsAndAlbumsFetched(
    const std::optional<ash::AmbientSettings>& settings,
    ash::PersonalAlbums personal_albums) {
  // `settings` value implies success.
  if (!settings) {
    fetch_settings_retry_backoff_.InformOfRequest(/*succeeded=*/false);
    if (fetch_settings_retry_backoff_.failure_count() > kMaxRetries) {
      return;
    }

    const base::TimeDelta kDelay =
        fetch_settings_retry_backoff_.GetTimeUntilRelease();
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &PersonalizationAppAmbientProviderImpl::FetchSettingsAndAlbums,
            read_weak_factory_.GetWeakPtr()),
        kDelay);
    return;
  }

  fetch_settings_retry_backoff_.Reset();
  settings_ = settings;
  cached_settings_ = settings;
  personal_albums_ = std::move(personal_albums);
  SyncSettingsAndAlbums();

  OnTemperatureUnitChanged();

  // Notify `OnAlbumsChanged()` first because the albums info is needed to
  // render the description text of the topic source buttons. E.g. if the Google
  // Photos album is empty, it will show different text.
  OnAlbumsChanged();
  OnTopicSourceChanged();

  // If weather info is disabled, call `UpdateSettings()` immediately to force
  // it to true. Please see b/177456397.
  if (!settings_->show_weather && IsAmbientModeEnabled()) {
    UpdateSettings();
  }
}

void PersonalizationAppAmbientProviderImpl::SyncSettingsAndAlbums() {
  // Clear the `selected` field, which will be populated with new value below.
  // It is necessary if `UpdateSettings()` failed and we need to reset the
  // cached settings.
  for (auto& album : personal_albums_.albums) {
    album.selected = false;
  }

  auto it = settings_->selected_album_ids.begin();
  while (it != settings_->selected_album_ids.end()) {
    const std::string& album_id = *it;
    ash::PersonalAlbum* album = FindPersonalAlbumById(album_id);
    if (album) {
      album->selected = true;
      ++it;
    } else {
      // The selected album does not exist any more.
      it = settings_->selected_album_ids.erase(it);
    }
  }

  if (settings_->selected_album_ids.empty()) {
    MaybeUpdateTopicSource(mojom::TopicSource::kArtGallery);
  }
}

void PersonalizationAppAmbientProviderImpl::MaybeUpdateTopicSource(
    mojom::TopicSource topic_source) {
  DCHECK_NE(settings_->topic_source, mojom::TopicSource::kVideo)
      << "Video topic source should automatically get set via the video "
         "AmbientTheme. Should not be reflected in the server.";
  // If the setting is the same, no need to update.
  if (settings_->topic_source != topic_source) {
    settings_->topic_source = topic_source;
    if (IsAmbientModeEnabled()) {
      // Only send update to server if ambient mode is currently enabled.
      UpdateSettings();
    }
  }

  OnTopicSourceChanged();
}

void PersonalizationAppAmbientProviderImpl::FetchPreviewImages() {
  needs_update_previews_ = false;
  previews_weak_factory_.InvalidateWeakPtrs();
  if (GetCurrentUiSettings().theme() == mojom::AmbientTheme::kVideo) {
    std::optional<AmbientVideo> video = GetCurrentUiSettings().video();
    DCHECK(video.has_value());
    auto url_arr =
        AmbientBackendController::Get()->GetTimeOfDayVideoPreviewImageUrls(
            video.value());
    std::vector<GURL> previews;
    base::ranges::transform(url_arr, std::back_inserter(previews),
                            [](const char* url) { return GURL(url); });
    OnPreviewsFetched(std::move(previews));
    return;
  }

  const gfx::Size image_size = gfx::Size(kBannerWidthPx, kBannerHeightPx);
  ash::AmbientBackendController::Get()->FetchPreviewImages(
      image_size,
      base::BindOnce(&PersonalizationAppAmbientProviderImpl::OnPreviewsFetched,
                     previews_weak_factory_.GetWeakPtr()));
}

void PersonalizationAppAmbientProviderImpl::OnPreviewsFetched(
    const std::vector<GURL>& preview_urls) {
  DVLOG(4) << __func__ << " preview_urls_size=" << preview_urls.size();
  ambient_observer_remote_->OnPreviewsFetched(preview_urls);
}

ash::PersonalAlbum*
PersonalizationAppAmbientProviderImpl::FindPersonalAlbumById(
    const std::string& album_id) {
  auto it = base::ranges::find(personal_albums_.albums, album_id,
                               &ash::PersonalAlbum::album_id);

  if (it == personal_albums_.albums.end()) {
    return nullptr;
  }

  return &(*it);
}

ash::ArtSetting* PersonalizationAppAmbientProviderImpl::FindArtAlbumById(
    const std::string& album_id) {
  auto it = base::ranges::find(settings_->art_settings, album_id,
                               &ash::ArtSetting::album_id);
  // Album does not exist any more.
  if (it == settings_->art_settings.end()) {
    return nullptr;
  }

  return &(*it);
}

void PersonalizationAppAmbientProviderImpl::ResetLocalSettings() {
  write_weak_factory_.InvalidateWeakPtrs();
  read_weak_factory_.InvalidateWeakPtrs();
  previews_weak_factory_.InvalidateWeakPtrs();

  settings_.reset();
  cached_settings_.reset();
  update_settings_retry_backoff_.Reset();
  fetch_settings_retry_backoff_.Reset();
  is_updating_backend_ = false;
}

void PersonalizationAppAmbientProviderImpl::StartScreenSaverPreview() {
  Shell::Get()->ambient_controller()->SetUiVisibilityPreview();
}

void PersonalizationAppAmbientProviderImpl::ShouldShowTimeOfDayBanner(
    ShouldShowTimeOfDayBannerCallback callback) {
  // Time of day banner should not display for the users with policy managed
  // wallpapers who cannot change their wallpapers. Note that although
  // enterprise users are not able to access screen saver, some of them are able
  // to access wallpaper subpage and change wallpapers.
  std::move(callback).Run(
      !WallpaperController::Get()->IsWallpaperControlledByPolicy(
          GetAccountId(profile_)) &&
      features::IsTimeOfDayScreenSaverEnabled() &&
      contextual_tooltip::ShouldShowNudge(
          profile_->GetPrefs(),
          contextual_tooltip::TooltipType::kTimeOfDayFeatureBanner,
          /*recheck_delay=*/nullptr));
}

void PersonalizationAppAmbientProviderImpl::HandleTimeOfDayBannerDismissed() {
  contextual_tooltip::HandleGesturePerformed(
      profile_->GetPrefs(),
      contextual_tooltip::TooltipType::kTimeOfDayFeatureBanner);
}

void PersonalizationAppAmbientProviderImpl::
    IsGeolocationEnabledForSystemServices(
        IsGeolocationEnabledForSystemServicesCallback callback) {
  std::move(callback).Run(IsGeolocationEnabledForSystemServices());
}

void PersonalizationAppAmbientProviderImpl::IsGeolocationUserModifiable(
    IsGeolocationUserModifiableCallback callback) {
  std::move(callback).Run(IsGeolocationUserModifiable());
}

void PersonalizationAppAmbientProviderImpl::
    EnableGeolocationForSystemServices() {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetInteger(
      prefs::kUserGeolocationAccessLevel,
      static_cast<int>(GeolocationAccessLevel::kOnlyAllowedForSystem));
}

void PersonalizationAppAmbientProviderImpl::OnAmbientUiVisibilityChanged(
    ash::AmbientUiVisibility visibility) {
  if (ambient_observer_remote_.is_bound()) {
    ambient_observer_remote_->OnAmbientUiVisibilityChanged(visibility);
  }
}

mojom::TopicSource
PersonalizationAppAmbientProviderImpl::GetCurrentTopicSource() const {
  if (GetCurrentUiSettings().theme() == mojom::AmbientTheme::kVideo) {
    return mojom::TopicSource::kVideo;
  } else {
    DCHECK(settings_);
    return settings_->topic_source;
  }
}

}  // namespace ash::personalization_app

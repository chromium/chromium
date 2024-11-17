// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/system/focus_mode/sounds/focus_mode_sounds_controller.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_metrics_recorder.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/focus_mode/sounds/focus_mode_soundscape_delegate.h"
#include "ash/system/focus_mode/sounds/focus_mode_youtube_music_delegate.h"
#include "ash/system/focus_mode/sounds/sound_section_view.h"
#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_types.h"
#include "base/barrier_callback.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/media_session/public/cpp/media_session_service.h"
#include "services/media_session/public/cpp/util.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr size_t kMaxAttemptToDownloadThumbnail = 3;

// Arrays for histogram records.
constexpr focus_mode_histogram_names::FocusModePlaylistChosen
    soundscapes_chosen[] = {
        focus_mode_histogram_names::FocusModePlaylistChosen::kSoundscapes1,
        focus_mode_histogram_names::FocusModePlaylistChosen::kSoundscapes2,
        focus_mode_histogram_names::FocusModePlaylistChosen::kSoundscapes3,
        focus_mode_histogram_names::FocusModePlaylistChosen::kSoundscapes4};
constexpr focus_mode_histogram_names::FocusModePlaylistChosen
    youtube_music_chosen[] = {
        focus_mode_histogram_names::FocusModePlaylistChosen::kYouTubeMusic1,
        focus_mode_histogram_names::FocusModePlaylistChosen::kYouTubeMusic2,
        focus_mode_histogram_names::FocusModePlaylistChosen::kYouTubeMusic3,
        focus_mode_histogram_names::FocusModePlaylistChosen::kYouTubeMusic4};

constexpr net::NetworkTrafficAnnotationTag kFocusModeSoundsThumbnailTag =
    net::DefineNetworkTrafficAnnotation("focus_mode_sounds_image_downloader",
                                        R"(
        semantics {
          sender: "Focus Mode"
          description:
            "Download Focus Mode Sounds playlist thumbnails which will be "
            "shown on the focus mode panel."
          trigger: "User opens a panel in Focus Mode."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
          user_data {
            type: NONE
          }
          internal {
            contacts {
              email: "hongyulong@google.com"
            }
            contacts {
              email: "chromeos-wms@google.com"
            }
          }
          last_reviewed: "2024-03-15"
        }
        policy {
         cookies_allowed: NO
         setting:
           "This feature is off by default and can be overridden by user."
         chrome_policy {
           FocusModeSoundsEnabled {
             FocusModeSoundsEnabled: "disabled"
           }
         }
        })");

// Invoked upon completion of the `thumbnail` download. `thumbnail` can be a
// null image if the download attempt from the url failed. A simple retry will
// be applied until `attempt_counter` reaches the maximum.
void OnOneThumbnailDownloaded(
    const base::Time start_time,
    base::OnceCallback<void(
        std::unique_ptr<FocusModeSoundsController::Playlist>)> barrier_callback,
    const FocusModeSoundsDelegate::Playlist& playlist,
    const size_t attempt_counter,
    const gfx::ImageSkia& thumbnail) {
  const std::string method = "ImageDownload.PlaylistThumbnail";
  focus_mode_util::RecordHistogramForApiLatency(method,
                                                base::Time::Now() - start_time);

  if (thumbnail.isNull() && attempt_counter < kMaxAttemptToDownloadThumbnail) {
    FocusModeSoundsController::DownloadTrackThumbnail(
        playlist.thumbnail_url, base::BindOnce(&OnOneThumbnailDownloaded,
                                               /*start_time=*/base::Time::Now(),
                                               std::move(barrier_callback),
                                               playlist, attempt_counter + 1));
    return;
  }

  focus_mode_util::RecordHistogramForApiResult(
      method, /*successful=*/!thumbnail.isNull());
  focus_mode_util::RecordHistogramForApiRetryCount(
      method, static_cast<int>(attempt_counter) - 1);

  std::move(barrier_callback)
      .Run(std::make_unique<FocusModeSoundsController::Playlist>(
          playlist.id, playlist.title, thumbnail));
}

// Re-order `playlists` according to the order of `data`.
void ReorderPlaylists(
    const std::vector<FocusModeSoundsDelegate::Playlist>& data,
    base::OnceCallback<
        void(std::vector<std::unique_ptr<FocusModeSoundsController::Playlist>>)>
        sorted_playlists_callback,
    std::vector<std::unique_ptr<FocusModeSoundsController::Playlist>>
        unsorted_playlists) {
  std::vector<std::unique_ptr<FocusModeSoundsController::Playlist>>
      sorted_playlists;

  // Create `sorted_playlists` to match the given order.
  for (const auto& item : data) {
    auto iter = std::find_if(
        unsorted_playlists.begin(), unsorted_playlists.end(),
        [item](const std::unique_ptr<FocusModeSoundsController::Playlist>&
                   playlist) {
          return playlist && playlist->playlist_id == item.id;
        });
    if (iter == unsorted_playlists.end()) {
      continue;
    }

    sorted_playlists.push_back(std::move(*iter));
  }

  std::move(sorted_playlists_callback).Run(std::move(sorted_playlists));
}

// In response to receiving the playlists, start downloading the playlist
// thumbnails.
void DispatchRequests(
    base::OnceCallback<
        void(std::vector<std::unique_ptr<FocusModeSoundsController::Playlist>>)>
        sorted_playlists_callback,
    const std::vector<FocusModeSoundsDelegate::Playlist>& data) {
  if (data.empty()) {
    LOG(WARNING) << "Retrieving Playlist data failed.";
    std::move(sorted_playlists_callback).Run({});
    return;
  }

  CHECK_EQ(data.size(), kFocusModePlaylistViewsNum);

  // TODO(b/340304748): Currently, when opening the focus panel, we will clean
  // up all saved data and then download all playlists. In the future, we can
  // keep this cached and update if there are new playlists.
  using BarrierReturn = std::unique_ptr<FocusModeSoundsController::Playlist>;
  auto barrier_callback = base::BarrierCallback<BarrierReturn>(
      /*num_callbacks=*/kFocusModePlaylistViewsNum,
      /*done_callback=*/base::BindOnce(&ReorderPlaylists, data,
                                       std::move(sorted_playlists_callback)));

  for (const auto& item : data) {
    FocusModeSoundsController::DownloadTrackThumbnail(
        item.thumbnail_url,
        base::BindOnce(&OnOneThumbnailDownloaded,
                       /*start_time=*/base::Time::Now(), barrier_callback, item,
                       /*attempt_counter=*/1));
  }
}

// In response to receiving the track, start playing the track.
void OnTrackFetched(
    FocusModeSoundsController::GetNextTrackCallback callback,
    const std::optional<FocusModeSoundsDelegate::Track>& track) {
  if (!track) {
    // TODO(b/343961303): Potentially retry the request.
    LOG(WARNING) << "Retrieving track failed";
  }

  std::move(callback).Run(track);
}

// Parses the ash.focus_mode.sounds_enabled pref and returns a set of the
// `SoundType`s that should be enabled.
base::flat_set<focus_mode_util::SoundType> ReadSoundSectionPolicy(
    const PrefService* pref_service) {
  CHECK(pref_service);
  const std::string& enabled_sections_pref =
      pref_service->GetString(prefs::kFocusModeSoundsEnabled);

  if (enabled_sections_pref == focus_mode_util::kFocusModeSoundsEnabled) {
    return {focus_mode_util::SoundType::kSoundscape,
            focus_mode_util::SoundType::kYouTubeMusic};
  } else if (enabled_sections_pref == focus_mode_util::kFocusSoundsOnly) {
    return {focus_mode_util::SoundType::kSoundscape};
  } else if (enabled_sections_pref ==
             focus_mode_util::kFocusModeSoundsDisabled) {
    return {};
  }

  // Unrecognized value. It's likely a new restriction so disable everything.
  return {};
}

// Return true if there is no selected playlist, or if the selected playlist
// type doesn't match `playlists_fetched` (which means the selected playlist
// couldn't be found in this list), or if the selected playlist is found in the
// `playlists_fetched`.
bool MayContainsSelectedPlaylist(
    const focus_mode_util::SelectedPlaylist& selected_playlist,
    bool is_soundscape_type,
    const std::vector<std::unique_ptr<FocusModeSoundsController::Playlist>>&
        playlists_fetched) {
  if (selected_playlist.empty() ||
      selected_playlist.type !=
          (is_soundscape_type ? focus_mode_util::SoundType::kSoundscape
                              : focus_mode_util::SoundType::kYouTubeMusic)) {
    return true;
  }

  return base::Contains(playlists_fetched, selected_playlist.id,
                        &FocusModeSoundsController::Playlist::playlist_id);
}

bool HasAudioFocus(const base::UnguessableToken& focus_mode_request_id,
                   const base::UnguessableToken& session_request_id) {
  return !focus_mode_request_id.is_empty() &&
         focus_mode_request_id == session_request_id;
}

void RecordPlaylistPlayedLatency(focus_mode_util::SoundType playlist_type,
                                 const base::Time& sounds_started_time) {
  std::string histogram_name;
  switch (playlist_type) {
    case focus_mode_util::SoundType::kSoundscape:
      histogram_name = focus_mode_histogram_names::
          kSoundscapeLatencyInMillisecondsHistogramName;
      break;
    case focus_mode_util::SoundType::kYouTubeMusic:
      histogram_name = focus_mode_histogram_names::
          kYouTubeMusicLatencyInMillisecondsHistogramName;
      break;
    case focus_mode_util::SoundType::kNone:
      // A selected playlist should always have a valid type.
      NOTREACHED();
  }

  base::UmaHistogramCustomCounts(
      /*name=*/histogram_name,
      /*sample=*/(base::Time::Now() - sounds_started_time).InMilliseconds(),
      /*min=*/0, /*exclusive_max=*/2000, /*buckets=*/50);
}

focus_mode_histogram_names::FocusModePlaylistChosen GetPlaylistChosenType(
    size_t index,
    focus_mode_util::SoundType sound_type) {
  CHECK_LT(index, kFocusModePlaylistViewsNum);
  switch (sound_type) {
    case focus_mode_util::SoundType::kSoundscape:
      return soundscapes_chosen[index];
    case focus_mode_util::SoundType::kYouTubeMusic:
      return youtube_music_chosen[index];
    case focus_mode_util::SoundType::kNone:
      NOTREACHED();
  }
}

void RecordPlaylistChosenHistogram(
    const focus_mode_util::SelectedPlaylist& selected_playlist) {
  base::UmaHistogramEnumeration(
      /*name=*/focus_mode_histogram_names::kPlaylistChosenHistogram,
      /*sample=*/GetPlaylistChosenType(selected_playlist.list_position,
                                       selected_playlist.type));
}

}  // namespace

FocusModeSoundsController::FocusModeSoundsController(const std::string& locale)
    : youtube_music_delegate_(
          std::make_unique<FocusModeYouTubeMusicDelegate>()) {
  soundscape_delegate_ = FocusModeSoundscapeDelegate::Create(locale);

  // Default sound sections to enabled.
  enabled_sound_sections_ = {focus_mode_util::SoundType::kSoundscape,
                             focus_mode_util::SoundType::kYouTubeMusic};

  // `service` can be null in tests.
  media_session::MediaSessionService* service =
      Shell::Get()->shell_delegate()->GetMediaSessionService();
  if (!service) {
    return;
  }

  // Connect to receive audio focus events.
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote;
  service->BindAudioFocusManager(
      audio_focus_remote.BindNewPipeAndPassReceiver());
  audio_focus_remote->AddObserver(
      audio_focus_observer_receiver_.BindNewPipeAndPassRemote());

  // Connect to the `MediaControllerManager`.
  service->BindMediaControllerManager(
      media_controller_manager_remote_.BindNewPipeAndPassReceiver());
}

FocusModeSoundsController::~FocusModeSoundsController() = default;

// static
void FocusModeSoundsController::DownloadTrackThumbnail(
    const GURL& url,
    ImageDownloader::DownloadCallback callback) {
  CHECK(!url.is_empty());

  const UserSession* active_user_session =
      Shell::Get()->session_controller()->GetUserSession(0);
  CHECK(active_user_session);

  ImageDownloader::Get()->Download(url, kFocusModeSoundsThumbnailTag,
                                   active_user_session->user_info.account_id,
                                   std::move(callback));
}

void FocusModeSoundsController::GetNextTrack(GetNextTrackCallback callback) {
  if (selected_playlist_.type == focus_mode_util::SoundType::kNone ||
      selected_playlist_.id.empty() || !IsPlaylistAllowed(selected_playlist_)) {
    LOG(WARNING) << "No selected playlist";
    std::move(callback).Run(std::nullopt);
    return;
  }

  FocusModeSoundsDelegate* delegate;
  if (selected_playlist_.type == focus_mode_util::SoundType::kSoundscape) {
    delegate = soundscape_delegate_.get();
  } else if (selected_playlist_.type ==
             focus_mode_util::SoundType::kYouTubeMusic) {
    delegate = youtube_music_delegate_.get();
  } else {
    LOG(ERROR) << "Unrecognized playlist type";
    std::move(callback).Run(std::nullopt);
    return;
  }

  delegate->GetNextTrack(selected_playlist_.id,
                         base::BindOnce(&OnTrackFetched, std::move(callback)));
}

void FocusModeSoundsController::ReportPlayerError() {
  for (auto& observer : observers_) {
    observer.OnPlayerError();
  }
}

void FocusModeSoundsController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FocusModeSoundsController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FocusModeSoundsController::OnFocusGained(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  if (selected_playlist_.empty() || has_audio_focus_) {
    return;
  }

  CHECK(session->request_id.has_value());
  const auto& request_id =
      FocusModeController::Get()->GetMediaSessionRequestId();
  has_audio_focus_ = HasAudioFocus(request_id, session->request_id.value());

  // If it's not our focus mode media gained the focus, or if the request id
  // isn't changed, we will do nothing.
  if (!has_audio_focus_ || media_session_request_id_ == request_id) {
    return;
  }

  RecordPlaylistPlayedLatency(selected_playlist_.type, sounds_started_time_);
  RecordPlaylistChosenHistogram(selected_playlist_);

  // Otherwise, we will bind the media controller observer with the specific
  // request id to observe our media state.
  media_session_request_id_ = request_id;

  // `media_controller_manager_remote_` is null in test.
  if (!media_controller_manager_remote_) {
    return;
  }

  media_controller_remote_.reset();
  media_controller_observer_receiver_.reset();

  media_controller_manager_remote_->CreateMediaControllerForSession(
      media_controller_remote_.BindNewPipeAndPassReceiver(),
      media_session_request_id_);
  media_controller_remote_->AddObserver(
      media_controller_observer_receiver_.BindNewPipeAndPassRemote());
}

void FocusModeSoundsController::OnFocusLost(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  if (!has_audio_focus_) {
    return;
  }

  CHECK(session->request_id.has_value());
  has_audio_focus_ =
      HasAudioFocus(FocusModeController::Get()->GetMediaSessionRequestId(),
                    session->request_id.value());
}

void FocusModeSoundsController::OnRequestIdReleased(
    const base::UnguessableToken& request_id) {
  if (request_id.is_empty() || request_id != media_session_request_id_) {
    return;
  }

  has_audio_focus_ = false;
  media_session_request_id_ = base::UnguessableToken::Null();
  if (selected_playlist_.empty() ||
      selected_playlist_.state != focus_mode_util::SoundState::kPlaying) {
    return;
  }

  // When entering the ending moment, the media widget will be closed, then
  // the request id will be released. Hence, we need to update the state of
  // the `selected_playlist_` when entering the ending moment.
  selected_playlist_.state = focus_mode_util::SoundState::kSelected;
  for (auto& observer : observers_) {
    observer.OnPlaylistStateChanged();
  }
}

void FocusModeSoundsController::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  if (!session_info) {
    return;
  }

  switch (session_info->playback_state) {
    case media_session::mojom::MediaPlaybackState::kPlaying:
      selected_playlist_.state = focus_mode_util::SoundState::kPlaying;
      break;
    case media_session::mojom::MediaPlaybackState::kPaused:
      selected_playlist_.state = focus_mode_util::SoundState::kPaused;
      // TODO(b/343795949): Make sure to not count this when pause events are
      // triggered by the ending moment in the future.
      paused_event_count_++;
      break;
  }

  for (auto& observer : observers_) {
    observer.OnPlaylistStateChanged();
  }
}

void FocusModeSoundsController::TogglePlaylist(
    const focus_mode_util::SelectedPlaylist& playlist_data) {
  CHECK_LT(playlist_data.list_position, kFocusModePlaylistViewsNum);
  if (playlist_data.state != focus_mode_util::SoundState::kNone) {
    // When the user toggles a selected playlist, we will deselect it.
    ResetSelectedPlaylist();
  } else {
    SelectPlaylist(playlist_data);
  }
}

void FocusModeSoundsController::PausePlayback() {
  if (simulate_playback_for_testing_) {
    CHECK_IS_TEST();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce([]() {
          auto* sounds_controller =
              FocusModeController::Get()->focus_mode_sounds_controller();
          sounds_controller
              ->update_selected_playlist_state_for_testing(  // IN-TEST
                  focus_mode_util::SoundState::kPaused);
        }));
    return;
  }

  if (media_controller_remote_ && media_controller_remote_.is_bound() &&
      !media_session_request_id_.is_empty()) {
    media_session::PerformMediaSessionAction(
        media_session::mojom::MediaSessionAction::kPause,
        media_controller_remote_);
  }
}

void FocusModeSoundsController::ResumePlayingPlayback() {
  if (simulate_playback_for_testing_) {
    CHECK_IS_TEST();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce([]() {
          auto* sounds_controller =
              FocusModeController::Get()->focus_mode_sounds_controller();
          sounds_controller
              ->update_selected_playlist_state_for_testing(  // IN-TEST
                  focus_mode_util::SoundState::kPlaying);
        }));
    return;
  }

  if (media_controller_remote_ && media_controller_remote_.is_bound() &&
      !media_session_request_id_.is_empty()) {
    media_session::PerformMediaSessionAction(
        media_session::mojom::MediaSessionAction::kPlay,
        media_controller_remote_);
  }
}

void FocusModeSoundsController::DownloadPlaylistsForType(
    const bool is_soundscape_type,
    UpdateSoundsViewCallback update_sounds_view_callback) {
  // During shutdown, `ImageDownloader` may not exist here.
  if (!ImageDownloader::Get()) {
    return;
  }

  if (is_soundscape_type) {
    if (!base::Contains(enabled_sound_sections_,
                        focus_mode_util::SoundType::kSoundscape)) {
      LOG(WARNING) << "Playlist download for Focus Sounds blocked by policy";
      return;
    }
  } else {
    if (!base::Contains(enabled_sound_sections_,
                        focus_mode_util::SoundType::kYouTubeMusic)) {
      LOG(WARNING)
          << "Playlist download for YouTube Music blocked by policy or flag";
      return;
    }
  }

  auto sorted_playlists_callback =
      base::BindOnce(&FocusModeSoundsController::OnAllThumbnailsDownloaded,
                     weak_factory_.GetWeakPtr(), is_soundscape_type,
                     std::move(update_sounds_view_callback));

  if (is_soundscape_type) {
    soundscape_delegate_->GetPlaylists(base::BindOnce(
        &DispatchRequests, std::move(sorted_playlists_callback)));
  } else {
    youtube_music_delegate_->GetPlaylists(base::BindOnce(
        &DispatchRequests, std::move(sorted_playlists_callback)));
  }
}

void FocusModeSoundsController::UpdateFromUserPrefs() {
  PrefService* active_user_prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!active_user_prefs) {
    return;
  }
  pref_registrar_.Reset();
  pref_registrar_.Init(active_user_prefs);
  pref_registrar_.Add(
      prefs::kFocusModeSoundsEnabled,
      base::BindRepeating(&FocusModeSoundsController::OnPrefChanged,
                          weak_factory_.GetWeakPtr()));
  OnPrefChanged();

  const auto& dict = active_user_prefs->GetDict(prefs::kFocusModeSoundSection);

  // If the user didn't select any playlist before, we should show the
  // `Soundscape` sound section as default behavior.
  if (dict.empty()) {
    sound_type_ = focus_mode_util::SoundType::kSoundscape;
  } else {
    sound_type_ = static_cast<focus_mode_util::SoundType>(
        dict.FindInt(focus_mode_util::kSoundTypeKey).value());
  }
}

void FocusModeSoundsController::SetYouTubeMusicNoPremiumCallback(
    base::RepeatingClosure callback) {
  CHECK(callback);
  youtube_music_delegate_->SetNoPremiumCallback(std::move(callback));
}

void FocusModeSoundsController::SetErrorCallback(bool is_soundscape,
                                                 ApiErrorCallback callback) {
  CHECK(callback);
  CHECK(!is_soundscape) << "Soundscapes errors are unsupported";

  youtube_music_delegate_->SetErrorCallback(std::move(callback));
}

const std::optional<FocusModeApiError>&
FocusModeSoundsController::last_youtube_music_error() const {
  return youtube_music_delegate_->last_api_error();
}

void FocusModeSoundsController::ReportYouTubeMusicPlayback(
    const youtube_music::PlaybackData& playback_data) {
  youtube_music_delegate_->ReportPlayback(playback_data);
}

bool FocusModeSoundsController::ShouldDisplayYouTubeMusicOAuth() const {
  if (PrefService* active_user_prefs =
          Shell::Get()->session_controller()->GetActivePrefService()) {
    return active_user_prefs->GetBoolean(
        prefs::kFocusModeYTMDisplayOAuthConsent);
  }
  CHECK_IS_TEST();
  return true;
}

void FocusModeSoundsController::SavePrefForDisplayYouTubeMusicOAuth() {
  if (PrefService* active_user_prefs =
          Shell::Get()->session_controller()->GetActivePrefService()) {
    active_user_prefs->SetBoolean(prefs::kFocusModeYTMDisplayOAuthConsent,
                                  false);
  }
}

bool FocusModeSoundsController::ShouldDisplayYouTubeMusicFreeTrial() const {
  if (PrefService* active_user_prefs =
          Shell::Get()->session_controller()->GetActivePrefService()) {
    return active_user_prefs->GetBoolean(prefs::kFocusModeYTMDisplayFreeTrial);
  }
  CHECK_IS_TEST();
  return true;
}

void FocusModeSoundsController::SavePrefForDisplayYouTubeMusicFreeTrial() {
  if (PrefService* active_user_prefs =
          Shell::Get()->session_controller()->GetActivePrefService()) {
    active_user_prefs->SetBoolean(prefs::kFocusModeYTMDisplayFreeTrial, false);
  }
}

bool FocusModeSoundsController::IsMinorUser() {
  // `ChromeFocusModeDelegate::IsMinorUser` doesn't work in browsertest, since
  // it always returns true. `can_use_manta_service()` is
  // `signin::Tribool::kUnknown`.
  if (is_minor_user_for_testing_.has_value()) {
    CHECK_IS_TEST();
    return is_minor_user_for_testing_.value();
  }

  return FocusModeController::Get()->delegate()->IsMinorUser();
}

void FocusModeSoundsController::SetIsMinorUserForTesting(bool is_minor_user) {
  CHECK_IS_TEST();
  is_minor_user_for_testing_ = is_minor_user;
  OnPrefChanged();
}

bool FocusModeSoundsController::IsPlaylistAllowed(
    const focus_mode_util::SelectedPlaylist& playlist) const {
  return base::Contains(enabled_sound_sections_, playlist.type);
}

void FocusModeSoundsController::SaveUserPref() {
  if (PrefService* active_user_prefs =
          Shell::Get()->session_controller()->GetActivePrefService()) {
    base::Value::Dict dict;
    dict.Set(focus_mode_util::kSoundTypeKey, static_cast<int>(sound_type_));
    dict.Set(focus_mode_util::kPlaylistIdKey, selected_playlist_.id);
    active_user_prefs->SetDict(prefs::kFocusModeSoundSection, std::move(dict));
  }
}

void FocusModeSoundsController::ResetSelectedPlaylist() {
  // TODO: Stop the music for current selected playlist.
  selected_playlist_ = {};

  // We still want to keep the user pref for sound section after deselecting the
  // selected playlist.
  SaveUserPref();
  for (auto& observer : observers_) {
    observer.OnSelectedPlaylistChanged();
  }
}

void FocusModeSoundsController::SelectPlaylist(
    const focus_mode_util::SelectedPlaylist& playlist_data) {
  if (!IsPlaylistAllowed(playlist_data)) {
    LOG(WARNING) << "Playlist cannot be selected due to policy";
    return;
  }

  if (FocusModeController::Get()->in_focus_session()) {
    SoundsStarted();
  }

  selected_playlist_ = playlist_data;

  // TODO(b/337063849): Update the sound state when the media stream
  // actually starts playing.
  selected_playlist_.state = focus_mode_util::SoundState::kSelected;
  sound_type_ = selected_playlist_.type;

  // Reserve a place for the last selected playlist for future use.
  if (sound_type_ == focus_mode_util::SoundType::kYouTubeMusic) {
    youtube_music_delegate_->ReservePlaylistForGetPlaylists(
        selected_playlist_.id);
  }

  SaveUserPref();
  for (auto& observer : observers_) {
    observer.OnSelectedPlaylistChanged();
  }
}

void FocusModeSoundsController::OnAllThumbnailsDownloaded(
    bool is_soundscape_type,
    UpdateSoundsViewCallback update_sounds_view_callback,
    std::vector<std::unique_ptr<Playlist>> sorted_playlists) {
  // Please note, `sorted_playlists` can be empty, so it's important to check if
  // the number of playlists is expected before manipulating the list. For the
  // case that the `selected_playlist_` is missing from the list of playlists
  // fetched from backend, we will show the cached playlist info if the selected
  // playlist is currently playing; otherwise, we will clear the selected
  // playlist in the controller.
  if (sorted_playlists.size() == kFocusModePlaylistViewsNum &&
      !MayContainsSelectedPlaylist(selected_playlist_, is_soundscape_type,
                                   sorted_playlists)) {
    if (selected_playlist_.state == focus_mode_util::SoundState::kPlaying) {
      sorted_playlists.pop_back();
      sorted_playlists.insert(
          sorted_playlists.begin() + 1,
          std::make_unique<Playlist>(selected_playlist_.id,
                                     selected_playlist_.title,
                                     selected_playlist_.thumbnail));
      selected_playlist_.list_position = 1;
    } else {
      ResetSelectedPlaylist();
    }
  }

  // Only trigger the observer function when all the thumbnails are finished
  // downloading.
  // TODO(b/321071604): We may need to update this once caching is implemented.
  std::move(update_sounds_view_callback)
      .Run(is_soundscape_type, std::move(sorted_playlists));
}

void FocusModeSoundsController::OnPrefChanged() {
  PrefService* active_user_prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  enabled_sound_sections_ = ReadSoundSectionPolicy(active_user_prefs);

  // TODO: If we want to block the whole YTM section, we should make changes
  // here. And remove the calls to `FocusModeSoundsController::IsMinorUser()` in
  // `FocusModeSoundsView`.

  // Hide the YTM sound section if the flag isn't enabled.
  if (!features::IsFocusModeYTMEnabled()) {
    enabled_sound_sections_.erase(focus_mode_util::SoundType::kYouTubeMusic);
  }
}

}  // namespace ash

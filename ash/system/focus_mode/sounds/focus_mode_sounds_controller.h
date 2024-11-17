// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_CONTROLLER_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_CONTROLLER_H_

#include <optional>
#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/focus_mode/sounds/focus_mode_api_error.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_delegate.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace ash {

namespace youtube_music {
struct PlaybackData;
}  // namespace youtube_music

class FocusModeYouTubeMusicDelegate;

// This class is used to download images and record the info of playlists after
// getting the response data we need from Music API, which will be used to show
// on `FocusModeSoundsView`.
class ASH_EXPORT FocusModeSoundsController
    : public media_session::mojom::AudioFocusObserver,
      public media_session::mojom::MediaControllerObserver {
 public:
  // The data used to display on the focus panel. It will include a playlist id,
  // a string of its title, and the downloaded thumbnail for the playlist cover
  // currently. We will add the stream info in future.
  struct Playlist {
    // Playlist identifier.
    std::string playlist_id;

    // Title of the playlist.
    std::string title;

    // Playlist cover downloaded through its image url.
    gfx::ImageSkia thumbnail;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when a playlist is toggled by the user on the focus panel.
    virtual void OnSelectedPlaylistChanged() = 0;
    // Called when the state of `selected_playlist_` has been changed.
    virtual void OnPlaylistStateChanged() {}
    // Called when the media player encounters an error.
    virtual void OnPlayerError() {}
  };

  FocusModeSoundsController(const std::string& locale);
  FocusModeSoundsController(const FocusModeSoundsController&) = delete;
  FocusModeSoundsController& operator=(const FocusModeSoundsController&) =
      delete;
  ~FocusModeSoundsController() override;

  // Download the artwork for a track. Exposed here so that native portion of
  // the focus mode web UI can download the artwork using the focus mode network
  // traffic annotation.
  static void DownloadTrackThumbnail(
      const GURL& url,
      ImageDownloader::DownloadCallback callback);

  using GetNextTrackCallback = base::OnceCallback<void(
      const std::optional<FocusModeSoundsDelegate::Track>&)>;
  void GetNextTrack(GetNextTrackCallback callback);

  // Called by `FocusModeTrackProvider::ReportPlayerError`.
  void ReportPlayerError();

  const focus_mode_util::SelectedPlaylist& selected_playlist() const {
    return selected_playlist_;
  }

  focus_mode_util::SoundType sound_type() const { return sound_type_; }

  void SoundsStarted() { sounds_started_time_ = base::Time::Now(); }

  const base::flat_set<focus_mode_util::SoundType>& sound_sections() const {
    return enabled_sound_sections_;
  }

  void reset_paused_event_count() { paused_event_count_ = 0; }
  int paused_event_count() const { return paused_event_count_; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // media_session::mojom::AudioFocusObserver:
  void OnFocusGained(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnFocusLost(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnRequestIdReleased(const base::UnguessableToken& request_id) override;

  // media_session::mojom::MediaControllerObserver:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const std::optional<media_session::MediaMetadata>& metadata) override {}
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override {}
  void MediaSessionChanged(
      const std::optional<base::UnguessableToken>& request_id) override {}
  void MediaSessionPositionChanged(
      const std::optional<media_session::MediaPosition>& position) override {}

  // Toggles a playlist with the same id as the `playlist_data` to select or
  // deselect based on its previous state.
  void TogglePlaylist(const focus_mode_util::SelectedPlaylist& playlist_data);

  void PausePlayback();
  void ResumePlayingPlayback();

  // Download images by providing urls. `update_sounds_view_callback` will be
  // called only when finishing downloading all non-empty thumbnails for the
  // Soundscape type or the YouTube Music type of playlists; however, if
  // `ImageDownloader` doesn't exists or if there is an empty thumbnail
  // downloaded, `update_sounds_view_callback` will be not triggered.
  using UpdateSoundsViewCallback =
      base::OnceCallback<void(bool,
                              const std::vector<std::unique_ptr<Playlist>>&)>;
  void DownloadPlaylistsForType(
      const bool is_soundscape_type,
      UpdateSoundsViewCallback update_sounds_view_callback);

  void UpdateFromUserPrefs();

  // Sets the no premium callback for all YouTube Music API requests. This
  // callback is used to update the specific UIs that are dependent on the
  // account premium status.
  void SetYouTubeMusicNoPremiumCallback(base::RepeatingClosure callback);

  // Sets a callback to receive errors from the chosen API backend. If
  // `is_soundscape` is false, connects to YouTube Music. True is currently
  // unimplemented.
  void SetErrorCallback(bool is_soundscape, ApiErrorCallback error_callback);

  const std::optional<FocusModeApiError>& last_youtube_music_error() const;

  // Reports playback to the media server. It's only used for YouTube Music at
  // the moment.
  void ReportYouTubeMusicPlayback(
      const youtube_music::PlaybackData& playback_data);

  bool ShouldDisplayYouTubeMusicOAuth() const;
  void SavePrefForDisplayYouTubeMusicOAuth();
  bool ShouldDisplayYouTubeMusicFreeTrial() const;
  void SavePrefForDisplayYouTubeMusicFreeTrial();

  void set_selected_playlist_for_testing(
      const focus_mode_util::SelectedPlaylist& playlist) {
    selected_playlist_ = playlist;
  }
  void update_selected_playlist_state_for_testing(
      focus_mode_util::SoundState new_state) {
    selected_playlist_.state = new_state;
  }
  void set_simulate_playback_for_testing() {
    simulate_playback_for_testing_ = true;
  }

  bool IsMinorUser();
  void SetIsMinorUserForTesting(bool is_minor_user);

 private:
  bool IsPlaylistAllowed(
      const focus_mode_util::SelectedPlaylist& playlist) const;

  void SaveUserPref();
  void ResetSelectedPlaylist();
  void SelectPlaylist(const focus_mode_util::SelectedPlaylist& playlist_data);

  void OnAllThumbnailsDownloaded(
      bool is_soundscape_type,
      UpdateSoundsViewCallback update_sounds_view_callback,
      std::vector<std::unique_ptr<Playlist>> sorted_playlists);

  // Handler for changes in the FocusModeSoundsEnabled pref.
  void OnPrefChanged();

  std::unique_ptr<FocusModeSoundsDelegate> soundscape_delegate_;
  std::unique_ptr<FocusModeYouTubeMusicDelegate> youtube_music_delegate_;

  focus_mode_util::SelectedPlaylist selected_playlist_;
  focus_mode_util::SoundType sound_type_ =
      focus_mode_util::SoundType::kSoundscape;

  // Records the time when we requested to play a selected playlist.
  base::Time sounds_started_time_;
  // Records how many times the user paused `selected_playlist_` during a
  // session.
  int paused_event_count_ = 0;

  PrefChangeRegistrar pref_registrar_;
  base::flat_set<focus_mode_util::SoundType> enabled_sound_sections_;

  // True if the request id of the focus mode media session has gained audio
  // focus. Note that focus mode will only have a maximum of one media playing
  // at any given time.
  bool has_audio_focus_ = false;
  base::UnguessableToken media_session_request_id_ =
      base::UnguessableToken::Null();

  bool simulate_playback_for_testing_ = false;

  // Sets the value to true or false in browertest.
  std::optional<bool> is_minor_user_for_testing_;

  base::ObserverList<Observer> observers_;

  // Used to control the media session.
  mojo::Remote<media_session::mojom::MediaControllerManager>
      media_controller_manager_remote_;
  mojo::Remote<media_session::mojom::MediaController> media_controller_remote_;

  // Connections with the media session service to listen for audio focus
  // updates and control media sessions.
  mojo::Receiver<media_session::mojom::AudioFocusObserver>
      audio_focus_observer_receiver_{this};
  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      media_controller_observer_receiver_{this};

  base::WeakPtrFactory<FocusModeSoundsController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_CONTROLLER_H_

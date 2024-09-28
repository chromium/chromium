// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_VIEW_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/style/error_message_toast.h"
#include "ash/style/rounded_container.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_controller.h"
#include "base/containers/flat_set.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class SoundSectionView;
class TabSliderButton;

// This view will be added on `FocusModeDetailedView` below the task container
// row to show playlists of YouTube music. Clicking two tab slider buttons will
// display two different types of music. Each playlist view will show a
// thumbnail of the playlist cover, a title of the playlist and some media
// control icons.
class ASH_EXPORT FocusModeSoundsView
    : public RoundedContainer,
      public FocusModeSoundsController::Observer {
  METADATA_HEADER(FocusModeSoundsView, RoundedContainer)

 public:
  FocusModeSoundsView(
      const base::flat_set<focus_mode_util::SoundType>& sound_sections,
      bool is_network_connected);
  FocusModeSoundsView(const FocusModeSoundsView&) = delete;
  FocusModeSoundsView& operator=(const FocusModeSoundsView&) = delete;
  ~FocusModeSoundsView() override;

  // views::View:
  void Layout(PassKey) override;

  // FocusModeSoundsController::Observer:
  void OnSelectedPlaylistChanged() override;
  void OnPlaylistStateChanged() override;
  void OnPlayerError() override;

  std::pair<TabSliderButton*, SoundSectionView*> soundscape_views() const {
    return {soundscape_button_, soundscape_container_};
  }

  std::pair<TabSliderButton*, SoundSectionView*> youtube_music_views() const {
    return {youtube_music_button_, youtube_music_container_};
  }

 private:
  struct ToastData {
    ToastData();
    ToastData(const ToastData&);
    ~ToastData();

    constexpr std::partial_ordering operator<=>(const ToastData& other) const;

    focus_mode_util::SoundType source;
    absl::variant<int, std::u16string> message;
    ErrorMessageToast::ButtonActionType action_type;
    bool fatal;
  };

  // Updates this view based on `is_soundscape_type`.
  void UpdateSoundsView(
      bool is_soundscape_type,
      const std::vector<std::unique_ptr<FocusModeSoundsController::Playlist>>&
          playlists);

  // Updates the playback state for all of the playlists under
  // `soundscape_container_` and `youtube_music_container_`.
  void UpdateStateForSelectedPlaylist(
      const focus_mode_util::SelectedPlaylist& selected_playlist);

  // Creates `soundscape_button_` and `youtube_music_button_` if the YouTube
  // Music playlists exist; otherwise, creates a lebel as the header.
  void CreateHeader(const base::flat_set<focus_mode_util::SoundType>& sections,
                    bool is_network_connected);

  // Creates `soundscape_container_` and `youtube_music_container_` and may
  // download playlists.
  void CreatesSoundSectionViews(
      const base::flat_set<focus_mode_util::SoundType>& sections);

  // Called when the Get started button in the YouTube Music OAuth consent view
  // was pressed.
  void OnOAuthGetStartedButtonPressed();

  // Toggles YouTube Music alternate view. It's used to update the UIs for
  // non-premium account.
  void ToggleYouTubeMusicAlternateView(bool show);

  // Handles an error from the YouTube Music backend.
  void YouTubeMusicError(const FocusModeApiError& api_error);

  // Called to show YouTube Music soundscape playlists.
  void OnSoundscapeButtonToggled();

  // Called to show personalized YouTube Music playlists.
  void OnYouTubeMusicButtonToggled();

  // If `show` is true, `soundscape_container_` will be visible instead of
  // `youtube_music_container_`; also, we will update the a11y state for two
  // `TabSliderButton` based on which container is visible if they exists.
  void MayShowSoundscapeContainer(bool show);
  // Loads the playlists based on `is_soundscape_type` into `SoundSectionView`.
  void DownloadPlaylistsForType(bool is_soundscape_type);

  void MaybeDismissErrorMessage();

  // Handles errors for display represented by `data`. In order to handle a
  // series of errors, we guarantee that the most severe error is shown. As a
  // consequence, less severe errors may not be displayed.
  void ProcessError(const ToastData& data);

  void ShowErrorMessageForType(bool is_soundscape_type,
                               const std::u16string& message,
                               ErrorMessageToast::ButtonActionType type);

  std::optional<ToastData> youtube_music_api_error_;

  // The slider buttons on the sound view.
  raw_ptr<TabSliderButton> soundscape_button_ = nullptr;
  raw_ptr<TabSliderButton> youtube_music_button_ = nullptr;

  // Container views for the Soundscape type or the YouTube Music type.
  raw_ptr<SoundSectionView> soundscape_container_ = nullptr;
  raw_ptr<SoundSectionView> youtube_music_container_ = nullptr;

  // An error toast will be shown at the bottom of this view if there is an
  // error when loading playlists or when playing audio.
  raw_ptr<ErrorMessageToast> error_message_ = nullptr;

  base::WeakPtrFactory<FocusModeSoundsView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_VIEW_H_

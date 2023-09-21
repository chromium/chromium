// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_TAB_HELPER_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_TAB_HELPER_H_

#include "base/time/time.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

class HostContentSettingsMap;

// The AutoPictureInPictureTabHelper is a TabHelper attached to each WebContents
// that facilitates automatically opening and closing picture-in-picture windows
// as the given WebContents becomes hidden or visible. WebContents are only
// eligible for auto picture-in-picture if ALL of the following are true:
//   - The website has registered a MediaSession action handler for the
//     'enterpictureinpicture' action.
//   - The 'Auto Picture-in-Picture' content setting is allowed for the website.
//   - The website is capturing camera or microphone.
class AutoPictureInPictureTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<AutoPictureInPictureTabHelper>,
      public TabStripModelObserver,
      public media_session::mojom::AudioFocusObserver,
      public media_session::mojom::MediaSessionObserver {
 public:
  ~AutoPictureInPictureTabHelper() override;
  AutoPictureInPictureTabHelper(const AutoPictureInPictureTabHelper&) = delete;
  AutoPictureInPictureTabHelper& operator=(
      const AutoPictureInPictureTabHelper&) = delete;

  // True if the current page has registered for auto picture-in-picture since
  // last navigation. Remains true even if the page unregisters for auto
  // picture-in-picture. It only resets on navigation.
  bool HasAutoPictureInPictureBeenRegistered() const;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void MediaPictureInPictureChanged(bool is_in_picture_in_picture) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // media_session::mojom::AudioFocusObserver:
  void OnFocusGained(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnFocusLost(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnRequestIdReleased(const base::UnguessableToken& request_id) override {}

  // media_session::mojom::MediaSessionObserver:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr session_info) override;
  void MediaSessionMetadataChanged(
      const absl::optional<media_session::MediaMetadata>& metadata) override {}
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& action)
      override;
  void MediaSessionImagesChanged(
      const base::flat_map<media_session::mojom::MediaSessionImageType,
                           std::vector<media_session::MediaImage>>& images)
      override {}
  void MediaSessionPositionChanged(
      const absl::optional<media_session::MediaPosition>& position) override {}

  bool IsInAutoPictureInPicture() const;

 private:
  explicit AutoPictureInPictureTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<AutoPictureInPictureTabHelper>;
  FRIEND_TEST_ALL_PREFIXES(AutoPictureInPictureTabHelperBrowserTest,
                           CannotAutopipViaHttp);

  void MaybeEnterAutoPictureInPicture();

  void MaybeExitAutoPictureInPicture();

  bool IsEligibleForAutoPictureInPicture() const;

  void UpdateIsTabActivated();

  TabStripModel* GetCurrentTabStripModel() const;

  // Returns true if the tab is currently playing unmuted playback.
  bool HasSufficientPlayback() const;

  // Returns true if the tab is currently using the camera or microphone.
  bool IsUsingCameraOrMicrophone() const;

  // Returns the current state of the 'Auto Picture-in-Picture' content
  // setting for the current website of the observed WebContents.
  ContentSetting GetCurrentContentSetting() const;

  // HostContentSettingsMap is tied to the Profile which outlives the
  // WebContents (which we're tied to), so this is safe.
  const raw_ptr<HostContentSettingsMap> host_content_settings_map_;

  // Tracks when browser tab strips change so we can tell when the observed
  // WebContents changes between being the active tab and not being the active
  // tab.
  //
  // TODO(https://crbug.com/1465988): Directly observe the TabStripModel that
  // contains the observed WebContents.
  BrowserTabStripTracker browser_tab_strip_tracker_{this, nullptr};

  // True if the tab is the activated tab on its tab strip.
  bool is_tab_activated_ = false;

  // True if the media session associated with the observed WebContents has
  // gained audio focus.
  bool has_audio_focus_ = false;

  // True if the media session associated with the observed WebContents is
  // currently playing.
  bool is_playing_ = false;

  // True if the observed WebContents is currently in picture-in-picture.
  bool is_in_picture_in_picture_ = false;

  // True if the observed WebContents is currently in picture-in-picture due
  // to autopip.
  bool is_in_auto_picture_in_picture_ = false;

  // This is used to determine whether the website has used an
  // EnterAutoPictureInPicture action handler to open a picture-in-picture
  // window. When we send the message, we set this time to be the length of a
  // user activation, and if the WebContents enters picture-in-picture before
  // that time, then we will assume we have entered auto-picture-in-picture
  // (and are therefore eligible to exit auto-picture-in-picture when the tab
  // becomes visible again).
  base::TimeTicks auto_picture_in_picture_activation_time_;

  // True if the 'EnterAutoPictureInPicture' action is available on the media
  // session.
  bool is_enter_auto_picture_in_picture_available_ = false;

  // True if the current page has registered for auto picture-in-picture since
  // last navigation. Remains true even if the page unregisters for auto
  // picture-in-picture. It only resets on navigation.
  bool has_ever_registered_for_auto_picture_in_picture_ = false;

  // Connections with the media session service to listen for audio focus
  // updates and control media sessions.
  mojo::Receiver<media_session::mojom::AudioFocusObserver>
      audio_focus_observer_receiver_{this};
  mojo::Receiver<media_session::mojom::MediaSessionObserver>
      media_session_observer_receiver_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_TAB_HELPER_H_

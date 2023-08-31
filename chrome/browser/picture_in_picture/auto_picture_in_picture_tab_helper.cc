// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"

namespace {

// The length of time after sending an EnterAutoPictureInPicture action that
// we'll assume any new picture-in-picture windows will be from that action.
constexpr base::TimeDelta kAutoPictureInPictureActivationThreshold =
    base::Seconds(5);

}  // namespace

AutoPictureInPictureTabHelper::AutoPictureInPictureTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<AutoPictureInPictureTabHelper>(
          *web_contents),
      host_content_settings_map_(HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
  // TODO(https://crbug.com/1465988): Instead of observing all tabstrips at all
  // times, only observe |web_contents()|'s current tabstrip and only while
  // kEnterAutoPictureInPicture is available.
  browser_tab_strip_tracker_.Init();
  UpdateIsTabActivated();

  // Connect to receive audio focus events.
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote;
  content::GetMediaSessionService().BindAudioFocusManager(
      audio_focus_remote.BindNewPipeAndPassReceiver());
  audio_focus_remote->AddObserver(
      audio_focus_observer_receiver_.BindNewPipeAndPassRemote());

  // Connect to receive media session updates.
  content::MediaSession::Get(web_contents)
      ->AddObserver(
          media_session_observer_receiver_.BindNewPipeAndPassRemote());
}

AutoPictureInPictureTabHelper::~AutoPictureInPictureTabHelper() = default;

bool AutoPictureInPictureTabHelper::HasAutoPictureInPictureBeenRegistered()
    const {
  return has_ever_registered_for_auto_picture_in_picture_;
}

void AutoPictureInPictureTabHelper::PrimaryPageChanged(content::Page& page) {
  has_ever_registered_for_auto_picture_in_picture_ = false;
}

void AutoPictureInPictureTabHelper::MediaPictureInPictureChanged(
    bool is_in_picture_in_picture) {
  if (is_in_picture_in_picture_ == is_in_picture_in_picture) {
    return;
  }
  is_in_picture_in_picture_ = is_in_picture_in_picture;

  if (!is_in_picture_in_picture_) {
    is_in_auto_picture_in_picture_ = false;
    return;
  }

  if (base::TimeTicks::Now() < auto_picture_in_picture_activation_time_) {
    is_in_auto_picture_in_picture_ = true;
    auto_picture_in_picture_activation_time_ = base::TimeTicks();

    // If the tab is activated by the time auto picture-in-picture fires, we
    // should immediately close the auto picture-in-picture.
    if (is_tab_activated_) {
      MaybeExitAutoPictureInPicture();
    }
  }
}

void AutoPictureInPictureTabHelper::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  const bool old_is_tab_activated = is_tab_activated_;
  UpdateIsTabActivated();
  if (is_tab_activated_ == old_is_tab_activated) {
    return;
  }

  if (is_tab_activated_) {
    MaybeExitAutoPictureInPicture();
  } else {
    MaybeEnterAutoPictureInPicture();
  }
}

void AutoPictureInPictureTabHelper::OnFocusGained(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  if (has_audio_focus_) {
    return;
  }
  auto request_id =
      content::MediaSession::GetRequestIdFromWebContents(web_contents());
  if (request_id.is_empty()) {
    return;
  }
  has_audio_focus_ = (request_id == session->request_id);
}

void AutoPictureInPictureTabHelper::OnFocusLost(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  if (!has_audio_focus_) {
    return;
  }
  auto request_id =
      content::MediaSession::GetRequestIdFromWebContents(web_contents());
  if (request_id.is_empty()) {
    // I don't think this can happen, but if we reach here without a request ID,
    // we can safely assume we no longer have focus.
    has_audio_focus_ = false;
    return;
  }
  has_audio_focus_ = (request_id != session->request_id);
}

void AutoPictureInPictureTabHelper::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  is_playing_ =
      session_info && session_info->playback_state ==
                          media_session::mojom::MediaPlaybackState::kPlaying;
}

void AutoPictureInPictureTabHelper::MediaSessionActionsChanged(
    const std::vector<media_session::mojom::MediaSessionAction>& actions) {
  is_enter_auto_picture_in_picture_available_ =
      base::ranges::find(actions,
                         media_session::mojom::MediaSessionAction::
                             kEnterAutoPictureInPicture) != actions.end();

  if (is_enter_auto_picture_in_picture_available_) {
    has_ever_registered_for_auto_picture_in_picture_ = true;
  }
}

void AutoPictureInPictureTabHelper::MaybeEnterAutoPictureInPicture() {
  if (!IsEligibleForAutoPictureInPicture()) {
    return;
  }
  auto_picture_in_picture_activation_time_ =
      base::TimeTicks::Now() + kAutoPictureInPictureActivationThreshold;
  content::MediaSession::Get(web_contents())->EnterAutoPictureInPicture();
}

void AutoPictureInPictureTabHelper::MaybeExitAutoPictureInPicture() {
  if (!is_in_auto_picture_in_picture_) {
    return;
  }
  is_in_auto_picture_in_picture_ = false;

  PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
}

bool AutoPictureInPictureTabHelper::IsEligibleForAutoPictureInPicture() const {
  // The tab must either have playback or be using camera/microphone to autopip.
  if (!HasSufficientPlayback() && !IsUsingCameraOrMicrophone()) {
    return false;
  }

  // The user may block autopip via a content setting.
  if (GetCurrentContentSetting() == CONTENT_SETTING_BLOCK) {
    return false;
  }

  // The website must have registered for autopip.
  if (!is_enter_auto_picture_in_picture_available_) {
    return false;
  }

  // Do not autopip if the tab is already in PiP.
  if (is_in_picture_in_picture_) {
    return false;
  }

  return true;
}

void AutoPictureInPictureTabHelper::UpdateIsTabActivated() {
  auto* tab_strip = GetCurrentTabStripModel();
  if (tab_strip) {
    is_tab_activated_ = tab_strip->GetActiveWebContents() == web_contents();
  }
}

TabStripModel* AutoPictureInPictureTabHelper::GetCurrentTabStripModel() const {
  // If this WebContents isn't in a normal browser window, then auto
  // picture-in-picture is not supported.
  auto* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser || !browser->is_type_normal()) {
    return nullptr;
  }
  return browser->tab_strip_model();
}

bool AutoPictureInPictureTabHelper::HasSufficientPlayback() const {
  // TODO(https://crbug.com/1464251): Make sure that there is a video that is
  // large enough and visible.
  return has_audio_focus_ && is_playing_;
}

bool AutoPictureInPictureTabHelper::IsUsingCameraOrMicrophone() const {
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->GetMediaStreamCaptureIndicator()
      ->IsCapturingUserMedia(web_contents());
}

ContentSetting AutoPictureInPictureTabHelper::GetCurrentContentSetting() const {
  GURL url = web_contents()->GetLastCommittedURL();
  return host_content_settings_map_->GetContentSetting(
      url, url, ContentSettingsType::AUTO_PICTURE_IN_PICTURE);
}

bool AutoPictureInPictureTabHelper::IsInAutoPictureInPicture() const {
  return is_in_auto_picture_in_picture_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AutoPictureInPictureTabHelper);

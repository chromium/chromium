// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_helper.h"

#include "base/feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_strip_observer_helper.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/frame/user_activation_state.h"

AutoPictureInPictureTabHelper::AutoPictureInPictureTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<AutoPictureInPictureTabHelper>(
          *web_contents),
      host_content_settings_map_(HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))),
      auto_blocker_(PermissionDecisionAutoBlockerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
  // `base::Unretained` is safe here since we own `tab_strip_observer_helper_`.
  tab_strip_observer_helper_ =
      std::make_unique<AutoPictureInPictureTabStripObserverHelper>(
          web_contents,
          base::BindRepeating(
              &AutoPictureInPictureTabHelper::OnTabActivatedChanged,
              base::Unretained(this)));

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
    MaybeStartOrStopObservingTabStrip();
    return;
  }

  if (AreAutoPictureInPicturePreconditionsMet()) {
    is_in_auto_picture_in_picture_ = true;
    auto_picture_in_picture_activation_time_ = base::TimeTicks();

    // If the tab is activated by the time auto picture-in-picture fires, we
    // should immediately close the auto picture-in-picture.
    if (is_tab_activated_) {
      MaybeExitAutoPictureInPicture();
    }
  }
}

void AutoPictureInPictureTabHelper::OnTabActivatedChanged(
    bool is_tab_activated) {
  is_tab_activated_ = is_tab_activated;
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
  MaybeStartOrStopObservingTabStrip();
}

void AutoPictureInPictureTabHelper::MaybeEnterAutoPictureInPicture() {
  if (!IsEligibleForAutoPictureInPicture()) {
    return;
  }
  auto_picture_in_picture_activation_time_ =
      base::TimeTicks::Now() + blink::kActivationLifespan;
  content::MediaSession::Get(web_contents())->EnterAutoPictureInPicture();
}

void AutoPictureInPictureTabHelper::MaybeExitAutoPictureInPicture() {
  if (!is_in_auto_picture_in_picture_) {
    return;
  }
  is_in_auto_picture_in_picture_ = false;

  PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
}

void AutoPictureInPictureTabHelper::MaybeStartOrStopObservingTabStrip() {
  if (is_enter_auto_picture_in_picture_available_ ||
      is_in_auto_picture_in_picture_) {
    tab_strip_observer_helper_->StartObserving();
  } else {
    tab_strip_observer_helper_->StopObserving();
  }
}

bool AutoPictureInPictureTabHelper::IsEligibleForAutoPictureInPicture() const {
  // The tab must either have playback or be using camera/microphone to autopip.
  if (!HasSufficientPlayback() && !IsUsingCameraOrMicrophone()) {
    return false;
  }

  // The user may block autopip via a content setting. Also, if we're in an
  // incognito window, then we should treat "ask" as "block".
  ContentSetting setting = GetCurrentContentSetting();
  if (setting == CONTENT_SETTING_BLOCK ||
      (setting == CONTENT_SETTING_ASK &&
       Profile::FromBrowserContext(web_contents()->GetBrowserContext())
           ->IsIncognitoProfile())) {
    return false;
  }

  // Only https:// or file:// may autopip.
  const GURL url = web_contents()->GetLastCommittedURL();
  if (!url.SchemeIs(url::kHttpsScheme) && !url.SchemeIsFile()) {
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

bool AutoPictureInPictureTabHelper::HasSufficientPlayback() const {
  if (!base::FeatureList::IsEnabled(
          media::kAutoPictureInPictureForVideoPlayback)) {
    return false;
  }

  // TODO(https://crbug.com/1464351): Make sure that there is a video that is
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
  auto setting = host_content_settings_map_->GetContentSetting(
      url, url, ContentSettingsType::AUTO_PICTURE_IN_PICTURE);
  if (setting == CONTENT_SETTING_ASK && auto_blocker_ &&
      auto_blocker_->IsEmbargoed(
          url, ContentSettingsType::AUTO_PICTURE_IN_PICTURE)) {
    return CONTENT_SETTING_BLOCK;
  }
  return setting;
}

bool AutoPictureInPictureTabHelper::IsInAutoPictureInPicture() const {
  return is_in_auto_picture_in_picture_;
}

bool AutoPictureInPictureTabHelper::AreAutoPictureInPicturePreconditionsMet()
    const {
  // Note that `auto_picture_in_picture_activation_time_` is not set if all of
  // the other preconditions are not set.
  return base::TimeTicks::Now() < auto_picture_in_picture_activation_time_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AutoPictureInPictureTabHelper);

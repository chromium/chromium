// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/media_state_observer.h"

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "media/base/media_switches.h"

namespace {
// Values defined in Tab.java and must be kept in sync.
enum MediaState {
  NONE = 0,
  MUTED = 1,
  AUDIBLE = 2,
  RECORDING = 3,
  SHARING = 4,
};
}  // namespace

// TODO(crbug.com/454045510): Add tests.
MediaStateObserver::MediaStateObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<MediaStateObserver>(*web_contents),
      recently_audible_subscription_(MaybeSubscribeToRecentlyAudible()) {
  media_stream_capture_indicator_observation_.Observe(
      MediaCaptureDevicesDispatcher::GetInstance()
          ->GetMediaStreamCaptureIndicator()
          .get());
}

MediaStateObserver::~MediaStateObserver() = default;

void MediaStateObserver::DidUpdateAudioMutingState(bool muted) {
  if (is_audio_muted_ == muted) {
    return;
  }
  is_audio_muted_ = muted;
  UpdateMediaState();
}

void MediaStateObserver::OnAudioStateChanged(bool audible) {
  if (recently_audible_subscription_) {
    return;
  }
  UpdateAudibleState(audible);
}

void MediaStateObserver::OnIsCapturingVideoChanged(
    content::WebContents* web_contents,
    bool is_capturing_video) {
  if (web_contents != WebContentsObserver::web_contents() ||
      is_capturing_video_ == is_capturing_video) {
    return;
  }
  is_capturing_video_ = is_capturing_video;
  UpdateMediaState();
}

void MediaStateObserver::OnIsCapturingAudioChanged(
    content::WebContents* web_contents,
    bool is_capturing_audio) {
  if (web_contents != WebContentsObserver::web_contents() ||
      is_capturing_audio_ == is_capturing_audio) {
    return;
  }
  is_capturing_audio_ = is_capturing_audio;
  UpdateMediaState();
}

void MediaStateObserver::OnIsBeingMirroredChanged(
    content::WebContents* web_contents,
    bool is_being_mirrored) {
  if (web_contents != WebContentsObserver::web_contents() ||
      is_being_mirrored_ == is_being_mirrored) {
    return;
  }

  is_being_mirrored_ = is_being_mirrored;
  UpdateMediaState();
}

base::CallbackListSubscription
MediaStateObserver::MaybeSubscribeToRecentlyAudible() {
  if (base::FeatureList::IsEnabled(media::kEnableAudioMonitoringOnAndroid)) {
    return RecentlyAudibleHelper::FromWebContents(web_contents())
        ->RegisterRecentlyAudibleChangedCallback(base::BindRepeating(
            &MediaStateObserver::OnRecentlyAudibleStateChanged,
            base::Unretained(this)));
  }
  return base::CallbackListSubscription();
}

void MediaStateObserver::OnRecentlyAudibleStateChanged(bool was_audible) {
  UpdateAudibleState(was_audible);
}

void MediaStateObserver::UpdateAudibleState(bool audible) {
  if (is_audible_ == audible) {
    return;
  }
  is_audible_ = audible;
  UpdateMediaState();
}

void MediaStateObserver::UpdateMediaState() {
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents());
  if (!tab) {
    return;
  }

  if (is_being_mirrored_) {
    tab->SetMediaState(MediaState::SHARING);
  } else if (is_capturing_video_ || is_capturing_audio_) {
    tab->SetMediaState(MediaState::RECORDING);
  } else if (is_audible_) {
    tab->SetMediaState(is_audio_muted_ ? MediaState::MUTED
                                       : MediaState::AUDIBLE);
  } else {
    tab->SetMediaState(MediaState::NONE);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MediaStateObserver);

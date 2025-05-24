// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/audio_ducker.h"

#include "build/build_config.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"

#if BUILDFLAG(IS_WIN)
#include "content/public/browser/audio_service_info.h"
#include "media/audio/win/audio_ducker_win.h"
#endif  // BUILDFLAG(IS_WIN)

AudioDucker::AudioDucker(content::Page& page)
    : content::PageUserData<AudioDucker>(page),
      content::WebContentsObserver(GetWebContents()) {
#if BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(media::kAudioDuckingWin)) {
    // `base::Unretained()` is safe here since we own `windows_ducker_`.
    windows_ducker_ =
        std::make_unique<media::AudioDuckerWin>(base::BindRepeating(
            &AudioDucker::ShouldDuckProcess, base::Unretained(this)));
  }
#endif  // BUILDFLAG(IS_WIN)
}

AudioDucker::~AudioDucker() {
  StopDuckingOtherAudio();
}

bool AudioDucker::StartDuckingOtherAudio() {
  if (!base::FeatureList::IsEnabled(media::kAudioDucking)) {
    return false;
  }
  if (ducking_state_ == AudioDuckingState::kDucking) {
    return true;
  }
  if (!BindToAudioFocusManagerIfNecessary()) {
    return false;
  }
  StartDuckingImpl();
  ducking_state_ = AudioDuckingState::kDucking;
  return true;
}

bool AudioDucker::StopDuckingOtherAudio() {
  if (!base::FeatureList::IsEnabled(media::kAudioDucking)) {
    return false;
  }
  if (ducking_state_ == AudioDuckingState::kNoDucking) {
    return true;
  }
  if (!BindToAudioFocusManagerIfNecessary()) {
    return false;
  }
  audio_focus_remote_->StopDuckingAllAudio();

#if BUILDFLAG(IS_WIN)
  if (windows_ducker_) {
    windows_ducker_->StopDuckingOtherWindowsApplications();
  }
#endif  // BUILDFLAG(IS_WIN)

  ducking_state_ = AudioDuckingState::kNoDucking;
  return true;
}

void AudioDucker::MediaSessionCreated(content::MediaSession* session) {
  // When a MediaSession is created and we're already ducking, we need to tell
  // the AudioFocusManager to start ducking again while exempting the new
  // request ID. This will supercede the previous request and replace it with a
  // request that has an exempted MediaSession.
  if (ducking_state_ == AudioDuckingState::kDucking &&
      BindToAudioFocusManagerIfNecessary()) {
    StartDuckingImpl();
  }
}

void AudioDucker::StartDuckingImpl() {
  // Null base::UnguessableTokens cannot be passed via mojo, so convert to an
  // empty optional if the request ID is null.
  const base::UnguessableToken& request_id =
      content::MediaSession::GetRequestIdFromWebContents(GetWebContents());
  std::optional<base::UnguessableToken> optional_request_id;
  if (!request_id.is_empty()) {
    optional_request_id = request_id;
  }
  audio_focus_remote_->StartDuckingAllAudio(optional_request_id);

#if BUILDFLAG(IS_WIN)
  if (windows_ducker_) {
    windows_ducker_->StartDuckingOtherWindowsApplications();
  }
#endif  // BUILDFLAG(IS_WIN)
}

content::WebContents* AudioDucker::GetWebContents() const {
  return content::WebContents::FromRenderFrameHost(&page().GetMainDocument());
}

bool AudioDucker::BindToAudioFocusManagerIfNecessary() {
  if (audio_focus_remote_.is_bound()) {
    return true;
  }
  content::GetMediaSessionService().BindAudioFocusManager(
      audio_focus_remote_.BindNewPipeAndPassReceiver());
  audio_focus_remote_.reset_on_disconnect();
  return audio_focus_remote_.is_bound();
}

#if BUILDFLAG(IS_WIN)
bool AudioDucker::ShouldDuckProcess(base::ProcessId process_id) const {
  // If this audio session is associated with Chrome (either the browser
  // process or the audio process), then we don't want to duck it.
  if (process_id == base::GetCurrentProcId()) {
    return false;
  }

  base::ProcessId audio_service_process_id =
      content::GetProcessIdForAudioService();
  if (audio_service_process_id != base::kNullProcessId &&
      process_id == audio_service_process_id) {
    return false;
  }

  return true;
}
#endif  // BUILDFLAG(IS_WIN)

PAGE_USER_DATA_KEY_IMPL(AudioDucker);

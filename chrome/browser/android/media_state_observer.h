// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_MEDIA_STATE_OBSERVER_H_
#define CHROME_BROWSER_ANDROID_MEDIA_STATE_OBSERVER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

// Observes media-related state changes for a tab's webcontents and updates the
// TabAndroid media indicator.
class MediaStateObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<MediaStateObserver>,
      public MediaStreamCaptureIndicator::Observer {
 public:
  explicit MediaStateObserver(content::WebContents* web_contents);
  ~MediaStateObserver() override;

  MediaStateObserver(const MediaStateObserver&) = delete;
  MediaStateObserver& operator=(const MediaStateObserver&) = delete;

  // content::WebContentsObserver overrides:
  // Called when the audio muting state of the WebContents has changed.
  void DidUpdateAudioMutingState(bool muted) override;
  // Called when the audible state changes. This is used when
  // kEnableAudioMonitoringOnAndroid is disabled to avoid the 2-second audible
  // polling delay.
  void OnAudioStateChanged(bool audible) override;

  // MediaStreamCaptureIndicator::Observer overrides:
  // Called when the video capture state of the WebContents has changed.
  void OnIsCapturingVideoChanged(content::WebContents* web_contents,
                                 bool is_capturing_video) override;
  // Called when the audio capture state of the WebContents has changed.
  void OnIsCapturingAudioChanged(content::WebContents* web_contents,
                                 bool is_capturing_audio) override;
  // Called when the mirroring state of the WebContents has changed.
  void OnIsBeingMirroredChanged(content::WebContents* web_contents,
                                bool is_being_mirrored) override;

 private:
  friend class content::WebContentsUserData<MediaStateObserver>;

  // Subscribes to notifications about changes in the "recently audible" state.
  base::CallbackListSubscription MaybeSubscribeToRecentlyAudible();

  // Handles debounced audible state changes from RecentlyAudibleHelper. This is
  // used when kEnableAudioMonitoringOnAndroid is enabled to prevent UI
  // flickering from rapid state changes.
  void OnRecentlyAudibleStateChanged(bool was_audible);
  // Updates the audible state.
  void UpdateAudibleState(bool is_audible);
  // Updates the overall media state and updates the tab android.
  void UpdateMediaState();

  // Keep track of the media state.
  bool is_being_mirrored_ = false;
  bool is_capturing_video_ = false;
  bool is_capturing_audio_ = false;
  bool is_audio_muted_ = false;
  bool is_audible_ = false;

  // Subscription to be notified when the recently audible state has changed.
  const base::CallbackListSubscription recently_audible_subscription_;

  // Observes the MediaStreamCaptureIndicator so the media state observer will
  // be notified when a media stream capture has changed.
  base::ScopedObservation<MediaStreamCaptureIndicator,
                          MediaStreamCaptureIndicator::Observer>
      media_stream_capture_indicator_observation_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_ANDROID_MEDIA_STATE_OBSERVER_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_MEDIA_STATE_OBSERVER_H_
#define CHROME_BROWSER_ANDROID_MEDIA_STATE_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

// Observes media-related state changes for a tab and notifies TabAndroid.
class MediaStateObserver
    : public content::WebContentsObserver,
      public MediaStreamCaptureIndicator::Observer,
      public content::WebContentsUserData<MediaStateObserver> {
 public:
  explicit MediaStateObserver(content::WebContents* web_contents);
  ~MediaStateObserver() override;

  MediaStateObserver(const MediaStateObserver&) = delete;
  MediaStateObserver& operator=(const MediaStateObserver&) = delete;

  // content::WebContentsObserver overrides:
  void DidUpdateAudioMutingState(bool muted) override;
  void OnAudioStateChanged(bool audible) override;

  // MediaStreamCaptureIndicator::Observer overrides:
  void OnIsCapturingVideoChanged(content::WebContents* web_contents,
                                 bool is_capturing_video) override;
  void OnIsCapturingAudioChanged(content::WebContents* web_contents,
                                 bool is_capturing_audio) override;

 private:
  friend class content::WebContentsUserData<MediaStateObserver>;
  void UpdateMediaState();

  bool is_capturing_video_ = false;
  bool is_capturing_audio_ = false;
  bool is_audio_muted_ = false;
  bool is_audible_ = false;

  base::ScopedObservation<MediaStreamCaptureIndicator,
                          MediaStreamCaptureIndicator::Observer>
      media_stream_capture_indicator_observation_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_ANDROID_MEDIA_STATE_OBSERVER_H_

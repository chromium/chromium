// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_ANDROID_SESSION_VIEW_ANDROID_H_
#define CHROME_BROWSER_TTC_ANDROID_SESSION_VIEW_ANDROID_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "chrome/browser/ttc/core/session_view.h"
#include "chrome/browser/ttc/core/states.h"
#include "components/ttc/app/public/error_codes.h"

namespace ttc {

class SessionViewDelegate;
class TtcKeyedServiceAndroid;

// Android SessionView. The session UI itself lives in Java, so this forwards
// the session's UI events to the profile's TtcKeyedServiceAndroid bridge,
// which relays them to the Java TtcKeyedService observers.
class SessionViewAndroid : public SessionView {
 public:
  explicit SessionViewAndroid(SessionViewDelegate& delegate);
  SessionViewAndroid(const SessionViewAndroid&) = delete;
  SessionViewAndroid& operator=(const SessionViewAndroid&) = delete;
  ~SessionViewAndroid() override;

  // SessionController::Observer:
  void OnSessionLifecycleChanged(SessionLifecycle lifecycle) override;
  void OnError(ErrorCode error) override;
  void OnTranscriptionsUpdated(const std::string& user_transcript,
                               const std::string& model_transcript) override;
  void OnUserAudioLevelUpdate(float audio_level) override;
  void OnMicMutedChanged(bool muted) override;
  void OnSpeakerChanged(Speaker speaker) override;

 private:
  // Owned by the TtcKeyedService, which outlives the session this view
  // belongs to.
  const raw_ref<TtcKeyedServiceAndroid> bridge_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_ANDROID_SESSION_VIEW_ANDROID_H_

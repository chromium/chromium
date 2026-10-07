// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_ANDROID_SESSION_VIEW_ANDROID_H_
#define CHROME_BROWSER_TTC_ANDROID_SESSION_VIEW_ANDROID_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ttc/core/session_view.h"

namespace ttc {

class SessionViewDelegate;
class TtcKeyedServiceAndroid;

// Android SessionView. The session UI itself lives in Java, so this forwards
// every view event to the profile's TtcKeyedServiceAndroid bridge, which
// relays it to the Java TtcKeyedService observers.
class SessionViewAndroid : public SessionView {
 public:
  explicit SessionViewAndroid(SessionViewDelegate& delegate);
  SessionViewAndroid(const SessionViewAndroid&) = delete;
  SessionViewAndroid& operator=(const SessionViewAndroid&) = delete;
  ~SessionViewAndroid() override;

  // SessionView:
  void UpdateAudioLevel(float audio_level) override;
  void OnSessionInitialized() override;
  void OnError(ErrorCode error) override;

 private:
  // Owned by the TtcKeyedService, which outlives the session this view
  // belongs to.
  const raw_ref<TtcKeyedServiceAndroid> bridge_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_ANDROID_SESSION_VIEW_ANDROID_H_

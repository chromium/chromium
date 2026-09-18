// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_SESSION_VIEW_H_
#define CHROME_BROWSER_TTC_CORE_SESSION_VIEW_H_

namespace ttc {

// Interface for the object holding and implementing all UI interaction for a
// session.
class SessionView {
 public:
  virtual ~SessionView() = default;

  // Called when the audio level of the user's microphone input changes.
  virtual void UpdateAudioLevel(float audio_level) = 0;

  // Called when the session has finished initializing and audio capture is
  // live.
  virtual void OnSessionInitialized() = 0;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_SESSION_VIEW_H_

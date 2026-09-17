// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_SESSION_VIEW_IMPL_H_
#define CHROME_BROWSER_TTC_CORE_SESSION_VIEW_IMPL_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "chrome/browser/ttc/core/session_view.h"

namespace dictation {
class DictationBubbleUi;
}  // namespace dictation

namespace ttc {

class SessionViewDelegate;

class SessionViewImpl : public SessionView {
 public:
  explicit SessionViewImpl(SessionViewDelegate& delegate);
  ~SessionViewImpl() override;
  SessionViewImpl(const SessionViewImpl&) = delete;
  SessionViewImpl& operator=(const SessionViewImpl&) = delete;

  // SessionView implementation:
  void UpdateAudioLevel(float audio_level) override;
  void OnSessionInitialized() override;
  void OnSessionEnded() override;

 private:
  void CreateVoicePlateUi();

  // Invoked when the user clicks the voice plate's close button.
  void OnVoicePlateCloseClicked();

  // Safe because the delegate is guaranteed to outlive this object. Assigned on
  // construction.
  const raw_ref<SessionViewDelegate> delegate_;

  // The main UI surface for the session; a bubble anchored to the top-center of
  // the browser window.
  std::unique_ptr<dictation::DictationBubbleUi> voice_plate_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_SESSION_VIEW_IMPL_H_

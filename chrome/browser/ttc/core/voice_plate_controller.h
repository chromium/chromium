// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_VOICE_PLATE_CONTROLLER_H_
#define CHROME_BROWSER_TTC_CORE_VOICE_PLATE_CONTROLLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/views/dictation/ui_state.h"

class BrowserWindowInterface;
class Profile;

namespace dictation {
class DictationBubbleUi;
}  // namespace dictation

namespace ttc {

// Owns the voice plate - the bubble that is the main UI surface of a session -
// and keeps it in the browser window the user is currently working in by
// following window activation changes.
class VoicePlateController : public BrowserCollectionObserver {
 public:
  VoicePlateController(Profile& profile, base::RepeatingClosure close_callback);
  ~VoicePlateController() override;
  VoicePlateController(const VoicePlateController&) = delete;
  VoicePlateController& operator=(const VoicePlateController&) = delete;

  void SetState(dictation::UiState state);
  void UpdateAudioLevel(float audio_level);

  // BrowserCollectionObserver:
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

 private:
  void MoveToWindow(BrowserWindowInterface* browser);

  const base::RepeatingClosure close_callback_;

  // The window `voice_plate_` is currently anchored in. Null iff
  // `voice_plate_` is null.
  raw_ptr<BrowserWindowInterface> anchored_browser_ = nullptr;

  // Null if there was no suitable window to show the plate in.
  std::unique_ptr<dictation::DictationBubbleUi> voice_plate_;

  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_VOICE_PLATE_CONTROLLER_H_

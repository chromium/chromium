// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_SPEAKER_TRACKER_H_
#define CHROME_BROWSER_TTC_APP_SPEAKER_TRACKER_H_

#include <stdint.h>

#include "base/functional/callback.h"
#include "chrome/browser/ttc/core/states.h"
#include "components/ttc/app/public/turn_state_update.h"

namespace ttc {

// Tracks who is currently speaking in the conversation, based on the turn
// state updates reported by the backend.
class SpeakerTracker {
 public:
  // Called whenever the speaker changes.
  using SpeakerChangedCallback =
      base::RepeatingCallback<void(Speaker old_speaker, Speaker new_speaker)>;

  explicit SpeakerTracker(SpeakerChangedCallback speaker_changed_callback);
  ~SpeakerTracker();
  SpeakerTracker(const SpeakerTracker&) = delete;
  SpeakerTracker& operator=(const SpeakerTracker&) = delete;

  void OnModelTurnStateUpdate(ModelTurnStateUpdate update,
                              int64_t generation_id);
  void OnUserTurnStateUpdate(UserTurnStateUpdate update);

  Speaker speaker() const { return speaker_; }

 private:
  void SetSpeaker(Speaker speaker);

  SpeakerChangedCallback speaker_changed_callback_;
  Speaker speaker_ = Speaker::kIdle;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_SPEAKER_TRACKER_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/speaker_tracker.h"

#include <utility>

#include "base/check.h"

namespace ttc {

SpeakerTracker::SpeakerTracker(SpeakerChangedCallback speaker_changed_callback)
    : speaker_changed_callback_(std::move(speaker_changed_callback)) {
  CHECK(speaker_changed_callback_);
}

SpeakerTracker::~SpeakerTracker() = default;

void SpeakerTracker::OnModelTurnStateUpdate(ModelTurnStateUpdate update,
                                            int64_t generation_id) {
  switch (update) {
    case ModelTurnStateUpdate::kTurnStarted:
      SetSpeaker(Speaker::kModel);
      break;
    case ModelTurnStateUpdate::kTurnCompleted:
      // When the model is interrupted by the user, the user's turn starts
      // before the model's turn completes so the user keeps the turn.
      if (speaker_ == Speaker::kModel) {
        SetSpeaker(Speaker::kIdle);
      }
      break;
    case ModelTurnStateUpdate::kInterrupted:
    case ModelTurnStateUpdate::kGenerationCompleted:
      break;
  }
}

void SpeakerTracker::OnUserTurnStateUpdate(UserTurnStateUpdate update) {
  switch (update) {
    case UserTurnStateUpdate::kTurnStarted:
      SetSpeaker(Speaker::kUser);
      break;
    case UserTurnStateUpdate::kTurnCompleted:
      if (speaker_ == Speaker::kUser) {
        SetSpeaker(Speaker::kIdle);
      }
      break;
  }
}

void SpeakerTracker::SetSpeaker(Speaker speaker) {
  if (speaker_ == speaker) {
    return;
  }
  const Speaker old_speaker = speaker_;
  speaker_ = speaker;
  speaker_changed_callback_.Run(old_speaker, speaker_);
}

}  // namespace ttc

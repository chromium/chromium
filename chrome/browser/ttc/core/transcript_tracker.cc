// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/transcript_tracker.h"

namespace ttc {

TranscriptTracker::TranscriptTracker() = default;

TranscriptTracker::~TranscriptTracker() = default;

void TranscriptTracker::AddUserTranscription(const std::string& text) {
  if (last_speaker_ != Speaker::kUser) {
    // A new exchange.
    user_transcript_.clear();
    model_transcript_.clear();
  }
  user_transcript_ += text;
  last_speaker_ = Speaker::kUser;
}

void TranscriptTracker::AddModelTranscription(const std::string& text) {
  model_transcript_ += text;
  last_speaker_ = Speaker::kModel;
}

void TranscriptTracker::OnModelTurnComplete() {
  last_speaker_ = Speaker::kNone;
}

}  // namespace ttc

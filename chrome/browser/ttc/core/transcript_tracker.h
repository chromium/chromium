// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_TRANSCRIPT_TRACKER_H_
#define CHROME_BROWSER_TTC_CORE_TRANSCRIPT_TRACKER_H_

#include <string>

namespace ttc {

// Keeps the transcripts of a session's current exchange, i.e. of the user's
// turn and the model's reply to it, for captions.
//
// The backend streams each turn's transcription in increments, which are
// accumulated here. Rather than inferring turns, an exchange ends when the user
// is transcribed after the model (e.g. barging in), or after the model's turn
// completed (e.g. after a silent tool call). The model's transcription only
// adds to the current exchange, since it can arrive after its audio or its turn
// has completed; so a second reply without new speech from the user adds to
// the first.
class TranscriptTracker {
 public:
  TranscriptTracker();
  TranscriptTracker(const TranscriptTracker&) = delete;
  TranscriptTracker& operator=(const TranscriptTracker&) = delete;
  ~TranscriptTracker();

  // Adds the next increment of the user's or the model's transcription.
  void AddUserTranscription(const std::string& text);
  void AddModelTranscription(const std::string& text);

  // Called when the model's turn completes.
  void OnModelTurnComplete();

  // The full text so far of the user's turn and the model's reply in the
  // current exchange. Either may be empty.
  const std::string& user_transcript() const { return user_transcript_; }
  const std::string& model_transcript() const { return model_transcript_; }

 private:
  // Who was last transcribed in the current exchange.
  enum class Speaker {
    // Nobody yet, or the model's turn has completed since.
    kNone,
    kUser,
    kModel,
  };

  Speaker last_speaker_ = Speaker::kNone;
  std::string user_transcript_;
  std::string model_transcript_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_TRANSCRIPT_TRACKER_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/transcript_tracker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

TEST(TranscriptTrackerTest, TranscriptsAccumulateWithinExchange) {
  TranscriptTracker tracker;
  tracker.AddUserTranscription("What's the ");
  tracker.AddUserTranscription("weather?");
  tracker.AddModelTranscription("It's ");
  tracker.AddModelTranscription("sunny.");
  EXPECT_EQ(tracker.user_transcript(), "What's the weather?");
  EXPECT_EQ(tracker.model_transcript(), "It's sunny.");

  // The user speaking again starts a new exchange.
  tracker.OnModelTurnComplete();
  tracker.AddUserTranscription("Thanks");
  EXPECT_EQ(tracker.user_transcript(), "Thanks");
  EXPECT_EQ(tracker.model_transcript(), "");
}

TEST(TranscriptTrackerTest, BargeInStartsNewExchange) {
  TranscriptTracker tracker;
  tracker.AddUserTranscription("Tell me a story");
  tracker.AddModelTranscription("Once upon");

  // However the interruption is ordered with it, the user's speech after the
  // model's starts a new exchange.
  tracker.AddUserTranscription("Stop ");
  tracker.AddUserTranscription("please");
  EXPECT_EQ(tracker.user_transcript(), "Stop please");
  EXPECT_EQ(tracker.model_transcript(), "");
}

TEST(TranscriptTrackerTest, SilentTurnThenNewSpeechResets) {
  TranscriptTracker tracker;
  tracker.AddUserTranscription("Scroll down");
  tracker.OnModelTurnComplete();
  tracker.AddUserTranscription("Scroll up");

  EXPECT_EQ(tracker.user_transcript(), "Scroll up");
  EXPECT_EQ(tracker.model_transcript(), "");
}

TEST(TranscriptTrackerTest, TrailingModelTranscriptStaysInExchange) {
  TranscriptTracker tracker;
  tracker.AddUserTranscription("Hello");
  tracker.AddModelTranscription("Hi ");
  tracker.OnModelTurnComplete();

  // The rest of the reply's transcription arrives after its turn completed.
  tracker.AddModelTranscription("there.");
  EXPECT_EQ(tracker.user_transcript(), "Hello");
  EXPECT_EQ(tracker.model_transcript(), "Hi there.");
}

}  // namespace ttc

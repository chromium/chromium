// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/android/read_aloud_bridge.h"

#include "chrome/browser/readaloud/read_aloud_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/jni_zero/jni_zero.h"

namespace readaloud {

namespace {

// Matches Java PlaybackArgs.PlaybackMode values.
constexpr int kJavaPlaybackModeUnspecified = 0;
constexpr int kJavaPlaybackModeClassic = 1;
constexpr int kJavaPlaybackModeOverview = 2;
constexpr int kInvalidPlaybackMode = 99;

// Matches Java Feedback.FeedbackType values.
constexpr int kJavaFeedbackTypeNone = 0;
constexpr int kJavaFeedbackTypePositive = 1;
constexpr int kJavaFeedbackTypeNegative = 2;
constexpr int kInvalidFeedbackType = 99;

}  // namespace

class ReadAloudBridgeTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  ReadAloudService service_{&profile_};
};

TEST_F(ReadAloudBridgeTest, SetPlaybackMode) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  ReadAloudBridge bridge(env, nullptr, &service_);

  bridge.SetPlaybackMode(env, kJavaPlaybackModeOverview);
  EXPECT_EQ(service_.playback_mode(),
            ReadAloudService::PlaybackMode::kOverview);

  bridge.SetPlaybackMode(env, kJavaPlaybackModeClassic);
  EXPECT_EQ(service_.playback_mode(),
            ReadAloudService::PlaybackMode::kClassic);

  bridge.SetPlaybackMode(env, kJavaPlaybackModeUnspecified);
  EXPECT_EQ(service_.playback_mode(),
            ReadAloudService::PlaybackMode::kUnspecified);

  EXPECT_DEATH_IF_SUPPORTED(bridge.SetPlaybackMode(env, kInvalidPlaybackMode),
                            "");
}

TEST_F(ReadAloudBridgeTest, SendFeedback) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  ReadAloudBridge bridge(env, nullptr, &service_);

  bridge.SendFeedback(env, kJavaFeedbackTypeNone);
  bridge.SendFeedback(env, kJavaFeedbackTypePositive);
  bridge.SendFeedback(env, kJavaFeedbackTypeNegative);

  EXPECT_DEATH_IF_SUPPORTED(bridge.SendFeedback(env, kInvalidFeedbackType), "");
}

}  // namespace readaloud

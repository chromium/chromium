// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/audible_contents_tracker.h"

#include <memory>

#include "base/path_service.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Observer for testing AudibleContentsTracker.
class MockAudibleContentsObserver
    : public metrics::AudibleContentsTracker::Observer {
 public:
  MockAudibleContentsObserver() = default;

  MockAudibleContentsObserver(const MockAudibleContentsObserver&) = delete;
  MockAudibleContentsObserver& operator=(const MockAudibleContentsObserver&) =
      delete;

  // AudibleContentsTracker::Observer:
  void OnAudioStart() override { is_audio_playing_ = true; }
  void OnAudioEnd() override { is_audio_playing_ = false; }

  bool is_audio_playing() const { return is_audio_playing_; }

 private:
  bool is_audio_playing_ = false;
};

}  // namespace

class AudibleContentsTrackerTest : public InProcessBrowserTest {
 public:
  AudibleContentsTrackerTest() = default;

  AudibleContentsTrackerTest(const AudibleContentsTrackerTest&) = delete;
  AudibleContentsTrackerTest& operator=(const AudibleContentsTrackerTest&) =
      delete;

  void SetUp() override {
    observer_ = std::make_unique<MockAudibleContentsObserver>();
    tracker_ = std::make_unique<metrics::AudibleContentsTracker>(observer());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    tracker_.reset();
    observer_.reset();
  }

  MockAudibleContentsObserver* observer() const { return observer_.get(); }

 private:
  std::unique_ptr<MockAudibleContentsObserver> observer_;
  std::unique_ptr<metrics::AudibleContentsTracker> tracker_;
};

IN_PROC_BROWSER_TEST_F(AudibleContentsTrackerTest, TestAudioNotifications) {
  MockAudibleContentsObserver* audio_observer = observer();
  EXPECT_FALSE(audio_observer->is_audio_playing());

  // Add a request handler for serving audio.
  base::FilePath test_data_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));
  embedded_test_server()->ServeFilesFromDirectory(
      test_data_dir.AppendASCII("chrome/test/data/"));
  // Start the test server after adding the request handler for thread safety.
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/autoplay_audio.html")));

  // Wait until the audio starts.
  while (!audio_observer->is_audio_playing()) {
    base::RunLoop().RunUntilIdle();
  }

  // Wait until the audio stops.
  while (audio_observer->is_audio_playing()) {
    base::RunLoop().RunUntilIdle();
  }
}

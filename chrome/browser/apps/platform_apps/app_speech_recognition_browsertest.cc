// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "media/base/media_switches.h"

class SpeechRecognitionTest : public extensions::PlatformAppBrowserTest {
 public:
  SpeechRecognitionTest() = default;
  SpeechRecognitionTest(const SpeechRecognitionTest&) = delete;
  SpeechRecognitionTest& operator=(const SpeechRecognitionTest&) = delete;
  ~SpeechRecognitionTest() override = default;

 protected:
  void SetUp() override {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    // For SpeechRecognitionTest.SpeechFromBackgroundPage test, we need to
    // fake the speech input to make tests run OK in bots.
    if (!strcmp(test_info->name(), "SpeechFromBackgroundPage")) {
      fake_speech_recognition_manager_ =
          std::make_unique<content::FakeSpeechRecognitionManager>();
      fake_speech_recognition_manager_->set_should_send_fake_response(true);
      // Inject the fake manager factory so that the test result is returned to
      // the web page.
      content::SpeechRecognitionManager::SetManagerForTesting(
          fake_speech_recognition_manager_.get());
    }

    extensions::PlatformAppBrowserTest::SetUp();
  }

 private:
  std::unique_ptr<content::FakeSpeechRecognitionManager>
      fake_speech_recognition_manager_;
};

IN_PROC_BROWSER_TEST_F(SpeechRecognitionTest, SpeechFromBackgroundPage) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kUseFakeUIForMediaStream);
  ASSERT_TRUE(RunExtensionTest("platform_apps/speech/background_page",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionTest,
                       SpeechFromBackgroundPageWithoutPermission) {
  EXPECT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFakeUIForMediaStream));
  ASSERT_TRUE(
      RunExtensionTest("platform_apps/speech/background_page_no_permission",
                       {.launch_as_platform_app = true}))
      << message_;
}

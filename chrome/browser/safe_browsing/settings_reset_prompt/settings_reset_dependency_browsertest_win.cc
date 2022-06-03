// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/settings_resetter_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_test_utils.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_util_win.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

using ::testing::InvokeWithoutArgs;
using ::testing::StrictMock;

class MockSettingsResetPromptDelegate : public SettingsResetPromptDelegate {
 public:
  MOCK_CONST_METHOD0(ShowSettingsResetPromptWithDelay, void());
};

// Test params: settings_reset_prompt_enabled_ representing if the corresponding
// feature is enabled.
class SettingsResetDependencyTest : public InProcessBrowserTest,
                                    public ::testing::WithParamInterface<bool> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    SetSettingsResetPromptDelegate(&delegate_);

    settings_reset_prompt_enabled_ = GetParam();

    if (settings_reset_prompt_enabled_) {
      scoped_feature_list_.InitAndEnableFeature(kSettingsResetPrompt);
      EXPECT_CALL(delegate_, ShowSettingsResetPromptWithDelay())
          .WillOnce(
              InvokeWithoutArgs([this] { reset_prompt_checked_ = true; }));
    } else {
      scoped_feature_list_.InitAndDisableFeature(kSettingsResetPrompt);
    }
  }

  void TearDownInProcessBrowserTestFixture() override {
    SetSettingsResetPromptDelegate(nullptr);
  }

 protected:
  bool settings_reset_prompt_enabled_;

  bool reset_prompt_checked_ = false;

  StrictMock<MockSettingsResetPromptDelegate> delegate_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SettingsResetDependencyTest,
                       PromptAfterPostCleanupReset) {
  if (settings_reset_prompt_enabled_) {
    while (!reset_prompt_checked_)
      base::RunLoop().RunUntilIdle();
  }
}

INSTANTIATE_TEST_SUITE_P(Default,
                         SettingsResetDependencyTest,
                         ::testing::Bool());

}  // namespace
}  // namespace safe_browsing

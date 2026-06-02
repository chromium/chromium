// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autocomplete/tab_matcher_android.h"

#include <memory>

#include "base/android/device_info.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabMatcherAndroidTest : public testing::Test {
 protected:
  TabMatcherAndroidTest() = default;
  ~TabMatcherAndroidTest() override = default;

  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  void TearDown() override {
    base::android::device_info::reset_is_desktop_for_testing();
  }

  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(TabMatcherAndroidTest, GetOpenTabs_ExcludeHeadlessOnDesktop) {
  // Enable desktop android mode.
  base::android::device_info::set_is_desktop_for_testing(true);

  // Create a headless tab model.
  OwningTestTabModel headless_model(profile(),
                                    chrome::android::ActivityType::kTabbed,
                                    TabModel::TabModelType::kHeadless);

  TabMatcherAndroid matcher(nullptr, profile());
  AutocompleteInput input;

  // Should return empty because the only tab model is headless and we are on
  // desktop. This also avoids JNI call because tab_models size is 0 after
  // filtering.
  std::vector<TabMatcher::TabWrapper> open_tabs = matcher.GetOpenTabs(&input);
  EXPECT_TRUE(open_tabs.empty());
}

TEST_F(TabMatcherAndroidTest, GetOpenTabs_IncludeHeadlessOnNonDesktop) {
  // Disable desktop android mode.
  base::android::device_info::set_is_desktop_for_testing(false);

  // Create a headless tab model.
  OwningTestTabModel headless_model(profile(),
                                    chrome::android::ActivityType::kTabbed,
                                    TabModel::TabModelType::kHeadless);

  TabMatcherAndroid matcher(nullptr, profile());
  AutocompleteInput input;

  // On non-desktop, headless is not filtered out, so it will try to call JNI.
  // In this test environment, JNI will return null because the model's Java
  // object is null. So it should still return empty, but it confirms it didn't
  // crash during JNI call.
  std::vector<TabMatcher::TabWrapper> open_tabs = matcher.GetOpenTabs(&input);
  EXPECT_TRUE(open_tabs.empty());
}

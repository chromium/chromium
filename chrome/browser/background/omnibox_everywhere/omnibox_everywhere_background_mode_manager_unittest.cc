// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/omnibox_everywhere/omnibox_everywhere_background_mode_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace omnibox_everywhere {

class OmniboxEverywhereBackgroundModeManagerTest : public ChromeViewsTestBase {
 public:
  OmniboxEverywhereBackgroundModeManagerTest() = default;
  ~OmniboxEverywhereBackgroundModeManagerTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_{omnibox::kOmniboxEverywhere};
};

TEST_F(OmniboxEverywhereBackgroundModeManagerTest, InitializationDoesNotCrash) {
  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));
}

TEST_F(OmniboxEverywhereBackgroundModeManagerTest, BackgroundModePrefToggle) {
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();

  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));

  // Toggle background mode pref off and on.
  if (local_state) {
    local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, false);
    local_state->SetBoolean(prefs::kOmniboxEverywhereBackgroundMode, true);
  }
}

TEST_F(OmniboxEverywhereBackgroundModeManagerTest,
       StatusIconClickTriggersCallback) {
  bool callback_called = false;
  OmniboxEverywhereBackgroundModeManager manager(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));

  // Simulate status icon click callback execution
  static_cast<StatusIconObserver*>(&manager)->OnStatusIconClicked();
  EXPECT_TRUE(callback_called);
}

}  // namespace omnibox_everywhere

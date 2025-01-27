// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/launcher/glic_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicControllerBrowserTest : public InProcessBrowserTest {
 public:
  GlicControllerBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton},
        /*disabled_features=*/{});
  }

  ~GlicControllerBrowserTest() override = default;

 protected:
  GlicController& glic_controller() { return glic_controller_; }

  GlicController glic_controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicControllerBrowserTest, ShowAndHide) {
  EXPECT_FALSE(GlicProfileManager::GetInstance()->HasActiveGlicService());

  glic_controller().Show();
  EXPECT_TRUE(GlicProfileManager::GetInstance()->HasActiveGlicService());

  glic_controller().Hide();
  EXPECT_FALSE(GlicProfileManager::GetInstance()->HasActiveGlicService());
}

}  // namespace glic

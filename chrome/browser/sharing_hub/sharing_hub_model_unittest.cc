// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing_hub/sharing_hub_model.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class SharingHubModelTest : public ::testing::Test {
 public:
  SharingHubModelTest() = default;
  ~SharingHubModelTest() override = default;

  sharing_hub::SharingHubModel* model() { return &model_; }
  Profile* profile() { return &profile_; }
  content::WebContents* web_contents() { return test_web_contents_.get(); }

  void NavigateTo(const GURL& url) {
    content::WebContentsTester::For(test_web_contents_.get())
        ->NavigateAndCommit(url);
  }

  std::vector<sharing_hub::SharingHubAction> GetFirstPartyActions() {
    return model_.GetFirstPartyActionList(test_web_contents_.get());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> test_web_contents_ =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);

  sharing_hub::SharingHubModel model_{&profile_};
};

// Verifies that first-party actions are available for both web and internal
// Chrome URLs even when `content::WebContents` is not embedded in a browser
// window.
TEST_F(SharingHubModelTest, FirstPartyOptionsOfferedOnAllURLs) {
  NavigateTo(GURL("https://www.chromium.org"));
  EXPECT_GT(GetFirstPartyActions().size(), 0u);
  NavigateTo(GURL("chrome://version"));
  EXPECT_GT(GetFirstPartyActions().size(), 0u);
}

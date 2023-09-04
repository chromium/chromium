// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/help_app/help_app_ui_delegate.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/borealis/testing/features.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_web_ui.h"

namespace ash {

class HelpAppUiDelegateTest : public BrowserWithTestWindowTest {
 public:
  HelpAppUiDelegateTest()
      : user_manager_(new FakeChromeUserManager()),
        scoped_user_manager_(
            std::unique_ptr<FakeChromeUserManager>(user_manager_)),
        web_ui_(std::make_unique<content::TestWebUI>()) {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL("about:blank"));
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    web_ui_->set_web_contents(contents);
    delegate_ = std::make_unique<ChromeHelpAppUIDelegate>(web_ui());
  }

  void TearDown() override {
    delegate_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  content::WebUI* web_ui() { return web_ui_.get(); }

  TestingProfile profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<FakeChromeUserManager, DanglingUntriaged | ExperimentalAsh>
      user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<ChromeHelpAppUIDelegate> delegate_;
};

TEST_F(HelpAppUiDelegateTest, DeviceInfoWhenBorealisIsNotAllowed) {
  base::test::TestFuture<help_app::mojom::DeviceInfoPtr> info_future;
  delegate_->GetDeviceInfo(info_future.GetCallback());

  help_app::mojom::DeviceInfoPtr device_info_ptr = info_future.Take();
  ASSERT_EQ(device_info_ptr->is_steam_allowed, false);
}

TEST_F(HelpAppUiDelegateTest, DeviceInfoWhenBorealisIsAllowed) {
  borealis::AllowBorealis(profile(), &scoped_feature_list_, user_manager_,
                          /*also_enable=*/false);

  base::test::TestFuture<help_app::mojom::DeviceInfoPtr> info_future;
  delegate_->GetDeviceInfo(info_future.GetCallback());

  help_app::mojom::DeviceInfoPtr device_info_ptr = info_future.Take();
  ASSERT_EQ(device_info_ptr->is_steam_allowed, true);
}

}  // namespace ash

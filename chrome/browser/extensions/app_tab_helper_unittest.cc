// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_tab_helper.h"

#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/test/test_extension_dir.h"
#include "url/origin.h"

namespace extensions {

class AppTabHelperUnitTest : public ExtensionServiceTestWithInstall {
 public:
  void SetUp() override {
    ExtensionServiceTestWithInstall::SetUp();
    InitializeEmptyExtensionService();

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    web_contents_tester_ = content::WebContentsTester::For(web_contents_.get());
    AppTabHelper::CreateForWebContents(web_contents_.get());
    app_tab_helper_ = AppTabHelper::FromWebContents(web_contents_.get());
  }

  void TearDown() override {
    app_tab_helper_ = nullptr;
    web_contents_tester_ = nullptr;
    web_contents_.reset();
    ExtensionServiceTestWithInstall::TearDown();
  }

  AppTabHelper* app_tab_helper() { return app_tab_helper_; }

 private:
  std::unique_ptr<content::WebContents> web_contents_;

  raw_ptr<content::WebContentsTester> web_contents_tester_ = nullptr;
  raw_ptr<AppTabHelper> app_tab_helper_ = nullptr;
};

TEST_F(AppTabHelperUnitTest, ClearsExtensionOnUnload) {
  const Extension* extension =
      PackAndInstallCRX(data_dir().AppendASCII("hosted_app"), INSTALL_NEW);
  ASSERT_TRUE(extension);

  app_tab_helper()->SetExtensionApp(extension);
  EXPECT_EQ(extension->id(), app_tab_helper()->GetExtensionAppId());
  EXPECT_TRUE(app_tab_helper()->is_app());
  registrar()->RemoveExtension(extension->id(),
                               UnloadedExtensionReason::TERMINATE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ExtensionId(), app_tab_helper()->GetExtensionAppId());
}

}  // namespace extensions

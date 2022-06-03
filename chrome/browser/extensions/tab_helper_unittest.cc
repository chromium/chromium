// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/tab_helper.h"

#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/web_contents_tester.h"

namespace extensions {

TEST_F(ExtensionServiceTestWithInstall, TabHelperClearsExtensionOnUnload) {
  InitializeEmptyExtensionService();
  const Extension* extension =
      PackAndInstallCRX(data_dir().AppendASCII("hosted_app"), INSTALL_NEW);
  ASSERT_TRUE(extension);
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  TabHelper::CreateForWebContents(web_contents.get());
  TabHelper* tab_helper = TabHelper::FromWebContents(web_contents.get());
  tab_helper->SetExtensionApp(extension);
  EXPECT_EQ(extension->id(), tab_helper->GetExtensionAppId());
  EXPECT_TRUE(tab_helper->is_app());
  service()->UnloadExtension(extension->id(),
                             UnloadedExtensionReason::TERMINATE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ExtensionId(), tab_helper->GetExtensionAppId());
}

}  // namespace extensions

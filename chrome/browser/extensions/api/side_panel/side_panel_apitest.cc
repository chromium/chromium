// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"

namespace extensions {

class SidePanelApiTest : public ExtensionApiTest {
 public:
  SidePanelApiTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionSidePanelIntegration);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ScopedCurrentChannel current_channel_{version_info::Channel::CANARY};
};

// Verify normal chrome.sidePanel functionality.
IN_PROC_BROWSER_TEST_F(SidePanelApiTest, Extension) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("side_panel/extension")) << message_;
}

// Verify chrome.sidePanel behavior without permissions.
IN_PROC_BROWSER_TEST_F(SidePanelApiTest, PermissionMissing) {
  ASSERT_TRUE(RunExtensionTest("side_panel/permission_missing")) << message_;
}

// Verify chrome.sidePanel.get behavior without side_panel manifest key.
IN_PROC_BROWSER_TEST_F(SidePanelApiTest, MissingManifestKey) {
  ASSERT_TRUE(RunExtensionTest("side_panel/missing_manifest_key")) << message_;
}

}  // namespace extensions

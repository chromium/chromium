// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

namespace extensions {

class WmDesksPrivateApiTest : public ExtensionApiTest {
 public:
  WmDesksPrivateApiTest() {
    scoped_feature_list.InitWithFeatures(
        /*enabled_features=*/{ash::features::kEnableSavedDesks,
                              ash::features::kDesksTemplates},
        /*disabled_features=*/{ash::features::kDeskTemplateSync});
  }

  ~WmDesksPrivateApiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list;
};

IN_PROC_BROWSER_TEST_F(WmDesksPrivateApiTest, WmDesksPrivateApiTest) {
  // This loads and runs an extension from
  // chrome/test/data/extensions/api_test/wm_desks_private.
  ASSERT_TRUE(RunExtensionTest("wm_desks_private"));
}

}  // namespace extensions

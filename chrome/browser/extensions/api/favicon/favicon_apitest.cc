// Copyright 2022 The Chromium Authors. All rights reserved.
// // Use of this source code is governed by a BSD-style license that can be
// // found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"

namespace extensions {

class FaviconApiTest : public ExtensionApiTest {
 public:
  FaviconApiTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kNewExtensionFaviconHandling);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ScopedCurrentChannel current_cnannel_{version_info::Channel::CANARY};
};

IN_PROC_BROWSER_TEST_F(FaviconApiTest, Extension) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(
      RunExtensionTest("favicon/extension", {.extension_url = "test.html"}))
      << message_;
}

}  // namespace extensions

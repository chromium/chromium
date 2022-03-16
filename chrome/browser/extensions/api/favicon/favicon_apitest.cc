// Copyright 2022 The Chromium Authors. All rights reserved.
// // Use of this source code is governed by a BSD-style license that can be
// // found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"

namespace extensions {

class FaviconApiTest : public ExtensionApiTest {
 private:
  ScopedCurrentChannel current_cnannel_{version_info::Channel::CANARY};
};

IN_PROC_BROWSER_TEST_F(FaviconApiTest, Extension) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(
      RunExtensionTest("favicon/extension", {.extension_url = "test.html"}))
      << message_;
}

}  // namespace extensions

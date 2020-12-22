// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

namespace extensions {

// Flaky for Mac: crbug.com/1141100
#if defined(OS_MAC)
#define MAYBE_Storage DISABLED_Storage
#else
#define MAYBE_Storage Storage
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_Storage) {
  ASSERT_TRUE(RunExtensionTest("storage")) << message_;
}

}  // namespace extensions

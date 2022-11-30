// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

namespace extensions {

// Flaky for Mac: crbug.com/1141100
// TODO(crbug.com/1317431): WebSQL does not work on Fuchsia.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA)
#define MAYBE_Storage DISABLED_Storage
#else
#define MAYBE_Storage Storage
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_Storage) {
  ASSERT_TRUE(RunExtensionTest("storage")) << message_;
}

}  // namespace extensions

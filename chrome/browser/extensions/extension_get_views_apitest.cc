// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

namespace extensions {

// Failed run on ChromeOS CI builder. https://crbug.com/1245240
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_GetViews DISABLED_GetViews
#else
#define MAYBE_GetViews GetViews
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_GetViews) {
  ASSERT_TRUE(RunExtensionTest("get_views")) << message_;
}

}  // namespace extensions

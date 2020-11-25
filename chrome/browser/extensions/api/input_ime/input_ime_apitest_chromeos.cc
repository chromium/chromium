// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/test/test_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, InputImeApiBasic) {
  ASSERT_TRUE(RunExtensionTest("input_ime")) << message_;
}
#endif

}  // namespace extensions

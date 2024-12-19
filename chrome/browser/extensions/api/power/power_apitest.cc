// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/power/power_api.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/extension_platform_apitest.h"
#else
#include "chrome/browser/extensions/extension_apitest.h"
#endif

namespace extensions {
namespace {

#if BUILDFLAG(IS_ANDROID)
using PowerApiTest = ExtensionPlatformApiTest;
#else
using PowerApiTest = ExtensionApiTest;
#endif

IN_PROC_BROWSER_TEST_F(PowerApiTest, Basics) {
  ASSERT_TRUE(RunExtensionTest("power/basics")) << message_;

  // The test should leave no wake locks (no "level" for any extension).
  EXPECT_TRUE(PowerAPI::Get(profile())->extension_levels().empty());
}

}  // namespace
}  // namespace extensions

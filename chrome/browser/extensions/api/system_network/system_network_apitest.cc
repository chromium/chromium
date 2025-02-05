// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/extension_platform_apitest.h"
#else
#include "chrome/browser/extensions/extension_apitest.h"
#endif

namespace extensions {
namespace {

#if BUILDFLAG(IS_ANDROID)
using SystemNetworkApiTest = ExtensionPlatformApiTest;
#else
using SystemNetworkApiTest = ExtensionApiTest;
#endif

IN_PROC_BROWSER_TEST_F(SystemNetworkApiTest, SystemNetworkExtension) {
  ASSERT_TRUE(RunExtensionTest("system_network")) << message_;
}

}  // namespace
}  // namespace extensions

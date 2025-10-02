// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

using ContextType = extensions::browser_test_util::ContextType;

class ExtensionModuleApiTest : public ExtensionApiTest,
                               public testing::WithParamInterface<ContextType> {
 public:
  ExtensionModuleApiTest() : ExtensionApiTest(GetParam()) {}
  ~ExtensionModuleApiTest() override = default;
  ExtensionModuleApiTest(const ExtensionModuleApiTest&) = delete;
  ExtensionModuleApiTest& operator=(const ExtensionModuleApiTest&) = delete;
};

// Android only supports service worker.
#if !BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ExtensionModuleApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));
#endif
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionModuleApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

}  // namespace

IN_PROC_BROWSER_TEST_P(ExtensionModuleApiTest, CognitoFile) {
  ASSERT_TRUE(RunExtensionTest("extension_module/cognito_file", {},
                               {.allow_file_access = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionModuleApiTest, IncognitoFile) {
  ASSERT_TRUE(
      RunExtensionTest("extension_module/incognito_file", {},
                       {.allow_in_incognito = true, .allow_file_access = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionModuleApiTest, CognitoNoFile) {
  ASSERT_TRUE(RunExtensionTest("extension_module/cognito_nofile")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionModuleApiTest, IncognitoNoFile) {
  ASSERT_TRUE(RunExtensionTest("extension_module/incognito_nofile", {},
                               {.allow_in_incognito = true}))
      << message_;
}

}  // namespace extensions

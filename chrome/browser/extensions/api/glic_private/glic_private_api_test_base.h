// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_GLIC_PRIVATE_GLIC_PRIVATE_API_TEST_BASE_H_
#define CHROME_BROWSER_EXTENSIONS_API_GLIC_PRIVATE_GLIC_PRIVATE_API_TEST_BASE_H_

#include <memory>
#include <string>

#include "chrome/browser/extensions/extension_apitest.h"

namespace content {
class URLLoaderInterceptor;
}

namespace extensions {

extern const char kGlicPrivateTestExtensionId[];

class GlicPrivateApiTestBase : public ExtensionApiTest {
 public:
  GlicPrivateApiTestBase();
  ~GlicPrivateApiTestBase() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

 protected:
  void SetupIdentityAndCapabilities();

  static std::unique_ptr<content::URLLoaderInterceptor>
  CreateMockPromptResponseInterceptor(
      const std::string& prompt_data = "Mock successful prompt data");
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_GLIC_PRIVATE_GLIC_PRIVATE_API_TEST_BASE_H_

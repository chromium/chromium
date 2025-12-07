// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_USER_SCRIPTS_USER_SCRIPTS_APITEST_H_
#define CHROME_BROWSER_EXTENSIONS_API_USER_SCRIPTS_USER_SCRIPTS_APITEST_H_

#include "chrome/browser/extensions/extension_apitest.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace base {
class FilePath;
}  // namespace base

namespace content {
class EvalJsResult;
class RenderFrameHost;
}  // namespace content

class GURL;

namespace extensions {

// Used to test the chrome.userScripts API.
class UserScriptsAPITest : public ExtensionApiTest,
                           public testing::WithParamInterface<bool> {
 public:
  UserScriptsAPITest();
  UserScriptsAPITest(const UserScriptsAPITest&) = delete;
  const UserScriptsAPITest& operator=(const UserScriptsAPITest&) = delete;
  ~UserScriptsAPITest() override;

 protected:
  void SetUpOnMainThread() override;

  // Retrieves the <div> IDs of the injected elements in a given frame.
  content::EvalJsResult GetInjectedElements(content::RenderFrameHost* host);

  // Enables the chrome.userScripts API and runs an extension test for an
  // extension installed at `test_data_dir_`/`extension_sub_path`.
  testing::AssertionResult RunUserScriptsExtensionTest(
      const char* extension_sub_path);

  // Does not enable the chrome.userScripts API and runs an extension test for
  // an extension installed at `test_data_dir_`/`extension_sub_path`.
  testing::AssertionResult RunUserScriptsExtensionTestNotAllowed(
      const base::FilePath& extension_path);

  // Opens a URL in a new tab
  content::RenderFrameHost* OpenInNewTab(const GURL& url);

  // Opens a URL in the current tab.
  void OpenInCurrentTab(const GURL& url);

 private:
  testing::AssertionResult RunUserScriptsExtensionTestImpl(
      const base::FilePath& extension_path,
      bool allow_api);

  // Some userScripts API methods are currently behind a feature restriction.
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_USER_SCRIPTS_USER_SCRIPTS_APITEST_H_

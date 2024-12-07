// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PLATFORM_BROWSERTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PLATFORM_BROWSERTEST_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/extension_browser_test_util.h"
#include "chrome/test/base/platform_browser_test.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;

// A cross-platform base class for extensions-related browser tests.
// `PlatformBrowserTest` inherits from different test suites based on the
// platform; `ExtensionPlatformBrowserTest` provides additional functionality
// that is available on all platforms.
class ExtensionPlatformBrowserTest : public PlatformBrowserTest {
 public:
  using LoadOptions = extensions::browser_test_util::LoadOptions;
  using ContextType = extensions::browser_test_util::ContextType;

  explicit ExtensionPlatformBrowserTest(
      ContextType context_type = ContextType::kNone);
  ExtensionPlatformBrowserTest(const ExtensionPlatformBrowserTest&) = delete;
  ExtensionPlatformBrowserTest& operator=(const ExtensionPlatformBrowserTest&) =
      delete;
  ~ExtensionPlatformBrowserTest() override;

 protected:
  // content::BrowserTestBase:
  void SetUp() override;
  void SetUpOnMainThread() override;
  void TearDown() override;
  void TearDownOnMainThread() override;

  const Extension* LoadExtension(const base::FilePath& path);
  const Extension* LoadExtension(const base::FilePath& path,
                                 const LoadOptions& options);

  void DisableExtension(const std::string& extension_id, int disable_reasons);

  // Returns the WebContents of the currently active tab.
  // Note that when the test first launches, this will be the same as the
  // default tab's web_contents(). However, if the test creates new tabs and
  // switches the active tab, this will return the WebContents of the new active
  // tab.
  content::WebContents* GetActiveWebContents();

  // Returns incognito profile. Creates the profile if it doesn't exist.
  Profile* GetOrCreateIncognitoProfile();

  // Opens `url` in an incognito browser window with the incognito profile of
  // `profile`, blocking until the navigation finishes.
  void PlatformOpenURLOffTheRecord(Profile* profile, const GURL& url);

  // Lower case to match the style of InProcessBrowserTest.
  Profile* profile();

  // WebContents* of the default tab or nullptr if the default tab is destroyed.
  content::WebContents* web_contents();

  const ExtensionId& last_loaded_extension_id() {
    return last_loaded_extension_id_;
  }

  // Set to "chrome/test/data/extensions". Derived classes may override.
  base::FilePath test_data_dir_;

  const ContextType context_type_;

 private:
  // Temporary directory for testing.
  base::ScopedTempDir temp_dir_;

  // WebContents* of the default tab or nullptr if the default tab is destroyed.
  base::WeakPtr<content::WebContents> web_contents_;

  ExtensionId last_loaded_extension_id_;

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  class TestTabModel;
  std::unique_ptr<TestTabModel> tab_model_;
#endif
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PLATFORM_BROWSERTEST_H_

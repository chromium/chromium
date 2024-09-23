// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "ui/native_theme/native_theme.h"

namespace extensions {

namespace {

class TestWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit TestWebContentsObserver(content::WebContents* contents)
      : WebContentsObserver(contents) {}

  // content::WebContentsObserver:
  void OnColorProviderChanged() override { ++color_provider_changed_; }

  int color_provider_changed() const { return color_provider_changed_; }

 private:
  int color_provider_changed_ = 0;
};

}  // namespace

// Tests that background pages do not have color provider changed.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       NoColorProviderChangedForBackgroundPage) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("good")
                                .AppendASCII("Extensions")
                                .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                                .AppendASCII("1.0.0.0")));

  ProcessManager* manager = ProcessManager::Get(browser()->profile());
  ExtensionHost* host = FindHostWithPath(manager, "/backgroundpage.html", 1);

  TestWebContentsObserver observer(host->host_contents());

  // Simulate theme change.
  ui::NativeTheme::GetInstanceForWeb()->NotifyOnNativeThemeUpdated();

  // No color provider change should happen.
  EXPECT_EQ(0, observer.color_provider_changed());
}

}  // namespace extensions

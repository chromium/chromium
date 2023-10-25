// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_FEATURE_BROWSERTEST_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_FEATURE_BROWSERTEST_H_

#include <memory>

#include "chrome/test/base/chromeos/ash_browser_test_starter.h"
#include "chrome/test/base/in_process_browser_test.h"

class Profile;

namespace ash {

// Class that can be used for Accessibility feature browsertests that run
// in Ash or Lacros.
class AccessibilityFeatureBrowserTest : public InProcessBrowserTest {
 public:
  AccessibilityFeatureBrowserTest();
  AccessibilityFeatureBrowserTest(const AccessibilityFeatureBrowserTest&) =
      delete;
  AccessibilityFeatureBrowserTest& operator=(
      const AccessibilityFeatureBrowserTest&) = delete;
  ~AccessibilityFeatureBrowserTest() override;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;
  void TearDownInProcessBrowserTestFixture() override;

  // Navigates to the given URL in Ash or Lacros. Does not wait for the load to
  // complete.
  void NavigateToUrl(const GURL& url);

  bool IsLacrosRunning() const;

  // Returns the Ash profile.
  Profile* GetProfile() const;

 private:
  std::unique_ptr<::test::AshBrowserTestStarter> ash_starter_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_FEATURE_BROWSERTEST_H_

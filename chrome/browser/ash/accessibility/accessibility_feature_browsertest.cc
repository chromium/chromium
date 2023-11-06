// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_feature_browsertest.h"

#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/ui_test_utils.h"

namespace ash {

AccessibilityFeatureBrowserTest::AccessibilityFeatureBrowserTest() {}

AccessibilityFeatureBrowserTest::~AccessibilityFeatureBrowserTest() {}

void AccessibilityFeatureBrowserTest::SetUpInProcessBrowserTestFixture() {
  ash_starter_ = std::make_unique<::test::AshBrowserTestStarter>();
  if (ash_starter_->HasLacrosArgument()) {
    ASSERT_TRUE(ash_starter_->PrepareEnvironmentForLacros());
  }
}

void AccessibilityFeatureBrowserTest::SetUpOnMainThread() {
  CHECK(ash_starter_);
  if (ash_starter_->HasLacrosArgument()) {
    ash_starter_->StartLacros(this);
  }
}

void AccessibilityFeatureBrowserTest::TearDownInProcessBrowserTestFixture() {
  ash_starter_.reset();
}

void AccessibilityFeatureBrowserTest::NavigateToUrl(const GURL& url) {
  CHECK(ash_starter_);
  if (ash_starter_->HasLacrosArgument()) {
    crosapi::BrowserManager::Get()->OpenUrl(
        url, crosapi::mojom::OpenUrlFrom::kUnspecified,
        crosapi::mojom::OpenUrlParams::WindowOpenDisposition::
            kNewForegroundTab);
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        BrowserList::GetInstance()->GetLastActive(), url));
  }
}

bool AccessibilityFeatureBrowserTest::IsLacrosRunning() const {
  CHECK(ash_starter_);
  return ash_starter_->HasLacrosArgument();
}

Profile* AccessibilityFeatureBrowserTest::GetProfile() const {
  return AccessibilityManager::Get()->profile();
}

}  // namespace ash

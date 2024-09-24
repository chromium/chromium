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

void AccessibilityFeatureBrowserTest::NavigateToUrl(const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        BrowserList::GetInstance()->GetLastActive(), url));
}

Profile* AccessibilityFeatureBrowserTest::GetProfile() const {
  return AccessibilityManager::Get()->profile();
}

}  // namespace ash

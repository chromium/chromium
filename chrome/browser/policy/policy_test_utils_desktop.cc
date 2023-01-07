// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "url/gurl.h"

namespace policy {

bool PolicyTest::NavigateToUrl(GURL url, PlatformBrowserTest* browser_test) {
  return ui_test_utils::NavigateToURL(browser_test->browser(), url);
}

}  // namespace policy

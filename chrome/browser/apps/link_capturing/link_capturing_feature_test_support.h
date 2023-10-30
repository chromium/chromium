// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURE_TEST_SUPPORT_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURE_TEST_SUPPORT_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"

namespace apps {

// The functions should only be called from tests, and is used to enable or
// disable link capturing UXes. Only use these if link capturing needs to be
// enabled on all platforms, i.e. ChromeOS, Windows, Mac and Linux. For platform
// specific implementations, prefer initializing the feature list in the test
// file itself.
void EnableLinkCapturingUXForTesting(
    base::test::ScopedFeatureList& scoped_feature_list);
void DisableLinkCapturingUXForTesting(
    base::test::ScopedFeatureList& scoped_feature_list);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_LINK_CAPTURING_FEATURE_TEST_SUPPORT_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_FEATURES_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_FEATURES_H_

#include "base/feature_list.h"

namespace default_browser {

// Returns whether the default browser framework feature flag is enabled.
bool IsDefaultBrowserFrameworkEnabled();

// Returns whether the default browser changed os notification feature flag is
// enabled.
bool IsDefaultBrowserChangedOsNotificationEnabled();

BASE_DECLARE_FEATURE(kDefaultBrowserFramework);

// Enables the framework to perform additional checks when detecting default
// browser.
BASE_DECLARE_FEATURE(kPerformDefaultBrowserCheckValidations);

// Enables the framework to show Os Notification when Chrome is no longer the
// default browser.
// NOTE: This flag expect that `kDefaultBrowserFramework` is enabled first.
BASE_DECLARE_FEATURE(kDefaultBrowserChangedOsNotification);

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_BROWSER_FEATURES_H_

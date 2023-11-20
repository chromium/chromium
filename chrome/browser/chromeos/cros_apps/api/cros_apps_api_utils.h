// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_UTILS_H_

class GURL;

// Returns whether `url` is eligible for ChromeOS Apps APIs.
bool IsUrlEligibleForCrosAppsApis(const GURL& url);

#endif  // CHROME_BROWSER_CHROMEOS_CROS_APPS_API_CROS_APPS_API_UTILS_H_

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_FEATURES_H_
#define CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_FEATURES_H_

namespace base {
struct Feature;
}

namespace apps {

extern const base::Feature kAppDiscoveryRemoteUrlSearch;

bool IsRemoteUrlSearchEnabled();

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_DISCOVERY_SERVICE_APP_DISCOVERY_FEATURES_H_

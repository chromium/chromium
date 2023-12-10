// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_utils.h"

#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/gurl.h"

bool IsUrlEligibleForCrosAppsApis(const GURL& url) {
  return network::IsUrlPotentiallyTrustworthy(url);
}

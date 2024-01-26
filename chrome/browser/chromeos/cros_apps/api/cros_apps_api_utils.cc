// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_utils.h"

#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/gurl.h"
#include "url/url_constants.h"

bool IsUrlEligibleForCrosAppsApis(const GURL& url) {
  // TODO(b/311528206): Decide if this scheme check should be removed.
  //
  // The following schemes because they share the same origin as their creator
  // (i.e. the App), and could cause problem during origin matching.
  //
  // The app could inadvertently create these URLs that serve third-party (from
  // the App's perspective) untrustworthy content. Said third-party content
  // probably shouldn't be treated as same origin as the app.
  if (url.SchemeIs(url::kBlobScheme) || url.SchemeIs(url::kFileSystemScheme)) {
    return false;
  }

  return network::IsUrlPotentiallyTrustworthy(url);
}

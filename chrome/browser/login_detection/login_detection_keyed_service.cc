// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_detection/login_detection_keyed_service.h"

#include "chrome/browser/login_detection/login_detection_prefs.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "content/public/browser/child_process_security_policy.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace login_detection {

bool OriginComparator::operator()(const std::string& a,
                                  const std::string& b) const {
  return url::Origin::Create(GURL(a)) < url::Origin::Create(GURL(b));
}

LoginDetectionKeyedService::LoginDetectionKeyedService(Profile* profile)
    : profile_(profile) {
  // Apply site isolation to logged-in sites that had previously been saved by
  // login detection. Needs to be called before any navigations happen in
  // `profile`.
  //
  // TODO(alexmos): Move this initialization to components/site_isolation once
  // login detection is moved into its own component.
  site_isolation::SiteIsolationPolicy::IsolateStoredOAuthSites(
      profile, prefs::GetOAuthSignedInSites(profile->GetPrefs()));
}

LoginDetectionKeyedService::~LoginDetectionKeyedService() = default;

}  // namespace login_detection

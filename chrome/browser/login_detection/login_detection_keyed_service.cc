// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_detection/login_detection_keyed_service.h"

#include "chrome/browser/login_detection/login_detection_prefs.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "content/public/browser/child_process_security_policy.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace login_detection {

namespace {

// Gets the set of sites to be treated as logged-in from field trial.
std::set<std::string, OriginComparator> GetLoggedInSites() {
  auto sites = GetLoggedInSitesFromFieldTrial();
  return std::set<std::string, OriginComparator>(sites.begin(), sites.end());
}

}  // namespace

bool OriginComparator::operator()(const std::string& a,
                                  const std::string& b) const {
  return url::Origin::Create(GURL(a)) < url::Origin::Create(GURL(b));
}

LoginDetectionKeyedService::LoginDetectionKeyedService(Profile* profile)
    : profile_(profile),
      field_trial_logged_in_sites_(GetLoggedInSites()),
      profile_password_sites_(PasswordStoreFactory::GetForProfile(
                                  profile,
                                  ServiceAccessType::EXPLICIT_ACCESS)
                                  .get()),
      account_password_sites_(AccountPasswordStoreFactory::GetForProfile(
                                  profile,
                                  ServiceAccessType::EXPLICIT_ACCESS)
                                  .get()) {
  if (auto* optimization_guide_decider =
          OptimizationGuideKeyedServiceFactory::GetForProfile(profile_)) {
    optimization_guide_decider->RegisterOptimizationTypes(
        {optimization_guide::proto::LOGIN_DETECTION});
  }

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

LoginDetectionType LoginDetectionKeyedService::GetPersistentLoginDetection(
    const GURL& url) const {
  // Check if OAuth login for this site was detected earlier, and remembered
  // in prefs.
  if (prefs::IsSiteInOAuthSignedInList(profile_->GetPrefs(), url))
    return LoginDetectionType::kOauthLogin;

  // Check if this is common log-in site retrieved from field trial.
  if (field_trial_logged_in_sites_.find(GetSiteNameForURL(url)) !=
      field_trial_logged_in_sites_.end()) {
    return LoginDetectionType::kFieldTrialLoggedInSite;
  }

  auto* child_process_security_policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  url::Origin url_origin = url::Origin::Create(url);

  // Check for password entered logins. These are saved as user triggered source
  // in site-isolation.
  if (child_process_security_policy->IsIsolatedSiteFromSource(
          url_origin, content::ChildProcessSecurityPolicy::
                          IsolatedOriginSource::USER_TRIGGERED)) {
    return LoginDetectionType::kPasswordEnteredLogin;
  }

  // Check for sites from preloaded list. These are saved as built-in source in
  // site-isolation.
  if (child_process_security_policy->IsIsolatedSiteFromSource(
          url_origin, content::ChildProcessSecurityPolicy::
                          IsolatedOriginSource::BUILT_IN)) {
    return LoginDetectionType::kPreloadedPasswordSiteLogin;
  }

  if (auto* optimization_guide_decider =
          OptimizationGuideKeyedServiceFactory::GetForProfile(profile_)) {
    if (optimization_guide_decider->CanApplyOptimization(
            url, optimization_guide::proto::LOGIN_DETECTION, nullptr) ==
        optimization_guide::OptimizationGuideDecision::kTrue) {
      return LoginDetectionType::kOptimizationGuideDetected;
    }
  }

  // Check for sites saved in the password manager.
  if (profile_password_sites_.IsSiteInPasswordStore(url) ||
      account_password_sites_.IsSiteInPasswordStore(url)) {
    return LoginDetectionType::kPasswordManagerSavedSite;
  }

  return LoginDetectionType::kNoLogin;
}

}  // namespace login_detection

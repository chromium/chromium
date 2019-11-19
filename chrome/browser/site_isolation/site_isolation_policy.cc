// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_isolation/site_isolation_policy.h"

#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/site_isolation_policy.h"

// static
bool SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled() {
  // If the user has explicitly enabled site isolation for password sites from
  // chrome://flags or from the command line, honor this regardless of policies
  // that may disable site isolation.  In particular, this means that the
  // chrome://flags switch for this feature takes precedence over any memory
  // threshold restrictions and over a switch for disabling site isolation.
  if (base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
          features::kSiteIsolationForPasswordSites.name,
          base::FeatureList::OVERRIDE_ENABLE_FEATURE)) {
    return true;
  }

  // Don't isolate anything when site isolation is turned off by the user or
  // policy. This includes things like the switches::kDisableSiteIsolation
  // command-line switch, the corresponding "Disable site isolation" entry in
  // chrome://flags, enterprise policy controlled via
  // switches::kDisableSiteIsolationForPolicy, and memory threshold checks in
  // ShouldDisableSiteIsolationDueToMemoryThreshold().
  if (!content::SiteIsolationPolicy::AreDynamicIsolatedOriginsEnabled())
    return false;

  // The feature needs to be checked last, because checking the feature
  // activates the field trial and assigns the client either to a control or an
  // experiment group - such assignment should be final.
  return base::FeatureList::IsEnabled(features::kSiteIsolationForPasswordSites);
}

// static
bool SiteIsolationPolicy::IsEnterprisePolicyApplicable() {
#if defined(OS_ANDROID)
  // https://crbug.com/844118: Limiting policy to devices with > 1GB RAM.
  // Using 1077 rather than 1024 because 1) it helps ensure that devices with
  // exactly 1GB of RAM won't get included because of inaccuracies or off-by-one
  // errors and 2) this is the bucket boundary in Memory.Stats.Win.TotalPhys2.
  bool have_enough_memory = base::SysInfo::AmountOfPhysicalMemoryMB() > 1077;
  return have_enough_memory;
#else
  return true;
#endif
}

// static
bool SiteIsolationPolicy::ShouldDisableSiteIsolationDueToMemoryThreshold() {
  // The memory threshold behavior differs for desktop and Android:
  // - Android uses a 1900MB default threshold, which is the threshold used by
  //   password-triggered site isolation - see docs in
  //   https://crbug.com/849815.  This can be overridden via a param defined in
  //   a kSitePerProcessOnlyForHighMemoryClients field trial.
  // - Desktop does not enforce a default memory threshold, but for now we
  //   still support a threshold defined via a
  //   kSitePerProcessOnlyForHighMemoryClients field trial.  The trial
  //   typically carries the threshold in a param; if it doesn't, use a default
  //   that's slightly higher than 1GB (see https://crbug.com/844118).
  //
  // TODO(alexmos): currently, this threshold applies to all site isolation
  // modes.  Eventually, we may need separate thresholds for different modes,
  // such as full site isolation vs. password-triggered site isolation.
#if defined(OS_ANDROID)
  constexpr int kDefaultMemoryThresholdMb = 1900;
#else
  constexpr int kDefaultMemoryThresholdMb = 1077;
#endif

  // TODO(acolwell): Rename feature since it now affects more than just the
  // site-per-process case.
  if (base::FeatureList::IsEnabled(
          features::kSitePerProcessOnlyForHighMemoryClients)) {
    int memory_threshold_mb = base::GetFieldTrialParamByFeatureAsInt(
        features::kSitePerProcessOnlyForHighMemoryClients,
        features::kSitePerProcessOnlyForHighMemoryClientsParamName,
        kDefaultMemoryThresholdMb);
    return base::SysInfo::AmountOfPhysicalMemoryMB() <= memory_threshold_mb;
  }

#if defined(OS_ANDROID)
  if (base::SysInfo::AmountOfPhysicalMemoryMB() <= kDefaultMemoryThresholdMb) {
    return true;
  }
#endif

  return false;
}

// static
void SiteIsolationPolicy::ApplyPersistedIsolatedOrigins(Profile* profile) {
  // If the user turned off password-triggered isolation, don't apply any
  // stored isolated origins, but also don't clear them from prefs, so that
  // they can be used if password-triggered isolation is re-enabled later.
  if (!IsIsolationForPasswordSitesEnabled())
    return;

  std::vector<url::Origin> origins;
  for (const auto& value :
       *profile->GetPrefs()->GetList(prefs::kUserTriggeredIsolatedOrigins)) {
    origins.push_back(url::Origin::Create(GURL(value.GetString())));
  }

  if (!origins.empty()) {
    auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
    using IsolatedOriginSource =
        content::ChildProcessSecurityPolicy::IsolatedOriginSource;
    policy->AddIsolatedOrigins(origins, IsolatedOriginSource::USER_TRIGGERED,
                               /* browser_context = */ profile);
  }

  UMA_HISTOGRAM_COUNTS_1000(
      "SiteIsolation.SavedUserTriggeredIsolatedOrigins.Size", origins.size());
}

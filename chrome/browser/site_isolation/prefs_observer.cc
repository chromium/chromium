// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_isolation/prefs_observer.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/site_isolation/site_isolation_policy.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/site_isolation_policy.h"

SiteIsolationPrefsObserver::SiteIsolationPrefsObserver(
    PrefService* pref_service) {
  pref_change_registrar_.Init(pref_service);

  // Start listening for pref change notifications.
  //
  // base::Unretained is okay below, because |pref_change_registrar_|'s lifetime
  // is owned by (and shorter than) |this|.
  pref_change_registrar_.Add(
      prefs::kIsolateOrigins,
      base::BindRepeating(
          &SiteIsolationPrefsObserver::OnChangeInIsolatedOriginsPref,
          base::Unretained(this)));

  // Make sure that not only *future* changes of prefs are applied, but that
  // also the *current* state of prefs is applied.
  OnChangeInIsolatedOriginsPref();
}

void SiteIsolationPrefsObserver::OnChangeInIsolatedOriginsPref() {
  // Don't do anything if the policy was removed or shouldn't apply.
  if (!pref_change_registrar_.prefs()->HasPrefPath(prefs::kIsolateOrigins))
    return;
  if (!SiteIsolationPolicy::IsEnterprisePolicyApplicable())
    return;

  // Add isolated origins based on the policy.  Note that the policy may only
  // *add* origins (e.g. if policy changes from isolating A,B,C to isolating
  // B,C,D origins then *all* of A,B,C,D will be isolated until the next Chrome
  // restart).
  std::string isolated_origins =
      pref_change_registrar_.prefs()->GetString(prefs::kIsolateOrigins);
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  policy->AddIsolatedOrigins(
      isolated_origins,
      content::ChildProcessSecurityPolicy::IsolatedOriginSource::POLICY,
      /* browser_context = */ nullptr);
}

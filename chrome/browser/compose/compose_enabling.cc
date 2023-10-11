// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/compose_enabling.h"

#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/compose/buildflags.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

bool ComposeEnabling::IsEnabledForProfile(Profile* profile) {
#if BUILDFLAG(ENABLE_COMPOSE)
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return IsEnabled(profile, identity_manager);
#else
  return false;
#endif
}

bool ComposeEnabling::IsEnabled(Profile* profile,
                                signin::IdentityManager* identity_manager) {
  if (profile == nullptr || identity_manager == nullptr) {
    return false;
  }

  // Check that the feature flag is enabled.
  if (!base::FeatureList::IsEnabled(compose::features::kEnableCompose)) {
    DVLOG(2) << "feature not enabled " << __func__;
    return false;
  }

  // Check MSBB.
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper> helper =
      unified_consent::UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(profile->GetPrefs());
  if (helper != nullptr && !helper->IsEnabled()) {
    DVLOG(2) << "MSBB not enabled " << __func__;
    return false;
  }

  // Check signin status.
  CoreAccountInfo core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (core_account_info.IsEmpty()) {
    DVLOG(2) << "user not signed in " << __func__;
    return false;
  }

  // TODO(b/300974056): Check with optization guide (age, labs).

  return true;
}

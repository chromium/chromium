// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/companion_permission_utils.h"

#include "chrome/browser/companion/core/features.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

namespace companion {

bool IsUserPermittedToSharePageURLWithCompanion(PrefService* pref_service) {
  // Sharing page content is more permissive than sharing page URL.
  if (IsUserPermittedToSharePageContentWithCompanion(pref_service))
    return true;

  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper> helper =
      unified_consent::UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(pref_service);
  return helper->IsEnabled();
}

bool IsUserPermittedToSharePageContentWithCompanion(PrefService* pref_service) {
  if (switches::ShouldOverrideCheckingUserPermissionsForCompanion())
    return true;

  // If the user's returned to the control group or disabled the flag, even if
  // they left the pref enabled, page contents should not be shared.
  return base::FeatureList::IsEnabled(features::kCompanionEnablePageContent) &&
         pref_service->GetBoolean(
             unified_consent::prefs::kPageContentCollectionEnabled);
}

}  // namespace companion

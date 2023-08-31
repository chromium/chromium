// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/companion_permission_utils.h"

#include "chrome/browser/companion/core/features.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

namespace companion {

bool IsUserPermittedToSharePageURLWithCompanion(PrefService* pref_service) {
  if (switches::ShouldOverrideCheckingUserPermissionsForCompanion()) {
    return true;
  }

  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper> helper =
      unified_consent::UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(pref_service);
  return helper->IsEnabled();
}

bool IsUserPermittedToSharePageInfoWithCompanion(PrefService* pref_service) {
  return IsUserPermittedToSharePageURLWithCompanion(pref_service);
  // TODO(crbug.com/1476887): Take PCO into account.
}

}  // namespace companion

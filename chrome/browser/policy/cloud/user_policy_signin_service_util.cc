// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service_util.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/policy/core/common/cloud/affiliation.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/policy/core/common/policy_loader_lacros.h"
#else
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#endif

namespace policy {

void UpdateProfileAttributesWhenSignout(Profile* profile,
                                        ProfileManager* profile_manager) {
  if (!profile_manager) {
    // Skip the update there isn't a profile manager which can happen when
    // testing.
    return;
  }

  ProfileAttributesEntry* entry =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (entry)
    entry->SetUserAcceptedAccountManagement(false);
}

std::string GetDeviceDMTokenIfAffiliated(
    const std::vector<std::string>& user_affiliation_ids) {
  if (!IsAffiliated(user_affiliation_ids,
                    g_browser_process->browser_policy_connector()
                        ->device_affiliation_ids())) {
    return std::string();
  }
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return PolicyLoaderLacros::device_dm_token();
#else
  DMToken token = BrowserDMTokenStorage::Get()->RetrieveDMToken();
  return token.is_valid() ? token.value() : std::string();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

std::string GetProfileId(Profile* profile) {
  return enterprise::ProfileIdServiceFactory::GetForProfile(profile)
      ->GetProfileId()
      .value_or("");
}

}  // namespace policy

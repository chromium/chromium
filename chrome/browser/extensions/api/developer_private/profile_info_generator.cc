// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/profile_info_generator.h"

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace extensions {

namespace developer = api::developer_private;

developer::ProfileInfo CreateProfileInfo(Profile* profile) {
  developer::ProfileInfo info;

  PrefService* prefs = profile->GetPrefs();
  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kExtensionsUIDeveloperMode);
  info.is_incognito_available = IncognitoModePrefs::GetAvailability(prefs) !=
                                policy::IncognitoModeAvailability::kDisabled;
  info.is_developer_mode_controlled_by_policy = pref->IsManaged();
  info.in_developer_mode = !info.is_child_account &&
                           prefs->GetBoolean(prefs::kExtensionsUIDeveloperMode);

  info.is_child_account =
      supervised_user::AreExtensionsPermissionsEnabled(profile);
  info.can_load_unpacked =
      ExtensionManagementFactory::GetForBrowserContext(profile)
          ->HasAllowlistedExtension();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  info.is_mv2_deprecation_notice_dismissed =
      ManifestV2ExperimentManager::Get(profile)
          ->DidUserAcknowledgeNoticeGlobally();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  return info;
}

}  // namespace extensions

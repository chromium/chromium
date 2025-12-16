// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_NEW_TAB_PAGE_UTIL_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_NEW_TAB_PAGE_UTIL_H_

#include <set>

#include "base/feature_list.h"
#include "components/ntp_tiles/tile_type.h"

class Profile;

namespace variations {
class VariationsService;
}  // namespace variations

bool IsCartModuleEnabled();
bool IsDriveModuleEnabled();
bool IsDriveModuleEnabledForProfile(bool is_managed_profile, Profile* profile);
bool IsEnUSLocaleOnlyFeatureEnabled(const base::Feature& ntp_feature);
bool IsFeatureEnabled(const base::Feature& feature);
bool IsFeatureForceEnabled(const base::Feature& feature);
bool IsGoogleCalendarModuleEnabled(bool is_managed_profile, Profile* profile);
bool IsMicrosoftFilesModuleEnabledForProfile(Profile* profile);
bool IsMostRelevantTabResumeModuleEnabled(Profile* profile);
bool IsOutlookCalendarModuleEnabledForProfile(Profile* profile);
bool IsMicrosoftModuleEnabledForProfile(Profile* profile);
bool IsProfileSignedIn(Profile* profile);

// Return the country code as provided by the variations service.
std::string GetVariationsServiceCountryCode(
    variations::VariationsService* variations_service);
void LogModuleEnablement(const base::Feature& feature,
                         bool enabled,
                         const std::string& reason);
void LogModuleDismissed(const base::Feature& feature,
                        bool enabled,
                        const std::string& remaining_hours);
void LogModuleError(const base::Feature& feature,
                    const std::string& error_message);

bool IsTopSitesEnabled(Profile* profile);
bool IsCustomLinksEnabled(Profile* profile);
bool IsEnterpriseShortcutsEnabled(Profile* profile);
bool IsPersonalShortcutsVisible(Profile* profile);
// Returns the set of enabled NTP tile types.
std::set<ntp_tiles::TileType> GetEnabledTileTypes(Profile* profile);

// Sets the kNtpShortcutsAutoRemovalDisabled pref to true for the module.
void DisableShortcutsAutoRemoval(Profile* profile);

// Sets the kNtpModulesAutoRemovalDisabledDict pref to true for the module.
void DisableModuleAutoRemoval(Profile* profile, const std::string& module_id);

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_NEW_TAB_PAGE_UTIL_H_

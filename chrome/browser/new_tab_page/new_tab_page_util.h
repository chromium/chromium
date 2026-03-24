// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_NEW_TAB_PAGE_UTIL_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_NEW_TAB_PAGE_UTIL_H_

#include <set>
#include <vector>

#include "base/feature_list.h"
#include "base/values.h"
#include "components/ntp_tiles/tile_type.h"

class Profile;

namespace variations {
class VariationsService;
}  // namespace variations

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(NtpShortcutsAutoRemovalReason)
enum class NtpShortcutsAutoRemovalReason {
  kManagedPreference = 0,
  kNotVisible = 1,
  kDisabled = 2,
  kStaleDaysCount = 3,
  kMaxValue = kStaleDaysCount,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/new_tab_page/enums.xml:NtpShortcutsAutoRemovalReason)

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

// Updates the staleness information for shortcuts.
void UpdateShortcutsStaleness(Profile* profile);

// Updates the staleness counters for modules iff:
// (1) It's not the first staleness update.
// (2) Time since last update is above the staleness update threshold.
// (3) Auto-removal feature is not disabled for all modules.
// (4) Auto-removal feature is not disabled for the module.
void UpdateModulesStaleness(Profile* profile,
                            const std::vector<std::string>& module_ids);

// Sets the kNtpShortcutsAutoRemovalDisabled pref to true for the module.
void DisableShortcutsAutoRemoval(Profile* profile);

// Sets the kNtpModulesAutoRemovalDisabledDict pref to true for the module.
void DisableModuleAutoRemoval(Profile* profile, const std::string& module_id);

// Sets the kNtpModulesAutoRemovalDisabledDict pref to true for the modules.
void DisableModuleListAutoRemoval(Profile* profile,
                                  const std::vector<std::string>& module_ids);

// Logs the auto-removal metrics for shortcuts, including:
// (1) The reason the shortcuts were not auto-removed
// (2) The new staleness count for the shortcuts if applicable
void RecordShortcutsAutoRemovalMetrics(Profile* profile, int prev_count);

// Logs the auto-removal metrics for the module, including:
// (1) The reason the module was not auto-removed
// (2) The new staleness count for the module if applicable
void RecordModuleAutoRemovalMetrics(
    Profile* profile,
    const base::DictValue& auto_removal_disabled_dict,
    const std::string& module_id,
    const int prev_count);

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_NEW_TAB_PAGE_UTIL_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_MODULES_CONSTANTS_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_MODULES_CONSTANTS_H_

#include <array>

namespace ntp_modules {

inline constexpr char kDriveModuleId[] = "drive";
inline constexpr char kDummyModuleId[] = "dummy";
inline constexpr char kFeedModuleId[] = "feed";
inline constexpr char kGoogleCalendarModuleId[] = "google_calendar";
inline constexpr char kMostRelevantTabResumptionModuleId[] = "tab_resumption";
inline constexpr char kMicrosoftAuthenticationModuleId[] =
    "microsoft_authentication";
inline constexpr char kOutlookCalendarModuleId[] = "outlook_calendar";
inline constexpr char kMicrosoftFilesModuleId[] = "microsoft_files";
inline constexpr char kTabGroupsModuleId[] = "tab_groups";

// All modules that require successful Microsoft authentication before being
// loaded.
inline constexpr std::array<const char*, 2> kMicrosoftAuthDependentModuleIds = {
    kMicrosoftFilesModuleId,
    kOutlookCalendarModuleId,
};

// Default order for modules.
inline constexpr std::array<const char*, 6> kOrderedModuleIds = {
    ntp_modules::kMicrosoftAuthenticationModuleId,
    ntp_modules::kOutlookCalendarModuleId,
    ntp_modules::kMicrosoftFilesModuleId,
    ntp_modules::kGoogleCalendarModuleId,
    ntp_modules::kDriveModuleId,
    ntp_modules::kMostRelevantTabResumptionModuleId};

}  // namespace ntp_modules

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_MODULES_CONSTANTS_H_

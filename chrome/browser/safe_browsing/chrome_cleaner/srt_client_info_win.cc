// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/srt_client_info_win.h"

#include <algorithm>
#include <vector>

#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/channel_info.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/version_info/version_info.h"

namespace safe_browsing {

int ChannelAsInt() {
  switch (chrome::GetChannel()) {
    case version_info::Channel::UNKNOWN:
      return 0;
    case version_info::Channel::CANARY:
      return 1;
    case version_info::Channel::DEV:
      return 2;
    case version_info::Channel::BETA:
      return 3;
    case version_info::Channel::STABLE:
      return 4;
  }
  NOTREACHED();
  return 0;
}

bool SafeBrowsingExtendedReportingEnabled() {
  // Check all profiles registered with the manager.
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  return std::any_of(profiles.begin(), profiles.end(),
                     [](const Profile* profile) {
                       return IsExtendedReportingEnabled(*profile->GetPrefs());
                     });
}

bool SafeBrowsingExtendedReportingScoutEnabled() {
  std::vector<Profile*> profiles = ProfileManager::GetLastOpenedProfiles();
  return std::any_of(
      profiles.begin(), profiles.end(), [](const Profile* profile) {
        return profile && GetExtendedReportingLevel(*profile->GetPrefs()) ==
                              SBER_LEVEL_SCOUT;
      });
}

}  // namespace safe_browsing

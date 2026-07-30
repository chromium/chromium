// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_BACKGROUND_MODE_MANAGER_H_
#define CHROME_BROWSER_BACKGROUND_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_BACKGROUND_MODE_MANAGER_H_

#include "components/prefs/pref_member.h"

namespace omnibox_everywhere {

// Manages OmniboxEverywhere's background mode and status icon based on pref
// state.
class OmniboxEverywhereBackgroundModeManager {
 public:
  OmniboxEverywhereBackgroundModeManager();
  OmniboxEverywhereBackgroundModeManager(
      const OmniboxEverywhereBackgroundModeManager&) = delete;
  OmniboxEverywhereBackgroundModeManager& operator=(
      const OmniboxEverywhereBackgroundModeManager&) = delete;
  ~OmniboxEverywhereBackgroundModeManager();

 private:
  void OnPrefChanged();

  BooleanPrefMember background_mode_pref_member_;
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_BACKGROUND_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_BACKGROUND_MODE_MANAGER_H_

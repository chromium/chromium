// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/omnibox_everywhere/omnibox_everywhere_background_mode_manager.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "components/prefs/pref_service.h"

namespace omnibox_everywhere {

OmniboxEverywhereBackgroundModeManager::
    OmniboxEverywhereBackgroundModeManager() {
  CHECK(base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhere));
  CHECK(g_browser_process && g_browser_process->local_state());

  background_mode_pref_member_.Init(
      prefs::kOmniboxEverywhereBackgroundMode, g_browser_process->local_state(),
      base::BindRepeating(
          &OmniboxEverywhereBackgroundModeManager::OnPrefChanged,
          base::Unretained(this)));
}

OmniboxEverywhereBackgroundModeManager::
    ~OmniboxEverywhereBackgroundModeManager() = default;

void OmniboxEverywhereBackgroundModeManager::OnPrefChanged() {
  // TODO (b/532190282): Update browser process keep-alive and status icon based
  // on pref state.
}

}  // namespace omnibox_everywhere

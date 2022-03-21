// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_FEATURES_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_FEATURES_H_

#include "base/feature_list.h"

namespace apps::features {

extern const base::Feature kLinkCapturingUiUpdate;
extern const base::Feature kLinkCapturingInfoBar;
extern const base::Feature kIntentChipSkipsPicker;
extern const base::Feature kIntentChipAppIcon;

// Returns true if the overall link capturing UI update feature is enabled.
bool LinkCapturingUiUpdateEnabled();

// Returns true if clicking the Intent Chip should skip the Intent Picker when
// there is only one relevant app. Only returns true if
// LinkCapturingUiUpdateEnabled() returns true.
bool ShouldIntentChipSkipIntentPicker();

// Returns true if the Link Capturing Info Bar should be shown when launching a
// web app through the Intent Picker. Only returns true if
// LinkCapturingUiUpdateEnabled() returns true.
bool LinkCapturingInfoBarEnabled();

// Returns true if the Intent Chip should show the app icon for the app which
// can handle the current URL. If false, a generic icon should always be used.
// Only returns true if LinkCapturingUiUpdateEnabled() returns true.
bool AppIconInIntentChipEnabled();

}  // namespace apps::features

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_FEATURES_H_

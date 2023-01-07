// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_FEATURES_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_FEATURES_H_

#include "base/feature_list.h"

namespace apps::features {

BASE_DECLARE_FEATURE(kLinkCapturingUiUpdate);
BASE_DECLARE_FEATURE(kLinkCapturingInfoBar);
BASE_DECLARE_FEATURE(kIntentChipSkipsPicker);
BASE_DECLARE_FEATURE(kIntentChipAppIcon);
BASE_DECLARE_FEATURE(kLinkCapturingAutoDisplayIntentPicker);

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

// Returns true if the intent picker bubble should automatically display when
// navigating through a link click to a page with installed link capturing apps.
// Always returns true if LinkCapturingUiUpdateEnabled() returns false.
bool IntentPickerAutoDisplayEnabled();

}  // namespace apps::features

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_FEATURES_H_

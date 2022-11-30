// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_picker_features.h"
#include "base/feature_list.h"

namespace apps::features {

BASE_FEATURE(kLinkCapturingUiUpdate,
             "LinkCapturingUiUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLinkCapturingInfoBar,
             "LinkCapturingInfoBar",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIntentChipSkipsPicker,
             "IntentChipSkipsPicker",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIntentChipAppIcon,
             "AppIconInIntentChip",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLinkCapturingAutoDisplayIntentPicker,
             "LinkCapturingAutoDisplayIntentPicker",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool LinkCapturingUiUpdateEnabled() {
  return base::FeatureList::IsEnabled(kLinkCapturingUiUpdate);
}

bool LinkCapturingInfoBarEnabled() {
  return LinkCapturingUiUpdateEnabled() &&
         base::FeatureList::IsEnabled(kLinkCapturingInfoBar);
}

bool ShouldIntentChipSkipIntentPicker() {
  return LinkCapturingUiUpdateEnabled() &&
         base::FeatureList::IsEnabled(kIntentChipSkipsPicker);
}

bool AppIconInIntentChipEnabled() {
  return LinkCapturingUiUpdateEnabled() &&
         base::FeatureList::IsEnabled(kIntentChipAppIcon);
}

bool IntentPickerAutoDisplayEnabled() {
  return !LinkCapturingUiUpdateEnabled() ||
         base::FeatureList::IsEnabled(kLinkCapturingAutoDisplayIntentPicker);
}

}  // namespace apps::features

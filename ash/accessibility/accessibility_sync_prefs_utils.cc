// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_sync_prefs_utils.h"

#include <limits>
#include <utility>

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "components/live_caption/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "ui/base/cursor/cursor.h"

namespace ash {

//
// TODO(crbug.com/479890756): For each function below, once respective feature
// flag is enabled by default, stabilized and removed:
//
// 1/ the registration of preferences whose
// AccessibilityPrefBatchEntry::conflict_resolution_policy is set to `kNone`
// should be moved back to Accessibility::RegisterProfilePrefs(), and not be
// listed with AccessibilityPrefBatchEntry anymore.
//
// 2/ After /1/, the getter function will only list preferences that
// require a conflict resolution policy. The name of the
// function should be changed reflect it, eg
// GetSyncableAccessibilityPrefsWithConflictResolutionPolicy().
//
// 3/ Eventually, all the 3 feature flags will be enabled and stable, and the
// functions below will be combined into one.

std::vector<AccessibilityPrefBatchEntry> GetSyncableAccessibilityPrefsBatch1() {
  const uint32_t registration_flags =
      base::FeatureList::IsEnabled(features::kOsSyncAccessibilitySettingsBatch1)
          ? user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF
          : 0;

  std::vector<AccessibilityPrefBatchEntry> v;

  // Color correction.
  v.emplace_back(prefs::kAccessibilityColorCorrectionEnabled,
                 base::Value(false), ConflictResolutionPolicy::kNone,
                 registration_flags);
  v.emplace_back(prefs::kAccessibilityColorCorrectionHasBeenSetup,
                 base::Value(false), ConflictResolutionPolicy::kNone,
                 registration_flags);

  // Cursor highlight.
  v.emplace_back(prefs::kAccessibilityCursorHighlightEnabled,
                 base::Value(false), ConflictResolutionPolicy::kDialogNeeded,
                 registration_flags);

  // Cursor color.
  v.emplace_back(prefs::kAccessibilityCursorColorEnabled, base::Value(false),
                 ConflictResolutionPolicy::kNone, registration_flags);
  v.emplace_back(prefs::kAccessibilityCursorColor,
                 base::Value(static_cast<int>(ui::kDefaultCursorColor)),
                 ConflictResolutionPolicy::kNone, registration_flags);

  // Large cursor.
  v.emplace_back(prefs::kAccessibilityLargeCursorEnabled, base::Value(false),
                 ConflictResolutionPolicy::kDialogNeeded, registration_flags);
  v.emplace_back(prefs::kAccessibilityLargeCursorDipSize,
                 base::Value(ui::kDefaultLargeCursorSize),
                 ConflictResolutionPolicy::kNone, registration_flags);

  // High contrast.
  v.emplace_back(prefs::kAccessibilityHighContrastEnabled, base::Value(false),
                 ConflictResolutionPolicy::kDialogNeeded, registration_flags);
  v.emplace_back(prefs::kHighContrastAcceleratorDialogHasBeenAccepted,
                 base::Value(false), ConflictResolutionPolicy::kNone,
                 registration_flags);

  // Caret highlight.
  v.emplace_back(prefs::kAccessibilityCaretHighlightEnabled, base::Value(false),
                 ConflictResolutionPolicy::kDialogNeeded, registration_flags);
  v.emplace_back(prefs::kAccessibilityCaretBlinkInterval,
                 base::Value(kDefaultCaretBlinkIntervalMs),
                 ConflictResolutionPolicy::kNone, registration_flags);

  // Focus highlight.
  v.emplace_back(prefs::kAccessibilityFocusHighlightEnabled, base::Value(false),
                 ConflictResolutionPolicy::kDialogNeeded, registration_flags);

  return v;
}

std::vector<AccessibilityPrefBatchEntry> GetSyncableAccessibilityPrefsBatch2() {
  const uint32_t registration_flags =
      base::FeatureList::IsEnabled(features::kOsSyncAccessibilitySettingsBatch2)
          ? user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF
          : 0;

  std::vector<AccessibilityPrefBatchEntry> v;

  // Reduced animations.
  v.emplace_back(prefs::kAccessibilityReducedAnimationsEnabled,
                 base::Value(false), ConflictResolutionPolicy::kNone,
                 registration_flags);

  // Captions (custom registration).
  v.emplace_back(::prefs::kAccessibilityCaptionsTextSize,
                 base::Value(std::string()), ConflictResolutionPolicy::kNone,
                 registration_flags,
                 /*has_custom_registration=*/true);
  v.emplace_back(::prefs::kAccessibilityCaptionsTextFont,
                 base::Value(std::string()), ConflictResolutionPolicy::kNone,
                 registration_flags,
                 /*has_custom_registration=*/true);
  v.emplace_back(::prefs::kAccessibilityCaptionsTextColor,
                 base::Value(std::string()), ConflictResolutionPolicy::kNone,
                 registration_flags,
                 /*has_custom_registration=*/true);
  v.emplace_back(::prefs::kAccessibilityCaptionsTextOpacity, base::Value(100),
                 ConflictResolutionPolicy::kNone, registration_flags,
                 /*has_custom_registration=*/true);
  v.emplace_back(::prefs::kAccessibilityCaptionsBackgroundOpacity,
                 base::Value(100), ConflictResolutionPolicy::kNone,
                 registration_flags,
                 /*has_custom_registration=*/true);
  v.emplace_back(::prefs::kAccessibilityCaptionsBackgroundColor,
                 base::Value(std::string()), ConflictResolutionPolicy::kNone,
                 registration_flags,
                 /*has_custom_registration=*/true);
  v.emplace_back(::prefs::kAccessibilityCaptionsTextShadow,
                 base::Value(std::string()), ConflictResolutionPolicy::kNone,
                 registration_flags,
                 /*has_custom_registration=*/true);

  return v;
}

std::vector<AccessibilityPrefBatchEntry> GetSyncableAccessibilityPrefsBatch3() {
  const uint32_t registration_flags =
      base::FeatureList::IsEnabled(features::kOsSyncAccessibilitySettingsBatch3)
          ? user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF
          : 0;

  std::vector<AccessibilityPrefBatchEntry> v;

  // Screen magnifier.
  v.emplace_back(prefs::kAccessibilityScreenMagnifierEnabled,
                 base::Value(false), ConflictResolutionPolicy::kDialogNeeded,
                 registration_flags);
  v.emplace_back(prefs::kAccessibilityScreenMagnifierScale,
                 base::Value(std::numeric_limits<double>::min()),
                 ConflictResolutionPolicy::kLocalClobberRemote,
                 registration_flags);
  v.emplace_back(prefs::kScreenMagnifierAcceleratorDialogHasBeenAccepted,
                 base::Value(false), ConflictResolutionPolicy::kNone,
                 registration_flags);

  // Select to speak.
  v.emplace_back(prefs::kAccessibilitySelectToSpeakEnabled, base::Value(false),
                 ConflictResolutionPolicy::kDialogNeeded, registration_flags);
  v.emplace_back(prefs::kSelectToSpeakAcceleratorDialogHasBeenAccepted,
                 base::Value(false), ConflictResolutionPolicy::kNone,
                 registration_flags);

  // Docked magnifier (custom registration).
  v.emplace_back(prefs::kDockedMagnifierEnabled, base::Value(false),
                 ConflictResolutionPolicy::kDialogNeeded, registration_flags,
                 /*has_custom_registration=*/true);
  constexpr float kDefaultMagnifierScale = 4.0f;
  v.emplace_back(
      prefs::kDockedMagnifierScale, base::Value(kDefaultMagnifierScale),
      ConflictResolutionPolicy::kLocalClobberRemote, registration_flags,
      /*has_custom_registration=*/true);
  v.emplace_back(prefs::kDockedMagnifierAcceleratorDialogHasBeenAccepted,
                 base::Value(false), ConflictResolutionPolicy::kNone,
                 registration_flags);

  return v;
}

std::vector<AccessibilityPrefBatchEntry>
GetAccessibilityPrefBatchesWithSyncEnabled() {
  std::vector<AccessibilityPrefBatchEntry> prefs;

  if (base::FeatureList::IsEnabled(
          features::kOsSyncAccessibilitySettingsBatch1)) {
    auto batch = GetSyncableAccessibilityPrefsBatch1();
    std::move(batch.begin(), batch.end(), std::back_inserter(prefs));
  }

  if (base::FeatureList::IsEnabled(
          features::kOsSyncAccessibilitySettingsBatch2)) {
    auto batch = GetSyncableAccessibilityPrefsBatch2();
    std::move(batch.begin(), batch.end(), std::back_inserter(prefs));
  }

  if (base::FeatureList::IsEnabled(
          features::kOsSyncAccessibilitySettingsBatch3)) {
    auto batch = GetSyncableAccessibilityPrefsBatch3();
    std::move(batch.begin(), batch.end(), std::back_inserter(prefs));
  }

  return prefs;
}

}  // namespace ash

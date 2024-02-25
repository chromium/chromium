// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_HOLDING_SPACE_WALLPAPER_NUDGE_HOLDING_SPACE_WALLPAPER_NUDGE_METRICS_H_
#define ASH_USER_EDUCATION_HOLDING_SPACE_WALLPAPER_NUDGE_HOLDING_SPACE_WALLPAPER_NUDGE_METRICS_H_

#include <string>

#include "ash/ash_export.h"
#include "base/containers/enum_set.h"
#include "base/time/time.h"

namespace ash::holding_space_wallpaper_nudge_metrics {

// Enums -----------------------------------------------------------------------

// Enumeration of reasons a user may be deemed ineligible for the nudge. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class IneligibleReason {
  kMinValue = 0,
  kManagedAccount = kMinValue,
  kUserNewnessNotAvailable = 1,
  kUserNotNewCrossDevice = 2,
  kUserNotNewLocally = 3,
  kUserTypeNotRegular = 4,
  kMaxValue = kUserTypeNotRegular,
};

// Enumeration of interactions users may engage in after the Holding Space
// wallpaper nudge. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused. Be sure to update
// `kAllInteractionsSet` accordingly.
enum class Interaction {
  kMinValue = 0,
  kDroppedFileOnHoldingSpace = kMinValue,
  kDroppedFileOnWallpaper = 1,
  kDraggedFileOverWallpaper = 2,
  kOpenedHoldingSpace = 3,
  kPinnedFileFromAnySource = 4,
  kPinnedFileFromContextMenu = 5,
  kPinnedFileFromFilesApp = 6,
  kPinnedFileFromHoldingSpaceDrop = 7,
  kPinnedFileFromPinButton = 8,
  kPinnedFileFromWallpaperDrop = 9,
  kUsedOtherItem = 10,
  kUsedPinnedItem = 11,
  kMaxValue = kUsedPinnedItem,
};

static constexpr auto kAllInteractionsSet =
    base::EnumSet<Interaction, Interaction::kMinValue, Interaction::kMaxValue>({
        Interaction::kDroppedFileOnHoldingSpace,
        Interaction::kDroppedFileOnWallpaper,
        Interaction::kDraggedFileOverWallpaper,
        Interaction::kOpenedHoldingSpace,
        Interaction::kPinnedFileFromAnySource,
        Interaction::kPinnedFileFromContextMenu,
        Interaction::kPinnedFileFromFilesApp,
        Interaction::kPinnedFileFromHoldingSpaceDrop,
        Interaction::kPinnedFileFromPinButton,
        Interaction::kPinnedFileFromWallpaperDrop,
        Interaction::kUsedOtherItem,
        Interaction::kUsedPinnedItem,
    });

// Enumeration of reasons a the nudge may be suppressed. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class SuppressedReason {
  kMinValue = 0,
  kCountLimitReached = kMinValue,
  kNotPrimaryAccount = 1,
  kTimePeriod = 2,
  kUserHasPinned = 3,
  kUserNotEligible = 4,
  kMaxValue = kUserNotEligible,
};

// Utilities -------------------------------------------------------------------

// Records metrics concerning the user's first pinning action.
ASH_EXPORT void RecordFirstPin();

// Records metrics concerning the user performing the given `interaction`.
ASH_EXPORT void RecordInteraction(Interaction interaction);

// Records that the nudge was shown for the given `duration`.
ASH_EXPORT void RecordNudgeDuration(base::TimeDelta duration);

// Records that the nudge was shown or would have been shown, if counterfactual.
ASH_EXPORT void RecordNudgeShown();

// Records that the nudge was suppressed or would have been suppressed, if
// counterfactual.
ASH_EXPORT void RecordNudgeSuppressed(SuppressedReason reason);

// Records that the user was determined to be eligible for the nudge if `reason`
// is `std::nullopt`, or was ineligible for the nudge for the given `reason`.
ASH_EXPORT void RecordUserEligibility(std::optional<IneligibleReason> reason);

// Returns a string representation of the given `interaction`.
ASH_EXPORT std::string ToString(Interaction interaction);

}  // namespace ash::holding_space_wallpaper_nudge_metrics

#endif  // ASH_USER_EDUCATION_HOLDING_SPACE_WALLPAPER_NUDGE_HOLDING_SPACE_WALLPAPER_NUDGE_METRICS_H_

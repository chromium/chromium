// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_metrics.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_prefs.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/user_education/common/help_bubble_params.h"

namespace ash::holding_space_wallpaper_nudge_metrics {
namespace {

// Helpers ---------------------------------------------------------------------

PrefService* GetLastActiveUserPrefService() {
  return Shell::Get()->session_controller()->GetLastActiveUserPrefService();
}

}  // namespace

// Utilities -------------------------------------------------------------------

void RecordFirstPin() {
  CHECK(features::IsHoldingSpaceWallpaperNudgeEnabled());

  auto* prefs = GetLastActiveUserPrefService();
  if (!prefs) {
    return;
  }

  // This metric should only be recorded for users who were eligible to see the
  // nudge in the first place.
  const auto first_session_time =
      holding_space_wallpaper_nudge_prefs::GetTimeOfFirstEligibleSession(prefs);
  if (!first_session_time.has_value()) {
    return;
  }

  auto nudge_shown_count =
      holding_space_wallpaper_nudge_prefs::GetNudgeShownCount(prefs);

  base::UmaHistogramExactLinear(
      "Ash.HoldingSpaceWallpaperNudge.ShownBeforeFirstPin", nudge_shown_count,
      /*exclusive_max=*/4u);
}

void RecordInteraction(Interaction interaction) {
  CHECK(features::IsHoldingSpaceWallpaperNudgeEnabled());

  auto* prefs = GetLastActiveUserPrefService();
  if (!prefs) {
    return;
  }

  // These metrics can and should only be recorded for users who were eligible
  // to see the nudge in the first place.
  const auto first_session_time =
      holding_space_wallpaper_nudge_prefs::GetTimeOfFirstEligibleSession(prefs);
  if (!first_session_time.has_value()) {
    return;
  }

  base::UmaHistogramEnumeration(
      "Ash.HoldingSpaceWallpaperNudge.Interaction.Count", interaction);

  if (holding_space_wallpaper_nudge_prefs::MarkTimeOfFirstInteraction(
          prefs, interaction)) {
    const auto now = base::Time::Now();
    const auto time_delta = now - first_session_time.value();

    // Record high fidelity `time_delta`.
    base::UmaHistogramCustomTimes(
        base::StrCat({"Ash.HoldingSpaceWallpaperNudge.Interaction.FirstTime.",
                      ToString(interaction)}),
        time_delta, /*min=*/base::Seconds(1), /*max=*/base::Days(3),
        /*buckets=*/100);

    // Record high readability time bucket.
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"Ash.HoldingSpaceWallpaperNudge.Interaction.FirstTimeBucket.",
             ToString(interaction)}),
        user_education_util::GetTimeBucket(time_delta));
  }
}

void RecordNudgeDuration(base::TimeDelta duration) {
  CHECK(features::IsHoldingSpaceWallpaperNudgeEnabled());

  base::UmaHistogramCustomTimes("Ash.HoldingSpaceWallpaperNudge.Duration",
                                duration, /*min=*/base::Milliseconds(100),
                                /*max=*/base::Seconds(10), /*buckets=*/50);
}

void RecordNudgeShown() {
  CHECK(features::IsHoldingSpaceWallpaperNudgeEnabled());

  auto* prefs = GetLastActiveUserPrefService();
  if (!prefs) {
    return;
  }

  auto nudge_shown_count =
      holding_space_wallpaper_nudge_prefs::GetNudgeShownCount(prefs);

  base::UmaHistogramExactLinear("Ash.HoldingSpaceWallpaperNudge.Shown",
                                nudge_shown_count, /*exclusive_max=*/4u);
}

void RecordNudgeSuppressed(SuppressedReason reason) {
  CHECK(features::IsHoldingSpaceWallpaperNudgeEnabled());

  base::UmaHistogramEnumeration(
      "Ash.HoldingSpaceWallpaperNudge.SuppressedReason", reason);
}

void RecordUserEligibility(std::optional<IneligibleReason> reason) {
  CHECK(features::IsHoldingSpaceWallpaperNudgeEnabled());

  base::UmaHistogramBoolean("Ash.HoldingSpaceWallpaperNudge.Eligible",
                            !reason.has_value());

  if (reason.has_value()) {
    base::UmaHistogramEnumeration(
        "Ash.HoldingSpaceWallpaperNudge.IneligibleReason", reason.value());
  }
}

std::string ToString(Interaction interaction) {
  switch (interaction) {
    case Interaction::kDroppedFileOnHoldingSpace:
      return "DroppedFileOnHoldingSpace";
    case Interaction::kDroppedFileOnWallpaper:
      return "DroppedFileOnWallpaper";
    case Interaction::kDraggedFileOverWallpaper:
      return "DraggedFileOverWallpaper";
    case Interaction::kOpenedHoldingSpace:
      return "OpenedHoldingSpace";
    case Interaction::kPinnedFileFromAnySource:
      return "PinnedFileFromAnySource";
    case Interaction::kPinnedFileFromContextMenu:
      return "PinnedFileFromContextMenu";
    case Interaction::kPinnedFileFromFilesApp:
      return "PinnedFileFromFilesApp";
    case Interaction::kPinnedFileFromHoldingSpaceDrop:
      return "PinnedFileFromHoldingSpaceDrop";
    case Interaction::kPinnedFileFromPinButton:
      return "PinnedFileFromPinButton";
    case Interaction::kPinnedFileFromWallpaperDrop:
      return "PinnedFileFromWallpaperDrop";
    case Interaction::kUsedOtherItem:
      return "UsedOtherItem";
    case Interaction::kUsedPinnedItem:
      return "UsedPinnedItem";
  }
  NOTREACHED_NORETURN();
}

}  // namespace ash::holding_space_wallpaper_nudge_metrics

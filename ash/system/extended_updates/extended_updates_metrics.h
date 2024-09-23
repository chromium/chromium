// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_EXTENDED_UPDATES_EXTENDED_UPDATES_METRICS_H_
#define ASH_SYSTEM_EXTENDED_UPDATES_EXTENDED_UPDATES_METRICS_H_

#include <string_view>

#include "ash/ash_export.h"

namespace ash {

inline constexpr std::string_view kExtendedUpdatesDialogEventMetric =
    "Ash.ExtendedUpdates.DialogEvent";
inline constexpr std::string_view kExtendedUpdatesEntryPointEventMetric =
    "Ash.ExtendedUpdates.EntryPointEvent";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ExtendedUpdatesDialogEvent)
enum class ExtendedUpdatesDialogEvent {
  kDialogShown = 0,
  kOptInConfirmed = 1,
  kMaxValue = kOptInConfirmed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ash/enums.xml:ExtendedUpdatesDialogEventEnum)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ExtendedUpdatesEntryPointEvent)
enum class ExtendedUpdatesEntryPointEvent {
  kSettingsSetUpButtonShown = 0,
  kSettingsSetUpButtonClicked = 1,
  kQuickSettingsBannerShown = 2,
  kQuickSettingsBannerClicked = 3,
  kNoArcNotificationShown = 4,
  kNoArcNotificationClicked = 5,
  kMaxValue = kNoArcNotificationClicked,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ash/enums.xml:ExtendedUpdatesEntryPointEventEnum)

ASH_EXPORT void RecordExtendedUpdatesDialogEvent(
    ExtendedUpdatesDialogEvent event);

ASH_EXPORT void RecordExtendedUpdatesEntryPointEvent(
    ExtendedUpdatesEntryPointEvent event);

}  // namespace ash

#endif  // ASH_SYSTEM_EXTENDED_UPDATES_EXTENDED_UPDATES_METRICS_H_

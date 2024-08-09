// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_METRICS_H_
#define ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_METRICS_H_

#include <string>

#include "ash/ash_export.h"
#include "base/containers/enum_set.h"

class PrefService;

namespace base {
class TimeDelta;
}  // namespace base

namespace ash::welcome_tour_metrics {

// Enums -----------------------------------------------------------------------

// Enumeration of reasons the Welcome Tour may be aborted. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class AbortedReason {
  kMinValue = 0,
  kUnknown = kMinValue,
  kAccelerator = 1,
  kChromeVoxEnabled = 2,
  kTabletModeEnabled = 3,
  kUserDeclinedTour = 4,
  kShutdown = 5,
  kMaxValue = kShutdown,
};

// Enumeration of when ChromeVox is enabled in the Welcome Tour. These values
// are persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class ChromeVoxEnabled {
  kMinValue = 0,
  kBeforeTour = kMinValue,
  kDuringTour = 1,
  kMaxValue = kDuringTour,
};

// Enumeration of the experimental arm in which the user was active when an
// attempt was made to show the Welcome Tour. These values are persisted to
// logs. Entries should not be renumbered and numeric values should never be
// reused.
enum class ExperimentalArm {
  kMinValue = 0,
  kHoldback = kMinValue,
  kV1 = 1,
  kV2 = 2,
  kMaxValue = kV2,
};

static constexpr auto kAllExperimentalArmsSet =
    base::EnumSet<ExperimentalArm,
                  ExperimentalArm::kMinValue,
                  ExperimentalArm::kMaxValue>({
        ExperimentalArm::kHoldback,
        ExperimentalArm::kV1,
        ExperimentalArm::kV2,
    });

// Enumeration of interactions users may engage in after the Welcome Tour. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused. Be sure to update `kAllInteractionsSet`
// accordingly.
enum class Interaction {
  kMinValue = 0,
  kFilesApp = kMinValue,
  kLauncher = 1,
  kQuickSettings = 2,
  kSearch = 3,
  kSettingsApp = 4,
  kExploreApp = 5,
  kMaxValue = kExploreApp,
};

static constexpr auto kAllInteractionsSet =
    base::EnumSet<Interaction, Interaction::kMinValue, Interaction::kMaxValue>({
        Interaction::kExploreApp,
        Interaction::kFilesApp,
        Interaction::kLauncher,
        Interaction::kQuickSettings,
        Interaction::kSearch,
        Interaction::kSettingsApp,
    });

// Enumeration of reasons the Welcome Tour may be prevented. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused. Be sure to update `kAllPreventedReasonsSet` accordingly.
enum class PreventedReason {
  kMinValue = 0,
  kUnknown = kMinValue,
  kChromeVoxEnabled = 1,
  // kCounterfactualExperimentArm = 2, Deprecated.
  kManagedAccount = 3,
  kTabletModeEnabled = 4,
  kUserNewnessNotAvailable = 5,
  kUserNotNewCrossDevice = 6,
  kUserTypeNotRegular = 7,
  kUserNotNewLocally = 8,
  kHoldbackExperimentArm = 9,
  kMaxValue = kHoldbackExperimentArm,
};

static constexpr auto kAllPreventedReasonsSet =
    base::EnumSet<PreventedReason,
                  PreventedReason::kMinValue,
                  PreventedReason::kMaxValue>({
        PreventedReason::kUnknown,
        PreventedReason::kChromeVoxEnabled,
        PreventedReason::kManagedAccount,
        PreventedReason::kTabletModeEnabled,
        PreventedReason::kUserNewnessNotAvailable,
        PreventedReason::kUserNotNewCrossDevice,
        PreventedReason::kUserTypeNotRegular,
        PreventedReason::kUserNotNewLocally,
        PreventedReason::kHoldbackExperimentArm,
    });

// Enumeration of steps in the Welcome Tour. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class Step {
  kMinValue = 0,
  kDialog = kMinValue,
  kExploreApp = 1,
  kExploreAppWindow = 2,
  kHomeButton = 3,
  kSearch = 4,
  kSettingsApp = 5,
  kShelf = 6,
  kStatusArea = 7,
  kFilesApp = 8,
  kMaxValue = kFilesApp,
};

// Enumeration of results recorded for each attempt to show the Welcome Tour to
// the user. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class TourResult {
  kMinValue = 0,
  kAborted = kMinValue,
  kCompleted = 1,
  // kHoldback = 2, Deprecated.
  kMaxValue = kCompleted,
};

// Utilities -------------------------------------------------------------------

// Attempts to activate an experimental arm if and only if the user was
// previously active in an experimental arm during the first attempt to show the
// Welcome Tour.
ASH_EXPORT void MaybeActivateExperimentalArm(PrefService* prefs);

// Record the experimental arm in which the user was active when the first
// attempt was made to show the Welcome Tour.
ASH_EXPORT void MaybeRecordExperimentalArm(PrefService* prefs);

// Record the usage of ChromeVox in the Welcome Tour.
ASH_EXPORT void RecordChromeVoxEnabled(ChromeVoxEnabled when);

// Record that a given `interaction` has occurred.
ASH_EXPORT void RecordInteraction(PrefService* prefs, Interaction interaction);

// Record that the given `step` of the Welcome Tour was aborted.
ASH_EXPORT void RecordStepAborted(Step step);

// Record the `duration` that a `step` of the Welcome Tour was shown.
ASH_EXPORT void RecordStepDuration(Step step, base::TimeDelta duration);

// Record that the given `step` of the Welcome Tour was shown.
ASH_EXPORT void RecordStepShown(Step step);

// Record that the Welcome Tour was aborted for the given `reason`.
ASH_EXPORT void RecordTourAborted(AbortedReason reason);

// Record the `duration` of the Welcome Tour as a whole. If the tour was not
// fully completed, `completed` should be false.
ASH_EXPORT void RecordTourDuration(PrefService* prefs,
                                   base::TimeDelta duration,
                                   bool completed);

// Record that the Welcome Tour was prevented for the given `reason`.
ASH_EXPORT void RecordTourPrevented(PrefService* prefs, PreventedReason reason);

// Record the result for an attempt to show the Welcome Tour to the user.
ASH_EXPORT void RecordTourResult(TourResult result);

// Returns a string representation of the given `interaction`.
ASH_EXPORT std::string ToString(Interaction interaction);

// Returns a string representation of the given `step`.
ASH_EXPORT std::string ToString(Step step);

}  // namespace ash::welcome_tour_metrics

#endif  // ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_METRICS_H_

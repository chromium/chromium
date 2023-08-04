// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_METRICS_H_
#define ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_METRICS_H_

#include "ash/ash_export.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ash::welcome_tour_metrics {

// Enums -----------------------------------------------------------------------

// Enumeration of reasons the Welcome Tour may be aborted. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class AbortedReason {
  kUnknown,
  kMaxValue = kUnknown,
};

// Enumeration of reasons the Welcome Tour may be prevented. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class PreventedReason {
  kUnknown = 0,
  kChromeVoxEnabled = 1,
  kCounterfactualExperimentArm = 2,
  kManagedAccount = 3,
  kTabletModeEnabled = 4,
  kUserNewnessNotAvailable = 5,
  kUserNotNew = 6,
  kUserTypeNotRegular = 7,
  kMaxValue = kUserTypeNotRegular,
};

// Enumeration of steps in the Welcome Tour. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class Step {
  kDialog,
  kExploreApp,
  kExploreAppWindow,
  kHomeButton,
  kSearch,
  kSettingsApp,
  kShelf,
  kStatusArea,
  kMaxValue = kStatusArea,
};

// Enumeration of interactions users may engage in after the Welcome Tour. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class Interaction {
  kExploreApp,
  kFilesApp,
  kLauncher,
  kQuickSettings,
  kSearch,
  kSettingsApp,
  kMaxValue = kSettingsApp,
};

// Utilities -------------------------------------------------------------------

// Record that the given `step` of the Welcome Tour was aborted.
ASH_EXPORT void RecordStepAborted(Step step);

// Record the `duration` that a `step` of the Welcome Tour was shown.
ASH_EXPORT void RecordStepDuration(Step step, base::TimeDelta duration);

// Record that the given `step` of the Welcome Tour was shown.
ASH_EXPORT void RecordStepShown(Step step);

// Record the time to first occurrence of a given `interaction`. This should
// be measured from the time the user is first able to interact in the intended
// way, i.e. after the Welcome Tour is ended or prevented.
ASH_EXPORT void RecordTimeToInteraction(Interaction interaction,
                                        base::TimeDelta delta);

// Record that the Welcome Tour was aborted for the given `reason`.
ASH_EXPORT void RecordTourAborted(AbortedReason reason);

// Record the `duration` of the Welcome Tour as a whole. If the tour was not
// fully completed, `completed` should be false.
ASH_EXPORT void RecordTourDuration(base::TimeDelta duration, bool completed);

// Record that the Welcome Tour was prevented for the given `reason`.
ASH_EXPORT void RecordTourPrevented(PreventedReason reason);

}  // namespace ash::welcome_tour_metrics

#endif  // ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_METRICS_H_

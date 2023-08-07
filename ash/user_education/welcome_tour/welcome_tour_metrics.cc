// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"

namespace ash::welcome_tour_metrics {
namespace {

// These strings are persisted to logs. These string values should never be
// changed or reused. Any values added to `Step` must be added here.
std::string ToString(Step step) {
  switch (step) {
    case Step::kDialog:
      return "Dialog";
    case Step::kExploreApp:
      return "ExploreApp";
    case Step::kExploreAppWindow:
      return "ExploreAppWindow";
    case Step::kHomeButton:
      return "HomeButton";
    case Step::kSearch:
      return "Search";
    case Step::kSettingsApp:
      return "SettingsApp";
    case Step::kShelf:
      return "Shelf";
    case Step::kStatusArea:
      return "StatusArea";
  }
  NOTREACHED_NORETURN();
}

}  // namespace

void RecordStepAborted(Step step) {
  base::UmaHistogramEnumeration("Ash.WelcomeTour.Step.Aborted", step);
}

void RecordStepDuration(Step step, base::TimeDelta duration) {
  base::UmaHistogramCustomTimes(
      base::StrCat({"Ash.WelcomeTour.Step.Duration.", ToString(step)}),
      duration, /*min=*/base::Milliseconds(1), /*max=*/base::Minutes(5),
      /*buckets=*/50);
}

void RecordStepShown(Step step) {
  base::UmaHistogramEnumeration("Ash.WelcomeTour.Step.Shown", step);
}

void RecordTimeToInteraction(Interaction interaction, base::TimeDelta delta) {
  NOTIMPLEMENTED() << "Emit `Ash.WelcomeTour.TimeToInteraction."
                   << ToString(interaction) << "`.";
}

void RecordTourAborted(AbortedReason reason) {
  base::UmaHistogramEnumeration("Ash.WelcomeTour.Aborted.Reason", reason);
}

void RecordTourDuration(base::TimeDelta duration, bool completed) {
  const std::string metric_infix = completed ? "Completed" : "Aborted";
  base::UmaHistogramCustomTimes(
      base::StrCat({"Ash.WelcomeTour.", metric_infix, ".Duration"}), duration,
      /*min=*/base::Seconds(1),
      /*max=*/base::Minutes(10), /*buckets=*/50);
}

void RecordTourPrevented(PreventedReason reason) {
  base::UmaHistogramEnumeration("Ash.WelcomeTour.Prevented.Reason", reason);
}

// These strings are persisted to logs. These string values should never be
// changed or reused. Any values added to `Interaction` must be added here.
std::string ToString(Interaction interaction) {
  switch (interaction) {
    case Interaction::kFilesApp:
      return "FilesApp";
    case Interaction::kLauncher:
      return "Launcher";
    case Interaction::kQuickSettings:
      return "QuickSettings";
    case Interaction::kSearch:
      return "Search";
    case Interaction::kSettingsApp:
      return "SettingsApp";
  }
  NOTREACHED_NORETURN();
}

}  // namespace ash::welcome_tour_metrics

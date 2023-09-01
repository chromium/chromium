// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_metrics.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/user_education/welcome_tour/welcome_tour_prefs.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"

namespace ash::welcome_tour_metrics {
namespace {

// Helpers ---------------------------------------------------------------------

PrefService* GetLastActiveUserPrefService() {
  return Shell::Get()->session_controller()->GetLastActiveUserPrefService();
}

}  // namespace

void RecordInteraction(Interaction interaction) {
  CHECK(features::IsWelcomeTourEnabled());

  // Some interactions, like `kQuickSettings`, can occur before user activation.
  auto* prefs = GetLastActiveUserPrefService();
  if (!prefs) {
    return;
  }

  auto completed_time = welcome_tour_prefs::GetTimeOfFirstTourCompletion(prefs);
  auto prevented_time = welcome_tour_prefs::GetTimeOfFirstTourPrevention(prefs);
  bool prevented_counterfactually =
      welcome_tour_prefs::GetReasonForFirstTourPrevention(prefs) ==
      welcome_tour_metrics::PreventedReason::kCounterfactualExperimentArm;

  // These metrics should only be recorded for users who have completed the
  // Welcome Tour, or users who are part of the counterfactual experiment arm.
  if (!completed_time && !(prevented_time && prevented_counterfactually)) {
    return;
  }

  const std::string completion_string =
      prevented_counterfactually ? "Counterfactual" : "Completed";

  base::UmaHistogramEnumeration(
      base::StrCat(
          {"Ash.WelcomeTour.", completion_string, ".Interaction.Count"}),
      interaction);

  // Attempt to mark that this interaction happened for the first time. If it
  // succeeds, then it was, so record the relevant metric.
  if (welcome_tour_prefs::MarkTimeOfFirstInteraction(prefs, interaction)) {
    // Time to interaction should be measured from tour prevention if the tour
    // was prevented, else use the time the tour was completed.
    const auto relevant_time = prevented_time.has_value()
                                   ? prevented_time.value()
                                   : completed_time.value();
    const auto delta = base::Time::Now() - relevant_time;

    base::UmaHistogramCustomTimes(
        base::StrCat({"Ash.WelcomeTour.", completion_string,
                      ".Interaction.FirstTime.", ToString(interaction)}),
        delta, /*min=*/base::Seconds(1), /*max=*/base::Days(3),
        /*buckets=*/100);
  }
}

void RecordStepAborted(Step step) {
  CHECK(features::IsWelcomeTourEnabled());

  base::UmaHistogramEnumeration("Ash.WelcomeTour.Step.Aborted", step);
}

void RecordStepDuration(Step step, base::TimeDelta duration) {
  CHECK(features::IsWelcomeTourEnabled());

  base::UmaHistogramCustomTimes(
      base::StrCat({"Ash.WelcomeTour.Step.Duration.", ToString(step)}),
      duration, /*min=*/base::Milliseconds(1), /*max=*/base::Minutes(5),
      /*buckets=*/50);
}

void RecordStepShown(Step step) {
  CHECK(features::IsWelcomeTourEnabled());

  base::UmaHistogramEnumeration("Ash.WelcomeTour.Step.Shown", step);
}

void RecordTourAborted(AbortedReason reason) {
  CHECK(features::IsWelcomeTourEnabled());

  base::UmaHistogramEnumeration("Ash.WelcomeTour.Aborted.Reason", reason);
}

void RecordTourDuration(base::TimeDelta duration, bool completed) {
  CHECK(features::IsWelcomeTourEnabled());

  if (completed) {
    welcome_tour_prefs::MarkTimeOfFirstTourCompletion(
        GetLastActiveUserPrefService());
  }

  const std::string metric_infix = completed ? "Completed" : "Aborted";
  base::UmaHistogramCustomTimes(
      base::StrCat({"Ash.WelcomeTour.", metric_infix, ".Duration"}), duration,
      /*min=*/base::Seconds(1),
      /*max=*/base::Minutes(10), /*buckets=*/50);
}

void RecordTourPrevented(PreventedReason reason) {
  CHECK(features::IsWelcomeTourEnabled());

  welcome_tour_prefs::MarkFirstTourPrevention(GetLastActiveUserPrefService(),
                                              reason);

  base::UmaHistogramEnumeration("Ash.WelcomeTour.Prevented.Reason", reason);
}

// These strings are persisted to logs. These string values should never be
// changed or reused. Any values added to `Interaction` must be added here.
std::string ToString(Interaction interaction) {
  switch (interaction) {
    case Interaction::kExploreApp:
      return "ExploreApp";
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

}  // namespace ash::welcome_tour_metrics

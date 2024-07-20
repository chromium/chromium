// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_metrics.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "ash/user_education/welcome_tour/welcome_tour_prefs.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"

namespace ash::welcome_tour_metrics {
namespace {

// Constants -------------------------------------------------------------------

static constexpr char kWelcomeTourHistogramNamePrefix[] = "Ash.WelcomeTour.";

// Helpers ---------------------------------------------------------------------

PrefService* GetLastActiveUserPrefService() {
  return Shell::HasInstance() ? Shell::Get()
                                    ->session_controller()
                                    ->GetLastActiveUserPrefService()
                              : nullptr;
}

}  // namespace

void RecordChromeVoxEnabled(ChromeVoxEnabled when) {
  CHECK(features::IsWelcomeTourEnabled());

  base::UmaHistogramEnumeration(
      base::StrCat({kWelcomeTourHistogramNamePrefix, "ChromeVoxEnabled.When"}),
      when);
}

void RecordExperimentalArm() {
  CHECK(features::IsWelcomeTourEnabled());

  std::optional<ExperimentalArm> experimental_arm;
  if (features::IsWelcomeTourCounterfactuallyEnabled()) {
    CHECK(!features::IsWelcomeTourHoldbackEnabled());
    CHECK(!features::IsWelcomeTourV2Enabled());
    experimental_arm = ExperimentalArm::kV1;
  } else if (features::IsWelcomeTourHoldbackEnabled()) {
    CHECK(!features::IsWelcomeTourCounterfactuallyEnabled());
    CHECK(!features::IsWelcomeTourV2Enabled());
    experimental_arm = ExperimentalArm::kHoldback;
  } else if (features::IsWelcomeTourV2Enabled()) {
    CHECK(!features::IsWelcomeTourCounterfactuallyEnabled());
    CHECK(!features::IsWelcomeTourHoldbackEnabled());
    experimental_arm = ExperimentalArm::kV2;
  }

  if (experimental_arm) {
    base::UmaHistogramEnumeration(
        base::StrCat({kWelcomeTourHistogramNamePrefix, "ExperimentalArm"}),
        experimental_arm.value());
  }
}

void RecordInteraction(Interaction interaction) {
  CHECK(features::IsWelcomeTourEnabled());

  // Some interactions, like `kQuickSettings`, can occur before user activation.
  auto* prefs = GetLastActiveUserPrefService();
  if (!prefs) {
    return;
  }

  // These metrics should only be recorded for users who have attempted the
  // tour.
  const auto first_time = welcome_tour_prefs::GetTimeOfFirstTourAttempt(prefs);
  if (!first_time) {
    return;
  }

  base::UmaHistogramEnumeration(
      base::StrCat({kWelcomeTourHistogramNamePrefix, "Interaction.Count"}),
      interaction);

  // Attempt to mark that this interaction happened for the first time. If it
  // succeeds, then it was, so record the relevant metric.
  if (welcome_tour_prefs::MarkTimeOfFirstInteraction(prefs, interaction)) {
    // Time to interaction should be measured from first tour attempt.
    const auto time_delta = base::Time::Now() - first_time.value();

    // Record high fidelity `time_delta`.
    base::UmaHistogramCustomTimes(
        base::StrCat({kWelcomeTourHistogramNamePrefix, "Interaction.FirstTime.",
                      ToString(interaction)}),
        time_delta, /*min=*/base::Seconds(1), /*max=*/base::Days(3),
        /*buckets=*/100);

    // Record high readability time bucket.
    base::UmaHistogramEnumeration(
        base::StrCat({kWelcomeTourHistogramNamePrefix,
                      "Interaction.FirstTimeBucket.", ToString(interaction)}),
        user_education_util::GetTimeBucket(time_delta));
  }
}

void RecordStepAborted(Step step) {
  CHECK(features::IsWelcomeTourEnabled());

  base::UmaHistogramEnumeration(
      base::StrCat({kWelcomeTourHistogramNamePrefix, "Step.Aborted"}), step);
}

void RecordStepDuration(Step step, base::TimeDelta duration) {
  CHECK(features::IsWelcomeTourEnabled());

  base::UmaHistogramCustomTimes(
      base::StrCat(
          {kWelcomeTourHistogramNamePrefix, "Step.Duration.", ToString(step)}),
      duration, /*min=*/base::Milliseconds(1), /*max=*/base::Minutes(5),
      /*buckets=*/50);
}

void RecordStepShown(Step step) {
  CHECK(features::IsWelcomeTourEnabled());

  base::UmaHistogramEnumeration(
      base::StrCat({kWelcomeTourHistogramNamePrefix, "Step.Shown"}), step);
}

void RecordTourAborted(AbortedReason reason) {
  CHECK(features::IsWelcomeTourEnabled());

  base::UmaHistogramEnumeration(
      base::StrCat({kWelcomeTourHistogramNamePrefix, "Aborted.Reason"}),
      reason);
}

void RecordTourDuration(base::TimeDelta duration, bool completed) {
  CHECK(features::IsWelcomeTourEnabled());

  if (completed) {
    welcome_tour_prefs::MarkTimeOfFirstTourCompletion(
        GetLastActiveUserPrefService());
  } else {
    welcome_tour_prefs::MarkTimeOfFirstTourAborted(
        GetLastActiveUserPrefService());
  }

  const std::string metric_infix = completed ? "Completed" : "Aborted";
  base::UmaHistogramCustomTimes(base::StrCat({kWelcomeTourHistogramNamePrefix,
                                              metric_infix, ".Duration"}),
                                duration,
                                /*min=*/base::Seconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);
}

void RecordTourPrevented(PreventedReason reason) {
  CHECK(features::IsWelcomeTourEnabled());
  // TODO: b/345829923 - `prefs` could be nullptr in the tests.
  if (auto* prefs = GetLastActiveUserPrefService()) {
    welcome_tour_prefs::MarkFirstTourPrevention(prefs, reason);
  }

  base::UmaHistogramEnumeration(
      base::StrCat({kWelcomeTourHistogramNamePrefix, "Prevented.Reason"}),
      reason);
}

void RecordTourResult(TourResult result) {
  CHECK(features::IsWelcomeTourEnabled());

  base::UmaHistogramEnumeration(
      base::StrCat({kWelcomeTourHistogramNamePrefix, "Result"}), result);
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
    case Step::kFilesApp:
      return "FilesApp";
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

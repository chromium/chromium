// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"

namespace glic {

void RecordExperimentalOptInShown(RequiredExperimentalOptIn required_state) {
  switch (required_state) {
    case RequiredExperimentalOptIn::kGlic:
      base::RecordAction(base::UserMetricsAction(
          "Glic.ExperimentalTriggering.OptIn.Glic.Shown"));
      break;
    case RequiredExperimentalOptIn::kActuation:
      base::RecordAction(base::UserMetricsAction(
          "Glic.ExperimentalTriggering.OptIn.Actuation.Shown"));
      break;
    case RequiredExperimentalOptIn::kExperimental:
      base::RecordAction(base::UserMetricsAction(
          "Glic.ExperimentalTriggering.OptIn.Experimental.Shown"));
      break;
    case RequiredExperimentalOptIn::kNotNeeded:
      // WebUI loaded but opt-in was no longer required. This will have been
      // changed to an experimental version before actually showing, but it's
      // important we log this here & below for visibility into this case.
      base::RecordAction(base::UserMetricsAction(
          "Glic.ExperimentalTriggering.OptIn.Experimental.Shown"));
      break;
  }
  base::UmaHistogramEnumeration(
      "Glic.ExperimentalTriggering.OptIn.Shown.Version", required_state);
}

void RecordExperimentalOptInAccepted(RequiredExperimentalOptIn required_state) {
  switch (required_state) {
    case RequiredExperimentalOptIn::kGlic:
      base::RecordAction(base::UserMetricsAction(
          "Glic.ExperimentalTriggering.OptIn.Glic.Accepted"));
      break;
    case RequiredExperimentalOptIn::kActuation:
      base::RecordAction(base::UserMetricsAction(
          "Glic.ExperimentalTriggering.OptIn.Actuation.Accepted"));
      break;
    case RequiredExperimentalOptIn::kExperimental:
      base::RecordAction(base::UserMetricsAction(
          "Glic.ExperimentalTriggering.OptIn.Experimental.Accepted"));
      break;
    case RequiredExperimentalOptIn::kNotNeeded:
      NOTREACHED();
  }
  base::UmaHistogramEnumeration(
      "Glic.ExperimentalTriggering.OptIn.Accepted.Version", required_state);
}

void RecordExperimentalOptInRejected(RequiredExperimentalOptIn required_state) {
  // TODO(b/511184397): Add in a difference between dismiss & reject for the
  // experimental opt in for glic.
  switch (required_state) {
    case RequiredExperimentalOptIn::kGlic:
      base::RecordAction(base::UserMetricsAction(
          "Glic.ExperimentalTriggering.OptIn.Glic.NoThanks"));
      break;
    case RequiredExperimentalOptIn::kActuation:
      base::RecordAction(base::UserMetricsAction(
          "Glic.ExperimentalTriggering.OptIn.Actuation.NoThanks"));
      break;
    case RequiredExperimentalOptIn::kExperimental:
      base::RecordAction(base::UserMetricsAction(
          "Glic.ExperimentalTriggering.OptIn.Experimental.NoThanks"));
      break;
    case RequiredExperimentalOptIn::kNotNeeded:
      NOTREACHED();
  }
  base::UmaHistogramEnumeration(
      "Glic.ExperimentalTriggering.OptIn.NoThanks.Version", required_state);
}

}  // namespace glic

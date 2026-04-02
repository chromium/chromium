// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_registry.h"

#include <utility>

#include "chrome/browser/metrics/critical_user_journeys/features.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace metrics {

CriticalUserJourneyRegistry::CriticalUserJourneyRegistry() = default;
CriticalUserJourneyRegistry::~CriticalUserJourneyRegistry() = default;

void CriticalUserJourneyRegistry::AddJourneys() {
  AddJourney(
      CriticalUserJourney::Builder(&kViewDownloadedFileJourney)
          .AddStep(kDownloadEndedCustomEventId,
                   ui::InteractionSequence::StepType::kCustomEvent,
                   /*metric_id=*/1)
          .AddAnyOf({Branch(kToolbarDownloadBubbleElementId,
                            ui::InteractionSequence::StepType::kShown,
                            /*metric_id=*/2),
                     Branch(kToolbarDownloadButtonElementId,
                            ui::InteractionSequence::StepType::kActivated,
                            /*metric_id=*/3)})
          .AddStep(kDownloadBubbleOpenButtonId,
                   ui::InteractionSequence::StepType::kActivated,
                   /*metric_id=*/4)
          .Build());

  AddJourney(
      CriticalUserJourney::Builder(&kViewDownloadedFileFromAppMenuJourney)
          .AddStep(kDownloadEndedCustomEventId,
                   ui::InteractionSequence::StepType::kCustomEvent,
                   /*metric_id=*/1)
          .AddStep(kToolbarAppMenuButtonElementId,
                   ui::InteractionSequence::StepType::kActivated,
                   /*metric_id=*/2)
          .AddStep(AppMenuModel::kDownloadsMenuItem,
                   ui::InteractionSequence::StepType::kActivated,
                   /*metric_id=*/3)
          .AddStep(kDownloadedFileOpenedCustomEventId,
                   ui::InteractionSequence::StepType::kCustomEvent,
                   /*metric_id=*/4)
          .Build());
}

void CriticalUserJourneyRegistry::AddJourney(
    std::unique_ptr<CriticalUserJourney> journey) {
  journeys_.push_back(std::move(journey));
}

}  // namespace metrics

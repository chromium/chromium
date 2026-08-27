// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_registry.h"

#include <utility>

#include "chrome/browser/browsing_data/browsing_data_important_sites_util.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey.h"
#include "chrome/browser/metrics/critical_user_journeys/features.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/webui/settings/settings_element_ids.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace metrics {

CriticalUserJourneyRegistry::CriticalUserJourneyRegistry() = default;
CriticalUserJourneyRegistry::~CriticalUserJourneyRegistry() = default;

void CriticalUserJourneyRegistry::AddJourneys() {
  HatsParams download_hats_params;
  download_hats_params.trigger = metrics::kHatsSurveyTriggerDownloadJourney;

  HatsParams pin_extension_hats_params;
  pin_extension_hats_params.trigger =
      metrics::kHatsSurveyTriggerPinExtensionJourney;

  HatsParams clear_browsing_history_hats_params;
  clear_browsing_history_hats_params.trigger =
      metrics::kHatsSurveyTriggerClearBrowsingHistory;

  // ---------------------------------------------------------------------------
  // Existing Browser Journeys
  // ---------------------------------------------------------------------------

  AddJourney(
      CriticalUserJourney::Builder(&kViewDownloadedFileJourney)
          .AddStep(kDownloadEndedCustomEventId,
                   ui::InteractionSequence::StepType::kCustomEvent,
                   ViewDownloadedFileJourneySteps::kFinishDownload)
          .AddAnyOf(
              {Branch(kToolbarDownloadBubbleElementId,
                      ui::InteractionSequence::StepType::kShown,
                      ViewDownloadedFileJourneySteps::kDownloadBubbleAppears),
               Branch(
                   kToolbarDownloadButtonElementId,
                   ui::InteractionSequence::StepType::kActivated,
                   ViewDownloadedFileJourneySteps::kUserClickedDownloadBubble)})
          .AddStep(kDownloadBubbleOpenButtonId,
                   ui::InteractionSequence::StepType::kActivated,
                   ViewDownloadedFileJourneySteps::kClickDownloadedFile)
          .LaunchHatsSurveyOnCompletion(download_hats_params)
          .Build());

  AddJourney(
      CriticalUserJourney::Builder(&kViewDownloadedFileFromAppMenuJourney)
          .AddStep(kDownloadEndedCustomEventId,
                   ui::InteractionSequence::StepType::kCustomEvent,
                   ViewDownloadedFileFromAppMenuJourneySteps::kFinishDownload)
          .AddStep(
              kToolbarAppMenuButtonElementId,
              ui::InteractionSequence::StepType::kActivated,
              ViewDownloadedFileFromAppMenuJourneySteps::kAppMenuButtonClicked)
          .AddStep(AppMenuModel::kDownloadsMenuItem,
                   ui::InteractionSequence::StepType::kActivated,
                   ViewDownloadedFileFromAppMenuJourneySteps::
                       kDownloadsMenuItemClicked)
          .AddStep(
              kDownloadedFileOpenedCustomEventId,
              ui::InteractionSequence::StepType::kCustomEvent,
              ViewDownloadedFileFromAppMenuJourneySteps::kDownloadedFileClicked)
          .LaunchHatsSurveyOnCompletion(download_hats_params)
          .Build());

  AddJourney(
      metrics::CriticalUserJourney::Builder(&kPinExtensionJourney)
          .AddStep(kExtensionsMenuButtonElementId,
                   ui::InteractionSequence::StepType::kActivated,
                   PinExtensionJourneySteps::kExtensionsMenuButtonClicked)
          .AddAnyOf(
              {Branch(kExtensionsMenuPinExtensionsEventId,
                      PinExtensionJourneySteps::kPinExtensionsViaMenuItem),
               Branch(kExtensionsSidePanelPinExtensionsEventId,
                      PinExtensionJourneySteps::
                          kPinExtensionsViaSidePanelPinButton)})
          .LaunchHatsSurveyOnCompletion(pin_extension_hats_params)
          .Build());

  AddJourney(
      CriticalUserJourney::Builder(&kClearBrowsingHistoryJourney)
          .AddAnyOf(
              {Branch(browsing_data_important_sites_util::
                          kOpenClearBrowsingDataDialogViaAcceleratorEventId,
                      ClearBrowsingHistoryJourneySteps::
                          kOpenClearBrowsingDataDialogViaAccelerator),
               Branch(
                   kToolbarAppMenuButtonElementId,
                   ui::InteractionSequence::StepType::kActivated,
                   ClearBrowsingHistoryJourneySteps::kActivateAppMenuButton)})
          .AddStep(browsing_data_important_sites_util::
                       kShowClearBrowsingDataDialogEventId,
                   ui::InteractionSequence::StepType::kCustomEvent,
                   ClearBrowsingHistoryJourneySteps::
                       kShowClearBrowsingDataSettingsDialog)
          .AddStep(
              browsing_data_important_sites_util::
                  kClearBrowsingDataHistoryEventId,
              ui::InteractionSequence::StepType::kCustomEvent,
              ClearBrowsingHistoryJourneySteps::kClearBrowsingDataHistoryEvent)
          .LaunchHatsSurveyOnCompletion(clear_browsing_history_hats_params)
          .Build());

  // ---------------------------------------------------------------------------
  // Settings Glow Up Journeys
  // ---------------------------------------------------------------------------

  // Navigational IA Findability
  AddJourney(
      CriticalUserJourney::Builder(&kSettingsGlowupJourneys)
          .AddStep(settings::kSettingsPageLoadedId,
                   ui::InteractionSequence::StepType::kShown, 0)
          .AddStep(settings::kSettingsNavCategoryClickedId,
                   ui::InteractionSequence::StepType::kActivated, 1)
          .AddStep(settings::kSettingStateChangedEventId,
                   ui::InteractionSequence::StepType::kCustomEvent, 2)
          .Build());

  // Supercharged Search & Inline Controls
  AddJourney(
      CriticalUserJourney::Builder(&kSettingsGlowupJourneys)
          .AddStep(settings::kSettingsSearchBoxElementId,
                   ui::InteractionSequence::StepType::kShown, 0)
          .AddStep(settings::kSettingsSearchQueryEnteredId,
                   ui::InteractionSequence::StepType::kActivated, 1)
          .AddStep(settings::kSettingsSearchResultClickedEventId,
                   ui::InteractionSequence::StepType::kCustomEvent, 2)
          .Build());

  // Dedicated Search Engine & Shortcuts
  AddJourney(
      CriticalUserJourney::Builder(&kSettingsGlowupJourneys)
          .AddStep(settings::kSearchEngineNavMenuItemId,
                   ui::InteractionSequence::StepType::kActivated, 0)
          .AddStep(settings::kDefaultSearchEngineChangedId,
                   ui::InteractionSequence::StepType::kCustomEvent, 1)
          .AddStep(settings::kSearchShortcutsToggledEventId,
                   ui::InteractionSequence::StepType::kCustomEvent, 2)
          .Build());

  // Sites Dashboard & Site Permissions
  AddJourney(
      CriticalUserJourney::Builder(&kSettingsGlowupJourneys)
          .AddStep(settings::kSitesNavMenuItemId,
                   ui::InteractionSequence::StepType::kActivated, 0)
          .AddStep(settings::kSitePermissionCategoryElementId,
                   ui::InteractionSequence::StepType::kActivated, 1)
          .AddStep(settings::kSitePermissionChangedEventId,
                   ui::InteractionSequence::StepType::kCustomEvent, 2)
          .Build());

  // Clear Browsing Data Baseline
  AddJourney(
      CriticalUserJourney::Builder(&kSettingsGlowupJourneys)
          .AddStep(settings::kClearBrowsingDataElementId,
                   ui::InteractionSequence::StepType::kActivated, 0)
          .AddStep(settings::kClearBrowsingDataDialogOkButtonElementId,
                   ui::InteractionSequence::StepType::kActivated, 1)
          .Build());

  // Appearance & GM3 Themes
  AddJourney(
      CriticalUserJourney::Builder(&kSettingsGlowupJourneys)
          .AddStep(settings::kAppearanceNavMenuItemId,
                   ui::InteractionSequence::StepType::kActivated, 0)
          .AddStep(settings::kAppearanceColorTileSelectedId,
                   ui::InteractionSequence::StepType::kActivated, 1)
          .AddStep(settings::kAppearanceThemeChangedId,
                   ui::InteractionSequence::StepType::kCustomEvent, 2)
          .Build());
}

void CriticalUserJourneyRegistry::AddJourney(
    std::unique_ptr<CriticalUserJourney> journey) {
  journeys_.push_back(std::move(journey));
}

}  // namespace metrics
// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_REGISTRY_H_
#define CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_REGISTRY_H_

#include <map>
#include <memory>
#include <vector>

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey.h"
#include "ui/base/interaction/element_identifier.h"

namespace metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PinExtensionJourneySteps)
enum class PinExtensionJourneySteps {
  kExtensionsMenuButtonClicked = 1,
  kPinExtensionsViaMenuItem = 2,
  kPinExtensionsViaSidePanelPinButton = 3,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/critical_user_journeys/enums.xml:PinExtensionJourneySteps)

// LINT.IfChange(ClearBrowsingHistoryJourneySteps)
enum class ClearBrowsingHistoryJourneySteps {
  kOpenClearBrowsingDataDialogViaAccelerator = 1,
  kActivateAppMenuButton = 2,
  kShowClearBrowsingDataSettingsDialog = 3,
  kClearBrowsingDataHistoryEvent = 4,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/critical_user_journeys/enums.xml:ClearBrowsingHistoryJourneySteps)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ViewDownloadedFileFromAppMenuJourneySteps)
enum class ViewDownloadedFileFromAppMenuJourneySteps {
  kFinishDownload = 1,
  kAppMenuButtonClicked = 2,
  kDownloadsMenuItemClicked = 3,
  kDownloadedFileClicked = 4,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/critical_user_journeys/enums.xml:ViewDownloadedFileFromAppMenuJourneySteps)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ViewDownloadedFileJourneySteps)
enum class ViewDownloadedFileJourneySteps {
  kFinishDownload = 1,
  kDownloadBubbleAppears = 2,
  kUserClickedDownloadBubble = 3,
  kClickDownloadedFile = 4,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/critical_user_journeys/enums.xml:ViewDownloadedFileJourneySteps)

// Registry for all defined Critical User Journeys.
// Used to store and retrieve journey definitions by their starting element.
class CriticalUserJourneyRegistry {
 public:
  CriticalUserJourneyRegistry();
  ~CriticalUserJourneyRegistry();

  CriticalUserJourneyRegistry(const CriticalUserJourneyRegistry&) = delete;
  CriticalUserJourneyRegistry& operator=(const CriticalUserJourneyRegistry&) =
      delete;

  // Adds a journey to the registry.
  void AddJourney(std::unique_ptr<CriticalUserJourney> journey);

  // Populates the registry with all defined journeys.
  void AddJourneys();

  // Returns all registered journeys.
  const std::vector<std::unique_ptr<CriticalUserJourney>>& journeys() const {
    return journeys_;
  }

 private:
  std::vector<std::unique_ptr<CriticalUserJourney>> journeys_;
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_CRITICAL_USER_JOURNEYS_CRITICAL_USER_JOURNEY_REGISTRY_H_

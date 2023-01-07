// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_TRACKED_PREFERENCE_INCIDENT_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_TRACKED_PREFERENCE_INCIDENT_H_

#include <stdint.h>

#include <memory>

#include "chrome/browser/safe_browsing/incident_reporting/incident.h"

namespace safe_browsing {

class ClientIncidentReport_IncidentData_TrackedPreferenceIncident;

// An incident representing a tracked preference that has been modified or
// cleared.
class TrackedPreferenceIncident : public Incident {
 public:
  TrackedPreferenceIncident(
      std::unique_ptr<
          ClientIncidentReport_IncidentData_TrackedPreferenceIncident>
          tracked_preference,
      bool is_personal);

  TrackedPreferenceIncident(const TrackedPreferenceIncident&) = delete;
  TrackedPreferenceIncident& operator=(const TrackedPreferenceIncident&) =
      delete;

  ~TrackedPreferenceIncident() override;

  // Incident methods:
  IncidentType GetType() const override;
  std::string GetKey() const override;
  uint32_t ComputeDigest() const override;
  std::unique_ptr<ClientIncidentReport_IncidentData> TakePayload() override;

 private:
  bool is_personal_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_TRACKED_PREFERENCE_INCIDENT_H_

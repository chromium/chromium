// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_RESOURCE_REQUEST_INCIDENT_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_RESOURCE_REQUEST_INCIDENT_H_

#include <stdint.h>

#include <memory>

#include "chrome/browser/safe_browsing/incident_reporting/incident.h"

namespace safe_browsing {

class ClientIncidentReport_IncidentData_ResourceRequestIncident;

// Represents a suspicious script detection incident.
class ResourceRequestIncident : public Incident {
 public:
  explicit ResourceRequestIncident(
      std::unique_ptr<ClientIncidentReport_IncidentData_ResourceRequestIncident>
          script_detection_incident);

  ResourceRequestIncident(const ResourceRequestIncident&) = delete;
  ResourceRequestIncident& operator=(const ResourceRequestIncident&) = delete;

  ~ResourceRequestIncident() override;

  // Incident methods:
  IncidentType GetType() const override;
  std::string GetKey() const override;
  uint32_t ComputeDigest() const override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_RESOURCE_REQUEST_INCIDENT_H_

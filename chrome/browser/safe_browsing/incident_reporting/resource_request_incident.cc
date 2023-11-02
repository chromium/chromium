// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/resource_request_incident.h"

#include "base/check.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_handler_util.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

ResourceRequestIncident::ResourceRequestIncident(
    std::unique_ptr<ClientIncidentReport_IncidentData_ResourceRequestIncident>
        script_request_incident) {
  DCHECK(script_request_incident);
  DCHECK(script_request_incident->has_digest());
  payload()->set_allocated_resource_request(script_request_incident.release());
}

ResourceRequestIncident::~ResourceRequestIncident() {
}

IncidentType ResourceRequestIncident::GetType() const {
  return IncidentType::RESOURCE_REQUEST;
}

std::string ResourceRequestIncident::GetKey() const {
  // Use a static key in addition to a fixed digest below to ensure that only
  // one incident per user is reported.
  return "resource_request_incident";
}

uint32_t ResourceRequestIncident::ComputeDigest() const {
  // Return a constant in addition to a fixed key per resource request type
  // above to ensure that only one incident per user is reported.
  return 42;
}

}  // namespace safe_browsing

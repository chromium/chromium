// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/tracked_preference_incident.h"

#include "base/check.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_handler_util.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

TrackedPreferenceIncident::TrackedPreferenceIncident(
    std::unique_ptr<ClientIncidentReport_IncidentData_TrackedPreferenceIncident>
        tracked_preference_incident,
    bool is_personal)
    : is_personal_(is_personal) {
  DCHECK(tracked_preference_incident);
  DCHECK(tracked_preference_incident->has_path());
  payload()->set_allocated_tracked_preference(
      tracked_preference_incident.release());
}

TrackedPreferenceIncident::~TrackedPreferenceIncident() {
}

IncidentType TrackedPreferenceIncident::GetType() const {
  return IncidentType::TRACKED_PREFERENCE;
}

// Returns the preference path.
std::string TrackedPreferenceIncident::GetKey() const {
  return payload()->tracked_preference().path();
}

// Returns a digest computed over the payload.
uint32_t TrackedPreferenceIncident::ComputeDigest() const {
  // Tracked preference incidents are sufficiently canonical (and have no
  // default values), so it's safe to serialize the incident as given.
  return HashMessage(payload()->tracked_preference());
}

// Filter out personal preferences.
std::unique_ptr<ClientIncidentReport_IncidentData>
TrackedPreferenceIncident::TakePayload() {
  std::unique_ptr<ClientIncidentReport_IncidentData> payload(
      Incident::TakePayload());

  if (is_personal_) {
    ClientIncidentReport_IncidentData_TrackedPreferenceIncident* incident =
        payload->mutable_tracked_preference();
    incident->clear_atomic_value();
    incident->clear_split_key();
  }

  return payload;
}

}  // namespace safe_browsing

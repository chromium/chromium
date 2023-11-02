// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_incident.h"

#include "base/check.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_handler_util.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

BinaryIntegrityIncident::BinaryIntegrityIncident(
    std::unique_ptr<ClientIncidentReport_IncidentData_BinaryIntegrityIncident>
        binary_integrity_incident) {
  DCHECK(binary_integrity_incident);
  DCHECK(binary_integrity_incident->has_file_basename());
  payload()->set_allocated_binary_integrity(
      binary_integrity_incident.release());
}

BinaryIntegrityIncident::~BinaryIntegrityIncident() {
}

IncidentType BinaryIntegrityIncident::GetType() const {
  return IncidentType::BINARY_INTEGRITY;
}

// Returns the basename of the binary.
std::string BinaryIntegrityIncident::GetKey() const {
  return payload()->binary_integrity().file_basename();
}

// Returns a digest computed over the payload.
uint32_t BinaryIntegrityIncident::ComputeDigest() const {
  return HashMessage(payload()->binary_integrity());
}

}  // namespace safe_browsing

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_INCIDENT_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_INCIDENT_H_

#include <stdint.h>

#include <memory>
#include <string>

namespace safe_browsing {

class ClientIncidentReport_IncidentData;

// An incident's type. Values from this enum are used for histograms (hence the
// making underlying type the same as histogram samples.). Do not re-use
// existing values.
enum class IncidentType : int32_t {
  // Start with 1 rather than zero; otherwise there won't be enough buckets for
  // the histogram.
  TRACKED_PREFERENCE = 1,
  BINARY_INTEGRITY = 2,
  OBSOLETE_BLOCKLIST_LOAD = 3,
  OBSOLETE_OMNIBOX_INTERACTION = 4,
  OBSOLETE_VARIATIONS_SEED_SIGNATURE = 5,
  RESOURCE_REQUEST = 6,
  OBSOLETE_SUSPICIOUS_MODULE = 7,
  // Values for new incident types go here.
  NUM_TYPES = 8
};

// An abstract incident. Subclasses provide type-specific functionality to
// enable logging and pruning by the incident reporting service.
class Incident {
 public:
  Incident(const Incident&) = delete;
  Incident& operator=(const Incident&) = delete;

  virtual ~Incident();

  // Returns the type of the incident.
  virtual IncidentType GetType() const = 0;

  // Returns a key that identifies a particular instance among the type's
  // possibilities.
  virtual std::string GetKey() const = 0;

  // Returns a computed fingerprint of the payload. Incidents of the same
  // incident must result in the same digest.
  virtual uint32_t ComputeDigest() const = 0;

  // Returns the incident's payload.
  virtual std::unique_ptr<ClientIncidentReport_IncidentData> TakePayload();

 protected:
  // Constructs the payload with an empty protobuf, setting its incident time to
  // the current time.
  Incident();

  // Accessors for the payload. These must not be called after the payload has
  // been taken.
  ClientIncidentReport_IncidentData* payload();
  const ClientIncidentReport_IncidentData* payload() const;

 private:
  std::unique_ptr<ClientIncidentReport_IncidentData> payload_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_INCIDENT_H_

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/incident.h"

#include <utility>

#include "base/check.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

Incident::~Incident() {
}

std::unique_ptr<ClientIncidentReport_IncidentData> Incident::TakePayload() {
  return std::move(payload_);
}

Incident::Incident() : payload_(new ClientIncidentReport_IncidentData) {
  payload_->set_incident_time_msec(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
}

ClientIncidentReport_IncidentData* Incident::payload() {
  DCHECK(payload_);
  return payload_.get();
}

const ClientIncidentReport_IncidentData* Incident::payload() const {
  DCHECK(payload_);
  return payload_.get();
}

}  // namespace safe_browsing

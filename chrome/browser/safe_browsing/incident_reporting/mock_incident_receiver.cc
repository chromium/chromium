// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/mock_incident_receiver.h"

#include "chrome/browser/safe_browsing/incident_reporting/incident.h"

namespace safe_browsing {

MockIncidentReceiver::MockIncidentReceiver() = default;

MockIncidentReceiver::~MockIncidentReceiver() = default;

void MockIncidentReceiver::AddIncidentForProfile(
    Profile* profile,
    std::unique_ptr<Incident> incident) {
  DoAddIncidentForProfile(profile, &incident);
}

void MockIncidentReceiver::AddIncidentForProcess(
    std::unique_ptr<Incident> incident) {
  DoAddIncidentForProcess(&incident);
}

void MockIncidentReceiver::ClearIncidentForProcess(
    std::unique_ptr<Incident> incident) {
  DoClearIncidentForProcess(&incident);
}

}  // namespace safe_browsing

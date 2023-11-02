// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_MOCK_INCIDENT_RECEIVER_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_MOCK_INCIDENT_RECEIVER_H_

#include <utility>

#include "chrome/browser/safe_browsing/incident_reporting/incident_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace safe_browsing {

class MockIncidentReceiver : public IncidentReceiver {
 public:
  MockIncidentReceiver();
  ~MockIncidentReceiver() override;

  MOCK_METHOD2(DoAddIncidentForProfile,
               void(Profile*, std::unique_ptr<Incident>*));
  MOCK_METHOD1(DoAddIncidentForProcess, void(std::unique_ptr<Incident>*));
  MOCK_METHOD1(DoClearIncidentForProcess, void(std::unique_ptr<Incident>*));

 protected:
  void AddIncidentForProfile(Profile* profile,
                             std::unique_ptr<Incident> incident) override {
    DoAddIncidentForProfile(profile, &incident);
  }

  void AddIncidentForProcess(std::unique_ptr<Incident> incident) override {
    DoAddIncidentForProcess(&incident);
  }

  void ClearIncidentForProcess(std::unique_ptr<Incident> incident) override {
    DoClearIncidentForProcess(&incident);
  }
};

// An action that passes ownership of the incident in |arg0| to |recipient|.
ACTION_P(TakeIncident, recipient) {
  *recipient = std::move(*arg0);
}

// An action that passes ownership of the incident in |arg0| to the vector in
// |incidents|.
ACTION_P(TakeIncidentToVector, incidents) {
  incidents->push_back(std::move(*arg0));
}

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_MOCK_INCIDENT_RECEIVER_H_

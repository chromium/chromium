// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_INCIDENT_RECEIVER_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_INCIDENT_RECEIVER_H_

#include <memory>

class Profile;

namespace safe_browsing {

class Incident;

// An interface by which incidents may be added to the incident reporting
// service.
class IncidentReceiver {
 public:
  virtual ~IncidentReceiver() {}

  // Adds an incident relating to |profile|. Must be called from the UI thread.
  virtual void AddIncidentForProfile(Profile* profile,
                                     std::unique_ptr<Incident> incident) = 0;

  // Adds an incident relating to the entire browser process. May be called from
  // any thread.
  virtual void AddIncidentForProcess(std::unique_ptr<Incident> incident) = 0;

  // Clears all data associated with the specified |incident| relating to the
  // entire browser process. May be called from any thread.
  virtual void ClearIncidentForProcess(std::unique_ptr<Incident> incident) = 0;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_INCIDENT_RECEIVER_H_

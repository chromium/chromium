// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_RESOURCE_REQUEST_DETECTOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_RESOURCE_REQUEST_DETECTOR_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_receiver.h"
#include "components/safe_browsing/db/database_manager.h"

namespace safe_browsing {

class ClientIncidentReport_IncidentData_ResourceRequestIncident;

struct ResourceRequestInfo {
  GURL url;
  content::ResourceType resource_type;
  int render_process_id;
  int render_frame_id;
};

// Observes network requests and reports suspicious activity.
class ResourceRequestDetector {
 public:
  ResourceRequestDetector(
      scoped_refptr<SafeBrowsingDatabaseManager> sb_database_manager,
      std::unique_ptr<IncidentReceiver> incident_receiver);
  ~ResourceRequestDetector();

  // Analyzes the |request| and triggers an incident report on suspicious
  // script inclusion.
  void ProcessResourceRequest(const ResourceRequestInfo* request);

 protected:
  // Testing hook.
  void set_allow_null_profile_for_testing(bool allow_null_profile_for_testing);

 private:
  void ReportIncidentOnUIThread(
      int render_process_id,
      int render_frame_id,
      std::unique_ptr<ClientIncidentReport_IncidentData_ResourceRequestIncident>
          incident_data);

  std::unique_ptr<IncidentReceiver> incident_receiver_;
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  bool allow_null_profile_for_testing_;

  base::WeakPtrFactory<ResourceRequestDetector> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestDetector);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_RESOURCE_REQUEST_DETECTOR_H_

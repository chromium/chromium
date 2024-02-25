// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_analyzer.h"

#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_incident.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_receiver.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

void ClearBinaryIntegrityForFile(IncidentReceiver* incident_receiver,
                                 const std::string& basename) {
  std::unique_ptr<ClientIncidentReport_IncidentData_BinaryIntegrityIncident>
      incident(new ClientIncidentReport_IncidentData_BinaryIntegrityIncident());
  incident->set_file_basename(basename);
  incident_receiver->ClearIncidentForProcess(
      std::make_unique<BinaryIntegrityIncident>(std::move(incident)));
}

void RegisterBinaryIntegrityAnalysis() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  scoped_refptr<SafeBrowsingService> safe_browsing_service(
      g_browser_process->safe_browsing_service());

  safe_browsing_service->RegisterDelayedAnalysisCallback(
      base::BindOnce(&VerifyBinaryIntegrity));
#endif
}

}  // namespace safe_browsing

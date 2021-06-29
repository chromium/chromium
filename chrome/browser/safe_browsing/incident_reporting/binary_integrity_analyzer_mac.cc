// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_analyzer_mac.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_incident.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_receiver.h"
#include "chrome/browser/safe_browsing/signature_evaluator_mac.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

namespace {

void VerifyBinaryIntegrityHelper(IncidentReceiver* incident_receiver,
                                 const base::FilePath& path,
                                 const std::string& requirement) {
  MacSignatureEvaluator evaluator(path, requirement);
  if (!evaluator.Initialize()) {
    LOG(ERROR) << "Could not initialize mac signature evaluator";
    return;
  }

  std::unique_ptr<ClientIncidentReport_IncidentData_BinaryIntegrityIncident>
      incident(new ClientIncidentReport_IncidentData_BinaryIntegrityIncident());
  if (!evaluator.PerformEvaluation(incident.get())) {
    incident_receiver->AddIncidentForProcess(
        std::make_unique<BinaryIntegrityIncident>(std::move(incident)));
  } else {
    // Clear past incidents involving this bundle if the signature is
    // now valid.
    ClearBinaryIntegrityForFile(incident_receiver, path.BaseName().value());
  }
}

}  // namespace

std::vector<PathAndRequirement> GetCriticalPathsAndRequirements() {
  // Get the path to the main executable.
  std::vector<PathAndRequirement> critical_binaries;
  // This requirement describes a developer ID signed application,
  // with Google's team identifier, and the com.Google.Chrome[.canary]
  // identifier.
  std::string requirement =
      "(identifier \"com.google.Chrome\" or "
      "identifier \"com.google.Chrome.beta\" or "
      "identifier \"com.google.Chrome.dev\" or "
      "identifier \"com.google.Chrome.canary\") "
      "and certificate leaf = H\"c9a99324ca3fcb23dbcc36bd5fd4f9753305130a\"";

  critical_binaries.push_back(
      PathAndRequirement(base::mac::OuterBundlePath(), requirement));
  critical_binaries.push_back(
      PathAndRequirement(base::mac::FrameworkBundlePath(), requirement));
  return critical_binaries;
}

void VerifyBinaryIntegrityForTesting(IncidentReceiver* incident_receiver,
                                     const base::FilePath& path,
                                     const std::string& requirement) {
  VerifyBinaryIntegrityHelper(incident_receiver, path, requirement);
}

void VerifyBinaryIntegrity(
    std::unique_ptr<IncidentReceiver> incident_receiver) {
  size_t i = 0;
  for (const auto& p : GetCriticalPathsAndRequirements()) {
    base::TimeTicks time_before = base::TimeTicks::Now();
    VerifyBinaryIntegrityHelper(incident_receiver.get(), p.path, p.requirement);
    RecordSignatureVerificationTime(i++, base::TimeTicks::Now() - time_before);
  }
}

}  // namespace

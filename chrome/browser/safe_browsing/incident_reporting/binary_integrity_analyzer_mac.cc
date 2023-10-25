// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_analyzer_mac.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/apple/bundle_locations.h"
#include "base/files/file_util.h"
#include "base/logging.h"
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
  std::vector<PathAndRequirement> critical_binaries;
  // This requirement describes a developer ID signed application, with Google's
  // team identifier, and the com.Google.Chrome[.channel] identifier.
  //
  // This is the default requirement that codesign normally uses, from 12.0.1
  // Security-60157.40.30.0.1/OSX/libsecurity_codesigning/lib/drmaker.cpp
  // Security::CodeSigning::DRMaker::appleAnchor, in the isDeveloperIDSignature
  // case. The precise requirement here is tailored to Google's Developer ID
  // Application signing certificate by including the correct team ID,
  // DEVELOPER_ID_LEAF_TEAM_ID (EQHXZ8M8AV). It forms the trust chain from Apple
  // to Google's Developer ID Application signing certificate.
  //
  // 1.2.840.113635.100.6 = {iso(1) member-body(2) us(840) apple(113635)
  // appleDataSecurity(100) appleCertificateExtensions(6)}. The .1 that may
  // follow is appleCertificateExtensionCodeSigning(1).
  //
  // DEVELOPER_ID_LEAF_APPLICATION_OID (1.2.840.113635.100.6.1.13) must be
  // present on the leaf certificate (the one issued to the developer). This
  // attribute is defined in the Apple Developer ID Certification Practice
  // Statement document,
  // https://www.apple.com/certificateauthority/Apple_Developer_ID_CPS. It
  // identifies Developer ID Application signing certificates. drmaker.cpp
  // refers to this OID as caspianLeafMarker and devIdLeafMarkerOID, with the
  // comment "Caspian leaf certificate marker". Based on context and other
  // appearances in the Security project's source code, Caspian is a code name
  // for Developer ID.
  //
  // DEVELOPER_ID_INTERMEDIATE_OID (1.2.840.113635.100.6.2.6) must be present on
  // the intermediate certificate. There's no published definition of this
  // attribute, but it is present on the Developer ID Certification Authority
  // certificate. Among Apple-published intermediate certificates at
  // https://www.apple.com/certificateauthority/, the attribute is unique to
  // Developer ID. drmaker.cpp refers to this OID as caspianSdkMarker and
  // devIdSdkMarkerOID, with the comment "Caspian intermediate marker". Even
  // absent a published definition, this is reasonable assurance that the
  // attribute is being used correctly (and, in fact, following codesign's lead
  // is strong evidence).
  //
  // The difference between "anchor apple" and "anchor apple generic" is that
  // "anchor apple" indicates that something was signed by Apple as Apple's own
  // product, where "anchor apple generic" indicates that something was signed
  // by Apple but is not necessarily Apple's own product. 12.0.1
  // Security-60157.40.30.0.1/OSX/libsecurity_codesigning/lib/reqinterp.cpp
  // Security::CodeSigning::Requirement::Interpreter::eval, cases opAppleAnchor
  // and opAppleGenericAnchor.
  //
  // Some code ends up signed with a requirement that "certificate
  // leaf[field.1.2.840.113635.100.6.1.9]" be present. Per the Apple Worldwide
  // Developer Relations Certification Practice Statement document,
  // https://www.apple.com/certificateauthority/WWDR_CPS, this indicates Mac App
  // Store Application signing. As this is out of scope for Chrome, it does not
  // appear in the requirement string here.
#define DEVELOPER_ID_INTERMEDIATE_OID "field.1.2.840.113635.100.6.2.6"
#define DEVELOPER_ID_LEAF_APPLICATION_OID "field.1.2.840.113635.100.6.1.13"
#define DEVELOPER_ID_LEAF_TEAM_ID "EQHXZ8M8AV"
  // clang-format off
  std::string requirement =
      "(identifier \"com.google.Chrome\" or "
      "identifier \"com.google.Chrome.beta\" or "
      "identifier \"com.google.Chrome.dev\" or "
      "identifier \"com.google.Chrome.canary\") and "
      "anchor apple generic and "
      "certificate 1[" DEVELOPER_ID_INTERMEDIATE_OID "] and "
      "certificate leaf[" DEVELOPER_ID_LEAF_APPLICATION_OID "] and "
      "certificate leaf[subject.OU] = " DEVELOPER_ID_LEAF_TEAM_ID;
  // clang-format on

  critical_binaries.push_back(
      PathAndRequirement(base::apple::OuterBundlePath(), requirement));
  critical_binaries.push_back(
      PathAndRequirement(base::apple::FrameworkBundlePath(), requirement));
  return critical_binaries;
}

void VerifyBinaryIntegrityForTesting(IncidentReceiver* incident_receiver,
                                     const base::FilePath& path,
                                     const std::string& requirement) {
  VerifyBinaryIntegrityHelper(incident_receiver, path, requirement);
}

void VerifyBinaryIntegrity(
    std::unique_ptr<IncidentReceiver> incident_receiver) {
  for (const auto& p : GetCriticalPathsAndRequirements()) {
    VerifyBinaryIntegrityHelper(incident_receiver.get(), p.path, p.requirement);
  }
}

}  // namespace

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_incident.h"

#include <stddef.h>

#include <array>
#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

std::unique_ptr<Incident> MakeIncident(const char* file_basename) {
  std::unique_ptr<ClientIncidentReport_IncidentData_BinaryIntegrityIncident>
      incident(new ClientIncidentReport_IncidentData_BinaryIntegrityIncident);

  incident->set_file_basename(file_basename);

  // Set the signature.
  incident->mutable_signature()->set_trusted(true);
  ClientDownloadRequest_CertificateChain* certificate_chain =
      incident->mutable_signature()->add_certificate_chain();

  // Fill the certificate chain with 2 elements.
  const std::array<std::vector<unsigned char>, 2> certificates = {
      std::vector<unsigned char>{42, 255, 100, 53, 2},
      std::vector<unsigned char>{64, 33, 51, 91, 210},
  };
  for (size_t i = 0; i < std::size(certificates); ++i) {
    ClientDownloadRequest_CertificateChain_Element* element =
        certificate_chain->add_element();
    element->set_certificate(certificates[i].data(), certificates[i].size());
  }

  return std::make_unique<BinaryIntegrityIncident>(std::move(incident));
}

}  // namespace

TEST(BinaryIntegrityIncident, GetType) {
  ASSERT_EQ(IncidentType::BINARY_INTEGRITY, MakeIncident("foo")->GetType());
}

TEST(BinaryIntegrityIncident, GetKeyIsFile) {
  ASSERT_EQ(std::string("foo"), MakeIncident("foo")->GetKey());
}

TEST(BinaryIntegrityIncident, SameIncidentSameDigest) {
  ASSERT_EQ(MakeIncident("foo")->ComputeDigest(),
            MakeIncident("foo")->ComputeDigest());
}

TEST(BinaryIntegrityIncident, DifferentIncidentDifferentDigest) {
  ASSERT_NE(MakeIncident("foo")->ComputeDigest(),
            MakeIncident("bar")->ComputeDigest());
}

}  // namespace safe_browsing

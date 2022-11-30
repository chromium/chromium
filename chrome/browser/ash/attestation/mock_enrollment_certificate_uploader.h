// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_MOCK_ENROLLMENT_CERTIFICATE_UPLOADER_H_
#define CHROME_BROWSER_ASH_ATTESTATION_MOCK_ENROLLMENT_CERTIFICATE_UPLOADER_H_

#include "chrome/browser/ash/attestation/enrollment_certificate_uploader.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace attestation {

class MockEnrollmentCertificateUploader : public EnrollmentCertificateUploader {
 public:
  MockEnrollmentCertificateUploader();

  MockEnrollmentCertificateUploader(const MockEnrollmentCertificateUploader&) =
      delete;
  MockEnrollmentCertificateUploader& operator=(
      const MockEnrollmentCertificateUploader&) = delete;

  ~MockEnrollmentCertificateUploader() override;

  MOCK_METHOD1(ObtainAndUploadCertificate, void(UploadCallback));
  MOCK_METHOD2(ObtainAndUploadCertificateWithRsuDeviceId,
               void(const std::string&, UploadCallback));
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_MOCK_ENROLLMENT_CERTIFICATE_UPLOADER_H_

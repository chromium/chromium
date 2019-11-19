// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_MOCK_ENROLLMENT_CERTIFICATE_UPLOADER_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_MOCK_ENROLLMENT_CERTIFICATE_UPLOADER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/attestation/enrollment_certificate_uploader.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace attestation {

class MockEnrollmentCertificateUploader : public EnrollmentCertificateUploader {
 public:
  MockEnrollmentCertificateUploader();
  ~MockEnrollmentCertificateUploader();

  MOCK_METHOD1(ObtainAndUploadCertificate, void(UploadCallback));
  MOCK_METHOD2(ObtainAndUploadCertificateWithRsuDeviceId,
               void(const std::string&, UploadCallback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEnrollmentCertificateUploader);
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_MOCK_ENROLLMENT_CERTIFICATE_UPLOADER_H_

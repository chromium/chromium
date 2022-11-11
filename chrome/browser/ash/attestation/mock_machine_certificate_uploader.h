// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_MOCK_MACHINE_CERTIFICATE_UPLOADER_H_
#define CHROME_BROWSER_ASH_ATTESTATION_MOCK_MACHINE_CERTIFICATE_UPLOADER_H_

#include "chrome/browser/ash/attestation/machine_certificate_uploader.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace attestation {

class MockMachineCertificateUploader : public MachineCertificateUploader {
 public:
  MockMachineCertificateUploader();

  MockMachineCertificateUploader(const MockMachineCertificateUploader&) =
      delete;
  MockMachineCertificateUploader& operator=(
      const MockMachineCertificateUploader&) = delete;

  ~MockMachineCertificateUploader() override;

  MOCK_METHOD1(UploadCertificateIfNeeded, void(UploadCallback));
  MOCK_METHOD1(RefreshAndUploadCertificate, void(UploadCallback));
  MOCK_METHOD1(WaitForUploadComplete, void(UploadCallback));
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_MOCK_MACHINE_CERTIFICATE_UPLOADER_H_

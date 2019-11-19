// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_ENROLLMENT_CERTIFICATE_UPLOADER_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_ENROLLMENT_CERTIFICATE_UPLOADER_H_

#include "base/callback.h"

namespace chromeos {
namespace attestation {

// An abstract class for enrollment certificate uploaders.
class EnrollmentCertificateUploader {
 public:
  // A callback where |status| is true when the upload is successful.
  using UploadCallback = base::OnceCallback<void(bool status)>;

  virtual ~EnrollmentCertificateUploader() = default;

  // Obtains a fresh enrollment certificate and uploads it.
  virtual void ObtainAndUploadCertificate(UploadCallback callback) = 0;
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_ENROLLMENT_CERTIFICATE_UPLOADER_H_

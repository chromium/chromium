// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_MACHINE_CERTIFICATE_UPLOADER_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_MACHINE_CERTIFICATE_UPLOADER_H_

#include "base/callback.h"

namespace chromeos {
namespace attestation {

// An abstract class for machine certificate uploaders.
class MachineCertificateUploader {
 public:
  using UploadCallback = base::OnceCallback<void(bool)>;

  virtual ~MachineCertificateUploader() = default;

  // Checks if the certificate has been uploaded, and if not, do so.
  // A certificate will be obtained if needed.
  virtual void UploadCertificateIfNeeded(UploadCallback callback) = 0;

  // Forces the obtention of a fresh certificate and uploads it.
  virtual void RefreshAndUploadCertificate(UploadCallback callback) = 0;
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_MACHINE_CERTIFICATE_UPLOADER_H_

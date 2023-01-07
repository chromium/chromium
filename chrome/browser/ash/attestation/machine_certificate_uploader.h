// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_MACHINE_CERTIFICATE_UPLOADER_H_
#define CHROME_BROWSER_ASH_ATTESTATION_MACHINE_CERTIFICATE_UPLOADER_H_

#include "base/functional/callback.h"

namespace ash {
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

  // Non-blocking wait for a certificate to be uploaded. Calls the |callback|
  // immediately if the certificate was already uploaded or wait for the next
  // attempt to do so.
  virtual void WaitForUploadComplete(UploadCallback callback) = 0;
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_MACHINE_CERTIFICATE_UPLOADER_H_

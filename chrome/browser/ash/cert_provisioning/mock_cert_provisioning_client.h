// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_CLIENT_H_
#define CHROME_BROWSER_ASH_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_CLIENT_H_

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash::cert_provisioning {

class MockCertProvisioningClient : public CertProvisioningClient {
 public:
  MockCertProvisioningClient();
  ~MockCertProvisioningClient() override;

  MOCK_METHOD(void,
              Start,
              (ProvisioningProcess provisioning_process,
               StartCallback callback),
              (override));

  MOCK_METHOD(void,
              GetNextInstruction,
              (ProvisioningProcess provisioning_process,
               NextInstructionCallback callback),
              (override));

  MOCK_METHOD(void,
              Authorize,
              (ProvisioningProcess provisioning_process,
               std::string va_challenge_response,
               AuthorizeCallback callback),
              (override));

  MOCK_METHOD(void,
              UploadProofOfPossession,
              (ProvisioningProcess provisioning_process,
               std::string signature,
               UploadProofOfPossessionCallback callback),
              (override));

  MOCK_METHOD(void,
              StartCsr,
              (ProvisioningProcess provisioning_process,
               StartCsrCallback callback),
              (override));
  MOCK_METHOD(void,
              FinishCsr,
              (ProvisioningProcess provisioning_process,
               std::string va_challenge_response,
               std::string signature,
               FinishCsrCallback callback),
              (override));
  MOCK_METHOD(void,
              DownloadCert,
              (ProvisioningProcess provisioning_process,
               DownloadCertCallback callback),
              (override));
};

}  // namespace ash::cert_provisioning

#endif  // CHROME_BROWSER_ASH_CERT_PROVISIONING_MOCK_CERT_PROVISIONING_CLIENT_H_

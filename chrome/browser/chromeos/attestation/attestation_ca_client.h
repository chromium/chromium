// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_ATTESTATION_CA_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_ATTESTATION_CA_CLIENT_H_

#include <list>
#include <map>
#include <string>

#include "base/macros.h"
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/dbus/constants/attestation_constants.h"

namespace network {
class SimpleURLLoader;
}

namespace chromeos {
namespace attestation {

// This class is a ServerProxy implementation for the Chrome OS attestation
// flow.  It sends all requests to an Attestation CA via HTTPS.
class AttestationCAClient : public ServerProxy {
 public:
  AttestationCAClient();
  ~AttestationCAClient() override;

  // chromeos::attestation::ServerProxy:
  void SendEnrollRequest(const std::string& request,
                         const DataCallback& on_response) override;
  void SendCertificateRequest(const std::string& request,
                              const DataCallback& on_response) override;

  void OnURLLoadComplete(
      std::list<std::unique_ptr<network::SimpleURLLoader>>::iterator it,
      const DataCallback& on_response,
      std::unique_ptr<std::string> response_body);

  PrivacyCAType GetType() override;

 private:
  PrivacyCAType pca_type_;

  // POSTs |request| data to |url| and calls |on_response| with the response
  // data when the fetch is complete.
  void FetchURL(const std::string& url,
                const std::string& request,
                const DataCallback& on_response);

  // Loaders used for the processing the requests. Invalidated after completion.
  std::list<std::unique_ptr<network::SimpleURLLoader>> url_loaders_;

  DISALLOW_COPY_AND_ASSIGN(AttestationCAClient);
};

}  // namespace attestation
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ATTESTATION_ATTESTATION_CA_CLIENT_H_

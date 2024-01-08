// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_ATTESTATION_CA_CLIENT_H_
#define CHROME_BROWSER_ASH_ATTESTATION_ATTESTATION_CA_CLIENT_H_

#include <list>
#include <map>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"

namespace network {

class SimpleURLLoader;

namespace mojom {
class NetworkContext;
}  // namespace mojom

}  // namespace network

namespace ash {
namespace attestation {

// This class is a ServerProxy implementation for the Chrome OS attestation
// flow.  It sends all requests to an Attestation CA via HTTPS.
class AttestationCAClient : public ServerProxy {
 public:
  AttestationCAClient();

  AttestationCAClient(const AttestationCAClient&) = delete;
  AttestationCAClient& operator=(const AttestationCAClient&) = delete;

  ~AttestationCAClient() override;

  // chromeos::attestation::ServerProxy:
  void SendEnrollRequest(const std::string& request,
                         DataCallback on_response) override;
  void SendCertificateRequest(const std::string& request,
                              DataCallback on_response) override;

  void OnURLLoadComplete(
      std::list<std::unique_ptr<network::SimpleURLLoader>>::iterator it,
      DataCallback on_response,
      std::unique_ptr<std::string> response_body);

  PrivacyCAType GetType() override;

  void CheckIfAnyProxyPresent(ProxyPresenceCallback callback) override;

  void set_network_context_for_testing(
      network::mojom::NetworkContext* network_context) {
    network_context_for_testing_ = network_context;
  }

 private:
  PrivacyCAType pca_type_;

  // POSTs |request| data to |url| and calls |on_response| with the response
  // data when the fetch is complete.
  void FetchURL(const std::string& url,
                const std::string& request,
                DataCallback on_response);

  // Loaders used for the processing the requests. Invalidated after completion.
  std::list<std::unique_ptr<network::SimpleURLLoader>> url_loaders_;

  raw_ptr<network::mojom::NetworkContext> network_context_for_testing_ =
      nullptr;
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_ATTESTATION_CA_CLIENT_H_

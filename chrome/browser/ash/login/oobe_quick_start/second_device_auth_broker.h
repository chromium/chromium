// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_SECOND_DEVICE_AUTH_BROKER_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_SECOND_DEVICE_AUTH_BROKER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"

class GoogleServiceAuthError;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash::quick_start {

class SecondDeviceAuthBroker {
 public:
  enum class AttestationErrorType { kTransientError, kPermanentError };

  using ChallengeBytesCallback = base::OnceCallback<void(
      const base::expected<std::string, GoogleServiceAuthError>&)>;
  using AttestationCertificateCallback = base::OnceCallback<void(
      const base::expected<std::string, AttestationErrorType>&)>;

  // Constructs an instance of `SecondDeviceAuthBroker`.
  SecondDeviceAuthBroker(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<attestation::AttestationFlow> attestation_flow);
  SecondDeviceAuthBroker(const SecondDeviceAuthBroker&) = delete;
  SecondDeviceAuthBroker& operator=(const SecondDeviceAuthBroker&) = delete;
  ~SecondDeviceAuthBroker();

  // Gets Base64 encoded nonce challenge bytes from Gaia SecondDeviceAuth
  // service.
  // The callback is completed with either the challenge bytes - for successful
  // execution, or with a `GoogleServiceAuthError` - for a failed execution.
  void GetChallengeBytes(ChallengeBytesCallback challenge_callback);

  // Fetches a new Remote Attestation certificate - for proving device
  // integrity.
  // The callback is completed with either a PEM encoded certificate chain
  // string, or with the type of error (`AttestationErrorType`) which occurred
  // during attestation.
  void FetchAttestationCertificate(
      const std::string& fido_credential_id,
      AttestationCertificateCallback certificate_callback);

 private:
  // Callback for handling challenge bytes response from Gaia.
  void OnChallengeBytesFetched(ChallengeBytesCallback challenge_callback,
                               std::unique_ptr<EndpointResponse> response);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Used for fetching results from Gaia endpoints.
  std::unique_ptr<EndpointFetcher> endpoint_fetcher_ = nullptr;

  // Used for interacting with Google's Privacy CA, for getting a Remote
  // Attestation certificate.
  std::unique_ptr<attestation::AttestationFlow> attestation_;

  base::WeakPtrFactory<SecondDeviceAuthBroker> weak_ptr_factory_;
};

}  //  namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_SECOND_DEVICE_AUTH_BROKER_H_

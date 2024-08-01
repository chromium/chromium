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
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
#include "chromeos/ash/components/quick_start/types.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class GoogleServiceAuthError;

namespace ash::attestation {
class AttestationFeatures;
class AttestationFlow;
}  // namespace ash::attestation

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash::quick_start {

struct FidoAssertionInfo;

class SecondDeviceAuthBroker {
 public:
  enum class AttestationErrorType {
    // The error was temporary / transient and the request can be tried again.
    kTransientError,

    // The error was permanent and the request should not be retried.
    kPermanentError,
  };

  // Fields which are common in most `AuthCodeCallback` responses.
  struct AuthCodeBaseResponse {
    // User's email. May be empty.
    std::string email;
  };

  // `AuthCodeCallback` request failed with a parsing error.
  struct AuthCodeParsingErrorResponse {};

  // `AuthCodeCallback` request failed with an unknown error.
  struct AuthCodeUnknownErrorResponse {};

  // `AuthCodeCallback` request completed successfully.
  struct AuthCodeSuccessResponse : public AuthCodeBaseResponse {
    // OAuth Authorization Code.
    std::string auth_code;

    // Obfuscated Gaia id of the user. May be empty.
    std::string gaia_id;
  };

  // `AuthCodeCallback` request was rejected.
  struct AuthCodeRejectionResponse : public AuthCodeBaseResponse {
    enum Reason {
      // Google's authentication server rejected the request but did not tell us
      // why. `email` field may be empty in this case.
      kUnknownReason,

      // The token on the source device is invalid.
      kInvalidOAuthToken,

      // Sign in with this account type is not supported.
      kAccountNotSupported,

      // Sign-in device is less secure.
      kLessSecureDevice,

      // Session is already authenticated.
      kAlreadyAuthenticated,

      // Session has expired.
      kSessionExpired,

      // Challenge expired error thrown during FIDO assertion verification.
      kChallengeExpired,

      // Credential ID mismatch thrown during FIDO assertion verification.
      kCredentialIdMismatch,

      // Federated Enterprise accounts are currently not supported.
      kFederatedEnterpriseAccountNotSupported,

      // A Unicorn account was used for QuickStart when QuickStart is disallowed
      // for the account.
      kUnicornAccountNotEnabled,

      // Account lookup failed - account not found.
      kAccountNotFound,

      // Account lookup failed - captcha required.
      kCaptchaRequired,
    };

    Reason reason;
  };

  // The user needs to be presented with additional challenges on the target
  // device, in response to `AuthCodeCallback`.
  struct AuthCodeAdditionalChallengesOnSourceResponse
      : public AuthCodeBaseResponse {
    // The url to be loaded in a webview to show additional challenges.
    std::string fallback_url;

    // Session identifier for target session. Do NOT log this field anywhere.
    // May be empty.
    std::string target_session_identifier;
  };

  // The user needs to be presented with additional challenges on the target
  // device, in response to `AuthCodeCallback`.
  struct AuthCodeAdditionalChallengesOnTargetResponse
      : public AuthCodeBaseResponse {
    // The url to be loaded in a webview to show additional challenges.
    std::string fallback_url;
  };

  using ChallengeBytesOrError =
      const base::expected<Base64UrlString, GoogleServiceAuthError>&;
  using ChallengeBytesCallback =
      base::OnceCallback<void(ChallengeBytesOrError)>;
  using AttestationCertificateOrError =
      const base::expected<PEMCertChain, AttestationErrorType>&;
  using AttestationCertificateCallback =
      base::OnceCallback<void(AttestationCertificateOrError)>;

  // Possible set of response types for `AuthCodeCallback`.
  using AuthCodeResponse =
      absl::variant<AuthCodeUnknownErrorResponse,
                    AuthCodeSuccessResponse,
                    AuthCodeParsingErrorResponse,
                    AuthCodeRejectionResponse,
                    AuthCodeAdditionalChallengesOnSourceResponse,
                    AuthCodeAdditionalChallengesOnTargetResponse>;
  using AuthCodeCallback = base::OnceCallback<void(const AuthCodeResponse&)>;

  // Constructs an instance of `SecondDeviceAuthBroker`.
  // `device_id` must be between 0 (exclusive) and 64 (inclusive) characters.
  SecondDeviceAuthBroker(
      const std::string& device_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<attestation::AttestationFlow> attestation_flow);
  SecondDeviceAuthBroker(const SecondDeviceAuthBroker&) = delete;
  SecondDeviceAuthBroker& operator=(const SecondDeviceAuthBroker&) = delete;
  virtual ~SecondDeviceAuthBroker();

  // Fetches Base64Url encoded nonce challenge bytes from Gaia SecondDeviceAuth
  // service.
  // The callback is completed with either the challenge bytes - for successful
  // execution, or with a `GoogleServiceAuthError` - for a failed execution.
  // Virtual for testing.
  virtual void FetchChallengeBytes(ChallengeBytesCallback challenge_callback);

  // Fetches a new Remote Attestation certificate - for proving device
  // integrity.
  // The callback is completed with either a PEM encoded certificate chain
  // string, or with the type of error (`AttestationErrorType`) which occurred
  // during attestation.
  // Virtual for testing.
  virtual void FetchAttestationCertificate(
      const Base64UrlString& fido_credential_id,
      AttestationCertificateCallback certificate_callback);

  // Fetches an OAuth authorization code.
  // `certificate` is a PEM encoded certificate chain retrieved earlier using
  // `FetchAttestationCertificate()`.
  // `auth_code_callback` is completed with one of a possible set of result
  // types. See the type definition of `AuthCodeResponse` for reference.
  // Virtual for testing.
  virtual void FetchAuthCode(const FidoAssertionInfo& fido_assertion_info,
                             const PEMCertChain& certificate,
                             AuthCodeCallback auth_code_callback);

 private:
  // Callback for handling challenge bytes response from Gaia.
  void OnChallengeBytesFetched(ChallengeBytesCallback challenge_callback,
                               std::unique_ptr<EndpointResponse> response);

  // Callback for handling the response from Gaia to our request for an OAuth
  // authorization code.
  // If successful, the received OAuth authorization code is used to complete
  // `auth_code_callback`.
  // Otherwise `auth_code_callback` is completed with an appropriate
  // `AuthCodeResponse` error type.
  void OnAuthorizationCodeFetched(AuthCodeCallback auth_code_callback,
                                  std::unique_ptr<EndpointResponse> response);

  // Same as `FetchAttestationCertificate` except that it is called with
  // `attestation_features`.
  void FetchAttestationCertificateInternal(
      const Base64UrlString& fido_credential_id,
      AttestationCertificateCallback certificate_callback,
      const attestation::AttestationFeatures* attestation_features);

  // Internal helper method to respond to `callback`.
  void RunAttestationCertificateCallback(
      SecondDeviceAuthBroker::AttestationCertificateCallback callback,
      attestation::AttestationStatus status,
      const std::string& pem_certificate_chain);

  // Internal helper method to respond to `auth_code_callback`.
  void RunAuthCodeCallbackFromParsedResponse(
      SecondDeviceAuthBroker::AuthCodeCallback auth_code_callback,
      std::unique_ptr<EndpointResponse> unparsed_response,
      data_decoder::DataDecoder::ValueOrError response);

  // Internal helper methods to respond to `challenge_callback`.
  void HandleFetchChallengeBytesErrorResponse(
      SecondDeviceAuthBroker::ChallengeBytesCallback challenge_callback,
      std::unique_ptr<EndpointResponse> response);
  void RunChallengeBytesCallbackWithError(
      SecondDeviceAuthBroker::ChallengeBytesCallback challenge_callback,
      const GoogleServiceAuthError& error);
  void RunChallengeBytesCallback(
      SecondDeviceAuthBroker::ChallengeBytesCallback challenge_callback,
      const Base64UrlString& challenge);

  // Must be between 0 (exclusive) and 64 (inclusive) characters.
  const std::string device_id_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  QuickStartMetrics metrics_;

  // Used for fetching results from Gaia endpoints.
  std::unique_ptr<EndpointFetcher> endpoint_fetcher_ = nullptr;

  // Used for interacting with Google's Privacy CA, for getting a Remote
  // Attestation certificate.
  std::unique_ptr<attestation::AttestationFlow> attestation_;

  base::WeakPtrFactory<SecondDeviceAuthBroker> weak_ptr_factory_;
};

std::ostream& operator<<(
    std::ostream& stream,
    const SecondDeviceAuthBroker::AuthCodeRejectionResponse::Reason& reason);

std::ostream& operator<<(
    std::ostream& stream,
    const SecondDeviceAuthBroker::AttestationErrorType& attestation_error);

}  //  namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_SECOND_DEVICE_AUTH_BROKER_H_

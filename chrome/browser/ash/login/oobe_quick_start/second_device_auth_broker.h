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
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class GoogleServiceAuthError;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash::quick_start {

struct FidoAssertionInfo;

class SecondDeviceAuthBroker : public GaiaAuthConsumer {
 public:
  enum class AttestationErrorType {
    // The error was temporary / transient and the request can be tried again.
    kTransientError,

    // The error was permanent and the request should not be retried.
    kPermanentError,
  };

  // Fields which are common in most `RefreshTokenCallback` responses.
  struct RefreshTokenBaseResponse {
    // User's email. May be empty.
    std::string email;
  };

  // `RefreshTokenCallback` request failed with a parsing error.
  struct RefreshTokenParsingErrorResponse {};

  // `RefreshTokenCallback` request failed with an unknown error.
  struct RefreshTokenUnknownErrorResponse {};

  // `RefreshTokenCallback` request completed successfully.
  struct RefreshTokenSuccessResponse : public RefreshTokenBaseResponse {
    // Login Scoped OAuth Refresh Token.
    std::string refresh_token;
  };

  // `RefreshTokenCallback` request was rejected.
  struct RefreshTokenRejectionResponse : public RefreshTokenBaseResponse {
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

      // OAuth authorization code to refresh token exchange request failed.
      kInvalidAuthorizationCode,
    };

    Reason reason;
  };

  // The user needs to be presented with additional challenges on the target
  // device, in response to `RefreshTokenCallback`.
  struct RefreshTokenAdditionalChallengesOnSourceResponse
      : public RefreshTokenBaseResponse {
    // The url to be loaded in a webview to show additional challenges.
    std::string fallback_url;

    // Session identifier for target session. Do NOT log this field anywhere.
    // May be empty.
    std::string target_session_identifier;
  };

  // The user needs to be presented with additional challenges on the target
  // device, in response to `RefreshTokenCallback`.
  struct RefreshTokenAdditionalChallengesOnTargetResponse
      : public RefreshTokenBaseResponse {
    // The url to be loaded in a webview to show additional challenges.
    std::string fallback_url;
  };

  using ChallengeBytesCallback = base::OnceCallback<void(
      const base::expected<std::string, GoogleServiceAuthError>&)>;
  using AttestationCertificateCallback = base::OnceCallback<void(
      const base::expected<std::string, AttestationErrorType>&)>;
  using RefreshTokenOrErrorCallback = base::OnceCallback<void(
      const base::expected<std::string, GoogleServiceAuthError>&)>;

  // Possible set of response types for `RefreshTokenCallback`.
  using RefreshTokenResponse =
      absl::variant<RefreshTokenUnknownErrorResponse,
                    RefreshTokenSuccessResponse,
                    RefreshTokenParsingErrorResponse,
                    RefreshTokenRejectionResponse,
                    RefreshTokenAdditionalChallengesOnSourceResponse,
                    RefreshTokenAdditionalChallengesOnTargetResponse>;
  using RefreshTokenCallback =
      base::OnceCallback<void(const RefreshTokenResponse&)>;

  // Constructs an instance of `SecondDeviceAuthBroker`.
  SecondDeviceAuthBroker(
      const std::string& device_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<attestation::AttestationFlow> attestation_flow);
  SecondDeviceAuthBroker(const SecondDeviceAuthBroker&) = delete;
  SecondDeviceAuthBroker& operator=(const SecondDeviceAuthBroker&) = delete;
  ~SecondDeviceAuthBroker() override;

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

  // Fetches a Login Scoped OAuth Refresh Token (LST).
  // `certificate` is a PEM encoded certificate chain retrieved earlier using
  // `FetchAttestationCertificate()`.
  // `refresh_token_callback` is completed with one of a possible set of result
  // types. See the type definition of `RefreshTokenResponse` for reference.
  void FetchRefreshToken(const FidoAssertionInfo& fido_assertion_info,
                         const std::string& certificate,
                         RefreshTokenCallback refresh_token_callback);

 private:
  // Callback for handling challenge bytes response from Gaia.
  void OnChallengeBytesFetched(ChallengeBytesCallback challenge_callback,
                               std::unique_ptr<EndpointResponse> response);

  // Callback for handling the response from Gaia to our request for an OAuth
  // authorization code.
  // If successful, the received OAuth authorization code is in turn exchanged
  // for an LST, and `refresh_token_callback` is completed.
  // Otherwise `refresh_token_callback` is completed with an appropriate
  // `RefreshTokenResponse` type.
  void OnAuthorizationCodeFetched(RefreshTokenCallback refresh_token_callback,
                                  std::unique_ptr<EndpointResponse> response);

  // Exchanges an OAuth `authorization_code` for an OAuth login scoped refresh
  // token.
  // The callback is completed with a refresh token or an error reason.
  void FetchRefreshTokenFromAuthorizationCode(
      const std::string& authorization_code,
      RefreshTokenOrErrorCallback callback);

  // `GaiaAuthConsumer` overrides.
  void OnClientOAuthSuccess(const ClientOAuthResult& result) override;
  void OnClientOAuthFailure(const GoogleServiceAuthError& error) override;

  RefreshTokenOrErrorCallback refresh_token_internal_callback_;

  const std::string device_id_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Used for fetching results from Gaia endpoints.
  std::unique_ptr<EndpointFetcher> endpoint_fetcher_ = nullptr;

  // Used for interacting with Google's Privacy CA, for getting a Remote
  // Attestation certificate.
  std::unique_ptr<attestation::AttestationFlow> attestation_;

  // Used for fetching OAuth refresh tokens.
  std::unique_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;

  base::WeakPtrFactory<SecondDeviceAuthBroker> weak_ptr_factory_;
};

}  //  namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_SECOND_DEVICE_AUTH_BROKER_H_

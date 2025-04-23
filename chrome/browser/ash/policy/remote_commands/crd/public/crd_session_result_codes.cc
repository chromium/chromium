// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd/public/crd_session_result_codes.h"

#include "base/notreached.h"
#include "remoting/protocol/errors.h"

namespace policy {

namespace {
using remoting::protocol::ErrorCode;
}  // namespace

ExtendedStartCrdSessionResultCode ToExtendedStartCrdSessionResultCode(
    ErrorCode error_code) {
  switch (error_code) {
    case ErrorCode::OK:
      return ExtendedStartCrdSessionResultCode::kSuccess;
    case ErrorCode::PEER_IS_OFFLINE:
      return ExtendedStartCrdSessionResultCode::kFailurePeerIsOffline;
    case ErrorCode::SESSION_REJECTED:
      return ExtendedStartCrdSessionResultCode::kFailureSessionRejected;
    case ErrorCode::INCOMPATIBLE_PROTOCOL:
      return ExtendedStartCrdSessionResultCode::kFailureIncompatibleProtocol;
    case ErrorCode::AUTHENTICATION_FAILED:
      return ExtendedStartCrdSessionResultCode::kFailureAuthenticationFailed;
    case ErrorCode::INVALID_ACCOUNT:
      return ExtendedStartCrdSessionResultCode::kFailureInvalidAccount;
    case ErrorCode::CHANNEL_CONNECTION_ERROR:
      return ExtendedStartCrdSessionResultCode::kFailureChannelConnectionError;
    case ErrorCode::SIGNALING_ERROR:
      return ExtendedStartCrdSessionResultCode::kFailureSignalingError;
    case ErrorCode::SIGNALING_TIMEOUT:
      return ExtendedStartCrdSessionResultCode::kFailureSignalingTimeout;
    case ErrorCode::HOST_OVERLOAD:
      return ExtendedStartCrdSessionResultCode::kFailureHostOverload;
    case ErrorCode::MAX_SESSION_LENGTH:
      return ExtendedStartCrdSessionResultCode::kFailureMaxSessionLength;
    case ErrorCode::HOST_CONFIGURATION_ERROR:
      return ExtendedStartCrdSessionResultCode::kFailureHostConfigurationError;
    case ErrorCode::HOST_CERTIFICATE_ERROR:
      return ExtendedStartCrdSessionResultCode::kFailureHostCertificateError;
    case ErrorCode::HOST_REGISTRATION_ERROR:
      return ExtendedStartCrdSessionResultCode::kFailureHostRegistrationError;
    case ErrorCode::EXISTING_ADMIN_SESSION:
      return ExtendedStartCrdSessionResultCode::kFailureExistingAdminSession;
    case ErrorCode::AUTHZ_POLICY_CHECK_FAILED:
      return ExtendedStartCrdSessionResultCode::kFailureAuthzPolicyCheckFailed;
    case ErrorCode::LOCATION_AUTHZ_POLICY_CHECK_FAILED:
      return ExtendedStartCrdSessionResultCode::
          kFailureLocationAuthzPolicyCheckFailed;
    case ErrorCode::DISALLOWED_BY_POLICY:
      return ExtendedStartCrdSessionResultCode::kFailureDisabledByPolicy;
    case ErrorCode::UNAUTHORIZED_ACCOUNT:
      return ExtendedStartCrdSessionResultCode::kFailureUnauthorizedAccount;
    case ErrorCode::UNKNOWN_ERROR:
    // This error can only take place for windows builds which is not a part for
    // commercial CRD.
    case ErrorCode::ELEVATION_ERROR:
    // This error is only reported on Mac.
    case ErrorCode::LOGIN_SCREEN_NOT_SUPPORTED:
      return ExtendedStartCrdSessionResultCode::kFailureUnknownError;
    case ErrorCode::REAUTHZ_POLICY_CHECK_FAILED:
      return ExtendedStartCrdSessionResultCode::
          kFailureReauthzPolicyCheckFailed;
    case ErrorCode::NO_COMMON_AUTH_METHOD:
      return ExtendedStartCrdSessionResultCode::kFailureNoCommonAuthMethod;
    case ErrorCode::SESSION_POLICIES_CHANGED:
      return ExtendedStartCrdSessionResultCode::kFailureSessionPoliciesChanged;
    case ErrorCode::UNEXPECTED_AUTHENTICATOR_ERROR:
      return ExtendedStartCrdSessionResultCode::
          kFailureUnexpectedAuthenticatorError;
    case ErrorCode::INVALID_STATE:
      return ExtendedStartCrdSessionResultCode::kFailureInvalidState;
    case ErrorCode::INVALID_ARGUMENT:
      return ExtendedStartCrdSessionResultCode::kFailureInvalidArgument;
    case ErrorCode::NETWORK_FAILURE:
      return ExtendedStartCrdSessionResultCode::kFailureNetworkFailure;
  }
  NOTREACHED();
}

StartCrdSessionResultCode ToStartCrdSessionResultCode(
    ExtendedStartCrdSessionResultCode result_code) {
  switch (result_code) {
    case ExtendedStartCrdSessionResultCode::kSuccess:
      return StartCrdSessionResultCode::START_CRD_SESSION_SUCCESS;
    case ExtendedStartCrdSessionResultCode::kFailureUnsupportedUserType:
      return StartCrdSessionResultCode::FAILURE_UNSUPPORTED_USER_TYPE;
    case ExtendedStartCrdSessionResultCode::kFailureNotIdle:
      return StartCrdSessionResultCode::FAILURE_NOT_IDLE;
    case ExtendedStartCrdSessionResultCode::kFailureNoOauthToken:
      return StartCrdSessionResultCode::FAILURE_NO_OAUTH_TOKEN;
    case ExtendedStartCrdSessionResultCode::kFailureCrdHostError:
      return StartCrdSessionResultCode::FAILURE_CRD_HOST_ERROR;
    case ExtendedStartCrdSessionResultCode::kFailureUnmanagedEnvironment:
      return StartCrdSessionResultCode::FAILURE_UNMANAGED_ENVIRONMENT;
    case ExtendedStartCrdSessionResultCode::kFailureDisabledByPolicy:
      return StartCrdSessionResultCode::FAILURE_DISABLED_BY_POLICY;
    case ExtendedStartCrdSessionResultCode::kFailureUnknownError:
      return StartCrdSessionResultCode::START_CRD_SESSION_RESULT_UNKNOWN;

    case ExtendedStartCrdSessionResultCode::kFailureAuthzPolicyCheckFailed:
    case ExtendedStartCrdSessionResultCode::kFailurePeerIsOffline:
    case ExtendedStartCrdSessionResultCode::kFailureSessionRejected:
    case ExtendedStartCrdSessionResultCode::kFailureIncompatibleProtocol:
    case ExtendedStartCrdSessionResultCode::kFailureAuthenticationFailed:
    case ExtendedStartCrdSessionResultCode::kFailureInvalidAccount:
    case ExtendedStartCrdSessionResultCode::kFailureChannelConnectionError:
    case ExtendedStartCrdSessionResultCode::kFailureSignalingError:
    case ExtendedStartCrdSessionResultCode::kFailureSignalingTimeout:
    case ExtendedStartCrdSessionResultCode::kFailureHostOverload:
    case ExtendedStartCrdSessionResultCode::kFailureMaxSessionLength:
    case ExtendedStartCrdSessionResultCode::kFailureHostConfigurationError:
    case ExtendedStartCrdSessionResultCode::kFailureHostCertificateError:
    case ExtendedStartCrdSessionResultCode::kFailureHostRegistrationError:
    case ExtendedStartCrdSessionResultCode::kFailureExistingAdminSession:
    case ExtendedStartCrdSessionResultCode::
        kFailureLocationAuthzPolicyCheckFailed:
    case ExtendedStartCrdSessionResultCode::kFailureUnauthorizedAccount:
    case ExtendedStartCrdSessionResultCode::kFailureHostPolicyError:
    case ExtendedStartCrdSessionResultCode::kFailureHostInvalidDomainError:
    case ExtendedStartCrdSessionResultCode::kHostSessionDisconnected:
    case ExtendedStartCrdSessionResultCode::kFailureReauthzPolicyCheckFailed:
    case ExtendedStartCrdSessionResultCode::kFailureNoCommonAuthMethod:
    case ExtendedStartCrdSessionResultCode::kFailureSessionPoliciesChanged:
    case ExtendedStartCrdSessionResultCode::
        kFailureUnexpectedAuthenticatorError:
    case ExtendedStartCrdSessionResultCode::kFailureInvalidState:
    case ExtendedStartCrdSessionResultCode::kFailureInvalidArgument:
    case ExtendedStartCrdSessionResultCode::kFailureNetworkFailure:
      // The server side is not interested in a lot of the different CRD host
      // failures, which is why most of them are simply mapped to
      // 'FAILURE_CRD_HOST_ERROR`.
      return StartCrdSessionResultCode::FAILURE_CRD_HOST_ERROR;
  }
  NOTREACHED();
}
}  // namespace policy

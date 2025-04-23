// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_PUBLIC_CRD_SESSION_RESULT_CODES_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_PUBLIC_CRD_SESSION_RESULT_CODES_H_

#include "components/policy/proto/device_management_backend.pb.h"
#include "remoting/protocol/errors.h"

namespace policy {

using ::enterprise_management::StartCrdSessionResultCode;

// Extended version of `StartCrdSessionResultCode`, which contains extra values
// that we want to log to UMA.
// This enum can't be renumbered because it's logged to UMA.
// NOTE: Whenever this enum is updated, please also update
// EnterpriseCrdSessionResultCode in
// tools/metrics/histograms/metadata/enterprise/enums.xml to keep them in sync.
enum class ExtendedStartCrdSessionResultCode {
  // Successfully obtained access code.
  kSuccess = 0,

  // Failed as required services are not launched on the device.
  // deprecated FAILURE_SERVICES_NOT_READY = 1,

  // Failure as the current user type does not support remotely starting CRD.
  kFailureUnsupportedUserType = 2,

  // Failed as device is currently in use and no interruptUser flag is set.
  kFailureNotIdle = 3,

  // Failed as we could not get OAuth token for whatever reason.
  kFailureNoOauthToken = 4,

  // Failed as we could not get ICE configuration for whatever reason.
  // deprecated FAILURE_NO_ICE_CONFIG = 5,

  // Failure during attempt to start CRD host and obtain CRD token.
  kFailureCrdHostError = 6,

  // Failure to start a curtained session as we're not in a managed
  // environment.
  kFailureUnmanagedEnvironment = 7,

  // Failed because RemoteAccessHostAllowEnterpriseRemoteSupportConnections
  // policy is disabled.
  kFailureDisabledByPolicy = 8,

  // Failure to start because client device is unreachable.
  kFailurePeerIsOffline = 9,

  // Failure to start the session because the local user on the host device
  // rejected the support session request.
  kFailureSessionRejected = 10,

  // Failure to start because the protocol doesn't match between the host and
  // the client device.
  kFailureIncompatibleProtocol = 11,

  // Failure to start the session because authentication failed.
  kFailureAuthenticationFailed = 12,

  // Failure to start the session because the admin user's domain is blocked by
  // policy or if the username is invalid.
  kFailureInvalidAccount = 13,

  // Failure when webrtc operations failed while establishing a channel
  // connection.
  kFailureChannelConnectionError = 14,

  // Failure when the register-support-host request is failed or disconnected
  // before registration succeeds.
  kFailureSignalingError = 15,

  // Failure when the register-support-host request timeout.
  kFailureSignalingTimeout = 16,

  // Failure while starting the session as host was overloaded with failed login
  // attempts.
  kFailureHostOverload = 17,

  // Failure on the host device when the maximum session length is reached.
  kFailureMaxSessionLength = 18,

  // Failure as the host could not create a desktop environment (for instance
  // the curtain could not be initialized).
  kFailureHostConfigurationError = 19,

  // Failure as the certificate generation on the host device has failed.
  kFailureHostCertificateError = 20,

  // Failure as the registration support host failed to parse the received
  // response.
  kFailureHostRegistrationError = 21,

  // Failure to start the session as there is an existing admin session ongoing
  // on the host device.
  kFailureExistingAdminSession = 22,

  // Failure because the client is authorized to connect to the host device due
  // to a policy defined by third party auth service has failed.
  kFailureAuthzPolicyCheckFailed = 23,

  // Failure because the client is not authorized to connect to the host device
  // based on their current location due to a policy defined by the third party
  // auth service.
  kFailureLocationAuthzPolicyCheckFailed = 24,

  // Failure to start the session as the admin user is not authorized for
  // starting a remote desktop session.
  kFailureUnauthorizedAccount = 25,

  // Failure as the host is unable to load the device policy.
  kFailureHostPolicyError = 26,

  // Failure as Indicates the remote support host could not start as the user's
  // domain is not included in the device policy allowlist.
  kFailureHostInvalidDomainError = 27,

  // Host session was disconnected without any error.
  kHostSessionDisconnected = 28,

  kFailureUnknownError = 29,

  // Failure because a policy defined by the third party auth service no longer
  // permits the connection.
  kFailureReauthzPolicyCheckFailed = 30,

  // Failed to find an authentication method that is supported by both the host
  // and the client.
  kFailureNoCommonAuthMethod = 31,

  // Failure because the session policies have changed.
  kFailureSessionPoliciesChanged = 32,

  // Failure because the session authenticator has returned an unexpected error.
  kFailureUnexpectedAuthenticatorError = 33,

  // Failure because the application is in an invalid state.
  kFailureInvalidState = 34,

  // Failure because the peer has sent an invalid message. E.g. fields are
  // missing in the message.
  kFailureInvalidArgument = 35,

  // Failure because a local network issue has prevented the remote connection.
  kFailureNetworkFailure = 36,

  kMaxValue = kFailureNetworkFailure
};

// Translates the error code.
ExtendedStartCrdSessionResultCode ToExtendedStartCrdSessionResultCode(
    remoting::protocol::ErrorCode error_code);

StartCrdSessionResultCode ToStartCrdSessionResultCode(
    ExtendedStartCrdSessionResultCode error_code);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_PUBLIC_CRD_SESSION_RESULT_CODES_H_

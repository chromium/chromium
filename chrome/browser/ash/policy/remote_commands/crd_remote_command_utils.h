// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains some utilities that are used by both CRD related
// remote commands (`FETCH_CRD_AVAILABILITY_INFO` and
// `DEVICE_START_CRD_SESSION`).

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_REMOTE_COMMAND_UTILS_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_REMOTE_COMMAND_UTILS_H_

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "remoting/protocol/errors.h"

namespace policy {

// The current active session type on the device, or `NO_SESSION` if no user
// is currently logged in.
using ::enterprise_management::UserSessionType;

// The type of the CRD session.
using ::enterprise_management::CrdSessionType;

// This enum can't be renumbered because it's logged to UMA.
// Update the tools/metrics/histogram/enums.xml to be in parity with this for
// UMA.
enum class ResultCode {
  // Successfully obtained access code.
  SUCCESS = 0,

  // Failed as required services are not launched on the device.
  // deprecated FAILURE_SERVICES_NOT_READY = 1,

  // Failure as the current user type does not support remotely starting CRD.
  FAILURE_UNSUPPORTED_USER_TYPE = 2,

  // Failed as device is currently in use and no interruptUser flag is set.
  FAILURE_NOT_IDLE = 3,

  // Failed as we could not get OAuth token for whatever reason.
  FAILURE_NO_OAUTH_TOKEN = 4,

  // Failed as we could not get ICE configuration for whatever reason.
  // deprecated FAILURE_NO_ICE_CONFIG = 5,

  // Failure during attempt to start CRD host and obtain CRD token.
  FAILURE_CRD_HOST_ERROR = 6,

  // Failure to start a curtained session as we're not in a managed
  // environment.
  FAILURE_UNMANAGED_ENVIRONMENT = 7,

  // Failed because RemoteAccessHostAllowEnterpriseRemoteSupportConnections
  // policy is disabled.
  FAILURE_DISABLED_BY_POLICY = 8,

  // Failure to start because client device is unreachable.
  FAILURE_PEER_IS_OFFLINE = 9,

  // Failure to start the session because the local user on the host device
  // rejected the support session request.
  FAILURE_SESSION_REJECTED = 10,

  // Failure to start because the protocol doesn't match between the host and
  // the client device.
  FAILURE_INCOMPATIBLE_PROTOCOL = 11,

  // Failure to start the session because authentication failed.
  FAILURE_AUTHENTICATION_FAILED = 12,

  // Failure to start the session because the admin user's domain is blocked by
  // policy or if the username is invalid.
  FAILURE_INVALID_ACCOUNT = 13,

  // Failure when webrtc operations failed while establishing a channel
  // connection.
  FAILURE_CHANNEL_CONNECTION_ERROR = 14,

  // Failure when the register-support-host request is failed or disconnected
  // before registration succeeds.
  FAILURE_SIGNALING_ERROR = 15,

  // Failure when the register-support-host request timeout.
  FAILURE_SIGNALING_TIMEOUT = 16,

  // Failure while starting the session as host was overloaded with failed login
  // attempts.
  FAILURE_HOST_OVERLOAD = 17,

  // Failure on the host device when the maximum session length is reached.
  FAILURE_MAX_SESSION_LENGTH = 18,

  // Failure as the host could not create a desktop environment (for instance
  // the curtain could not be initialized).
  FAILURE_HOST_CONFIGURATION_ERROR = 19,

  // Failure as the certificate generation on the host device has failed.
  FAILURE_HOST_CERTIFICATE_ERROR = 20,

  // Failure as the registration support host failed to parse the received
  // response.
  FAILURE_HOST_REGISTRATION_ERROR = 21,

  // Failure to start the session as there is an existing admin session ongoing
  // on the host device.
  FAILURE_EXISTING_ADMIN_SESSION = 22,

  // Failure because the client is authorized to connect to the host device due
  // to a policy defined by third party auth service has failed.
  FAILURE_AUTHZ_POLICY_CHECK_FAILED = 23,

  // Failure because the client is not authorized to connect to the host device
  // based on their current location due to a policy defined by the third party
  // auth service.
  FAILURE_LOCATION_AUTHZ_POLICY_CHECK_FAILED = 24,

  // Failure to start the session as the admin user is not authorized for
  // starting a remote desktop session.
  FAILURE_UNAUTHORIZED_ACCOUNT = 25,

  // Failure as the host is unable to load the device policy.
  FAILURE_HOST_POLICY_ERROR = 26,

  // Failure as Indicates the remote support host could not start as the user's
  // domain is not included in the device policy allowlist.
  FAILURE_HOST_INVALID_DOMAIN_ERROR = 27,

  // Host session was disconnected without any error.
  HOST_SESSION_DISCONNECTED = 28,

  FAILURE_UNKNOWN_ERROR = 29,

  kMaxValue = FAILURE_UNKNOWN_ERROR
};

// Translates the `remoting::protocol::ErrorCode` to `ResultCode`.
ResultCode ConvertErrorCodeToResultCode(
    remoting::protocol::ErrorCode error_code);

// Returns the time since the last user activity on this device.
// Returns `TimeDelta::Max()` if there was no user activity since the last
// reboot.
base::TimeDelta GetDeviceIdleTime();

// Returns the type of the currently active user session.
UserSessionType GetCurrentUserSessionType();

// Returns if a remote admin is allowed to start a 'CRD remote support' session
// when an user session of the given type is active.
bool UserSessionSupportsRemoteSupport(UserSessionType user_session);

// Returns if a remote admin is allowed to start a 'CRD remote access' session
// when an user session of the given type is active.
bool UserSessionSupportsRemoteAccess(UserSessionType user_session);

const char* UserSessionTypeToString(UserSessionType value);
const char* CrdSessionTypeToString(CrdSessionType value);

// Returns asynchronously if the ChromeOS device is in a managed environment.
// We consider the device's environment to be managed if there is a
//      * active (connected) network
//      * with a policy ONC source set (meaning the network is managed)
//      * which is not cellular
//
// The reasoning is that these conditions will only be met if the device is in
// an office building or a store, and these conditions will not be met if the
// device is in a private setting like an user's home.
using ManagedEnvironmentResultCallback = base::OnceCallback<void(bool)>;
void CalculateIsInManagedEnvironmentAsync(
    ManagedEnvironmentResultCallback result_callback);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_REMOTE_COMMAND_UTILS_H_

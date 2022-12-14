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

namespace policy {

// The current active session type on the device, or `NO_SESSION` if no user
// is currently logged in.
using ::enterprise_management::UserSessionType;

// The type of the CRD session.
using ::enterprise_management::CrdSessionType;

// Returns the time since the last user activity on this device.
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

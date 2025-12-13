// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains some utilities that are used by both CRD related
// remote commands (`FETCH_CRD_AVAILABILITY_INFO` and
// `DEVICE_START_CRD_SESSION`).

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CRD_REMOTE_COMMAND_UTILS_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CRD_REMOTE_COMMAND_UTILS_H_

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/shared_crd_session.h"
#include "chrome/browser/ash/policy/remote_commands/crd/start_crd_session_job_delegate.h"
#include "components/crash/core/common/crash_key.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "remoting/host/chromeos/chromeos_enterprise_params.h"

namespace policy {

// The current active session type on the device, or `NO_SESSION` if no user
// is currently logged in.
using ::enterprise_management::UserSessionType;

// The type of the CRD session.
using ::enterprise_management::CrdSessionType;

static const char kCrdCrashKeyName[] = "crd-enterprise";

// Returns a crash key-value string identifying the current CRD session and user
// session types.
const std::string GetCrdCrashKeyValue(CrdSessionType crd_session_type,
                                      UserSessionType session_type);

// Returns the time since the last user activity on this device.
// Returns `TimeDelta::Max()` if there was no user activity since the last
// reboot.
base::TimeDelta GetDeviceIdleTime();

// Returns true if the device has been idle since the last reboot.
bool IsDeviceIdleSinceReboot();

// Returns the type of the currently active user session.
UserSessionType GetCurrentUserSessionType();

// Returns if a remote admin is allowed to start a 'CRD remote support' session
// when an user session of the given type is active.
bool UserSessionSupportsRemoteSupport(UserSessionType user_session);

// Returns if a remote admin is allowed to start a 'CRD remote access' session
// when an user session of the given type is active.
bool UserSessionSupportsRemoteAccess(UserSessionType user_session);

// Returns if a remote admin is allowed by policy to start a 'CRD remote access'
// session when no user is present at the device.
bool IsRemoteAccessAllowedByPolicy(const PrefService& policy_service);

// Returns if a remote admin is allowed by policy to start a 'CRD remote
// support' session when no user is present at the device.
bool IsRemoteSupportAllowedByPolicy(const PrefService& policy_service);

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

// Convert from `StartCrdSessionJobDelegate::RequestOrigin` to
// `remoting::ChromeOsEnterpriseRequestOrigin`.
remoting::ChromeOsEnterpriseRequestOrigin
ConvertToChromeOsEnterpriseRequestOrigin(
    StartCrdSessionJobDelegate::RequestOrigin request_origin);

// Convert from `SharedCrdSession::RequestOrigin` to
// `StartCrdSessionJobDelegate::RequestOrigin`.
StartCrdSessionJobDelegate::RequestOrigin
ConvertToStartCrdSessionJobDelegateRequestOrigin(
    SharedCrdSession::RequestOrigin request_origin);

// Convert from `StartCrdSessionJobDelegate::AudioPlayback` to
// `remoting::ChromeOsEnterpriseAudioPlayback`.
remoting::ChromeOsEnterpriseAudioPlayback
ConvertToChromeOsEnterpriseAudioPlayback(
    StartCrdSessionJobDelegate::AudioPlayback audio_playback);

// Convert from `SharedCrdSession::AudioPlayback` to
// `StartCrdSessionJobDelegate::AudioPlayback`.
StartCrdSessionJobDelegate::AudioPlayback
ConvertToStartCrdSessionJobDelegateAudioPlayback(
    SharedCrdSession::AudioPlayback audio_playback);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CRD_REMOTE_COMMAND_UTILS_H_

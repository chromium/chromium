// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_fetch_crd_availability_info_job.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/remote_commands/crd_logging.h"
#include "chrome/browser/ash/policy/remote_commands/crd_remote_command_utils.h"
#include "extensions/common/value_builder.h"

namespace policy {

namespace {

using enterprise_management::RemoteCommand;
using extensions::DictionaryBuilder;

constexpr char kIdleTime[] = "deviceIdleTimeInSeconds";
constexpr char kUserSessionType[] = "userSessionType";
constexpr char kSupportedCrdSessionTypes[] = "supportedCrdSessionTypes";
constexpr char kIsInManagedEnvironment[] = "isInManagedEnvironment";

base::Value::List GetSupportedSessionTypes(bool is_in_managed_environment) {
  base::Value::List result;

  if (UserSessionSupportsRemoteSupport(GetCurrentUserSessionType())) {
    result.Append(static_cast<int>(CrdSessionType::REMOTE_SUPPORT_SESSION));
  }

  if (UserSessionSupportsRemoteAccess(GetCurrentUserSessionType()) &&
      is_in_managed_environment) {
    result.Append(static_cast<int>(CrdSessionType::REMOTE_ACCESS_SESSION));
  }

  return result;
}

}  // namespace

DeviceCommandFetchCrdAvailabilityInfoJob::
    DeviceCommandFetchCrdAvailabilityInfoJob() = default;
DeviceCommandFetchCrdAvailabilityInfoJob::
    ~DeviceCommandFetchCrdAvailabilityInfoJob() = default;

enterprise_management::RemoteCommand_Type
DeviceCommandFetchCrdAvailabilityInfoJob::GetType() const {
  return RemoteCommand::FETCH_CRD_AVAILABILITY_INFO;
}

void DeviceCommandFetchCrdAvailabilityInfoJob::RunImpl(
    CallbackWithResult succeed_callback,
    CallbackWithResult failed_callback) {
  CalculateIsInManagedEnvironmentAsync(base::BindOnce(
      &DeviceCommandFetchCrdAvailabilityInfoJob::SendPayload,
      weak_ptr_factory_.GetWeakPtr(), std::move(succeed_callback)));
}

void DeviceCommandFetchCrdAvailabilityInfoJob::SendPayload(
    CallbackWithResult callback,
    bool is_in_managed_environment) {
  std::string payload =
      extensions::DictionaryBuilder()
          .Set(kIdleTime, static_cast<int>(GetDeviceIdleTime().InSeconds()))
          .Set(kUserSessionType, static_cast<int>(GetCurrentUserSessionType()))
          .Set(kIsInManagedEnvironment, is_in_managed_environment)
          .Set(kSupportedCrdSessionTypes,
               GetSupportedSessionTypes(is_in_managed_environment))
          .ToJSON();

  CRD_DVLOG(1) << "Finished FETCH_CRD_AVAILABILITY_INFO remote command: "
               << payload;
  std::move(callback).Run(std::move(payload));
}

}  // namespace policy

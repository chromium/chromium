// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/instance_id/instance_id_api.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

// Error messages.
const char kInvalidParameter[] = "Function was called with invalid parameters.";
const char kDisabled[] = "Instance ID is currently disabled.";
const char kAsyncOperationPending[] = "Asynchronous operation is pending.";
const char kNetworkError[] = "Network error occurred.";
const char kServerError[] = "Server error occurred.";
const char kUnknownError[] = "Unknown error occurred.";

const char* InstanceIDResultToError(instance_id::InstanceID::Result result) {
  switch (result) {
    case instance_id::InstanceID::INVALID_PARAMETER:
      return kInvalidParameter;
    case instance_id::InstanceID::DISABLED:
      return kDisabled;
    case instance_id::InstanceID::ASYNC_OPERATION_PENDING:
      return kAsyncOperationPending;
    case instance_id::InstanceID::NETWORK_ERROR:
      return kNetworkError;
    case instance_id::InstanceID::SERVER_ERROR:
      return kServerError;
    case instance_id::InstanceID::UNKNOWN_ERROR:
      return kUnknownError;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected value of result cannot be converted: " << result;
  }
  return "";
}

}  // namespace

InstanceIDApiFunction::InstanceIDApiFunction() = default;

InstanceIDApiFunction::~InstanceIDApiFunction() = default;

ExtensionFunction::ResponseAction InstanceIDApiFunction::Run() {
  if (Profile::FromBrowserContext(browser_context())->IsOffTheRecord()) {
    return RespondNow(Error(
        "chrome.instanceID not supported in incognito mode"));
  }
  return DoWork();
}

instance_id::InstanceID* InstanceIDApiFunction::GetInstanceID() const {
  return instance_id::InstanceIDProfileServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()))->driver()->
          GetInstanceID(extension()->id());
}

InstanceIDGetIDFunction::InstanceIDGetIDFunction() {}

InstanceIDGetIDFunction::~InstanceIDGetIDFunction() {}

ExtensionFunction::ResponseAction InstanceIDGetIDFunction::DoWork() {
  GetInstanceID()->GetID(
      base::BindOnce(&InstanceIDGetIDFunction::GetIDCompleted, this));
  return RespondLater();
}

void InstanceIDGetIDFunction::GetIDCompleted(const std::string& id) {
  Respond(WithArguments(id));
}

InstanceIDGetCreationTimeFunction::InstanceIDGetCreationTimeFunction() {}

InstanceIDGetCreationTimeFunction::~InstanceIDGetCreationTimeFunction() {}

ExtensionFunction::ResponseAction InstanceIDGetCreationTimeFunction::DoWork() {
  GetInstanceID()->GetCreationTime(base::BindOnce(
      &InstanceIDGetCreationTimeFunction::GetCreationTimeCompleted, this));
  return RespondLater();
}

void InstanceIDGetCreationTimeFunction::GetCreationTimeCompleted(
    const base::Time& creation_time) {
  Respond(WithArguments(creation_time.InSecondsFSinceUnixEpoch()));
}

InstanceIDGetTokenFunction::InstanceIDGetTokenFunction() {}

InstanceIDGetTokenFunction::~InstanceIDGetTokenFunction() {}

ExtensionFunction::ResponseAction InstanceIDGetTokenFunction::DoWork() {
  std::optional<api::instance_id::GetToken::Params> params =
      api::instance_id::GetToken::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GetInstanceID()->GetToken(
      params->get_token_params.authorized_entity,
      params->get_token_params.scope, /*time_to_live=*/base::TimeDelta(),
      /*flags=*/{},
      base::BindOnce(&InstanceIDGetTokenFunction::GetTokenCompleted, this));

  return RespondLater();
}

void InstanceIDGetTokenFunction::GetTokenCompleted(
    const std::string& token,
    instance_id::InstanceID::Result result) {
  if (result == instance_id::InstanceID::SUCCESS)
    Respond(WithArguments(token));
  else
    Respond(Error(InstanceIDResultToError(result)));
}

InstanceIDDeleteTokenFunction::InstanceIDDeleteTokenFunction() {}

InstanceIDDeleteTokenFunction::~InstanceIDDeleteTokenFunction() {}

ExtensionFunction::ResponseAction InstanceIDDeleteTokenFunction::DoWork() {
  std::optional<api::instance_id::DeleteToken::Params> params =
      api::instance_id::DeleteToken::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GetInstanceID()->DeleteToken(
      params->delete_token_params.authorized_entity,
      params->delete_token_params.scope,
      base::BindOnce(&InstanceIDDeleteTokenFunction::DeleteTokenCompleted,
                     this));

  return RespondLater();
}

void InstanceIDDeleteTokenFunction::DeleteTokenCompleted(
    instance_id::InstanceID::Result result) {
  if (result == instance_id::InstanceID::SUCCESS)
    Respond(NoArguments());
  else
    Respond(Error(InstanceIDResultToError(result)));
}

InstanceIDDeleteIDFunction::InstanceIDDeleteIDFunction() {}

InstanceIDDeleteIDFunction::~InstanceIDDeleteIDFunction() {}

ExtensionFunction::ResponseAction InstanceIDDeleteIDFunction::DoWork() {
  GetInstanceID()->DeleteID(
      base::BindOnce(&InstanceIDDeleteIDFunction::DeleteIDCompleted, this));

  return RespondLater();
}

void InstanceIDDeleteIDFunction::DeleteIDCompleted(
    instance_id::InstanceID::Result result) {
  if (result == instance_id::InstanceID::SUCCESS)
    Respond(NoArguments());
  else
    Respond(Error(InstanceIDResultToError(result)));
}

}  // namespace extensions

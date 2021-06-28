// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"
#include "chrome/browser/extensions/api/platform_keys/platform_keys_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/enterprise_platform_keys.h"
#include "chrome/common/extensions/api/enterprise_platform_keys_internal.h"
#include "chromeos/lacros/lacros_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

namespace {

namespace api_epk = api::enterprise_platform_keys;
namespace api_epki = api::enterprise_platform_keys_internal;
using KeystoreService = crosapi::mojom::KeystoreService;

std::vector<uint8_t> VectorFromString(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

std::string StringFromVector(const std::vector<uint8_t>& v) {
  return std::string(v.begin(), v.end());
}

const char kLacrosNotImplementedError[] = "Not implemented.";
const char kUnsupportedByAsh[] = "Not implemented.";
const char kUnsupportedProfile[] = "Not available.";
const char kExtensionDoesNotHavePermission[] =
    "The extension does not have permission to call this function.";

// Performs common crosapi validation. These errors are not caused by the
// extension so they are considered recoverable. Returns an error message on
// error, or empty string on success. |min_version| is the minimum version of
// the ash implementation of KeystoreService necessary to support this
// extension. |context| is the browser context in which the extension is hosted.
std::string ValidateCrosapi(int min_version, content::BrowserContext* context) {
  int version = chromeos::LacrosService::Get()->GetInterfaceVersion(
      KeystoreService::Uuid_);
  if (version < min_version)
    return kUnsupportedByAsh;

  // These APIs are used in security-sensitive contexts. We need to ensure that
  // the user for ash is the same as the user for lacros. We do this by
  // restricting the API to the default profile, which is guaranteed to be the
  // same user.
  if (!Profile::FromBrowserContext(context)->IsMainProfile())
    return kUnsupportedProfile;

  return "";
}

}  // namespace

//------------------------------------------------------------------------------

ExtensionFunction::ResponseAction LacrosNotImplementedExtensionFunction::Run() {
  return RespondNow(Error(kLacrosNotImplementedError));
}

//------------------------------------------------------------------------------

ExtensionFunction::ResponseAction
EnterprisePlatformKeysChallengeMachineKeyFunction::Run() {
  std::unique_ptr<api_epk::ChallengeMachineKey::Params> params(
      api_epk::ChallengeMachineKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string error = ValidateCrosapi(
      KeystoreService::kChallengeAttestationOnlyKeystoreMinVersion,
      browser_context());
  if (!error.empty())
    return RespondNow(Error(error));

  if (!platform_keys::IsExtensionAllowed(
          Profile::FromBrowserContext(browser_context()), extension())) {
    return RespondNow(Error(kExtensionDoesNotHavePermission));
  }

  auto c = base::BindOnce(&EnterprisePlatformKeysChallengeMachineKeyFunction::
                              OnChallengeAttestationOnlyKeystore,
                          this);
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::KeystoreService>()
      ->ChallengeAttestationOnlyKeystore(
          StringFromVector(params->challenge),
          crosapi::mojom::KeystoreType::kDevice,
          /*migrate=*/params->register_key ? *params->register_key : false,
          std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysChallengeMachineKeyFunction::
    OnChallengeAttestationOnlyKeystore(ResultPtr result) {
  using Result = crosapi::mojom::KeystoreStringResult;
  switch (result->which()) {
    case Result::Tag::ERROR_MESSAGE:
      Respond(Error(result->get_error_message()));
      return;
    case Result::Tag::CHALLENGE_RESPONSE:
      Respond(ArgumentList(api_epk::ChallengeMachineKey::Results::Create(
          VectorFromString(result->get_challenge_response()))));
      return;
  }
}

//------------------------------------------------------------------------------

ExtensionFunction::ResponseAction
EnterprisePlatformKeysChallengeUserKeyFunction::Run() {
  std::unique_ptr<api_epk::ChallengeUserKey::Params> params(
      api_epk::ChallengeUserKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string error = ValidateCrosapi(
      KeystoreService::kChallengeAttestationOnlyKeystoreMinVersion,
      browser_context());
  if (!error.empty())
    return RespondNow(Error(error));

  if (!platform_keys::IsExtensionAllowed(
          Profile::FromBrowserContext(browser_context()), extension())) {
    return RespondNow(Error(kExtensionDoesNotHavePermission));
  }

  auto c = base::BindOnce(&EnterprisePlatformKeysChallengeUserKeyFunction::
                              OnChallengeAttestationOnlyKeystore,
                          this);
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::KeystoreService>()
      ->ChallengeAttestationOnlyKeystore(StringFromVector(params->challenge),
                                         crosapi::mojom::KeystoreType::kUser,
                                         /*migrate=*/params->register_key,
                                         std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysChallengeUserKeyFunction::
    OnChallengeAttestationOnlyKeystore(ResultPtr result) {
  using Result = crosapi::mojom::KeystoreStringResult;
  switch (result->which()) {
    case Result::Tag::ERROR_MESSAGE:
      Respond(Error(result->get_error_message()));
      return;
    case Result::Tag::CHALLENGE_RESPONSE:
      Respond(ArgumentList(api_epk::ChallengeUserKey::Results::Create(
          VectorFromString(result->get_challenge_response()))));
      return;
  }
}

}  // namespace extensions

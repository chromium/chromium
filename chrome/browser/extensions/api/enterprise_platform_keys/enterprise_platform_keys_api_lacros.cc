// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/extensions/api/platform_keys/platform_keys_api.h"
#include "chrome/common/extensions/api/enterprise_platform_keys.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"

namespace extensions {

namespace {

namespace api_epk = api::enterprise_platform_keys;

std::vector<uint8_t> VectorFromString(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

std::string StringFromVector(const std::vector<uint8_t>& v) {
  return std::string(v.begin(), v.end());
}

const char kLacrosNotImplementedError[] = "not-implemented-yet-for-lacros";

}  // namespace

ExtensionFunction::ResponseAction LacrosNotImplementedExtensionFunction::Run() {
  return RespondNow(Error(kLacrosNotImplementedError));
}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysChallengeMachineKeyFunction::Run() {
  std::unique_ptr<api_epk::ChallengeMachineKey::Params> params(
      api_epk::ChallengeMachineKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  // TODO(https://crbug.com/1113443): This implementation needs to check if the
  // extension is allowlisted via the AttestationExtensionAllowlist policy.
  auto c = base::BindOnce(
      &EnterprisePlatformKeysChallengeMachineKeyFunction::OnChallengedKeyLacros,
      this);
  chromeos::LacrosChromeServiceImpl::Get()->attestation_remote()->ChallengeKey(
      StringFromVector(params->challenge),
      crosapi::mojom::ChallengeKeyType::kDevice, std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysChallengeMachineKeyFunction::OnChallengedKeyLacros(
    crosapi::mojom::ChallengeKeyResultPtr result) {
  switch (result->which()) {
    case crosapi::mojom::ChallengeKeyResult::Tag::ERROR_MESSAGE:
      Respond(Error(result->get_error_message()));
      return;
    case crosapi::mojom::ChallengeKeyResult::Tag::CHALLENGE_RESPONSE:
      Respond(ArgumentList(api_epk::ChallengeMachineKey::Results::Create(
          VectorFromString(result->get_challenge_response()))));
      return;
  }
}

}  // namespace extensions

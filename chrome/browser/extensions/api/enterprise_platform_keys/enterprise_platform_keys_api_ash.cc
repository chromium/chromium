// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api_ash.h"

#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/extensions/api/platform_keys/platform_keys_api.h"
#include "chrome/browser/platform_keys/extension_platform_keys_service.h"
#include "chrome/browser/platform_keys/extension_platform_keys_service_factory.h"
#include "chrome/browser/platform_keys/platform_keys.h"
#include "chrome/common/extensions/api/enterprise_platform_keys.h"
#include "chrome/common/extensions/api/enterprise_platform_keys_internal.h"
#include "chromeos/crosapi/mojom/keystore_error.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

namespace {

namespace api_epk = api::enterprise_platform_keys;
namespace api_epki = api::enterprise_platform_keys_internal;

std::vector<uint8_t> VectorFromString(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

std::string StringFromVector(const std::vector<uint8_t>& v) {
  return std::string(v.begin(), v.end());
}

}  // namespace

//------------------------------------------------------------------------------

EnterprisePlatformKeysChallengeMachineKeyFunction::
    EnterprisePlatformKeysChallengeMachineKeyFunction() = default;

EnterprisePlatformKeysChallengeMachineKeyFunction::
    ~EnterprisePlatformKeysChallengeMachineKeyFunction() = default;

ExtensionFunction::ResponseAction
EnterprisePlatformKeysChallengeMachineKeyFunction::Run() {
  std::unique_ptr<api_epk::ChallengeMachineKey::Params> params(
      api_epk::ChallengeMachineKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  ash::attestation::TpmChallengeKeyCallback callback = base::BindOnce(
      &EnterprisePlatformKeysChallengeMachineKeyFunction::OnChallengedKey,
      this);
  // base::Unretained is safe on impl_ since its life-cycle matches |this| and
  // |callback| holds a reference to |this|.
  base::OnceClosure task = base::BindOnce(
      &EPKPChallengeKey::Run, base::Unretained(&impl_),
      chromeos::attestation::KEY_DEVICE, scoped_refptr<ExtensionFunction>(this),
      std::move(callback), StringFromVector(params->challenge),
      params->register_key ? *params->register_key : false);
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(task));
  return RespondLater();
}

void EnterprisePlatformKeysChallengeMachineKeyFunction::OnChallengedKey(
    const ash::attestation::TpmChallengeKeyResult& result) {
  if (result.IsSuccess()) {
    Respond(ArgumentList(api_epk::ChallengeMachineKey::Results::Create(
        VectorFromString(result.challenge_response))));
  } else {
    Respond(Error(result.GetErrorMessage()));
  }
}

//------------------------------------------------------------------------------

EnterprisePlatformKeysChallengeUserKeyFunction::
    EnterprisePlatformKeysChallengeUserKeyFunction() = default;

EnterprisePlatformKeysChallengeUserKeyFunction::
    ~EnterprisePlatformKeysChallengeUserKeyFunction() = default;

ExtensionFunction::ResponseAction
EnterprisePlatformKeysChallengeUserKeyFunction::Run() {
  std::unique_ptr<api_epk::ChallengeUserKey::Params> params(
      api_epk::ChallengeUserKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  ash::attestation::TpmChallengeKeyCallback callback = base::BindOnce(
      &EnterprisePlatformKeysChallengeUserKeyFunction::OnChallengedKey, this);
  // base::Unretained is safe on impl_ since its life-cycle matches |this| and
  // |callback| holds a reference to |this|.
  base::OnceClosure task = base::BindOnce(
      &EPKPChallengeKey::Run, base::Unretained(&impl_),
      chromeos::attestation::KEY_USER, scoped_refptr<ExtensionFunction>(this),
      std::move(callback), StringFromVector(params->challenge),
      params->register_key);
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(task));
  return RespondLater();
}

void EnterprisePlatformKeysChallengeUserKeyFunction::OnChallengedKey(
    const ash::attestation::TpmChallengeKeyResult& result) {
  if (result.IsSuccess()) {
    Respond(ArgumentList(api_epk::ChallengeUserKey::Results::Create(
        VectorFromString(result.challenge_response))));
  } else {
    Respond(Error(result.GetErrorMessage()));
  }
}

}  // namespace extensions

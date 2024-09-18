// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys_private/enterprise_platform_keys_private_api.h"

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/api/platform_keys_core/platform_keys_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/enterprise_platform_keys_private.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/manifest.h"

namespace extensions {

namespace api_epkp = api::enterprise_platform_keys_private;

EPKPChallengeKey::EPKPChallengeKey() = default;
EPKPChallengeKey::~EPKPChallengeKey() = default;

// Check if the extension is allowisted in the user policy.
bool EPKPChallengeKey::IsExtensionAllowed(
    Profile* profile,
    scoped_refptr<const Extension> extension) {
  if (!ash::ProfileHelper::Get()->GetUserByProfile(profile)) {
    // Only allow remote attestation for apps that were force-installed on the
    // login/signin screen.
    // TODO(drcrash): Use a separate device-wide policy for the API.
    return Manifest::IsPolicyLocation(extension->location());
  }
  return platform_keys::IsExtensionAllowed(profile, extension.get());
}

void EPKPChallengeKey::Run(::attestation::VerifiedAccessFlow type,
                           scoped_refptr<ExtensionFunction> caller,
                           ash::attestation::TpmChallengeKeyCallback callback,
                           const std::string& challenge,
                           bool register_key) {
  Profile* profile = Profile::FromBrowserContext(caller->browser_context());

  if (!IsExtensionAllowed(profile, caller->extension())) {
    std::move(callback).Run(ash::attestation::TpmChallengeKeyResult::MakeError(
        ash::attestation::TpmChallengeKeyResultCode::
            kExtensionNotAllowedError));
    return;
  }

  std::string key_name_for_spkac;
  if (register_key && (type == ::attestation::ENTERPRISE_MACHINE)) {
    key_name_for_spkac = ash::attestation::kEnterpriseMachineKeyForSpkacPrefix +
                         caller->extension()->id();
  }

  impl_ = ash::attestation::TpmChallengeKeyFactory::Create();
  impl_->BuildResponse(type, profile, std::move(callback), challenge,
                       register_key, attestation::KEY_TYPE_RSA,
                       key_name_for_spkac, /*signals=*/std::nullopt);
}

EnterprisePlatformKeysPrivateChallengeMachineKeyFunction::
    EnterprisePlatformKeysPrivateChallengeMachineKeyFunction() = default;

EnterprisePlatformKeysPrivateChallengeMachineKeyFunction::
    ~EnterprisePlatformKeysPrivateChallengeMachineKeyFunction() = default;

ExtensionFunction::ResponseAction
EnterprisePlatformKeysPrivateChallengeMachineKeyFunction::Run() {
  std::optional<api_epkp::ChallengeMachineKey::Params> params =
      api_epkp::ChallengeMachineKey::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  ash::attestation::TpmChallengeKeyCallback callback =
      base::BindOnce(&EnterprisePlatformKeysPrivateChallengeMachineKeyFunction::
                         OnChallengedKey,
                     this);

  std::string challenge;
  if (!base::Base64Decode(params->challenge, &challenge)) {
    auto result = ash::attestation::TpmChallengeKeyResult::MakeError(
        ash::attestation::TpmChallengeKeyResultCode::kChallengeBadBase64Error);
    return RespondNow(Error(result.GetErrorMessage()));
  }

  // base::Unretained is safe on impl_ since its life-cycle matches |this| and
  // |callback| holds a reference to |this|.
  base::OnceClosure task = base::BindOnce(
      &EPKPChallengeKey::Run, base::Unretained(&impl_),
      ::attestation::ENTERPRISE_MACHINE, scoped_refptr<ExtensionFunction>(this),
      std::move(callback), challenge,
      /*register_key=*/false);
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(task));
  return RespondLater();
}

void EnterprisePlatformKeysPrivateChallengeMachineKeyFunction::OnChallengedKey(
    const ash::attestation::TpmChallengeKeyResult& result) {
  if (result.IsSuccess()) {
    Respond(ArgumentList(api_epkp::ChallengeMachineKey::Results::Create(
        base::Base64Encode(result.challenge_response))));
  } else {
    Respond(Error(result.GetErrorMessage()));
  }
}

EnterprisePlatformKeysPrivateChallengeUserKeyFunction::
    EnterprisePlatformKeysPrivateChallengeUserKeyFunction() = default;

EnterprisePlatformKeysPrivateChallengeUserKeyFunction::
    ~EnterprisePlatformKeysPrivateChallengeUserKeyFunction() = default;

ExtensionFunction::ResponseAction
EnterprisePlatformKeysPrivateChallengeUserKeyFunction::Run() {
  std::optional<api_epkp::ChallengeUserKey::Params> params =
      api_epkp::ChallengeUserKey::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  ash::attestation::TpmChallengeKeyCallback callback = base::BindOnce(
      &EnterprisePlatformKeysPrivateChallengeUserKeyFunction::OnChallengedKey,
      this);

  std::string challenge;
  if (!base::Base64Decode(params->challenge, &challenge)) {
    auto result = ash::attestation::TpmChallengeKeyResult::MakeError(
        ash::attestation::TpmChallengeKeyResultCode::kChallengeBadBase64Error);
    return RespondNow(Error(result.GetErrorMessage()));
  }

  // base::Unretained is safe on impl_ since its life-cycle matches |this| and
  // |callback| holds a reference to |this|.
  base::OnceClosure task = base::BindOnce(
      &EPKPChallengeKey::Run, base::Unretained(&impl_),
      ::attestation::ENTERPRISE_USER, scoped_refptr<ExtensionFunction>(this),
      std::move(callback), challenge, params->register_key);
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(task));
  return RespondLater();
}

void EnterprisePlatformKeysPrivateChallengeUserKeyFunction::OnChallengedKey(
    const ash::attestation::TpmChallengeKeyResult& result) {
  if (result.IsSuccess()) {
    Respond(ArgumentList(api_epkp::ChallengeUserKey::Results::Create(
        base::Base64Encode(result.challenge_response))));
  } else {
    Respond(Error(result.GetErrorMessage()));
  }
}

}  // namespace extensions

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys_private/enterprise_platform_keys_private_api.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/enterprise_platform_keys_private.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/manifest.h"

namespace {
// Prefix for naming machine keys used for SignedPublicKeyAndChallenge when
// challenging the EMK with register=true.
const char kEnterpriseMachineKeyForSpkacPrefix[] = "attest-ent-machine-";
}  // namespace

namespace extensions {

namespace api_epkp = api::enterprise_platform_keys_private;

EPKPChallengeKey::EPKPChallengeKey() = default;
EPKPChallengeKey::~EPKPChallengeKey() = default;

void EPKPChallengeKey::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kAttestationExtensionWhitelist);
}

// Check if the extension is whitelisted in the user policy.
bool EPKPChallengeKey::IsExtensionWhitelisted(
    Profile* profile,
    scoped_refptr<const Extension> extension) {
  if (!chromeos::ProfileHelper::Get()->GetUserByProfile(profile)) {
    // Only allow remote attestation for apps that were force-installed on the
    // login/signin screen.
    // TODO(drcrash): Use a separate device-wide policy for the API.
    return Manifest::IsPolicyLocation(extension->location());
  }
  if (Manifest::IsComponentLocation(extension->location())) {
    // Note: For this to even be called, the component extension must also be
    // whitelisted in chrome/common/extensions/api/_permission_features.json
    return true;
  }
  const base::ListValue* list =
      profile->GetPrefs()->GetList(prefs::kAttestationExtensionWhitelist);
  base::Value value(extension->id());
  return list->Find(value) != list->end();
}

void EPKPChallengeKey::Run(
    chromeos::attestation::AttestationKeyType type,
    scoped_refptr<ExtensionFunction> caller,
    chromeos::attestation::TpmChallengeKeyCallback callback,
    const std::string& challenge,
    bool register_key) {
  Profile* profile = ChromeExtensionFunctionDetails(caller.get()).GetProfile();

  if (!IsExtensionWhitelisted(profile, caller->extension())) {
    std::move(callback).Run(
        chromeos::attestation::TpmChallengeKeyResult::MakeError(
            chromeos::attestation::TpmChallengeKeyResultCode::
                kExtensionNotWhitelistedError));
    return;
  }

  std::string key_name_for_spkac;
  if (register_key && (type == chromeos::attestation::KEY_DEVICE)) {
    key_name_for_spkac =
        kEnterpriseMachineKeyForSpkacPrefix + caller->extension()->id();
  }

  impl_ = chromeos::attestation::TpmChallengeKeyFactory::Create();
  impl_->BuildResponse(type, profile, std::move(callback), challenge,
                       register_key, key_name_for_spkac);
}

EnterprisePlatformKeysPrivateChallengeMachineKeyFunction::
    EnterprisePlatformKeysPrivateChallengeMachineKeyFunction() = default;

EnterprisePlatformKeysPrivateChallengeMachineKeyFunction::
    ~EnterprisePlatformKeysPrivateChallengeMachineKeyFunction() = default;

ExtensionFunction::ResponseAction
EnterprisePlatformKeysPrivateChallengeMachineKeyFunction::Run() {
  std::unique_ptr<api_epkp::ChallengeMachineKey::Params> params(
      api_epkp::ChallengeMachineKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  chromeos::attestation::TpmChallengeKeyCallback callback =
      base::Bind(&EnterprisePlatformKeysPrivateChallengeMachineKeyFunction::
                     OnChallengedKey,
                 this);

  std::string challenge;
  if (!base::Base64Decode(params->challenge, &challenge)) {
    auto result = chromeos::attestation::TpmChallengeKeyResult::MakeError(
        chromeos::attestation::TpmChallengeKeyResultCode::
            kChallengeBadBase64Error);
    return RespondNow(Error(result.GetErrorMessage()));
  }

  // base::Unretained is safe on impl_ since its life-cycle matches |this| and
  // |callback| holds a reference to |this|.
  base::OnceClosure task = base::BindOnce(
      &EPKPChallengeKey::Run, base::Unretained(&impl_),
      chromeos::attestation::KEY_DEVICE, scoped_refptr<ExtensionFunction>(this),
      std::move(callback), challenge,
      /*register_key=*/false);
  base::PostTask(FROM_HERE, {content::BrowserThread::UI}, std::move(task));
  return RespondLater();
}

void EnterprisePlatformKeysPrivateChallengeMachineKeyFunction::OnChallengedKey(
    const chromeos::attestation::TpmChallengeKeyResult& result) {
  if (result.IsSuccess()) {
    std::string encoded_response;
    base::Base64Encode(result.data, &encoded_response);
    Respond(ArgumentList(
        api_epkp::ChallengeMachineKey::Results::Create(encoded_response)));
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
  std::unique_ptr<api_epkp::ChallengeUserKey::Params> params(
      api_epkp::ChallengeUserKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  chromeos::attestation::TpmChallengeKeyCallback callback = base::Bind(
      &EnterprisePlatformKeysPrivateChallengeUserKeyFunction::OnChallengedKey,
      this);

  std::string challenge;
  if (!base::Base64Decode(params->challenge, &challenge)) {
    auto result = chromeos::attestation::TpmChallengeKeyResult::MakeError(
        chromeos::attestation::TpmChallengeKeyResultCode::
            kChallengeBadBase64Error);
    return RespondNow(Error(result.GetErrorMessage()));
  }

  // base::Unretained is safe on impl_ since its life-cycle matches |this| and
  // |callback| holds a reference to |this|.
  base::OnceClosure task = base::BindOnce(
      &EPKPChallengeKey::Run, base::Unretained(&impl_),
      chromeos::attestation::KEY_USER, scoped_refptr<ExtensionFunction>(this),
      std::move(callback), challenge, params->register_key);
  base::PostTask(FROM_HERE, {content::BrowserThread::UI}, std::move(task));
  return RespondLater();
}

void EnterprisePlatformKeysPrivateChallengeUserKeyFunction::OnChallengedKey(
    const chromeos::attestation::TpmChallengeKeyResult& result) {
  if (result.IsSuccess()) {
    std::string encoded_response;
    base::Base64Encode(result.data, &encoded_response);
    Respond(ArgumentList(
        api_epkp::ChallengeUserKey::Results::Create(encoded_response)));
  } else {
    Respond(Error(result.GetErrorMessage()));
  }
}

}  // namespace extensions

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains code that is shared between Ash and Lacros.
// Lacros/Ash-specific counterparts are implemented in separate files.

#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"

#include "base/values.h"
#include "chrome/browser/extensions/api/platform_keys/platform_keys_api.h"
#include "chrome/browser/platform_keys/extension_platform_keys_service.h"
#include "chrome/browser/platform_keys/extension_platform_keys_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/enterprise_platform_keys.h"
#include "chrome/common/extensions/api/enterprise_platform_keys_internal.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

namespace extensions {

namespace {
namespace api_epk = api::enterprise_platform_keys;
namespace api_epki = api::enterprise_platform_keys_internal;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
const char kUnsupportedProfile[] = "Not available.";
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}  // namespace

namespace platform_keys {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kAttestationExtensionAllowlist);
}

bool IsExtensionAllowed(Profile* profile, const Extension* extension) {
  if (Manifest::IsComponentLocation(extension->location())) {
    // Note: For this to even be called, the component extension must also be
    // allowed in chrome/common/extensions/api/_permission_features.json
    return true;
  }
  const base::ListValue* list =
      profile->GetPrefs()->GetList(prefs::kAttestationExtensionAllowlist);
  DCHECK_NE(list, nullptr);
  base::Value value(extension->id());
  return std::find(list->GetList().begin(), list->GetList().end(), value) !=
         list->GetList().end();
}

}  // namespace platform_keys

//------------------------------------------------------------------------------

EnterprisePlatformKeysInternalGenerateKeyFunction::
    ~EnterprisePlatformKeysInternalGenerateKeyFunction() = default;

ExtensionFunction::ResponseAction
EnterprisePlatformKeysInternalGenerateKeyFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(b/191958380): Lift the restriction when *.platformKeys.* APIs are
  // implemented for secondary profiles in Lacros.
  if (!Profile::FromBrowserContext(browser_context())->IsMainProfile())
    return RespondNow(Error(kUnsupportedProfile));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  std::unique_ptr<api_epki::GenerateKey::Params> params(
      api_epki::GenerateKey::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params);
  absl::optional<chromeos::platform_keys::TokenId> platform_keys_token_id =
      platform_keys::ApiIdToPlatformKeysTokenId(params->token_id);
  if (!platform_keys_token_id)
    return RespondNow(Error(platform_keys::kErrorInvalidToken));

  chromeos::ExtensionPlatformKeysService* service =
      chromeos::ExtensionPlatformKeysServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);

  if (params->algorithm.name == "RSASSA-PKCS1-v1_5") {
    // TODO(pneubeck): Add support for unsigned integers to IDL.
    EXTENSION_FUNCTION_VALIDATE(params->algorithm.modulus_length &&
                                *(params->algorithm.modulus_length) >= 0);
    service->GenerateRSAKey(
        platform_keys_token_id.value(), *(params->algorithm.modulus_length),
        extension_id(),
        base::BindOnce(
            &EnterprisePlatformKeysInternalGenerateKeyFunction::OnGeneratedKey,
            this));
  } else if (params->algorithm.name == "ECDSA") {
    EXTENSION_FUNCTION_VALIDATE(params->algorithm.named_curve);
    service->GenerateECKey(
        platform_keys_token_id.value(), *(params->algorithm.named_curve),
        extension_id(),
        base::BindOnce(
            &EnterprisePlatformKeysInternalGenerateKeyFunction::OnGeneratedKey,
            this));
  } else {
    NOTREACHED();
    EXTENSION_FUNCTION_VALIDATE(false);
  }
  return RespondLater();
}

void EnterprisePlatformKeysInternalGenerateKeyFunction::OnGeneratedKey(
    const std::string& public_key_der,
    absl::optional<crosapi::mojom::KeystoreError> error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!error) {
    Respond(ArgumentList(api_epki::GenerateKey::Results::Create(
        std::vector<uint8_t>(public_key_der.begin(), public_key_der.end()))));
  } else {
    Respond(
        Error(chromeos::platform_keys::KeystoreErrorToString(error.value())));
  }
}

//------------------------------------------------------------------------------

}  // namespace extensions

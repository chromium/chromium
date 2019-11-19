// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cryptotoken_private/cryptotoken_private_api.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/permissions/attestation_permission_request.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "crypto/sha2.h"
#include "device/fido/features.h"
#include "extensions/common/error_utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/origin.h"

#if defined(OS_WIN)
#include "device/fido/win/webauthn_api.h"
#endif  // defined(OS_WIN)

namespace {

// U2FAttestationPromptResult enumerates events related to attestation prompts.
// These values are recorded in an UMA histogram and so should not be
// reassigned.
enum class U2FAttestationPromptResult {
  // kQueried indicates that the embedder was queried in order to determine
  // whether attestation information should be returned to the origin.
  kQueried = 0,
  // kAllowed indicates that the query to the embedder was resolved positively.
  // (E.g. the user clicked to allow, or the embedder allowed immediately by
  // policy.) Note that this may still be recorded if the user clicks to allow
  // attestation after the request has timed out.
  kAllowed = 1,
  // kBlocked indicates that the query to the embedder was resolved negatively.
  // (E.g. the user clicked to block, or closed the dialog.) Navigating away or
  // closing the tab also fall into this bucket.
  kBlocked = 2,
  kMaxValue = kBlocked,
};

const char kGoogleDotCom[] = "google.com";
constexpr const char* kGoogleGstaticAppIds[] = {
    "https://www.gstatic.com/securitykey/origins.json",
    "https://www.gstatic.com/securitykey/a/google.com/origins.json"};

// ContainsAppIdByHash returns true iff the SHA-256 hash of one of the
// elements of |list| equals |hash|.
bool ContainsAppIdByHash(const base::ListValue& list,
                         const std::vector<uint8_t>& hash) {
  if (hash.size() != crypto::kSHA256Length) {
    return false;
  }

  for (const auto& i : list) {
    const std::string& s = i.GetString();
    if (s.find('/') == std::string::npos) {
      // No slashes mean that this is a webauthn RP ID, not a U2F AppID.
      continue;
    }

    if (crypto::SHA256HashString(s).compare(
            0, crypto::kSHA256Length,
            reinterpret_cast<const char*>(hash.data()),
            crypto::kSHA256Length) == 0) {
      return true;
    }
  }

  return false;
}

void RecordAttestationEvent(U2FAttestationPromptResult event) {
  UMA_HISTOGRAM_ENUMERATION("WebAuthentication.U2FAttestationPromptResult",
                            event);
}

}  // namespace

namespace extensions {

namespace api {

void CryptotokenRegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kSecurityKeyPermitAttestation);
}

CryptotokenPrivateCanOriginAssertAppIdFunction::
    CryptotokenPrivateCanOriginAssertAppIdFunction() = default;

ExtensionFunction::ResponseAction
CryptotokenPrivateCanOriginAssertAppIdFunction::Run() {
  std::unique_ptr<cryptotoken_private::CanOriginAssertAppId::Params> params =
      cryptotoken_private::CanOriginAssertAppId::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  const GURL origin_url(params->security_origin);
  if (!origin_url.is_valid()) {
    return RespondNow(Error(extensions::ErrorUtils::FormatErrorMessage(
        "Security origin * is not a valid URL", params->security_origin)));
  }
  const GURL app_id_url(params->app_id_url);
  if (!app_id_url.is_valid()) {
    return RespondNow(Error(extensions::ErrorUtils::FormatErrorMessage(
        "appId * is not a valid URL", params->app_id_url)));
  }

  if (origin_url == app_id_url) {
    return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
  }

  // Fetch the eTLD+1 of both.
  const std::string origin_etldp1 =
      net::registry_controlled_domains::GetDomainAndRegistry(
          origin_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (origin_etldp1.empty()) {
    return RespondNow(Error(extensions::ErrorUtils::FormatErrorMessage(
        "Could not find an eTLD for origin *", params->security_origin)));
  }
  const std::string app_id_etldp1 =
      net::registry_controlled_domains::GetDomainAndRegistry(
          app_id_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (app_id_etldp1.empty()) {
    return RespondNow(Error(extensions::ErrorUtils::FormatErrorMessage(
        "Could not find an eTLD for appId *", params->app_id_url)));
  }
  if (origin_etldp1 == app_id_etldp1) {
    return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
  }
  // For legacy purposes, allow google.com origins to assert certain
  // gstatic.com appIds.
  // TODO(juanlang): remove when legacy constraints are removed.
  if (origin_etldp1 == kGoogleDotCom) {
    for (const char* id : kGoogleGstaticAppIds) {
      if (params->app_id_url == id)
        return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
    }
  }
  return RespondNow(OneArgument(std::make_unique<base::Value>(false)));
}

CryptotokenPrivateIsAppIdHashInEnterpriseContextFunction::
    CryptotokenPrivateIsAppIdHashInEnterpriseContextFunction() {}

ExtensionFunction::ResponseAction
CryptotokenPrivateIsAppIdHashInEnterpriseContextFunction::Run() {
  std::unique_ptr<cryptotoken_private::IsAppIdHashInEnterpriseContext::Params>
      params(
          cryptotoken_private::IsAppIdHashInEnterpriseContext::Params::Create(
              *args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  const PrefService* const prefs = profile->GetPrefs();
  const base::ListValue* const permit_attestation =
      prefs->GetList(prefs::kSecurityKeyPermitAttestation);

  return RespondNow(ArgumentList(
      cryptotoken_private::IsAppIdHashInEnterpriseContext::Results::Create(
          ContainsAppIdByHash(*permit_attestation, params->app_id_hash))));
}

CryptotokenPrivateCanAppIdGetAttestationFunction::
    CryptotokenPrivateCanAppIdGetAttestationFunction() {}

ExtensionFunction::ResponseAction
CryptotokenPrivateCanAppIdGetAttestationFunction::Run() {
  std::unique_ptr<cryptotoken_private::CanAppIdGetAttestation::Params> params =
      cryptotoken_private::CanAppIdGetAttestation::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  const GURL origin_url(params->options.origin);
  if (!origin_url.is_valid()) {
    return RespondNow(Error(extensions::ErrorUtils::FormatErrorMessage(
        "Security origin * is not a valid URL", params->options.origin)));
  }
  const url::Origin origin(url::Origin::Create(origin_url));

  const std::string& app_id = params->options.app_id;

  // If the appId is permitted by the enterprise policy then no permission
  // prompt is shown.
  Profile* const profile = Profile::FromBrowserContext(browser_context());
  const PrefService* const prefs = profile->GetPrefs();
  const base::ListValue* const permit_attestation =
      prefs->GetList(prefs::kSecurityKeyPermitAttestation);

  if (std::find_if(permit_attestation->begin(), permit_attestation->end(),
                   [&app_id](const base::Value& v) -> bool {
                     return v.GetString() == app_id;
                   }) != permit_attestation->end()) {
    return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
  }

  // If prompting is disabled, allow attestation because that is the historical
  // behavior.
  if (!base::FeatureList::IsEnabled(
          ::features::kSecurityKeyAttestationPrompt)) {
    return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
  }

#if defined(OS_WIN)
  // If the request was handled by the Windows WebAuthn API on a version of
  // Windows that shows an attestation permission prompt, don't show another
  // one.
  //
  // Note that this does not account for the possibility of the
  // WinWebAuthnApi having been disabled by a FidoDiscoveryFactory override,
  // which may be done in tests or via the Virtual Authenticator WebDriver
  // API.
  if (base::FeatureList::IsEnabled(device::kWebAuthUseNativeWinApi) &&
      device::WinWebAuthnApi::GetDefault()->IsAvailable() &&
      device::WinWebAuthnApi::GetDefault()->Version() >=
          WEBAUTHN_API_VERSION_2) {
    return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
  }
#endif  // defined(OS_WIN)

  // Otherwise, show a permission prompt and pass the user's decision back.
  const GURL app_id_url(app_id);
  EXTENSION_FUNCTION_VALIDATE(app_id_url.is_valid());

  content::WebContents* web_contents = nullptr;
  if (!ExtensionTabUtil::GetTabById(params->options.tab_id, browser_context(),
                                    true /* include incognito windows */,
                                    &web_contents)) {
    return RespondNow(Error("cannot find specified tab"));
  }

  PermissionRequestManager* permission_request_manager =
      PermissionRequestManager::FromWebContents(web_contents);
  if (!permission_request_manager) {
    return RespondNow(Error("no PermissionRequestManager"));
  }

  RecordAttestationEvent(U2FAttestationPromptResult::kQueried);
  // The created AttestationPermissionRequest deletes itself once complete.
  permission_request_manager->AddRequest(NewAttestationPermissionRequest(
      origin,
      base::BindOnce(
          &CryptotokenPrivateCanAppIdGetAttestationFunction::Complete, this)));
  return RespondLater();
}

void CryptotokenPrivateCanAppIdGetAttestationFunction::Complete(bool result) {
  RecordAttestationEvent(result ? U2FAttestationPromptResult::kAllowed
                                : U2FAttestationPromptResult::kBlocked);
  Respond(OneArgument(std::make_unique<base::Value>(result)));
}

}  // namespace api
}  // namespace extensions

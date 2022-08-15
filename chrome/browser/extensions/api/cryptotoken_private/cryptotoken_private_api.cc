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
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/permissions/attestation_permission_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/permissions/permission_request_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "crypto/sha2.h"
#include "device/fido/features.h"
#include "device/fido/filter.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/features.h"
#include "device/fido/win/webauthn_api.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

namespace extensions {

namespace api {

namespace {

const char kGoogleDotCom[] = "google.com";
constexpr const char* kGoogleGstaticAppIds[] = {
    "https://www.gstatic.com/securitykey/origins.json",
    "https://www.gstatic.com/securitykey/a/google.com/origins.json"};

// ContainsAppIdByHash returns true iff the SHA-256 hash of one of the
// elements of |list| equals |hash|.
bool ContainsAppIdByHash(const base::Value::List& list,
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

content::RenderFrameHost* RenderFrameHostForTabAndFrameId(
    content::BrowserContext* const browser_context,
    const int tab_id,
    const int frame_id) {
  content::WebContents* web_contents = nullptr;
  if (!ExtensionTabUtil::GetTabById(tab_id, browser_context,
                                    /*include_incognito=*/true,
                                    &web_contents)) {
    return nullptr;
  }
  return ExtensionApiFrameIdMap::GetRenderFrameHostById(web_contents, frame_id);
}

}  // namespace

void CryptotokenRegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kSecurityKeyPermitAttestation);
}

CryptotokenPrivateCanOriginAssertAppIdFunction::
    CryptotokenPrivateCanOriginAssertAppIdFunction() = default;

ExtensionFunction::ResponseAction
CryptotokenPrivateCanOriginAssertAppIdFunction::Run() {
  std::unique_ptr<cryptotoken_private::CanOriginAssertAppId::Params> params =
      cryptotoken_private::CanOriginAssertAppId::Params::Create(args());
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
    return RespondNow(WithArguments(true));
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
    return RespondNow(WithArguments(true));
  }
  // For legacy purposes, allow google.com origins to assert certain
  // gstatic.com appIds.
  // TODO(juanlang): remove when legacy constraints are removed.
  if (origin_etldp1 == kGoogleDotCom) {
    for (const char* id : kGoogleGstaticAppIds) {
      if (params->app_id_url == id)
        return RespondNow(WithArguments(true));
    }
  }
  return RespondNow(WithArguments(false));
}

CryptotokenPrivateIsAppIdHashInEnterpriseContextFunction::
    CryptotokenPrivateIsAppIdHashInEnterpriseContextFunction() {}

ExtensionFunction::ResponseAction
CryptotokenPrivateIsAppIdHashInEnterpriseContextFunction::Run() {
  std::unique_ptr<cryptotoken_private::IsAppIdHashInEnterpriseContext::Params>
      params(
          cryptotoken_private::IsAppIdHashInEnterpriseContext::Params::Create(
              args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  const PrefService* const prefs = profile->GetPrefs();
  const base::Value::List& permit_attestation =
      prefs->GetValueList(prefs::kSecurityKeyPermitAttestation);

  return RespondNow(ArgumentList(
      cryptotoken_private::IsAppIdHashInEnterpriseContext::Results::Create(
          ContainsAppIdByHash(permit_attestation, params->app_id_hash))));
}

CryptotokenPrivateCanAppIdGetAttestationFunction::
    CryptotokenPrivateCanAppIdGetAttestationFunction() {}

ExtensionFunction::ResponseAction
CryptotokenPrivateCanAppIdGetAttestationFunction::Run() {
  std::unique_ptr<cryptotoken_private::CanAppIdGetAttestation::Params> params =
      cryptotoken_private::CanAppIdGetAttestation::Params::Create(args());
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
  const base::Value::List& permit_attestation =
      prefs->GetValueList(prefs::kSecurityKeyPermitAttestation);

  for (const auto& entry : permit_attestation) {
    if (entry.GetString() == app_id)
      return RespondNow(WithArguments(true));
  }

  // If the origin is blocked, reject attestation.
  if (device::fido_filter::Evaluate(
          device::fido_filter::Operation::MAKE_CREDENTIAL, origin.Serialize(),
          /*device=*/absl::nullopt, /*id=*/absl::nullopt) ==
      device::fido_filter::Action::NO_ATTESTATION) {
    return RespondNow(WithArguments(false));
  }

  // If prompting is disabled, allow attestation because that is the historical
  // behavior.
  if (!base::FeatureList::IsEnabled(
          ::features::kSecurityKeyAttestationPrompt)) {
    return RespondNow(WithArguments(true));
  }

#if BUILDFLAG(IS_WIN)
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
    return RespondNow(WithArguments(true));
  }
#endif  // BUILDFLAG(IS_WIN)

  // Otherwise, show a permission prompt and pass the user's decision back.
  const GURL app_id_url(app_id);
  EXTENSION_FUNCTION_VALIDATE(app_id_url.is_valid());

  content::WebContents* web_contents = nullptr;
  if (!ExtensionTabUtil::GetTabById(params->options.tab_id, browser_context(),
                                    true /* include incognito windows */,
                                    &web_contents)) {
    return RespondNow(Error("cannot find specified tab"));
  }

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  if (!permission_request_manager) {
    return RespondNow(Error("no PermissionRequestManager"));
  }

  // The created AttestationPermissionRequest deletes itself once complete.
  permission_request_manager->AddRequest(
      web_contents
          ->GetPrimaryMainFrame(),  // Extension API targets a particular tab,
                                    // so select the current main frame to
                                    // handle the request.
      NewAttestationPermissionRequest(
          origin,
          base::BindOnce(
              &CryptotokenPrivateCanAppIdGetAttestationFunction::Complete,
              this)));
  return RespondLater();
}

void CryptotokenPrivateCanAppIdGetAttestationFunction::Complete(bool result) {
  Respond(WithArguments(result));
}

CryptotokenPrivateCanMakeU2fApiRequestFunction::
    CryptotokenPrivateCanMakeU2fApiRequestFunction() = default;

ExtensionFunction::ResponseAction
CryptotokenPrivateCanMakeU2fApiRequestFunction::Run() {
  auto params =
      cryptotoken_private::CanMakeU2fApiRequest::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // The `chrome.tabs` API doesn't work in Chrome OS sign-in contexts (e.g.
  // device login with some SAML provider making a U2F request). This means that
  // in these contexts we can't figure out the sender frame of the original U2F
  // API request and therefore can't show a permission prompt or check for
  // origin trial enrollment. Hence, just let these requests succeed without
  // further checks.
  if (!ash::ProfileHelper::IsRegularProfile(
          Profile::FromBrowserContext(browser_context()))) {
    DCHECK_EQ(params->options.tab_id, api::tabs::TAB_ID_NONE);
    return RespondNow(WithArguments(true));
  }
#endif

  content::WebContents* web_contents = nullptr;
  if (!ExtensionTabUtil::GetTabById(params->options.tab_id, browser_context(),
                                    true /* include incognito windows */,
                                    &web_contents)) {
    return RespondNow(Error("cannot find specified tab"));
  }

  content::RenderFrameHost* frame = RenderFrameHostForTabAndFrameId(
      browser_context(), params->options.tab_id, params->options.frame_id);
  if (!frame) {
    return RespondNow(Error("cannot find frame"));
  }
  frame->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kWarning,
      R"(The U2F Security Key API is deprecated and will be removed soon. If you own this website, please migrate to the Web Authentication API. For more information see https://groups.google.com/a/chromium.org/g/blink-dev/c/xHC3AtU_65A/m/yg20tsVFBAAJ)");

  blink::TrialTokenValidator validator;
  const net::HttpResponseHeaders* response_headers =
      frame->GetLastResponseHeaders();
  const bool u2f_api_origin_trial_enabled =
      (response_headers && validator.RequestEnablesFeature(
                               frame->GetLastCommittedURL(), response_headers,
                               extension_misc::kCryptotokenDeprecationTrialName,
                               base::Time::Now()));

  DCHECK(
      base::FeatureList::IsEnabled(extensions_features::kU2FSecurityKeyAPI) ||
      u2f_api_origin_trial_enabled);

  // Don't show a permission prompt if its feature flag is disabled, or if the
  // site enrolled in the deprecation trial (since they're obviously aware of
  // the deprecation).
  //
  // Also don't show the prompt in "non-regular" ChromeOS profiles, which
  // includes CrOS SAML sign-in context that doesn't support permission prompts
  // (crbug.com/1257293).
  if (!base::FeatureList::IsEnabled(device::kU2fPermissionPrompt) ||
      u2f_api_origin_trial_enabled) {
    return RespondNow(WithArguments(true));
  }

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  if (!permission_request_manager) {
    return RespondNow(Error("no PermissionRequestManager"));
  }

  const GURL origin_url(params->options.origin);
  if (!origin_url.is_valid()) {
    return RespondNow(Error(extensions::ErrorUtils::FormatErrorMessage(
        "invalid origin", params->options.origin)));
  }

  permission_request_manager->AddRequest(
      frame, NewU2fApiPermissionRequest(
                 url::Origin::Create(origin_url),
                 base::BindOnce(
                     &CryptotokenPrivateCanMakeU2fApiRequestFunction::Complete,
                     this)));
  return RespondLater();
}

void CryptotokenPrivateCanMakeU2fApiRequestFunction::Complete(bool result) {
  Respond(WithArguments(result));
}

ExtensionFunction::ResponseAction
CryptotokenPrivateRecordRegisterRequestFunction::Run() {
  auto params =
      cryptotoken_private::RecordRegisterRequest::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  content::RenderFrameHost* frame = RenderFrameHostForTabAndFrameId(
      browser_context(), params->tab_id, params->frame_id);
  if (!frame) {
    return RespondNow(Error("cannot find specified tab or frame"));
  }

  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      frame, blink::mojom::WebFeature::kU2FCryptotokenRegister);
  return RespondNow(WithArguments());
}

ExtensionFunction::ResponseAction
CryptotokenPrivateRecordSignRequestFunction::Run() {
  auto params = cryptotoken_private::RecordSignRequest::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  content::RenderFrameHost* frame = RenderFrameHostForTabAndFrameId(
      browser_context(), params->tab_id, params->frame_id);
  if (!frame) {
    return RespondNow(Error("cannot find specified tab or frame"));
  }

  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      frame, blink::mojom::WebFeature::kU2FCryptotokenSign);
  return RespondNow(WithArguments());
}

}  // namespace api
}  // namespace extensions

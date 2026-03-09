// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_internals_page_handler.h"

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/fre/fre_util.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace glic {

namespace {

mojom::ProfileEnablementPtr BuildProfileEnablement(
    content::BrowserContext* browser_context,
    const GlicActorPolicyChecker& actor_policy_checker) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile);

  auto result = mojom::ProfileEnablement::New();
  result->feature_disabled = enablement.feature_disabled;
  result->not_regular_profile = enablement.not_regular_profile;
  result->not_rolled_out = enablement.not_rolled_out;
  result->primary_account_not_capable = enablement.primary_account_not_capable;
  result->primary_account_not_fully_signed_in =
      enablement.primary_account_not_fully_signed_in;
  result->disallowed_by_chrome_policy = enablement.disallowed_by_chrome_policy;
  result->disallowed_by_remote_admin = enablement.disallowed_by_remote_admin;
  result->disallowed_by_remote_other = enablement.disallowed_by_remote_other;
  result->not_consented = enablement.not_consented;
  result->live_disallowed = enablement.live_disallowed;
  result->share_image_disallowed = enablement.share_image_disallowed;
  result->actuation_not_consented =
      profile->GetPrefs()->GetBoolean(prefs::kGlicUserEnabledActuationOnWeb) ==
      false;

  using CannotActReason = GlicActorPolicyChecker::CannotActReason;
  if (actor_policy_checker.CanActOnWeb()) {
    result->actuation_eligibility = mojom::ActuationEligibility::kEligible;
  } else {
    switch (actor_policy_checker.CannotActOnWebReason()) {
      case CannotActReason::kAccountCapabilityIneligible:
        result->actuation_eligibility =
            mojom::ActuationEligibility::kMissingAccountCapability;
        break;
      case CannotActReason::kAccountMissingChromeBenefits:
        result->actuation_eligibility =
            mojom::ActuationEligibility::kMissingChromeBenefits;
        break;
      case CannotActReason::kDisabledByPolicy:
        result->actuation_eligibility =
            mojom::ActuationEligibility::kDisabledByPolicy;
        break;
      case CannotActReason::kEnterpriseWithoutManagement:
        result->actuation_eligibility =
            mojom::ActuationEligibility::kEnterpriseWithoutManagement;
        break;
      case CannotActReason::kNone:
        NOTREACHED();
    }
  }

  return result;
}

}  // namespace

GlicInternalsPageHandler::GlicInternalsPageHandler(
    content::WebContents* webui_contents,
    mojo::PendingReceiver<glic::mojom::InternalsPageHandler> receiver)
    : webui_contents_(webui_contents),
      browser_context_(webui_contents->GetBrowserContext()),
      receiver_(this, std::move(receiver)) {}

GlicInternalsPageHandler::~GlicInternalsPageHandler() = default;

GlicKeyedService* GlicInternalsPageHandler::GetGlicService() {
  return GlicKeyedServiceFactory::GetGlicKeyedService(browser_context_);
}

void GlicInternalsPageHandler::GetInternalsDataPayload(
    GetInternalsDataPayloadCallback callback) {
  mojom::InternalsDataPayloadPtr payload = mojom::InternalsDataPayload::New();

  payload->enablement = BuildProfileEnablement(
      browser_context_, GetGlicService()->actor_policy_checker());

  mojom::ConfigInfoPtr config = mojom::ConfigInfo::New();
  config->guest_url = GetGuestURL();
  config->fre_guest_url =
      GetFreURL(Profile::FromBrowserContext(browser_context_));

  config->autopush_guest_url = GURL(g_browser_process->local_state()->GetString(
      prefs::kGlicGuestUrlPresetAutopush));
  config->staging_guest_url = GURL(g_browser_process->local_state()->GetString(
      prefs::kGlicGuestUrlPresetStaging));
  config->preprod_guest_url = GURL(g_browser_process->local_state()->GetString(
      prefs::kGlicGuestUrlPresetPreprod));
  config->prod_guest_url = GURL(g_browser_process->local_state()->GetString(
      prefs::kGlicGuestUrlPresetProd));

  payload->config = std::move(config);

  std::move(callback).Run(std::move(payload));
}

void GlicInternalsPageHandler::SetGuestUrlPresets(const GURL& autopush_url,
                                                  const GURL& staging_url,
                                                  const GURL& preprod_url,
                                                  const GURL& prod_url) {
  g_browser_process->local_state()->SetString(
      prefs::kGlicGuestUrlPresetAutopush, autopush_url.spec());
  g_browser_process->local_state()->SetString(prefs::kGlicGuestUrlPresetStaging,
                                              staging_url.spec());
  g_browser_process->local_state()->SetString(prefs::kGlicGuestUrlPresetPreprod,
                                              preprod_url.spec());
  g_browser_process->local_state()->SetString(prefs::kGlicGuestUrlPresetProd,
                                              prod_url.spec());
}

}  // namespace glic

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_internals_page_handler.h"

#include <cstdio>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/fre/fre_util.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"
#endif

namespace glic {

namespace {

mojom::ProfileEnablementPtr BuildProfileEnablement(
    content::BrowserContext* browser_context,
    const GlicActorPolicyChecker* actor_policy_checker) {
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
  result->disallowed_by_country_filter =
      enablement.disallowed_by_country_filter;
  result->disallowed_by_locale_filter = enablement.disallowed_by_locale_filter;
  result->live_disallowed = enablement.live_disallowed;
  result->share_image_disallowed = enablement.share_image_disallowed;
  auto* service = GlicKeyedService::Get(profile);
  result->actuation_not_consented =
      !(service && service->enabling().GetUserEnabledActuationOnWeb());

  using CannotActReason = ::glic::CannotActReason;
  if (actor_policy_checker) {
    if (actor_policy_checker->CanActOnWeb()) {
      result->actuation_eligibility = mojom::ActuationEligibility::kEligible;
    } else {
      switch (actor_policy_checker->CannotActOnWebReason()) {
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
  } else {
    // If there is no GlicKeyedService (e.g., due to country filter),
    // default to missing capability or disabled by policy.
    result->actuation_eligibility =
        mojom::ActuationEligibility::kMissingAccountCapability;
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

  GlicKeyedService* glic_service = GetGlicService();
  payload->enablement = BuildProfileEnablement(
      browser_context_,
      glic_service ? &glic_service->actor_policy_checker() : nullptr);

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

  config->web_continuity_originating_host_url =
      GURL(g_browser_process->local_state()->GetString(
          prefs::kGlicWebContinuityOriginatingHostUrlPreset));

  payload->show_error_allowed = Profile::FromBrowserContext(browser_context_)
                                    ->GetPrefs()
                                    ->GetBoolean(prefs::kGlicShowErrorAllowed);

  payload->experimental_triggering_enabled =
      base::FeatureList::IsEnabled(features::kGlicExperimentalTriggering);

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

void GlicInternalsPageHandler::TriggerInvokeFromInternalsAction(
    mojom::TriggerInvokeFromInternalsOptionsPtr mojo_options,
    TriggerInvokeFromInternalsActionCallback callback) {
  GlicKeyedService* service = GetGlicService();
  if (!service) {
    std::move(callback).Run(false, "No GlicKeyedService available.");
    return;
  }

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(webui_contents_);
  if (!tab) {
    std::move(callback).Run(false, "No active tab found.");
    return;
  }

  GlicInvokeOptions options{mojo_options->invocation_source};
  options.prompts = std::move(mojo_options->prompts);

  if (mojo_options->additional_context) {
    options.additional_context = AdditionalTabContext(
        std::move(mojo_options->additional_context),
        content::GlobalRenderFrameHostId(), PolicyCheck::kClipboard);
  }

  if (mojo_options->conversation->is_new_conversation()) {
    options.target.conversation = NewConversation();
  } else if (mojo_options->conversation->is_conversation_id()) {
    options.target.conversation = ConversationId{
        mojo_options->conversation->get_conversation_id(), std::nullopt};
  } else {
    options.target.conversation = DefaultConversation();
  }

  options.feature_mode = mojo_options->feature_mode;
  options.disable_zss = mojo_options->disable_zss;
  if (mojo_options->zss_config) {
    options.zss_config =
        ZssConfig(mojo_options->zss_config->additional_content);
  }
  options.skill_id = std::move(mojo_options->skill_id);
  options.error_message = std::move(mojo_options->error_message);
  options.timeout = mojo_options->timeout;
  options.fre_override = mojo_options->fre_override;
  options.wait_for_panel_open = mojo_options->wait_for_panel_open;
  options.target.actuation_target = mojo_options->actuation_target;

  auto split_callback = base::SplitOnceCallback(std::move(callback));

  options.on_success = base::BindOnce(
      [](TriggerInvokeFromInternalsActionCallback cb) {
        std::move(cb).Run(true, "");
      },
      std::move(split_callback.first));

  options.on_error = base::BindOnce(
      [](TriggerInvokeFromInternalsActionCallback cb, GlicInvokeError error) {
        std::string error_msg;
        switch (error) {
          case GlicInvokeError::kTimeout:
            error_msg = "Timeout";
            break;
          case GlicInvokeError::kInvalidConversationId:
            error_msg = "Invalid Conversation ID";
            break;
          case GlicInvokeError::kInvalidTab:
            error_msg = "Invalid Tab";
            break;
          case GlicInvokeError::kTabClosed:
            error_msg = "Tab Closed";
            break;
          case GlicInvokeError::kInstanceDestroyed:
            error_msg = "Instance Destroyed";
            break;
          case GlicInvokeError::kInvokeInProgress:
            error_msg = "Invoke In Progress";
            break;
          case GlicInvokeError::kUnknown:
          default:
            error_msg = "Unknown Error";
            break;
        }
        std::move(cb).Run(false, error_msg);
      },
      std::move(split_callback.second));

  BrowserWindowInterface* current_browser = tab->GetBrowserWindowInterface();
  if (mojo_options->surface->is_default_surface()) {
    options.target.surface = DefaultSurface{current_browser};
  } else if (mojo_options->surface->is_new_tab()) {
    NewTab new_tab{current_browser};
    new_tab.open_in_foreground =
        mojo_options->surface->get_new_tab()->open_in_foreground;
    options.target.surface = new_tab;
  }

  if (mojo_options->auto_submit) {
    GlicInvokeWithAutoSubmitOptions auto_submit_options;
    if (mojo_options->show_panel.has_value()) {
      auto_submit_options.show_panel = mojo_options->show_panel.value();
    }
    service->InvokeWithAutoSubmit(
        InvokeWithAutoSubmitPasskeyProvider::GetPassKey(), std::move(options),
        std::move(auto_submit_options));
  } else {
    static_cast<GlicInstanceCoordinatorImpl&>(service->instance_coordinator())
        .Invoke(std::move(options));
  }
}

void GlicInternalsPageHandler::SetWebContinuityOriginatingHostUrlPreset(
    const GURL& web_continuity_originating_host_url) {
  g_browser_process->local_state()->SetString(
      prefs::kGlicWebContinuityOriginatingHostUrlPreset,
      web_continuity_originating_host_url.spec());
}

void GlicInternalsPageHandler::SetShowErrorAllowed(bool allowed) {
  Profile::FromBrowserContext(browser_context_)
      ->GetPrefs()
      ->SetBoolean(prefs::kGlicShowErrorAllowed, allowed);
}

void GlicInternalsPageHandler::ShowExperimentalOptIn() {
#if !BUILDFLAG(IS_ANDROID)
  GlicKeyedService* service = GetGlicService();
  if (!service) {
    return;
  }

  service->opt_in_controller().ShowDialog(webui_contents_);
#endif
}

}  // namespace glic

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_internals_page_handler.h"

#include <cstdio>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/actor/glic_actor_policy_checker.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_hotkey.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_service.h"
#include "components/skills/features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/device_form_factor.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"
#endif

#include <sstream>

#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace glic {

namespace {

mojom::ProfileEnablementPtr BuildProfileEnablement(
    content::BrowserContext* browser_context,
    const GlicActorPolicyChecker* actor_policy_checker) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  GlicEnabling::ProfileEnablement enablement =
      GlicEnabling::EnablementForProfile(profile);

  auto result = mojom::ProfileEnablement::New();
  result->feature_enabled = enablement.feature_enabled;
  result->is_regular_profile = enablement.is_regular_profile;
  result->is_rolled_out = enablement.is_rolled_out;
  result->primary_account_is_capable = enablement.primary_account_is_capable;
  result->primary_account_is_fully_signed_in =
      enablement.primary_account_is_fully_signed_in;
  result->allowed_by_chrome_policy = enablement.allowed_by_chrome_policy;
  result->allowed_by_remote_admin = enablement.allowed_by_remote_admin;
  result->allowed_by_remote_other = enablement.allowed_by_remote_other;
  result->fre_is_consented = enablement.fre_is_consented;
  result->allowed_by_country_filter = enablement.allowed_by_country_filter;
  result->allowed_by_locale_filter = enablement.allowed_by_locale_filter;
  result->live_allowed = enablement.live_allowed;
  result->share_image_allowed = enablement.share_image_allowed;
  if (enablement.gemini_enterprise_settings) {
    result->gemini_enterprise_settings =
        glic::mojom::GeminiEnterpriseSettings::New(
            enablement.gemini_enterprise_settings->project_id,
            enablement.gemini_enterprise_settings->app_id,
            enablement.gemini_enterprise_settings->location);
  }
  auto* service = GlicKeyedService::Get(profile);
  result->actuation_is_consented =
      (service && service->enabling().GetUserEnabledActuationOnWeb());

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

std::string InvocationSourceToString(glic::mojom::InvocationSource source) {
  // LINT.IfChange(InvocationSource)
  switch (source) {
    case glic::mojom::InvocationSource::kOsButton:
      return "kOsButton";
    case glic::mojom::InvocationSource::kOsButtonMenu:
      return "kOsButtonMenu";
    case glic::mojom::InvocationSource::kOsHotkey:
      return "kOsHotkey";
    case glic::mojom::InvocationSource::kTopChromeButton:
      return "kTopChromeButton";
    case glic::mojom::InvocationSource::kFre:
      return "kFre";
    case glic::mojom::InvocationSource::kProfilePicker:
      return "kProfilePicker";
    case glic::mojom::InvocationSource::kNudge:
      return "kNudge";
    case glic::mojom::InvocationSource::kThreeDotsMenu:
      return "kThreeDotsMenu";
    case glic::mojom::InvocationSource::kUnsupported:
      return "kUnsupported";
    case glic::mojom::InvocationSource::kWhatsNew:
      return "kWhatsNew";
    case glic::mojom::InvocationSource::kAfterSignIn:
      return "kAfterSignIn";
    case glic::mojom::InvocationSource::kSharedTab:
      return "kSharedTab";
    case glic::mojom::InvocationSource::kActorTaskIcon:
      return "kActorTaskIcon";
    case glic::mojom::InvocationSource::kSharedImage:
      return "kSharedImage";
    case glic::mojom::InvocationSource::kHandoffButton:
      return "kHandoffButton";
    case glic::mojom::InvocationSource::kSkills:
      return "kSkills";
    case glic::mojom::InvocationSource::kAutoOpenedByContextualCue:
      return "kAutoOpenedByContextualCue";
    case glic::mojom::InvocationSource::kPdfSummarizeButton:
      return "kPdfSummarizeButton";
    case glic::mojom::InvocationSource::kNavigationCapture:
      return "kNavigationCapture";
    case glic::mojom::InvocationSource::kAutoOpenedForPdf:
      return "kAutoOpenedForPdf";
    case glic::mojom::InvocationSource::kCaptureRegionHotkey:
      return "kCaptureRegionHotkey";
    case glic::mojom::InvocationSource::kIph:
      return "kIph";
    case glic::mojom::InvocationSource::kAnchoredContextualCue:
      return "kAnchoredContextualCue";
    case glic::mojom::InvocationSource::kWebContentsContextMenu:
      return "kWebContentsContextMenu";
    case glic::mojom::InvocationSource::kTextSelectionNudge:
      return "kTextSelectionNudge";
    case glic::mojom::InvocationSource::kTextSelectionWidget:
      return "kTextSelectionWidget";
    case glic::mojom::InvocationSource::kZeroStateAutoSummarize:
      return "kZeroStateAutoSummarize";
    case glic::mojom::InvocationSource::kUniversalCart:
      return "kUniversalCart";
    case glic::mojom::InvocationSource::kExperimentalTriggering:
      return "kExperimentalTriggering";
    case glic::mojom::InvocationSource::kPasswordChange:
      return "kPasswordChange";
    case glic::mojom::InvocationSource::kAutofill:
      return "kAutofill";
    case glic::mojom::InvocationSource::kToolbarButton:
      return "kToolbarButton";
    case glic::mojom::InvocationSource::kIndigoPageAction:
      return "kIndigoPageAction";
    case glic::mojom::InvocationSource::kWebDragDrop:
      return "kWebDragDrop";
    case glic::mojom::InvocationSource::kPromotionPage:
      return "kPromotionPage";
  }
  LOG(ERROR) << "Unexpected value for InvocationSource: "
             << static_cast<int>(source);
  return "Unknown";
  // LINT.ThenChange(//chrome/browser/glic/host/glic.mojom:InvocationSource)
}

std::string FeatureModeToString(glic::mojom::FeatureMode mode) {
  switch (mode) {
    case glic::mojom::FeatureMode::kUnspecified:
      return "kUnspecified";
    case glic::mojom::FeatureMode::kImageGeneration:
      return "kImageGeneration";
    case glic::mojom::FeatureMode::kActuation:
      return "kActuation";
    case glic::mojom::FeatureMode::kExperimentalTriggering:
      return "kExperimentalTriggering";
    case glic::mojom::FeatureMode::kUniversalCart:
      return "kUniversalCart";
    case glic::mojom::FeatureMode::kPromotionPage:
      return "kPromotionPage";
  }
  LOG(ERROR) << "Unexpected value for FeatureMode: " << static_cast<int>(mode);
  return "Unknown";
}

std::string FreOverrideToString(glic::mojom::FreOverride fre_override) {
  switch (fre_override) {
    case glic::mojom::FreOverride::kUnspecified:
      return "kUnspecified";
    case glic::mojom::FreOverride::kTrustFirstText:
      return "kTrustFirstText";
    case glic::mojom::FreOverride::kTrustFirstClick:
      return "kTrustFirstClick";
    case glic::mojom::FreOverride::kTrustFirstInline:
      return "kTrustFirstInline";
  }
  LOG(ERROR) << "Unexpected value for FreOverride: "
             << static_cast<int>(fre_override);
  return "Unknown";
}

std::string ActuationTargetToString(glic::mojom::ActuationTarget target) {
  switch (target) {
    case glic::mojom::ActuationTarget::kUnknown:
      return "kUnknown";
    case glic::mojom::ActuationTarget::kAgentDecides:
      return "kAgentDecides";
    case glic::mojom::ActuationTarget::kCurrentTab:
      return "kCurrentTab";
    case glic::mojom::ActuationTarget::kNewTab:
      return "kNewTab";
  }
  LOG(ERROR) << "Unexpected value for ActuationTarget: "
             << static_cast<int>(target);
  return "Unknown";
}

void LogGlicInvokeOptions(const GlicInvokeOptions& options,
                          bool auto_submit,
                          std::optional<bool> show_panel) {
  std::stringstream ss;

  std::visit(
      absl::Overload{
          [&ss](glic::mojom::InvocationSource source) {
            ss << "  source: " << InvocationSourceToString(source) << "\n";
          },
          [&ss](const glic::mojom::InvocationPayloadPtr& payload) {
            ss << "  payload: ";
            if (payload->is_universal_cart()) {
              ss << "UniversalCartPayload { serialized_metadata size: "
                 << payload->get_universal_cart()->serialized_metadata.size()
                 << " }";
            } else {
              ss << "Unknown Payload";
            }
            ss << "\n";
          }},
      options.source_or_payload);

  ss << "  auto_submit: " << (auto_submit ? "true" : "false") << "\n";

  if (show_panel.has_value()) {
    ss << "  show_panel: " << (show_panel.value() ? "true" : "false") << "\n";
  }

  if (!options.prompts.empty()) {
    std::vector<std::string> quoted_prompts;
    for (const auto& prompt : options.prompts) {
      quoted_prompts.push_back(base::StrCat({"\"", prompt, "\""}));
    }
    ss << "  prompts: [" << base::JoinString(quoted_prompts, ", ") << "]\n";
  }

  if (options.disable_zss) {
    ss << "  disable_zss: true\n";
  }

  if (options.feature_mode &&
      *options.feature_mode != glic::mojom::FeatureMode::kUnspecified) {
    ss << "  feature_mode: " << FeatureModeToString(*options.feature_mode)
       << "\n";
  }

  if (options.skill_id) {
    ss << "  skill_id: " << *options.skill_id << "\n";
  }

  if (options.error_message) {
    ss << "  error_message: " << *options.error_message << "\n";
  }

  if (options.timeout) {
    ss << "  timeout: " << options.timeout->InMilliseconds() << "ms\n";
  }

  if (options.wait_for_panel_open) {
    ss << "  wait_for_panel_open: true\n";
  }

  if (options.fre_override != glic::mojom::FreOverride::kUnspecified) {
    ss << "  fre_override: " << FreOverrideToString(options.fre_override)
       << "\n";
  }

  std::vector<std::string> target_pieces;

  std::visit(absl::Overload{
                 [&target_pieces](const DefaultSurface& surface) {
                   if (surface.browser != nullptr) {
                     target_pieces.push_back(base::StringPrintf(
                         "    surface: DefaultSurface { browser: %p }",
                         surface.browser.get()));
                   }
                 },
                 [&target_pieces](const NewTab& new_tab) {
                   target_pieces.push_back(base::StrCat(
                       {"    surface: NewTab { window: ",
                        base::StringPrintf("%p", new_tab.window.get()),
                        ", open_in_foreground: ",
                        new_tab.open_in_foreground ? "true" : "false", " }"}));
                 },
                 [&target_pieces](const tabs::TabHandle& tab) {
                   target_pieces.push_back("    surface: TabHandle {}");
                 },
                 [&target_pieces](const Floating& floating) {
                   target_pieces.push_back("    surface: Floating {}");
                 }},
             options.target.surface);

  std::visit(
      absl::Overload{
          [](const DefaultConversation&) {},
          [&target_pieces](const NewConversation&) {
            target_pieces.push_back("    conversation: NewConversation {}");
          },
          [&target_pieces](const ConversationId& conversation_id) {
            std::string id_str =
                base::StrCat({"    conversation: ConversationId { id: ",
                              conversation_id.conversation_id});
            if (conversation_id.turn_id) {
              base::StrAppend(&id_str, {", turn: ", *conversation_id.turn_id});
            }
            id_str += " }";
            target_pieces.push_back(id_str);
          },
          [&target_pieces](const InstanceId& instance_id) {
            target_pieces.push_back(base::StrCat(
                {"    conversation: InstanceId { id: ", instance_id.value(),
                 " }"}));
          }},
      options.target.conversation);

  if (options.target.actuation_target !=
      glic::mojom::ActuationTarget::kAgentDecides) {
    target_pieces.push_back(base::StrCat(
        {"    actuation_target: ",
         ActuationTargetToString(options.target.actuation_target)}));
  }

  if (!target_pieces.empty()) {
    ss << "  target: Target {\n"
       << base::JoinString(target_pieces, "\n") << "\n  }\n";
  }

  if (options.zss_config) {
    ss << "  zss_config: ZssConfig { ";
    if (options.zss_config->additional_content) {
      ss << "additional_content: \"" << *options.zss_config->additional_content
         << "\"";
    }
    ss << " }\n";
  }

  if (options.additional_context) {
    ss << "  additional_context: Yes (details omitted)\n";
  }

  LOG(WARNING) << "GlicInvokeOptions {\n" << ss.str() << "}";
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
      browser_context_, (glic_service && glic_service->HasActorPolicyChecker())
                            ? &glic_service->actor_policy_checker()
                            : nullptr);

  mojom::ConfigInfoPtr config = mojom::ConfigInfo::New();
  config->guest_url = GetGuestURL();
  config->fre_guest_url = GURL();

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

  mojom::InternalsDebugInfoPtr debug_info = mojom::InternalsDebugInfo::New();

  // Feature flags
  debug_info->glic_feature_enabled =
      base::FeatureList::IsEnabled(features::kGlic);
  debug_info->glic_actor_feature_enabled =
      base::FeatureList::IsEnabled(features::kGlicActor);
  debug_info->glic_rollout_feature_enabled =
      base::FeatureList::IsEnabled(features::kGlicRollout);

  // Platform and form factor
  debug_info->platform = GetGlicPlatform();
  debug_info->form_factor = GetGlicFormFactor(ui::GetDeviceFormFactor());

  // WebClientInitialState fields (browser and user settings)
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  PrefService* pref_service = profile->GetPrefs();

  base::flat_map<std::string, bool> boolean_settings;

  boolean_settings["Microphone Permission Enabled"] =
      pref_service->GetBoolean(prefs::kGlicMicrophoneEnabled);
  boolean_settings["Location Permission Enabled"] =
      pref_service->GetBoolean(prefs::kGlicGeolocationEnabled);
  boolean_settings["Tab Context Permission Enabled"] =
      pref_service->GetBoolean(prefs::kGlicTabContextEnabled);
  boolean_settings["OS Location Permission Enabled"] =
      system_permission_settings::IsAllowed(ContentSettingsType::GEOLOCATION);

  boolean_settings["Zero State Suggestions Enabled"] =
      IsZeroStateSuggestionsEnabled();
  boolean_settings["Cached Get User Profile Info Enabled"] =
      base::FeatureList::IsEnabled(
          features::kGlicEnableCachedGetUserProfileInfo);
  boolean_settings["Scroll To Enabled"] =
      base::FeatureList::IsEnabled(features::kGlicScrollTo);
  boolean_settings["Default Tab Context Setting Feature Enabled"] =
      base::FeatureList::IsEnabled(features::kGlicDefaultTabContextSetting);
  boolean_settings["Default Tab Context Setting Enabled"] =
      pref_service->GetBoolean(prefs::kGlicDefaultTabContextEnabled);
  boolean_settings["Closed Captioning Setting Enabled"] =
      pref_service->GetBoolean(prefs::kGlicClosedCaptioningEnabled);
  boolean_settings["Maybe Refresh User Status Enabled"] =
      base::FeatureList::IsEnabled(features::kGlicUserStatusCheck) &&
      features::kGlicUserStatusRefreshApi.Get();
  boolean_settings["Get Context Actor Enabled"] =
      base::FeatureList::IsEnabled(glic::mojom::features::kGlicActorTabContext);
  boolean_settings["Web Actuation Setting Feature Enabled"] =
      base::FeatureList::IsEnabled(features::kGlicWebActuationSetting);

  boolean_settings["Get Page Metadata Enabled"] =
      base::FeatureList::IsEnabled(blink::features::kFrameMetadataObserver);
  boolean_settings["API Activation Gating Enabled"] =
      base::FeatureList::IsEnabled(features::kGlicApiActivationGating);
  boolean_settings["Capture Region Enabled"] =
      base::FeatureList::IsEnabled(features::kGlicCaptureRegion);
  boolean_settings["Can Act on Web"] =
      glic_service ? glic_service->actor_policy_checker().CanActOnWeb() : false;

  boolean_settings["Activate Tab Enabled"] =
      base::FeatureList::IsEnabled(glic::mojom::features::kGlicActivateTabApi);
  boolean_settings["Get Tab by ID Enabled"] =
      base::FeatureList::IsEnabled(features::kGlicGetTabByIdApi);
  boolean_settings["Open Password Manager Settings Page Enabled"] =
      base::FeatureList::IsEnabled(
          features::kGlicOpenPasswordManagerSettingsPageApi);
  boolean_settings["Skills Feature Enabled"] =
      base::FeatureList::IsEnabled(features::kSkillsEnabled);
  boolean_settings["Get Tab Favicon by ID Enabled"] =
      base::FeatureList::IsEnabled(features::kGlicGetTabFaviconById);
  boolean_settings["Process Counter Abuse Verdict Enabled"] =
      base::FeatureList::IsEnabled(features::kGlicProcessCounterAbuseVerdict);

  debug_info->boolean_settings = std::move(boolean_settings);

#if !BUILDFLAG(IS_ANDROID)
  debug_info->hotkey = GetHotkeyString();
#else
  debug_info->hotkey = "";
#endif

  payload->debug_info = std::move(debug_info);

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

  GlicInvokeOptions options =
      mojo_options->payload
          ? GlicInvokeOptions(std::move(mojo_options->payload))
          : GlicInvokeOptions(mojo_options->invocation_source);
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
        // LINT.IfChange(GlicInvokeError)
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
          case GlicInvokeError::kInvalidConfiguration:
            error_msg = "Invalid Configuration";
            break;
          case GlicInvokeError::kAdditionalContextSawNavigation:
            error_msg = "Navigation during context gathering";
            break;
          case GlicInvokeError::kAdditionalContextFailedCopyPolicy:
            error_msg = "Copy policy check failed";
            break;
          case GlicInvokeError::kAdditionalContextFailedPastePolicy:
            error_msg = "Paste policy check failed";
            break;
          case GlicInvokeError::kAdditionalContextNoSourceFrame:
            error_msg = "No source frame for context";
            break;
          case GlicInvokeError::kAdditionalContextNoClientFrame:
            error_msg = "No client frame for context";
            break;
          case GlicInvokeError::kAdditionalContextNoClipboardMetadata:
            error_msg = "No clipboard metadata for context";
            break;
          case GlicInvokeError::kUnknown:
            error_msg = "Unknown Error";
            break;
          default:
            error_msg = "Unknown Error";
            break;
        }
        // LINT.ThenChange(//chrome/browser/glic/public/glic_invoke_options.h:GlicInvokeError)
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

  LogGlicInvokeOptions(options, mojo_options->auto_submit,
                       mojo_options->show_panel);

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

  service->opt_in_controller().ShowDialog(webui_contents_, base::DoNothing());
#endif
}

}  // namespace glic

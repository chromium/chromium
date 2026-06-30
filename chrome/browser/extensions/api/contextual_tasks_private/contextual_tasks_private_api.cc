// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/contextual_tasks_private/contextual_tasks_private_api.h"

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/extensions/api/contextual_tasks_private.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace extensions {

namespace {

constexpr char kInvalidFrameError[] = "Invalid frame.";
constexpr char kNotTopLevelFrameError[] = "Not top-level frame";

content::RenderFrameHost* GetRfhForDocumentId(
    const std::string& document_id_str) {
  ExtensionApiFrameIdMap::DocumentId document_id =
      ExtensionApiFrameIdMap::DocumentIdFromString(document_id_str);
  return ExtensionApiFrameIdMap::Get()->GetRenderFrameHostByDocumentId(
      document_id);
}

bool IsContextualTasksEnabledForProfile(Profile* profile) {
  contextual_tasks::ContextualTasksUiService* ui_service =
      contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
          profile);
  return ui_service && ui_service->GetEligibilityManager() &&
         ui_service->GetEligibilityManager()->IsEligible();
}

GURL AppendAimUrlParams(
    const GURL& base_url,
    const api::contextual_tasks_private::AimParams& aim_params) {
  GURL url = base_url;
  const struct {
    const char* name;
    std::optional<std::string> value;
  } kParams[] = {
      {"ntc", aim_params.ntc},     {"mstk", aim_params.mstk},
      {"aioh", aim_params.aioh},   {"csuir", aim_params.csuir},
      {"ved", aim_params.ved},     {"cs", aim_params.cs},
      {"sxsrf", aim_params.sxsrf}, {"ei", aim_params.ei},
  };
  for (const auto& param : kParams) {
    if (param.value && !param.value->empty()) {
      url = net::AppendQueryParameter(url, param.name, *param.value);
    }
  }
  return url;
}

}  // namespace

ContextualTasksPrivateGetStateFunction::
    ContextualTasksPrivateGetStateFunction() = default;

ContextualTasksPrivateGetStateFunction::
    ~ContextualTasksPrivateGetStateFunction() = default;

ExtensionFunction::ResponseAction
ContextualTasksPrivateGetStateFunction::Run() {
  std::optional<api::contextual_tasks_private::GetState::Params> params =
      api::contextual_tasks_private::GetState::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  content::RenderFrameHost* rfh = GetRfhForDocumentId(params->document_id);
  if (!rfh) {
    return RespondNow(Error(kInvalidFrameError));
  }

  if (!rfh->IsInPrimaryMainFrame()) {
    return RespondNow(Error(kNotTopLevelFrameError));
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());

  api::contextual_tasks_private::ProfileState state;
  state.is_eligible = IsContextualTasksEnabledForProfile(profile);

  return RespondNow(ArgumentList(
      api::contextual_tasks_private::GetState::Results::Create(state)));
}

ContextualTasksPrivateLaunchPanelInNewTabFunction::
    ContextualTasksPrivateLaunchPanelInNewTabFunction() = default;

ContextualTasksPrivateLaunchPanelInNewTabFunction::
    ~ContextualTasksPrivateLaunchPanelInNewTabFunction() = default;

ExtensionFunction::ResponseAction
ContextualTasksPrivateLaunchPanelInNewTabFunction::Run() {
  std::optional<api::contextual_tasks_private::LaunchPanelInNewTab::Params>
      params =
          api::contextual_tasks_private::LaunchPanelInNewTab::Params::Create(
              args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());

  if (!IsContextualTasksEnabledForProfile(profile)) {
    return RespondNow(
        Error("ContextualTasks Private API is not eligible for this profile"));
  }

  GURL target_url(params->details.target_url);

  if (!target_url.is_valid() || !target_url.SchemeIs(url::kHttpsScheme)) {
    return RespondNow(Error("URLs must be valid and use HTTPS"));
  }

  contextual_tasks::ContextualTasksUiService* ui_service =
      contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
          profile);

  content::RenderFrameHost* rfh =
      GetRfhForDocumentId(params->details.document_id);
  if (!rfh) {
    return RespondNow(Error(kInvalidFrameError));
  }

  if (!rfh->IsInPrimaryMainFrame()) {
    return RespondNow(Error(kNotTopLevelFrameError));
  }

  if (!ui_service->IsSearchResultsUrl(rfh->GetLastCommittedURL())) {
    return RespondNow(
        Error("Contextual Tasks are only supported on Search Results pages."));
  }

  // Determine the target host: use the forced embedded page host if set;
  // otherwise, default to the host/origin of the one the request came from.
  std::string host = contextual_tasks::GetForcedEmbeddedPageHost();
  if (host.empty()) {
    host = rfh->GetLastCommittedURL().host();
  }

  GURL default_ai_url = ui_service->GetDefaultAiPageUrl();
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpsScheme);
  replacements.SetHostStr(host);
  GURL base_aim_url = default_ai_url.ReplaceComponents(replacements);

  GURL aim_url = AppendAimUrlParams(base_aim_url, params->details.aim_params);

  if (!aim_url.is_valid() || !aim_url.SchemeIs(url::kHttpsScheme)) {
    return RespondNow(Error("Generated AI URL is invalid or not HTTPS"));
  }

  content::WebContents* initiator_web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!initiator_web_contents) {
    return RespondNow(
        Error("Unable to find WebContents for the provided document ID"));
  }

  if (initiator_web_contents->GetBrowserContext() != profile) {
    return RespondNow(
        Error("Document ID does not belong to the active profile."));
  }

  tabs::TabInterface* initiator_tab =
      tabs::TabInterface::MaybeGetFromContents(initiator_web_contents);
  BrowserWindowInterface* browser =
      initiator_tab ? initiator_tab->GetBrowserWindowInterface() : nullptr;
  if (!browser) {
    return RespondNow(Error("Unable to identify active browser window"));
  }

  if (browser->GetType() != BrowserWindowInterface::Type::TYPE_NORMAL) {
    return RespondNow(Error(
        "Contextual Tasks are only supported in normal browser windows."));
  }

  TabListInterface* tab_list = TabListInterface::From(browser);
  if (!tab_list) {
    return RespondNow(Error("Tab list unavailable"));
  }

  std::unique_ptr<content::WebContents> new_contents =
      content::WebContents::Create(content::WebContents::CreateParams(profile));
  int active_index = tab_list->GetActiveIndex();
  tabs::TabInterface* target_tab =
      tab_list->InsertWebContentsAt(active_index + 1, std::move(new_contents),
                                    /*should_pin=*/false, std::nullopt);
  if (!target_tab) {
    return RespondNow(Error("Failed to insert new tab"));
  }
  tab_list->SetOpenerForTab(target_tab->GetHandle(),
                            initiator_tab->GetHandle());

  tab_list->ActivateTab(target_tab->GetHandle());

  // Mimic a renderer-initiated navigation (like window.open) from the initiator
  // frame. This ensures that Site Isolation correctly routes the new tab's
  // navigation based on the initiator's site context, and that security checks
  // (like same-origin or CORS policies) correctly identify the request source.
  content::NavigationController::LoadURLParams load_params(target_url);
  load_params.initiator_origin = rfh->GetLastCommittedOrigin();
  load_params.initiator_frame_token = rfh->GetFrameToken();
  load_params.initiator_process_id =
      rfh->GetProcess()->GetID().GetUnsafeValue();
  load_params.source_site_instance = rfh->GetSiteInstance();
  load_params.is_renderer_initiated = true;
  load_params.has_user_gesture = user_gesture();
  target_tab->GetContents()->GetController().LoadURLWithParams(load_params);

  ui_service->StartTaskUiInSidePanel(browser, target_tab, aim_url,
                                     /*session_handle=*/nullptr,
                                     /*associate_web_contents=*/false);

  return RespondNow(NoArguments());
}

}  // namespace extensions

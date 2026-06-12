// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_navigation_throttle.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_request_headers.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "url/origin.h"

using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

namespace contextual_tasks {

ContextualTasksNavigationThrottle::ContextualTasksNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

ContextualTasksNavigationThrottle::~ContextualTasksNavigationThrottle() =
    default;

const char* ContextualTasksNavigationThrottle::GetNameForLogging() {
  return "ContextualTasksNavigationThrottle";
}

ThrottleCheckResult ContextualTasksNavigationThrottle::WillStartRequest() {
  return ProcessNavigation();
}

ThrottleCheckResult ContextualTasksNavigationThrottle::WillRedirectRequest() {
  return ProcessNavigation();
}

ThrottleCheckResult ContextualTasksNavigationThrottle::ProcessNavigation() {
  // Do not intercept about:blank or data: URLs.
  if (navigation_handle()->GetURL().IsAboutBlank() ||
      navigation_handle()->GetURL().SchemeIs(url::kDataScheme)) {
    return PROCEED;
  }

  auto* web_contents = navigation_handle()->GetWebContents();
  content::OpenURLParams url_params =
      content::OpenURLParams::FromNavigationHandle(navigation_handle());

  ContextualTasksUiService* const ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());

  // TODO(b/485973605): Find a way to consolidate this logic into
  // ContextualTasksUiService if it gets more complicated or needs to handle in
  // other places.
  if (base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksUrlRedirectToAimUrl)) {
    bool is_cobrowse_eligible =
        ui_service && ui_service->GetEligibilityManager() &&
        ui_service->GetEligibilityManager()->IsEligibleWithoutIdentity();
    if ((!base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks) ||
         !is_cobrowse_eligible) &&
        ContextualTasksUiService::IsContextualTasksUrl(url_params.url)) {
      // Redirect contextual tasks URL to aim page URL.
      GURL url = ContextualTasksUiService::CopyParamsFromWebUIUrl(
          GURL(GetContextualTasksAiPageUrl()), url_params.url);
      if (url.is_empty()) {
        url = GURL(GetContextualTasksAiPageUrl());
      }
      // Post a task to open the URL.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](base::WeakPtr<content::WebContents> web_contents,
                 const GURL& url) {
                if (web_contents) {
                  web_contents->GetController().LoadURLWithParams(
                      content::NavigationController::LoadURLParams(url));
                }
              },
              web_contents->GetWeakPtr(), url));
      return CANCEL;
    }
  }

  if (!base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks)) {
    return PROCEED;
  }

  content::SiteInstance* site = navigation_handle()->GetSourceSiteInstance();
  bool is_same_site_or_from_ui =
      site && site->IsSameSiteWithURL(navigation_handle()->GetURL());

  const net::HttpRequestHeaders& headers =
      navigation_handle()->GetRequestHeaders();
  std::optional<std::string> sec_ch_ua_mobile =
      headers.GetHeader("sec-ch-ua-mobile");
  bool is_mobile_ua = sec_ch_ua_mobile.has_value() && *sec_ch_ua_mobile == "?1";

  std::optional<url::Origin> initiator_origin =
      navigation_handle()->GetInitiatorOrigin();

  std::optional<content::GlobalRenderFrameHostToken> initiator_frame_token;
  if (navigation_handle()->GetInitiatorFrameToken().has_value()) {
    initiator_frame_token = content::GlobalRenderFrameHostToken(
        navigation_handle()->GetInitiatorProcessId(),
        navigation_handle()->GetInitiatorFrameToken().value());
  }

  if (ui_service &&
      ui_service->HandleNavigation(
          std::move(url_params), web_contents->GetResponsibleWebContents(),
          /*is_from_embedded_page=*/web_contents !=
                  web_contents->GetResponsibleWebContents() ||
              navigation_handle()->IsGuestViewMainFrame(),
          /*from_can_create_window=*/false, is_same_site_or_from_ui,
          is_mobile_ua, initiator_origin, initiator_frame_token,
          blink::mojom::WindowFeatures())) {
    return CANCEL;
  }
  return PROCEED;
}

// static
void ContextualTasksNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  // Ignore navigations that aren't in the outermost main frame and not in a
  // prerender frame.
  content::NavigationHandle& nav_handle = registry.GetNavigationHandle();
  if (!nav_handle.IsInOutermostMainFrame() ||
      nav_handle.IsInPrerenderedMainFrame()) {
    return;
  }

  registry.AddThrottle(
      std::make_unique<ContextualTasksNavigationThrottle>(registry));
}

}  // namespace contextual_tasks

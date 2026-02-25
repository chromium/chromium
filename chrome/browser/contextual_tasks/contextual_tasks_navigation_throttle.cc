// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_navigation_throttle.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

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

  // TODO(b/485973605): Find a way to consolidate this logic into
  // ContextualTasksUiService if it gets more complicated or needs to handle in
  // other places.
  if (base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksUrlRedirectToAimUrl)) {
    AimEligibilityService* aim_service =
        AimEligibilityServiceFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents->GetBrowserContext()));
    bool is_cobrowse_eligible =
        aim_service && aim_service->IsCobrowseEligible();

    if ((!base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks) ||
         !is_cobrowse_eligible) &&
        ContextualTasksUiService::IsContextualTasksUrl(url_params.url)) {
      // Redirect contextual tasks URL to aim page URL.
      GURL url = ContextualTasksUiService::GetAimUrlFromContextualTasksUrl(
          url_params.url);
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

  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());

  if (ui_service &&
      ui_service->HandleNavigation(
          std::move(url_params), web_contents->GetResponsibleWebContents(),
          /*is_from_embedded_page=*/web_contents !=
              web_contents->GetResponsibleWebContents(),
          /*is_to_new_tab=*/false)) {
    return CANCEL;
  }
  return PROCEED;
}

// static
void ContextualTasksNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  // Ignore navigations that aren't in the main frame.
  if (!registry.GetNavigationHandle().IsInPrimaryMainFrame()) {
    return;
  }

  registry.AddThrottle(
      std::make_unique<ContextualTasksNavigationThrottle>(registry));
}

}  // namespace contextual_tasks

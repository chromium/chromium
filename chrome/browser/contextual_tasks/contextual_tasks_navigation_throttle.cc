// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_navigation_throttle.h"

#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
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
  auto* web_contents = navigation_handle()->GetWebContents();
  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());

  content::OpenURLParams url_params =
      content::OpenURLParams::FromNavigationHandle(navigation_handle());

  if (ui_service &&
      ui_service->HandleNavigation(std::move(url_params),
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

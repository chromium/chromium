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
  // If this throttle was created, we always want to block the navigation.
  return CANCEL;
}

// static
void ContextualTasksNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  // Ignore navigations that aren't in the main frame.
  if (!registry.GetNavigationHandle().IsInPrimaryMainFrame()) {
    return;
  }

  auto* web_contents = registry.GetNavigationHandle().GetWebContents();
  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (ui_service->HandleNavigation(
          registry.GetNavigationHandle().GetURL(),
          web_contents->GetResponsibleWebContents()->GetLastCommittedURL(),
          web_contents, /*is_to_new_tab=*/false)) {
    registry.AddThrottle(
        std::make_unique<ContextualTasksNavigationThrottle>(registry));
  }
}

}  // namespace contextual_tasks

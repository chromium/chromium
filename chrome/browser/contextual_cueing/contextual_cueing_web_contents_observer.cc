// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_web_contents_observer.h"

#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace contextual_cueing {

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualCueingWebContentsObserver);

ContextualCueingWebContentsObserver::ContextualCueingWebContentsObserver(
    content::WebContents* web_contents,
    ContextualCueingService* service)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ContextualCueingWebContentsObserver>(
          *web_contents),
      service_(service) {}

ContextualCueingWebContentsObserver::~ContextualCueingWebContentsObserver() =
    default;

void ContextualCueingWebContentsObserver::PrimaryPageChanged(
    content::Page& page) {
  service_->ReportPageLoad();
}

void ContextualCueingWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Ignore sub-frame and uncommitted navigations.
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  if (!navigation_handle->HasCommitted()) {
    return;
  }

  // Ignore reloads.
  if (PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                               ui::PAGE_TRANSITION_RELOAD)) {
    return;
  }
  if (navigation_handle->GetPreviousPrimaryMainFrameURL() ==
      navigation_handle->GetURL()) {
    return;
  }

  // Ignore fragment changes for cueing only.
  if (navigation_handle->GetPreviousPrimaryMainFrameURL().GetWithoutRef() ==
      navigation_handle->GetURL().GetWithoutRef()) {
    return;
  }

  if (auto* controller =
          ContextualCueingController::GetForWebContents(GetWebContents())) {
    controller->HideCue();
    if (auto* tab = tabs::TabInterface::MaybeGetFromContents(&GetWebContents());
        tab->IsActivated()) {
      controller->ActiveTabUrlChanged(navigation_handle->GetURL());
    }
  }
}

}  // namespace contextual_cueing

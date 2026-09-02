// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_web_contents_observer.h"

#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

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
  should_evaluate_cues_on_load_ = false;
  if (!navigation_handle->HasCommitted()) {
    return;
  }

  const bool is_back_forward = (navigation_handle->GetPageTransition() &
                                ui::PAGE_TRANSITION_FORWARD_BACK) != 0;

  if (!is_back_forward) {
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
  }

  auto* tab = tabs::TabInterface::MaybeGetFromContents(&GetWebContents());
  if (!tab) {
    return;
  }

  if (auto* controller =
          ContextualCueingController::GetForWebContents(GetWebContents())) {
    controller->HideCue();
    if (tab->IsActivated()) {
      controller->OnUrlChanged(navigation_handle->GetURL());
    }
    if (navigation_handle->IsServedFromBackForwardCache()) {
      controller->EvaluateCues();
    } else if (!navigation_handle->IsSameDocument()) {
      should_evaluate_cues_on_load_ = true;
    }
  }

  if (auto* window = tab->GetBrowserWindowInterface()) {
    if (auto* tab_list = TabListInterface::From(window)) {
      for (int i = 0; i < tab_list->GetTabCount(); ++i) {
        tabs::TabInterface* other_tab = tab_list->GetTab(i);
        if (other_tab == tab) {
          continue;
        }
        if (auto* other_controller =
                other_tab->GetTabFeatures()->contextual_cueing_controller()) {
          other_controller->OnTabNavigated(tab);
        }
      }
    }
  }
}

void ContextualCueingWebContentsObserver::
    DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (!should_evaluate_cues_on_load_) {
    return;
  }
  should_evaluate_cues_on_load_ = false;

  auto* tab = tabs::TabInterface::MaybeGetFromContents(&GetWebContents());
  if (!tab) {
    return;
  }

  if (auto* controller =
          ContextualCueingController::GetForWebContents(GetWebContents())) {
    controller->EvaluateCues();
  }
}

}  // namespace contextual_cueing

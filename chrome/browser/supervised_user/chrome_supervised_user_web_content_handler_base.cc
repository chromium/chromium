// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chrome_supervised_user_web_content_handler_base.h"

#include "chrome/browser/supervised_user/supervised_user_interstitial_tab_closer.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

ChromeSupervisedUserWebContentHandlerBase::
    ChromeSupervisedUserWebContentHandlerBase(
        content::WebContents* web_contents,
        content::FrameTreeNodeId frame_id,
        int64_t interstitial_navigation_id)
    : web_contents_(web_contents),
      frame_id_(frame_id),
      interstitial_navigation_id_(interstitial_navigation_id) {
  CHECK(web_contents_);
}

ChromeSupervisedUserWebContentHandlerBase::
    ~ChromeSupervisedUserWebContentHandlerBase() = default;

bool ChromeSupervisedUserWebContentHandlerBase::IsMainFrame() const {
  return web_contents_->GetPrimaryMainFrame()->GetFrameTreeNodeId() ==
         frame_id_;
}

void ChromeSupervisedUserWebContentHandlerBase::CleanUpInfoBarOnMainFrame() {
  if (!IsMainFrame()) {
    return;
  }
  infobars::ContentInfoBarManager* manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents_);
  if (!manager) {
    return;
  }
  content::LoadCommittedDetails details;
  // |details.is_same_document| is default false, and |details.is_main_frame|
  // is default true. This results in is_navigation_to_different_page()
  // returning true.
  CHECK(details.is_navigation_to_different_page());
  content::NavigationController& controller = web_contents_->GetController();
  details.entry = controller.GetVisibleEntry();
  if (controller.GetLastCommittedEntry()) {
    details.previous_entry_index = controller.GetLastCommittedEntryIndex();
    details.previous_main_frame_url =
        controller.GetLastCommittedEntry()->GetURL();
  }
  // Copy the infobars to allow safe iteration while removing elements.
  const auto infobars = manager->infobars();
  for (infobars::InfoBar* infobar : infobars) {
    if (infobar->delegate()->ShouldExpire(
            infobars::ContentInfoBarManager::
                NavigationDetailsFromLoadCommittedDetails(details))) {
      manager->RemoveInfoBar(infobar);
    }
  }
}

int64_t ChromeSupervisedUserWebContentHandlerBase::GetInterstitialNavigationId()
    const {
  return interstitial_navigation_id_;
}

void ChromeSupervisedUserWebContentHandlerBase::GoBack() {
  // GoBack only for main frame.
  CHECK(IsMainFrame());
  if (!AttemptMoveAwayFromCurrentFrameURL()) {
    TabCloser::CheckIfInBrowserThenCloseTab(web_contents_);
  }
  OnInterstitialDone();
}

bool ChromeSupervisedUserWebContentHandlerBase::
    AttemptMoveAwayFromCurrentFrameURL() {
  // No need to do anything if the WebContents is in the process of being
  // destroyed anyway.
  if (web_contents_->IsBeingDestroyed()) {
    return true;
  }
  // If the interstitial was shown over an existing page, navigate back from
  // that page. If that is not possible, attempt to close the entire tab.
  if (web_contents_->GetController().CanGoBack()) {
    web_contents_->GetController().GoBack();
    return true;
  }
  return false;
}

void ChromeSupervisedUserWebContentHandlerBase::OnInterstitialDone() {
  auto* navigation_observer =
      SupervisedUserNavigationObserver::FromWebContents(web_contents_);
  // After this, the WebContents may be destroyed. Make sure we don't try to use
  // it again. `OnInterstitialDone` will destruct the web content handler,
  // and consequently the web_contents_ pointer.
  web_contents_ = nullptr;
  navigation_observer->OnInterstitialDone(frame_id_);
}

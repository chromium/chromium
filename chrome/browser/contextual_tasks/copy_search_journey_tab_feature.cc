// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/copy_search_journey_tab_feature.h"

#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/contextual_tasks/copy_search_journey_tracker.h"
#include "chrome/browser/contextual_tasks/copy_search_journey_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/google/core/common/google_util.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace contextual_tasks {

DEFINE_USER_DATA(CopySearchJourneyTabFeature);

CopySearchJourneyTabFeature::CopySearchJourneyTabFeature(
    tabs::TabInterface& tab)
    : content::WebContentsObserver(tab.GetContents()),
      will_discard_contents_subscription_(
          tab.RegisterWillDiscardContents(base::BindRepeating(
              &CopySearchJourneyTabFeature::OnWillDiscardContents,
              base::Unretained(this)))),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  if (content::WebContents* contents = tab.GetContents()) {
    Profile* profile =
        Profile::FromBrowserContext(contents->GetBrowserContext());
    tracker_ = CopySearchJourneyTrackerFactory::GetForProfile(profile);
    template_url_service_ = TemplateURLServiceFactory::GetForProfile(profile);
  }
}

CopySearchJourneyTabFeature::~CopySearchJourneyTabFeature() {
  NotifyTabDestroyed(web_contents());
}

// static
CopySearchJourneyTabFeature* CopySearchJourneyTabFeature::From(
    tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

void CopySearchJourneyTabFeature::OnTextCopiedToClipboard(
    content::RenderFrameHost* render_frame_host,
    const std::u16string& copied_text) {
  if (!tracker_ || !web_contents()) {
    return;
  }
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry) {
    return;
  }
  tracker_->OnCopyRecorded(sessions::SessionTabHelper::IdForTab(web_contents()),
                           entry->GetUniqueID(), copied_text);
}

void CopySearchJourneyTabFeature::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!tracker_ || !template_url_service_ ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsErrorPage() ||
      !google_util::IsGoogleSearchUrl(navigation_handle->GetURL())) {
    return;
  }

  const TemplateURL* default_provider =
      template_url_service_->GetDefaultSearchProvider();
  std::u16string search_terms;
  if (!default_provider ||
      !default_provider->ExtractSearchTermsFromURL(
          navigation_handle->GetURL(),
          template_url_service_->search_terms_data(), &search_terms) ||
      search_terms.empty()) {
    return;
  }

  tracker_->OnSearchNavigationCommitted(
      sessions::SessionTabHelper::IdForTab(web_contents()), search_terms);
}

void CopySearchJourneyTabFeature::OnWillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  NotifyTabDestroyed(old_contents);
  Observe(new_contents);
}

void CopySearchJourneyTabFeature::NotifyTabDestroyed(
    content::WebContents* contents) {
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(contents);
  if (tracker_ && tab_id.is_valid()) {
    tracker_->OnTabDestroyed(tab_id);
  }
}

}  // namespace contextual_tasks

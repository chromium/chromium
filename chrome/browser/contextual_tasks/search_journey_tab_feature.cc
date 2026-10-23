// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/search_journey_tab_feature.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/search_journey_tracker.h"
#include "chrome/browser/contextual_tasks/search_journey_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/utils.h"
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
#include "net/base/url_util.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace contextual_tasks {

namespace {

int GetLastCommittedEntryId(content::WebContents* contents) {
  content::NavigationEntry* entry =
      contents->GetController().GetLastCommittedEntry();
  return entry ? entry->GetUniqueID() : 0;
}

// Returns the AI Mode URL that restores `journey`'s AI Overview as the first
// turn of a thread: `ai_page_url` over HTTPS, with the journey's search terms
// as `q`, its non-empty restore parameters and the ABA entry point. `journey`
// must have restore parameters.
GURL BuildRestoreUrl(const GURL& ai_page_url, const JourneyState& journey) {
  CHECK(journey.promo_params.has_value());
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpsScheme);
  GURL url = ai_page_url.ReplaceComponents(replacements);
  url = net::AppendOrReplaceQueryParameter(
      url, "q", base::UTF16ToUTF8(journey.search_terms));

  const AimPromoParams& params = *journey.promo_params;
  const std::pair<std::string_view, const std::string&> restore_params[] = {
      {"mstk", params.mstk},
      {"aioh", params.aioh},
      {"csuir", params.csuir},
      {"sxsrf", params.sxsrf},
  };
  for (const auto& [name, value] : restore_params) {
    if (!value.empty()) {
      url = net::AppendOrReplaceQueryParameter(url, name, value);
    }
  }
  return AppendAimEntryPointParams(
      url, omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_ABA_ONRAMP_IPH);
}

}  // namespace

DEFINE_USER_DATA(SearchJourneyTabFeature);

SearchJourneyTabFeature::SearchJourneyTabFeature(tabs::TabInterface& tab)
    : content::WebContentsObserver(tab.GetContents()),
      tab_(tab),
      will_discard_contents_subscription_(tab.RegisterWillDiscardContents(
          base::BindRepeating(&SearchJourneyTabFeature::OnWillDiscardContents,
                              base::Unretained(this)))),
      did_activate_subscription_(tab.RegisterDidActivate(
          base::BindRepeating(&SearchJourneyTabFeature::OnTabDidActivate,
                              base::Unretained(this)))),
      will_deactivate_subscription_(tab.RegisterWillDeactivate(
          base::BindRepeating(&SearchJourneyTabFeature::OnTabWillDeactivate,
                              base::Unretained(this)))),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  if (content::WebContents* contents = tab.GetContents()) {
    Profile* profile =
        Profile::FromBrowserContext(contents->GetBrowserContext());
    tracker_ = SearchJourneyTrackerFactory::GetForProfile(profile);
    template_url_service_ = TemplateURLServiceFactory::GetForProfile(profile);
  }
}

SearchJourneyTabFeature::~SearchJourneyTabFeature() {
  NotifyTabDestroyed(web_contents());
}

// static
SearchJourneyTabFeature* SearchJourneyTabFeature::From(
    tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

bool SearchJourneyTabFeature::CanRestoreJourneyInSidePanel() const {
  return GetRestoreUrl().is_valid();
}

bool SearchJourneyTabFeature::RestoreJourneyInSidePanel() {
  const GURL url = GetRestoreUrl();
  if (!url.is_valid()) {
    return false;
  }

  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());
  // Tab A isn't added as context. It shows as the suggested tab, and is only
  // sent if the user attaches it to a follow-up.
  ui_service->StartTaskUiInSidePanel(
      tab_->GetBrowserWindowInterface(), &tab_.get(), url,
      /*session_handle=*/nullptr,
      StartTaskUiOptions{
          .associate_web_contents = false,
          .entry_point =
              omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_ABA_ONRAMP_IPH,
          .use_mstk_for_task_association = true,
          .open_time_ticks = base::TimeTicks::Now(),
      });
  tracker_->OnJourneyRestored(
      sessions::SessionTabHelper::IdForTab(web_contents()));
  return true;
}

void SearchJourneyTabFeature::OnTextCopiedToClipboard(
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

void SearchJourneyTabFeature::DidFinishNavigation(
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

void SearchJourneyTabFeature::OnTabDidActivate(tabs::TabInterface* tab) {
  if (!tracker_ || !web_contents()) {
    return;
  }
  tracker_->OnTabActivated(sessions::SessionTabHelper::IdForTab(web_contents()),
                           GetLastCommittedEntryId(web_contents()));
}

void SearchJourneyTabFeature::OnTabWillDeactivate(tabs::TabInterface* tab) {
  if (!tracker_ || !web_contents()) {
    return;
  }
  SessionID opener_tab_id = SessionID::InvalidValue();
  if (BrowserWindowInterface* window = tab->GetBrowserWindowInterface()) {
    if (TabListInterface* tab_list = TabListInterface::From(window)) {
      if (tabs::TabInterface* opener =
              tab_list->GetOpenerForTab(tab->GetHandle())) {
        opener_tab_id =
            sessions::SessionTabHelper::IdForTab(opener->GetContents());
      }
    }
  }
  tracker_->OnTabDeactivated(
      sessions::SessionTabHelper::IdForTab(web_contents()),
      GetLastCommittedEntryId(web_contents()),
      web_contents()->GetLastCommittedURL(), opener_tab_id);
}

void SearchJourneyTabFeature::OnWillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  NotifyTabDestroyed(old_contents);
  Observe(new_contents);
}

void SearchJourneyTabFeature::NotifyTabDestroyed(
    content::WebContents* contents) {
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(contents);
  if (tracker_ && tab_id.is_valid()) {
    tracker_->OnTabDestroyed(tab_id);
  }
}

GURL SearchJourneyTabFeature::GetRestoreUrl() const {
  if (!tracker_ || !web_contents()) {
    return GURL();
  }
  const JourneyState* journey = tracker_->GetRestorableJourney(
      sessions::SessionTabHelper::IdForTab(web_contents()),
      GetLastCommittedEntryId(web_contents()));
  if (!journey) {
    return GURL();
  }

  // The side panel only opens next to the active tab.
  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());
  if (!tab_->GetBrowserWindowInterface() || !tab_->IsActivated() ||
      !ui_service ||
      !EntryPointEligibilityManager::IsEligible(tab_->GetProfile())) {
    return GURL();
  }

  const GURL url = BuildRestoreUrl(ui_service->GetDefaultAiPageUrl(), *journey);
  return url.is_valid() && url.SchemeIs(url::kHttpsScheme) ? url : GURL();
}

}  // namespace contextual_tasks

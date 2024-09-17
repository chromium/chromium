// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_tab_helper.h"

#include <optional>
#include <string>

#include "build/build_config.h"
#include "chrome/browser/complex_tasks/task_tab_helper.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"
#include "chrome/browser/history_embeddings/history_embeddings_tab_helper.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/history/content/browser/history_context_helper.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/sessions/content/navigation_task_id.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "ui/base/page_transition_types.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_string.h"
#include "chrome/browser/android/background_tab_manager.h"
#include "chrome/browser/feed/feed_service_factory.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#include "chrome/browser/history/jni_headers/HistoryTabHelper_jni.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/feed/core/v2/public/feed_api.h"      // nogncheck
#include "components/feed/core/v2/public/feed_service.h"  // nogncheck
#include "content/public/browser/web_contents.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#endif


namespace {

using content::NavigationEntry;
using content::WebContents;
#if BUILDFLAG(IS_ANDROID)
using chrome::android::BackgroundTabManager;
#endif

#if BUILDFLAG(IS_ANDROID)
bool IsNavigationFromFeed(content::WebContents& web_contents, const GURL& url) {
  feed::FeedService* feed_service =
      feed::FeedServiceFactory::GetForBrowserContext(
          web_contents.GetBrowserContext());
  if (!feed_service)
    return false;

  return feed_service->GetStream()->WasUrlRecentlyNavigatedFromFeed(url);
}
#endif  // BUILDFLAG(IS_ANDROID)

bool ShouldConsiderForNtpMostVisited(
    content::WebContents& web_contents,
    content::NavigationHandle* navigation_handle) {
#if BUILDFLAG(IS_ANDROID)
  // Clicks on content suggestions on the NTP should not contribute to the
  // Most Visited tiles in the NTP.
  DCHECK(!navigation_handle->GetRedirectChain().empty());
  if (ui::PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                   ui::PAGE_TRANSITION_AUTO_BOOKMARK) &&
      IsNavigationFromFeed(web_contents,
                           navigation_handle->GetRedirectChain()[0])) {
    return false;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  return true;
}

// Returns the page associated with `opener_web_contents`.
std::optional<history::Opener> GetHistoryOpenerFromOpenerWebContents(
    base::WeakPtr<content::WebContents> opener_web_contents) {
  if (!opener_web_contents)
    return std::nullopt;

  // The last committed entry could hypothetically change from when the opener
  // was set on `HistoryTabHelper` to when this function gets called. It is
  // unlikely that it will change since we should only be calling this on
  // the first navigation this tab helper observes, but we are fine with that
  // edge case.
  auto* last_committed_entry =
      opener_web_contents->GetController().GetLastCommittedEntry();
  if (!last_committed_entry)
    return std::nullopt;

  return history::Opener(
      history::ContextIDForWebContents(opener_web_contents.get()),
      last_committed_entry->GetUniqueID(),
      opener_web_contents->GetLastCommittedURL());
}

history::VisitContextAnnotations::BrowserType GetBrowserType(
    WebContents* web_contents) {
#if BUILDFLAG(IS_ANDROID)
  TabModel* tab_model = TabModelList::GetTabModelForWebContents(web_contents);
  if (!tab_model) {
    return history::VisitContextAnnotations::BrowserType::kUnknown;
  }
  switch (tab_model->activity_type()) {
    case chrome::android::ActivityType::kTabbed:
      return history::VisitContextAnnotations::BrowserType::kTabbed;
    case chrome::android::ActivityType::kCustomTab:
      return history::VisitContextAnnotations::BrowserType::kCustomTab;
    case chrome::android::ActivityType::kAuthTab:
      return history::VisitContextAnnotations::BrowserType::kAuthTab;
    case chrome::android::ActivityType::kTrustedWebActivity:
    case chrome::android::ActivityType::kWebapp:
    case chrome::android::ActivityType::kWebApk:
    case chrome::android::ActivityType::kPreFirstTab:
      return history::VisitContextAnnotations::BrowserType::kUnknown;
  }
#else
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    return history::VisitContextAnnotations::BrowserType::kUnknown;
  }
  switch (browser->type()) {
    case Browser::TYPE_NORMAL:
      return history::VisitContextAnnotations::BrowserType::kTabbed;
    case Browser::TYPE_POPUP:
    case Browser::TYPE_APP:
    case Browser::TYPE_APP_POPUP:
    case Browser::TYPE_PICTURE_IN_PICTURE:
      return history::VisitContextAnnotations::BrowserType::kPopup;
    case Browser::TYPE_DEVTOOLS:
      return history::VisitContextAnnotations::BrowserType::kUnknown;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case Browser::TYPE_CUSTOM_TAB:
      return history::VisitContextAnnotations::BrowserType::kCustomTab;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

history::VisitContentAnnotations::PasswordState
ConvertSessionsPasswordStateToHistory(
    sessions::SerializedNavigationEntry::PasswordState password_state) {
  switch (password_state) {
    case sessions::SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN:
      return history::VisitContentAnnotations::PasswordState::kUnknown;
    case sessions::SerializedNavigationEntry::NO_PASSWORD_FIELD:
      return history::VisitContentAnnotations::PasswordState::kNoPasswordField;
    case sessions::SerializedNavigationEntry::HAS_PASSWORD_FIELD:
      return history::VisitContentAnnotations::PasswordState::kHasPasswordField;
  }
}

}  // namespace

HistoryTabHelper::HistoryTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<HistoryTabHelper>(*web_contents) {
  // A translate client is not always attached to web contents (e.g. tests).
  if (ChromeTranslateClient* translate_client =
          ChromeTranslateClient::FromWebContents(web_contents)) {
    translate_observation_.Observe(translate_client->GetTranslateDriver());
  }
}

HistoryTabHelper::~HistoryTabHelper() = default;

void HistoryTabHelper::UpdateHistoryForNavigation(
    const history::HistoryAddPageArgs& add_page_args) {
  history::HistoryService* history_service = GetHistoryService();
  if (!history_service)
    return;

  // Update the previous navigation's end time.
  if (cached_navigation_state_) {
    history_service->UpdateWithPageEndTime(
        history::ContextIDForWebContents(web_contents()),
        cached_navigation_state_->nav_entry_id, cached_navigation_state_->url,
        base::Time::Now());
  }
  // Cache the relevant fields of the current navigation, so we can later update
  // its end time too.
  cached_navigation_state_ = {add_page_args.nav_entry_id, add_page_args.url};

  // Now, actually add the new navigation to history.
  history_service->AddPage(add_page_args);
}

history::HistoryAddPageArgs HistoryTabHelper::CreateHistoryAddPageArgs(
    const GURL& virtual_url,
    base::Time timestamp,
    int nav_entry_id,
    content::NavigationHandle* navigation_handle) {
  const ui::PageTransition page_transition =
      navigation_handle->GetPageTransition();
  int http_response_code =
      navigation_handle->GetResponseHeaders()
          ? navigation_handle->GetResponseHeaders()->response_code()
          : 0;
  const bool status_code_is_error =
      (http_response_code >= 400) && (http_response_code < 600);
  // Top-level frame navigations are visible; everything else is hidden.
  // Also hide top-level navigations that result in an error in order to
  // prevent the omnibox from suggesting URLs that have never been navigated
  // to successfully.  (If a top-level navigation to the URL succeeds at some
  // point, the URL will be unhidden and thus eligible to be suggested by the
  // omnibox.)
  const bool hidden =
      !ui::PageTransitionIsMainFrame(navigation_handle->GetPageTransition()) ||
      status_code_is_error;

  // If the full referrer URL is provided, use that. Otherwise, we probably have
  // an incomplete referrer due to referrer policy (empty or origin-only).
  // Fall back to the previous main frame URL if the referrer policy required
  // that only the origin be sent as the referrer and it matches the previous
  // main frame URL.
  GURL referrer_url = navigation_handle->GetReferrer().url;
  if (navigation_handle->IsInPrimaryMainFrame() && !referrer_url.is_empty() &&
      referrer_url == referrer_url.DeprecatedGetOriginAsURL() &&
      referrer_url.DeprecatedGetOriginAsURL() ==
          navigation_handle->GetPreviousPrimaryMainFrameURL()
              .DeprecatedGetOriginAsURL()) {
    referrer_url = navigation_handle->GetPreviousPrimaryMainFrameURL();
  }

  history::VisitContextAnnotations::OnVisitFields context_annotations;

  context_annotations.browser_type = GetBrowserType(web_contents());

  context_annotations.window_id =
      sessions::SessionTabHelper::IdForWindowContainingTab(web_contents());
  context_annotations.tab_id =
      sessions::SessionTabHelper::IdForTab(web_contents());

  // Note: We can't use TaskTabHelper::get_task_id_for_navigation() here - that
  // wants the ID of a NavigationEntry, but we have a NavigationHandle which has
  // a different ID.
  const sessions::NavigationTaskId* nav_task_id =
      tasks::TaskTabHelper::GetCurrentTaskId(web_contents());
  if (nav_task_id) {
    context_annotations.task_id = nav_task_id->id();
    context_annotations.root_task_id = nav_task_id->root_id();
    context_annotations.parent_task_id = nav_task_id->parent_id();
  }

  context_annotations.response_code = http_response_code;

  ChromeNavigationUIData* chrome_ui_data =
      navigation_handle->GetNavigationUIData() == nullptr
          ? nullptr
          : static_cast<ChromeNavigationUIData*>(
                navigation_handle->GetNavigationUIData());

  // (crbug.com/365922169) When generating the HistoryAddPageArgs below, we must
  // calculate the value for its member `is_ephemeral`. This member represents
  // whether our navigation came from a credentialless iframe (which is an
  // ephemeral context). Our goal is to use this information to avoid storing
  // ephemeral navigations from credentialless iframes in the history backend.
  // Currently, this is behavior which will be tested behind the partitioned
  // :visited links experiments flags (PartitionVisitedLinkDatabase and
  // PartitionVisitedLinkDatabaseWithSelfLinks). HOWEVER, due to layering
  // constraints, we do not have the ability to check these blink::feature flags
  // in any code found in components/history/core/ (which is where most history
  // DB code lives).

  // Instead, we check the values of these flags here - setting `is_ephemeral`
  // to false if neither of these experimental flags are enabled. Once the
  // experiments have completed, is_ephemeral will go back to being a pure check
  // of whether the navigation is from a credentialless iframe.
  const bool are_partitioned_visited_links_enabled =
      base::FeatureList::IsEnabled(
          blink::features::kPartitionVisitedLinkDatabase) ||
      base::FeatureList::IsEnabled(
          blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks);

  history::HistoryAddPageArgs add_page_args(
      navigation_handle->GetURL(), timestamp,
      history::ContextIDForWebContents(web_contents()), nav_entry_id,
      navigation_handle->GetNavigationId(), referrer_url,
      navigation_handle->GetRedirectChain(), page_transition, hidden,
      history::SOURCE_BROWSED, navigation_handle->DidReplaceEntry(),
      ShouldConsiderForNtpMostVisited(*web_contents(), navigation_handle),
      // Reloads do not result in calling TitleWasSet() (which normally sets
      // the title), so a reload needs to set the title. This is
      // important for a reload after clearing history.
      navigation_handle->IsSameDocument() ||
              navigation_handle->GetReloadType() != content::ReloadType::NONE
          ? std::optional<std::u16string>(
                navigation_handle->GetWebContents()->GetTitle())
          : std::nullopt,
      // Our top-level site is the previous primary main frame.
      navigation_handle->GetPreviousPrimaryMainFrameURL(),
      // Only compute the opener page if it's the first committed page for this
      // WebContents.
      navigation_handle->GetPreviousPrimaryMainFrameURL().is_empty()
          ? GetHistoryOpenerFromOpenerWebContents(opener_web_contents_)
          // Or use the opener for same-document navigations to connect these
          // visits.
          : (navigation_handle->IsSameDocument()
                 ? std::make_optional(history::Opener(
                       history::ContextIDForWebContents(web_contents()),
                       nav_entry_id,
                       navigation_handle->GetPreviousPrimaryMainFrameURL()))
                 : std::nullopt),
      chrome_ui_data == nullptr ? std::nullopt : chrome_ui_data->bookmark_id(),
      app_id_, std::move(context_annotations),
      are_partitioned_visited_links_enabled
          ? navigation_handle->GetRenderFrameHost()
                ->GetStorageKey()
                .nonce()
                .has_value()
          : false);

  if (ui::PageTransitionIsMainFrame(page_transition) &&
      virtual_url != navigation_handle->GetURL()) {
    // Hack on the "virtual" URL so that it will appear in history. For some
    // types of URLs, we will display a magic URL that is different from where
    // the page is actually navigated. We want the user to see in history what
    // they saw in the URL bar, so we add the virtual URL as a redirect.  This
    // only applies to the main frame, as the virtual URL doesn't apply to
    // sub-frames.
    add_page_args.url = virtual_url;
    if (!add_page_args.redirects.empty())
      add_page_args.redirects.back() = virtual_url;
  }
  return add_page_args;
}

void HistoryTabHelper::OnPasswordStateUpdated(
    sessions::SerializedNavigationEntry::PasswordState password_state) {
  if (history::HistoryService* hs = GetHistoryService()) {
    NavigationEntry* entry =
        web_contents()->GetController().GetLastCommittedEntry();
    if (entry) {
      hs->SetPasswordStateForVisit(
          history::ContextIDForWebContents(web_contents()),
          entry->GetUniqueID(), web_contents()->GetLastCommittedURL(),
          ConvertSessionsPasswordStateToHistory(password_state));
    }
  }
}

void HistoryTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted())
    return;

  if (navigation_handle->IsInPrimaryMainFrame()) {
    is_loading_ = true;
    num_title_changes_ = 0;
  } else if (!navigation_handle->IsInMainFrame() &&
             !navigation_handle->HasSubframeNavigationEntryCommitted()) {
    // Filter out unwanted URLs. We don't add auto-subframe URLs that don't
    // change which NavigationEntry is current. They are a large part of history
    // (think iframes for ads) and we never display them in history UI. We will
    // still add manual subframes, which are ones the user has clicked on to
    // get.
    return;
  }

  // Update history. Note that this needs to happen after the entry is complete,
  // which WillNavigate[Main,Sub]Frame will do before this function is called.
  if (!navigation_handle->ShouldUpdateHistory())
    return;

  // No-state prefetch should not update history. The prefetch will have its own
  // WebContents with all observers (including |this|), and go through the
  // normal flow of a navigation, including commit.
  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());
  if (no_state_prefetch_manager &&
      no_state_prefetch_manager->IsWebContentsPrefetching(web_contents())) {
    return;
  }

  DCHECK(navigation_handle->GetRenderFrameHost()->GetPage().IsPrimary());

  // Most of the time, the displayURL matches the loaded URL, but for about:
  // URLs, we use a data: URL as the real value.  We actually want to save the
  // about: URL to the history db and keep the data: URL hidden. This is what
  // the WebContents' URL getter does.
  NavigationEntry* last_committed =
      web_contents()->GetController().GetLastCommittedEntry();
  base::Time timestamp = last_committed->GetTimestamp();
  history::HistoryAddPageArgs add_page_args = CreateHistoryAddPageArgs(
      web_contents()->GetLastCommittedURL(), timestamp,
      last_committed->GetUniqueID(), navigation_handle);

  if (!IsEligibleTab(add_page_args))
    return;

  UpdateHistoryForNavigation(add_page_args);

  if (HistoryClustersTabHelper* clusters_tab_helper =
          HistoryClustersTabHelper::FromWebContents(web_contents())) {
    clusters_tab_helper->OnUpdatedHistoryForNavigation(
        navigation_handle->GetNavigationId(), timestamp, add_page_args.url);
  }

  if (HistoryEmbeddingsTabHelper* embeddings_tab_helper =
          HistoryEmbeddingsTabHelper::FromWebContents(web_contents())) {
    embeddings_tab_helper->OnUpdatedHistoryForNavigation(
        navigation_handle, timestamp, add_page_args.url);
  }
}

void HistoryTabHelper::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!render_frame_host->IsInPrimaryMainFrame())
    return;

  is_loading_ = false;
  last_load_completion_ = base::TimeTicks::Now();
}

void HistoryTabHelper::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  HistoryTabHelper* new_history_tab_helper =
      HistoryTabHelper::FromWebContents(new_contents);
  if (!new_history_tab_helper)
    return;

  // This should only be set once on a new tab helper.
  DCHECK(!new_history_tab_helper->opener_web_contents_);
  new_history_tab_helper->opener_web_contents_ = web_contents()->GetWeakPtr();
}

void HistoryTabHelper::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  if (history::HistoryService* hs = GetHistoryService()) {
    NavigationEntry* entry =
        web_contents()->GetController().GetLastCommittedEntry();
    if (entry) {
      hs->SetPageLanguageForVisit(
          history::ContextIDForWebContents(web_contents()),
          entry->GetUniqueID(), web_contents()->GetLastCommittedURL(),
          details.adopted_language);
    }
  }
}

void HistoryTabHelper::TitleWasSet(NavigationEntry* entry) {
  if (!entry)
    return;

  // Protect against pages changing their title too often.
  if (num_title_changes_ >= history::kMaxTitleChanges)
    return;

  // Only store page titles into history if they were set while the page was
  // loading or during a brief span after load is complete. This fixes the case
  // where a page uses a title change to alert a user of a situation but that
  // title change ends up saved in history.
  if (is_loading_ || (base::TimeTicks::Now() - last_load_completion_ <
                      history::GetTitleSettingWindow())) {
    history::HistoryService* hs = GetHistoryService();
    if (hs) {
      hs->SetPageTitle(entry->GetVirtualURL(), entry->GetTitleForDisplay());
      ++num_title_changes_;
    }
  }
}

history::HistoryService* HistoryTabHelper::GetHistoryService() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (profile->IsOffTheRecord())
    return nullptr;

  return HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS);
}

void HistoryTabHelper::WebContentsDestroyed() {
  translate_observation_.Reset();

  history::HistoryService* history_service = GetHistoryService();
  if (!history_service)
    return;

  history::ContextID context_id =
      history::ContextIDForWebContents(web_contents());

  // If there is a current history-eligible navigation in this tab (i.e.
  // `cached_navigation_state_` exists), that visit is concluded now, so update
  // its end time.
  if (cached_navigation_state_) {
    history_service->UpdateWithPageEndTime(
        context_id, cached_navigation_state_->nav_entry_id,
        cached_navigation_state_->url, base::Time::Now());
  }

  history_service->ClearCachedDataForContextID(context_id);
}

bool HistoryTabHelper::IsEligibleTab(
    const history::HistoryAddPageArgs& add_page_args) const {
  if (force_eligible_tab_for_testing_)
    return true;

#if BUILDFLAG(IS_ANDROID)
  if (web_contents()) {
    auto* background_tab_manager =
        BackgroundTabManager::FromWebContents(web_contents());
    if (background_tab_manager) {
      // No history insertion is done for now since this is a tab that
      // speculates future navigations. Just caching and returning for now.
      background_tab_manager->CacheHistory(add_page_args);
      return false;
    }
  }
  return true;
#else
  // Don't update history if this web contents isn't associated with a tab.
  return chrome::FindBrowserWithTab(web_contents()) != nullptr;
#endif
}

#if BUILDFLAG(IS_ANDROID)
static void JNI_HistoryTabHelper_SetAppIdNative(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& japp_id,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  auto* history_tab_helper = HistoryTabHelper::FromWebContents(web_contents);
  history_tab_helper->SetAppId(base::android::ConvertJavaStringToUTF8(japp_id));
}
#endif
WEB_CONTENTS_USER_DATA_KEY_IMPL(HistoryTabHelper);

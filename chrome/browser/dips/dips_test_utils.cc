// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_test_utils.h"

#include "base/test/bind.h"
#include "chrome/browser/dips/dips_cleanup_service_factory.h"
#include "chrome/browser/dips/dips_features.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::CookieAccessDetails;
using content::NavigationHandle;
using content::RenderFrameHost;
using content::WebContents;

void CloseTab(content::WebContents* web_contents) {
  content::WebContentsDestroyedWatcher destruction_watcher(web_contents);
  web_contents->Close();
  destruction_watcher.Wait();
}

base::expected<WebContents*, std::string> OpenInNewTab(
    WebContents* original_tab,
    const GURL& url) {
  OpenedWindowObserver tab_observer(original_tab,
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB);
  if (!content::ExecJs(original_tab,
                       content::JsReplace("window.open($1, '_blank');", url))) {
    return base::unexpected("window.open failed");
  }
  tab_observer.Wait();

  // Wait for the new tab to finish navigating.
  content::WaitForLoadStop(tab_observer.window());

  return tab_observer.window();
}

void AccessCookieViaJSIn(content::WebContents* web_contents,
                         content::RenderFrameHost* frame) {
  FrameCookieAccessObserver observer(web_contents, frame,
                                     CookieOperation::kChange);
  ASSERT_TRUE(content::ExecJs(frame, "document.cookie = 'foo=bar';",
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  observer.Wait();
}

absl::optional<StateValue> GetDIPSState(DIPSService* dips_service,
                                        const GURL& url) {
  absl::optional<StateValue> state;

  auto* storage = dips_service->storage();
  DCHECK(storage);
  storage->AsyncCall(&DIPSStorage::Read)
      .WithArgs(url)
      .Then(base::BindLambdaForTesting([&](const DIPSState& loaded_state) {
        if (loaded_state.was_loaded()) {
          state = loaded_state.ToStateValue();
        }
      }));
  WaitOnStorage(dips_service);

  return state;
}

URLCookieAccessObserver::URLCookieAccessObserver(WebContents* web_contents,
                                                 const GURL& url,
                                                 CookieOperation access_type)
    : WebContentsObserver(web_contents), url_(url), access_type_(access_type) {}

void URLCookieAccessObserver::Wait() {
  run_loop_.Run();
}

void URLCookieAccessObserver::OnCookiesAccessed(
    RenderFrameHost* render_frame_host,
    const CookieAccessDetails& details) {
  cookie_accessed_in_primary_page_ = IsInPrimaryPage(render_frame_host);

  if (details.type == access_type_ && details.url == url_) {
    run_loop_.Quit();
  }
}

void URLCookieAccessObserver::OnCookiesAccessed(
    NavigationHandle* navigation_handle,
    const CookieAccessDetails& details) {
  cookie_accessed_in_primary_page_ = IsInPrimaryPage(navigation_handle);

  if (details.type == access_type_ && details.url == url_) {
    run_loop_.Quit();
  }
}

bool URLCookieAccessObserver::CookieAccessedInPrimaryPage() const {
  return cookie_accessed_in_primary_page_;
}

FrameCookieAccessObserver::FrameCookieAccessObserver(
    WebContents* web_contents,
    RenderFrameHost* render_frame_host,
    CookieOperation access_type)
    : WebContentsObserver(web_contents),
      render_frame_host_(render_frame_host),
      access_type_(access_type) {}

void FrameCookieAccessObserver::Wait() {
  run_loop_.Run();
}

void FrameCookieAccessObserver::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  if (details.type == access_type_ && render_frame_host_ == render_frame_host) {
    run_loop_.Quit();
  }
}

RedirectChainObserver::RedirectChainObserver(DIPSService* service,
                                             GURL final_url)
    : final_url_(std::move(final_url)) {
  obs_.Observe(service);
}

RedirectChainObserver::~RedirectChainObserver() = default;

void RedirectChainObserver::OnChainHandled(
    const DIPSRedirectChainInfoPtr& chain) {
  handle_call_count++;
  if (chain->final_url == final_url_) {
    run_loop_.Quit();
  }
}

void RedirectChainObserver::Wait() {
  run_loop_.Run();
}

UserActivationObserver::UserActivationObserver(
    WebContents* web_contents,
    RenderFrameHost* render_frame_host)
    : WebContentsObserver(web_contents),
      render_frame_host_(render_frame_host) {}

void UserActivationObserver::Wait() {
  run_loop_.Run();
}

void UserActivationObserver::FrameReceivedUserActivation(
    RenderFrameHost* render_frame_host) {
  if (render_frame_host_ == render_frame_host) {
    run_loop_.Quit();
  }
}

EntryUrlsAre::EntryUrlsAre(std::string entry_name,
                           std::vector<std::string> urls)
    : entry_name_(std::move(entry_name)), expected_urls_(std::move(urls)) {
  // Sort the URLs before comparing, so order doesn't matter. (DIPSDatabase
  // currently sorts its results, but that could change and these tests
  // shouldn't care.)
  std::sort(expected_urls_.begin(), expected_urls_.end());
}

EntryUrlsAre::EntryUrlsAre(const EntryUrlsAre&) = default;
EntryUrlsAre::EntryUrlsAre(EntryUrlsAre&&) = default;
EntryUrlsAre::~EntryUrlsAre() = default;

bool EntryUrlsAre::MatchAndExplain(
    const ukm::TestUkmRecorder& ukm_recorder,
    testing::MatchResultListener* result_listener) const {
  std::vector<std::string> actual_urls;
  for (const auto* entry : ukm_recorder.GetEntriesByName(entry_name_)) {
    GURL url = ukm_recorder.GetSourceForSourceId(entry->source_id)->url();
    actual_urls.push_back(url.spec());
  }
  std::sort(actual_urls.begin(), actual_urls.end());

  // ExplainMatchResult() won't print out the full contents of `actual_urls`,
  // so for more helpful error messages, we do it ourselves.
  *result_listener << "whose entries for '" << entry_name_
                   << "' contain the URLs "
                   << testing::PrintToString(actual_urls) << ", ";

  // Use ContainerEq() instead of e.g. ElementsAreArray() because the error
  // messages are much more useful.
  return ExplainMatchResult(testing::ContainerEq(expected_urls_), actual_urls,
                            result_listener);
}

void EntryUrlsAre::DescribeTo(std::ostream* os) const {
  *os << "has entries for '" << entry_name_ << "' whose URLs are "
      << testing::PrintToString(expected_urls_);
}

void EntryUrlsAre::DescribeNegationTo(std::ostream* os) const {
  *os << "does not have entries for '" << entry_name_ << "' whose URLs are "
      << testing::PrintToString(expected_urls_);
}

ScopedInitFeature::ScopedInitFeature(const base::Feature& feature,
                                     bool enable,
                                     const base::FieldTrialParams& params) {
  if (enable) {
    feature_list_.InitAndEnableFeatureWithParameters(feature, params);
  } else {
    feature_list_.InitAndDisableFeature(feature);
  }
}

ScopedInitDIPSFeature::ScopedInitDIPSFeature(
    bool enable,
    const base::FieldTrialParams& params)
    // DIPSServiceFactory and DIPSCleanupServiceFactory are singletons, and we
    // want to create them *before* constructing `init_feature_`, so that they
    // are initialized using the default value of dips::kFeature. We only want
    // `init_feature_` to affect CreateProfileSelections(). We do this
    // concisely by using the comma operator in the arguments to
    // `init_feature_` to call DIPSServiceFactory::GetInstance() and
    // DIPSCleanupServiceFactory::GetInstance() while ignoring their return
    // values.
    : init_feature_((DIPSServiceFactory::GetInstance(),
                     DIPSCleanupServiceFactory::GetInstance(),
                     dips::kFeature),
                    enable,
                    params),
      override_profile_selections_for_dips_service_(
          DIPSServiceFactory::GetInstance(),
          DIPSServiceFactory::CreateProfileSelections()),
      override_profile_selections_for_dips_cleanup_service_(
          DIPSCleanupServiceFactory::GetInstance(),
          DIPSCleanupServiceFactory::CreateProfileSelections()) {}

OpenedWindowObserver::OpenedWindowObserver(
    content::WebContents* web_contents,
    WindowOpenDisposition open_disposition)
    : WebContentsObserver(web_contents), open_disposition_(open_disposition) {}

void OpenedWindowObserver::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  if (!window_ && disposition == open_disposition_) {
    window_ = new_contents;
    run_loop_.Quit();
  }
}

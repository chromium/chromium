// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_test_utils.h"

#include <string_view>

#include "base/test/bind.h"
#include "chrome/browser/dips/dips_cleanup_service_factory.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/dips/dips_service_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
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

bool NavigateToSetCookie(content::WebContents* web_contents,
                         const net::EmbeddedTestServer* server,
                         std::string_view host,
                         bool is_secure_cookie_set,
                         bool is_ad_tagged) {
  std::string relative_url = "/set-cookie?name=value";
  if (is_secure_cookie_set) {
    relative_url += ";Secure;SameSite=None";
  }
  if (is_ad_tagged) {
    relative_url += "&isad=1";
  }
  const auto url = server->GetURL(host, relative_url);

  URLCookieAccessObserver observer(web_contents, url, CookieOperation::kChange);
  bool success = content::NavigateToURL(web_contents, url);
  if (success) {
    observer.Wait();
  }
  return success;
}

void CreateImageAndWaitForCookieAccess(content::WebContents* web_contents,
                                       const GURL& image_url) {
  URLCookieAccessObserver observer(web_contents, image_url,
                                   CookieOperation::kRead);
  ASSERT_TRUE(content::ExecJs(web_contents,
                              content::JsReplace(
                                  R"(
    let img = document.createElement('img');
    img.src = $1;
    document.body.appendChild(img);)",
                                  image_url),
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  // The image must cause a cookie access, or else this will hang.
  observer.Wait();
}

std::optional<StateValue> GetDIPSState(DIPSServiceImpl* dips_service,
                                       const GURL& url) {
  std::optional<StateValue> state;

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
                                             GURL final_url,
                                             size_t expected_match_count)
    : final_url_(std::move(final_url)),
      expected_match_count_(expected_match_count) {
  obs_.Observe(service);
}

RedirectChainObserver::~RedirectChainObserver() = default;

void RedirectChainObserver::OnChainHandled(
    const DIPSRedirectChainInfoPtr& chain) {
  handle_call_count++;
  if (chain->final_url.url == final_url_ &&
      ++match_count_ == expected_match_count_) {
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
  for (const ukm::mojom::UkmEntry* entry :
       ukm_recorder.GetEntriesByName(entry_name_)) {
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
    : init_feature_(features::kDIPS, enable, params) {}

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

void SimulateMouseClickAndWait(WebContents* web_contents) {
  content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());
  UserActivationObserver observer(web_contents,
                                  web_contents->GetPrimaryMainFrame());
  content::SimulateMouseClick(web_contents, 0,
                              blink::WebMouseEvent::Button::kLeft);
  observer.Wait();
}

UrlAndSourceId MakeUrlAndId(std::string_view url) {
  return UrlAndSourceId(GURL(url), ukm::AssignNewSourceId());
}

testing::AssertionResult SimulateDipsBounce(content::WebContents* web_contents,
                                            const GURL& initial_url,
                                            const GURL& bounce_url,
                                            const GURL& final_url,
                                            const GURL& next_initial_url) {
  if (web_contents->GetLastCommittedURL() == initial_url) {
    return testing::AssertionFailure() << "Already on " << initial_url;
  }

  DIPSService* dips_service =
      DIPSService::Get(web_contents->GetBrowserContext());
  RedirectChainObserver initial_observer(dips_service, initial_url);
  if (!content::NavigateToURL(web_contents, initial_url)) {
    return testing::AssertionFailure()
           << "Failed to navigate to " << initial_url;
  }
  initial_observer.Wait();

  if (testing::Test::HasFailure()) {
    return testing::AssertionFailure()
           << "Failure generated while waiting for the previous redirect chain "
              "to be reported";
  }

  if (!content::NavigateToURLFromRenderer(web_contents, bounce_url)) {
    return testing::AssertionFailure()
           << "Failed to navigate to " << bounce_url;
  }

  testing::AssertionResult js_result =
      content::ExecJs(web_contents, "document.cookie = 'bounce=stateful';",
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE);
  if (!js_result) {
    return js_result;
  }

  RedirectChainObserver final_observer(dips_service, final_url);
  if (!content::NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                            final_url)) {
    return testing::AssertionFailure() << "Failed to navigate to " << final_url;
  }

  // End redirect chain by navigating with a user gesture.
  if (!content::NavigateToURLFromRenderer(web_contents, next_initial_url)) {
    return testing::AssertionFailure()
           << "Failed to navigate to " << next_initial_url;
  }
  final_observer.Wait();

  if (testing::Test::HasFailure()) {
    return testing::AssertionFailure() << "Failure generated while waiting for "
                                          "the redirect chain to be reported";
  }

  return testing::AssertionSuccess();
}

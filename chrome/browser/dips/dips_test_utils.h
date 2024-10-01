// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_TEST_UTILS_H_
#define CHROME_BROWSER_DIPS_DIPS_TEST_UTILS_H_

#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "chrome/browser/dips/dips_redirect_info.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_service_impl.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace testing {
class MatchResultListener;
}

constexpr char kStorageAccessScript[] = R"(
    async function accessDatabase() {
      var my_db = openDatabase('my_db', '1.0', 'description', 1024);
      var num_rows;
      await new Promise((resolve, reject) => {
        my_db.transaction((tx) => {
          tx.executeSql('CREATE TABLE IF NOT EXISTS tbl (id unique, data)');
          tx.executeSql('INSERT INTO tbl (id, data) VALUES (1, "foo")');
          tx.executeSql('SELECT * FROM tbl', [], (tx, results) => {
            num_rows = results.rows.length;
          });
        }, reject, resolve);
      });
      if(num_rows <= 0) {throw new Error('Failed to access!')}
    }

    function accessLocalStorage() {
      localStorage.setItem('foo', 'bar');
      return localStorage.getItem('foo');
    }

    function accessSessionStorage() {
      sessionStorage.setItem('foo', 'bar');
      return sessionStorage.getItem('foo') == 'bar';
    }

    async function accessFileSystem() {
      const fs = await new Promise((resolve, reject) => {
        window.webkitRequestFileSystem(TEMPORARY, 1024, resolve, reject);
      });
      return new Promise((resolve, reject) => {
        fs.root.getFile('foo.txt', {create: true, exclusive: true}, resolve,
          reject);
      });
    }

    function accessIndexedDB() {
      var request = indexedDB.open('my_db', 2);

      request.onupgradeneeded = () => {
        request.result.createObjectStore('store');
      }
      return new Promise((resolve) => {
        request.onsuccess = () => {
          request.result.close();
          resolve(true);
        }
        request.onerror = () => {throw new Error('Failed to access!')}
      });
    }

    function accessCacheStorage() {
      return caches.open("cache")
      .then((cache) => cache.put("/foo", new Response("bar")))
      .then(() => true)
      .catch(() => {throw new Error('Failed to access!')});
    }

    // Placeholder for execution statement.
    access%s();
  )";

using StateForURLCallback = base::OnceCallback<void(DIPSState)>;

// Helper function to close (and waits for closure of) a `web_contents` tab.
void CloseTab(content::WebContents* web_contents);

// Helper function to open a link to the given URL in a new tab and return the
// new tab's WebContents.
base::expected<content::WebContents*, std::string> OpenInNewTab(
    content::WebContents* original_tab,
    const GURL& url);

// Helper function for performing client side cookie access via JS.
void AccessCookieViaJSIn(content::WebContents* web_contents,
                         content::RenderFrameHost* frame);

// Helper function to navigate to /set-cookie on `host` and wait for
// OnCookiesAccessed() to be called.
bool NavigateToSetCookie(content::WebContents* web_contents,
                         const net::EmbeddedTestServer* server,
                         std::string_view host,
                         bool is_secure_cookie_set,
                         bool is_ad_tagged);

// Helper function for creating an image with a cookie access on the provided
// WebContents.
void CreateImageAndWaitForCookieAccess(content::WebContents* web_contents,
                                       const GURL& image_url);

// Helper function to block until all DIPS storage requests are complete.
inline void WaitOnStorage(DIPSServiceImpl* dips_service) {
  dips_service->storage()->FlushPostedTasksForTesting();
}

// Helper function to query the `url` state from DIPS storage.
std::optional<StateValue> GetDIPSState(DIPSServiceImpl* dips_service,
                                       const GURL& url);

inline DIPSServiceImpl* GetDipsService(content::WebContents* web_contents) {
  return DIPSServiceImpl::Get(web_contents->GetBrowserContext());
}

class URLCookieAccessObserver : public content::WebContentsObserver {
 public:
  URLCookieAccessObserver(content::WebContents* web_contents,
                          const GURL& url,
                          CookieOperation access_type);

  void Wait();

  bool CookieAccessedInPrimaryPage() const;

 private:
  // WebContentsObserver overrides
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;
  void OnCookiesAccessed(content::NavigationHandle* navigation_handle,
                         const content::CookieAccessDetails& details) override;

  GURL url_;
  CookieOperation access_type_;
  bool cookie_accessed_in_primary_page_ = false;
  base::RunLoop run_loop_;
};

class FrameCookieAccessObserver : public content::WebContentsObserver {
 public:
  explicit FrameCookieAccessObserver(
      content::WebContents* web_contents,
      content::RenderFrameHost* render_frame_host,
      CookieOperation access_type);

  // Wait until the frame accesses cookies.
  void Wait();

  // WebContentsObserver override
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;

 private:
  const raw_ptr<content::RenderFrameHost, AcrossTasksDanglingUntriaged>
      render_frame_host_;
  CookieOperation access_type_;
  base::RunLoop run_loop_;
};

class RedirectChainObserver : public DIPSService::Observer {
 public:
  explicit RedirectChainObserver(DIPSService* service,
                                 GURL final_url,
                                 size_t expected_match_count = 1);
  ~RedirectChainObserver() override;

  void OnChainHandled(const DIPSRedirectChainInfoPtr& chain) override;

  void Wait();

  size_t handle_call_count = 0;

 private:
  GURL final_url_;
  size_t match_count_ = 0;
  size_t expected_match_count_;
  base::RunLoop run_loop_;
  base::ScopedObservation<DIPSService, Observer> obs_{this};
};

class UserActivationObserver : public content::WebContentsObserver {
 public:
  explicit UserActivationObserver(content::WebContents* web_contents,
                                  content::RenderFrameHost* render_frame_host);

  // Wait until the frame receives user activation.
  void Wait();

 private:
  // WebContentsObserver override
  void FrameReceivedUserActivation(
      content::RenderFrameHost* render_frame_host) override;

  raw_ptr<content::RenderFrameHost, AcrossTasksDanglingUntriaged> const
      render_frame_host_;
  base::RunLoop run_loop_;
};

// Checks that the URLs associated with the UKM entries with the given name are
// as expected. Sorts the URLs so order doesn't matter.
//
// Example usage:
//
// EXPECT_THAT(ukm_recorder, EntryUrlsAre(entry_name, {url1, url2, url3}));
class EntryUrlsAre {
 public:
  using is_gtest_matcher = void;
  EntryUrlsAre(std::string entry_name, std::vector<std::string> urls);
  EntryUrlsAre(const EntryUrlsAre&);
  EntryUrlsAre(EntryUrlsAre&&);
  ~EntryUrlsAre();

  using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
  bool MatchAndExplain(const ukm::TestUkmRecorder& ukm_recorder,
                       testing::MatchResultListener* result_listener) const;

  void DescribeTo(std::ostream* os) const;
  void DescribeNegationTo(std::ostream* os) const;

 private:
  std::string entry_name_;
  std::vector<std::string> expected_urls_;
};

// Enables or disables a base::Feature.
class ScopedInitFeature {
 public:
  explicit ScopedInitFeature(const base::Feature& feature,
                             bool enable,
                             const base::FieldTrialParams& params);

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Enables/disables the DIPS Feature.
class ScopedInitDIPSFeature {
 public:
  explicit ScopedInitDIPSFeature(bool enable,
                                 const base::FieldTrialParams& params = {});

 private:
  ScopedInitFeature init_feature_;
};

// Waits for a window to open.
class OpenedWindowObserver : public content::WebContentsObserver {
 public:
  explicit OpenedWindowObserver(content::WebContents* web_contents,
                                WindowOpenDisposition open_disposition);

  void Wait() { run_loop_.Run(); }
  content::WebContents* window() { return window_; }

 private:
  // WebContentsObserver overrides:
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;

  const WindowOpenDisposition open_disposition_;
  raw_ptr<content::WebContents> window_ = nullptr;
  base::RunLoop run_loop_;
};

// Simulate a mouse click and wait for the main frame to receive user
// activation.
void SimulateMouseClickAndWait(content::WebContents*);

// Make a UrlAndSourceId with a randomly-generated UKM source id.
UrlAndSourceId MakeUrlAndId(std::string_view url);

// Cause DIPS to record a stateful client bounce on `bounce_url` to `final_url`.
// The redirect chain will be started by performing a browser-initiated
// navigation to `initial_url`, and terminated by another such navigation to
// `next_url`.
testing::AssertionResult SimulateDipsBounce(content::WebContents*,
                                            const GURL& initial_url,
                                            const GURL& bounce_url,
                                            const GURL& final_url,
                                            const GURL& next_url);

#endif  // CHROME_BROWSER_DIPS_DIPS_TEST_UTILS_H_

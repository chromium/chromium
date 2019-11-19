// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_TEST_UTILS_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_TEST_UTILS_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/synchronization/lock.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/safe_browsing/db/test_database_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "url/gurl.h"

namespace prerender {

namespace test_utils {

// A SafeBrowsingDatabaseManager implementation that returns a fixed result for
// a given URL.
class FakeSafeBrowsingDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  FakeSafeBrowsingDatabaseManager();

  // Called on the IO thread to check if the given url is safe or not.  If we
  // can synchronously determine that the url is safe, CheckUrl returns true.
  // Otherwise it returns false, and "client" is called asynchronously with the
  // result when it is ready.
  // Returns true, indicating a SAFE result, unless the URL is the fixed URL
  // specified by the user, and the user-specified result is not SAFE
  // (in which that result will be communicated back via a call into the
  // client, and false will be returned).
  // Overrides SafeBrowsingDatabaseManager::CheckBrowseUrl.
  bool CheckBrowseUrl(const GURL& gurl,
                      const safe_browsing::SBThreatTypeSet& threat_types,
                      Client* client) override;

  void SetThreatTypeForUrl(const GURL& url,
                           safe_browsing::SBThreatType threat_type) {
    bad_urls_[url.spec()] = threat_type;
  }

  // These are called when checking URLs, so we implement them.
  bool IsSupported() const override;
  bool ChecksAreAlwaysAsync() const override;
  bool CanCheckResourceType(
      content::ResourceType /* resource_type */) const override;

  bool CheckExtensionIDs(const std::set<std::string>& extension_ids,
                         Client* client) override;

 private:
  ~FakeSafeBrowsingDatabaseManager() override;

  void OnCheckBrowseURLDone(const GURL& gurl, Client* client);

  std::unordered_map<std::string, safe_browsing::SBThreatType> bad_urls_;
  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingDatabaseManager);
};

// PrerenderContents that stops the UI message loop on DidStopLoading().
class TestPrerenderContents : public PrerenderContents,
                              public content::RenderWidgetHostObserver {
 public:
  TestPrerenderContents(PrerenderManager* prerender_manager,
                        Profile* profile,
                        const GURL& url,
                        const content::Referrer& referrer,
                        const base::Optional<url::Origin>& initiator_origin,
                        Origin origin,
                        FinalStatus expected_final_status,
                        bool ignore_final_status);

  ~TestPrerenderContents() override;

  bool CheckURL(const GURL& url) override;

  // For tests that open the prerender in a new background tab, the RenderView
  // will not have been made visible when the PrerenderContents is destroyed
  // even though it is used.
  void set_should_be_shown(bool value) { should_be_shown_ = value; }

  // For tests which do not know whether the prerender will be used.
  void set_skip_final_checks(bool value) { skip_final_checks_ = value; }

  FinalStatus expected_final_status() const { return expected_final_status_; }

 private:
  void OnRenderViewHostCreated(
      content::RenderViewHost* new_render_view_host) override;
  void RenderWidgetHostVisibilityChanged(content::RenderWidgetHost* widget_host,
                                         bool became_visible) override;
  void RenderWidgetHostDestroyed(
      content::RenderWidgetHost* widget_host) override;

  FinalStatus expected_final_status_;

  ScopedObserver<content::RenderWidgetHost, content::RenderWidgetHostObserver>
      observer_;
  // The RenderViewHost created for the prerender, if any.
  content::RenderViewHost* new_render_view_host_;
  // Set to true when the prerendering RenderWidget is hidden.
  bool was_hidden_;
  // Set to true when the prerendering RenderWidget is shown, after having been
  // hidden.
  bool was_shown_;
  // Expected final value of was_shown_.  Defaults to true for
  // FINAL_STATUS_USED, and false otherwise.
  bool should_be_shown_;
  // If true, |expected_final_status_| and other shutdown checks are skipped.
  bool skip_final_checks_;

  DISALLOW_COPY_AND_ASSIGN(TestPrerenderContents);
};

// A handle to a TestPrerenderContents whose lifetime is under the caller's
// control. A PrerenderContents may be destroyed at any point. This allows
// tracking the FinalStatus.
class TestPrerender : public PrerenderContents::Observer,
                      public base::SupportsWeakPtr<TestPrerender> {
 public:
  TestPrerender();
  ~TestPrerender() override;

  TestPrerenderContents* contents() const { return contents_; }
  int number_of_loads() const { return number_of_loads_; }
  FinalStatus GetFinalStatus() const;

  void WaitForCreate();
  void WaitForStart();
  void WaitForStop();

  // Waits for |number_of_loads()| to be at least |expected_number_of_loads| OR
  // for the prerender to stop running (just to avoid a timeout if the prerender
  // dies). Note: this does not assert equality on the number of loads; the
  // caller must do it instead.
  void WaitForLoads(int expected_number_of_loads);

  void OnPrerenderCreated(TestPrerenderContents* contents);

  // PrerenderContents::Observer implementation:
  void OnPrerenderStart(PrerenderContents* contents) override;

  void OnPrerenderStopLoading(PrerenderContents* contents) override;

  void OnPrerenderStop(PrerenderContents* contents) override;

 private:
  TestPrerenderContents* contents_;
  FinalStatus final_status_;
  int number_of_loads_;

  int expected_number_of_loads_;
  std::unique_ptr<base::RunLoop> load_waiter_;

  bool started_;
  bool stopped_;

  base::RunLoop create_loop_;
  base::RunLoop start_loop_;
  base::RunLoop stop_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestPrerender);
};

// Blocks until a TestPrerenderContents has been destroyed with the given final
// status. Should be created with a TestPrerenderContents, and then
// WaitForDestroy should be called and its return value checked.
class DestructionWaiter {
 public:
  // Does not own the prerender_contents, which must outlive any call to
  // WaitForDestroy().
  DestructionWaiter(TestPrerenderContents* prerender_contents,
                    FinalStatus expected_final_status);

  ~DestructionWaiter();

  // Returns true if the TestPrerenderContents was destroyed with the correct
  // final status, or false otherwise. Note this also may hang if the contents
  // is never destroyed (which will presumably cause the test to time out).
  bool WaitForDestroy();

 private:
  class DestructionMarker : public PrerenderContents::Observer {
   public:
    // Does not own the waiter which must outlive the TestPrerenderContents.
    explicit DestructionMarker(DestructionWaiter* waiter);

    ~DestructionMarker() override;

    void OnPrerenderStop(PrerenderContents* contents) override;

   private:
    DestructionWaiter* waiter_;

    DISALLOW_COPY_AND_ASSIGN(DestructionMarker);
  };

  // To be called by a DestructionMarker.
  void MarkDestruction(FinalStatus reason);

  base::RunLoop wait_loop_;
  FinalStatus expected_final_status_;
  bool saw_correct_status_;
  std::unique_ptr<DestructionMarker> marker_;

  DISALLOW_COPY_AND_ASSIGN(DestructionWaiter);
};

// Wait until a PrerenderManager has seen a first contentful paint.
class FirstContentfulPaintManagerWaiter : public PrerenderManagerObserver {
 public:
  // Create and return a pointer to a |FirstContentfulPaintManagerWaiter|. The
  // instance is owned by the |PrerenderManager|.
  static FirstContentfulPaintManagerWaiter* Create(PrerenderManager* manager);

  ~FirstContentfulPaintManagerWaiter() override;

  void OnFirstContentfulPaint() override;

  // Wait for a first contentful paint to be seen by our PrerenderManager.
  void Wait();

 private:
  FirstContentfulPaintManagerWaiter();

  std::unique_ptr<base::RunLoop> waiter_;
  bool saw_fcp_;

  DISALLOW_COPY_AND_ASSIGN(FirstContentfulPaintManagerWaiter);
};

// PrerenderContentsFactory that uses TestPrerenderContents.
class TestPrerenderContentsFactory : public PrerenderContents::Factory {
 public:
  TestPrerenderContentsFactory();

  ~TestPrerenderContentsFactory() override;

  std::unique_ptr<TestPrerender> ExpectPrerenderContents(
      FinalStatus final_status);

  void IgnorePrerenderContents();

  PrerenderContents* CreatePrerenderContents(
      PrerenderManager* prerender_manager,
      Profile* profile,
      const GURL& url,
      const content::Referrer& referrer,
      const base::Optional<url::Origin>& initiator_origin,
      Origin origin) override;

 private:
  struct ExpectedContents {
    ExpectedContents();
    ExpectedContents(const ExpectedContents& other);
    ExpectedContents(FinalStatus final_status,
                     const base::WeakPtr<TestPrerender>& handle);
    explicit ExpectedContents(bool ignore);
    ~ExpectedContents();

    FinalStatus final_status = FINAL_STATUS_UNKNOWN;
    bool ignore = false;
    base::WeakPtr<TestPrerender> handle = nullptr;
  };

  base::circular_deque<ExpectedContents> expected_contents_queue_;

  DISALLOW_COPY_AND_ASSIGN(TestPrerenderContentsFactory);
};

class PrerenderInProcessBrowserTest : virtual public InProcessBrowserTest {
 public:
  PrerenderInProcessBrowserTest();

  ~PrerenderInProcessBrowserTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override;
  void TearDownInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;
  content::SessionStorageNamespace* GetSessionStorageNamespace() const;

  // Many of the file and server manipulation commands are fussy about paths
  // being relative or absolute. This makes path absolute if it is not
  // already. The path must not be empty.
  std::string MakeAbsolute(const std::string& path);

  bool UrlIsInPrerenderManager(const std::string& html_file) const;
  bool UrlIsInPrerenderManager(const GURL& url) const;

  // Convenience function to get the currently active WebContents in
  // current_browser().
  content::WebContents* GetActiveWebContents() const;

  PrerenderManager* GetPrerenderManager() const;

  TestPrerenderContents* GetPrerenderContentsFor(const GURL& url) const;

  // Set up an HTTPS server.
  void UseHttpsSrcServer();

  // Returns the currently active server. See |UseHttpsSrcServer|.
  net::EmbeddedTestServer* src_server();

  safe_browsing::TestSafeBrowsingServiceFactory* safe_browsing_factory() const {
    return safe_browsing_factory_.get();
  }

  test_utils::FakeSafeBrowsingDatabaseManager*
  GetFakeSafeBrowsingDatabaseManager();

  TestPrerenderContentsFactory* prerender_contents_factory() const {
    return prerender_contents_factory_;
  }

  void set_autostart_test_server(bool value) { autostart_test_server_ = value; }

  void set_browser(Browser* browser) { explicitly_set_browser_ = browser; }

  Browser* current_browser() const {
    return explicitly_set_browser_ ? explicitly_set_browser_ : browser();
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

  // Returns a string for pattern-matching TaskManager tab entries.
  base::string16 MatchTaskManagerTab(const char* page_title);

  // Returns a string for pattern-matching TaskManager prerender entries.
  base::string16 MatchTaskManagerPrerender(const char* page_title);

  // Returns a GURL for an EmbeddedTestServer that will serves the file
  // |url_file| with |replacement_text| replacing |replacement_variable|.
  GURL GetURLWithReplacement(const std::string& url_file,
                             const std::string& replacement_variable,
                             const std::string& replacement_text);

 protected:
  // For each FinalStatus in |expected_final_status_queue| creates a prerender
  // that is going to verify the correctness of its FinalStatus upon
  // destruction. Waits for creation of the first PrerenderContents.
  std::vector<std::unique_ptr<TestPrerender>> NavigateWithPrerenders(
      const GURL& loader_url,
      const std::vector<FinalStatus>& expected_final_status_queue);

  // Creates the URL that instructs the test server to substitute the text
  // |replacement_variable| in the contents of the file pointed to by
  // |loader_path| with |url_to_prerender|. Also appends the |loader_query| to
  // the URL.
  GURL ServeLoaderURL(const std::string& loader_path,
                      const std::string& replacement_variable,
                      const GURL& url_to_prerender,
                      const std::string& loader_query);

  uint32_t GetRequestCount(const GURL& url);
  void WaitForRequestCount(const GURL& url, uint32_t expected_count);

 private:
  void MonitorResourceRequest(const net::test_server::HttpRequest& request);
  std::unique_ptr<ExternalProtocolHandler::Delegate>
      external_protocol_handler_delegate_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;
  TestPrerenderContentsFactory* prerender_contents_factory_;
  Browser* explicitly_set_browser_;
  bool autostart_test_server_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<net::EmbeddedTestServer> https_src_server_;

  // The following are guarded by |lock_| as they're used on multiple threads.
  std::map<GURL, uint32_t> requests_;
  GURL waiting_url_;
  uint32_t waiting_count_ = 0;
  base::Closure waiting_closure_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderInProcessBrowserTest);
};

// RAII class to save and restore the prerender mode.
class RestorePrerenderMode {
 public:
  RestorePrerenderMode() : prev_mode_(PrerenderManager::GetMode()) {}

  ~RestorePrerenderMode() {
    PrerenderManager::SetMode(prev_mode_);
  }

 private:
  PrerenderManager::PrerenderManagerMode prev_mode_;

  DISALLOW_COPY_AND_ASSIGN(RestorePrerenderMode);
};

}  // namespace test_utils

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_TEST_UTILS_H_

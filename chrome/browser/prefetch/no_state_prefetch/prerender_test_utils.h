// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_NO_STATE_PREFETCH_PRERENDER_TEST_UTILS_H_
#define CHROME_BROWSER_PREFETCH_NO_STATE_PREFETCH_PRERENDER_TEST_UTILS_H_

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
#include "chrome/browser/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents_delegate.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/safe_browsing/core/db/fake_database_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "url/gurl.h"

namespace prerender {

namespace test_utils {

class TestNoStatePrefetchContents : public NoStatePrefetchContents,
                                    public content::RenderWidgetHostObserver {
 public:
  TestNoStatePrefetchContents(
      NoStatePrefetchManager* no_state_prefetch_manager,
      content::BrowserContext* browser_context,
      const GURL& url,
      const content::Referrer& referrer,
      const base::Optional<url::Origin>& initiator_origin,
      Origin origin,
      FinalStatus expected_final_status,
      bool ignore_final_status);

  ~TestNoStatePrefetchContents() override;

  bool CheckURL(const GURL& url) override;

  // For tests that open the no-state prefetcher in a new background tab, the
  // RenderView will not have been made visible when the NoStatePrefetchContents
  // is destroyed even though it is used.
  void set_should_be_shown(bool value) { should_be_shown_ = value; }

  // For tests which do not know whether the no-state prefetcher will be used.
  void set_skip_final_checks(bool value) { skip_final_checks_ = value; }

  FinalStatus expected_final_status() const { return expected_final_status_; }

 private:
  // WebContentsObserver overrides.
  void RenderFrameHostChanged(
      content::RenderFrameHost* old_frame_host,
      content::RenderFrameHost* new_frame_host) override;

  // RenderWidgetHostObserver overrides.
  void RenderWidgetHostVisibilityChanged(content::RenderWidgetHost* widget_host,
                                         bool became_visible) override;
  void RenderWidgetHostDestroyed(
      content::RenderWidgetHost* widget_host) override;

  FinalStatus expected_final_status_;

  ScopedObserver<content::RenderWidgetHost, content::RenderWidgetHostObserver>
      observer_;
  // The main frame created for the prerender, if any.
  content::RenderFrameHost* new_main_frame_ = nullptr;
  // Set to true when the prerendering RenderWidget is shown, after having been
  // hidden.
  bool was_shown_ = false;
  // Expected final value of was_shown_.  Defaults to true for
  // FINAL_STATUS_USED, and false otherwise.
  bool should_be_shown_;
  // If true, |expected_final_status_| and other shutdown checks are skipped.
  bool skip_final_checks_;

  DISALLOW_COPY_AND_ASSIGN(TestNoStatePrefetchContents);
};

// A handle to a TestNoStatePrefetchContents whose lifetime is under the
// caller's control. A NoStatePrefetchContents may be destroyed at any point.
// This allows tracking the FinalStatus.
class TestPrerender : public NoStatePrefetchContents::Observer,
                      public base::SupportsWeakPtr<TestPrerender> {
 public:
  TestPrerender();
  ~TestPrerender() override;

  TestNoStatePrefetchContents* contents() const { return contents_; }
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

  void OnPrefetchContentsCreated(TestNoStatePrefetchContents* contents);

  // NoStatePrefetchContents::Observer implementation:
  void OnPrefetchStart(NoStatePrefetchContents* contents) override;

  void OnPrefetchStopLoading(NoStatePrefetchContents* contents) override;

  void OnPrefetchStop(NoStatePrefetchContents* contents) override;

 private:
  TestNoStatePrefetchContents* contents_;
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

// Blocks until a TestNoStatePrefetchContents has been destroyed with the given
// final status. Should be created with a TestNoStatePrefetchContents, and then
// WaitForDestroy should be called and its return value checked.
class DestructionWaiter {
 public:
  // Does not own the no_state_prefetch_contents, which must outlive any call to
  // WaitForDestroy().
  DestructionWaiter(TestNoStatePrefetchContents* no_state_prefetch_contents,
                    FinalStatus expected_final_status);

  ~DestructionWaiter();

  // Returns true if the TestNoStatePrefetchContents was destroyed with the
  // correct final status, or false otherwise. Note this also may hang if the
  // contents is never destroyed (which will presumably cause the test to time
  // out).
  bool WaitForDestroy();

 private:
  class DestructionMarker : public NoStatePrefetchContents::Observer {
   public:
    // Does not own the waiter which must outlive the
    // TestNoStatePrefetchContents.
    explicit DestructionMarker(DestructionWaiter* waiter);

    ~DestructionMarker() override;

    void OnPrefetchStop(NoStatePrefetchContents* contents) override;

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

// Wait until a NoStatePrefetchManager has seen a first contentful paint.
class FirstContentfulPaintManagerWaiter
    : public NoStatePrefetchManagerObserver {
 public:
  // Create and return a pointer to a |FirstContentfulPaintManagerWaiter|. The
  // instance is owned by the |NoStatePrefetchManager|.
  static FirstContentfulPaintManagerWaiter* Create(
      NoStatePrefetchManager* manager);

  ~FirstContentfulPaintManagerWaiter() override;

  void OnFirstContentfulPaint() override;

  // Wait for a first contentful paint to be seen by our NoStatePrefetchManager.
  void Wait();

 private:
  FirstContentfulPaintManagerWaiter();

  std::unique_ptr<base::RunLoop> waiter_;
  bool saw_fcp_;

  DISALLOW_COPY_AND_ASSIGN(FirstContentfulPaintManagerWaiter);
};

// NoStatePrefetchContentsFactory that uses TestNoStatePrefetchContents.
class TestNoStatePrefetchContentsFactory
    : public NoStatePrefetchContents::Factory {
 public:
  TestNoStatePrefetchContentsFactory();

  ~TestNoStatePrefetchContentsFactory() override;

  std::unique_ptr<TestPrerender> ExpectNoStatePrefetchContents(
      FinalStatus final_status);

  void IgnoreNoStatePrefetchContents();

  NoStatePrefetchContents* CreateNoStatePrefetchContents(
      std::unique_ptr<NoStatePrefetchContentsDelegate> delegate,
      NoStatePrefetchManager* no_state_prefetch_manager,
      content::BrowserContext* browser_context,
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

  DISALLOW_COPY_AND_ASSIGN(TestNoStatePrefetchContentsFactory);
};

class PrerenderInProcessBrowserTest : virtual public InProcessBrowserTest {
 public:
  PrerenderInProcessBrowserTest();

  ~PrerenderInProcessBrowserTest() override;

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override;
  void TearDownInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;
  content::SessionStorageNamespace* GetSessionStorageNamespace() const;

  // Many of the file and server manipulation commands are fussy about paths
  // being relative or absolute. This makes path absolute if it is not
  // already. The path must not be empty.
  std::string MakeAbsolute(const std::string& path);

  bool UrlIsInNoStatePrefetchManager(const std::string& html_file) const;
  bool UrlIsInNoStatePrefetchManager(const GURL& url) const;

  // Convenience function to get the currently active WebContents in
  // current_browser().
  content::WebContents* GetActiveWebContents() const;

  NoStatePrefetchManager* GetNoStatePrefetchManager() const;

  TestNoStatePrefetchContents* GetNoStatePrefetchContentsFor(
      const GURL& url) const;

  // Set up an HTTPS server.
  void UseHttpsSrcServer();

  // Returns the currently active server. See |UseHttpsSrcServer|.
  net::EmbeddedTestServer* src_server();

  safe_browsing::TestSafeBrowsingServiceFactory* safe_browsing_factory() const {
    return safe_browsing_factory_.get();
  }

  safe_browsing::FakeSafeBrowsingDatabaseManager*
  GetFakeSafeBrowsingDatabaseManager();

  TestNoStatePrefetchContentsFactory* no_state_prefetch_contents_factory()
      const {
    return no_state_prefetch_contents_factory_;
  }

  void set_autostart_test_server(bool value) { autostart_test_server_ = value; }

  void set_browser(Browser* browser) { explicitly_set_browser_ = browser; }

  Browser* current_browser() const {
    return explicitly_set_browser_ ? explicitly_set_browser_ : browser();
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

  // Returns a string for pattern-matching TaskManager tab entries.
  std::u16string MatchTaskManagerTab(const char* page_title);

  // Returns a string for pattern-matching TaskManager prerender entries.
  std::u16string MatchTaskManagerPrerender(const char* page_title);

  // Returns a GURL for an EmbeddedTestServer that will serves the file
  // |url_file| with |replacement_text| replacing |replacement_variable|.
  GURL GetURLWithReplacement(const std::string& url_file,
                             const std::string& replacement_variable,
                             const std::string& replacement_text);

 protected:
  // For each FinalStatus in |expected_final_status_queue| creates a prerender
  // that is going to verify the correctness of its FinalStatus upon
  // destruction. Waits for creation of the first NoStatePrefetchContents.
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
                      const std::string& loader_query,
                      const std::string& hostname_alternative = std::string());

  // A variation of the above that allows for overriding the hostname.
  GURL ServeLoaderURLWithHostname(const std::string& loader_path,
                                  const std::string& replacement_variable,
                                  const GURL& url_to_prerender,
                                  const std::string& loader_query,
                                  const std::string& hostname);

  uint32_t GetRequestCount(const GURL& url);
  void WaitForRequestCount(const GURL& url, uint32_t expected_count);

 private:
  void MonitorResourceRequest(const net::test_server::HttpRequest& request);
  std::unique_ptr<ExternalProtocolHandler::Delegate>
      external_protocol_handler_delegate_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;
  TestNoStatePrefetchContentsFactory* no_state_prefetch_contents_factory_;
  Browser* explicitly_set_browser_;
  bool autostart_test_server_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<net::EmbeddedTestServer> https_src_server_;

  // The following are guarded by |lock_| as they're used on multiple threads.
  std::map<GURL, uint32_t> requests_;
  GURL waiting_url_;
  uint32_t waiting_count_ = 0;
  base::OnceClosure waiting_closure_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderInProcessBrowserTest);
};

}  // namespace test_utils

}  // namespace prerender

#endif  // CHROME_BROWSER_PREFETCH_NO_STATE_PREFETCH_PRERENDER_TEST_UTILS_H_

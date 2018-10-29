// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#include "chrome/browser/page_load_metrics/observers/prerender_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_link_manager.h"
#include "chrome/browser/prerender/prerender_link_manager_factory.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/prerender/prerender_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/speech/tts_platform.h"
#include "chrome/browser/task_manager/mock_web_contents_task_manager.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tags_manager.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/page_load_metrics/test/page_load_metrics_test_util.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/nacl/common/buildflags.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/safe_browsing/db/database_manager.h"
#include "components/safe_browsing/db/util.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/ppapi_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/escape.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_job.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using chrome_browser_net::NetworkPredictionOptions;
using content::BrowserThread;
using content::DevToolsAgentHost;
using content::NavigationController;
using content::OpenURLParams;
using content::Referrer;
using content::RenderFrameHost;
using content::RenderViewHost;
using content::RenderWidgetHost;
using content::TestNavigationObserver;
using content::WebContents;
using content::WebContentsObserver;
using net::NetworkChangeNotifier;
using prerender::test_utils::RequestCounter;
using prerender::test_utils::CreateCountingInterceptorOnIO;
using prerender::test_utils::CreateMockInterceptorOnIO;
using prerender::test_utils::TestPrerender;
using prerender::test_utils::TestPrerenderContents;
using task_manager::browsertest_util::WaitForTaskManagerRows;

// crbug.com/708158
#if !defined(OS_MACOSX) || !defined(ADDRESS_SANITIZER)

// Prerender tests work as follows:
//
// A page with a prefetch link to the test page is loaded.  Once prerendered,
// its Javascript function DidPrerenderPass() is called, which returns true if
// the page behaves as expected when prerendered.
//
// The prerendered page is then displayed on a tab.  The Javascript function
// DidDisplayPass() is called, and returns true if the page behaved as it
// should while being displayed.

namespace prerender {

namespace {

const char kPrefetchJpeg[] = "/prerender/image.jpeg";

class FaviconUpdateWatcher : public favicon::FaviconDriverObserver {
 public:
  explicit FaviconUpdateWatcher(content::WebContents* web_contents)
      : seen_(false), running_(false), scoped_observer_(this) {
    scoped_observer_.Add(
        favicon::ContentFaviconDriver::FromWebContents(web_contents));
  }

  void Wait() {
    if (seen_)
      return;

    running_ = true;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
  }

 private:
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override {
    seen_ = true;
    if (!running_)
      return;

    message_loop_runner_->Quit();
    running_ = false;
  }

  bool seen_;
  bool running_;
  ScopedObserver<favicon::FaviconDriver, FaviconUpdateWatcher> scoped_observer_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(FaviconUpdateWatcher);
};

class MockNetworkChangeNotifierWIFI : public NetworkChangeNotifier {
 public:
  ConnectionType GetCurrentConnectionType() const override {
    return NetworkChangeNotifier::CONNECTION_WIFI;
  }
};

class MockNetworkChangeNotifier4G : public NetworkChangeNotifier {
 public:
  ConnectionType GetCurrentConnectionType() const override {
    return NetworkChangeNotifier::CONNECTION_4G;
  }
};

// Constants used in the test HTML files.
const char* kReadyTitle = "READY";
const char* kPassTitle = "PASS";

std::string CreateClientRedirect(const std::string& dest_url) {
  const char* const kClientRedirectBase = "/client-redirect?";
  return kClientRedirectBase + net::EscapeQueryParamValue(dest_url, false);
}

std::string CreateServerRedirect(const std::string& dest_url) {
  const char* const kServerRedirectBase = "/server-redirect?";
  return kServerRedirectBase + net::EscapeQueryParamValue(dest_url, false);
}

// Clears the specified data using BrowsingDataRemover.
void ClearBrowsingData(Browser* browser, int remove_mask) {
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(browser->profile());
  content::BrowsingDataRemoverCompletionObserver observer(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(), remove_mask,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, &observer);
  observer.BlockUntilCompletion();
  // BrowsingDataRemover deletes itself.
}

// Returns true if the prerender is expected to abort on its own, before
// attempting to swap it.
bool ShouldAbortPrerenderBeforeSwap(FinalStatus status) {
  switch (status) {
    case FINAL_STATUS_USED:
    case FINAL_STATUS_WINDOW_OPENER:
    case FINAL_STATUS_APP_TERMINATING:
    case FINAL_STATUS_PROFILE_DESTROYED:
    case FINAL_STATUS_CACHE_OR_HISTORY_CLEARED:
    // We'll crash the renderer after it's loaded.
    case FINAL_STATUS_RENDERER_CRASHED:
    case FINAL_STATUS_CANCELLED:
    case FINAL_STATUS_DEVTOOLS_ATTACHED:
    case FINAL_STATUS_PAGE_BEING_CAPTURED:
    case FINAL_STATUS_NAVIGATION_UNCOMMITTED:
    case FINAL_STATUS_NON_EMPTY_BROWSING_INSTANCE:
      return false;
    default:
      return true;
  }
}

// Convenience function to wait for a title. Handles the case when the
// WebContents already has the expected title.
void WaitForASCIITitle(WebContents* web_contents,
                       const char* expected_title_ascii) {
  base::string16 expected_title = base::ASCIIToUTF16(expected_title_ascii);
  if (web_contents->GetTitle() == expected_title)
    return;
  content::TitleWatcher title_watcher(web_contents, expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Waits for the destruction of a RenderProcessHost's IPC channel.
// Used to make sure the PrerenderLinkManager's OnChannelClosed function has
// been called, before checking its state.
class ChannelDestructionWatcher {
 public:
  ChannelDestructionWatcher() : channel_destroyed_(false) {
  }

  ~ChannelDestructionWatcher() {
  }

  void WatchChannel(content::RenderProcessHost* host) {
    host->AddFilter(new DestructionMessageFilter(this));
  }

  void WaitForChannelClose() {
    run_loop_.Run();
    EXPECT_TRUE(channel_destroyed_);
  }

 private:
  // When destroyed, calls ChannelDestructionWatcher::OnChannelDestroyed.
  // Ignores all messages.
  class DestructionMessageFilter : public content::BrowserMessageFilter {
   public:
     explicit DestructionMessageFilter(ChannelDestructionWatcher* watcher)
         : BrowserMessageFilter(0),
           watcher_(watcher) {
    }

   private:
    ~DestructionMessageFilter() override {
      base::PostTaskWithTraits(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(&ChannelDestructionWatcher::OnChannelDestroyed,
                         base::Unretained(watcher_)));
    }

    bool OnMessageReceived(const IPC::Message& message) override {
      return false;
    }

    ChannelDestructionWatcher* watcher_;

    DISALLOW_COPY_AND_ASSIGN(DestructionMessageFilter);
  };

  void OnChannelDestroyed() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    EXPECT_FALSE(channel_destroyed_);
    channel_destroyed_ = true;
    run_loop_.Quit();
  }

  bool channel_destroyed_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ChannelDestructionWatcher);
};

// A navigation observer to wait until WebContents is destroyed.
class WebContentsDestructionObserver : public WebContentsObserver {
 public:
  explicit WebContentsDestructionObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  // Waits for destruction of the observed WebContents.
  void Wait() {
    loop_.Run();
  }

  // WebContentsObserver implementation:
  void WebContentsDestroyed() override {
    loop_.Quit();
  }

 private:
  base::RunLoop loop_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsDestructionObserver);
};

// A navigation observer to wait on either a new load or a swap of a
// WebContents. On swap, if the new WebContents is still loading, wait for that
// load to complete as well. Note that the load must begin after the observer is
// attached.
class NavigationOrSwapObserver : public WebContentsObserver,
                                 public TabStripModelObserver {
 public:
  // Waits for either a new load or a swap of |tab_strip_model|'s active
  // WebContents.
  NavigationOrSwapObserver(TabStripModel* tab_strip_model,
                           WebContents* web_contents)
      : WebContentsObserver(web_contents),
        tab_strip_model_(tab_strip_model),
        did_start_loading_(false),
        number_of_loads_(1) {
    CHECK_NE(TabStripModel::kNoTab,
             tab_strip_model->GetIndexOfWebContents(web_contents));
    tab_strip_model_->AddObserver(this);
  }

  // Waits for either |number_of_loads| loads or a swap of |tab_strip_model|'s
  // active WebContents.
  NavigationOrSwapObserver(TabStripModel* tab_strip_model,
                           WebContents* web_contents,
                           int number_of_loads)
      : WebContentsObserver(web_contents),
        tab_strip_model_(tab_strip_model),
        did_start_loading_(false),
        number_of_loads_(number_of_loads) {
    CHECK_NE(TabStripModel::kNoTab,
             tab_strip_model->GetIndexOfWebContents(web_contents));
    tab_strip_model_->AddObserver(this);
  }

  ~NavigationOrSwapObserver() override {
    tab_strip_model_->RemoveObserver(this);
  }

  void set_did_start_loading() {
    did_start_loading_ = true;
  }

  void Wait() {
    loop_.Run();
  }

  // WebContentsObserver implementation:
  void DidStartLoading() override { did_start_loading_ = true; }
  void DidStopLoading() override {
    if (!did_start_loading_)
      return;
    number_of_loads_--;
    if (number_of_loads_ == 0)
      loop_.Quit();
  }

  // TabStripModelObserver implementation:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kReplaced)
      return;

    for (const auto& delta : change.deltas()) {
      if (delta.replace.old_contents != web_contents())
        continue;

      // Switch to observing the new WebContents.
      Observe(delta.replace.new_contents);
      if (delta.replace.new_contents->IsLoading()) {
        // If the new WebContents is still loading, wait for it to complete.
        // Only one load post-swap is supported.
        did_start_loading_ = true;
        number_of_loads_ = 1;
      } else {
        loop_.Quit();
      }
    }
  }

 private:
  TabStripModel* tab_strip_model_;
  bool did_start_loading_;
  int number_of_loads_;
  base::RunLoop loop_;
};

// Waits for a new tab to open and a navigation or swap in it.
class NewTabNavigationOrSwapObserver {
 public:
  NewTabNavigationOrSwapObserver()
      : new_tab_observer_(
            chrome::NOTIFICATION_TAB_ADDED,
            base::Bind(&NewTabNavigationOrSwapObserver::OnTabAdded,
                       base::Unretained(this))) {
    // Watch for NOTIFICATION_TAB_ADDED. Add a callback so that the
    // NavigationOrSwapObserver can be attached synchronously and no events are
    // missed.
  }

  void Wait() {
    new_tab_observer_.Wait();
    swap_observer_->Wait();
  }

  bool OnTabAdded(const content::NotificationSource& source,
                  const content::NotificationDetails& details) {
    if (swap_observer_)
      return true;
    WebContents* new_tab = content::Details<WebContents>(details).ptr();
    TabStripModel* tab_strip_model =
        chrome::FindBrowserWithWebContents(new_tab)->tab_strip_model();
    swap_observer_.reset(new NavigationOrSwapObserver(tab_strip_model,
                                                      new_tab));
    swap_observer_->set_did_start_loading();
    return true;
  }

 private:
  content::WindowedNotificationObserver new_tab_observer_;
  std::unique_ptr<NavigationOrSwapObserver> swap_observer_;
};

class FakeDevToolsClient : public content::DevToolsAgentHostClient {
 public:
  FakeDevToolsClient() {}
  ~FakeDevToolsClient() override {}
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override {}
  void AgentHostClosed(DevToolsAgentHost* agent_host) override {}
};

// A ContentBrowserClient that cancels all prerenderers on OpenURL.
class TestContentBrowserClient : public ChromeContentBrowserClient {
 public:
  TestContentBrowserClient() {}
  ~TestContentBrowserClient() override {}

  // ChromeContentBrowserClient:
  bool ShouldAllowOpenURL(content::SiteInstance* site_instance,
                          const GURL& url) override {
    PrerenderManagerFactory::GetForBrowserContext(
        site_instance->GetBrowserContext())
        ->CancelAllPrerenders();
    return ChromeContentBrowserClient::ShouldAllowOpenURL(site_instance,
                                                                  url);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestContentBrowserClient);
};

base::FilePath GetTestPath(const std::string& file_name) {
  return ui_test_utils::GetTestFilePath(
      base::FilePath(FILE_PATH_LITERAL("prerender")),
      base::FilePath().AppendASCII(file_name));
}

page_load_metrics::PageLoadExtraInfo GenericPageLoadExtraInfo(
    const GURL& dest_url) {
  return page_load_metrics::PageLoadExtraInfo::CreateForTesting(
      dest_url, false /* started_in_foreground */);
}

}  // namespace

class PrerenderBrowserTest : public test_utils::PrerenderInProcessBrowserTest {
 public:
  PrerenderBrowserTest()
      : call_javascript_(true),
        check_load_events_(true),
        loader_path_("/prerender/prerender_loader.html") {}

  ~PrerenderBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);

    test_utils::PrerenderInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  std::unique_ptr<TestPrerender> PrerenderTestURL(
      const std::string& html_file,
      FinalStatus expected_final_status,
      int expected_number_of_loads) {
    GURL url = src_server()->GetURL(MakeAbsolute(html_file));
    return PrerenderTestURL(url, expected_final_status,
                            expected_number_of_loads);
  }

  std::unique_ptr<TestPrerender> PrerenderTestURL(
      const GURL& url,
      FinalStatus expected_final_status,
      int expected_number_of_loads) {
    std::vector<FinalStatus> expected_final_status_queue(1,
                                                         expected_final_status);
    auto prerenders = PrerenderTestURLImpl(url, expected_final_status_queue,
                                           expected_number_of_loads);
    CHECK_EQ(1u, prerenders.size());
    return std::move(prerenders[0]);
  }

  std::vector<std::unique_ptr<TestPrerender>> PrerenderTestURL(
      const std::string& html_file,
      const std::vector<FinalStatus>& expected_final_status_queue,
      int expected_number_of_loads) {
    GURL url = src_server()->GetURL(MakeAbsolute(html_file));
    return PrerenderTestURLImpl(url, expected_final_status_queue,
                                expected_number_of_loads);
  }

  void SetUpOnMainThread() override {
    test_utils::PrerenderInProcessBrowserTest::SetUpOnMainThread();
    prerender::PrerenderManager::SetMode(
        prerender::PrerenderManager::PRERENDER_MODE_ENABLED);
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    // This one test fails with the host resolver redirecting all hosts.
    if (std::string(test_info->name()) != "PrerenderServerRedirectInIframe")
      host_resolver()->AddRule("*", "127.0.0.1");
  }

  void TearDownOnMainThread() override {
    test_utils::PrerenderInProcessBrowserTest::TearDownOnMainThread();
    interceptor_.reset();
  }

  void NavigateToDestURL() const {
    NavigateToDestURLWithDisposition(WindowOpenDisposition::CURRENT_TAB, true);
  }

  // Opens the url in a new tab, with no opener.
  void NavigateToDestURLWithDisposition(
      WindowOpenDisposition disposition,
      bool expect_swap_to_succeed) const {
    NavigateToURLWithParams(
        content::OpenURLParams(dest_url_, Referrer(), disposition,
                               ui::PAGE_TRANSITION_TYPED, false),
        expect_swap_to_succeed);
  }

  void NavigateToURL(const std::string& dest_html_file) const {
    NavigateToURLWithDisposition(dest_html_file,
                                 WindowOpenDisposition::CURRENT_TAB, true);
  }

  void NavigateToURLWithDisposition(const std::string& dest_html_file,
                                    WindowOpenDisposition disposition,
                                    bool expect_swap_to_succeed) const {
    GURL dest_url = embedded_test_server()->GetURL(dest_html_file);
    NavigateToURLWithDisposition(dest_url, disposition, expect_swap_to_succeed);
  }

  void NavigateToURLWithDisposition(const GURL& dest_url,
                                    WindowOpenDisposition disposition,
                                    bool expect_swap_to_succeed) const {
    NavigateToURLWithParams(
        content::OpenURLParams(dest_url, Referrer(), disposition,
                               ui::PAGE_TRANSITION_TYPED, false),
        expect_swap_to_succeed);
  }

  void NavigateToURLWithParams(const content::OpenURLParams& params,
                               bool expect_swap_to_succeed) const {
    NavigateToURLImpl(params, expect_swap_to_succeed);
  }

  void OpenDestURLViaClick() const {
    OpenURLViaClick(dest_url_);
  }

  void OpenURLViaClick(const GURL& url) const {
    OpenURLWithJSImpl("Click", url, GURL(), false);
  }

  void OpenDestURLViaClickTarget() const {
    OpenURLWithJSImpl("ClickTarget", dest_url_, GURL(), true);
  }

  void OpenDestURLViaClickPing(const GURL& ping_url) const {
    OpenURLWithJSImpl("ClickPing", dest_url_, ping_url, false);
  }

  void OpenDestURLViaClickNewWindow() const {
    OpenURLWithJSImpl("ShiftClick", dest_url_, GURL(), true);
  }

  void OpenDestURLViaClickNewForegroundTab() const {
#if defined(OS_MACOSX)
    OpenURLWithJSImpl("MetaShiftClick", dest_url_, GURL(), true);
#else
    OpenURLWithJSImpl("CtrlShiftClick", dest_url_, GURL(), true);
#endif
  }

  void OpenDestURLViaWindowOpen() const {
    OpenURLViaWindowOpen(dest_url_);
  }

  void OpenURLViaWindowOpen(const GURL& url) const {
    OpenURLWithJSImpl("WindowOpen", url, GURL(), true);
  }

  void RemoveLinkElement(int i) const {
    GetActiveWebContents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(base::StringPrintf("RemoveLinkElement(%d)", i)));
  }

  void ClickToNextPageAfterPrerender() {
    TestNavigationObserver nav_observer(GetActiveWebContents());
    RenderFrameHost* render_frame_host = GetActiveWebContents()->GetMainFrame();
    render_frame_host->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("ClickOpenLink()"));
    nav_observer.Wait();
  }

  void NavigateToNextPageAfterPrerender() const {
    ui_test_utils::NavigateToURL(
        current_browser(),
        embedded_test_server()->GetURL("/prerender/prerender_page.html"));
  }

  // Called after the prerendered page has been navigated to and then away from.
  // Navigates back through the history to the prerendered page.
  void GoBackToPrerender() {
    TestNavigationObserver back_nav_observer(GetActiveWebContents());
    chrome::GoBack(current_browser(), WindowOpenDisposition::CURRENT_TAB);
    back_nav_observer.Wait();
    bool original_prerender_page = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        GetActiveWebContents(),
        "window.domAutomationController.send(IsOriginalPrerenderPage())",
        &original_prerender_page));
    EXPECT_TRUE(original_prerender_page);
  }

  void DisableJavascriptCalls() {
    call_javascript_ = false;
  }

  void EnableJavascriptCalls() { call_javascript_ = true; }

  void DisableLoadEventCheck() {
    check_load_events_ = false;
  }

  const PrerenderLinkManager* GetPrerenderLinkManager() const {
    PrerenderLinkManager* prerender_link_manager =
        PrerenderLinkManagerFactory::GetForProfile(
            current_browser()->profile());
    return prerender_link_manager;
  }

  int GetPrerenderEventCount(int index, const std::string& type) const {
    int event_count;
    std::string expression = base::StringPrintf(
        "window.domAutomationController.send("
        "    GetPrerenderEventCount(%d, '%s'))", index, type.c_str());

    CHECK(content::ExecuteScriptAndExtractInt(
        GetActiveWebContents(), expression, &event_count));
    return event_count;
  }

  bool DidReceivePrerenderStartEventForLinkNumber(int index) const {
    return GetPrerenderEventCount(index, "webkitprerenderstart") > 0;
  }

  int GetPrerenderLoadEventCountForLinkNumber(int index) const {
    return GetPrerenderEventCount(index, "webkitprerenderload");
  }

  int GetPrerenderDomContentLoadedEventCountForLinkNumber(int index) const {
    return GetPrerenderEventCount(index, "webkitprerenderdomcontentloaded");
  }

  bool DidReceivePrerenderStopEventForLinkNumber(int index) const {
    return GetPrerenderEventCount(index, "webkitprerenderstop") > 0;
  }

  void WaitForPrerenderEventCount(int index,
                                  const std::string& type,
                                  int count) const {
    int dummy;
    std::string expression = base::StringPrintf(
        "WaitForPrerenderEventCount(%d, '%s', %d,"
        "    window.domAutomationController.send.bind("
        "        window.domAutomationController, 0))",
        index, type.c_str(), count);

    CHECK(content::ExecuteScriptAndExtractInt(
        GetActiveWebContents(), expression, &dummy));
    CHECK_EQ(0, dummy);
  }

  bool HadPrerenderEventErrors() const {
    bool had_prerender_event_errors;
    CHECK(content::ExecuteScriptAndExtractBool(
        GetActiveWebContents(),
        "window.domAutomationController.send(Boolean("
        "    hadPrerenderEventErrors))",
        &had_prerender_event_errors));
    return had_prerender_event_errors;
  }

  // Asserting on this can result in flaky tests.  PrerenderHandles are
  // removed from the PrerenderLinkManager when the prerender is canceled from
  // the browser, when the prerenders are cancelled from the renderer process,
  // or the channel for the renderer process is closed on the IO thread.  In the
  // last case, the code must be careful to wait for the channel to close, as it
  // is done asynchronously after swapping out the old process.  See
  // ChannelDestructionWatcher.
  bool IsEmptyPrerenderLinkManager() const {
    return GetPrerenderLinkManager()->IsEmpty();
  }

  size_t GetLinkPrerenderCount() const {
    return GetPrerenderLinkManager()->prerenders_.size();
  }

  size_t GetRunningLinkPrerenderCount() const {
    return GetPrerenderLinkManager()->CountRunningPrerenders();
  }

  // Returns length of |prerender_manager_|'s history, or SIZE_MAX on failure.
  size_t GetHistoryLength() const {
    std::unique_ptr<base::DictionaryValue> prerender_dict =
        GetPrerenderManager()->CopyAsValue();
    if (!prerender_dict)
      return std::numeric_limits<size_t>::max();
    base::ListValue* history_list;
    if (!prerender_dict->GetList("history", &history_list))
      return std::numeric_limits<size_t>::max();
    return history_list->GetSize();
  }

  void SetLoaderHostOverride(const std::string& host) {
    loader_host_override_ = host;
  }

  void set_loader_path(const std::string& path) {
    loader_path_ = path;
  }

  void set_loader_query(const std::string& query) {
    loader_query_ = query;
  }

  GURL GetCrossDomainTestUrl(const std::string& path) {
    static const std::string secondary_domain = "www.foo.com";
    std::string url_str(base::StringPrintf(
        "http://%s:%d/%s", secondary_domain.c_str(),
        embedded_test_server()->host_port_pair().port(), path.c_str()));
    return GURL(url_str);
  }

  const GURL& dest_url() const {
    return dest_url_;
  }

  bool DidPrerenderPass(WebContents* web_contents) const {
    bool prerender_test_result = false;
    if (!content::ExecuteScriptAndExtractBool(
            web_contents,
            "window.domAutomationController.send(DidPrerenderPass())",
            &prerender_test_result))
      return false;
    return prerender_test_result;
  }

  bool DidDisplayPass(WebContents* web_contents) const {
    bool display_test_result = false;
    if (!content::ExecuteScriptAndExtractBool(
            web_contents,
            "window.domAutomationController.send(DidDisplayPass())",
            &display_test_result))
      return false;
    return display_test_result;
  }

  std::unique_ptr<TestPrerender> ExpectPrerender(
      FinalStatus expected_final_status) {
    return prerender_contents_factory()->ExpectPrerenderContents(
        expected_final_status);
  }

  void AddPrerender(const GURL& url, int index) {
    std::string javascript = base::StringPrintf(
        "AddPrerender('%s', %d)", url.spec().c_str(), index);
    RenderFrameHost* render_frame_host = GetActiveWebContents()->GetMainFrame();
    render_frame_host->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(javascript));
  }

  base::SimpleTestTickClock* OverridePrerenderManagerTimeTicks() {
    // The default zero time causes the prerender manager to do strange things.
    clock_.Advance(base::TimeDelta::FromSeconds(1));
    GetPrerenderManager()->SetTickClockForTesting(&clock_);
    return &clock_;
  }

  void SetMidLoadClockAdvance(base::SimpleTestTickClock* clock,
                              base::TimeDelta delta) {
    mid_load_clock_ = clock;
    mid_load_clock_tick_advance_ = delta;
  }

  void ClearMidLoadClock() {
    mid_load_clock_tick_advance_ = base::TimeDelta();
    mid_load_clock_ = nullptr;
  }

  // Makes |url| never respond on the first load, and then with the contents of
  // |file| afterwards. When the first load has been scheduled, runs
  // |callback_io| on the IO thread.
  void CreateHangingFirstRequestInterceptor(const GURL& url,
                                            const base::FilePath& file,
                                            base::Closure closure) {
    DCHECK(!interceptor_);
    interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
        base::BindLambdaForTesting(
            [=](content::URLLoaderInterceptor::RequestParams* params) {
              if (params->url_request.url == url) {
                static bool first = true;
                if (first) {
                  first = false;
                  // Need to leak the client pipe, or else the renderer will
                  // get a disconnect error and load the error page.
                  (void)params->client.PassInterface().PassHandle().release();
                  closure.Run();
                  return true;
                }
              }
              return false;
            }));
  }

 private:
  // TODO(davidben): Remove this altogether so the tests don't globally assume
  // only one prerender.
  TestPrerenderContents* GetPrerenderContents() const {
    return GetPrerenderContentsFor(dest_url_);
  }

  std::vector<std::unique_ptr<TestPrerender>> PrerenderTestURLImpl(
      const GURL& prerender_url,
      const std::vector<FinalStatus>& expected_final_status_queue,
      int expected_number_of_loads) {
    dest_url_ = prerender_url;

    GURL loader_url = ServeLoaderURL(loader_path_, "REPLACE_WITH_PRERENDER_URL",
                                     prerender_url, "&" + loader_query_);
    GURL::Replacements loader_replacements;
    if (!loader_host_override_.empty())
      loader_replacements.SetHostStr(loader_host_override_);
    loader_url = loader_url.ReplaceComponents(loader_replacements);

    std::vector<std::unique_ptr<TestPrerender>> prerenders =
        NavigateWithPrerenders(loader_url, expected_final_status_queue);
    prerenders[0]->WaitForLoads(expected_number_of_loads);

    if (!mid_load_clock_tick_advance_.is_zero()) {
      EXPECT_TRUE(mid_load_clock_);
      mid_load_clock_->Advance(mid_load_clock_tick_advance_);
    }

    FinalStatus expected_final_status = expected_final_status_queue.front();
    if (ShouldAbortPrerenderBeforeSwap(expected_final_status)) {
      // The prerender will abort on its own. Assert it does so correctly.
      prerenders[0]->WaitForStop();
      EXPECT_FALSE(prerenders[0]->contents());
      EXPECT_TRUE(DidReceivePrerenderStopEventForLinkNumber(0));
    } else {
      // Otherwise, check that it prerendered correctly.
      TestPrerenderContents* prerender_contents = prerenders[0]->contents();
      CHECK(prerender_contents);
      EXPECT_EQ(FINAL_STATUS_MAX, prerender_contents->final_status());
      EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));

      if (call_javascript_) {
        // Check if page behaves as expected while in prerendered state.
        EXPECT_TRUE(DidPrerenderPass(prerender_contents->prerender_contents()));
      }
    }

    // Test that the referring page received the right start and load events.
    EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(0));
    if (check_load_events_) {
      EXPECT_EQ(expected_number_of_loads, prerenders[0]->number_of_loads());
      EXPECT_EQ(expected_number_of_loads,
                GetPrerenderLoadEventCountForLinkNumber(0));
    }
    EXPECT_FALSE(HadPrerenderEventErrors());

    return prerenders;
  }

  void NavigateToURLImpl(const content::OpenURLParams& params,
                         bool expect_swap_to_succeed) const {
    ASSERT_TRUE(GetPrerenderManager());
    // Make sure in navigating we have a URL to use in the PrerenderManager.
    ASSERT_TRUE(GetPrerenderContents());

    WebContents* web_contents = GetPrerenderContents()->prerender_contents();

    // Navigate and wait for either the load to finish normally or for a swap to
    // occur.
    // TODO(davidben): The only handles CURRENT_TAB navigations, which is the
    // only case tested or prerendered right now.
    CHECK_EQ(WindowOpenDisposition::CURRENT_TAB, params.disposition);
    NavigationOrSwapObserver swap_observer(current_browser()->tab_strip_model(),
                                           GetActiveWebContents());
    WebContents* target_web_contents = current_browser()->OpenURL(params);
    swap_observer.Wait();

    if (web_contents && expect_swap_to_succeed) {
      EXPECT_EQ(web_contents, target_web_contents);
      if (call_javascript_)
        EXPECT_TRUE(DidDisplayPass(web_contents));
    }
  }

  // Opens the prerendered page using javascript functions in the loader
  // page. |javascript_function_name| should be a 0 argument function which is
  // invoked. |new_web_contents| is true if the navigation is expected to
  // happen in a new WebContents via OpenURL.
  void OpenURLWithJSImpl(const std::string& javascript_function_name,
                         const GURL& url,
                         const GURL& ping_url,
                         bool new_web_contents) const {
    WebContents* web_contents = GetActiveWebContents();
    RenderFrameHost* render_frame_host = web_contents->GetMainFrame();
    // Extra arguments in JS are ignored.
    std::string javascript = base::StringPrintf(
        "%s('%s', '%s')", javascript_function_name.c_str(),
        url.spec().c_str(), ping_url.spec().c_str());

    if (new_web_contents) {
      NewTabNavigationOrSwapObserver observer;
      render_frame_host->ExecuteJavaScriptWithUserGestureForTests(
          base::ASCIIToUTF16(javascript));
      observer.Wait();
    } else {
      NavigationOrSwapObserver observer(current_browser()->tab_strip_model(),
                                        web_contents);
      render_frame_host->ExecuteJavaScriptForTests(
          base::ASCIIToUTF16(javascript));
      observer.Wait();
    }
  }

  // Test TickClock that is set by OverridePrerenderManagerTimeTicks().
  base::SimpleTestTickClock clock_;

  GURL dest_url_;
  bool call_javascript_;
  bool check_load_events_;
  std::string loader_host_override_;
  std::string loader_path_;
  std::string loader_query_;
  base::test::ScopedFeatureList feature_list_;
  base::TimeDelta mid_load_clock_tick_advance_;
  base::SimpleTestTickClock* mid_load_clock_;
  std::unique_ptr<content::URLLoaderInterceptor> interceptor_;
};

// Checks that a page is correctly prerendered in the case of a
// <link rel=prerender> tag and then loaded into a tab in response to a
// navigation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPage) {
  test_utils::FirstContentfulPaintManagerWaiter* fcp_waiter =
      test_utils::FirstContentfulPaintManagerWaiter::Create(
          GetPrerenderManager());
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  EXPECT_EQ(1, GetPrerenderDomContentLoadedEventCountForLinkNumber(0));

  ChannelDestructionWatcher channel_close_watcher;
  channel_close_watcher.WatchChannel(
      GetActiveWebContents()->GetMainFrame()->GetProcess());
  NavigateToDestURL();
  channel_close_watcher.WaitForChannelClose();
  fcp_waiter->Wait();

  histogram_tester().ExpectTotalCount(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Visible", 1);
  histogram_tester().ExpectTotalCount(
      "Prerender.websame_PerceivedTTFCPRecorded.Visible", 1);

  ASSERT_TRUE(IsEmptyPrerenderLinkManager());
}

// Checks that the correct page load metrics observers are produced without a
// prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PageLoadMetricsSimple) {
  // The prefetch page is used as a simple page with a nonempty layout; no
  // prefetching is performed.
  test_utils::FirstContentfulPaintManagerWaiter* simple_fcp_waiter =
      test_utils::FirstContentfulPaintManagerWaiter::Create(
          GetPrerenderManager());
  ui_test_utils::NavigateToURL(
      current_browser(), src_server()->GetURL("/prerender/prefetch_page.html"));
  simple_fcp_waiter->Wait();

  histogram_tester().ExpectTotalCount(
      "Prerender.none_PrefetchTTFCP.Reference.Cacheable.Visible", 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.DocumentTiming.NavigationToFirstLayout", 1);

  // Histogram only emitted during a prerender, which should not happen here.
  histogram_tester().ExpectTotalCount(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Visible", 0);
}

// Checks that the correct page load metrics observers are produced with a
// prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PageLoadMetricsPrerender) {
  test_utils::FirstContentfulPaintManagerWaiter* prerender_fcp_waiter =
      test_utils::FirstContentfulPaintManagerWaiter::Create(
          GetPrerenderManager());
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
  prerender_fcp_waiter->Wait();

  histogram_tester().ExpectTotalCount(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Visible", 1);

  // Histograms only emitted during the simple load which does not happen here
  // (as prefetch_loader.html has an empty body, it does not generate a FCP).
  histogram_tester().ExpectTotalCount(
      "Prerender.none_PrefetchTTFCP.Reference.Cacheable.Visible", 0);
  histogram_tester().ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint", 0);
}

// Checks that cross-domain prerenders emit the correct histograms.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPageCrossDomain) {
  test_utils::FirstContentfulPaintManagerWaiter* fcp_waiter =
      test_utils::FirstContentfulPaintManagerWaiter::Create(
          GetPrerenderManager());
  PrerenderTestURL(GetCrossDomainTestUrl("prerender/prerender_page.html"),
                   FINAL_STATUS_USED, 1);

  NavigateToDestURL();
  fcp_waiter->Wait();

  histogram_tester().ExpectTotalCount(
      "Prerender.webcross_PrefetchTTFCP.Warm.Cacheable.Visible", 1);
}

// Checks that pending prerenders launch and receive proper event treatment.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPagePending) {
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_page_pending.html", FINAL_STATUS_USED, 1);

  // Navigate to the prerender.
  std::unique_ptr<TestPrerender> prerender2 =
      ExpectPrerender(FINAL_STATUS_USED);
  NavigateToDestURL();
  // Abort early if the original prerender didn't swap, so as not to hang.
  ASSERT_FALSE(prerender->contents());

  // Wait for the new prerender to be ready.
  prerender2->WaitForStart();
  prerender2->WaitForLoads(1);

  const GURL prerender_page_url =
      embedded_test_server()->GetURL("/prerender/prerender_page.html");
  EXPECT_FALSE(IsEmptyPrerenderLinkManager());
  EXPECT_TRUE(GetPrerenderContentsFor(prerender_page_url));

  // Now navigate to our target page.
  NavigationOrSwapObserver swap_observer(current_browser()->tab_strip_model(),
                                         GetActiveWebContents());
  ui_test_utils::NavigateToURLWithDisposition(
      current_browser(), prerender_page_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  swap_observer.Wait();

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

// Checks that pending prerenders which are canceled before they are launched
// never get started.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPageRemovesPending) {
  PrerenderTestURL("/prerender/prerender_page_removes_pending.html",
                   FINAL_STATUS_USED, 1);

  ChannelDestructionWatcher channel_close_watcher;
  channel_close_watcher.WatchChannel(
      GetActiveWebContents()->GetMainFrame()->GetProcess());
  NavigateToDestURL();
  channel_close_watcher.WaitForChannelClose();

  EXPECT_FALSE(DidReceivePrerenderStartEventForLinkNumber(1));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(1));
  EXPECT_FALSE(HadPrerenderEventErrors());
  // IsEmptyPrerenderLinkManager() is not racy because the earlier DidReceive*
  // calls did a thread/process hop to the renderer which insured pending
  // renderer events have arrived.
  ASSERT_TRUE(IsEmptyPrerenderLinkManager());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPageRemovingLink) {
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_page.html", FINAL_STATUS_CANCELLED, 1);

  // No ChannelDestructionWatcher is needed here, since prerenders in the
  // PrerenderLinkManager should be deleted by removing the links, rather than
  // shutting down the renderer process.
  RemoveLinkElement(0);
  prerender->WaitForStop();

  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(0));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));
  EXPECT_FALSE(HadPrerenderEventErrors());
  // IsEmptyPrerenderLinkManager() is not racy because the earlier DidReceive*
  // calls did a thread/process hop to the renderer which insured pending
  // renderer events have arrived.
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest, PrerenderPageRemovingLinkWithTwoLinks) {
  GetPrerenderManager()->mutable_config().max_link_concurrency = 2;
  GetPrerenderManager()->mutable_config().max_link_concurrency_per_launcher = 2;

  set_loader_query("links_to_insert=2");
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_page.html", FINAL_STATUS_CANCELLED, 1);
  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(0));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));
  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(1));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(1));

  RemoveLinkElement(0);
  RemoveLinkElement(1);
  prerender->WaitForStop();

  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(0));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));
  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(1));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(1));
  EXPECT_FALSE(HadPrerenderEventErrors());
  // IsEmptyPrerenderLinkManager() is not racy because the earlier DidReceive*
  // calls did a thread/process hop to the renderer which insured pending
  // renderer events have arrived.
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest, PrerenderPageRemovingLinkWithTwoLinksOneLate) {
  GetPrerenderManager()->mutable_config().max_link_concurrency = 2;
  GetPrerenderManager()->mutable_config().max_link_concurrency_per_launcher = 2;

  GURL url = embedded_test_server()->GetURL("/prerender/prerender_page.html");
  std::unique_ptr<TestPrerender> prerender =
      PrerenderTestURL(url, FINAL_STATUS_CANCELLED, 1);

  // Add a second prerender for the same link. It reuses the prerender, so only
  // the start event fires here.
  AddPrerender(url, 1);
  WaitForPrerenderEventCount(1, "webkitprerenderstart", 1);
  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(1));
  EXPECT_EQ(0, GetPrerenderLoadEventCountForLinkNumber(1));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(1));

  RemoveLinkElement(0);
  RemoveLinkElement(1);
  prerender->WaitForStop();

  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(0));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));
  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(1));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(1));
  EXPECT_FALSE(HadPrerenderEventErrors());
  // IsEmptyPrerenderLinkManager() is not racy because the earlier DidReceive*
  // calls did a thread/process hop to the renderer which insured pending
  // renderer events have arrived.
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    PrerenderPageRemovingLinkWithTwoLinksRemovingOne) {
  GetPrerenderManager()->mutable_config().max_link_concurrency = 2;
  GetPrerenderManager()->mutable_config().max_link_concurrency_per_launcher = 2;
  set_loader_query("links_to_insert=2");
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(0));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));
  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(1));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(1));

  RemoveLinkElement(0);
  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(0));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));
  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(1));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(1));
  EXPECT_FALSE(HadPrerenderEventErrors());
  // IsEmptyPrerenderLinkManager() is not racy because the earlier DidReceive*
  // calls did a thread/process hop to the renderer which insured pending
  // renderer events have arrived.
  EXPECT_FALSE(IsEmptyPrerenderLinkManager());

  ChannelDestructionWatcher channel_close_watcher;
  channel_close_watcher.WatchChannel(
      GetActiveWebContents()->GetMainFrame()->GetProcess());
  NavigateToDestURL();
  channel_close_watcher.WaitForChannelClose();

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

// Checks that the visibility API works.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderVisibility) {
  PrerenderTestURL("/prerender/prerender_visibility.html", FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that the prerendering of a page is canceled correctly if we try to
// swap it in before it commits.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNoCommitNoSwap) {
  // Navigate to a page that triggers a prerender for a URL that never commits.
  const GURL kNoCommitUrl("http://never-respond.example.com");
  base::FilePath file(GetTestPath("prerender_page.html"));

  base::RunLoop prerender_start_loop;
  CreateHangingFirstRequestInterceptor(kNoCommitUrl, file,
                                       prerender_start_loop.QuitClosure());
  DisableJavascriptCalls();
  PrerenderTestURL(kNoCommitUrl,
                   FINAL_STATUS_NAVIGATION_UNCOMMITTED,
                   0);
  // Wait for the hanging request to be scheduled.
  prerender_start_loop.Run();

  // Navigate to the URL, but assume the contents won't be swapped in.
  NavigateToDestURLWithDisposition(WindowOpenDisposition::CURRENT_TAB, false);
}

// Checks that client redirects don't add alias URLs until after they commit.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNoCommitNoSwap2) {
  // Navigate to a page that then navigates to a URL that never commits.
  const GURL kNoCommitUrl("http://never-respond.example.com");
  base::FilePath file(GetTestPath("prerender_page.html"));

  base::RunLoop prerender_start_loop;
  CreateHangingFirstRequestInterceptor(kNoCommitUrl, file,
                                       prerender_start_loop.QuitClosure());
  DisableJavascriptCalls();
  PrerenderTestURL(CreateClientRedirect(kNoCommitUrl.spec()),
                   FINAL_STATUS_APP_TERMINATING, 1);
  // Wait for the hanging request to be scheduled.
  prerender_start_loop.Run();

  // Navigating to the second URL should not swap.
  NavigateToURLWithDisposition(kNoCommitUrl, WindowOpenDisposition::CURRENT_TAB,
                               false);
}

// Checks that the prerendering of a page is canceled correctly when a
// Javascript alert is called.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderAlertBeforeOnload) {
  PrerenderTestURL("/prerender/prerender_alert_before_onload.html",
                   FINAL_STATUS_JAVASCRIPT_ALERT, 0);
}

// Checks that the prerendering of a page is canceled correctly when a
// Javascript alert is called.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderAlertAfterOnload) {
  PrerenderTestURL("/prerender/prerender_alert_after_onload.html",
                   FINAL_STATUS_JAVASCRIPT_ALERT, 1);
}

// Checks that plugins are not loaded while a page is being preloaded, but
// are loaded when the page is displayed.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDelayLoadPlugin) {
  HostContentSettingsMap* content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(
          current_browser()->profile());
  GURL server_root = embedded_test_server()->GetURL("/");
  content_settings_map->SetContentSettingDefaultScope(
      server_root, server_root, CONTENT_SETTINGS_TYPE_PLUGINS, std::string(),
      CONTENT_SETTING_ALLOW);

  PrerenderTestURL("/prerender/prerender_plugin_delay_load.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();

  // Because NavigateToDestURL relies on a synchronous check, and the plugin
  // loads asynchronously, we use a separate DidPluginLoad() test. Failure
  // is indicated by timeout, as plugins may take arbitrarily long to load.
  bool plugin_loaded = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetActiveWebContents(), "DidPluginLoad()", &plugin_loaded));
  EXPECT_TRUE(plugin_loaded);
}

// For Plugin Power Saver, checks that plugins are not loaded while
// a page is being preloaded, but are loaded when the page is displayed.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPluginPowerSaver) {
  HostContentSettingsMap* content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(
          current_browser()->profile());
  GURL server_root = embedded_test_server()->GetURL("/");
  content_settings_map->SetContentSettingDefaultScope(
      server_root, server_root, CONTENT_SETTINGS_TYPE_PLUGINS, std::string(),
      CONTENT_SETTING_ALLOW);

  PrerenderTestURL("/prerender/prerender_plugin_power_saver.html",
                   FINAL_STATUS_USED, 1);

  DisableJavascriptCalls();
  NavigateToDestURL();
  bool second_placeholder_present = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetActiveWebContents(), "AwaitPluginPrerollAndPlaceholder();",
      &second_placeholder_present));
  EXPECT_TRUE(second_placeholder_present);
}

// For Content Setting BLOCK, checks that plugins are never loaded.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderContentSettingBlock) {
  HostContentSettingsMap* content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(
          current_browser()->profile());
  content_settings_map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                                 CONTENT_SETTING_BLOCK);

  PrerenderTestURL("/prerender/prerender_plugin_never_load.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that we don't load a NaCl plugin when NaCl is disabled.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNaClPluginDisabled) {
  PrerenderTestURL("/prerender/prerender_plugin_nacl_disabled.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();


  // Run this check again.  When we try to load aa ppapi plugin, the
  // "loadstart" event is asynchronously posted to a message loop.
  // It's possible that earlier call could have been run before the
  // the "loadstart" event was posted.
  // TODO(mmenke):  While this should reliably fail on regressions, the
  //                reliability depends on the specifics of ppapi plugin
  //                loading.  It would be great if we could avoid that.
  EXPECT_TRUE(DidDisplayPass(GetActiveWebContents()));
}

// Checks that plugins in an iframe are not loaded while a page is
// being preloaded, but are loaded when the page is displayed.
#if defined(USE_AURA) && !defined(OS_WIN)
// http://crbug.com/103496
#define MAYBE_PrerenderIframeDelayLoadPlugin \
        DISABLED_PrerenderIframeDelayLoadPlugin
#elif defined(OS_MACOSX)
// http://crbug.com/100514
#define MAYBE_PrerenderIframeDelayLoadPlugin \
        DISABLED_PrerenderIframeDelayLoadPlugin
#elif defined(OS_WIN)
// TODO(jschuh): Failing plugin tests. https://crbug.com/244653,
// https://crbug.com/876872
#define MAYBE_PrerenderIframeDelayLoadPlugin \
        DISABLED_PrerenderIframeDelayLoadPlugin
#else
#define MAYBE_PrerenderIframeDelayLoadPlugin PrerenderIframeDelayLoadPlugin
#endif
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MAYBE_PrerenderIframeDelayLoadPlugin) {
  HostContentSettingsMap* content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(
          current_browser()->profile());
  GURL server_root = embedded_test_server()->GetURL("/");
  content_settings_map->SetContentSettingDefaultScope(
      server_root, server_root, CONTENT_SETTINGS_TYPE_PLUGINS, std::string(),
      CONTENT_SETTING_ALLOW);

  PrerenderTestURL("/prerender/prerender_iframe_plugin_delay_load.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Renders a page that contains a prerender link to a page that contains an
// iframe with a source that requires http authentication. This should not
// prerender successfully.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHttpAuthentication) {
  PrerenderTestURL("/prerender/prerender_http_auth_container.html",
                   FINAL_STATUS_AUTH_NEEDED, 0);
}

// Checks that client-issued redirects work with prerendering.
// This version navigates to the page which issues the redirection, rather
// than the final destination page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectNavigateToFirst) {
  PrerenderTestURL(CreateClientRedirect("/prerender/prerender_page.html"),
                   FINAL_STATUS_USED, 2);
  NavigateToDestURL();
}

// Checks that client-issued redirects work with prerendering.
// This version navigates to the final destination page, rather than the
// page which does the redirection.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectNavigateToSecond) {
  PrerenderTestURL(CreateClientRedirect("/prerender/prerender_page.html"),
                   FINAL_STATUS_USED, 2);
  NavigateToURL("/prerender/prerender_page.html");
}

// Checks that redirects with location.replace do not cancel a prerender and
// and swap when navigating to the first page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderLocationReplaceNavigateToFirst) {
  PrerenderTestURL("/prerender/prerender_location_replace.html",
                   FINAL_STATUS_USED, 2);
  NavigateToDestURL();
}

// Checks that redirects with location.replace do not cancel a prerender and
// and swap when navigating to the second.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderLocationReplaceNavigateToSecond) {
  PrerenderTestURL("/prerender/prerender_location_replace.html",
                   FINAL_STATUS_USED, 2);
  NavigateToURL("/prerender/prerender_page.html");
}

// Checks that client-issued redirects work with prerendering.
// This version navigates to the final destination page, rather than the
// page which does the redirection via a mouse click.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectNavigateToSecondViaClick) {
  GURL prerender_url = embedded_test_server()->GetURL(
      CreateClientRedirect("/prerender/prerender_page.html"));
  GURL destination_url =
      embedded_test_server()->GetURL("/prerender/prerender_page.html");
  PrerenderTestURL(prerender_url, FINAL_STATUS_USED, 2);
  OpenURLViaClick(destination_url);
}

// Checks that a page served over HTTPS is correctly prerendered.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHttps) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("/prerender/prerender_page.html");
  PrerenderTestURL(https_url,
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that client-issued redirects within an iframe in a prerendered
// page will not count as an "alias" for the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectInIframe) {
  std::string redirect_path =
      CreateClientRedirect("/prerender/prerender_embedded_content.html");
  base::StringPairs replacement_text;
  replacement_text.push_back(std::make_pair("REPLACE_WITH_URL", redirect_path));
  std::string replacement_path;
  net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_iframe.html", replacement_text,
      &replacement_path);
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 2);
  EXPECT_FALSE(
      UrlIsInPrerenderManager("/prerender/prerender_embedded_content.html"));
  NavigateToDestURL();
}

// Checks that server-issued redirects work with prerendering.
// This version navigates to the page which issues the redirection, rather
// than the final destination page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderServerRedirectNavigateToFirst) {
  PrerenderTestURL(CreateServerRedirect("/prerender/prerender_page.html"),
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that server-issued redirects work with prerendering.
// This version navigates to the final destination page, rather than the
// page which does the redirection.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderServerRedirectNavigateToSecond) {
  PrerenderTestURL(CreateServerRedirect("/prerender/prerender_page.html"),
                   FINAL_STATUS_USED, 1);
  NavigateToURL("/prerender/prerender_page.html");
}

// Checks that server-issued redirects work with prerendering.
// This version navigates to the final destination page, rather than the
// page which does the redirection via a mouse click.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderServerRedirectNavigateToSecondViaClick) {
  GURL prerender_url = embedded_test_server()->GetURL(
      CreateServerRedirect("/prerender/prerender_page.html"));
  GURL destination_url =
      embedded_test_server()->GetURL("/prerender/prerender_page.html");
  PrerenderTestURL(prerender_url, FINAL_STATUS_USED, 1);
  OpenURLViaClick(destination_url);
}

// Checks that server-issued redirects within an iframe in a prerendered
// page will not count as an "alias" for the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderServerRedirectInIframe) {
  std::string redirect_path =
      CreateServerRedirect("//prerender/prerender_embedded_content.html");
  base::StringPairs replacement_text;
  replacement_text.push_back(std::make_pair("REPLACE_WITH_URL", redirect_path));
  std::string replacement_path;
  net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_iframe.html", replacement_text,
      &replacement_path);
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  EXPECT_FALSE(
      UrlIsInPrerenderManager("/prerender/prerender_embedded_content.html"));
  NavigateToDestURL();
}

// Prerenders a page that contains an automatic download triggered through an
// iframe. This should not prerender successfully.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDownloadIframe) {
  PrerenderTestURL("/prerender/prerender_download_iframe.html",
                   FINAL_STATUS_DOWNLOAD, 0);
}

// Prerenders a page that contains an automatic download triggered through
// Javascript changing the window.location. This should not prerender
// successfully
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDownloadLocation) {
  PrerenderTestURL(CreateClientRedirect("/download-test1.lib"),
                   FINAL_STATUS_DOWNLOAD, 1);
}

// Prerenders a page that contains an automatic download triggered through a
// client-issued redirect. This should not prerender successfully.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDownloadClientRedirect) {
  PrerenderTestURL("/prerender/prerender_download_refresh.html",
                   FINAL_STATUS_DOWNLOAD, 1);
}

// Checks that the referrer is set when prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderReferrer) {
  PrerenderTestURL("/prerender/prerender_referrer.html", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that the referrer is not set when prerendering and the source page is
// HTTPS.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNoSSLReferrer) {
  // Use http:// url for the prerendered page main resource.
  GURL url(
      embedded_test_server()->GetURL("/prerender/prerender_no_referrer.html"));

  // Use https:// for all other resources.
  UseHttpsSrcServer();

  PrerenderTestURL(url, FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that the referrer is set when prerendering is cancelled.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCancelReferrer) {
  std::unique_ptr<TestContentBrowserClient> test_content_browser_client(
      new TestContentBrowserClient);
  content::ContentBrowserClient* original_browser_client =
      content::SetBrowserClientForTesting(test_content_browser_client.get());

  PrerenderTestURL("/prerender/prerender_referrer.html", FINAL_STATUS_CANCELLED,
                   1);
  OpenDestURLViaClick();

  EXPECT_TRUE(DidDisplayPass(GetActiveWebContents()));

  content::SetBrowserClientForTesting(original_browser_client);
}

// Checks that popups on a prerendered page cause cancellation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPopup) {
  PrerenderTestURL("/prerender/prerender_popup.html",
                   FINAL_STATUS_CREATE_NEW_WINDOW, 0);
}

// Checks that registering a protocol handler causes cancellation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderRegisterProtocolHandler) {
  PrerenderTestURL("/prerender/prerender_register_protocol_handler.html",
                   FINAL_STATUS_REGISTER_PROTOCOL_HANDLER, 0);
}

// Checks that renderers using excessive memory will be terminated.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderExcessiveMemory) {
  ASSERT_TRUE(GetPrerenderManager());
  GetPrerenderManager()->mutable_config().max_bytes = 30 * 1024 * 1024;
  // The excessive memory kill may happen before or after the load event as it
  // happens asynchronously with IPC calls. Even if the test does not start
  // allocating until after load, the browser process might notice before the
  // message gets through. This happens on XP debug bots because they're so
  // slow. Instead, don't bother checking the load event count.
  DisableLoadEventCheck();
  PrerenderTestURL("/prerender/prerender_excessive_memory.html",
                   FINAL_STATUS_MEMORY_LIMIT_EXCEEDED, 0);
}

// Checks shutdown code while a prerender is active.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderQuickQuit) {
  DisableJavascriptCalls();
  DisableLoadEventCheck();
  PrerenderTestURL("/prerender/prerender_page.html",
                   FINAL_STATUS_APP_TERMINATING, 0);
}

// Checks that we don't prerender in an infinite loop.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderInfiniteLoop) {
  const char* const kHtmlFileA = "/prerender/prerender_infinite_a.html";
  const char* const kHtmlFileB = "/prerender/prerender_infinite_b.html";

  std::vector<FinalStatus> expected_final_status_queue;
  expected_final_status_queue.push_back(FINAL_STATUS_USED);
  expected_final_status_queue.push_back(FINAL_STATUS_APP_TERMINATING);

  std::vector<std::unique_ptr<TestPrerender>> prerenders =
      PrerenderTestURL(kHtmlFileA, expected_final_status_queue, 1);
  ASSERT_TRUE(prerenders[0]->contents());
  // Assert that the pending prerender is in there already. This relies on the
  // fact that the renderer sends out the AddLinkRelPrerender IPC before sending
  // the page load one.
  EXPECT_EQ(2U, GetLinkPrerenderCount());
  EXPECT_EQ(1U, GetRunningLinkPrerenderCount());

  // Next url should be in pending list but not an active entry.
  EXPECT_FALSE(UrlIsInPrerenderManager(kHtmlFileB));

  NavigateToDestURL();

  // Make sure the PrerenderContents for the next url is now in the manager and
  // not pending. This relies on pending prerenders being resolved in the same
  // event loop iteration as OnPrerenderStop.
  EXPECT_TRUE(UrlIsInPrerenderManager(kHtmlFileB));
  EXPECT_EQ(1U, GetLinkPrerenderCount());
  EXPECT_EQ(1U, GetRunningLinkPrerenderCount());
}

// Checks that we don't prerender in an infinite loop and multiple links are
// handled correctly.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_PrerenderInfiniteLoopMultiple) {
  const char* const kHtmlFileA =
      "/prerender/prerender_infinite_a_multiple.html";
  const char* const kHtmlFileB =
      "/prerender/prerender_infinite_b_multiple.html";
  const char* const kHtmlFileC =
      "/prerender/prerender_infinite_c_multiple.html";

  // This test is conceptually simplest if concurrency is at two, since we
  // don't have to worry about which of kHtmlFileB or kHtmlFileC gets evicted.
  GetPrerenderManager()->mutable_config().max_link_concurrency = 2;
  GetPrerenderManager()->mutable_config().max_link_concurrency_per_launcher = 2;

  std::vector<FinalStatus> expected_final_status_queue;
  expected_final_status_queue.push_back(FINAL_STATUS_USED);
  expected_final_status_queue.push_back(FINAL_STATUS_APP_TERMINATING);
  expected_final_status_queue.push_back(FINAL_STATUS_APP_TERMINATING);

  std::vector<std::unique_ptr<TestPrerender>> prerenders =
      PrerenderTestURL(kHtmlFileA, expected_final_status_queue, 1);
  ASSERT_TRUE(prerenders[0]->contents());

  // Next url should be in pending list but not an active entry. This relies on
  // the fact that the renderer sends out the AddLinkRelPrerender IPC before
  // sending the page load one.
  EXPECT_EQ(3U, GetLinkPrerenderCount());
  EXPECT_EQ(1U, GetRunningLinkPrerenderCount());
  EXPECT_FALSE(UrlIsInPrerenderManager(kHtmlFileB));
  EXPECT_FALSE(UrlIsInPrerenderManager(kHtmlFileC));

  NavigateToDestURL();

  // Make sure the PrerenderContents for the next urls are now in the manager
  // and not pending. One and only one of the URLs (the last seen) should be the
  // active entry. This relies on pending prerenders being resolved in the same
  // event loop iteration as OnPrerenderStop.
  bool url_b_is_active_prerender = UrlIsInPrerenderManager(kHtmlFileB);
  bool url_c_is_active_prerender = UrlIsInPrerenderManager(kHtmlFileC);
  EXPECT_TRUE(url_b_is_active_prerender && url_c_is_active_prerender);
  EXPECT_EQ(2U, GetLinkPrerenderCount());
  EXPECT_EQ(2U, GetRunningLinkPrerenderCount());
}

// Checks that pending prerenders are aborted (and never launched) when launched
// by a prerender that itself gets aborted.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderAbortPendingOnCancel) {
  const char* const kHtmlFileA = "/prerender/prerender_infinite_a.html";
  const char* const kHtmlFileB = "/prerender/prerender_infinite_b.html";

  std::unique_ptr<TestPrerender> prerender =
      PrerenderTestURL(kHtmlFileA, FINAL_STATUS_CANCELLED, 1);
  ASSERT_TRUE(prerender->contents());
  // Assert that the pending prerender is in there already. This relies on the
  // fact that the renderer sends out the AddLinkRelPrerender IPC before sending
  // the page load one.
  EXPECT_EQ(2U, GetLinkPrerenderCount());
  EXPECT_EQ(1U, GetRunningLinkPrerenderCount());

  // Next url should be in pending list but not an active entry.
  EXPECT_FALSE(UrlIsInPrerenderManager(kHtmlFileB));

  // Cancel the prerender.
  GetPrerenderManager()->CancelAllPrerenders();
  prerender->WaitForStop();

  // All prerenders are now gone.
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, OpenTaskManagerBeforePrerender) {
  const base::string16 any_prerender = MatchTaskManagerPrerender("*");
  const base::string16 any_tab = MatchTaskManagerTab("*");
  const base::string16 original = MatchTaskManagerTab("Preloader");
  const base::string16 prerender = MatchTaskManagerPrerender("Prerender Page");
  const base::string16 final = MatchTaskManagerTab("Prerender Page");

  // Show the task manager. This populates the model.
  chrome::OpenTaskManager(current_browser());
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, any_prerender));

  // Prerender a page in addition to the original tab.
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  // A TaskManager entry should appear like "Prerender: Prerender Page"
  // alongside the original tab entry. There should be just these two entries.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, original));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, final));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));

  // Swap in the prerendered content.
  NavigateToDestURL();

  // The "Prerender: " TaskManager entry should disappear, being replaced by a
  // "Tab: Prerender Page" entry, and nothing else.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, original));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, final));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, any_prerender));
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, OpenTaskManagerAfterPrerender) {
  const base::string16 any_prerender = MatchTaskManagerPrerender("*");
  const base::string16 any_tab = MatchTaskManagerTab("*");
  const base::string16 original = MatchTaskManagerTab("Preloader");
  const base::string16 prerender = MatchTaskManagerPrerender("Prerender Page");
  const base::string16 final = MatchTaskManagerTab("Prerender Page");

  // Start with two resources.
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  // Show the task manager. This populates the model. Importantly, we're doing
  // this after the prerender WebContents already exists - the task manager
  // needs to find it, it can't just listen for creation.
  chrome::OpenTaskManager(current_browser());

  // A TaskManager entry should appear like "Prerender: Prerender Page"
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, original));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, final));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));

  // Swap in the tab.
  NavigateToDestURL();

  // The "Prerender: Prerender Page" TaskManager row should disappear, being
  // replaced by "Tab: Prerender Page"
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, original));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, final));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, any_prerender));
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, OpenTaskManagerAfterSwapIn) {
  const base::string16 any_prerender = MatchTaskManagerPrerender("*");
  const base::string16 any_tab = MatchTaskManagerTab("*");
  const base::string16 final = MatchTaskManagerTab("Prerender Page");

  // Prerender, and swap it in.
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  NavigateToDestURL();

  // Show the task manager. This populates the model. Importantly, we're doing
  // this after the prerender has been swapped in.
  chrome::OpenTaskManager(current_browser());

  // We should not see a prerender resource in the task manager, just a normal
  // page.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, final));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, any_prerender));
}

// Checks that audio loads are deferred on prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHTML5Audio) {
  PrerenderTestURL("/prerender/prerender_html5_audio.html", FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
  WaitForASCIITitle(GetActiveWebContents(), kPassTitle);
}

// Checks that audio loads are deferred on prerendering and played back when
// the prerender is swapped in if autoplay is set.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHTML5AudioAutoplay) {
  PrerenderTestURL("/prerender/prerender_html5_audio_autoplay.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
  WaitForASCIITitle(GetActiveWebContents(), kPassTitle);
}

// Checks that audio loads are deferred on prerendering and played back when
// the prerender is swapped in if js starts playing.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHTML5AudioJsplay) {
  PrerenderTestURL("/prerender/prerender_html5_audio_jsplay.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
  WaitForASCIITitle(GetActiveWebContents(), kPassTitle);
}

// Checks that video loads are deferred on prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHTML5Video) {
  PrerenderTestURL("/prerender/prerender_html5_video.html", FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
  WaitForASCIITitle(GetActiveWebContents(), kPassTitle);
}

// Checks that video tags inserted by javascript are deferred and played
// correctly on swap in.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHTML5VideoJs) {
  PrerenderTestURL("/prerender/prerender_html5_video_script.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
  WaitForASCIITitle(GetActiveWebContents(), kPassTitle);
}

// Checks for correct network events by using a busy sleep the javascript.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHTML5VideoNetwork) {
  DisableJavascriptCalls();
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_html5_video_network.html", FINAL_STATUS_USED, 1);
  WaitForASCIITitle(prerender->contents()->prerender_contents(), kReadyTitle);
  EXPECT_TRUE(DidPrerenderPass(prerender->contents()->prerender_contents()));
  NavigateToDestURL();
  WaitForASCIITitle(GetActiveWebContents(), kPassTitle);
}

// Checks that scripts can retrieve the correct window size while prerendering.
// Disabled on ChromeOS. See https://crbug.com/807821.
#if defined(OS_CHROMEOS)
#define MAYBE_PrerenderWindowSize DISABLED_PrerenderWindowSize
#else
#define MAYBE_PrerenderWindowSize PrerenderWindowSize
#endif
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, MAYBE_PrerenderWindowSize) {
  PrerenderTestURL("/prerender/prerender_size.html", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// TODO(jam): http://crbug.com/350550
#if !(defined(OS_CHROMEOS) && defined(ADDRESS_SANITIZER))

// Checks that prerenderers will terminate when the RenderView crashes.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderRendererCrash) {
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_page.html", FINAL_STATUS_RENDERER_CRASHED, 1);

  // Navigate to about:crash and then wait for the renderer to crash.
  ASSERT_TRUE(prerender->contents());
  ASSERT_TRUE(prerender->contents()->prerender_contents());
  prerender->contents()->prerender_contents()->GetController().
      LoadURL(
          GURL(content::kChromeUICrashURL),
          content::Referrer(),
          ui::PAGE_TRANSITION_TYPED,
          std::string());
  prerender->WaitForStop();
}
#endif

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderPageWithFragment) {
  PrerenderTestURL("/prerender/prerender_page.html#fragment", FINAL_STATUS_USED,
                   1);

  ChannelDestructionWatcher channel_close_watcher;
  channel_close_watcher.WatchChannel(browser()
                                         ->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetMainFrame()
                                         ->GetProcess());
  NavigateToDestURL();
  channel_close_watcher.WaitForChannelClose();

  ASSERT_TRUE(IsEmptyPrerenderLinkManager());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderPageWithRedirectedFragment) {
  PrerenderTestURL(
      CreateClientRedirect("/prerender/prerender_page.html#fragment"),
      FINAL_STATUS_USED, 2);

  ChannelDestructionWatcher channel_close_watcher;
  channel_close_watcher.WatchChannel(browser()
                                         ->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetMainFrame()
                                         ->GetProcess());
  NavigateToDestURL();
  channel_close_watcher.WaitForChannelClose();

  ASSERT_TRUE(IsEmptyPrerenderLinkManager());
}

// Checks that we do not use a prerendered page when navigating from
// the main page to a fragment.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPageNavigateFragment) {
  PrerenderTestURL("/prerender/no_prerender_page.html",
                   FINAL_STATUS_APP_TERMINATING, 1);
  NavigateToURLWithDisposition("/prerender/no_prerender_page.html#fragment",
                               WindowOpenDisposition::CURRENT_TAB, false);
}

// Checks that we do not use a prerendered page when we prerender a fragment
// but navigate to the main page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderFragmentNavigatePage) {
  PrerenderTestURL("/prerender/no_prerender_page.html#fragment",
                   FINAL_STATUS_APP_TERMINATING, 1);
  NavigateToURLWithDisposition("/prerender/no_prerender_page.html",
                               WindowOpenDisposition::CURRENT_TAB, false);
}

// Checks that we do not use a prerendered page when we prerender a fragment
// but navigate to a different fragment on the same page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderFragmentNavigateFragment) {
  PrerenderTestURL("/prerender/no_prerender_page.html#other_fragment",
                   FINAL_STATUS_APP_TERMINATING, 1);
  NavigateToURLWithDisposition("/prerender/no_prerender_page.html#fragment",
                               WindowOpenDisposition::CURRENT_TAB, false);
}

// Checks that we do not use a prerendered page when the page uses a client
// redirect to refresh from a fragment on the same page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectFromFragment) {
  PrerenderTestURL(
      CreateClientRedirect("/prerender/no_prerender_page.html#fragment"),
      FINAL_STATUS_APP_TERMINATING, 2);
  NavigateToURLWithDisposition("/prerender/no_prerender_page.html",
                               WindowOpenDisposition::CURRENT_TAB, false);
}

// Checks that we do not use a prerendered page when the page uses a client
// redirect to refresh to a fragment on the same page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectToFragment) {
  PrerenderTestURL(CreateClientRedirect("/prerender/no_prerender_page.html"),
                   FINAL_STATUS_APP_TERMINATING, 2);
  NavigateToURLWithDisposition("/prerender/no_prerender_page.html#fragment",
                               WindowOpenDisposition::CURRENT_TAB, false);
}

// Checks that we correctly use a prerendered page when the page uses JS to set
// the window.location.hash to a fragment on the same page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderPageChangeFragmentLocationHash) {
  PrerenderTestURL("/prerender/prerender_fragment_location_hash.html",
                   FINAL_STATUS_USED, 1);
  NavigateToURL("/prerender/prerender_fragment_location_hash.html");
}

// Checks that prerendering a PNG works correctly.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderImagePng) {
  DisableJavascriptCalls();
  PrerenderTestURL("/prerender/image.png", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that prerendering a JPG works correctly.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderImageJpeg) {
  DisableJavascriptCalls();
  PrerenderTestURL(kPrefetchJpeg, FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that a prerender of a CRX will result in a cancellation due to
// download.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCrx) {
  PrerenderTestURL("/prerender/extension.crx", FINAL_STATUS_DOWNLOAD, 0);
}

// Checks that xhr GET requests allow prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrGet) {
  PrerenderTestURL("/prerender/prerender_xhr_get.html", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that xhr HEAD requests allow prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrHead) {
  PrerenderTestURL("/prerender/prerender_xhr_head.html", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that xhr OPTIONS requests allow prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrOptions) {
  PrerenderTestURL("/prerender/prerender_xhr_options.html", FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that xhr TRACE requests allow prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrTrace) {
  PrerenderTestURL("/prerender/prerender_xhr_trace.html", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that xhr POST requests allow prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrPost) {
  PrerenderTestURL("/prerender/prerender_xhr_post.html", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that xhr PUT cancels prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrPut) {
  PrerenderTestURL("/prerender/prerender_xhr_put.html",
                   FINAL_STATUS_INVALID_HTTP_METHOD, 1);
}

// Checks that xhr DELETE cancels prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrDelete) {
  PrerenderTestURL("/prerender/prerender_xhr_delete.html",
                   FINAL_STATUS_INVALID_HTTP_METHOD, 1);
}

// Sets up HTTPS server for prerendered page, and checks that an SSL error will
// cancel the prerender. The prerenderer loader will be served through HTTP.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLErrorTopLevel) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("/prerender/prerender_page.html");
  PrerenderTestURL(https_url, FINAL_STATUS_SSL_ERROR, 0);
}

// Checks that an SSL error that comes from a subresource does not cancel
// the page. Non-main-frame requests are simply cancelled if they run into
// an SSL problem.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLErrorSubresource) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL(kPrefetchJpeg);
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", https_url.spec()));
  std::string replacement_path;
  net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_image.html", replacement_text,
      &replacement_path);
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that an SSL error that comes from an iframe does not cancel
// the page. Non-main-frame requests are simply cancelled if they run into
// an SSL problem.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLErrorIframe) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  GURL https_url =
      https_server.GetURL("/prerender/prerender_embedded_content.html");
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", https_url.spec()));
  std::string replacement_path;
  net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_iframe.html", replacement_text,
      &replacement_path);
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that we cancel correctly when window.print() is called.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPrint) {
  DisableLoadEventCheck();
  PrerenderTestURL("/prerender/prerender_print.html", FINAL_STATUS_WINDOW_PRINT,
                   0);
}

// Checks that prerenders do not get swapped into target pages that have opened
// popups; the BrowsingInstance is not empty.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderTargetHasPopup) {
  PrerenderTestURL("/prerender/prerender_page.html",
                   FINAL_STATUS_NON_EMPTY_BROWSING_INSTANCE, 1);
  OpenURLViaWindowOpen(GURL(url::kAboutBlankURL));

  // Switch back to the current tab and attempt to swap it in.
  current_browser()->tab_strip_model()->ActivateTabAt(0, true);
  NavigateToDestURLWithDisposition(WindowOpenDisposition::CURRENT_TAB, false);
}

class TestClientCertStore : public net::ClientCertStore {
 public:
  explicit TestClientCertStore(const net::CertificateList& certs)
      : certs_(certs) {}
  ~TestClientCertStore() override {}

  // net::ClientCertStore:
  void GetClientCerts(const net::SSLCertRequestInfo& cert_request_info,
                      const ClientCertListCallback& callback) override {
    callback.Run(FakeClientCertIdentityListFromCertificateList(certs_));
  }

 private:
  net::CertificateList certs_;
};

std::unique_ptr<net::ClientCertStore> CreateCertStore(
    scoped_refptr<net::X509Certificate> available_cert) {
  return std::unique_ptr<net::ClientCertStore>(
      new TestClientCertStore(net::CertificateList(1, available_cert)));
}

// Checks that a top-level page which would normally request an SSL client
// certificate will never be seen since it's an https top-level resource.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderSSLClientCertTopLevel) {
  ProfileIOData::FromResourceContext(
      current_browser()->profile()->GetResourceContext())
      ->set_client_cert_store_factory_for_testing(base::Bind(
          &CreateCertStore, net::ImportCertFromFile(
                                net::GetTestCertsDirectory(), "ok_cert.pem")));
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("/prerender/prerender_page.html");
  PrerenderTestURL(https_url, FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED, 0);
}

// Checks that an SSL Client Certificate request that originates from a
// subresource will cancel the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderSSLClientCertSubresource) {
  ProfileIOData::FromResourceContext(
      current_browser()->profile()->GetResourceContext())
      ->set_client_cert_store_factory_for_testing(base::Bind(
          &CreateCertStore, net::ImportCertFromFile(
                                net::GetTestCertsDirectory(), "ok_cert.pem")));
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL(kPrefetchJpeg);
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", https_url.spec()));
  std::string replacement_path;
  net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_image.html", replacement_text,
      &replacement_path);
  PrerenderTestURL(replacement_path,
                   FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED,
                   0);
}

// Checks that an SSL Client Certificate request that originates from an
// iframe will cancel the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLClientCertIframe) {
  ProfileIOData::FromResourceContext(
      current_browser()->profile()->GetResourceContext())
      ->set_client_cert_store_factory_for_testing(base::Bind(
          &CreateCertStore, net::ImportCertFromFile(
                                net::GetTestCertsDirectory(), "ok_cert.pem")));
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  GURL https_url =
      https_server.GetURL("/prerender/prerender_embedded_content.html");
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", https_url.spec()));
  std::string replacement_path;
  net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_iframe.html", replacement_text,
      &replacement_path);
  PrerenderTestURL(replacement_path,
                   FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED,
                   0);
}

// Ensures that we do not prerender pages with a safe browsing
// interstitial.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSafeBrowsingTopLevel) {
  GURL url = embedded_test_server()->GetURL("/prerender/prerender_page.html");
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      url, safe_browsing::SB_THREAT_TYPE_URL_MALWARE);
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_SAFE_BROWSING,
                   0);
}

// Ensures that server redirects to a malware page will cancel prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderSafeBrowsingServerRedirect) {
  GURL url = embedded_test_server()->GetURL("/prerender/prerender_page.html");
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      url, safe_browsing::SB_THREAT_TYPE_URL_MALWARE);
  PrerenderTestURL(CreateServerRedirect("/prerender/prerender_page.html"),
                   FINAL_STATUS_SAFE_BROWSING, 0);
}

// Ensures that client redirects to a malware page will cancel prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderSafeBrowsingClientRedirect) {
  GURL url = embedded_test_server()->GetURL("/prerender/prerender_page.html");
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      url, safe_browsing::SB_THREAT_TYPE_URL_MALWARE);
  PrerenderTestURL(CreateClientRedirect("/prerender/prerender_page.html"),
                   FINAL_STATUS_SAFE_BROWSING, 1);
}

// Ensures that we do not prerender pages which have a malware subresource.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSafeBrowsingSubresource) {
  GURL image_url = embedded_test_server()->GetURL(kPrefetchJpeg);
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      image_url, safe_browsing::SB_THREAT_TYPE_URL_MALWARE);
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", image_url.spec()));
  std::string replacement_path;
  net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_image.html", replacement_text,
      &replacement_path);
  PrerenderTestURL(replacement_path,
                   FINAL_STATUS_SAFE_BROWSING,
                   0);
}

// Ensures that we do not prerender pages which have a malware iframe.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSafeBrowsingIframe) {
  GURL iframe_url = embedded_test_server()->GetURL(
      "/prerender/prerender_embedded_content.html");
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      iframe_url, safe_browsing::SB_THREAT_TYPE_URL_MALWARE);
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", iframe_url.spec()));
  std::string replacement_path;
  net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_iframe.html", replacement_text,
      &replacement_path);
  PrerenderTestURL(replacement_path,
                   FINAL_STATUS_SAFE_BROWSING,
                   0);
}

// Checks that a local storage read will not cause prerender to fail.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderLocalStorageRead) {
  PrerenderTestURL("/prerender/prerender_localstorage_read.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that a local storage write will not cause prerender to fail.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderLocalStorageWrite) {
  PrerenderTestURL("/prerender/prerender_localstorage_write.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that the favicon is properly loaded on prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderFavicon) {
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_favicon.html", FINAL_STATUS_USED, 1);
  NavigateToDestURL();

  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(GetActiveWebContents());
  if (!favicon_driver->FaviconIsValid()) {
    // If the favicon has not been set yet, wait for it to be.
    FaviconUpdateWatcher favicon_update_watcher(GetActiveWebContents());
    favicon_update_watcher.Wait();
  }
  EXPECT_TRUE(favicon_driver->FaviconIsValid());
}

// Checks that when prerendered page is swapped in and the referring page
// neither had set an unload nor it had set a beforeunload handler, the old
// WebContents will not leak.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderOldWebContentsDeleted) {
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  WebContentsDestructionObserver destruction_observer(GetActiveWebContents());
  NavigateToDestURL();
  destruction_observer.Wait();
}

// Checks that when a prerendered page is swapped in to a referring page, the
// unload handlers on the referring page are executed and its WebContents is
// destroyed.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderUnload) {
  set_loader_path("/prerender/prerender_loader_with_unload.html");
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  WebContentsDestructionObserver destruction_observer(GetActiveWebContents());
  NavigateToDestURL();
  // Matches URL in prerender_loader_with_unload.html.
  WaitForRequestCount(src_server()->GetURL("/unload-url"), 1);
  destruction_observer.Wait();
}

// Checks that a beforeunload handler is executed on the referring page when a
// prerendered page is swapped in. Also checks that the WebContents of the
// referring page is destroyed.
// Disabled on Windows. See https://crbug.com/875404.
#if defined(OS_WIN)
#define MAYBE_PrerenderBeforeUnload DISABLED_PrerenderBeforeUnload
#else
#define MAYBE_PrerenderBeforeUnload PrerenderBeforeUnload
#endif
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, MAYBE_PrerenderBeforeUnload) {
  set_loader_path("/prerender/prerender_loader_with_beforeunload.html");
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  WebContentsDestructionObserver destruction_observer(GetActiveWebContents());
  NavigateToDestURL();
  // This URL is requested from prerender_loader_with_beforeunload.html.
  WaitForRequestCount(src_server()->GetURL("/unload-url"), 1);
  destruction_observer.Wait();
}

// Checks that a hanging unload on the referring page of a prerender swap does
// not crash the browser on exit.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHangingUnload) {
  set_loader_path("/prerender/prerender_loader_with_hanging_unload.html");
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that when the history is cleared, prerendering is cancelled and
// prerendering history is cleared.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClearHistory) {
  std::unique_ptr<TestPrerender> prerender =
      PrerenderTestURL("/prerender/prerender_page.html",
                       FINAL_STATUS_CACHE_OR_HISTORY_CLEARED, 1);

  ClearBrowsingData(current_browser(),
                    ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY);
  prerender->WaitForStop();

  // Make sure prerender history was cleared.
  EXPECT_EQ(0U, GetHistoryLength());
}

// Checks that when the cache is cleared, prerenders are cancelled but
// prerendering history is not cleared.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClearCache) {
  std::unique_ptr<TestPrerender> prerender =
      PrerenderTestURL("/prerender/prerender_page.html",
                       FINAL_STATUS_CACHE_OR_HISTORY_CLEARED, 1);

  ClearBrowsingData(current_browser(),
                    content::BrowsingDataRemover::DATA_TYPE_CACHE);
  prerender->WaitForStop();

  // Make sure prerender history was not cleared.  Not a vital behavior, but
  // used to compare with PrerenderClearHistory test.
  EXPECT_EQ(1U, GetHistoryLength());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCancelAll) {
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_page.html", FINAL_STATUS_CANCELLED, 1);

  GetPrerenderManager()->CancelAllPrerenders();
  prerender->WaitForStop();

  EXPECT_FALSE(prerender->contents());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderEvents) {
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_page.html", FINAL_STATUS_CANCELLED, 1);

  GetPrerenderManager()->CancelAllPrerenders();
  prerender->WaitForStop();

  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(0));
  EXPECT_TRUE(DidReceivePrerenderStopEventForLinkNumber(0));
  EXPECT_FALSE(HadPrerenderEventErrors());
}

// Cancels the prerender of a page with its own prerender.  The second prerender
// should never be started.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderCancelPrerenderWithPrerender) {
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_infinite_a.html", FINAL_STATUS_CANCELLED, 1);

  GetPrerenderManager()->CancelAllPrerenders();
  prerender->WaitForStop();

  EXPECT_FALSE(prerender->contents());
}

// Prerendering and history tests.
// The prerendered page is navigated to in several ways [navigate via
// omnibox, click on link, key-modified click to open in background tab, etc],
// followed by a navigation to another page from the prerendered page, followed
// by a back navigation.

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNavigateClickGoBack) {
  PrerenderTestURL("/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
  ClickToNextPageAfterPrerender();
  GoBackToPrerender();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNavigateNavigateGoBack) {
  PrerenderTestURL("/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
  NavigateToNextPageAfterPrerender();
  GoBackToPrerender();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClickClickGoBack) {
  PrerenderTestURL("/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_USED, 1);
  OpenDestURLViaClick();
  ClickToNextPageAfterPrerender();
  GoBackToPrerender();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClickNavigateGoBack) {
  PrerenderTestURL("/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_USED, 1);
  OpenDestURLViaClick();
  NavigateToNextPageAfterPrerender();
  GoBackToPrerender();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClickNewWindow) {
  PrerenderTestURL("/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_APP_TERMINATING, 1);
  OpenDestURLViaClickNewWindow();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClickNewForegroundTab) {
  PrerenderTestURL("/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_APP_TERMINATING, 1);
  OpenDestURLViaClickNewForegroundTab();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       NavigateToPrerenderedPageWhenDevToolsAttached) {
  DisableJavascriptCalls();
  WebContents* web_contents =
      current_browser()->tab_strip_model()->GetActiveWebContents();
  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(web_contents));
  FakeDevToolsClient client;
  agent->AttachClient(&client);
  const char* url = "/prerender/prerender_page.html";
  PrerenderTestURL(url, FINAL_STATUS_DEVTOOLS_ATTACHED, 1);
  NavigateToURLWithDisposition(url, WindowOpenDisposition::CURRENT_TAB, false);
  agent->DetachClient(&client);
}

// Checks that the referrer policy is used when prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderReferrerPolicy) {
  set_loader_path("/prerender/prerender_loader_with_referrer_policy.html");
  PrerenderTestURL("/prerender/prerender_referrer_policy.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that the referrer policy is used when prerendering on HTTPS.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderSSLReferrerPolicy) {
  UseHttpsSrcServer();
  set_loader_path("/prerender/prerender_loader_with_referrer_policy.html");
  PrerenderTestURL("/prerender/prerender_referrer_policy.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that the referrer policy is used when prerendering is cancelled.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCancelReferrerPolicy) {
  std::unique_ptr<TestContentBrowserClient> test_content_browser_client(
      new TestContentBrowserClient);
  content::ContentBrowserClient* original_browser_client =
      content::SetBrowserClientForTesting(test_content_browser_client.get());

  set_loader_path("/prerender/prerender_loader_with_referrer_policy.html");
  PrerenderTestURL("/prerender/prerender_referrer_policy.html",
                   FINAL_STATUS_CANCELLED, 1);
  OpenDestURLViaClick();

  bool display_test_result = false;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send(DidDisplayPass())",
      &display_test_result));
  EXPECT_TRUE(display_test_result);

  content::SetBrowserClientForTesting(original_browser_client);
}

// Test interaction of the webNavigation and tabs API with prerender.
class PrerenderBrowserTestWithExtensions : public PrerenderBrowserTest,
                                           public extensions::ExtensionApiTest {
 public:
  PrerenderBrowserTestWithExtensions() {
    // The individual tests start the test server through ExtensionApiTest, so
    // the port number can be passed through to the extension.
    set_autostart_test_server(false);
  }

  void SetUp() override { PrerenderBrowserTest::SetUp(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    PrerenderBrowserTest::SetUpInProcessBrowserTestFixture();
    extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    PrerenderBrowserTest::TearDownInProcessBrowserTestFixture();
    extensions::ExtensionApiTest::TearDownInProcessBrowserTestFixture();
  }

  void TearDownOnMainThread() override {
    PrerenderBrowserTest::TearDownOnMainThread();
    extensions::ExtensionApiTest::TearDownOnMainThread();
  }

  void SetUpOnMainThread() override {
    PrerenderBrowserTest::SetUpOnMainThread();
    extensions::ExtensionApiTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTestWithExtensions, WebNavigation) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  extensions::FrameNavigationState::set_allow_extension_scheme(true);

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionTest("webnavigation/prerender")) << message_;

  extensions::ResultCatcher catcher;

  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  ChannelDestructionWatcher channel_close_watcher;
  channel_close_watcher.WatchChannel(browser()
                                         ->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetMainFrame()
                                         ->GetProcess());
  NavigateToDestURL();
  channel_close_watcher.WaitForChannelClose();

  ASSERT_TRUE(IsEmptyPrerenderLinkManager());
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTestWithExtensions, TabsApi) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  extensions::FrameNavigationState::set_allow_extension_scheme(true);

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionTest("tabs/on_replaced")) << message_;

  extensions::ResultCatcher catcher;

  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  ChannelDestructionWatcher channel_close_watcher;
  channel_close_watcher.WatchChannel(browser()
                                         ->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetMainFrame()
                                         ->GetProcess());
  NavigateToDestURL();
  channel_close_watcher.WaitForChannelClose();

  ASSERT_TRUE(IsEmptyPrerenderLinkManager());
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Checks that non-http/https/chrome-extension subresource cancels the
// prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderCancelSubresourceUnsupportedScheme) {
  GURL image_url = GURL("invalidscheme://www.google.com/test.jpg");
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", image_url.spec()));
  std::string replacement_path;
  net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_image.html", replacement_text,
      &replacement_path);
  // Disable load event checks because they race with cancellation.
  DisableLoadEventCheck();
  PrerenderTestURL(replacement_path, FINAL_STATUS_UNSUPPORTED_SCHEME, 0);
}

// Ensure that about:blank is permitted for any subresource.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderAllowAboutBlankSubresource) {
  GURL image_url = GURL("about:blank");
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", image_url.spec()));
  std::string replacement_path;
  net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_image.html", replacement_text,
      &replacement_path);
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that non-http/https/chrome-extension subresource cancels the prerender
// on redirect.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderCancelSubresourceRedirectUnsupportedScheme) {
  GURL image_url = embedded_test_server()->GetURL(
      CreateServerRedirect("invalidscheme://www.google.com/test.jpg"));
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", image_url.spec()));
  std::string replacement_path;
  net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_image.html", replacement_text,
      &replacement_path);
  // Disable load event checks because they race with cancellation.
  DisableLoadEventCheck();
  PrerenderTestURL(replacement_path, FINAL_STATUS_UNSUPPORTED_SCHEME, 0);
}

// Checks that chrome-extension subresource does not cancel the prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderKeepSubresourceExtensionScheme) {
  GURL image_url = GURL("chrome-extension://abcdefg/test.jpg");
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", image_url.spec()));
  std::string replacement_path;
  net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_image.html", replacement_text,
      &replacement_path);
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that redirect to chrome-extension subresource does not cancel the
// prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderKeepSubresourceRedirectExtensionScheme) {
  GURL image_url = embedded_test_server()->GetURL(
      CreateServerRedirect("chrome-extension://abcdefg/test.jpg"));
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", image_url.spec()));
  std::string replacement_path;
  net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_image.html", replacement_text,
      &replacement_path);
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that non-http/https main page redirects cancel the prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderCancelMainFrameRedirectUnsupportedScheme) {
  // Disable load event checks because they race with cancellation.
  DisableLoadEventCheck();
  GURL url = embedded_test_server()->GetURL(
      CreateServerRedirect("invalidscheme://www.google.com/test.html"));
  PrerenderTestURL(url, FINAL_STATUS_UNSUPPORTED_SCHEME, 0);
}

// Checks that media source video loads are deferred on prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHTML5MediaSourceVideo) {
  PrerenderTestURL("/prerender/prerender_html5_video_media_source.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
  WaitForASCIITitle(GetActiveWebContents(), kPassTitle);
}

// Checks that a prerender that creates an audio stream (via a WebAudioDevice)
// is cancelled.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderWebAudioDevice) {
  DisableLoadEventCheck();
  PrerenderTestURL("/prerender/prerender_web_audio_device.html",
                   FINAL_STATUS_CREATING_AUDIO_STREAM, 0);
}

// Checks that prerenders do not swap in to WebContents being captured.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCapturedWebContents) {
  PrerenderTestURL("/prerender/prerender_page.html",
                   FINAL_STATUS_PAGE_BEING_CAPTURED, 1);
  WebContents* web_contents = GetActiveWebContents();
  web_contents->IncrementCapturerCount(gfx::Size());
  NavigateToDestURLWithDisposition(WindowOpenDisposition::CURRENT_TAB, false);
  web_contents->DecrementCapturerCount();
}

// Checks that prerenders are aborted on cross-process navigation from
// a client redirect.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderCrossProcessClientRedirect) {
  // Cross-process navigation logic for renderer-initiated navigations
  // is partially controlled by the renderer, namely
  // ChromeContentRendererClient. This test instead relies on the Web
  // Store triggering such navigations.
  GURL webstore_url = extension_urls::GetWebstoreLaunchURL();

  // Mock out requests to the Web Store.
  base::FilePath file(GetTestPath("prerender_page.html"));
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&CreateMockInterceptorOnIO, webstore_url, file));

  PrerenderTestURL(CreateClientRedirect(webstore_url.spec()),
                   FINAL_STATUS_OPEN_URL, 1);
}

// Checks that a deferred redirect to an image is not loaded until the page is
// visible. Also test the right histogram events are emitted in this case.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDeferredImage) {
  DisableJavascriptCalls();

  // The prerender will not completely load until after the swap, so wait for a
  // title change before calling DidPrerenderPass.
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_deferred_image.html", FINAL_STATUS_USED, 0);
  WaitForASCIITitle(prerender->contents()->prerender_contents(), kReadyTitle);
  EXPECT_EQ(1, GetPrerenderDomContentLoadedEventCountForLinkNumber(0));
  EXPECT_TRUE(DidPrerenderPass(prerender->contents()->prerender_contents()));
  EXPECT_EQ(0, prerender->number_of_loads());

  // Swap.
  NavigationOrSwapObserver swap_observer(current_browser()->tab_strip_model(),
                                         GetActiveWebContents());
  ui_test_utils::NavigateToURLWithDisposition(
      current_browser(), dest_url(), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  swap_observer.Wait();

  // The prerender never observes the final load.
  EXPECT_EQ(0, prerender->number_of_loads());

  // Now check DidDisplayPass.
  EXPECT_TRUE(DidDisplayPass(GetActiveWebContents()));
}

// Checks that a deferred redirect to an image is not loaded until the
// page is visible, even after another redirect.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderDeferredImageAfterRedirect) {
  DisableJavascriptCalls();

  // The prerender will not completely load until after the swap, so wait for a
  // title change before calling DidPrerenderPass.
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_deferred_image.html", FINAL_STATUS_USED, 0);
  WaitForASCIITitle(prerender->contents()->prerender_contents(), kReadyTitle);
  EXPECT_TRUE(DidPrerenderPass(prerender->contents()->prerender_contents()));
  EXPECT_EQ(0, prerender->number_of_loads());

  // Swap.
  NavigationOrSwapObserver swap_observer(current_browser()->tab_strip_model(),
                                         GetActiveWebContents());
  ui_test_utils::NavigateToURLWithDisposition(
      current_browser(), dest_url(), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  swap_observer.Wait();

  // The prerender never observes the final load.
  EXPECT_EQ(0, prerender->number_of_loads());

  // Now check DidDisplayPass.
  EXPECT_TRUE(DidDisplayPass(GetActiveWebContents()));
}

// Checks that deferred redirects in the main frame are followed.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDeferredMainFrame) {
  DisableJavascriptCalls();
  PrerenderTestURL("/prerender/image-deferred.png", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that deferred redirects in the main frame are followed, even
// with a double-redirect.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderDeferredMainFrameAfterRedirect) {
  DisableJavascriptCalls();
  PrerenderTestURL(CreateServerRedirect("/prerender/image-deferred.png"),
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that deferred redirects in a synchronous XHR abort the prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDeferredSynchronousXHR) {
  // Disable load event checks because they race with cancellation.
  DisableLoadEventCheck();
  PrerenderTestURL("/prerender/prerender_deferred_sync_xhr.html",
                   FINAL_STATUS_BAD_DEFERRED_REDIRECT, 0);
  ui_test_utils::NavigateToURL(current_browser(), dest_url());
}

// Checks that prerenders are not swapped for navigations with extra headers.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderExtraHeadersNoSwap) {
  PrerenderTestURL("/prerender/prerender_page.html",
                   FINAL_STATUS_APP_TERMINATING, 1);

  content::OpenURLParams params(dest_url(), Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_TYPED, false);
  params.extra_headers = "X-Custom-Header: 42\r\n";
  NavigateToURLWithParams(params, false);
}

// Checks that prerenders are not swapped for navigations with browser-initiated
// POST data.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderBrowserInitiatedPostNoSwap) {
  PrerenderTestURL("/prerender/prerender_page.html",
                   FINAL_STATUS_APP_TERMINATING, 1);

  std::string post_data = "DATA";
  content::OpenURLParams params(dest_url(), Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_TYPED, false);
  params.uses_post = true;
  params.post_data = network::ResourceRequestBody::CreateFromBytes(
      post_data.data(), post_data.size());
  NavigateToURLWithParams(params, false);
}

// Checks that the prerendering of a page is canceled correctly when the
// prerendered page tries to make a second navigation entry.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNewNavigationEntry) {
  PrerenderTestURL("/prerender/prerender_new_entry.html",
                   FINAL_STATUS_NEW_NAVIGATION_ENTRY, 1);
}

// Attempt a swap-in in a new tab. The session storage doesn't match, so it
// should not swap.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPageNewTab) {
  PrerenderTestURL("/prerender/prerender_page.html",
                   FINAL_STATUS_APP_TERMINATING, 1);

  // Open a new tab to navigate in.
  ui_test_utils::NavigateToURLWithDisposition(
      current_browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Now navigate in the new tab.
  NavigateToDestURLWithDisposition(WindowOpenDisposition::CURRENT_TAB, false);
}

// Checks that prerenders honor |should_replace_current_entry|.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderReplaceCurrentEntry) {
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  content::OpenURLParams params(dest_url(), Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_TYPED, false);
  params.should_replace_current_entry = true;
  NavigateToURLWithParams(params, false);

  const NavigationController& controller =
      GetActiveWebContents()->GetController();
  // First entry is about:blank, second is prerender_page.html.
  EXPECT_FALSE(controller.GetPendingEntry());
  ASSERT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(GURL(url::kAboutBlankURL), controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(dest_url(), controller.GetEntryAtIndex(1)->GetURL());
}

// Checks that <a ping> requests are not dropped in prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPing) {
  // Count hits to a certain URL.
  const GURL kPingURL(src_server()->GetURL("/echo"));
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  OpenDestURLViaClickPing(kPingURL);

  WaitForRequestCount(kPingURL, 1);
}

// Checks that a prerender which calls window.close() on itself is aborted.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderWindowClose) {
  DisableLoadEventCheck();
  PrerenderTestURL("/prerender/prerender_window_close.html",
                   FINAL_STATUS_CLOSED, 0);
}

// Tests interaction between prerender and POST (i.e. POST request should still
// be made and POST data should not be dropped when the POST target is the same
// as a prerender link).
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, HttpPost) {
  // Expect that the prerendered contents won't get used (i.e. the prerendered
  // content should be destroyed when the test closes the browser under test).
  std::unique_ptr<TestPrerender> prerender =
      ExpectPrerender(FINAL_STATUS_APP_TERMINATING);

  // Navigate to a page containing a form that targets a prerendered link.
  GURL main_url(embedded_test_server()->GetURL(
      "/prerender/form_that_posts_to_prerendered_echoall.html"));
  ui_test_utils::NavigateToURL(current_browser(), main_url);

  // Wait for the prerender to be ready.
  prerender->WaitForStart();
  prerender->WaitForLoads(1);
  EXPECT_THAT(prerender->contents()->prerender_url().spec(),
              ::testing::MatchesRegex("^http://127.0.0.1.*:\\d+/echoall$"));

  // Submit the form.
  content::WebContents* web_contents =
      current_browser()->tab_strip_model()->GetActiveWebContents();
  TestNavigationObserver form_post_observer(web_contents, 1);
  EXPECT_TRUE(
      ExecuteScript(web_contents, "document.getElementById('form').submit();"));
  form_post_observer.Wait();

  // Verify that we arrived at the expected location.
  GURL target_url(embedded_test_server()->GetURL("/echoall"));
  EXPECT_EQ(target_url, web_contents->GetLastCommittedURL());

  // Verify that POST body was correctly passed to the server and ended up in
  // the body of the page (i.e. verify that we haven't used the prerendered
  // page that doesn't contain the POST body).
  std::string body;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send("
      "document.getElementsByTagName('pre')[0].innerText);",
      &body));
  EXPECT_EQ("text=value\n", body);
}

// Prerenders a page that tries to automatically sign user in via the Credential
// Manager API. The page should be killed.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, AutosigninInPrerenderer) {
  // Set up a credential in the password store.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    PasswordStoreFactory::GetInstance()->SetTestingFactory(
        current_browser()->profile(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                content::BrowserContext, password_manager::TestPasswordStore>));
  }
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              current_browser()->profile(),
              ServiceAccessType::IMPLICIT_ACCESS).get());
  autofill::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = base::ASCIIToUTF16("password");
  signin_form.username_value = base::ASCIIToUTF16("user");
  signin_form.origin = embedded_test_server()->base_url();
  signin_form.skip_zero_click = false;
  password_store->AddLogin(signin_form);
  // Enable 'auto signin' for the profile.
  password_bubble_experiment::RecordAutoSignInPromptFirstRunExperienceWasShown(
      browser()->profile()->GetPrefs());

  // Intercept the successful landing page where a signed in user ends up.
  // It should never load as the API is suppressed.
  GURL done_url = embedded_test_server()->GetURL("/password/done.html");
  base::FilePath empty_file = ui_test_utils::GetTestFilePath(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("empty.html")));
  RequestCounter done_counter;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&CreateCountingInterceptorOnIO, done_url, empty_file,
                     done_counter.AsWeakPtr()));
  // Loading may finish or be interrupted. The final result is important only.
  DisableLoadEventCheck();
  // TestPrenderContents is always created before the Autosignin JS can run, so
  // waiting for PrerenderContents to stop should be reliable.
  PrerenderTestURL("/password/autosignin.html",
                   FINAL_STATUS_CREDENTIAL_MANAGER_API, 0);
  EXPECT_EQ(0, done_counter.count());
}

// Checks that the requests from a prerender are IDLE priority before the swap
// (except on Android), but normal priority after the swap.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ResourcePriority) {
  GURL before_swap_url = embedded_test_server()->GetURL(kPrefetchJpeg);
  GURL after_swap_url = embedded_test_server()->GetURL("/prerender/image.png");
  GURL main_page_url =
      GetURLWithReplacement("/prerender/prerender_with_image.html",
                            "REPLACE_WITH_IMAGE_URL", kPrefetchJpeg);
  net::RequestPriority before_swap_priority = net::THROTTLED;
  net::RequestPriority after_swap_priority = net::THROTTLED;

  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url == before_swap_url)
          before_swap_priority = params->url_request.priority;
        else if (params->url_request.url == after_swap_url)
          after_swap_priority = params->url_request.priority;
        return false;
      }));

  // Start the prerender.
  PrerenderTestURL(main_page_url, FINAL_STATUS_USED, 1);

  // Check priority before swap.
  WaitForRequestCount(before_swap_url, 1);
#if defined(OS_ANDROID)
  EXPECT_GT(before_swap_priority, net::IDLE);
#else
  EXPECT_EQ(net::IDLE, before_swap_priority);
#endif

  // Swap.
  NavigateToDestURL();

  // Check priority after swap.
  GetActiveWebContents()->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16(
          "var img=new Image(); img.src='/prerender/image.png'"));
  WaitForRequestCount(after_swap_url, 1);
  EXPECT_NE(net::IDLE, after_swap_priority);
}

namespace {

class HangingURLLoader : public network::mojom::URLLoader {
 public:
  explicit HangingURLLoader(network::mojom::URLLoaderClientPtr client)
      : client_(std::move(client)) {}
  ~HangingURLLoader() override {}
  // mojom::URLLoader implementation:
  void FollowRedirect(const base::Optional<std::vector<std::string>>&
                          to_be_removed_request_headers,
                      const base::Optional<net::HttpRequestHeaders>&
                          modified_request_headers) override {}
  void ProceedWithResponse() override {}
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    if (set_priority_callback_)
      std::move(set_priority_callback_).Run(priority);
  }
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

  using SetPriorityCallback = base::OnceCallback<void(net::RequestPriority)>;
  void set_set_priority_callback(SetPriorityCallback callback) {
    set_priority_callback_ = std::move(callback);
  }

 private:
  network::mojom::URLLoaderClientPtr client_;
  SetPriorityCallback set_priority_callback_;
};

}  // namespace

// Checks that a request started before the swap gets its original priority back
// after the swap.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ResourcePriorityOverlappingSwap) {
  GURL image_url = embedded_test_server()->GetURL(kPrefetchJpeg);
  GURL main_page_url =
      GetURLWithReplacement("/prerender/prerender_with_image.html",
                            "REPLACE_WITH_IMAGE_URL", kPrefetchJpeg);

  net::RequestPriority priority = net::THROTTLED;
  base::RunLoop load_image_run_loop, set_priority_run_loop;
  content::URLLoaderInterceptor interceptor(
      base::BindLambdaForTesting(
          [&](content::URLLoaderInterceptor::RequestParams* params) {
            if (params->url_request.url == image_url) {
              // Check priority before swap.
              priority = params->url_request.priority;
              load_image_run_loop.QuitClosure().Run();

              auto loader =
                  std::make_unique<HangingURLLoader>(std::move(params->client));
              loader->set_set_priority_callback(base::BindLambdaForTesting(
                  [&](net::RequestPriority request_priority) {
                    priority = request_priority;
                    set_priority_run_loop.QuitClosure().Run();
                  }));
              mojo::MakeStrongBinding(std::move(loader),
                                      std::move(params->request));
              return true;
            }
            return false;
          }));

  // The prerender will hang on the image resource, can't run the usual checks.
  DisableLoadEventCheck();
  DisableJavascriptCalls();
  // Start the prerender.
  PrerenderTestURL(main_page_url, FINAL_STATUS_USED, 0);

  // Check priority before swap.
  load_image_run_loop.Run();
#if defined(OS_ANDROID)
  EXPECT_GT(priority, net::IDLE);
#else
  EXPECT_EQ(net::IDLE, priority);
#endif

  // Swap. Cannot use NavigateToDestURL, because it waits for the load to
  // complete, but the resource is still hung.
  current_browser()->OpenURL(content::OpenURLParams(
      dest_url(), Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_TYPED, false));

  // Check priority after swap. The test may timeout in case of failure.
  set_priority_run_loop.Run();
  EXPECT_GT(priority, net::IDLE);
}

// Flaky on chromium.linux/Linux Tests (dbg). https://crbug.com/832597
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_FirstContentfulPaintTimingSimple) {
  GetPrerenderManager()->DisablePageLoadMetricsObserverForTesting();
  base::SimpleTestTickClock* clock = OverridePrerenderManagerTimeTicks();
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  base::TimeTicks load_start = clock->NowTicks();
  clock->Advance(base::TimeDelta::FromSeconds(1));
  NavigateToDestURL();

  PrerenderPageLoadMetricsObserver observer(GetPrerenderManager(),
                                            GetActiveWebContents());
  observer.SetNavigationStartTicksForTesting(load_start);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);  // Non-null time.
  timing.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(2654);
  PopulateRequiredTimingFields(&timing);
  observer.OnFirstContentfulPaintInPage(timing,
                                        GenericPageLoadExtraInfo(dest_url()));

  histogram_tester().ExpectTotalCount(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Visible", 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Visible", 1654, 1);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, FirstContentfulPaintTimingReuse) {
  GetPrerenderManager()->DisablePageLoadMetricsObserverForTesting();
  base::SimpleTestTickClock* clock = OverridePrerenderManagerTimeTicks();

  GURL url = embedded_test_server()->GetURL("/prerender/prerender_page.html");
  base::RunLoop hanging_request_waiter;
  CreateHangingFirstRequestInterceptor(url, GetTestPath("prerender_page.html"),
                                       hanging_request_waiter.QuitClosure());
  // As this load will be canceled, it is not waited for, and hence no
  // javascript is executed.
  DisableJavascriptCalls();
  PrerenderTestURL(url, FINAL_STATUS_CANCELLED, 0);
  hanging_request_waiter.Run();

  // This prerender cancels and reuses the first.
  clock->Advance(base::TimeDelta::FromSeconds(1));
  base::TimeTicks load_start = clock->NowTicks();
  EnableJavascriptCalls();
  PrerenderTestURL(url, FINAL_STATUS_USED, 1);
  clock->Advance(base::TimeDelta::FromSeconds(1));

  NavigateToDestURL();
  PrerenderPageLoadMetricsObserver observer(GetPrerenderManager(),
                                            GetActiveWebContents());
  observer.SetNavigationStartTicksForTesting(load_start);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);  // Non-null time.
  timing.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(2361);
  PopulateRequiredTimingFields(&timing);
  observer.OnFirstContentfulPaintInPage(timing,
                                        GenericPageLoadExtraInfo(dest_url()));

  histogram_tester().ExpectTotalCount(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Visible", 1);
  // If the first prerender had been used, the perceived TTFCP would have been
  // under a second: 2362ms - 2 sec worth of Advance().
  histogram_tester().ExpectUniqueSample(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Visible", 1361, 1);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       FirstContentfulPaintTimingTimeout) {
  GetPrerenderManager()->DisablePageLoadMetricsObserverForTesting();
  base::SimpleTestTickClock* clock = OverridePrerenderManagerTimeTicks();

  // Make the first prerender time out.
  base::TimeDelta time_out_delta =
      GetPrerenderManager()->config().time_to_live +
      base::TimeDelta::FromSeconds(10);
  SetMidLoadClockAdvance(clock, time_out_delta);

  GURL url = embedded_test_server()->GetURL("/prerender/prerender_page.html");
  PrerenderTestURL(url, FINAL_STATUS_TIMED_OUT, 1);

  ClearMidLoadClock();
  base::TimeTicks load_start = clock->NowTicks();
  PrerenderTestURL(url, FINAL_STATUS_USED, 1);

  clock->Advance(base::TimeDelta::FromSeconds(1));
  NavigateToDestURL();

  PrerenderPageLoadMetricsObserver observer(GetPrerenderManager(),
                                            GetActiveWebContents());
  observer.SetNavigationStartTicksForTesting(load_start);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);  // Non-null time.
  timing.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(2361);
  PopulateRequiredTimingFields(&timing);
  observer.OnFirstContentfulPaintInPage(timing,
                                        GenericPageLoadExtraInfo(dest_url()));

  histogram_tester().ExpectTotalCount(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Visible", 1);
  // If the timed out had been used, the perceived TTFCP would have been
  // negative.
  histogram_tester().ExpectUniqueSample(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Visible", 1361, 1);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       FirstContentfulPaintTimingNoCommit) {
  GetPrerenderManager()->DisablePageLoadMetricsObserverForTesting();
  base::SimpleTestTickClock* clock = OverridePrerenderManagerTimeTicks();

  GURL url = embedded_test_server()->GetURL("/prerender/prerender_page.html");
  base::FilePath url_file = ui_test_utils::GetTestFilePath(
      base::FilePath(),
      base::FilePath(FILE_PATH_LITERAL("prerender/prerender_page.html")));

  base::RunLoop prerender_start_loop;
  CreateHangingFirstRequestInterceptor(url, url_file,
                                       prerender_start_loop.QuitClosure());
  // As this load is uncommitted, it is not waited for, and hence no
  // javascript is executed.
  DisableJavascriptCalls();
  PrerenderTestURL(url, FINAL_STATUS_NAVIGATION_UNCOMMITTED, 0);
  prerender_start_loop.Run();

  clock->Advance(base::TimeDelta::FromSeconds(1));
  NavigateToDestURLWithDisposition(WindowOpenDisposition::CURRENT_TAB, false);

  PrerenderPageLoadMetricsObserver observer(GetPrerenderManager(),
                                            GetActiveWebContents());
  observer.SetNavigationStartTicksForTesting(clock->NowTicks());

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);  // Non-null time.
  timing.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(2362);
  PopulateRequiredTimingFields(&timing);
  observer.OnFirstContentfulPaintInPage(timing,
                                        GenericPageLoadExtraInfo(dest_url()));

  histogram_tester().ExpectTotalCount(
      "Prerender.none_PrefetchTTFCP.Warm.Cacheable.Visible", 0);
  histogram_tester().ExpectTotalCount(
      "Prerender.none_PerceivedTTFCPRecorded.Visible", 1);

  // Check that the prerender didn't happen with a defined origin.
  histogram_tester().ExpectTotalCount(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Visible", 0);
  histogram_tester().ExpectTotalCount(
      "Prerender.websame_PerceivedTTFCPRecorded.Visible", 0);

  // A FCP is fired by the observer, but as a swap did not occur the perceived
  // time cannot be calculated, and an unrecorded perceived FCP time histogram
  // entry is made.
  histogram_tester().ExpectUniqueSample(
      "Prerender.none_PerceivedTTFCPRecorded.Visible", 0, 1);
}

// Disabled on ChromeOS due to flakiness. See https://crbug.com/808578.
#if defined(OS_CHROMEOS)
#define MAYBE_FirstContentfulPaintTimingTwoPages \
  DISABLED_FirstContentfulPaintTimingTwoPages
#else
#define MAYBE_FirstContentfulPaintTimingTwoPages \
  FirstContentfulPaintTimingTwoPages
#endif
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MAYBE_FirstContentfulPaintTimingTwoPages) {
  GetPrerenderManager()->DisablePageLoadMetricsObserverForTesting();
  base::SimpleTestTickClock* clock = OverridePrerenderManagerTimeTicks();

  // As this load will be canceled, it is not waited for, and hence no
  // javascript is executed.
  DisableJavascriptCalls();
  // First prerender a different page from the usual target.
  PrerenderTestURL("/prerender/prefetch_page.html", FINAL_STATUS_CANCELLED, 0);

  clock->Advance(base::TimeDelta::FromSeconds(1));
  base::TimeTicks load_start = clock->NowTicks();
  EnableJavascriptCalls();
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  clock->Advance(base::TimeDelta::FromSeconds(1));
  NavigateToDestURL();

  PrerenderPageLoadMetricsObserver observer(GetPrerenderManager(),
                                            GetActiveWebContents());
  observer.SetNavigationStartTicksForTesting(load_start);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);  // Non-null time.
  // The FCP time should end up on the edge of the bucket.
  timing.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(2654);
  PopulateRequiredTimingFields(&timing);
  observer.OnFirstContentfulPaintInPage(timing,
                                        GenericPageLoadExtraInfo(dest_url()));

  histogram_tester().ExpectTotalCount(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Visible", 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Visible", 1654, 1);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, FirstContentfulPaintHidden) {
  GetPrerenderManager()->DisablePageLoadMetricsObserverForTesting();
  base::SimpleTestTickClock* clock = OverridePrerenderManagerTimeTicks();

  base::TimeTicks load_start = clock->NowTicks();
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  clock->Advance(base::TimeDelta::FromSeconds(1));
  NavigateToDestURL();

  PrerenderPageLoadMetricsObserver observer(GetPrerenderManager(),
                                            GetActiveWebContents());
  observer.SetNavigationStartTicksForTesting(load_start);

  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnHidden(page_load_metrics::mojom::PageLoadTiming(),
                              GenericPageLoadExtraInfo(dest_url())));

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);  // Non-null time.
  timing.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(2654);
  PopulateRequiredTimingFields(&timing);
  observer.OnFirstContentfulPaintInPage(timing,
                                        GenericPageLoadExtraInfo(dest_url()));

  histogram_tester().ExpectTotalCount(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Hidden", 1);
  histogram_tester().ExpectUniqueSample(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Hidden", 1654, 1);
}

// Flaky on chromium.linux/Linux Tests (dbg). https://crbug.com/832597
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_FirstContentfulPaintHiddenNoCommit) {
  GetPrerenderManager()->DisablePageLoadMetricsObserverForTesting();
  base::SimpleTestTickClock* clock = OverridePrerenderManagerTimeTicks();

  GURL url = embedded_test_server()->GetURL("/prerender/prerender_page.html");
  base::FilePath url_file = ui_test_utils::GetTestFilePath(
      base::FilePath(),
      base::FilePath(FILE_PATH_LITERAL("prerender/prerender_page.html")));

  base::RunLoop prerender_start_loop;
  CreateHangingFirstRequestInterceptor(url, url_file,
                                       prerender_start_loop.QuitClosure());

  // As this load is uncommitted, it is not waited for, and hence no
  // javascript is executed.
  DisableJavascriptCalls();
  PrerenderTestURL(url, FINAL_STATUS_NAVIGATION_UNCOMMITTED, 0);
  prerender_start_loop.Run();

  clock->Advance(base::TimeDelta::FromSeconds(1));
  NavigateToDestURLWithDisposition(WindowOpenDisposition::CURRENT_TAB, false);

  PrerenderPageLoadMetricsObserver observer(GetPrerenderManager(),
                                            GetActiveWebContents());
  observer.SetNavigationStartTicksForTesting(clock->NowTicks());

  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer.OnHidden(page_load_metrics::mojom::PageLoadTiming(),
                              GenericPageLoadExtraInfo(dest_url())));

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(1);  // Non-null time.
  timing.paint_timing->first_contentful_paint =
      base::TimeDelta::FromMilliseconds(2362);
  PopulateRequiredTimingFields(&timing);
  observer.OnFirstContentfulPaintInPage(timing,
                                        GenericPageLoadExtraInfo(dest_url()));

  histogram_tester().ExpectTotalCount(
      "Prerender.none_PrefetchTTFCP.Warm.Cacheable.Hidden", 0);
  histogram_tester().ExpectTotalCount(
      "Prerender.none_PerceivedTTFCPRecorded.Hidden", 1);

  // Check that the prerender didn't happen with a defined origin.
  histogram_tester().ExpectTotalCount(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Hidden", 0);
  histogram_tester().ExpectTotalCount(
      "Prerender.websame_PerceivedTTFCPRecorded.Hidden", 0);

  // A FCP is fired by the observer, but the manager should detect that the
  // perceived time is not set and so update the following histogram.
  histogram_tester().ExpectUniqueSample(
      "Prerender.none_PerceivedTTFCPRecorded.Hidden", 0, 1);
}

// When instantiated, mocks out the global text-to-speech engine with something
// that emulates speaking any phrase for the duration of 0ms.
class TtsPlatformMock : public TtsPlatformImpl {
 public:
  TtsPlatformMock() : speaking_requested_(false) {
    TtsController::GetInstance()->SetPlatformImpl(this);
  }

  ~TtsPlatformMock() override {
    TtsController::GetInstance()->SetPlatformImpl(
        TtsPlatformImpl::GetInstance());
  }

  bool speaking_requested() { return speaking_requested_; }

  // TtsPlatformImpl:

  bool PlatformImplAvailable() override { return true; }

  bool Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const VoiceData& voice,
             const UtteranceContinuousParameters& params) override {
    speaking_requested_ = true;
    // Dispatch the end of speaking back to the page.
    TtsController::GetInstance()->OnTtsEvent(utterance_id, TTS_EVENT_END,
                                             static_cast<int>(utterance.size()),
                                             std::string());
    return true;
  }

  bool StopSpeaking() override { return true; }

  bool IsSpeaking() override { return false; }

  void GetVoices(std::vector<VoiceData>* out_voices) override {
    out_voices->push_back(VoiceData());
    VoiceData& voice = out_voices->back();
    voice.native = true;
    voice.name = "TtsPlatformMock";
    voice.events.insert(TTS_EVENT_END);
  }

  void Pause() override {}

  void Resume() override {}

 private:
  bool speaking_requested_;
};

// Checks that text-to-speech is not called from prerenders that did not reach
// the visible state. Disabled until the http://crbug.com/520275 is fixed.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_PrerenderSpeechSynthesis) {
  TtsPlatformMock tts_platform_mock;
  PrerenderTestURL("/prerender/prerender_speech_synthesis.html",
                   FINAL_STATUS_JAVASCRIPT_ALERT, 1);
  EXPECT_FALSE(tts_platform_mock.speaking_requested());
}

class PrerenderIncognitoBrowserTest : public PrerenderBrowserTest {
 public:
  void SetUpOnMainThread() override {
    Profile* normal_profile = current_browser()->profile();
    set_browser(OpenURLOffTheRecord(normal_profile, GURL("about:blank")));
    PrerenderBrowserTest::SetUpOnMainThread();
  }
};

// Checks that prerendering works in incognito mode.
IN_PROC_BROWSER_TEST_F(PrerenderIncognitoBrowserTest, PrerenderIncognito) {
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that prerenders are aborted when an incognito profile is closed.
IN_PROC_BROWSER_TEST_F(PrerenderIncognitoBrowserTest,
                       PrerenderIncognitoClosed) {
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_page.html", FINAL_STATUS_PROFILE_DESTROYED, 1);
  current_browser()->window()->Close();
  prerender->WaitForStop();
}

class PrerenderOmniboxBrowserTest : public PrerenderBrowserTest {
 public:
  LocationBar* GetLocationBar() {
    return current_browser()->window()->GetLocationBar();
  }

  OmniboxView* GetOmniboxView() {
    return GetLocationBar()->GetOmniboxView();
  }

  void WaitForAutocompleteDone(OmniboxView* omnibox_view) {
    AutocompleteController* controller =
        omnibox_view->model()->popup_model()->autocomplete_controller();
    while (!controller->done()) {
      content::WindowedNotificationObserver ready_observer(
          chrome::NOTIFICATION_AUTOCOMPLETE_CONTROLLER_RESULT_READY,
          content::Source<AutocompleteController>(controller));
      ready_observer.Wait();
    }
  }

  predictors::AutocompleteActionPredictor* GetAutocompleteActionPredictor() {
    Profile* profile = current_browser()->profile();
    return predictors::AutocompleteActionPredictorFactory::GetForProfile(
        profile);
  }

  std::unique_ptr<TestPrerender> StartOmniboxPrerender(
      const GURL& url,
      FinalStatus expected_final_status) {
    std::unique_ptr<TestPrerender> prerender =
        ExpectPrerender(expected_final_status);
    WebContents* web_contents = GetActiveWebContents();
    GetAutocompleteActionPredictor()->StartPrerendering(
        url,
        web_contents->GetController().GetDefaultSessionStorageNamespace(),
        gfx::Size(50, 50));
    prerender->WaitForStart();
    return prerender;
  }
};

// Checks that closing the omnibox popup cancels an omnibox prerender.
// http://crbug.com/395152
IN_PROC_BROWSER_TEST_F(PrerenderOmniboxBrowserTest,
                       DISABLED_PrerenderOmniboxCancel) {
  // Fake an omnibox prerender.
  std::unique_ptr<TestPrerender> prerender = StartOmniboxPrerender(
      embedded_test_server()->GetURL("/empty.html"), FINAL_STATUS_CANCELLED);

  // Revert the location bar. This should cancel the prerender.
  GetLocationBar()->Revert();
  prerender->WaitForStop();
}

// Checks that accepting omnibox input abandons an omnibox prerender.
// http://crbug.com/394592
IN_PROC_BROWSER_TEST_F(PrerenderOmniboxBrowserTest,
                       DISABLED_PrerenderOmniboxAbandon) {
  // Set the abandon timeout to something high so it does not introduce
  // flakiness if the prerender times out before the test completes.
  GetPrerenderManager()->mutable_config().abandon_time_to_live =
      base::TimeDelta::FromDays(999);

  // Enter a URL into the Omnibox.
  OmniboxView* omnibox_view = GetOmniboxView();
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(base::UTF8ToUTF16(
      embedded_test_server()->GetURL("/empty.html?1").spec()));
  omnibox_view->OnAfterPossibleChange(true);
  WaitForAutocompleteDone(omnibox_view);

  // Fake an omnibox prerender for a different URL.
  std::unique_ptr<TestPrerender> prerender =
      StartOmniboxPrerender(embedded_test_server()->GetURL("/empty.html?2"),
                            FINAL_STATUS_APP_TERMINATING);

  // The final status may be either FINAL_STATUS_APP_TERMINATING or
  // FINAL_STATUS_CANCELLED. Although closing the omnibox will not cancel an
  // abandoned prerender, the AutocompleteActionPredictor will cancel the
  // predictor on destruction.
  prerender->contents()->set_skip_final_checks(true);

  // Navigate to the URL entered.
  omnibox_view->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB, false);

  // Prerender should be running, but abandoned.
  EXPECT_TRUE(
      GetAutocompleteActionPredictor()->IsPrerenderAbandonedForTesting());
}

// Can't run tests with NaCl plugins if built without ENABLE_NACL.
#if BUILDFLAG(ENABLE_NACL)
class PrerenderBrowserTestWithNaCl : public PrerenderBrowserTest {
 public:
  PrerenderBrowserTestWithNaCl() {}
  ~PrerenderBrowserTestWithNaCl() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableNaCl);
  }
};

// PrerenderNaClPluginEnabled crashes on ARM: http://crbug.com/585251
#if defined(ARCH_CPU_ARM_FAMILY)
#define MAYBE_PrerenderNaClPluginEnabled DISABLED_PrerenderNaClPluginEnabled
#else
#define MAYBE_PrerenderNaClPluginEnabled PrerenderNaClPluginEnabled
#endif

// Check that NaCl plugins work when enabled, with prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTestWithNaCl,
                       MAYBE_PrerenderNaClPluginEnabled) {
  PrerenderTestURL("/prerender/prerender_plugin_nacl_enabled.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();

  // To avoid any chance of a race, we have to let the script send its response
  // asynchronously.
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  bool display_test_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(web_contents,
                                                   "DidDisplayReallyPass()",
                                                   &display_test_result));
  ASSERT_TRUE(display_test_result);
}
#endif  // BUILDFLAG(ENABLE_NACL)

}  // namespace prerender

#endif  // !defined(OS_MACOSX) || !defined(ADDRESS_SANITIZER)

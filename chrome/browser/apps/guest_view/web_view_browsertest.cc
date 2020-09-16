// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/prerender/prerender_link_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/download/public/common/download_task_runner.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/guest_view_manager_factory.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/prerender/browser/prerender_link_manager.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "content/public/test/find_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_file_error_injector.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/declarative/rules_cache_delegate.h"
#include "extensions/browser/api/declarative/rules_registry.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/declarative/test_rules_registry.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_embedder.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/extension_test_message_listener.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/display/display_switches.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/gl/gl_switches.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/ppapi_test_utils.h"
#endif

using extensions::ContextMenuMatcher;
using extensions::ExtensionsAPIClient;
using extensions::MenuItem;
using guest_view::GuestViewManager;
using guest_view::TestGuestViewManager;
using guest_view::TestGuestViewManagerFactory;
using prerender::PrerenderLinkManager;
using prerender::PrerenderLinkManagerFactory;
using task_manager::browsertest_util::MatchAboutBlankTab;
using task_manager::browsertest_util::MatchAnyApp;
using task_manager::browsertest_util::MatchAnyBackground;
using task_manager::browsertest_util::MatchAnyTab;
using task_manager::browsertest_util::MatchAnyWebView;
using task_manager::browsertest_util::MatchApp;
using task_manager::browsertest_util::MatchBackground;
using task_manager::browsertest_util::MatchWebView;
using task_manager::browsertest_util::WaitForTaskManagerRows;
using ui::MenuModel;

namespace {
const char kEmptyResponsePath[] = "/close-socket";
const char kRedirectResponsePath[] = "/server-redirect";
const char kUserAgentRedirectResponsePath[] = "/detect-user-agent";
const char kCacheResponsePath[] = "/cache-control-response";
const char kRedirectResponseFullPath[] =
    "/extensions/platform_apps/web_view/shim/guest_redirect.html";

class WebContentsHiddenObserver : public content::WebContentsObserver {
 public:
  WebContentsHiddenObserver(content::WebContents* web_contents,
                            base::OnceClosure hidden_callback)
      : WebContentsObserver(web_contents),
        hidden_callback_(std::move(hidden_callback)) {}
  WebContentsHiddenObserver(const WebContentsHiddenObserver&) = delete;
  WebContentsHiddenObserver& operator=(const WebContentsHiddenObserver&) =
      delete;

  // WebContentsObserver.
  void OnVisibilityChanged(content::Visibility visibility) override {
    if (visibility == content::Visibility::HIDDEN) {
      hidden_observed_ = true;
      std::move(hidden_callback_).Run();
    }
  }

  bool hidden_observed() const { return hidden_observed_; }

 private:
  base::OnceClosure hidden_callback_;
  bool hidden_observed_ = false;
};

// Watches for context menu to be shown, sets a boolean if it is shown.
class ContextMenuShownObserver {
 public:
  ContextMenuShownObserver() {
    RenderViewContextMenu::RegisterMenuShownCallbackForTesting(base::BindOnce(
        &ContextMenuShownObserver::OnMenuShown, base::Unretained(this)));
  }
  ContextMenuShownObserver(const ContextMenuShownObserver&) = delete;
  ContextMenuShownObserver& operator=(const ContextMenuShownObserver&) = delete;
  ~ContextMenuShownObserver() = default;

  void OnMenuShown(RenderViewContextMenu* context_menu) {
    shown_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&RenderViewContextMenuBase::Cancel,
                                  base::Unretained(context_menu)));
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop_.QuitClosure());
  }

  void Wait() { run_loop_.Run(); }

  bool shown() { return shown_; }

 private:
  bool shown_ = false;
  base::RunLoop run_loop_;
};

class EmbedderWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit EmbedderWebContentsObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  EmbedderWebContentsObserver(const EmbedderWebContentsObserver&) = delete;
  EmbedderWebContentsObserver& operator=(const EmbedderWebContentsObserver&) =
      delete;

  // WebContentsObserver.
  void RenderProcessGone(base::TerminationStatus status) override {
    terminated_ = true;
    if (message_loop_runner_.get())
      message_loop_runner_->Quit();
  }

  void WaitForEmbedderRenderProcessTerminate() {
    if (terminated_)
      return;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
  }

 private:
  bool terminated_ = false;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

void ExecuteScriptWaitForTitle(content::WebContents* web_contents,
                               const char* script,
                               const char* title) {
  base::string16 expected_title(base::ASCIIToUTF16(title));
  base::string16 error_title(base::ASCIIToUTF16("error"));

  content::TitleWatcher title_watcher(web_contents, expected_title);
  title_watcher.AlsoWaitForTitle(error_title);
  EXPECT_TRUE(content::ExecuteScript(web_contents, script));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

#if defined(USE_AURA)
// Waits for select control shown/closed.
class SelectControlWaiter : public aura::WindowObserver,
                            public aura::EnvObserver {
 public:
  SelectControlWaiter() { aura::Env::GetInstance()->AddObserver(this); }

  SelectControlWaiter(const SelectControlWaiter&) = delete;
  SelectControlWaiter& operator=(const SelectControlWaiter&) = delete;
  ~SelectControlWaiter() override {
    aura::Env::GetInstance()->RemoveObserver(this);
  }

  void Wait(bool wait_for_widget_shown) {
    wait_for_widget_shown_ = wait_for_widget_shown;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
    base::RunLoop().RunUntilIdle();
  }

  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (wait_for_widget_shown_ && visible)
      message_loop_runner_->Quit();
  }

  void OnWindowInitialized(aura::Window* window) override {
    if (window->type() != aura::client::WINDOW_TYPE_MENU)
      return;
    window->AddObserver(this);
    observed_windows_.insert(window);
  }

  void OnWindowDestroyed(aura::Window* window) override {
    observed_windows_.erase(window);
    if (!wait_for_widget_shown_ && observed_windows_.empty())
      message_loop_runner_->Quit();
  }

 private:
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  std::set<aura::Window*> observed_windows_;
  bool wait_for_widget_shown_ = false;
};

// Simulate real click with delay between mouse down and up.
class LeftMouseClick {
 public:
  explicit LeftMouseClick(content::WebContents* web_contents)
      : web_contents_(web_contents),
        mouse_event_(blink::WebInputEvent::Type::kMouseDown,
                     blink::WebInputEvent::kNoModifiers,
                     blink::WebInputEvent::GetStaticTimeStampForTests()) {
    mouse_event_.button = blink::WebMouseEvent::Button::kLeft;
  }

  LeftMouseClick(const LeftMouseClick&) = delete;
  LeftMouseClick& operator=(const LeftMouseClick&) = delete;
  ~LeftMouseClick() {
    DCHECK(click_completed_);
  }

  void Click(const gfx::Point& point, int duration_ms) {
    DCHECK(click_completed_);
    click_completed_ = false;
    mouse_event_.SetType(blink::WebInputEvent::Type::kMouseDown);
    mouse_event_.SetPositionInWidget(point.x(), point.y());
    const gfx::Rect offset = web_contents_->GetContainerBounds();
    mouse_event_.SetPositionInScreen(point.x() + offset.x(),
                                     point.y() + offset.y());
    mouse_event_.click_count = 1;
    web_contents_->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
        mouse_event_);

    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&LeftMouseClick::SendMouseUp, base::Unretained(this)),
        base::TimeDelta::FromMilliseconds(duration_ms));
  }

  // Wait for click completed.
  void Wait() {
    if (click_completed_)
      return;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
    message_loop_runner_ = nullptr;
  }

 private:
  void SendMouseUp() {
    mouse_event_.SetType(blink::WebInputEvent::Type::kMouseUp);
    web_contents_->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
        mouse_event_);
    click_completed_ = true;
    if (message_loop_runner_)
      message_loop_runner_->Quit();
  }

  // Unowned pointer.
  content::WebContents* web_contents_;

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  blink::WebMouseEvent mouse_event_;

  bool click_completed_ = true;
};

#endif

bool IsShowingInterstitial(content::WebContents* tab) {
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          tab);
  if (!helper) {
    return false;
  } else {
    return helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting() !=
           nullptr;
  }
}

}  // namespace

// This class intercepts media access request from the embedder. The request
// should be triggered only if the embedder API (from tests) allows the request
// in Javascript.
// We do not issue the actual media request; the fact that the request reached
// embedder's WebContents is good enough for our tests. This is also to make
// the test run successfully on trybots.
class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() = default;
  MockWebContentsDelegate(const MockWebContentsDelegate&) = delete;
  MockWebContentsDelegate& operator=(const MockWebContentsDelegate&) = delete;
  ~MockWebContentsDelegate() override = default;

  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override {
    requested_ = true;
    if (request_message_loop_runner_.get())
      request_message_loop_runner_->Quit();
  }

  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type) override {
    checked_ = true;
    if (check_message_loop_runner_.get())
      check_message_loop_runner_->Quit();
    return true;
  }

  void WaitForRequestMediaPermission() {
    if (requested_)
      return;
    request_message_loop_runner_ = new content::MessageLoopRunner;
    request_message_loop_runner_->Run();
  }

  void WaitForCheckMediaPermission() {
    if (checked_)
      return;
    check_message_loop_runner_ = new content::MessageLoopRunner;
    check_message_loop_runner_->Run();
  }

 private:
  bool requested_ = false;
  bool checked_ = false;
  scoped_refptr<content::MessageLoopRunner> request_message_loop_runner_;
  scoped_refptr<content::MessageLoopRunner> check_message_loop_runner_;
};

// This class intercepts download request from the guest.
class MockDownloadWebContentsDelegate : public content::WebContentsDelegate {
 public:
  explicit MockDownloadWebContentsDelegate(
      content::WebContentsDelegate* orig_delegate)
      : orig_delegate_(orig_delegate) {}
  MockDownloadWebContentsDelegate(const MockDownloadWebContentsDelegate&) =
      delete;
  MockDownloadWebContentsDelegate& operator=(
      const MockDownloadWebContentsDelegate&) = delete;
  ~MockDownloadWebContentsDelegate() override = default;

  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   base::OnceCallback<void(bool)> callback) override {
    orig_delegate_->CanDownload(
        url, request_method,
        base::BindOnce(&MockDownloadWebContentsDelegate::DownloadDecided,
                       base::Unretained(this), std::move(callback)));
  }

  void WaitForCanDownload(bool expect_allow) {
    EXPECT_FALSE(waiting_for_decision_);
    waiting_for_decision_ = true;

    if (decision_made_) {
      EXPECT_EQ(expect_allow, last_download_allowed_);
      return;
    }

    expect_allow_ = expect_allow;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
  }

  void DownloadDecided(base::OnceCallback<void(bool)> callback, bool allow) {
    EXPECT_FALSE(decision_made_);
    decision_made_ = true;

    if (waiting_for_decision_) {
      EXPECT_EQ(expect_allow_, allow);
      if (message_loop_runner_.get())
        message_loop_runner_->Quit();
      std::move(callback).Run(allow);
      return;
    }
    last_download_allowed_ = allow;
    std::move(callback).Run(allow);
  }

  void Reset() {
    waiting_for_decision_ = false;
    decision_made_ = false;
  }

 private:
  content::WebContentsDelegate* orig_delegate_;
  bool waiting_for_decision_ = false;
  bool expect_allow_ = false;
  bool decision_made_ = false;
  bool last_download_allowed_ = false;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

class WebViewTest : public extensions::PlatformAppBrowserTest {
 protected:
  void SetUp() override {
    if (UsesFakeSpeech()) {
      // SpeechRecognition test specific SetUp.
      fake_speech_recognition_manager_.reset(
          new content::FakeSpeechRecognitionManager());
      fake_speech_recognition_manager_->set_should_send_fake_response(true);
      // Inject the fake manager factory so that the test result is returned to
      // the web page.
      content::SpeechRecognitionManager::SetManagerForTesting(
          fake_speech_recognition_manager_.get());
    }
    extensions::PlatformAppBrowserTest::SetUp();
  }

  void TearDown() override {
    if (UsesFakeSpeech()) {
      // SpeechRecognition test specific TearDown.
      content::SpeechRecognitionManager::SetManagerForTesting(nullptr);
    }

    extensions::PlatformAppBrowserTest::TearDown();
  }

  void SetUpOnMainThread() override {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    // Mock out geolocation for geolocation specific tests.
    if (!strncmp(test_info->name(), "GeolocationAPI",
            strlen("GeolocationAPI"))) {
      geolocation_overrider_ =
          std::make_unique<device::ScopedGeolocationOverrider>(10, 20);
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose-gc");

    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
  }

  // Handles |request| by serving a redirect response if the |User-Agent| is
  // foobar.
  static std::unique_ptr<net::test_server::HttpResponse>
  UserAgentResponseHandler(const std::string& path,
                           const GURL& redirect_target,
                           const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(path, request.relative_url,
                          base::CompareCase::SENSITIVE))
      return std::unique_ptr<net::test_server::HttpResponse>();

    auto it = request.headers.find("User-Agent");
    EXPECT_TRUE(it != request.headers.end());
    if (!base::StartsWith("foobar", it->second,
                          base::CompareCase::SENSITIVE))
      return std::unique_ptr<net::test_server::HttpResponse>();

    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", redirect_target.spec());
    return std::move(http_response);
  }

  // Handles |request| by serving a redirect response.
  static std::unique_ptr<net::test_server::HttpResponse>
  RedirectResponseHandler(const std::string& path,
                          const GURL& redirect_target,
                          const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(path, request.relative_url,
                          base::CompareCase::SENSITIVE))
      return std::unique_ptr<net::test_server::HttpResponse>();

    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", redirect_target.spec());
    return std::move(http_response);
  }

  // Handles |request| by serving an empty response.
  static std::unique_ptr<net::test_server::HttpResponse> EmptyResponseHandler(
      const std::string& path,
      const net::test_server::HttpRequest& request) {
    if (base::StartsWith(path, request.relative_url,
                         base::CompareCase::SENSITIVE))
      return std::unique_ptr<net::test_server::HttpResponse>(
          new net::test_server::RawHttpResponse("", ""));

    return std::unique_ptr<net::test_server::HttpResponse>();
  }

  // Handles |request| by serving cache-able response.
  static std::unique_ptr<net::test_server::HttpResponse>
  CacheControlResponseHandler(const std::string& path,
                              const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(path, request.relative_url,
                          base::CompareCase::SENSITIVE))
      return std::unique_ptr<net::test_server::HttpResponse>();

    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->AddCustomHeader("Cache-control", "max-age=3600");
    http_response->set_content_type("text/plain");
    http_response->set_content("dummy text");
    return std::move(http_response);
  }

  // Shortcut to return the current MenuManager.
  extensions::MenuManager* menu_manager() {
    return extensions::MenuManager::Get(browser()->profile());
  }

  // This gets all the items that any extension has registered for possible
  // inclusion in context menus.
  MenuItem::List GetItems() {
    MenuItem::List result;
    std::set<MenuItem::ExtensionKey> extension_ids =
        menu_manager()->ExtensionIds();
    std::set<MenuItem::ExtensionKey>::iterator i;
    for (i = extension_ids.begin(); i != extension_ids.end(); ++i) {
      const MenuItem::OwnedList* list = menu_manager()->MenuItems(*i);
      for (const auto& item : *list)
        result.push_back(item.get());
    }
    return result;
  }

  enum TestServer {
    NEEDS_TEST_SERVER,
    NO_TEST_SERVER
  };

  void TestHelper(const std::string& test_name,
                  const std::string& app_location,
                  TestServer test_server) {
    // For serving guest pages.
    if (test_server == NEEDS_TEST_SERVER) {
      if (!InitializeEmbeddedTestServer()) {
        LOG(ERROR) << "FAILED TO START TEST SERVER.";
        return;
      }
      embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
          &WebViewTest::RedirectResponseHandler, kRedirectResponsePath,
          embedded_test_server()->GetURL(kRedirectResponseFullPath)));

      embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
          &WebViewTest::EmptyResponseHandler, kEmptyResponsePath));

      embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
          &WebViewTest::UserAgentResponseHandler,
          kUserAgentRedirectResponsePath,
          embedded_test_server()->GetURL(kRedirectResponseFullPath)));

      embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
          &WebViewTest::CacheControlResponseHandler, kCacheResponsePath));

      EmbeddedTestServerAcceptConnections();
    }

    LoadAndLaunchPlatformApp(app_location.c_str(), "Launched");

    // Flush any pending events to make sure we start with a clean slate.
    content::RunAllPendingInMessageLoop();

    content::WebContents* embedder_web_contents =
        GetFirstAppWindowWebContents();
    if (!embedder_web_contents) {
      LOG(ERROR) << "UNABLE TO FIND EMBEDDER WEB CONTENTS.";
      return;
    }

    ExtensionTestMessageListener done_listener("TEST_PASSED", false);
    done_listener.set_failure_message("TEST_FAILED");
    // Note that domAutomationController may not exist for some tests so we
    // must use the async version of ExecuteScript.
    content::ExecuteScriptAsync(
        embedder_web_contents,
        base::StringPrintf("try { "
                           "  runTest('%s'); "
                           "} catch (e) { "
                           "  console.log('UNABLE TO START TEST.'); "
                           "  console.log(e); "
                           "  chrome.test.sendMessage('TEST_FAILED'); "
                           "}",
                           test_name.c_str()));
    ASSERT_TRUE(done_listener.WaitUntilSatisfied());
  }

  content::WebContents* LoadGuest(const std::string& guest_path,
                                  const std::string& app_path) {
    GURL::Replacements replace_host;
    replace_host.SetHostStr("localhost");

    GURL guest_url = embedded_test_server()->GetURL(guest_path);
    guest_url = guest_url.ReplaceComponents(replace_host);

    ui_test_utils::UrlLoadObserver guest_observer(
        guest_url, content::NotificationService::AllSources());

    LoadAndLaunchPlatformApp(app_path.c_str(), "guest-loaded");

    guest_observer.Wait();
    content::Source<content::NavigationController> source =
        guest_observer.source();
    EXPECT_TRUE(source->GetWebContents()
                    ->GetMainFrame()
                    ->GetProcess()
                    ->IsForGuestsOnly());

    content::WebContents* guest_web_contents = source->GetWebContents();
    return guest_web_contents;
  }

  // Helper to load interstitial page in a <webview>.
  void InterstitialTestHelper() {
    // Start a HTTPS server so we can load an interstitial page inside guest.
    net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
    https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
    https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server.Start());

    net::HostPortPair host_and_port = https_server.host_port_pair();

    LoadAndLaunchPlatformApp("web_view/interstitial_teardown",
                             "EmbedderLoaded");

    // Create the guest.
    content::WebContents* embedder_web_contents =
        GetFirstAppWindowWebContents();
    ExtensionTestMessageListener guest_added("GuestAddedToDom", false);
    EXPECT_TRUE(content::ExecuteScript(embedder_web_contents,
                                       base::StringPrintf("createGuest();\n")));
    ASSERT_TRUE(guest_added.WaitUntilSatisfied());

    // Now load the guest.
    ExtensionTestMessageListener guest_loaded("GuestLoaded", false);
    EXPECT_TRUE(content::ExecuteScript(
        embedder_web_contents,
        base::StringPrintf("loadGuest(%d);\n", host_and_port.port())));
    ASSERT_TRUE(guest_loaded.WaitUntilSatisfied());

    // Wait for interstitial page to be shown in guest.
    content::WebContents* guest_web_contents =
        GetGuestViewManager()->WaitForSingleGuestCreated();
    ASSERT_TRUE(
        guest_web_contents->GetMainFrame()->GetProcess()->IsForGuestsOnly());
    GURL target_url = https_server.GetURL(
        "/extensions/platform_apps/web_view/interstitial_teardown/"
        "https_page.html");
    content::TestNavigationObserver observer(target_url);
    observer.WatchExistingWebContents();
    observer.WaitForNavigationFinished();
  }

  // Runs media_access/allow tests.
  void MediaAccessAPIAllowTestHelper(const std::string& test_name);

  // Runs media_access/deny tests, each of them are run separately otherwise
  // they timeout (mostly on Windows).
  void MediaAccessAPIDenyTestHelper(const std::string& test_name) {
    ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
    LoadAndLaunchPlatformApp("web_view/media_access/deny", "loaded");

    content::WebContents* embedder_web_contents =
        GetFirstAppWindowWebContents();
    ASSERT_TRUE(embedder_web_contents);

    ExtensionTestMessageListener test_run_listener("PASSED", false);
    test_run_listener.set_failure_message("FAILED");
    EXPECT_TRUE(
        content::ExecuteScript(
            embedder_web_contents,
            base::StringPrintf("startDenyTest('%s')", test_name.c_str())));
    ASSERT_TRUE(test_run_listener.WaitUntilSatisfied());
  }

  // Loads an app with a <webview> in it, returns once a guest is created.
  void LoadAppWithGuest(const std::string& app_path) {
    ExtensionTestMessageListener launched_listener("WebViewTest.LAUNCHED",
                                                   false);
    launched_listener.set_failure_message("WebViewTest.FAILURE");
    LoadAndLaunchPlatformApp(app_path.c_str(), &launched_listener);

    guest_web_contents_ = GetGuestViewManager()->WaitForSingleGuestCreated();
  }

  void SendMessageToEmbedder(const std::string& message) {
    EXPECT_TRUE(
        content::ExecuteScript(
            GetEmbedderWebContents(),
            base::StringPrintf("onAppCommand('%s');", message.c_str())));
  }

  void SendMessageToGuestAndWait(const std::string& message,
                                 const std::string& wait_message) {
    std::unique_ptr<ExtensionTestMessageListener> listener;
    if (!wait_message.empty()) {
      listener.reset(new ExtensionTestMessageListener(wait_message, false));
    }

    EXPECT_TRUE(
        content::ExecuteScript(
            GetGuestWebContents(),
            base::StringPrintf("onAppCommand('%s');", message.c_str())));

    if (listener) {
      ASSERT_TRUE(listener->WaitUntilSatisfied());
    }
  }

  void OpenContextMenu(content::WebContents* web_contents) {
    blink::WebMouseEvent mouse_event(
        blink::WebInputEvent::Type::kMouseDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    mouse_event.button = blink::WebMouseEvent::Button::kRight;
    mouse_event.SetPositionInWidget(1, 1);
    web_contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
        mouse_event);
    mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
    web_contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
        mouse_event);
  }

  content::WebContents* GetGuestWebContents() {
    return guest_web_contents_;
  }

  content::WebContents* GetEmbedderWebContents() {
    if (!embedder_web_contents_) {
      embedder_web_contents_ = GetFirstAppWindowWebContents();
    }
    return embedder_web_contents_;
  }

  TestGuestViewManager* GetGuestViewManager() {
    TestGuestViewManager* manager = static_cast<TestGuestViewManager*>(
        TestGuestViewManager::FromBrowserContext(browser()->profile()));
    // TestGuestViewManager::WaitForSingleGuestCreated may and will get called
    // before a guest is created.
    if (!manager) {
      manager = static_cast<TestGuestViewManager*>(
          GuestViewManager::CreateWithDelegate(
              browser()->profile(),
              ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(
                  browser()->profile())));
    }
    return manager;
  }

  WebViewTest()
      : guest_web_contents_(nullptr), embedder_web_contents_(nullptr) {
    GuestViewManager::set_factory_for_testing(&factory_);
  }

  ~WebViewTest() override {}

 private:
  bool UsesFakeSpeech() {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();

    // SpeechRecognition test specific SetUp.
    const char* name = "SpeechRecognitionAPI_HasPermissionAllow";
    return !strncmp(test_info->name(), name, strlen(name));
  }

  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
  std::unique_ptr<content::FakeSpeechRecognitionManager>
      fake_speech_recognition_manager_;

  TestGuestViewManagerFactory factory_;
  // Note that these are only set if you launch app using LoadAppWithGuest().
  content::WebContents* guest_web_contents_;
  content::WebContents* embedder_web_contents_;
};

// The following test suites are created to group tests based on specific
// features of <webview>.
using WebViewNewWindowTest = WebViewTest;
using WebViewSizeTest = WebViewTest;
using WebViewVisibilityTest = WebViewTest;
using WebViewSpeechAPITest = WebViewTest;
using WebViewAccessibilityTest = WebViewTest;

class WebViewDPITest : public WebViewTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebViewTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                                    base::StringPrintf("%f", scale()));
  }

  static float scale() { return 2.0f; }
};

class WebViewWithZoomForDSFTest : public WebViewTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebViewTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                                    base::StringPrintf("%f", scale()));
    command_line->AppendSwitch(switches::kEnableUseZoomForDSF);
  }

  static float scale() { return 2.0f; }
};

class WebContentsAudioMutedObserver : public content::WebContentsObserver {
 public:
  explicit WebContentsAudioMutedObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents),
        loop_runner_(new content::MessageLoopRunner) {}
  WebContentsAudioMutedObserver(const WebContentsAudioMutedObserver&) = delete;
  WebContentsAudioMutedObserver& operator=(
      const WebContentsAudioMutedObserver&) = delete;

  // WebContentsObserver.
  void DidUpdateAudioMutingState(bool muted) override {
    muting_update_observed_ = true;
    loop_runner_->Quit();
  }

  void WaitForUpdate() {
    loop_runner_->Run();
  }

  bool muting_update_observed() { return muting_update_observed_; }

 private:
  scoped_refptr<content::MessageLoopRunner> loop_runner_;
  bool muting_update_observed_ = false;
};

class IsAudibleObserver : public content::WebContentsObserver {
 public:
  explicit IsAudibleObserver(content::WebContents* contents)
      : WebContentsObserver(contents) {}
  IsAudibleObserver(const IsAudibleObserver&) = delete;
  IsAudibleObserver& operator=(const IsAudibleObserver&) = delete;
  ~IsAudibleObserver() override = default;

  void WaitForCurrentlyAudible(bool audible) {
    // If there's no state change to observe then return right away.
    if (web_contents()->IsCurrentlyAudible() == audible)
      return;

    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
    message_loop_runner_ = nullptr;

    EXPECT_EQ(audible, web_contents()->IsCurrentlyAudible());
    EXPECT_EQ(audible, audible_);
  }

 private:
  void OnAudioStateChanged(bool audible) override {
    audible_ = audible;
    if (message_loop_runner_.get())
      message_loop_runner_->Quit();
  }

  bool audible_ = false;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

IN_PROC_BROWSER_TEST_F(WebViewTest, AudibilityStatePropagates) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest audio.

  LoadAppWithGuest("web_view/simple");

  content::WebContents* embedder = GetEmbedderWebContents();
  content::WebContents* guest = GetGuestWebContents();
  IsAudibleObserver embedder_obs(embedder);
  EXPECT_FALSE(embedder->IsCurrentlyAudible());
  EXPECT_FALSE(guest->IsCurrentlyAudible());

  // Just in case we get console error messages from the guest, we should
  // surface them in the test output.
  EXPECT_TRUE(content::ExecuteScript(
      embedder,
      "wv = document.getElementsByTagName('webview')[0];"
      "wv.addEventListener('consolemessage', function (e) {"
      "  console.log('WebViewTest Guest: ' + e.message);"
      "});"));

  // Inject JS to start audio.
  GURL audio_url = embedded_test_server()->GetURL(
      "/extensions/platform_apps/web_view/simple/ping.mp3");
  std::string setup_audio_script = base::StringPrintf(
      "ae = document.createElement('audio');"
      "ae.src='%s';"
      "document.body.appendChild(ae);"
      "ae.play();",
      audio_url.spec().c_str());
  EXPECT_TRUE(content::ExecuteScript(guest, setup_audio_script));

  // Wait for audio to start.
  embedder_obs.WaitForCurrentlyAudible(true);
  EXPECT_TRUE(embedder->IsCurrentlyAudible());
  EXPECT_TRUE(guest->IsCurrentlyAudible());

  // Wait for audio to stop.
  embedder_obs.WaitForCurrentlyAudible(false);
  EXPECT_FALSE(embedder->IsCurrentlyAudible());
  EXPECT_FALSE(guest->IsCurrentlyAudible());
}

IN_PROC_BROWSER_TEST_F(WebViewTest, WebViewRespectsInsets) {
  LoadAppWithGuest("web_view/simple");

  content::WebContents* guest = GetGuestWebContents();
  content::RenderWidgetHostView* guest_host_view =
      guest->GetRenderWidgetHostView();

  gfx::Insets insets(0, 0, 100, 0);
  gfx::Rect expected(guest_host_view->GetVisibleViewportSize());
  expected.Inset(insets);

  guest_host_view->SetInsets(gfx::Insets(0, 0, 100, 0));

  gfx::Size size_after = guest_host_view->GetVisibleViewportSize();
  EXPECT_EQ(expected.size(), size_after);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, AudioMutesWhileAttached) {
  LoadAppWithGuest("web_view/simple");

  content::WebContents* embedder = GetEmbedderWebContents();
  content::WebContents* guest = GetGuestWebContents();

  EXPECT_FALSE(embedder->IsAudioMuted());
  EXPECT_FALSE(guest->IsAudioMuted());

  embedder->SetAudioMuted(true);
  EXPECT_TRUE(embedder->IsAudioMuted());
  EXPECT_TRUE(guest->IsAudioMuted());

  embedder->SetAudioMuted(false);
  EXPECT_FALSE(embedder->IsAudioMuted());
  EXPECT_FALSE(guest->IsAudioMuted());
}

IN_PROC_BROWSER_TEST_F(WebViewTest, AudioMutesOnAttach) {
  LoadAndLaunchPlatformApp("web_view/app_creates_webview",
                           "WebViewTest.LAUNCHED");
  content::WebContents* embedder = GetEmbedderWebContents();
  embedder->SetAudioMuted(true);
  EXPECT_TRUE(embedder->IsAudioMuted());

  SendMessageToEmbedder("create-guest");
  content::WebContents* guest =
      GetGuestViewManager()->WaitForSingleGuestCreated();

  EXPECT_TRUE(embedder->IsAudioMuted());
  WebContentsAudioMutedObserver observer(guest);
  // If the guest hasn't attached yet, it may not have received the muting
  // update, in which case we should wait until it does.
  if (!guest->IsAudioMuted())
    observer.WaitForUpdate();
  EXPECT_TRUE(guest->IsAudioMuted());
}

IN_PROC_BROWSER_TEST_F(WebViewTest, AudioStateJavascriptAPI) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutoplayPolicy,
      switches::autoplay::kNoUserGestureRequiredPolicy);

  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/audio_state_api"))
      << message_;
}

// Test that WebView does not override autoplay policy.
IN_PROC_BROWSER_TEST_F(WebViewTest, AutoplayPolicy) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutoplayPolicy,
      switches::autoplay::kDocumentUserActivationRequiredPolicy);

  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/autoplay"))
      << message_;
}

// This test exercises the webview spatial navigation API
IN_PROC_BROWSER_TEST_F(WebViewTest, SpatialNavigationJavascriptAPI) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableSpatialNavigation);

  ExtensionTestMessageListener next_step_listener("TEST_STEP_PASSED", false);
  next_step_listener.set_failure_message("TEST_STEP_FAILED");

  LoadAndLaunchPlatformApp("web_view/spatial_navigation_state_api",
                           "WebViewTest.LAUNCHED");

  // Check that spatial navigation is initialized in the beginning
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());
  next_step_listener.Reset();

  content::WebContents* embedder = GetEmbedderWebContents();

  // Spatial navigation enabled at this point, moves focus one element
  content::SimulateKeyPress(embedder, ui::DomKey::ARROW_RIGHT,
                            ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT, false,
                            false, false, false);

  // Check that focus has moved one element
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());
  next_step_listener.Reset();

  // Moves focus again
  content::SimulateKeyPress(embedder, ui::DomKey::ARROW_RIGHT,
                            ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT, false,
                            false, false, false);

  // Check that focus has moved one element
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());
  next_step_listener.Reset();

  // Check that spatial navigation was manually disabled
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());
  next_step_listener.Reset();

  // Spatial navigation disabled at this point, RIGHT key has no effect
  content::SimulateKeyPress(embedder, ui::DomKey::ARROW_RIGHT,
                            ui::DomCode::ARROW_RIGHT, ui::VKEY_RIGHT, false,
                            false, false, false);

  // Move focus one element to the left via SHIFT+TAB
  content::SimulateKeyPress(embedder, ui::DomKey::TAB, ui::DomCode::TAB,
                            ui::VKEY_TAB, false, true, false, false);

  // Check that focus has moved to the left
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());
}

// This test verifies that hiding the guest triggers WebContents::WasHidden().
IN_PROC_BROWSER_TEST_F(WebViewVisibilityTest, GuestVisibilityChanged) {
  LoadAppWithGuest("web_view/visibility_changed");

  scoped_refptr<content::MessageLoopRunner> loop_runner(
      new content::MessageLoopRunner);
  WebContentsHiddenObserver observer(GetGuestWebContents(),
                                     loop_runner->QuitClosure());

  // Handled in platform_apps/web_view/visibility_changed/main.js
  SendMessageToEmbedder("hide-guest");
  if (!observer.hidden_observed())
    loop_runner->Run();
}

// This test verifies that hiding the embedder also hides the guest.
IN_PROC_BROWSER_TEST_F(WebViewVisibilityTest, EmbedderVisibilityChanged) {
  LoadAppWithGuest("web_view/visibility_changed");

  scoped_refptr<content::MessageLoopRunner> loop_runner(
      new content::MessageLoopRunner);
  WebContentsHiddenObserver observer(GetGuestWebContents(),
                                     loop_runner->QuitClosure());

  // Handled in platform_apps/web_view/visibility_changed/main.js
  SendMessageToEmbedder("hide-embedder");
  if (!observer.hidden_observed())
    loop_runner->Run();
}

// This test verifies that reloading the embedder reloads the guest (and doest
// not crash).
IN_PROC_BROWSER_TEST_F(WebViewTest, ReloadEmbedder) {
  // Just load a guest from other test, we do not want to add a separate
  // platform_app for this test.
  LoadAppWithGuest("web_view/visibility_changed");

  ExtensionTestMessageListener launched_again_listener("WebViewTest.LAUNCHED",
                                                       false);
  GetEmbedderWebContents()->GetController().Reload(content::ReloadType::NORMAL,
                                                   false);
  ASSERT_TRUE(launched_again_listener.WaitUntilSatisfied());
}

// This test ensures JavaScript errors ("Cannot redefine property") do not
// happen when a <webview> is removed from DOM and added back.
IN_PROC_BROWSER_TEST_F(WebViewTest, AddRemoveWebView_AddRemoveWebView) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/addremove"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewSizeTest, AutoSize) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/autosize"))
      << message_;
}

// Test for http://crbug.com/419611.
IN_PROC_BROWSER_TEST_F(WebViewTest, DisplayNoneSetSrc) {
  LoadAndLaunchPlatformApp("web_view/display_none_set_src",
                           "WebViewTest.LAUNCHED");
  // Navigate the guest while it's in "display: none" state.
  SendMessageToEmbedder("navigate-guest");
  GetGuestViewManager()->WaitForSingleGuestCreated();

  // Now attempt to navigate the guest again.
  SendMessageToEmbedder("navigate-guest");

  ExtensionTestMessageListener test_passed_listener("WebViewTest.PASSED",
                                                    false);
  // Making the guest visible would trigger loadstop.
  SendMessageToEmbedder("show-guest");
  EXPECT_TRUE(test_passed_listener.WaitUntilSatisfied());
}

// Checks that {allFrames: true} injects script correctly to subframes
// inside <webview>.
IN_PROC_BROWSER_TEST_F(WebViewTest, ExecuteScript) {
  ASSERT_TRUE(RunPlatformAppTestWithArg(
      "platform_apps/web_view/common", "execute_script")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewSizeTest, Shim_TestAutosizeAfterNavigation) {
  TestHelper("testAutosizeAfterNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestAllowTransparencyAttribute) {
  TestHelper("testAllowTransparencyAttribute", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewDPITest, Shim_TestAutosizeHeight) {
  TestHelper("testAutosizeHeight", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewWithZoomForDSFTest, Shim_TestAutosizeHeight) {
  TestHelper("testAutosizeHeight", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewSizeTest, Shim_TestAutosizeHeight) {
  TestHelper("testAutosizeHeight", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewDPITest, Shim_TestAutosizeBeforeNavigation) {
  TestHelper("testAutosizeBeforeNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewWithZoomForDSFTest,
                       Shim_TestAutosizeBeforeNavigation) {
  TestHelper("testAutosizeBeforeNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewSizeTest, Shim_TestAutosizeBeforeNavigation) {
  TestHelper("testAutosizeBeforeNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewDPITest, Shim_TestAutosizeRemoveAttributes) {
  TestHelper("testAutosizeRemoveAttributes", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewWithZoomForDSFTest,
                       Shim_TestAutosizeRemoveAttributes) {
  TestHelper("testAutosizeRemoveAttributes", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewSizeTest, Shim_TestAutosizeRemoveAttributes) {
  TestHelper("testAutosizeRemoveAttributes", "web_view/shim", NO_TEST_SERVER);
}

// This test is disabled due to being flaky. http://crbug.com/282116
IN_PROC_BROWSER_TEST_F(WebViewSizeTest,
                       DISABLED_Shim_TestAutosizeWithPartialAttributes) {
  TestHelper("testAutosizeWithPartialAttributes",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestAPIMethodExistence) {
  TestHelper("testAPIMethodExistence", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       Shim_TestCustomElementCallbacksInaccessible) {
  TestHelper("testCustomElementCallbacksInaccessible", "web_view/shim",
             NO_TEST_SERVER);
}

// Tests the existence of WebRequest API event objects on the request
// object, on the webview element, and hanging directly off webview.
IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestWebRequestAPIExistence) {
  TestHelper("testWebRequestAPIExistence", "web_view/shim", NO_TEST_SERVER);
}

// Tests that addListener call succeeds on webview's WebRequest API events.
IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestWebRequestAPIAddListener) {
  TestHelper("testWebRequestAPIAddListener", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestWebRequestAPIErrorOccurred) {
  TestHelper("testWebRequestAPIErrorOccurred", "web_view/shim", NO_TEST_SERVER);
}

#if defined(USE_AURA)
// Test validates that select tag can be shown and hidden in webview safely
// using quick touch.
IN_PROC_BROWSER_TEST_F(WebViewTest, SelectShowHide) {
  LoadAppWithGuest("web_view/select");

  content::WebContents* embedder_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(embedder_contents);

  std::vector<content::WebContents*> guest_contents_list;
  GetGuestViewManager()->GetGuestWebContentsList(&guest_contents_list);
  ASSERT_EQ(1u, guest_contents_list.size());
  content::WebContents* guest_contents = guest_contents_list[0];

  const gfx::Rect embedder_rect = embedder_contents->GetContainerBounds();
  const gfx::Rect guest_rect = guest_contents->GetContainerBounds();
  const gfx::Point click_point(guest_rect.x() - embedder_rect.x() + 10,
                               guest_rect.y() - embedder_rect.y() + 10);

  LeftMouseClick mouse_click(guest_contents);
  SelectControlWaiter select_control_waiter;

  for (int i = 0; i < 5; ++i) {
    const int click_duration_ms = 10 + i * 25;
    mouse_click.Click(click_point, click_duration_ms);
    select_control_waiter.Wait(true);
    mouse_click.Wait();

    mouse_click.Click(click_point, click_duration_ms);
    select_control_waiter.Wait(false);
    mouse_click.Wait();
  }
}
#endif

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestChromeExtensionURL) {
  TestHelper("testChromeExtensionURL", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestChromeExtensionRelativePath) {
  TestHelper("testChromeExtensionRelativePath",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       Shim_TestContentInitiatedNavigationToDataUrlBlocked) {
  TestHelper("testContentInitiatedNavigationToDataUrlBlocked", "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestDisplayNoneWebviewLoad) {
  TestHelper("testDisplayNoneWebviewLoad", "web_view/shim", NO_TEST_SERVER);
}

#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_Shim_TestDisplayNoneWebviewRemoveChild \
  DISABLED_Shim_TestDisplayNoneWebviewRemoveChild
#else
#define MAYBE_Shim_TestDisplayNoneWebviewRemoveChild \
  Shim_TestDisplayNoneWebviewRemoveChild
#endif
// Flaky on Windows & Linux: https://crbug.com/1115106.
IN_PROC_BROWSER_TEST_F(WebViewTest,
                       MAYBE_Shim_TestDisplayNoneWebviewRemoveChild) {
  TestHelper("testDisplayNoneWebviewRemoveChild",
             "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestDisplayBlock) {
  TestHelper("testDisplayBlock", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       Shim_TestInlineScriptFromAccessibleResources) {
  TestHelper("testInlineScriptFromAccessibleResources",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestInvalidChromeExtensionURL) {
  TestHelper("testInvalidChromeExtensionURL", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestEventName) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  TestHelper("testEventName", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestOnEventProperty) {
  TestHelper("testOnEventProperties", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestLoadProgressEvent) {
  TestHelper("testLoadProgressEvent", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestDestroyOnEventListener) {
  TestHelper("testDestroyOnEventListener", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestCannotMutateEventName) {
  TestHelper("testCannotMutateEventName", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestPartitionChangeAfterNavigation) {
  TestHelper("testPartitionChangeAfterNavigation",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       Shim_TestPartitionRemovalAfterNavigationFails) {
  TestHelper("testPartitionRemovalAfterNavigationFails",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestAddContentScript) {
  TestHelper("testAddContentScript", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestAddMultipleContentScripts) {
  TestHelper("testAddMultipleContentScripts", "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    Shim_TestAddContentScriptWithSameNameShouldOverwriteTheExistingOne) {
  TestHelper("testAddContentScriptWithSameNameShouldOverwriteTheExistingOne",
             "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    Shim_TestAddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView) {
  TestHelper("testAddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView",
             "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestAddAndRemoveContentScripts) {
  TestHelper("testAddAndRemoveContentScripts", "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest,
                       Shim_TestAddContentScriptsWithNewWindowAPI) {
  TestHelper("testAddContentScriptsWithNewWindowAPI", "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    Shim_TestContentScriptIsInjectedAfterTerminateAndReloadWebView) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  TestHelper("testContentScriptIsInjectedAfterTerminateAndReloadWebView",
             "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       Shim_TestContentScriptExistsAsLongAsWebViewTagExists) {
  TestHelper("testContentScriptExistsAsLongAsWebViewTagExists", "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestAddContentScriptWithCode) {
  TestHelper("testAddContentScriptWithCode", "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    Shim_TestAddMultipleContentScriptsWithCodeAndCheckGeneratedScriptUrl) {
  TestHelper("testAddMultipleContentScriptsWithCodeAndCheckGeneratedScriptUrl",
             "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestExecuteScriptFail) {
  TestHelper("testExecuteScriptFail", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestExecuteScript) {
  TestHelper("testExecuteScript", "web_view/shim", NO_TEST_SERVER);
}

// Flaky and likely not testing the right assertion. https://crbug.com/703727
IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    DISABLED_Shim_TestExecuteScriptIsAbortedWhenWebViewSourceIsChanged) {
  TestHelper("testExecuteScriptIsAbortedWhenWebViewSourceIsChanged",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    Shim_TestExecuteScriptIsAbortedWhenWebViewSourceIsInvalid) {
  TestHelper("testExecuteScriptIsAbortedWhenWebViewSourceIsInvalid",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestTerminateAfterExit) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  TestHelper("testTerminateAfterExit", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestAssignSrcAfterCrash) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  TestHelper("testAssignSrcAfterCrash", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       Shim_TestNavOnConsecutiveSrcAttributeChanges) {
  TestHelper("testNavOnConsecutiveSrcAttributeChanges",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestNavOnSrcAttributeChange) {
  TestHelper("testNavOnSrcAttributeChange", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestNavigateAfterResize) {
  TestHelper("testNavigateAfterResize", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestNestedCrossOriginSubframes) {
  TestHelper("testNestedCrossOriginSubframes",
             "web_view/shim", NEEDS_TEST_SERVER);
}

#if defined(OS_MAC)
// Flaky on Mac. See https://crbug.com/674904.
#define MAYBE_Shim_TestNestedSubframes DISABLED_Shim_TestNestedSubframes
#else
#define MAYBE_Shim_TestNestedSubframes Shim_TestNestedSubframes
#endif
IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_Shim_TestNestedSubframes) {
  TestHelper("testNestedSubframes", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestRemoveSrcAttribute) {
  TestHelper("testRemoveSrcAttribute", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestReassignSrcAttribute) {
  TestHelper("testReassignSrcAttribute", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest, Shim_TestNewWindow) {
  TestHelper("testNewWindow", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest, Shim_TestNewWindowTwoListeners) {
  TestHelper("testNewWindowTwoListeners", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest,
                       Shim_TestNewWindowNoPreventDefault) {
  TestHelper("testNewWindowNoPreventDefault",
             "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest, Shim_TestNewWindowNoReferrerLink) {
  TestHelper("testNewWindowNoReferrerLink", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest,
                       Shim_TestWebViewAndEmbedderInNewWindow) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.

  // Launch the app and wait until it's ready to load a test.
  LoadAndLaunchPlatformApp("web_view/shim", "Launched");

  content::WebContents* embedder_web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(embedder_web_contents);

  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");

  std::string empty_guest_path(
      "/extensions/platform_apps/web_view/shim/empty_guest.html");
  GURL empty_guest_url = embedded_test_server()->GetURL(empty_guest_path);
  empty_guest_url = empty_guest_url.ReplaceComponents(replace_host);

  ui_test_utils::UrlLoadObserver empty_guest_observer(
      empty_guest_url, content::NotificationService::AllSources());

  // Run the test and wait until the guest WebContents is available and has
  // finished loading.
  ExtensionTestMessageListener done_listener("TEST_PASSED", false);
  done_listener.set_failure_message("TEST_FAILED");
  EXPECT_TRUE(content::ExecuteScript(
      embedder_web_contents, "runTest('testWebViewAndEmbedderInNewWindow')"));

  empty_guest_observer.Wait();

  content::Source<content::NavigationController> source =
      empty_guest_observer.source();
  EXPECT_TRUE(source->GetWebContents()
                  ->GetMainFrame()
                  ->GetProcess()
                  ->IsForGuestsOnly());
  ASSERT_TRUE(done_listener.WaitUntilSatisfied());

  // Make sure opener and owner for the empty_guest source are different.
  // In general, we should have two guests and two embedders. Once we know the
  // guests are different and the embedders are different, then we have four
  // distinct WebContents, as we expect.
  std::vector<content::WebContents*> guest_contents_list;
  GetGuestViewManager()->GetGuestWebContentsList(&guest_contents_list);
  ASSERT_EQ(2u, guest_contents_list.size());
  content::WebContents* new_window_guest_contents = guest_contents_list[0];

  content::WebContents* empty_guest_web_contents = source->GetWebContents();
  ASSERT_EQ(empty_guest_web_contents, guest_contents_list[1]);
  ASSERT_NE(empty_guest_web_contents, new_window_guest_contents);
  content::WebContents* empty_guest_embedder =
      GetEmbedderForGuest(empty_guest_web_contents);
  ASSERT_NE(empty_guest_embedder, embedder_web_contents);
  ASSERT_TRUE(empty_guest_embedder);
  content::RenderFrameHost* empty_guest_opener =
      empty_guest_web_contents->GetOriginalOpener();
  ASSERT_TRUE(empty_guest_opener);
  ASSERT_NE(empty_guest_opener, empty_guest_embedder->GetMainFrame());
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestContentLoadEvent) {
  TestHelper("testContentLoadEvent", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestContentLoadEventWithDisplayNone) {
  TestHelper("testContentLoadEventWithDisplayNone",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestDeclarativeWebRequestAPI) {
  TestHelper("testDeclarativeWebRequestAPI",
             "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       Shim_TestDeclarativeWebRequestAPISendMessage) {
  TestHelper("testDeclarativeWebRequestAPISendMessage",
             "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    Shim_TestDeclarativeWebRequestAPISendMessageSecondWebView) {
  TestHelper("testDeclarativeWebRequestAPISendMessageSecondWebView",
             "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestWebRequestAPI) {
  TestHelper("testWebRequestAPI", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestWebRequestAPIOnlyForInstance) {
  TestHelper("testWebRequestAPIOnlyForInstance", "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestWebRequestAPIWithHeaders) {
  TestHelper("testWebRequestAPIWithHeaders",
             "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestWebRequestAPIGoogleProperty) {
  TestHelper("testWebRequestAPIGoogleProperty",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       Shim_TestWebRequestListenerSurvivesReparenting) {
  TestHelper("testWebRequestListenerSurvivesReparenting",
             "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestLoadStartLoadRedirect) {
  TestHelper("testLoadStartLoadRedirect", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       Shim_TestLoadAbortChromeExtensionURLWrongPartition) {
  TestHelper("testLoadAbortChromeExtensionURLWrongPartition",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestLoadAbortEmptyResponse) {
  TestHelper("testLoadAbortEmptyResponse", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestLoadAbortIllegalChromeURL) {
  TestHelper("testLoadAbortIllegalChromeURL",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestLoadAbortIllegalFileURL) {
  TestHelper("testLoadAbortIllegalFileURL", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestLoadAbortIllegalJavaScriptURL) {
  TestHelper("testLoadAbortIllegalJavaScriptURL",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestLoadAbortInvalidNavigation) {
  TestHelper("testLoadAbortInvalidNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestLoadAbortNonWebSafeScheme) {
  TestHelper("testLoadAbortNonWebSafeScheme", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestReload) {
  TestHelper("testReload", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestReloadAfterTerminate) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  TestHelper("testReloadAfterTerminate", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestGetProcessId) {
  TestHelper("testGetProcessId", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewVisibilityTest, Shim_TestHiddenBeforeNavigation) {
  TestHelper("testHiddenBeforeNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestRemoveWebviewOnExit) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.

  // Launch the app and wait until it's ready to load a test.
  LoadAndLaunchPlatformApp("web_view/shim", "Launched");

  content::WebContents* embedder_web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(embedder_web_contents);

  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");

  std::string guest_path(
      "/extensions/platform_apps/web_view/shim/empty_guest.html");
  GURL guest_url = embedded_test_server()->GetURL(guest_path);
  guest_url = guest_url.ReplaceComponents(replace_host);

  ui_test_utils::UrlLoadObserver guest_observer(
      guest_url, content::NotificationService::AllSources());

  // Run the test and wait until the guest WebContents is available and has
  // finished loading.
  ExtensionTestMessageListener guest_loaded_listener("guest-loaded", false);
  EXPECT_TRUE(content::ExecuteScript(
                  embedder_web_contents,
                  "runTest('testRemoveWebviewOnExit')"));
  guest_observer.Wait();

  content::Source<content::NavigationController> source =
      guest_observer.source();
  EXPECT_TRUE(source->GetWebContents()
                  ->GetMainFrame()
                  ->GetProcess()
                  ->IsForGuestsOnly());

  ASSERT_TRUE(guest_loaded_listener.WaitUntilSatisfied());

  content::WebContentsDestroyedWatcher destroyed_watcher(
      source->GetWebContents());

  // Tell the embedder to kill the guest.
  EXPECT_TRUE(content::ExecuteScript(
                  embedder_web_contents,
                  "removeWebviewOnExitDoCrash();"));

  // Wait until the guest WebContents is destroyed.
  destroyed_watcher.Wait();
}

// Remove <webview> immediately after navigating it.
// This is a regression test for http://crbug.com/276023.
IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestRemoveWebviewAfterNavigation) {
  TestHelper("testRemoveWebviewAfterNavigation",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestNavigationToExternalProtocol) {
  TestHelper("testNavigationToExternalProtocol",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewSizeTest,
                       Shim_TestResizeWebviewWithDisplayNoneResizesContent) {
  TestHelper("testResizeWebviewWithDisplayNoneResizesContent",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewSizeTest, Shim_TestResizeWebviewResizesContent) {
  TestHelper("testResizeWebviewResizesContent",
             "web_view/shim",
             NO_TEST_SERVER);
}

// Test makes sure that interstitial pages renders in <webview>.
// Flaky on Win dbg: crbug.com/779973
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_InterstitialPage DISABLED_InterstitialPage
#else
#define MAYBE_InterstitialPage InterstitialPage
#endif

IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_InterstitialPage) {
  // This test tests that a inner WebContents' InterstitialPage is properly
  // connected to an outer WebContents through a CrossProcessFrameConnector.

  InterstitialTestHelper();

  content::WebContents* guest_web_contents =
      GetGuestViewManager()->WaitForSingleGuestCreated();
  EXPECT_TRUE(IsShowingInterstitial(guest_web_contents));
}

// Test makes sure that interstitial pages are registered in the
// RenderWidgetHostInputEventRouter when inside a <webview>.
// Flaky on Win dbg: crbug.com/779973
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_InterstitialPageRouteEvents DISABLED_InterstitialPageRouteEvents
#else
#define MAYBE_InterstitialPageRouteEvents InterstitialPageRouteEvents
#endif

IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_InterstitialPageRouteEvents) {
  // This test tests that a inner WebContents' InterstitialPage is properly
  // connected to an outer WebContents through a CrossProcessFrameConnector.

  InterstitialTestHelper();

  content::WebContents* web_contents = GetFirstAppWindowWebContents();

  std::vector<content::RenderWidgetHostView*> hosts =
      content::GetInputEventRouterRenderWidgetHostViews(web_contents);

  EXPECT_TRUE(base::Contains(hosts, web_contents->GetMainFrame()->GetView()));
}

// Test makes sure that the browser does not crash when a <webview> navigates
// out of an interstitial.
// Flaky on Win dbg: crbug.com/779973
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_InterstitialPageDetach DISABLED_InterstitialPageDetach
#else
#define MAYBE_InterstitialPageDetach InterstitialPageDetach
#endif

IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_InterstitialPageDetach) {
  InterstitialTestHelper();

  content::WebContents* guest_web_contents =
      GetGuestViewManager()->WaitForSingleGuestCreated();
  EXPECT_TRUE(IsShowingInterstitial(guest_web_contents));

  // Navigate to about:blank.
  content::TestNavigationObserver load_observer(guest_web_contents);
  bool result = ExecuteScript(guest_web_contents,
                              "window.location.assign('about:blank')");
  EXPECT_TRUE(result);
  load_observer.Wait();
}

// This test makes sure the browser process does not crash if app is closed
// while an interstitial page is being shown in guest.
// Flaky on Win dbg: crbug.com/779973
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_InterstitialTeardown DISABLED_InterstitialTeardown
#else
#define MAYBE_InterstitialTeardown InterstitialTeardown
#endif

IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_InterstitialTeardown) {
  InterstitialTestHelper();

  // Now close the app while interstitial page being shown in guest.
  extensions::AppWindow* window = GetFirstAppWindow();
  window->GetBaseWindow()->Close();
}

// This test makes sure the browser process does not crash if browser is shut
// down while an interstitial page is being shown in guest.
// Flaky. http://crbug.com/627962.
IN_PROC_BROWSER_TEST_F(WebViewTest,
                       DISABLED_InterstitialTeardownOnBrowserShutdown) {
  InterstitialTestHelper();

  // Now close the app while interstitial page being shown in guest.
  extensions::AppWindow* window = GetFirstAppWindow();
  window->GetBaseWindow()->Close();

  // InterstitialPage is not destroyed immediately, so the
  // RenderWidgetHostViewGuest for it is still there, closing all
  // renderer processes will cause the RWHVGuest's RenderProcessGone()
  // shutdown path to be exercised.
  chrome::CloseAllBrowsers();
}

IN_PROC_BROWSER_TEST_F(WebViewTest, ShimSrcAttribute) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/src_attribute"))
      << message_;
}

// This test verifies that prerendering has been disabled inside <webview>.
// This test is here rather than in PrerenderBrowserTest for testing convenience
// only. If it breaks then this is a bug in the prerenderer.
IN_PROC_BROWSER_TEST_F(WebViewTest, NoPrerenderer) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  content::WebContents* guest_web_contents =
      LoadGuest(
          "/extensions/platform_apps/web_view/noprerenderer/guest.html",
          "web_view/noprerenderer");
  ASSERT_TRUE(guest_web_contents != nullptr);

  PrerenderLinkManager* prerender_link_manager =
      PrerenderLinkManagerFactory::GetForBrowserContext(
          guest_web_contents->GetBrowserContext());
  ASSERT_TRUE(prerender_link_manager != nullptr);
  EXPECT_TRUE(prerender_link_manager->IsEmpty());
}

// Verify that existing <webview>'s are detected when the task manager starts
// up.
IN_PROC_BROWSER_TEST_F(WebViewTest, TaskManagerExistingWebView) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  LoadGuest("/extensions/platform_apps/web_view/task_manager/guest.html",
            "web_view/task_manager");

  chrome::ShowTaskManager(browser());  // Show task manager AFTER guest loads.

  const char* guest_title = "WebViewed test content";
  const char* app_name = "<webview> task manager test";
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchWebView(guest_title)));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchApp(app_name)));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchBackground(app_name)));

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyWebView()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyApp()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyBackground()));
}

// Verify that the task manager notices the creation of new <webview>'s.
IN_PROC_BROWSER_TEST_F(WebViewTest, TaskManagerNewWebView) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  chrome::ShowTaskManager(browser());  // Show task manager BEFORE guest loads.

  LoadGuest("/extensions/platform_apps/web_view/task_manager/guest.html",
            "web_view/task_manager");

  const char* guest_title = "WebViewed test content";
  const char* app_name = "<webview> task manager test";
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchWebView(guest_title)));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchApp(app_name)));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchBackground(app_name)));

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyWebView()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyApp()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyBackground()));
}

// This tests cookie isolation for packaged apps with webview tags. It navigates
// the main browser window to a page that sets a cookie and loads an app with
// multiple webview tags. Each tag sets a cookie and the test checks the proper
// storage isolation is enforced.
IN_PROC_BROWSER_TEST_F(WebViewTest, CookieIsolation) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Navigate the browser to a page which writes a sample cookie
  // The cookie is "testCookie=1"
  GURL set_cookie_url = embedded_test_server()->GetURL(
      "/extensions/platform_apps/web_view/cookie_isolation/set_cookie.html");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  set_cookie_url = set_cookie_url.ReplaceComponents(replace_host);

  ui_test_utils::NavigateToURL(browser(), set_cookie_url);
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/cookie_isolation"))
      << message_;
  // Finally, verify that the browser cookie has not changed.
  int cookie_size;
  std::string cookie_value;

  ui_test_utils::GetCookies(GURL("http://localhost"),
                            browser()->tab_strip_model()->GetWebContentsAt(0),
                            &cookie_size, &cookie_value);
  EXPECT_EQ("testCookie=1", cookie_value);
}

// This tests that in-memory storage partitions are reset on browser restart,
// but persistent ones maintain state for cookies and HTML5 storage.
IN_PROC_BROWSER_TEST_F(WebViewTest, PRE_StoragePersistence) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // We don't care where the main browser is on this test.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  // Since this test is PRE_ step, we need file access.
  ASSERT_TRUE(RunPlatformAppTestWithFlags(
      "platform_apps/web_view/storage_persistence", "PRE_StoragePersistence",
      kFlagEnableFileAccess, kFlagNone))
      << message_;
  content::EnsureCookiesFlushed(profile());
}

// This is the post-reset portion of the StoragePersistence test.  See
// PRE_StoragePersistence for main comment.
IN_PROC_BROWSER_TEST_F(WebViewTest, StoragePersistence) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // We don't care where the main browser is on this test.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));

  // Since this test has PRE_ step, we need file access (possibly because we
  // need to access previous profile).
  ASSERT_TRUE(RunPlatformAppTestWithFlags(
      "platform_apps/web_view/storage_persistence", "StoragePersistence",
      kFlagEnableFileAccess, kFlagNone))
      << message_;
}

// This tests DOM storage isolation for packaged apps with webview tags. It
// loads an app with multiple webview tags and each tag sets DOM storage
// entries, which the test checks to ensure proper storage isolation is
// enforced.
IN_PROC_BROWSER_TEST_F(WebViewTest, DOMStorageIsolation) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  GURL navigate_to_url = embedded_test_server()->GetURL(
      "/extensions/platform_apps/web_view/dom_storage_isolation/page.html");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  navigate_to_url = navigate_to_url.ReplaceComponents(replace_host);

  ui_test_utils::NavigateToURL(browser(), navigate_to_url);
  ASSERT_TRUE(
      RunPlatformAppTest("platform_apps/web_view/dom_storage_isolation"));
  // Verify that the browser tab's local/session storage does not have the same
  // values which were stored by the webviews.
  std::string output;
  std::string get_local_storage(
      "window.domAutomationController.send("
      "window.localStorage.getItem('foo') || 'badval')");
  std::string get_session_storage(
      "window.domAutomationController.send("
      "window.localStorage.getItem('baz') || 'badval')");
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetWebContentsAt(0),
      get_local_storage.c_str(), &output));
  EXPECT_STREQ("badval", output.c_str());
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetWebContentsAt(0),
      get_session_storage.c_str(), &output));
  EXPECT_STREQ("badval", output.c_str());
}

// This tests how guestviews should or should not be able to find each other
// depending on whether they are in the same storage partition or not.
// This is a regression test for https://crbug.com/794079 (where two guestviews
// in the same storage partition stopped being able to find each other).
// This is also a regression test for https://crbug.com/802278 (setting of
// a guestview as an opener should not leak any memory).
IN_PROC_BROWSER_TEST_F(WebViewTest, FindabilityIsolation) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  GURL navigate_to_url = embedded_test_server()->GetURL(
      "/extensions/platform_apps/web_view/findability_isolation/page.html");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  navigate_to_url = navigate_to_url.ReplaceComponents(replace_host);

  ui_test_utils::NavigateToURL(browser(), navigate_to_url);
  ASSERT_TRUE(
      RunPlatformAppTest("platform_apps/web_view/findability_isolation"));
}

// This tests IndexedDB isolation for packaged apps with webview tags. It loads
// an app with multiple webview tags and each tag creates an IndexedDB record,
// which the test checks to ensure proper storage isolation is enforced.
IN_PROC_BROWSER_TEST_F(WebViewTest, IndexedDBIsolation) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/web_view/isolation_indexeddb")) << message_;
}

// This test ensures that closing app window on 'loadcommit' does not crash.
// The test launches an app with guest and closes the window on loadcommit. It
// then launches the app window again. The process is repeated 3 times.
// TODO(crbug.com/949923): The test is flaky (crash) on ChromeOS debug and ASan/LSan
#if defined(OS_CHROMEOS) && (!defined(NDEBUG) || defined(ADDRESS_SANITIZER))
#define MAYBE_CloseOnLoadcommit DISABLED_CloseOnLoadcommit
#else
#define MAYBE_CloseOnLoadcommit CloseOnLoadcommit
#endif
IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_CloseOnLoadcommit) {
  LoadAndLaunchPlatformApp("web_view/close_on_loadcommit",
                           "done-close-on-loadcommit");
}

IN_PROC_BROWSER_TEST_F(WebViewTest, MediaAccessAPIDeny_TestDeny) {
  MediaAccessAPIDenyTestHelper("testDeny");
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       MediaAccessAPIDeny_TestDenyThenAllowThrows) {
  MediaAccessAPIDenyTestHelper("testDenyThenAllowThrows");
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       MediaAccessAPIDeny_TestDenyWithPreventDefault) {
  MediaAccessAPIDenyTestHelper("testDenyWithPreventDefault");
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       MediaAccessAPIDeny_TestNoListenersImplyDeny) {
  MediaAccessAPIDenyTestHelper("testNoListenersImplyDeny");
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       MediaAccessAPIDeny_TestNoPreventDefaultImpliesDeny) {
  MediaAccessAPIDenyTestHelper("testNoPreventDefaultImpliesDeny");
}

void WebViewTest::MediaAccessAPIAllowTestHelper(const std::string& test_name) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  LoadAndLaunchPlatformApp("web_view/media_access/allow", "Launched");

  content::WebContents* embedder_web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(embedder_web_contents);
  std::unique_ptr<MockWebContentsDelegate> mock(new MockWebContentsDelegate());
  embedder_web_contents->SetDelegate(mock.get());

  ExtensionTestMessageListener done_listener("TEST_PASSED", false);
  done_listener.set_failure_message("TEST_FAILED");
  EXPECT_TRUE(
      content::ExecuteScript(
          embedder_web_contents,
          base::StringPrintf("startAllowTest('%s')",
                             test_name.c_str())));
  ASSERT_TRUE(done_listener.WaitUntilSatisfied());

  mock->WaitForRequestMediaPermission();
}

IN_PROC_BROWSER_TEST_F(WebViewTest, OpenURLFromTab_CurrentTab_Abort) {
  LoadAppWithGuest("web_view/simple");

  // Verify that OpenURLFromTab with a window disposition of CURRENT_TAB will
  // navigate the current <webview>.
  ExtensionTestMessageListener load_listener("WebViewTest.LOADSTOP", false);

  // Navigating to a file URL is forbidden inside a <webview>.
  content::OpenURLParams params(GURL("file://foo"), content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                true /* is_renderer_initiated */);
  GetGuestWebContents()->GetDelegate()->OpenURLFromTab(
      GetGuestWebContents(), params);

  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Verify that the <webview> ends up at about:blank.
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            GetGuestWebContents()->GetLastCommittedURL());
}

// A navigation to a web-safe URL should succeed, even if it is not renderer-
// initiated, such as a navigation from the PDF viewer.
IN_PROC_BROWSER_TEST_F(WebViewTest, OpenURLFromTab_CurrentTab_Succeed) {
  LoadAppWithGuest("web_view/simple");

  // Verify that OpenURLFromTab with a window disposition of CURRENT_TAB will
  // navigate the current <webview>.
  ExtensionTestMessageListener load_listener("WebViewTest.LOADSTOP", false);

  GURL test_url("http://www.google.com");
  content::OpenURLParams params(
      test_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false /* is_renderer_initiated */);
  GetGuestWebContents()->GetDelegate()->OpenURLFromTab(GetGuestWebContents(),
                                                       params);

  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  EXPECT_EQ(test_url, GetGuestWebContents()->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest, OpenURLFromTab_NewWindow_Abort) {
  LoadAppWithGuest("web_view/simple");

  // Verify that OpenURLFromTab with a window disposition of NEW_BACKGROUND_TAB
  // will trigger the <webview>'s New Window API.
  ExtensionTestMessageListener new_window_listener(
      "WebViewTest.NEWWINDOW", false);

  // Navigating to a file URL is forbidden inside a <webview>.
  content::OpenURLParams params(GURL("file://foo"), content::Referrer(),
                                WindowOpenDisposition::NEW_BACKGROUND_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                true /* is_renderer_initiated */);
  GetGuestWebContents()->GetDelegate()->OpenURLFromTab(
      GetGuestWebContents(), params);

  ASSERT_TRUE(new_window_listener.WaitUntilSatisfied());

  // Verify that a new guest was created.
  content::WebContents* new_guest_web_contents =
      GetGuestViewManager()->GetLastGuestCreated();
  EXPECT_NE(GetGuestWebContents(), new_guest_web_contents);

  // Verify that the new <webview> guest ends up at about:blank.
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            new_guest_web_contents->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(WebViewTest, ContextMenuInspectElement) {
  LoadAppWithGuest("web_view/context_menus/basic");
  content::WebContents* guest_web_contents = GetGuestWebContents();
  ASSERT_TRUE(guest_web_contents);

  content::ContextMenuParams params;
  TestRenderViewContextMenu menu(guest_web_contents->GetMainFrame(), params);
  menu.Init();

  // Expect "Inspect" to be shown as we are running webview in a chrome app.
  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
}

// This test executes the context menu command 'LanguageSettings' which will
// load chrome://settings/languages in a browser window. This is a browser-
// initiated operation and so we expect this to succeed if the embedder is
// allowed to perform the operation.
IN_PROC_BROWSER_TEST_F(WebViewTest, ContextMenuLanguageSettings) {
  LoadAppWithGuest("web_view/context_menus/basic");

  content::WebContents* guest_web_contents = GetGuestWebContents();
  content::WebContents* embedder = GetEmbedderWebContents();
  ASSERT_TRUE(embedder);

  // Create and build our test context menu.
  content::WebContentsAddedObserver web_contents_added_observer;

  GURL page_url("http://www.google.com");
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(guest_web_contents, page_url, GURL(),
                                        GURL()));
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS, 0);

  content::WebContents* new_contents =
      web_contents_added_observer.GetWebContents();

  // Verify that a new WebContents has been created that is at the Language
  // Settings page.
  EXPECT_EQ(GURL(std::string(chrome::kChromeUISettingsURL) +
                 chrome::kLanguageOptionsSubPage),
            new_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(WebViewTest, ContextMenusAPI_Basic) {
  LoadAppWithGuest("web_view/context_menus/basic");

  content::WebContents* guest_web_contents = GetGuestWebContents();
  content::WebContents* embedder = GetEmbedderWebContents();
  ASSERT_TRUE(embedder);

  // 1. Basic property test.
  ExecuteScriptWaitForTitle(embedder, "checkProperties()", "ITEM_CHECKED");

  // 2. Create a menu item and wait for created callback to be called.
  ExecuteScriptWaitForTitle(embedder, "createMenuItem()", "ITEM_CREATED");

  // 3. Click the created item, wait for the click handlers to fire from JS.
  ExtensionTestMessageListener click_listener("ITEM_CLICKED", false);
  GURL page_url("http://www.google.com");
  // Create and build our test context menu.
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(guest_web_contents, page_url, GURL(),
                                        GURL()));
  // Look for the extension item in the menu, and execute it.
  int command_id = ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
  ASSERT_TRUE(menu->IsCommandIdEnabled(command_id));
  menu->ExecuteCommand(command_id, 0);
  // Wait for embedder's script to tell us its onclick fired, it does
  // chrome.test.sendMessage('ITEM_CLICKED')
  ASSERT_TRUE(click_listener.WaitUntilSatisfied());

  // 4. Update the item's title and verify.
  ExecuteScriptWaitForTitle(embedder, "updateMenuItem()", "ITEM_UPDATED");
  MenuItem::List items = GetItems();
  ASSERT_EQ(1u, items.size());
  MenuItem* item = items.at(0);
  EXPECT_EQ("new_title", item->title());

  // 5. Remove the item.
  ExecuteScriptWaitForTitle(embedder, "removeItem()", "ITEM_REMOVED");
  MenuItem::List items_after_removal = GetItems();
  ASSERT_EQ(0u, items_after_removal.size());

  // 6. Add some more items.
  ExecuteScriptWaitForTitle(
      embedder, "createThreeMenuItems()", "ITEM_MULTIPLE_CREATED");
  MenuItem::List items_after_insertion = GetItems();
  ASSERT_EQ(3u, items_after_insertion.size());

  // 7. Test removeAll().
  ExecuteScriptWaitForTitle(embedder, "removeAllItems()", "ITEM_ALL_REMOVED");
  MenuItem::List items_after_all_removal = GetItems();
  ASSERT_EQ(0u, items_after_all_removal.size());
}

IN_PROC_BROWSER_TEST_F(WebViewTest, ContextMenusAPI_PreventDefault) {
  LoadAppWithGuest("web_view/context_menus/basic");

  content::WebContents* guest_web_contents = GetGuestWebContents();
  content::WebContents* embedder = GetEmbedderWebContents();
  ASSERT_TRUE(embedder);

  // Add a preventDefault() call on context menu event so context menu
  // does not show up.
  ExtensionTestMessageListener prevent_default_listener(
      "WebViewTest.CONTEXT_MENU_DEFAULT_PREVENTED", false);
  EXPECT_TRUE(content::ExecuteScript(embedder, "registerPreventDefault()"));
  ContextMenuShownObserver context_menu_shown_observer;

  OpenContextMenu(guest_web_contents);

  EXPECT_TRUE(prevent_default_listener.WaitUntilSatisfied());
  // Expect the menu to not show up.
  EXPECT_EQ(false, context_menu_shown_observer.shown());

  // Now remove the preventDefault() and expect context menu to be shown.
  ExecuteScriptWaitForTitle(
      embedder, "removePreventDefault()", "PREVENT_DEFAULT_LISTENER_REMOVED");
  OpenContextMenu(guest_web_contents);

  // We expect to see a context menu for the second call to |OpenContextMenu|.
  context_menu_shown_observer.Wait();
  EXPECT_EQ(true, context_menu_shown_observer.shown());
}

// Tests that a context menu is created when right-clicking in the webview. This
// also tests that the 'contextmenu' event is handled correctly.
IN_PROC_BROWSER_TEST_F(WebViewTest, TestContextMenu) {
  LoadAppWithGuest("web_view/context_menus/basic");
  content::WebContents* guest_web_contents = GetGuestWebContents();

  auto close_menu_and_stop_run_loop = [](base::OnceClosure closure,
                                         RenderViewContextMenu* context_menu) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&RenderViewContextMenuBase::Cancel,
                                  base::Unretained(context_menu)));
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(closure));
  };

  base::RunLoop run_loop;
  RenderViewContextMenu::RegisterMenuShownCallbackForTesting(
      base::BindOnce(close_menu_and_stop_run_loop, run_loop.QuitClosure()));

  OpenContextMenu(guest_web_contents);

  // Wait for the context menu to be visible.
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebViewTest, MediaAccessAPIAllow_TestAllow) {
  MediaAccessAPIAllowTestHelper("testAllow");
}

IN_PROC_BROWSER_TEST_F(WebViewTest, MediaAccessAPIAllow_TestAllowAndThenDeny) {
  MediaAccessAPIAllowTestHelper("testAllowAndThenDeny");
}

IN_PROC_BROWSER_TEST_F(WebViewTest, MediaAccessAPIAllow_TestAllowTwice) {
  MediaAccessAPIAllowTestHelper("testAllowTwice");
}

IN_PROC_BROWSER_TEST_F(WebViewTest, MediaAccessAPIAllow_TestAllowAsync) {
  MediaAccessAPIAllowTestHelper("testAllowAsync");
}

IN_PROC_BROWSER_TEST_F(WebViewTest, MediaAccessAPIAllow_TestCheck) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  LoadAndLaunchPlatformApp("web_view/media_access/check", "Launched");

  content::WebContents* embedder_web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(embedder_web_contents);
  std::unique_ptr<MockWebContentsDelegate> mock(new MockWebContentsDelegate());
  embedder_web_contents->SetDelegate(mock.get());

  ExtensionTestMessageListener done_listener("TEST_PASSED", false);
  done_listener.set_failure_message("TEST_FAILED");
  EXPECT_TRUE(
      content::ExecuteScript(
          embedder_web_contents,
          base::StringPrintf("startCheckTest('')")));
  ASSERT_TRUE(done_listener.WaitUntilSatisfied());

  mock->WaitForCheckMediaPermission();
}

// Checks that window.screenX/screenY/screenLeft/screenTop works correctly for
// guests.
IN_PROC_BROWSER_TEST_F(WebViewTest, ScreenCoordinates) {
  ASSERT_TRUE(RunPlatformAppTestWithArg(
      "platform_apps/web_view/common", "screen_coordinates"))
          << message_;
}

// TODO(1057340): This test leaks memory.
#if defined(LEAK_SANITIZER)
#define MAYBE_TearDownTest DISABLED_TearDownTest
#else
#define MAYBE_TearDownTest TearDownTest
#endif
IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_TearDownTest) {
  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("web_view/simple", "WebViewTest.LAUNCHED");
  extensions::AppWindow* window = nullptr;
  if (!GetAppWindowCount())
    window = CreateAppWindow(browser()->profile(), extension);
  else
    window = GetFirstAppWindow();
  CloseAppWindow(window);

  // Load the app again.
  LoadAndLaunchPlatformApp("web_view/simple", "WebViewTest.LAUNCHED");
}

// Tests that an app can inject a content script into a webview, and that it can
// send cross-origin requests with CORS headers.
IN_PROC_BROWSER_TEST_F(WebViewTest, ContentScriptFetch) {
  TestHelper("testContentScriptFetch", "web_view/content_script_fetch",
             NEEDS_TEST_SERVER);
}

// In following GeolocationAPIEmbedderHasNoAccess* tests, embedder (i.e. the
// platform app) does not have geolocation permission for this test.
// No matter what the API does, geolocation permission would be denied.
// Note that the test name prefix must be "GeolocationAPI".
IN_PROC_BROWSER_TEST_F(WebViewTest, GeolocationAPIEmbedderHasNoAccessAllow) {
  TestHelper("testDenyDenies",
             "web_view/geolocation/embedder_has_no_permission",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, GeolocationAPIEmbedderHasNoAccessDeny) {
  TestHelper("testDenyDenies",
             "web_view/geolocation/embedder_has_no_permission",
             NEEDS_TEST_SERVER);
}

// In following GeolocationAPIEmbedderHasAccess* tests, embedder (i.e. the
// platform app) has geolocation permission
//
// Note that these test names must be "GeolocationAPI" prefixed (b/c we mock out
// geolocation in this case).
//
// Also note that these are run separately because OverrideGeolocation() doesn't
// mock out geolocation for multiple navigator.geolocation calls properly and
// the tests become flaky.
//
// GeolocationAPI* test 1 of 3.
// Currently disabled until crbug.com/526788 is fixed.
IN_PROC_BROWSER_TEST_F(WebViewTest,
                       DISABLED_GeolocationAPIEmbedderHasAccessAllow) {
  TestHelper("testAllow",
             "web_view/geolocation/embedder_has_permission",
             NEEDS_TEST_SERVER);
}

// GeolocationAPI* test 2 of 3.
IN_PROC_BROWSER_TEST_F(WebViewTest, GeolocationAPIEmbedderHasAccessDeny) {
  TestHelper("testDeny",
             "web_view/geolocation/embedder_has_permission",
             NEEDS_TEST_SERVER);
}

// GeolocationAPI* test 3 of 3.
// Currently disabled until crbug.com/526788 is fixed.
IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    DISABLED_GeolocationAPIEmbedderHasAccessMultipleBridgeIdAllow) {
  TestHelper("testMultipleBridgeIdAllow",
             "web_view/geolocation/embedder_has_permission",
             NEEDS_TEST_SERVER);
}

// Tests that
// BrowserPluginGeolocationPermissionContext::CancelGeolocationPermissionRequest
// is handled correctly (and does not crash).
IN_PROC_BROWSER_TEST_F(WebViewTest, GeolocationAPICancelGeolocation) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunPlatformAppTest(
        "platform_apps/web_view/geolocation/cancel_request")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, DISABLED_GeolocationRequestGone) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunPlatformAppTest(
        "platform_apps/web_view/geolocation/geolocation_request_gone"))
            << message_;
}

// In following FilesystemAPIRequestFromMainThread* tests, guest request
// filesystem access from main thread of the guest.
// FileSystemAPIRequestFromMainThread* test 1 of 3
IN_PROC_BROWSER_TEST_F(WebViewTest, FileSystemAPIRequestFromMainThreadAllow) {
  TestHelper("testAllow", "web_view/filesystem/main", NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromMainThread* test 2 of 3.
IN_PROC_BROWSER_TEST_F(WebViewTest, FileSystemAPIRequestFromMainThreadDeny) {
  TestHelper("testDeny", "web_view/filesystem/main", NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromMainThread* test 3 of 3.
IN_PROC_BROWSER_TEST_F(WebViewTest,
                       FileSystemAPIRequestFromMainThreadDefaultAllow) {
  TestHelper("testDefaultAllow", "web_view/filesystem/main", NEEDS_TEST_SERVER);
}

// In following FilesystemAPIRequestFromWorker* tests, guest create a worker
// to request filesystem access from worker thread.
// FileSystemAPIRequestFromWorker* test 1 of 3
IN_PROC_BROWSER_TEST_F(WebViewTest, FileSystemAPIRequestFromWorkerAllow) {
  TestHelper("testAllow", "web_view/filesystem/worker", NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromWorker* test 2 of 3.
IN_PROC_BROWSER_TEST_F(WebViewTest, FileSystemAPIRequestFromWorkerDeny) {
  TestHelper("testDeny", "web_view/filesystem/worker", NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromWorker* test 3 of 3.
IN_PROC_BROWSER_TEST_F(WebViewTest,
                       FileSystemAPIRequestFromWorkerDefaultAllow) {
  TestHelper(
      "testDefaultAllow", "web_view/filesystem/worker", NEEDS_TEST_SERVER);
}

// In following FilesystemAPIRequestFromSharedWorkerOfSingleWebViewGuest* tests,
// embedder contains a single webview guest. The guest creates a shared worker
// to request filesystem access from worker thread.
// FileSystemAPIRequestFromSharedWorkerOfSingleWebViewGuest* test 1 of 3
IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    FileSystemAPIRequestFromSharedWorkerOfSingleWebViewGuestAllow) {
  TestHelper("testAllow",
             "web_view/filesystem/shared_worker/single",
             NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromSharedWorkerOfSingleWebViewGuest* test 2 of 3.
IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    FileSystemAPIRequestFromSharedWorkerOfSingleWebViewGuestDeny) {
  TestHelper("testDeny",
             "web_view/filesystem/shared_worker/single",
             NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromSharedWorkerOfSingleWebViewGuest* test 3 of 3.
IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    FileSystemAPIRequestFromSharedWorkerOfSingleWebViewGuestDefaultAllow) {
  TestHelper(
      "testDefaultAllow",
      "web_view/filesystem/shared_worker/single",
      NEEDS_TEST_SERVER);
}

// In following FilesystemAPIRequestFromSharedWorkerOfMultiWebViewGuests* tests,
// embedder contains mutiple webview guests. Each guest creates a shared worker
// to request filesystem access from worker thread.
// FileSystemAPIRequestFromSharedWorkerOfMultiWebViewGuests* test 1 of 3
IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    FileSystemAPIRequestFromSharedWorkerOfMultiWebViewGuestsAllow) {
  TestHelper("testAllow",
             "web_view/filesystem/shared_worker/multiple",
             NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromSharedWorkerOfMultiWebViewGuests* test 2 of 3.
IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    FileSystemAPIRequestFromSharedWorkerOfMultiWebViewGuestsDeny) {
  TestHelper("testDeny",
             "web_view/filesystem/shared_worker/multiple",
             NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromSharedWorkerOfMultiWebViewGuests* test 3 of 3.
IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    FileSystemAPIRequestFromSharedWorkerOfMultiWebViewGuestsDefaultAllow) {
  TestHelper(
      "testDefaultAllow",
      "web_view/filesystem/shared_worker/multiple",
      NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, ClearData) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(
      RunPlatformAppTestWithArg("platform_apps/web_view/common", "cleardata"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, ClearSessionCookies) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunPlatformAppTestWithArg("platform_apps/web_view/common",
                                        "cleardata_session"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, ClearPersistentCookies) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunPlatformAppTestWithArg("platform_apps/web_view/common",
                                        "cleardata_persistent"))
      << message_;
}

// Regression test for https://crbug.com/615429.
IN_PROC_BROWSER_TEST_F(WebViewTest, ClearDataTwice) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunPlatformAppTestWithArg("platform_apps/web_view/common",
                                        "cleardata_twice"))
      << message_;
}

#if defined(OS_WIN)
// Test is disabled on Windows because it fails often (~9% time)
// http://crbug.com/489088
#define MAYBE_ClearDataCache DISABLED_ClearDataCache
#else
#define MAYBE_ClearDataCache ClearDataCache
#endif
IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_ClearDataCache) {
  TestHelper("testClearCache", "web_view/clear_data_cache", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, ConsoleMessage) {
  ASSERT_TRUE(RunPlatformAppTestWithArg(
      "platform_apps/web_view/common", "console_messages"))
          << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, DownloadPermission) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  content::WebContents* guest_web_contents =
      LoadGuest("/extensions/platform_apps/web_view/download/guest.html",
                "web_view/download");
  ASSERT_TRUE(guest_web_contents);

  std::unique_ptr<content::DownloadTestObserver> completion_observer(
      new content::DownloadTestObserverTerminal(
          content::BrowserContext::GetDownloadManager(
              guest_web_contents->GetBrowserContext()),
          1, content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));

  // Replace WebContentsDelegate with mock version so we can intercept download
  // requests.
  content::WebContentsDelegate* delegate = guest_web_contents->GetDelegate();
  std::unique_ptr<MockDownloadWebContentsDelegate> mock_delegate(
      new MockDownloadWebContentsDelegate(delegate));
  guest_web_contents->SetDelegate(mock_delegate.get());

  // Start test.
  // 1. Guest requests a download that its embedder denies.
  EXPECT_TRUE(content::ExecuteScript(guest_web_contents,
                                     "startDownload('download-link-1')"));
  mock_delegate->WaitForCanDownload(false);  // Expect to not allow.
  mock_delegate->Reset();

  // 2. Guest requests a download that its embedder allows.
  EXPECT_TRUE(content::ExecuteScript(guest_web_contents,
                                     "startDownload('download-link-2')"));
  mock_delegate->WaitForCanDownload(true);  // Expect to allow.
  mock_delegate->Reset();

  // 3. Guest requests a download that its embedder ignores, this implies deny.
  EXPECT_TRUE(content::ExecuteScript(guest_web_contents,
                                     "startDownload('download-link-3')"));
  mock_delegate->WaitForCanDownload(false);  // Expect to not allow.
  completion_observer->WaitForFinished();
}

namespace {

const char kDownloadPathPrefix[] = "/download_cookie_isolation_test";

// EmbeddedTestServer request handler for use with DownloadCookieIsolation test.
// Responds with the next status code 200 if the 'Cookie' header sent with the
// request matches the query() part of the URL. Otherwise, fails the request
// with an HTTP 403. The body of the response is the value of the Cookie
// header.
std::unique_ptr<net::test_server::HttpResponse> HandleDownloadRequestWithCookie(
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.relative_url, kDownloadPathPrefix,
                        base::CompareCase::SENSITIVE)) {
    return std::unique_ptr<net::test_server::HttpResponse>();
  }

  std::string cookie_to_expect = request.GetURL().query();
  const auto cookie_header_it = request.headers.find("cookie");
  std::unique_ptr<net::test_server::BasicHttpResponse> response;

  // Return a 403 if there's no cookie or if the cookie doesn't match.
  if (cookie_header_it == request.headers.end() ||
      cookie_header_it->second != cookie_to_expect) {
    response.reset(new net::test_server::BasicHttpResponse);
    response->set_code(net::HTTP_FORBIDDEN);
    response->set_content_type("text/plain");
    response->set_content("Forbidden");
    return std::move(response);
  }

  // We have a cookie. Send some content along with the next status code.
  response.reset(new net::test_server::BasicHttpResponse);
  response->set_code(net::HTTP_OK);
  response->set_content_type("application/octet-stream");
  response->set_content(cookie_to_expect);
  return std::move(response);
}

// Class for waiting for download manager to be initiailized.
class DownloadManagerWaiter : public content::DownloadManager::Observer {
 public:
  explicit DownloadManagerWaiter(content::DownloadManager* download_manager)
      : initialized_(false), download_manager_(download_manager) {
    download_manager_->AddObserver(this);
  }

  ~DownloadManagerWaiter() override { download_manager_->RemoveObserver(this); }

  void WaitForInitialized() {
    if (initialized_)
      return;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void OnManagerInitialized() override {
    initialized_ = true;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

 private:
  base::Closure quit_closure_;
  bool initialized_;
  content::DownloadManager* download_manager_;
};

}  // namespace

// TODO(crbug.com/994789): Flaky on MSan, Linux, and Chrome OS.
#if defined(MEMORY_SANITIZER) || defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_DownloadCookieIsolation DISABLED_DownloadCookieIsolation
#else
#define MAYBE_DownloadCookieIsolation DownloadCookieIsolation
#endif  // !defined(MEMORY_SANITIZER)
// Downloads initiated from isolated guest parititons should use their
// respective cookie stores. In addition, if those downloads are resumed, they
// should continue to use their respective cookie stores.
IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_DownloadCookieIsolation) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleDownloadRequestWithCookie));
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  LoadAndLaunchPlatformApp("web_view/download_cookie_isolation",
                           "created-webviews");

  content::WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);

  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(
          web_contents->GetBrowserContext());

  scoped_refptr<content::TestFileErrorInjector> error_injector(
      content::TestFileErrorInjector::Create(download_manager));

  content::TestFileErrorInjector::FileErrorInfo error_info(
      content::TestFileErrorInjector::FILE_OPERATION_STREAM_COMPLETE, 0,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED);
  error_info.stream_offset = 0;
  error_injector->InjectError(error_info);

  std::unique_ptr<content::DownloadTestObserver> interrupted_observer(
      new content::DownloadTestObserverInterrupted(
          download_manager, 2,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));

  EXPECT_TRUE(content::ExecuteScript(
      web_contents,
      base::StringPrintf(
          "startDownload('first', '%s?cookie=first')",
          embedded_test_server()->GetURL(kDownloadPathPrefix).spec().c_str())));

  EXPECT_TRUE(content::ExecuteScript(
      web_contents,
      base::StringPrintf(
          "startDownload('second', '%s?cookie=second')",
          embedded_test_server()->GetURL(kDownloadPathPrefix).spec().c_str())));

  // Both downloads should fail due to the error that was injected above to the
  // download manager. This maps to DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED.
  interrupted_observer->WaitForFinished();

  error_injector->ClearError();

  content::DownloadManager::DownloadVector downloads;
  download_manager->GetAllDownloads(&downloads);
  ASSERT_EQ(2u, downloads.size());

  CloseAppWindow(GetFirstAppWindow());

  std::unique_ptr<content::DownloadTestObserver> completion_observer(
      new content::DownloadTestObserverTerminal(
          download_manager, 2,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));

  for (auto* download : downloads) {
    ASSERT_TRUE(download->CanResume());
    EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED,
              download->GetLastReason());
    download->Resume(false);
  }

  completion_observer->WaitForFinished();

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::set<std::string> cookies;
  for (auto* download : downloads) {
    ASSERT_EQ(download::DownloadItem::COMPLETE, download->GetState());
    ASSERT_TRUE(base::PathExists(download->GetTargetFilePath()));
    std::string content;
    ASSERT_TRUE(
        base::ReadFileToString(download->GetTargetFilePath(), &content));
    // Note that the contents of the file is the value of the cookie.
    EXPECT_EQ(content, download->GetURL().query());
    cookies.insert(content);
  }

  ASSERT_EQ(2u, cookies.size());
  ASSERT_TRUE(cookies.find("cookie=first") != cookies.end());
  ASSERT_TRUE(cookies.find("cookie=second") != cookies.end());
}

IN_PROC_BROWSER_TEST_F(WebViewTest, PRE_DownloadCookieIsolation_CrossSession) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleDownloadRequestWithCookie));
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  LoadAndLaunchPlatformApp("web_view/download_cookie_isolation",
                           "created-webviews");
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner(
      new base::TestMockTimeTaskRunner);
  download::SetDownloadDBTaskRunnerForTesting(task_runner);

  content::WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);

  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(
          web_contents->GetBrowserContext());
  std::unique_ptr<content::DownloadTestObserver> interrupted_observer(
      new content::DownloadTestObserverInterrupted(
          download_manager, 2,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));

  scoped_refptr<content::TestFileErrorInjector> error_injector(
      content::TestFileErrorInjector::Create(download_manager));

  content::TestFileErrorInjector::FileErrorInfo error_info(
      content::TestFileErrorInjector::FILE_OPERATION_STREAM_COMPLETE, 0,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED);
  error_info.stream_offset = 0;
  error_injector->InjectError(error_info);

  EXPECT_TRUE(content::ExecuteScript(
      web_contents,
      base::StringPrintf(
          "startDownload('first', '%s?cookie=first')",
          embedded_test_server()->GetURL(kDownloadPathPrefix).spec().c_str())));

  // Note that the second webview uses an in-memory partition.
  EXPECT_TRUE(content::ExecuteScript(
      web_contents,
      base::StringPrintf(
          "startDownload('second', '%s?cookie=second')",
          embedded_test_server()->GetURL(kDownloadPathPrefix).spec().c_str())));

  // Both downloads should fail due to the error that was injected above to the
  // download manager. This maps to DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED.
  interrupted_observer->WaitForFinished();

  // Wait for both downloads to be stored.
  task_runner->FastForwardUntilNoTasksRemain();

  content::EnsureCookiesFlushed(profile());
}

// TODO(crbug.com/994789): Flaky on MSan, Linux, and ChromeOS.
#if defined(MEMORY_SANITIZER) || defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_DownloadCookieIsolation_CrossSession \
  DISABLED_DownloadCookieIsolation_CrossSession
#else
#define MAYBE_DownloadCookieIsolation_CrossSession \
  DownloadCookieIsolation_CrossSession
#endif  // !defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       MAYBE_DownloadCookieIsolation_CrossSession) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleDownloadRequestWithCookie));
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner(
      new base::TestMockTimeTaskRunner);
  download::SetDownloadDBTaskRunnerForTesting(task_runner);

  content::BrowserContext* browser_context = profile();
  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(browser_context);

  task_runner->FastForwardUntilNoTasksRemain();
  DownloadManagerWaiter waiter(download_manager);
  waiter.WaitForInitialized();
  content::DownloadManager::DownloadVector saved_downloads;
  download_manager->GetAllDownloads(&saved_downloads);
  ASSERT_EQ(2u, saved_downloads.size());

  content::DownloadManager::DownloadVector downloads;
  // We can't trivially resume the previous downloads because they are going to
  // try to talk to the old EmbeddedTestServer instance. We need to update the
  // URL to point to the new instance, which should only differ by the port
  // number.
  for (auto* download : saved_downloads) {
    const std::string port_string =
        base::NumberToString(embedded_test_server()->port());
    url::Replacements<char> replacements;
    replacements.SetPort(port_string.c_str(),
                         url::Component(0, port_string.size()));
    std::vector<GURL> url_chain;
    url_chain.push_back(download->GetURL().ReplaceComponents(replacements));

    downloads.push_back(download_manager->CreateDownloadItem(
        base::GenerateGUID(), download->GetId() + 2, download->GetFullPath(),
        download->GetTargetFilePath(), url_chain, download->GetReferrerUrl(),
        download->GetSiteUrl(), download->GetTabUrl(),
        download->GetTabReferrerUrl(), download->GetRequestInitiator(),
        download->GetMimeType(), download->GetOriginalMimeType(),
        download->GetStartTime(), download->GetEndTime(), download->GetETag(),
        download->GetLastModifiedTime(), download->GetReceivedBytes(),
        download->GetTotalBytes(), download->GetHash(), download->GetState(),
        download->GetDangerType(), download->GetLastReason(),
        download->GetOpened(), download->GetLastAccessTime(),
        download->IsTransient(), download->GetReceivedSlices()));
  }

  std::unique_ptr<content::DownloadTestObserver> completion_observer(
      new content::DownloadTestObserverTerminal(
          download_manager, 2,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));

  for (auto* download : downloads) {
    ASSERT_TRUE(download->CanResume());
    ASSERT_TRUE(download->GetFullPath().empty());
    EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED,
              download->GetLastReason());
    download->Resume(true);
  }

  completion_observer->WaitForFinished();

  // Of the two downloads, ?cookie=first will succeed and ?cookie=second will
  // fail. The latter fails because the underlying storage partition was not
  // persisted.

  download::DownloadItem* succeeded_download = downloads[0];
  download::DownloadItem* failed_download = downloads[1];

  if (downloads[0]->GetState() == download::DownloadItem::INTERRUPTED)
    std::swap(succeeded_download, failed_download);

  ASSERT_EQ(download::DownloadItem::COMPLETE, succeeded_download->GetState());

  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::PathExists(succeeded_download->GetTargetFilePath()));
  std::string content;
  ASSERT_TRUE(base::ReadFileToString(succeeded_download->GetTargetFilePath(),
                                     &content));
  // This is the cookie that should've been stored in the persisted storage
  // partition.
  EXPECT_STREQ("cookie=first", content.c_str());

  ASSERT_EQ(download::DownloadItem::INTERRUPTED, failed_download->GetState());
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN,
            failed_download->GetLastReason());
}

// This test makes sure loading <webview> does not crash when there is an
// extension which has content script allowlisted/forced.
IN_PROC_BROWSER_TEST_F(WebViewTest, AllowlistedContentScript) {
  // Allowlist the extension for running content script we are going to load.
  extensions::ExtensionsClient::ScriptingAllowlist allowlist;
  const std::string extension_id = "imeongpbjoodlnmlakaldhlcmijmhpbb";
  allowlist.push_back(extension_id);
  extensions::ExtensionsClient::Get()->SetScriptingAllowlist(allowlist);

  // Load the extension.
  const extensions::Extension* content_script_allowlisted_extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "platform_apps/web_view/extension_api/content_script"));
  ASSERT_TRUE(content_script_allowlisted_extension);
  ASSERT_EQ(extension_id, content_script_allowlisted_extension->id());

  // Now load an app with <webview>.
  LoadAndLaunchPlatformApp("web_view/content_script_allowlisted",
                           "TEST_PASSED");
}

IN_PROC_BROWSER_TEST_F(WebViewTest, SendMessageToExtensionFromGuest) {
  // Load the extension as a normal, non-component extension.
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "platform_apps/web_view/extension_api/component_extension"));
  ASSERT_TRUE(extension);

  TestHelper("testNonComponentExtension", "web_view/component_extension",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, SendMessageToComponentExtensionFromGuest) {
  const extensions::Extension* component_extension =
      LoadExtensionAsComponent(test_data_dir_.AppendASCII(
          "platform_apps/web_view/extension_api/component_extension"));
  ASSERT_TRUE(component_extension);

  TestHelper("testComponentExtension", "web_view/component_extension",
             NEEDS_TEST_SERVER);

  content::WebContents* embedder_web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(embedder_web_contents);

  // Retrive the guestProcessId and guestRenderFrameRoutingId from the
  // extension.
  int guest_process_id =
      content::ExecuteScriptAndGetValue(embedder_web_contents->GetMainFrame(),
                                        "window.guestProcessId")
          .GetInt();
  int guest_render_frame_routing_id =
      content::ExecuteScriptAndGetValue(embedder_web_contents->GetMainFrame(),
                                        "window.guestRenderFrameRoutingId")
          .GetInt();

  auto* guest_rfh = content::RenderFrameHost::FromID(
      guest_process_id, guest_render_frame_routing_id);
  // Verify that the guest related info (guest_process_id and
  // guest_render_frame_routing_id) actually points to a WebViewGuest.
  ASSERT_TRUE(extensions::WebViewGuest::FromWebContents(
      content::WebContents::FromRenderFrameHost(guest_rfh)));
}

IN_PROC_BROWSER_TEST_F(WebViewTest, SetPropertyOnDocumentReady) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/document_ready"))
                  << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, SetPropertyOnDocumentInteractive) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/document_interactive"))
                  << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewSpeechAPITest,
                       SpeechRecognitionAPI_HasPermissionAllow) {
  ASSERT_TRUE(
      RunPlatformAppTestWithArg("platform_apps/web_view/speech_recognition_api",
                                "allowTest"))
          << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewSpeechAPITest,
                       SpeechRecognitionAPI_HasPermissionDeny) {
  ASSERT_TRUE(
      RunPlatformAppTestWithArg("platform_apps/web_view/speech_recognition_api",
                                "denyTest"))
          << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewSpeechAPITest,
                       SpeechRecognitionAPI_NoPermission) {
  ASSERT_TRUE(
      RunPlatformAppTestWithArg("platform_apps/web_view/common",
                                "speech_recognition_api_no_permission"))
          << message_;
}

// Tests overriding user agent.
IN_PROC_BROWSER_TEST_F(WebViewTest, UserAgent) {
  ASSERT_TRUE(RunPlatformAppTestWithArg(
              "platform_apps/web_view/common", "useragent")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest, UserAgent_NewWindow) {
  ASSERT_TRUE(RunPlatformAppTestWithArg(
              "platform_apps/web_view/common",
              "useragent_newwindow")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, NoPermission) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/nopermission"))
                  << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Dialog_TestAlertDialog) {
  TestHelper("testAlertDialog", "web_view/dialog", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, TestConfirmDialog) {
  TestHelper("testConfirmDialog", "web_view/dialog", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Dialog_TestConfirmDialogCancel) {
  TestHelper("testConfirmDialogCancel", "web_view/dialog", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Dialog_TestConfirmDialogDefaultCancel) {
  TestHelper("testConfirmDialogDefaultCancel",
             "web_view/dialog",
             NO_TEST_SERVER);
}

// Disable due to runloop time out. https://crbug.com/937461
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_Dialog_TestConfirmDialogDefaultGCCancel \
  DISABLED_Dialog_TestConfirmDialogDefaultGCCancel
#else
#define MAYBE_Dialog_TestConfirmDialogDefaultGCCancel \
  Dialog_TestConfirmDialogDefaultGCCancel
#endif
IN_PROC_BROWSER_TEST_F(WebViewTest,
                       MAYBE_Dialog_TestConfirmDialogDefaultGCCancel) {
  TestHelper("testConfirmDialogDefaultGCCancel",
             "web_view/dialog",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Dialog_TestPromptDialog) {
  TestHelper("testPromptDialog", "web_view/dialog", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, NoContentSettingsAPI) {
  // Load the extension.
  const extensions::Extension* content_settings_extension =
      LoadExtension(
          test_data_dir_.AppendASCII(
              "platform_apps/web_view/extension_api/content_settings"));
  ASSERT_TRUE(content_settings_extension);
  TestHelper("testPostMessageCommChannel", "web_view/shim", NO_TEST_SERVER);
}

#if BUILDFLAG(ENABLE_PLUGINS)
class WebViewPluginTest : public WebViewTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebViewTest::SetUpCommandLine(command_line);
    ASSERT_TRUE(ppapi::RegisterTestPlugin(command_line));
  }
};

IN_PROC_BROWSER_TEST_F(WebViewPluginTest, TestLoadPluginEvent) {
  TestHelper("testPluginLoadPermission", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewPluginTest, TestLoadPluginInternalResource) {
  const char kTestMimeType[] = "application/pdf";
  const char kTestFileType[] = "pdf";
  content::WebPluginInfo plugin_info;
  plugin_info.type = content::WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS;
  plugin_info.mime_types.push_back(
      content::WebPluginMimeType(kTestMimeType, kTestFileType, std::string()));
  content::PluginService::GetInstance()->RegisterInternalPlugin(plugin_info,
                                                                true);

  TestHelper("testPluginLoadInternalResource", "web_view/shim", NO_TEST_SERVER);
  // Sanity check to ensure no GuestView was created.
  for (auto* guest_wc : GetEmbedderWebContents()->GetInnerWebContents()) {
    EXPECT_FALSE(extensions::MimeHandlerViewEmbedder::Get(
        guest_wc->GetMainFrame()->GetFrameTreeNodeId()));
  }
}

#endif  // BUILDFLAG(ENABLE_PLUGINS)

class WebViewCaptureTest : public WebViewTest {
 public:
  WebViewCaptureTest() {}
  ~WebViewCaptureTest() override {}
  void SetUp() override {
    EnablePixelOutput();
    WebViewTest::SetUp();
  }
};

// https://crbug.com/1087381
#if defined(OS_CHROMEOS) || (defined(OS_LINUX) && defined(ADDRESS_SANITIZER))
#define MAYBE_Shim_TestZoomAPI DISABLED_Shim_TestZoomAPI
#else
#define MAYBE_Shim_TestZoomAPI Shim_TestZoomAPI
#endif
IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_Shim_TestZoomAPI) {
  TestHelper("testZoomAPI", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestFindAPI) {
  TestHelper("testFindAPI", "web_view/shim", NO_TEST_SERVER);
}

// crbug.com/710486
#if defined(MEMORY_SANITIZER)
#define MAYBE_Shim_TestFindAPI_findupdate DISABLED_Shim_TestFindAPI_findupdate
#else
#define MAYBE_Shim_TestFindAPI_findupdate Shim_TestFindAPI_findupdate
#endif
IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_Shim_TestFindAPI_findupdate) {
  TestHelper("testFindAPI_findupdate", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_testFindInMultipleWebViews) {
  TestHelper("testFindInMultipleWebViews", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestLoadDataAPI) {
  TestHelper("testLoadDataAPI", "web_view/shim", NEEDS_TEST_SERVER);
}

// This test verifies that the resize and contentResize events work correctly.
IN_PROC_BROWSER_TEST_F(WebViewSizeTest, Shim_TestResizeEvents) {
  TestHelper("testResizeEvents", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestPerOriginZoomMode) {
  TestHelper("testPerOriginZoomMode", "web_view/shim", NO_TEST_SERVER);
}

// TODO(crbug.com/935665): Test has flaky failures on all platforms.
IN_PROC_BROWSER_TEST_F(WebViewTest, DISABLED_Shim_TestPerViewZoomMode) {
  TestHelper("testPerViewZoomMode", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestDisabledZoomMode) {
  TestHelper("testDisabledZoomMode", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestZoomBeforeNavigation) {
  TestHelper("testZoomBeforeNavigation", "web_view/shim", NO_TEST_SERVER);
}

// Test fixture to run the test on multiple channels.
class WebViewChannelTest
    : public WebViewTest,
      public testing::WithParamInterface<version_info::Channel> {
 public:
  WebViewChannelTest() : channel_(GetParam()) {}
  WebViewChannelTest(const WebViewChannelTest&) = delete;
  WebViewChannelTest& operator=(const WebViewChannelTest&) = delete;

 private:
  extensions::ScopedCurrentChannel channel_;
};

// This test verify that the set of rules registries of a webview will be
// removed from RulesRegistryService after the webview is gone.
// http://crbug.com/438327
IN_PROC_BROWSER_TEST_P(
    WebViewChannelTest,
    DISABLED_Shim_TestRulesRegistryIDAreRemovedAfterWebViewIsGone) {
  ASSERT_EQ(extensions::GetCurrentChannel(), GetParam());
  SCOPED_TRACE(
      base::StringPrintf("Testing Channel %s",
                         version_info::GetChannelString(GetParam()).c_str()));

  LoadAppWithGuest("web_view/rules_registry");

  content::WebContents* embedder_web_contents = GetEmbedderWebContents();
  ASSERT_TRUE(embedder_web_contents);
  std::unique_ptr<EmbedderWebContentsObserver> observer(
      new EmbedderWebContentsObserver(embedder_web_contents));

  content::WebContents* guest_web_contents = GetGuestWebContents();
  ASSERT_TRUE(guest_web_contents);
  extensions::WebViewGuest* guest =
      extensions::WebViewGuest::FromWebContents(guest_web_contents);
  ASSERT_TRUE(guest);

  // Register rule for the guest.
  Profile* profile = browser()->profile();
  int rules_registry_id =
      extensions::WebViewGuest::GetOrGenerateRulesRegistryID(
          guest->owner_web_contents()->GetMainFrame()->GetProcess()->GetID(),
          guest->view_instance_id());

  extensions::RulesRegistryService* registry_service =
      extensions::RulesRegistryService::Get(profile);
  extensions::TestRulesRegistry* rules_registry =
      new extensions::TestRulesRegistry(content::BrowserThread::UI, "ui",
                                        rules_registry_id);
  registry_service->RegisterRulesRegistry(base::WrapRefCounted(rules_registry));

  EXPECT_TRUE(
      registry_service->GetRulesRegistry(rules_registry_id, "ui").get());

  // Kill the embedder's render process, so the webview will go as well.
  base::Process process = base::Process::DeprecatedGetProcessFromHandle(
      embedder_web_contents->GetMainFrame()
          ->GetProcess()
          ->GetProcess()
          .Handle());
  process.Terminate(0, false);
  observer->WaitForEmbedderRenderProcessTerminate();

  EXPECT_FALSE(
      registry_service->GetRulesRegistry(rules_registry_id, "ui").get());
}

IN_PROC_BROWSER_TEST_P(WebViewChannelTest,
                       Shim_WebViewWebRequestRegistryHasNoPersistentCache) {
  ASSERT_EQ(extensions::GetCurrentChannel(), GetParam());
  SCOPED_TRACE(
      base::StringPrintf("Testing Channel %s",
                         version_info::GetChannelString(GetParam()).c_str()));

  LoadAppWithGuest("web_view/rules_registry");

  content::WebContents* guest_web_contents = GetGuestWebContents();
  ASSERT_TRUE(guest_web_contents);
  extensions::WebViewGuest* guest =
      extensions::WebViewGuest::FromWebContents(guest_web_contents);
  ASSERT_TRUE(guest);

  Profile* profile = browser()->profile();
  extensions::RulesRegistryService* registry_service =
      extensions::RulesRegistryService::Get(profile);
  int rules_registry_id =
      extensions::WebViewGuest::GetOrGenerateRulesRegistryID(
          guest->owner_web_contents()->GetMainFrame()->GetProcess()->GetID(),
          guest->view_instance_id());

  // Get an existing registered rule for the guest.
  extensions::RulesRegistry* registry =
      registry_service
          ->GetRulesRegistry(
              rules_registry_id,
              extensions::declarative_webrequest_constants::kOnRequest)
          .get();

  ASSERT_TRUE(registry);
  ASSERT_TRUE(registry->rules_cache_delegate_for_testing());
  EXPECT_EQ(extensions::RulesCacheDelegate::Type::kEphemeral,
            registry->rules_cache_delegate_for_testing()->type());
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebViewChannelTest,
                         testing::Values(version_info::Channel::UNKNOWN,
                                         version_info::Channel::STABLE));

// This test verifies that webview.contentWindow works inside an iframe.
IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestWebViewInsideFrame) {
  LoadAppWithGuest("web_view/inside_iframe");
}

// <webview> screenshot capture fails with ubercomp.
// See http://crbug.com/327035.
IN_PROC_BROWSER_TEST_F(WebViewCaptureTest, DISABLED_Shim_ScreenshotCapture) {
  TestHelper("testScreenshotCapture", "web_view/shim", NO_TEST_SERVER);
}

// Tests that browser process does not crash when loading plugin inside
// <webview> with content settings set to CONTENT_SETTING_BLOCK.
IN_PROC_BROWSER_TEST_F(WebViewTest, TestPlugin) {
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::PLUGINS,
                                 CONTENT_SETTING_BLOCK);
  TestHelper("testPlugin", "web_view/shim", NEEDS_TEST_SERVER);
}

// Test is disabled because it times out often.
// http://crbug.com/403325
IN_PROC_BROWSER_TEST_F(WebViewTest, DISABLED_WebViewInBackgroundPage) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("platform_apps/web_view/background"))
      << message_;
}

// This test verifies that the allowtransparency attribute properly propagates.
IN_PROC_BROWSER_TEST_F(WebViewTest, AllowTransparencyAndAllowScalingPropagate) {
  LoadAppWithGuest("web_view/simple");

  ASSERT_TRUE(GetGuestWebContents());
  extensions::WebViewGuest* guest =
      extensions::WebViewGuest::FromWebContents(GetGuestWebContents());
  ASSERT_TRUE(guest->allow_transparency());
  ASSERT_TRUE(guest->allow_scaling());
}

IN_PROC_BROWSER_TEST_F(WebViewTest, BasicPostMessage) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/post_message/basic"))
      << message_;
}

// Tests that webviews do get garbage collected.
// This test is disabled because it relies on garbage collections triggered from
// window.gc() to run precisely. This is not the case with unified heap where
// they need to conservatively scan the stack, potentially keeping objects
// alive. https://crbug.com/843903
IN_PROC_BROWSER_TEST_F(WebViewTest, DISABLED_Shim_TestGarbageCollect) {
  TestHelper("testGarbageCollect", "web_view/shim", NO_TEST_SERVER);
  GetGuestViewManager()->WaitForSingleViewGarbageCollected();
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestCloseNewWindowCleanup) {
  TestHelper("testCloseNewWindowCleanup", "web_view/shim", NEEDS_TEST_SERVER);
  auto* gvm = GetGuestViewManager();
  gvm->WaitForLastGuestDeleted();
  ASSERT_EQ(gvm->num_embedder_processes_destroyed(), 0);
}

// Ensure that focusing a WebView while it is already focused does not blur the
// guest content.
IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestFocusWhileFocused) {
  TestHelper("testFocusWhileFocused", "web_view/shim", NO_TEST_SERVER);
}

// TODO(crbug.com/776539): Disabled due to being flaky.
IN_PROC_BROWSER_TEST_F(WebViewTest, DISABLED_NestedGuestContainerBounds) {
  TestHelper("testPDFInWebview", "web_view/shim", NO_TEST_SERVER);

  std::vector<content::WebContents*> guest_web_contents_list;
  GetGuestViewManager()->WaitForNumGuestsCreated(2u);
  GetGuestViewManager()->GetGuestWebContentsList(&guest_web_contents_list);
  ASSERT_EQ(2u, guest_web_contents_list.size());

  content::WebContents* web_view_contents = guest_web_contents_list[0];
  content::WebContents* mime_handler_view_contents = guest_web_contents_list[1];

  // Make sure we've completed loading |mime_handler_view_guest|.
  bool load_success = pdf_extension_test_util::EnsurePDFHasLoaded(
      web_view_contents);
  EXPECT_TRUE(load_success);

  gfx::Rect web_view_container_bounds = web_view_contents->GetContainerBounds();
  gfx::Rect mime_handler_view_container_bounds =
      mime_handler_view_contents->GetContainerBounds();
  EXPECT_EQ(web_view_container_bounds.origin(),
            mime_handler_view_container_bounds.origin());
}

// Test that context menu Back/Forward items in a MimeHandlerViewGuest affect
// the embedder WebContents. See crbug.com/587355.
IN_PROC_BROWSER_TEST_F(WebViewTest, ContextMenuNavigationInMimeHandlerView) {
  TestHelper("testNavigateToPDFInWebview", "web_view/shim", NO_TEST_SERVER);

  std::vector<content::WebContents*> guest_web_contents_list;
  GetGuestViewManager()->WaitForNumGuestsCreated(2u);
  GetGuestViewManager()->GetGuestWebContentsList(&guest_web_contents_list);
  ASSERT_EQ(2u, guest_web_contents_list.size());

  content::WebContents* web_view_contents = guest_web_contents_list[0];
  content::WebContents* mime_handler_view_contents = guest_web_contents_list[1];
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_view_contents));

  // Ensure the <webview> has a previous entry, so we can navigate back to it.
  ASSERT_TRUE(web_view_contents->GetController().CanGoBack());

  // Open a context menu for the MimeHandlerViewGuest. Since the <webview> can
  // navigate back, the Back item should be enabled.
  content::ContextMenuParams params;
  TestRenderViewContextMenu menu(mime_handler_view_contents->GetMainFrame(),
                                 params);
  menu.Init();
  ASSERT_TRUE(menu.IsCommandIdEnabled(IDC_BACK));

  // Verify that the Back item causes the <webview> to navigate back to the
  // previous entry.
  content::TestNavigationObserver observer(web_view_contents);
  menu.ExecuteCommand(IDC_BACK, 0);
  observer.Wait();
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            web_view_contents->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestMailtoLink) {
  TestHelper("testMailtoLink", "web_view/shim", NEEDS_TEST_SERVER);
}

// Tests that a renderer navigation from an unattached guest that results in a
// server redirect works properly.
IN_PROC_BROWSER_TEST_F(WebViewTest,
                       Shim_TestRendererNavigationRedirectWhileUnattached) {
  TestHelper("testRendererNavigationRedirectWhileUnattached",
             "web_view/shim", NEEDS_TEST_SERVER);
}

// Tests that the embedder can create a blob URL and navigate a WebView to it.
// See https://crbug.com/652077.
// Also tests that the embedder can't navigate to a blob URL created by a
// WebView. See https://crbug.com/1106890.
IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestBlobURL) {
  TestHelper("testBlobURL", "web_view/shim", NEEDS_TEST_SERVER);
}

// Tests that no error page is shown when WebRequest blocks a navigation.
IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestWebRequestBlockedNavigation) {
  TestHelper("testWebRequestBlockedNavigation", "web_view/shim",
             NEEDS_TEST_SERVER);
}

// Tests that a WebView accessible resource can actually be loaded from a
// webpage in a WebView.
IN_PROC_BROWSER_TEST_F(WebViewTest, LoadWebviewAccessibleResource) {
  TestHelper("testLoadWebviewAccessibleResource",
             "web_view/load_webview_accessible_resource", NEEDS_TEST_SERVER);
}

// Tests that a WebView can be navigated to a WebView accessible resource.
IN_PROC_BROWSER_TEST_F(WebViewTest, NavigateGuestToWebviewAccessibleResource) {
  TestHelper("testNavigateGuestToWebviewAccessibleResource",
             "web_view/load_webview_accessible_resource", NO_TEST_SERVER);
}

// Tests that a WebView can reload a WebView accessible resource. See
// https://crbug.com/691941.
IN_PROC_BROWSER_TEST_F(WebViewTest, ReloadWebviewAccessibleResource) {
  TestHelper("testReloadWebviewAccessibleResource",
             "web_view/load_webview_accessible_resource", NEEDS_TEST_SERVER);

  content::WebContents* embedder_contents = GetEmbedderWebContents();
  content::WebContents* web_view_contents =
      GetGuestViewManager()->GetLastGuestCreated();
  ASSERT_TRUE(embedder_contents);
  ASSERT_TRUE(web_view_contents);

  GURL embedder_url(embedder_contents->GetLastCommittedURL());
  GURL webview_url(embedder_url.GetOrigin().spec() + "assets/foo.html");

  EXPECT_EQ(webview_url, web_view_contents->GetLastCommittedURL());
}

// Tests that a WebView can navigate an iframe to a blob URL that it creates
// while its main frame is at a WebView accessible resource.
IN_PROC_BROWSER_TEST_F(WebViewTest, BlobInWebviewAccessibleResource) {
  TestHelper("testBlobInWebviewAccessibleResource",
             "web_view/load_webview_accessible_resource", NEEDS_TEST_SERVER);

  content::WebContents* embedder_contents = GetEmbedderWebContents();
  content::WebContents* web_view_contents =
      GetGuestViewManager()->GetLastGuestCreated();
  ASSERT_TRUE(embedder_contents);
  ASSERT_TRUE(web_view_contents);

  GURL embedder_url(embedder_contents->GetLastCommittedURL());
  GURL webview_url(embedder_url.GetOrigin().spec() + "assets/foo.html");

  EXPECT_EQ(webview_url, web_view_contents->GetLastCommittedURL());

  content::RenderFrameHost* main_frame = web_view_contents->GetMainFrame();
  content::RenderFrameHost* blob_frame = ChildFrameAt(main_frame, 0);
  EXPECT_TRUE(blob_frame->GetLastCommittedURL().SchemeIsBlob());

  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      blob_frame,
      "window.domAutomationController.send(document.body.innerText);",
      &result));
  EXPECT_EQ("Blob content", result);
}

// Tests that a WebView cannot load a webview-inaccessible resource. See
// https://crbug.com/640072.
IN_PROC_BROWSER_TEST_F(WebViewTest, LoadWebviewInaccessibleResource) {
  TestHelper("testLoadWebviewInaccessibleResource",
             "web_view/load_webview_accessible_resource", NEEDS_TEST_SERVER);

  content::WebContents* embedder_contents = GetEmbedderWebContents();
  content::WebContents* web_view_contents =
      GetGuestViewManager()->GetLastGuestCreated();
  ASSERT_TRUE(embedder_contents);
  ASSERT_TRUE(web_view_contents);

  // Check that the webview stays at the first page that it loaded (foo.html),
  // and does not commit inaccessible.html.
  GURL embedder_url(embedder_contents->GetLastCommittedURL());
  GURL foo_url(embedder_url.GetOrigin().spec() + "assets/foo.html");

  EXPECT_EQ(foo_url, web_view_contents->GetLastCommittedURL());
}

// Ensure that only app resources accessible to the webview can be loaded in a
// webview even if the webview commits an app frame.
IN_PROC_BROWSER_TEST_F(WebViewTest,
                       LoadAccessibleSubresourceInAppWebviewFrame) {
  TestHelper("testLoadAccessibleSubresourceInAppWebviewFrame",
             "web_view/load_webview_accessible_resource", NEEDS_TEST_SERVER);
}
IN_PROC_BROWSER_TEST_F(WebViewTest,
                       InaccessibleResourceDoesNotLoadInAppWebviewFrame) {
  TestHelper("testInaccessibleResourceDoesNotLoadInAppWebviewFrame",
             "web_view/load_webview_accessible_resource", NEEDS_TEST_SERVER);
}

// Makes sure that a webview will display correctly after reloading it after a
// crash.
IN_PROC_BROWSER_TEST_F(WebViewTest, ReloadAfterCrash) {
  // Load guest and wait for it to appear.
  LoadAppWithGuest("web_view/simple");
  EXPECT_TRUE(GetGuestWebContents()->GetMainFrame()->GetView());
  content::RenderFrameSubmissionObserver frame_observer(GetGuestWebContents());
  frame_observer.WaitForMetadataChange();

  // Kill guest.
  auto* rph = GetGuestWebContents()->GetMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      rph, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(rph->Shutdown(content::RESULT_CODE_KILLED));
  crash_observer.Wait();
  EXPECT_FALSE(GetGuestWebContents()->GetMainFrame()->GetView());

  // Reload guest and make sure it appears.
  content::TestNavigationObserver load_observer(GetGuestWebContents());
  EXPECT_TRUE(ExecuteScript(GetEmbedderWebContents(),
                            "document.querySelector('webview').reload()"));
  load_observer.Wait();
  EXPECT_TRUE(GetGuestWebContents()->GetMainFrame()->GetView());
  // Ensure that the guest produces a new frame.
  frame_observer.WaitForAnyFrameSubmission();
}

// The presence of DomAutomationController interferes with these tests, so we
// disable it.
class WebViewTestNoDomAutomationController : public WebViewTest {
 public:
  ~WebViewTestNoDomAutomationController() override {}

  void SetUpInProcessBrowserTestFixture() override {
    WebViewTest::SetUpInProcessBrowserTestFixture();

    // DomAutomationController is added in BrowserTestBase::SetUp, so we need
    // to remove it here instead of in SetUpCommandLine.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    base::CommandLine new_command_line(command_line->GetProgram());
    base::CommandLine::SwitchMap switches = command_line->GetSwitches();
    switches.erase(switches::kDomAutomationController);
    for (const auto& s : switches)
      new_command_line.AppendSwitchNative(s.first, s.second);

    *command_line = new_command_line;
  }
};

// Tests that a webview inside an iframe can load and that it is destroyed when
// the iframe is detached.
// We need to disable DomAutomationController because it forces the creation of
// a script context. We want to test that we handle the case where there is no
// script context for the iframe. See crbug.com/788914
IN_PROC_BROWSER_TEST_F(WebViewTestNoDomAutomationController,
                       LoadWebviewInsideIframe) {
  TestHelper("testLoadWebviewInsideIframe",
             "web_view/load_webview_inside_iframe", NEEDS_TEST_SERVER);

  ASSERT_TRUE(GetGuestViewManager()->GetLastGuestCreated());

  content::WebContentsDestroyedWatcher watcher(
      GetGuestViewManager()->GetLastGuestCreated());

  // Remove the iframe.
  content::ExecuteScriptAsync(GetEmbedderWebContents(),
                              "document.querySelector('iframe').remove()");

  // Wait for guest to be destroyed.
  watcher.Wait();
}

IN_PROC_BROWSER_TEST_F(WebViewAccessibilityTest, LoadWebViewAccessibility) {
  LoadAppWithGuest("web_view/focus_accessibility");
  content::WebContents* web_contents = GetFirstAppWindowWebContents();
  content::EnableAccessibilityForWebContents(web_contents);
  content::WebContents* guest_web_contents = GetGuestWebContents();
  content::EnableAccessibilityForWebContents(guest_web_contents);
  content::WaitForAccessibilityTreeToContainNodeWithName(web_contents,
                                                         "Guest button");
}

IN_PROC_BROWSER_TEST_F(WebViewAccessibilityTest, FocusAccessibility) {
  LoadAppWithGuest("web_view/focus_accessibility");
  content::WebContents* web_contents = GetFirstAppWindowWebContents();
  content::EnableAccessibilityForWebContents(web_contents);
  content::WebContents* guest_web_contents = GetGuestWebContents();
  content::EnableAccessibilityForWebContents(guest_web_contents);

  // Wait for focus to land on the "root web area" role, representing
  // focus on the main document itself.
  while (content::GetFocusedAccessibilityNodeInfo(web_contents).role !=
         ax::mojom::Role::kRootWebArea) {
    content::WaitForAccessibilityFocusChange();
  }

  // Now keep pressing the Tab key until focus lands on a button.
  while (content::GetFocusedAccessibilityNodeInfo(web_contents).role !=
         ax::mojom::Role::kButton) {
    content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('\t'),
                              ui::DomCode::TAB, ui::VKEY_TAB, false, false,
                              false, false);
    content::WaitForAccessibilityFocusChange();
  }

  // Ensure that we hit the button inside the guest frame labeled
  // "Guest button".
  ui::AXNodeData node_data =
      content::GetFocusedAccessibilityNodeInfo(web_contents);
  EXPECT_EQ("Guest button",
            node_data.GetStringAttribute(ax::mojom::StringAttribute::kName));
}

class WebContentsAccessibilityEventWatcher
    : public content::WebContentsObserver {
 public:
  WebContentsAccessibilityEventWatcher(content::WebContents* web_contents,
                                       ax::mojom::Event event)
      : content::WebContentsObserver(web_contents), event_(event), count_(0) {}
  ~WebContentsAccessibilityEventWatcher() override {}

  void Wait() {
    if (count_ == 0) {
      loop_runner_ = new content::MessageLoopRunner();
      loop_runner_->Run();
    }
  }

  void AccessibilityEventReceived(
      const content::AXEventNotificationDetails& event_bundle) override {
    bool found = false;
    int event_node_id = 0;
    for (auto& event : event_bundle.events) {
      if (event.event_type == event_) {
        event_node_id = event.id;
        found = true;
        break;
      }
    }
    if (!found)
      return;

    for (auto& update : event_bundle.updates) {
      for (auto& node : update.nodes) {
        if (node.id == event_node_id) {
          count_++;
          node_data_ = node;
          loop_runner_->Quit();
          return;
        }
      }
    }
  }

  size_t count() const { return count_; }

  const ui::AXNodeData& node_data() const { return node_data_; }

 private:
  scoped_refptr<content::MessageLoopRunner> loop_runner_;
  ax::mojom::Event event_;
  ui::AXNodeData node_data_;
  size_t count_;
};

IN_PROC_BROWSER_TEST_F(WebViewAccessibilityTest, DISABLED_TouchAccessibility) {
  LoadAppWithGuest("web_view/touch_accessibility");
  content::WebContents* web_contents = GetFirstAppWindowWebContents();
  content::EnableAccessibilityForWebContents(web_contents);
  content::WebContents* guest_web_contents = GetGuestWebContents();
  content::EnableAccessibilityForWebContents(guest_web_contents);

  // Listen for accessibility events on both WebContents.
  WebContentsAccessibilityEventWatcher main_event_watcher(
      web_contents, ax::mojom::Event::kHover);
  WebContentsAccessibilityEventWatcher guest_event_watcher(
      guest_web_contents, ax::mojom::Event::kHover);

  // Send an accessibility touch event to the main WebContents, but
  // positioned on top of the button inside the inner WebView.
  blink::WebMouseEvent accessibility_touch_event(
      blink::WebInputEvent::Type::kMouseMove,
      blink::WebInputEvent::kIsTouchAccessibility,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  accessibility_touch_event.SetPositionInWidget(95, 55);
  web_contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      accessibility_touch_event);

  // Ensure that we got just a single hover event on the guest WebContents,
  // and that it was fired on a button.
  guest_event_watcher.Wait();
  ui::AXNodeData hit_node = guest_event_watcher.node_data();
  EXPECT_EQ(1U, guest_event_watcher.count());
  EXPECT_EQ(ax::mojom::Role::kButton, hit_node.role);
  EXPECT_EQ(0U, main_event_watcher.count());
}

class WebViewGuestScrollTest : public WebViewTest,
                               public testing::WithParamInterface<bool> {};

class WebViewGuestScrollTouchTest : public WebViewGuestScrollTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebViewGuestScrollTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kTouchEventFeatureDetection,
        switches::kTouchEventFeatureDetectionEnabled);
  }
};

// Tests that scrolls bubble from guest to embedder.
// Create two test instances, one where the guest body is scrollable and the
// other where the body is not scrollable: fast-path scrolling will generate
// different ack results in between these two cases.
INSTANTIATE_TEST_SUITE_P(WebViewScrollBubbling,
                         WebViewGuestScrollTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(WebViewGuestScrollTest, TestGuestWheelScrollsBubble) {
  LoadAppWithGuest("web_view/scrollable_embedder_and_guest");

  if (GetParam())
    SendMessageToGuestAndWait("set_overflow_hidden", "overflow_is_hidden");

  content::WebContents* embedder_contents = GetEmbedderWebContents();
  content::RenderFrameSubmissionObserver embedder_frame_observer(
      embedder_contents);

  std::vector<content::WebContents*> guest_web_contents_list;
  GetGuestViewManager()->WaitForNumGuestsCreated(1u);
  GetGuestViewManager()->GetGuestWebContentsList(&guest_web_contents_list);
  ASSERT_EQ(1u, guest_web_contents_list.size());

  content::WebContents* guest_contents = guest_web_contents_list[0];
  content::RenderFrameSubmissionObserver guest_frame_observer(guest_contents);

  gfx::Rect embedder_rect = embedder_contents->GetContainerBounds();
  gfx::Rect guest_rect = guest_contents->GetContainerBounds();

  guest_rect.set_x(guest_rect.x() - embedder_rect.x());
  guest_rect.set_y(guest_rect.y() - embedder_rect.y());
  embedder_rect.set_x(0);
  embedder_rect.set_y(0);

  gfx::Vector2dF default_offset;
  embedder_frame_observer.WaitForScrollOffset(default_offset);

  // Send scroll gesture to embedder & verify.
  // Make sure wheel events don't get filtered.
  float scroll_magnitude = 15.f;

  {
    // Scroll the embedder from a position in the embedder that is not over
    // the guest.
    gfx::Point embedder_scroll_location(
        embedder_rect.x() + embedder_rect.width() / 2,
        (embedder_rect.y() + guest_rect.y()) / 2);

    gfx::Vector2dF expected_offset(0.f, scroll_magnitude);

    content::SimulateMouseEvent(embedder_contents,
                                blink::WebInputEvent::Type::kMouseMove,
                                embedder_scroll_location);
    content::SimulateMouseWheelEvent(embedder_contents,
                                     embedder_scroll_location,
                                     gfx::Vector2d(0, -scroll_magnitude),
                                     blink::WebMouseWheelEvent::kPhaseBegan);

    embedder_frame_observer.WaitForScrollOffset(expected_offset);
  }

  guest_frame_observer.WaitForScrollOffset(default_offset);

  // Send scroll gesture to guest and verify embedder scrolls.
  // Perform a scroll gesture of the same magnitude, but in the opposite
  // direction and centered over the GuestView this time.
  guest_rect = guest_contents->GetContainerBounds();
  embedder_rect = embedder_contents->GetContainerBounds();
  guest_rect.set_x(guest_rect.x() - embedder_rect.x());
  guest_rect.set_y(guest_rect.y() - embedder_rect.y());
  {
    gfx::Point guest_scroll_location(guest_rect.x() + guest_rect.width() / 2,
                                     guest_rect.y());

    content::SimulateMouseEvent(embedder_contents,
                                blink::WebInputEvent::Type::kMouseMove,
                                guest_scroll_location);
    content::SimulateMouseWheelEvent(embedder_contents, guest_scroll_location,
                                     gfx::Vector2d(0, scroll_magnitude),
                                     blink::WebMouseWheelEvent::kPhaseChanged);
    embedder_frame_observer.WaitForScrollOffset(default_offset);
  }
}

// Test that when we bubble scroll from a guest, the guest does not also
// consume the scroll.
IN_PROC_BROWSER_TEST_P(WebViewGuestScrollTest,
                       ScrollLatchingPreservedInGuests) {
  LoadAppWithGuest("web_view/scrollable_embedder_and_guest");

  content::WebContents* embedder_contents = GetEmbedderWebContents();
  content::RenderFrameSubmissionObserver embedder_frame_observer(
      embedder_contents);

  std::vector<content::WebContents*> guest_web_contents_list;
  GetGuestViewManager()->WaitForNumGuestsCreated(1u);
  GetGuestViewManager()->GetGuestWebContentsList(&guest_web_contents_list);
  ASSERT_EQ(1u, guest_web_contents_list.size());

  content::WebContents* guest_contents = guest_web_contents_list[0];
  content::RenderFrameSubmissionObserver guest_frame_observer(guest_contents);
  content::RenderWidgetHostView* guest_host_view =
      guest_contents->GetRenderWidgetHostView();

  gfx::Vector2dF default_offset;
  guest_frame_observer.WaitForScrollOffset(default_offset);
  embedder_frame_observer.WaitForScrollOffset(default_offset);

  gfx::PointF guest_scroll_location(1, 1);
  gfx::PointF guest_scroll_location_in_root =
      guest_host_view->TransformPointToRootCoordSpaceF(guest_scroll_location);

  // When the guest is already scrolled to the top, scroll up so that we bubble
  // scroll.
  blink::WebGestureEvent scroll_begin(
      blink::WebGestureEvent::Type::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      blink::WebGestureDevice::kTouchpad);
  scroll_begin.SetPositionInWidget(guest_scroll_location);
  scroll_begin.SetPositionInScreen(guest_scroll_location_in_root);
  scroll_begin.data.scroll_begin.delta_x_hint = 0;
  scroll_begin.data.scroll_begin.delta_y_hint = 5;
  content::SimulateGestureEvent(guest_contents, scroll_begin,
                                ui::LatencyInfo(ui::SourceEventType::WHEEL));

  content::InputEventAckWaiter update_waiter(
      guest_contents->GetRenderViewHost()->GetWidget(),
      base::BindRepeating([](blink::mojom::InputEventResultSource,
                             blink::mojom::InputEventResultState state,
                             const blink::WebInputEvent& event) {
        return event.GetType() ==
                   blink::WebGestureEvent::Type::kGestureScrollUpdate &&
               state != blink::mojom::InputEventResultState::kConsumed;
      }));

  blink::WebGestureEvent scroll_update(
      blink::WebGestureEvent::Type::kGestureScrollUpdate,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests(),
      scroll_begin.SourceDevice());
  scroll_update.SetPositionInWidget(scroll_begin.PositionInWidget());
  scroll_update.SetPositionInScreen(scroll_begin.PositionInScreen());
  scroll_update.data.scroll_update.delta_x =
      scroll_begin.data.scroll_begin.delta_x_hint;
  scroll_update.data.scroll_update.delta_y =
      scroll_begin.data.scroll_begin.delta_y_hint;
  content::SimulateGestureEvent(guest_contents, scroll_update,
                                ui::LatencyInfo(ui::SourceEventType::WHEEL));
  update_waiter.Wait();
  update_waiter.Reset();

  // TODO(jonross): This test is only waiting on InputEventAckWaiter, but has an
  // implicit wait on frame submission. InputEventAckWaiter needs to be updated
  // to support VizDisplayCompositor, when it is, it should be tied to frame
  // tokens which will allow for synchronizing with frame submission for further
  // verifying metadata (crbug.com/812012)
  guest_frame_observer.WaitForScrollOffset(default_offset);

  // Now we switch directions and scroll down. The guest can scroll in this
  // direction, but since we're bubbling, the guest should not consume this.
  scroll_update.data.scroll_update.delta_y = -5;
  content::SimulateGestureEvent(guest_contents, scroll_update,
                                ui::LatencyInfo(ui::SourceEventType::WHEEL));
  update_waiter.Wait();

  guest_frame_observer.WaitForScrollOffset(default_offset);
}

INSTANTIATE_TEST_SUITE_P(WebViewScrollBubbling,
                         WebViewGuestScrollTouchTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(WebViewGuestScrollTouchTest,
                       TestGuestGestureScrollsBubble) {
  // Just in case we're running ChromeOS tests, we need to make sure the
  // debounce interval is set to zero so our back-to-back gesture-scrolls don't
  // get munged together. Since the first scroll will be put on the fast
  // (compositor) path, while the second one should always be slow-path so it
  // gets to BrowserPlugin, having them merged is definitely an error.
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  gesture_config->set_scroll_debounce_interval_in_ms(0);

  LoadAppWithGuest("web_view/scrollable_embedder_and_guest");

  if (GetParam())
    SendMessageToGuestAndWait("set_overflow_hidden", "overflow_is_hidden");

  content::WebContents* embedder_contents = GetEmbedderWebContents();
  content::RenderFrameSubmissionObserver embedder_frame_observer(
      embedder_contents);

  std::vector<content::WebContents*> guest_web_contents_list;
  GetGuestViewManager()->WaitForNumGuestsCreated(1u);
  GetGuestViewManager()->GetGuestWebContentsList(&guest_web_contents_list);
  ASSERT_EQ(1u, guest_web_contents_list.size());

  content::WebContents* guest_contents = guest_web_contents_list[0];
  content::RenderFrameSubmissionObserver guest_frame_observer(guest_contents);

  gfx::Rect embedder_rect = embedder_contents->GetContainerBounds();
  gfx::Rect guest_rect = guest_contents->GetContainerBounds();

  guest_rect.set_x(guest_rect.x() - embedder_rect.x());
  guest_rect.set_y(guest_rect.y() - embedder_rect.y());
  embedder_rect.set_x(0);
  embedder_rect.set_y(0);

  gfx::Vector2dF default_offset;
  embedder_frame_observer.WaitForScrollOffset(default_offset);

  // Send scroll gesture to embedder & verify.
  float gesture_distance = 15.f;
  {
    // Scroll the embedder from a position in the embedder that is not over
    // the guest.
    gfx::Point embedder_scroll_location(
        embedder_rect.x() + embedder_rect.width() / 2,
        (embedder_rect.y() + guest_rect.y()) / 2);

    gfx::Vector2dF expected_offset(0.f, gesture_distance);

    content::SimulateGestureScrollSequence(
        embedder_contents, embedder_scroll_location,
        gfx::Vector2dF(0, -gesture_distance));

    embedder_frame_observer.WaitForScrollOffset(expected_offset);
  }

  // Check that the guest has not scrolled.
  guest_frame_observer.WaitForScrollOffset(default_offset);

  // Send scroll gesture to guest and verify embedder scrolls.
  // Perform a scroll gesture of the same magnitude, but in the opposite
  // direction and centered over the GuestView this time.
  guest_rect = guest_contents->GetContainerBounds();
  {
    gfx::Point guest_scroll_location(guest_rect.width() / 2,
                                     guest_rect.height() / 2);

    content::SimulateGestureScrollSequence(guest_contents,
                                           guest_scroll_location,
                                           gfx::Vector2dF(0, gesture_distance));

    embedder_frame_observer.WaitForScrollOffset(default_offset);
  }
}

// This runs the chrome://chrome-signin page which includes an OOPIF-<webview>
// of accounts.google.com.
class ChromeSignInWebViewTest : public WebViewTest {
 public:
  ChromeSignInWebViewTest() {}
  ~ChromeSignInWebViewTest() override {}

 protected:
  void WaitForWebViewInDom() {
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    auto* script =
        "var count = 10;"
        "var interval;"
        "interval = setInterval(function(){"
        "  if (document.querySelector('inline-login-app').shadowRoot"
        "       .querySelector('webview')) {"
        "    document.title = 'success';"
        "    console.log('FOUND webview');"
        "    clearInterval(interval);"
        "  } else if (count == 0) {"
        "    document.title = 'error';"
        "    clearInterval(interval);"
        "  } else {"
        "    count -= 1;"
        "  }"
        "}, 1000);";
    ExecuteScriptWaitForTitle(web_contents, script, "success");
  }
};

#if (defined(OS_LINUX) && !defined(OS_CHROMEOS)) || defined(OS_MAC) || \
    defined(OS_WIN)
// This verifies the fix for http://crbug.com/667708.
IN_PROC_BROWSER_TEST_F(ChromeSignInWebViewTest,
                       ClosingChromeSignInShouldNotCrash) {
  GURL signin_url{"chrome://chrome-signin/?reason=5"};

  AddTabAtIndex(0, signin_url, ui::PAGE_TRANSITION_TYPED);
  AddTabAtIndex(1, signin_url, ui::PAGE_TRANSITION_TYPED);
  WaitForWebViewInDom();

  chrome::CloseTab(browser());
}
#endif

// This test verifies that unattached guests are not included as the inner
// WebContents. The test verifies this by triggering a find-in-page request on a
// page with both an attached and an unattached <webview> and verifies that,
// unlike the attached guest, no find requests are sent for the unattached
// guest. For more context see https://crbug.com/897465.
// TODO(crbug.com/914098): Address flakiness and reenable.
IN_PROC_BROWSER_TEST_F(ChromeSignInWebViewTest,
                       DISABLED_NoFindInPageForUnattachedGuest) {
  GURL signin_url{"chrome://chrome-signin"};
  ui_test_utils::NavigateToURL(browser(), signin_url);
  auto* embedder_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* attached_guest = GetGuestViewManager()->WaitForNextGuestCreated();
  GetGuestViewManager()->WaitUntilAttached(attached_guest);
  // Now add a new <webview> and wait until its guest WebContents is created.
  ASSERT_TRUE(ExecuteScript(embedder_web_contents,
                            "var webview = document.createElement('webview');"
                            "webview.src = 'data:text/html,foo';"
                            "document.body.appendChild(webview);"));
  // Right after this line, the guest is created but *not* attached (the
  // callback for 'GuestViewInternal.createGuest' is invoked after this line;
  // which is before attaching begins).
  auto* unattached_guest = GetGuestViewManager()->GetLastGuestCreated();
  EXPECT_NE(unattached_guest, attached_guest);
  auto* find_helper =
      find_in_page::FindTabHelper::FromWebContents(embedder_web_contents);
  find_helper->StartFinding(base::ASCIIToUTF16("doesn't matter"), true, true,
                            false);
  auto pending =
      content::GetRenderFrameHostsWithPendingFindResults(embedder_web_contents);
  // Request for main frame of the tab.
  EXPECT_EQ(1U, pending.count(embedder_web_contents->GetMainFrame()));
  // Request for main frame of the attached guest.
  EXPECT_EQ(1U, pending.count(attached_guest->GetMainFrame()));
  // No request for the unattached guest.
  EXPECT_EQ(0U, pending.count(unattached_guest->GetMainFrame()));
  // Sanity-check: try the set returned for guest.
  pending =
      content::GetRenderFrameHostsWithPendingFindResults(unattached_guest);
  EXPECT_TRUE(pending.empty());
}

// This test class makes "isolated.com" an isolated origin, to be used in
// testing isolated origins inside of a WebView.
class IsolatedOriginWebViewTest : public WebViewTest {
 public:
  IsolatedOriginWebViewTest() {}
  ~IsolatedOriginWebViewTest() override {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    std::string origin =
        embedded_test_server()->GetURL("isolated.com", "/").spec();
    command_line->AppendSwitchASCII(switches::kIsolateOrigins, origin);
    WebViewTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
    WebViewTest::SetUpOnMainThread();
  }
};

// Test isolated origins inside a WebView, and make sure that loading an
// isolated origin in a regular tab's subframe doesn't reuse a WebView process
// that had loaded it previously, which would result in renderer kills. See
// https://crbug.com/751916 and https://crbug.com/751920.
IN_PROC_BROWSER_TEST_F(IsolatedOriginWebViewTest, IsolatedOriginInWebview) {
  LoadAppWithGuest("web_view/simple");
  content::WebContents* guest = GetGuestWebContents();

  // Navigate <webview> to an isolated origin.
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.com", "/title1.html"));
  {
    content::TestNavigationObserver load_observer(guest);
    EXPECT_TRUE(
        ExecuteScript(guest, "location.href = '" + isolated_url.spec() + "';"));
    load_observer.Wait();
  }

  // TODO(alexmos, creis): The isolated origin currently has to use a
  // guest SiteInstance, rather than a SiteInstance with its own
  // meaningful site URL.  This should be fixed as part of
  // https://crbug.com/734722.
  EXPECT_TRUE(guest->GetMainFrame()->GetSiteInstance()->IsGuest());

  // Now, navigate <webview> to a regular page with a subframe.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/iframe.html"));
  {
    content::TestNavigationObserver load_observer(guest);
    EXPECT_TRUE(
        ExecuteScript(guest, "location.href = '" + foo_url.spec() + "';"));
    load_observer.Wait();
  }

  // Navigate subframe in <webview> to an isolated origin.
  EXPECT_TRUE(NavigateIframeToURL(guest, "test", isolated_url));

  // TODO(alexmos, creis): Unfortunately, the subframe currently has to stay in
  // the guest process.  The expectations here should change once WebViews
  // can support OOPIFs.  See https://crbug.com/614463.
  content::RenderFrameHost* webview_subframe =
      ChildFrameAt(guest->GetMainFrame(), 0);
  EXPECT_EQ(webview_subframe->GetProcess(),
            guest->GetMainFrame()->GetProcess());
  EXPECT_EQ(webview_subframe->GetSiteInstance(),
            guest->GetMainFrame()->GetSiteInstance());

  // Load a page with subframe in a regular tab.
  AddTabAtIndex(0, foo_url, ui::PAGE_TRANSITION_TYPED);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate that subframe to an isolated origin.  This should not join the
  // WebView process, which has isolated.foo.com committed in a different
  // storage partition.
  EXPECT_TRUE(NavigateIframeToURL(tab, "test", isolated_url));
  content::RenderFrameHost* subframe = ChildFrameAt(tab->GetMainFrame(), 0);
  EXPECT_NE(guest->GetMainFrame()->GetProcess(), subframe->GetProcess());

  // Check that the guest process hasn't crashed.
  EXPECT_TRUE(guest->GetMainFrame()->IsRenderFrameLive());

  // Check that accessing a foo.com cookie from the WebView doesn't result in a
  // renderer kill. This might happen if we erroneously applied an isolated.com
  // origin lock to the WebView process when committing isolated.com.
  bool cookie_is_correct = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      guest,
      "document.cookie = 'foo=bar';\n"
      "window.domAutomationController.send(document.cookie == 'foo=bar');\n",
      &cookie_is_correct));
  EXPECT_TRUE(cookie_is_correct);
}

// This test is similar to IsolatedOriginInWebview above, but loads an isolated
// origin in a <webview> subframe *after* loading the same isolated origin in a
// regular tab's subframe.  The isolated origin's subframe in the <webview>
// subframe should not reuse the regular tab's subframe process.  See
// https://crbug.com/751916 and https://crbug.com/751920.
IN_PROC_BROWSER_TEST_F(IsolatedOriginWebViewTest,
                       LoadIsolatedOriginInWebviewAfterLoadingInRegularTab) {
  LoadAppWithGuest("web_view/simple");
  content::WebContents* guest = GetGuestWebContents();

  // Load a page with subframe in a regular tab.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/iframe.html"));
  AddTabAtIndex(0, foo_url, ui::PAGE_TRANSITION_TYPED);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate that subframe to an isolated origin.
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(tab, "test", isolated_url));
  content::RenderFrameHost* subframe = ChildFrameAt(tab->GetMainFrame(), 0);
  EXPECT_NE(tab->GetMainFrame()->GetProcess(), subframe->GetProcess());

  // Navigate <webview> to a regular page with an isolated origin subframe.
  {
    content::TestNavigationObserver load_observer(guest);
    EXPECT_TRUE(
        ExecuteScript(guest, "location.href = '" + foo_url.spec() + "';"));
    load_observer.Wait();
  }
  EXPECT_TRUE(NavigateIframeToURL(guest, "test", isolated_url));

  // TODO(alexmos, creis): The subframe currently has to stay in the guest
  // process.  The expectations here should change once WebViews can support
  // OOPIFs.  See https://crbug.com/614463.
  content::RenderFrameHost* webview_subframe =
      ChildFrameAt(guest->GetMainFrame(), 0);
  EXPECT_EQ(webview_subframe->GetProcess(),
            guest->GetMainFrame()->GetProcess());
  EXPECT_EQ(webview_subframe->GetSiteInstance(),
            guest->GetMainFrame()->GetSiteInstance());
  EXPECT_NE(webview_subframe->GetProcess(), subframe->GetProcess());

  // Check that the guest and regular tab processes haven't crashed.
  EXPECT_TRUE(guest->GetMainFrame()->IsRenderFrameLive());
  EXPECT_TRUE(tab->GetMainFrame()->IsRenderFrameLive());
  EXPECT_TRUE(subframe->IsRenderFrameLive());

  // Check that accessing a foo.com cookie from the WebView doesn't result in a
  // renderer kill. This might happen if we erroneously applied an isolated.com
  // origin lock to the WebView process when committing isolated.com.
  bool cookie_is_correct = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      guest,
      "document.cookie = 'foo=bar';\n"
      "window.domAutomationController.send(document.cookie == 'foo=bar');\n",
      &cookie_is_correct));
  EXPECT_TRUE(cookie_is_correct);
}

// Sends an auto-resize message to the RenderWidgetHost and ensures that the
// auto-resize transaction is handled and produces a single response message
// from guest to embedder.
IN_PROC_BROWSER_TEST_F(WebViewTest, AutoResizeMessages) {
  LoadAppWithGuest("web_view/simple");
  content::WebContents* embedder = GetEmbedderWebContents();
  content::WebContents* guest = GetGuestWebContents();

  // Helper function as this test requires inspecting a number of content::
  // internal objects.
  EXPECT_TRUE(content::TestGuestAutoresize(
      embedder->GetRenderWidgetHostView()->GetRenderWidgetHost()->GetProcess(),
      guest->GetRenderWidgetHostView()->GetRenderWidgetHost()));
}

// Test that a guest sees the synthetic wheel events of a touchpad pinch.
IN_PROC_BROWSER_TEST_F(WebViewTest, TouchpadPinchSyntheticWheelEvents) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  LoadAppWithGuest("web_view/touchpad_pinch");
  content::WebContents* guest_contents = GetGuestWebContents();

  ExtensionTestMessageListener synthetic_wheel_listener("Seen wheel event",
                                                        false);

  const gfx::Rect contents_rect = guest_contents->GetContainerBounds();
  const gfx::Point pinch_position(contents_rect.width() / 2,
                                  contents_rect.height() / 2);
  content::SimulateGesturePinchSequence(guest_contents, pinch_position, 1.23,
                                        blink::WebGestureDevice::kTouchpad);

  ASSERT_TRUE(synthetic_wheel_listener.WaitUntilSatisfied());
}

// Tests that we can open and close a devtools window that inspects a contents
// containing a guest view without crashing.
IN_PROC_BROWSER_TEST_F(WebViewTest, OpenAndCloseDevTools) {
  LoadAppWithGuest("web_view/simple");
  content::WebContents* embedder = GetEmbedderWebContents();
  DevToolsWindow* devtools = DevToolsWindowTesting::OpenDevToolsWindowSync(
      embedder, false /* is_docked */);
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

// Regression test for https://crbug.com/1014385
// We load an extension whose background page attempts to declare variables with
// names that are the same as guest view types. The declarations should not be
// syntax errors.
using GuestViewExtensionNameCollisionTest = extensions::ExtensionBrowserTest;
IN_PROC_BROWSER_TEST_F(GuestViewExtensionNameCollisionTest,
                       GuestViewNamesDoNotCollideWithExtensions) {
  ExtensionTestMessageListener loaded_listener("LOADED", false);
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "platform_apps/web_view/no_extension_name_collision"));
  ASSERT_TRUE(loaded_listener.WaitUntilSatisfied());

  const std::string script =
      "window.domAutomationController.send("
      "    window.testPassed ? 'PASSED' : 'FAILED');";
  const std::string test_passed =
      ExecuteScriptInBackgroundPage(extension->id(), script);
  EXPECT_EQ("PASSED", test_passed);
}

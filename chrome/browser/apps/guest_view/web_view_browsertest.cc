// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/bluetooth/web_bluetooth_test_utils.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/hid/chrome_hid_delegate.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_link_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/hid/hid_chooser_controller.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/usb/usb_browser_test_utils.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/tracing.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/download/public/common/download_task_runner.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/guest_view_manager_factory.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_link_manager.h"
#include "components/permissions/mock_chooser_controller_view.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/db/fake_database_manager.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/hid_chooser.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/bluetooth_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/find_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "content/public/test/test_file_error_injector.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/api/declarative/rules_cache_delegate.h"
#include "extensions/browser/api/declarative/rules_registry.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/declarative/test_rules_registry.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/test/extension_test_message_listener.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_data_directory.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/cpp/test/fake_hid_manager.h"
#include "services/device/public/cpp/test/fake_serial_port_manager.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "services/network/public/cpp/network_switches.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_updates_and_events.h"
#include "ui/display/display_switches.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/gfx/geometry/point.h"
#include "ui/latency/latency_info.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"  // nogncheck
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if defined(USE_AURA)
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(ENABLE_PPAPI)
#include "content/public/test/ppapi_test_utils.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "base/test/with_feature_override.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/pdf/test_pdf_viewer_stream_manager.h"
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

using extensions::ContextMenuMatcher;
using extensions::ExtensionsAPIClient;
using extensions::MenuItem;
using guest_view::GuestViewManager;
using guest_view::TestGuestViewManager;
using guest_view::TestGuestViewManagerFactory;
using prerender::NoStatePrefetchLinkManager;
using prerender::NoStatePrefetchLinkManagerFactory;
using task_manager::browsertest_util::MatchAboutBlankTab;
using task_manager::browsertest_util::MatchAnyApp;
using task_manager::browsertest_util::MatchAnyBackground;
using task_manager::browsertest_util::MatchAnyTab;
using task_manager::browsertest_util::MatchAnyWebView;
using task_manager::browsertest_util::MatchApp;
using task_manager::browsertest_util::MatchBackground;
using task_manager::browsertest_util::MatchWebView;
using task_manager::browsertest_util::WaitForTaskManagerRows;
using testing::Return;
using ui::MenuModel;

namespace {
const char kEmptyResponsePath[] = "/close-socket";
const char kRedirectResponsePath[] = "/server-redirect";
const char kUserAgentRedirectResponsePath[] = "/detect-user-agent";
const char kCacheResponsePath[] = "/cache-control-response";
const char kRedirectResponseFullPath[] =
    "/extensions/platform_apps/web_view/shim/guest_redirect.html";

// Web Bluetooth
constexpr char kFakeBluetoothDeviceName[] = "Test Device";
constexpr char kDeviceAddress[] = "00:00:00:00:00:00";
constexpr char kHeartRateUUIDString[] = "0000180d-0000-1000-8000-00805f9b34fb";

class RenderWidgetHostVisibilityObserver
    : public content::RenderWidgetHostObserver {
 public:
  RenderWidgetHostVisibilityObserver(content::RenderWidgetHost* host,
                                     base::OnceClosure hidden_callback)
      : hidden_callback_(std::move(hidden_callback)) {
    observation_.Observe(host);
  }
  ~RenderWidgetHostVisibilityObserver() override = default;
  RenderWidgetHostVisibilityObserver(
      const RenderWidgetHostVisibilityObserver&) = delete;
  RenderWidgetHostVisibilityObserver& operator=(
      const RenderWidgetHostVisibilityObserver&) = delete;

  bool hidden_observed() const { return hidden_observed_; }

 private:
  // content::RenderWidgetHostObserver:
  void RenderWidgetHostVisibilityChanged(content::RenderWidgetHost* host,
                                         bool became_visible) override {
    if (!became_visible) {
      hidden_observed_ = true;
      std::move(hidden_callback_).Run();
    }
  }

  void RenderWidgetHostDestroyed(content::RenderWidgetHost* host) override {
    EXPECT_TRUE(observation_.IsObservingSource(host));
    observation_.Reset();
  }

  base::OnceClosure hidden_callback_;
  base::ScopedObservation<content::RenderWidgetHost,
                          content::RenderWidgetHostObserver>
      observation_{this};
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&RenderViewContextMenuBase::Cancel,
                                  base::Unretained(context_menu)));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop_.QuitClosure());
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
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    terminated_ = true;
    run_loop_.Quit();
  }

  void WaitForEmbedderRenderProcessTerminate() {
    if (terminated_)
      return;
    run_loop_.Run();
  }

 private:
  bool terminated_ = false;
  base::RunLoop run_loop_;
};

void ExecuteScriptWaitForTitle(content::WebContents* web_contents,
                               const char* script,
                               const char* title) {
  std::u16string expected_title(base::ASCIIToUTF16(title));
  std::u16string error_title(u"error");

  content::TitleWatcher title_watcher(web_contents, expected_title);
  title_watcher.AlsoWaitForTitle(error_title);
  EXPECT_TRUE(content::ExecJs(web_contents, script));
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
    for (aura::Window* window : observed_windows_) {
      window->RemoveObserver(this);
    }
  }

  void Wait(bool wait_for_widget_shown) {
    wait_for_widget_shown_ = wait_for_widget_shown;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    base::RunLoop().RunUntilIdle();
  }

  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (wait_for_widget_shown_ && visible)
      run_loop_->Quit();
  }

  void OnWindowInitialized(aura::Window* window) override {
    if (window->GetType() != aura::client::WINDOW_TYPE_MENU)
      return;
    window->AddObserver(this);
    observed_windows_.insert(window);
  }

  void OnWindowDestroyed(aura::Window* window) override {
    observed_windows_.erase(window);
    if (!wait_for_widget_shown_ && observed_windows_.empty())
      run_loop_->Quit();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  std::set<raw_ptr<aura::Window, SetExperimental>> observed_windows_;
  bool wait_for_widget_shown_ = false;
};

// Simulate real click with delay between mouse down and up.
class LeftMouseClick {
 public:
  explicit LeftMouseClick(content::RenderFrameHost* render_frame_host)
      : render_frame_host_(render_frame_host),
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
    const gfx::Rect offset =
        render_frame_host_->GetRenderWidgetHost()->GetView()->GetViewBounds();
    mouse_event_.SetPositionInScreen(point.x() + offset.x(),
                                     point.y() + offset.y());
    mouse_event_.click_count = 1;
    render_frame_host_->GetRenderWidgetHost()->ForwardMouseEvent(mouse_event_);

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&LeftMouseClick::SendMouseUp, base::Unretained(this)),
        base::Milliseconds(duration_ms));
  }

  // Wait for click completed.
  void Wait() {
    if (click_completed_)
      return;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  void SendMouseUp() {
    mouse_event_.SetType(blink::WebInputEvent::Type::kMouseUp);
    render_frame_host_->GetRenderWidgetHost()->ForwardMouseEvent(mouse_event_);
    click_completed_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }

  // Unowned pointer.
  raw_ptr<content::RenderFrameHost> render_frame_host_;

  std::unique_ptr<base::RunLoop> run_loop_;

  blink::WebMouseEvent mouse_event_;

  bool click_completed_ = true;
};

#endif

// Wraps around the browser-initiated |NavigateToURL| to hide direct guest
// WebContents access. For MPArch GuestView migration pre-work, we do not have
// such a mechanism to trigger a browser-initiated navigation on GuestView or
// guest RenderFrameHost.
[[nodiscard]] bool BrowserInitNavigationToUrl(guest_view::GuestViewBase* guest,
                                              const GURL& url) {
  return NavigateToURL(guest->web_contents(), url);
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
    if (request_run_loop_)
      request_run_loop_->Quit();
  }

  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override {
    checked_ = true;
    if (check_run_loop_)
      check_run_loop_->Quit();
    return true;
  }

  void WaitForRequestMediaPermission() {
    if (requested_)
      return;
    request_run_loop_ = std::make_unique<base::RunLoop>();
    request_run_loop_->Run();
  }

  void WaitForCheckMediaPermission() {
    if (checked_)
      return;
    check_run_loop_ = std::make_unique<base::RunLoop>();
    check_run_loop_->Run();
  }

 private:
  bool requested_ = false;
  bool checked_ = false;
  std::unique_ptr<base::RunLoop> request_run_loop_;
  std::unique_ptr<base::RunLoop> check_run_loop_;
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
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void DownloadDecided(base::OnceCallback<void(bool)> callback, bool allow) {
    EXPECT_FALSE(decision_made_);
    decision_made_ = true;

    if (waiting_for_decision_) {
      EXPECT_EQ(expect_allow_, allow);
      if (run_loop_)
        run_loop_->Quit();
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
  raw_ptr<content::WebContentsDelegate> orig_delegate_;
  bool waiting_for_decision_ = false;
  bool expect_allow_ = false;
  bool decision_made_ = false;
  bool last_download_allowed_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class WebViewTest : public extensions::PlatformAppBrowserTest {
 protected:
  void SetUp() override {
    if (UsesFakeSpeech()) {
      // SpeechRecognition test specific SetUp.
      fake_speech_recognition_manager_ =
          std::make_unique<content::FakeSpeechRecognitionManager>();
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

    geolocation_overrider_ =
        std::make_unique<device::ScopedGeolocationOverrider>(10, 20);

    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(blink::switches::kJavaScriptFlags,
                                    "--expose-gc");

    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Since LacrosAppsPublisherTest run without Ash, Lacros won't get
    // the Ash extension keeplist data from Ash (passed via crosapi). Therefore,
    // set empty ash keeplist for test.
    extensions::SetEmptyAshKeeplistForTest();
#endif
  }

  // Handles |request| by serving a redirect response if the |User-Agent| is
  // foobar.
  static std::unique_ptr<net::test_server::HttpResponse>
  UserAgentResponseHandler(const std::string& path,
                           const GURL& redirect_target,
                           const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(path, request.relative_url,
                          base::CompareCase::SENSITIVE)) {
      return nullptr;
    }

    auto it = request.headers.find("User-Agent");
    EXPECT_TRUE(it != request.headers.end());
    if (!base::StartsWith("foobar", it->second, base::CompareCase::SENSITIVE))
      return nullptr;

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
                          base::CompareCase::SENSITIVE)) {
      return nullptr;
    }

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

    return nullptr;
  }

  // Handles |request| by serving cache-able response.
  static std::unique_ptr<net::test_server::HttpResponse>
  CacheControlResponseHandler(const std::string& path,
                              const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(path, request.relative_url,
                          base::CompareCase::SENSITIVE)) {
      return nullptr;
    }

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

    ExtensionTestMessageListener done_listener("TEST_PASSED");
    done_listener.set_failure_message("TEST_FAILED");
    // Note that domAutomationController may not exist for some tests so we
    // must use the ExecuteScriptAsync.
    content::ExecuteScriptAsync(
        embedder_web_contents,
        base::StrCat({"try { runTest('", test_name,
                      "'); } catch (e) { "
                      "  console.log('UNABLE TO START TEST.'); "
                      "  console.log(e); "
                      "  chrome.test.sendMessage('TEST_FAILED'); "
                      "}"}));
    ASSERT_TRUE(done_listener.WaitUntilSatisfied());
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

    ExtensionTestMessageListener test_run_listener("PASSED");
    test_run_listener.set_failure_message("FAILED");
    EXPECT_TRUE(
        content::ExecJs(embedder_web_contents,
                        base::StrCat({"startDenyTest('", test_name, "')"})));
    ASSERT_TRUE(test_run_listener.WaitUntilSatisfied());
  }

  // Loads an app with a <webview> in it, returns once a guest is created.
  void LoadAppWithGuest(const std::string& app_path) {
    ExtensionTestMessageListener launched_listener("WebViewTest.LAUNCHED");
    launched_listener.set_failure_message("WebViewTest.FAILURE");
    LoadAndLaunchPlatformApp(app_path.c_str(), &launched_listener);

    guest_view_ = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  }

  void SendMessageToEmbedder(const std::string& message) {
    EXPECT_TRUE(
        content::ExecJs(GetEmbedderWebContents(),
                        base::StrCat({"onAppCommand('", message, "');"})));
  }

  void SendMessageToGuestAndWait(const std::string& message,
                                 const std::string& wait_message) {
    std::unique_ptr<ExtensionTestMessageListener> listener;
    if (!wait_message.empty()) {
      listener = std::make_unique<ExtensionTestMessageListener>(wait_message);
    }

    EXPECT_TRUE(content::ExecJs(
        GetGuestViewManager()->WaitForSingleGuestRenderFrameHostCreated(),
        base::StrCat({"onAppCommand('", message, "');"})));

    if (listener) {
      ASSERT_TRUE(listener->WaitUntilSatisfied());
    }
  }

  // Opens the context menu by simulating a mouse right-click at (1,1) relative
  // to the guest's |RenderWidgethostView|. The mouse event is forwarded
  // directly to the guest RWHV.
  void OpenContextMenu(content::RenderFrameHost* guest_main_frame) {
    ASSERT_TRUE(guest_main_frame);

    blink::WebMouseEvent mouse_event(
        blink::WebInputEvent::Type::kMouseDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());

    mouse_event.button = blink::WebMouseEvent::Button::kRight;
    // (1, 1) is chosen to make sure we click inside the guest.
    mouse_event.SetPositionInWidget(1, 1);

    auto* guest_rwh = guest_main_frame->GetRenderWidgetHost();
    guest_rwh->ForwardMouseEvent(mouse_event);
    mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
    guest_rwh->ForwardMouseEvent(mouse_event);
  }

  guest_view::GuestViewBase* GetGuestView() { return guest_view_; }
  content::WebContents* GetGuestWebContents() {
    return guest_view_->web_contents();
  }
  content::RenderFrameHost* GetGuestRenderFrameHost() {
    return guest_view_->GetGuestMainFrame();
  }

  content::WebContents* GetEmbedderWebContents() {
    if (!embedder_web_contents_) {
      embedder_web_contents_ = GetFirstAppWindowWebContents();
    }
    return embedder_web_contents_;
  }

  TestGuestViewManager* GetGuestViewManager() {
    return factory_.GetOrCreateTestGuestViewManager(
        browser()->profile(),
        ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate());
  }

  WebViewTest() = default;

  ~WebViewTest() override = default;

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
  raw_ptr<guest_view::GuestViewBase, AcrossTasksDanglingUntriaged> guest_view_ =
      nullptr;
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged>
      embedder_web_contents_ = nullptr;
};

// The following test suites are created to group tests based on specific
// features of <webview>.
using WebViewSizeTest = WebViewTest;
using WebViewVisibilityTest = WebViewTest;
using WebViewSpeechAPITest = WebViewTest;
using WebViewAccessibilityTest = WebViewTest;
using WebViewNewWindowTest = WebViewTest;

class WebViewDPITest : public WebViewTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebViewTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                                    base::NumberToString(scale()));
  }

  static float scale() { return 2.0f; }
};

class WebContentsAudioMutedObserver : public content::WebContentsObserver {
 public:
  explicit WebContentsAudioMutedObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  WebContentsAudioMutedObserver(const WebContentsAudioMutedObserver&) = delete;
  WebContentsAudioMutedObserver& operator=(
      const WebContentsAudioMutedObserver&) = delete;

  // WebContentsObserver.
  void DidUpdateAudioMutingState(bool muted) override {
    muting_update_observed_ = true;
    run_loop_.Quit();
  }

  void WaitForUpdate() { run_loop_.Run(); }

  bool muting_update_observed() { return muting_update_observed_; }

 private:
  base::RunLoop run_loop_;
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

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();

    EXPECT_EQ(audible, web_contents()->IsCurrentlyAudible());
    EXPECT_EQ(audible, audible_);
  }

 private:
  void OnAudioStateChanged(bool audible) override {
    audible_ = audible;
    run_loop_->Quit();
  }

  bool audible_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
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
  EXPECT_TRUE(
      content::ExecJs(embedder,
                      "wv = document.getElementsByTagName('webview')[0];"
                      "wv.addEventListener('consolemessage', function (e) {"
                      "  console.log('WebViewTest Guest: ' + e.message);"
                      "});"));

  // Inject JS to start audio.
  GURL audio_url = embedded_test_server()->GetURL(
      "/extensions/platform_apps/web_view/simple/ping.mp3");
  std::string setup_audio_script = base::StrCat(
      {"ae = document.createElement('audio'); ae.src='", audio_url.spec(),
       "'; document.body.appendChild(ae); ae.play();"});
  EXPECT_TRUE(content::ExecJs(guest, setup_audio_script,
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Wait for audio to start.
  embedder_obs.WaitForCurrentlyAudible(true);
  EXPECT_TRUE(embedder->IsCurrentlyAudible());
  EXPECT_TRUE(guest->IsCurrentlyAudible());

  // Wait for audio to stop.
  embedder_obs.WaitForCurrentlyAudible(false);
  EXPECT_FALSE(embedder->IsCurrentlyAudible());
  EXPECT_FALSE(guest->IsCurrentlyAudible());
}

IN_PROC_BROWSER_TEST_F(WebViewTest, SetAudioMuted) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  LoadAppWithGuest("web_view/simple");

  auto* guest = GetGuestViewManager()->GetLastGuestViewCreated();
  auto* web_view_guest = extensions::WebViewGuest::FromGuestViewBase(guest);
  content::WebContents* owner_web_contents = GetEmbedderWebContents();

  // The audio muted state for both WebContents and the WebViewGuest should be
  // false.
  EXPECT_FALSE(owner_web_contents->IsAudioMuted());
  EXPECT_FALSE(web_view_guest->IsAudioMuted());

  // Verify that the audio muted state can change for the webview when the owner
  // WebContents is unmuted.
  web_view_guest->SetAudioMuted(true);
  EXPECT_FALSE(owner_web_contents->IsAudioMuted());
  EXPECT_TRUE(web_view_guest->IsAudioMuted());

  web_view_guest->SetAudioMuted(false);
  EXPECT_FALSE(owner_web_contents->IsAudioMuted());
  EXPECT_FALSE(web_view_guest->IsAudioMuted());

  // Verify that the audio muted state changes to muted for the webview when the
  // owner WebContents is muted, and WebViewGuest remembers the muted setting of
  // the guest WebContents.
  owner_web_contents->SetAudioMuted(true);
  EXPECT_TRUE(owner_web_contents->IsAudioMuted());
  EXPECT_TRUE(web_view_guest->IsAudioMuted());

  web_view_guest->SetAudioMuted(true);
  EXPECT_TRUE(owner_web_contents->IsAudioMuted());
  EXPECT_TRUE(web_view_guest->IsAudioMuted());

  // Verify that the audio muted state cannot change from muted for the webview
  // when the owner WebContents is muted and the WebViewGuest remembers the set
  // audio muted state for the guest.
  web_view_guest->SetAudioMuted(false);
  EXPECT_TRUE(owner_web_contents->IsAudioMuted());
  EXPECT_TRUE(web_view_guest->IsAudioMuted());

  // Verify that the audio muted state changes to the last set audio state for
  // the webview when the owner WebContents changes to unmuted.
  owner_web_contents->SetAudioMuted(false);
  EXPECT_FALSE(owner_web_contents->IsAudioMuted());
  EXPECT_FALSE(web_view_guest->IsAudioMuted());

  owner_web_contents->SetAudioMuted(true);
  web_view_guest->SetAudioMuted(true);
  owner_web_contents->SetAudioMuted(false);
  EXPECT_FALSE(owner_web_contents->IsAudioMuted());
  EXPECT_TRUE(web_view_guest->IsAudioMuted());
}

IN_PROC_BROWSER_TEST_F(WebViewTest, WebViewRespectsInsets) {
  LoadAppWithGuest("web_view/simple");

  content::RenderWidgetHostView* guest_host_view =
      GetGuestView()->GetGuestMainFrame()->GetView();

  auto insets = gfx::Insets::TLBR(0, 0, 100, 0);
  gfx::Rect expected(guest_host_view->GetVisibleViewportSize());
  expected.Inset(insets);

  guest_host_view->SetInsets(gfx::Insets::TLBR(0, 0, 100, 0));

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
  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  content::WebContents* guest_contents = guest_view->web_contents();

  EXPECT_TRUE(embedder->IsAudioMuted());
  WebContentsAudioMutedObserver observer(guest_contents);
  // If the guest hasn't attached yet, it may not have received the muting
  // update, in which case we should wait until it does.
  if (!guest_contents->IsAudioMuted()) {
    observer.WaitForUpdate();
  }
  EXPECT_TRUE(guest_contents->IsAudioMuted());
}

IN_PROC_BROWSER_TEST_F(WebViewTest, AudioStateJavascriptAPI) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutoplayPolicy,
      switches::autoplay::kNoUserGestureRequiredPolicy);

  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunExtensionTest("platform_apps/web_view/audio_state_api",
                               {.launch_as_platform_app = true}))
      << message_;
}

// Test that WebView does not override autoplay policy.
IN_PROC_BROWSER_TEST_F(WebViewTest, AutoplayPolicy) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAutoplayPolicy,
      switches::autoplay::kDocumentUserActivationRequiredPolicy);

  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("platform_apps/web_view/autoplay",
                               {.launch_as_platform_app = true}))
      << message_;
}

// This test exercises the webview spatial navigation API
// TODO(crbug.com/41493388): Flaky timeouts on Mac and Cros.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_SpatialNavigationJavascriptAPI \
  DISABLED_SpatialNavigationJavascriptAPI
#else
#define MAYBE_SpatialNavigationJavascriptAPI SpatialNavigationJavascriptAPI
#endif
IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_SpatialNavigationJavascriptAPI) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableSpatialNavigation);

  ExtensionTestMessageListener next_step_listener("TEST_STEP_PASSED");
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

// This test verifies that hiding the guest triggers visibility change
// notifications.
IN_PROC_BROWSER_TEST_F(WebViewVisibilityTest, GuestVisibilityChanged) {
  LoadAppWithGuest("web_view/visibility_changed");

  base::RunLoop run_loop;
  RenderWidgetHostVisibilityObserver observer(
      GetGuestRenderFrameHost()->GetRenderWidgetHost(), run_loop.QuitClosure());

  // Handled in platform_apps/web_view/visibility_changed/main.js
  SendMessageToEmbedder("hide-guest");
  if (!observer.hidden_observed())
    run_loop.Run();
}

// This test verifies that hiding the embedder also hides the guest.
IN_PROC_BROWSER_TEST_F(WebViewVisibilityTest, EmbedderVisibilityChanged) {
  LoadAppWithGuest("web_view/visibility_changed");

  base::RunLoop run_loop;
  RenderWidgetHostVisibilityObserver observer(
      GetGuestRenderFrameHost()->GetRenderWidgetHost(), run_loop.QuitClosure());

  // Handled in platform_apps/web_view/visibility_changed/main.js
  SendMessageToEmbedder("hide-embedder");
  if (!observer.hidden_observed())
    run_loop.Run();
}

// This test verifies that reloading the embedder reloads the guest (and doest
// not crash).
IN_PROC_BROWSER_TEST_F(WebViewTest, ReloadEmbedder) {
  // Just load a guest from other test, we do not want to add a separate
  // platform_app for this test.
  LoadAppWithGuest("web_view/visibility_changed");

  ExtensionTestMessageListener launched_again_listener("WebViewTest.LAUNCHED");
  GetEmbedderWebContents()->GetController().Reload(content::ReloadType::NORMAL,
                                                   false);
  ASSERT_TRUE(launched_again_listener.WaitUntilSatisfied());
}

// This test ensures JavaScript errors ("Cannot redefine property") do not
// happen when a <webview> is removed from DOM and added back.
IN_PROC_BROWSER_TEST_F(WebViewTest, AddRemoveWebView_AddRemoveWebView) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunExtensionTest("platform_apps/web_view/addremove",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewSizeTest, AutoSize) {
  ASSERT_TRUE(RunExtensionTest("platform_apps/web_view/autosize",
                               {.launch_as_platform_app = true}))
      << message_;
}

// Test for http://crbug.com/419611.
IN_PROC_BROWSER_TEST_F(WebViewTest, DisplayNoneSetSrc) {
  LoadAndLaunchPlatformApp("web_view/display_none_set_src",
                           "WebViewTest.LAUNCHED");
  // Navigate the guest while it's in "display: none" state.
  SendMessageToEmbedder("navigate-guest");
  GetGuestViewManager()->WaitForSingleGuestViewCreated();

  // Now attempt to navigate the guest again.
  SendMessageToEmbedder("navigate-guest");

  ExtensionTestMessageListener test_passed_listener("WebViewTest.PASSED");
  // Making the guest visible would trigger loadstop.
  SendMessageToEmbedder("show-guest");
  EXPECT_TRUE(test_passed_listener.WaitUntilSatisfied());
}

// Checks that {allFrames: true} injects script correctly to subframes
// inside <webview>.
IN_PROC_BROWSER_TEST_F(WebViewTest, ExecuteScript) {
  ASSERT_TRUE(RunExtensionTest(
      "platform_apps/web_view/common",
      {.custom_arg = "execute_script", .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, ExecuteCode) {
  ASSERT_TRUE(RunExtensionTest(
      "platform_apps/web_view/common",
      {.custom_arg = "execute_code", .launch_as_platform_app = true}))
      << message_;
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

IN_PROC_BROWSER_TEST_F(WebViewSizeTest, Shim_TestAutosizeHeight) {
  TestHelper("testAutosizeHeight", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewDPITest, Shim_TestAutosizeBeforeNavigation) {
  TestHelper("testAutosizeBeforeNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewSizeTest, Shim_TestAutosizeBeforeNavigation) {
  TestHelper("testAutosizeBeforeNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewDPITest, Shim_TestAutosizeRemoveAttributes) {
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

  std::vector<content::RenderFrameHost*> guest_frames_list;
  GetGuestViewManager()->GetGuestRenderFrameHostList(&guest_frames_list);
  ASSERT_EQ(1u, guest_frames_list.size());
  content::RenderFrameHost* guest_frame = guest_frames_list[0];

  const gfx::Rect embedder_rect = embedder_contents->GetContainerBounds();
  const gfx::Rect guest_rect =
      guest_frame->GetRenderWidgetHost()->GetView()->GetViewBounds();
  const gfx::Point click_point(guest_rect.x() - embedder_rect.x() + 10,
                               guest_rect.y() - embedder_rect.y() + 10);

  LeftMouseClick mouse_click(guest_frame);
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
#define MAYBE_Shim_TestDisplayNoneWebviewRemoveChild \
  DISABLED_Shim_TestDisplayNoneWebviewRemoveChild
#else
#define MAYBE_Shim_TestDisplayNoneWebviewRemoveChild \
  Shim_TestDisplayNoneWebviewRemoveChild
#endif
// Flaky on most desktop platforms: https://crbug.com/1115106.
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
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  GTEST_SKIP() << "Flaky on Linux and Mac; http://crbug.com/1182801";
#else
  TestHelper("testAddContentScriptsWithNewWindowAPI", "web_view/shim",
             NEEDS_TEST_SERVER);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
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

#if BUILDFLAG(IS_MAC)
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

  // The first <webview> tag in the test will run window.open(), which the
  // embedder will translate into an injected second <webview> tag.  Ensure
  // that the two <webview>'s remain in the same BrowsingInstance and
  // StoragePartition.
  GetGuestViewManager()->WaitForNumGuestsCreated(2);
  std::vector<content::RenderFrameHost*> guest_rfh_list;
  GetGuestViewManager()->GetGuestRenderFrameHostList(&guest_rfh_list);
  ASSERT_EQ(2u, guest_rfh_list.size());
  auto* guest1 = guest_rfh_list[0];
  auto* guest2 = guest_rfh_list[1];
  ASSERT_NE(guest1, guest2);
  auto* guest_instance1 = guest1->GetSiteInstance();
  auto* guest_instance2 = guest2->GetSiteInstance();
  EXPECT_TRUE(guest_instance1->IsGuest());
  EXPECT_TRUE(guest_instance2->IsGuest());
  EXPECT_EQ(guest_instance1->GetStoragePartitionConfig(),
            guest_instance2->GetStoragePartitionConfig());
  EXPECT_TRUE(guest_instance1->IsRelatedSiteInstance(guest_instance2));
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
  GURL newwindow_url("about:blank#noreferrer");
  content::TestNavigationObserver observer(newwindow_url);
  observer.StartWatchingNewWebContents();

  TestHelper("testNewWindowNoReferrerLink", "web_view/shim", NEEDS_TEST_SERVER);

  // The first <webview> tag in the test will run window.open(), which the
  // embedder will translate into an injected second <webview> tag.  Ensure
  // that both <webview>'s are in guest SiteInstances and in the same
  // StoragePartition.
  GetGuestViewManager()->WaitForNumGuestsCreated(2);
  std::vector<content::RenderFrameHost*> guest_rfh_list;
  GetGuestViewManager()->GetGuestRenderFrameHostList(&guest_rfh_list);
  ASSERT_EQ(2u, guest_rfh_list.size());
  auto* guest1_rfh = guest_rfh_list[0];
  auto* guest2_rfh = guest_rfh_list[1];
  ASSERT_NE(guest1_rfh, guest2_rfh);
  auto* guest_instance1 = guest1_rfh->GetSiteInstance();
  auto* guest_instance2 = guest2_rfh->GetSiteInstance();
  EXPECT_TRUE(guest_instance1->IsGuest());
  EXPECT_TRUE(guest_instance2->IsGuest());
  EXPECT_EQ(guest_instance1->GetStoragePartitionConfig(),
            guest_instance2->GetStoragePartitionConfig());

  // The new guest should be in a different BrowsingInstance.
  EXPECT_FALSE(guest_instance1->IsRelatedSiteInstance(guest_instance2));

  // Check that the source SiteInstance used when the first guest opened the
  // new noreferrer window is also a guest SiteInstance in the same
  // StoragePartition.
  observer.Wait();
  ASSERT_TRUE(observer.last_source_site_instance());
  EXPECT_TRUE(observer.last_source_site_instance()->IsGuest());
  EXPECT_EQ(observer.last_source_site_instance()->GetStoragePartitionConfig(),
            guest_instance1->GetStoragePartitionConfig());
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest,
                       Shim_TestWebViewAndEmbedderInNewWindow) {
  TestHelper("testWebViewAndEmbedderInNewWindow", "web_view/shim",
             NEEDS_TEST_SERVER);
  content::WebContents* embedder_web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(embedder_web_contents);

  // Make sure opener and owner for the empty_guest source are different.
  // In general, we should have two guests and two embedders and all four
  // should be different.
  std::vector<content::RenderFrameHost*> guest_rfh_list;
  GetGuestViewManager()->GetGuestRenderFrameHostList(&guest_rfh_list);
  ASSERT_EQ(2u, guest_rfh_list.size());
  content::RenderFrameHost* new_window_guest_frame = guest_rfh_list[0];
  content::RenderFrameHost* empty_guest_frame = guest_rfh_list[1];
  EXPECT_TRUE(empty_guest_frame->GetProcess()->IsForGuestsOnly());

  guest_view::GuestViewBase* empty_guest_view =
      GetGuestViewManager()->GetLastGuestViewCreated();
  ASSERT_EQ(empty_guest_view->GetGuestMainFrame(), empty_guest_frame);
  ASSERT_NE(empty_guest_view->GetGuestMainFrame(), new_window_guest_frame);

  content::WebContents* empty_guest_embedder =
      empty_guest_view->embedder_web_contents();
  ASSERT_TRUE(empty_guest_embedder);
  ASSERT_NE(empty_guest_embedder->GetPrimaryMainFrame(), empty_guest_frame);

  // TODO(crbug.com/40202416): Introduce a test helper to expose the opener as a
  // `content::Page`.
  content::RenderFrameHost* empty_guest_opener =
      empty_guest_view->web_contents()
          ->GetFirstWebContentsInLiveOriginalOpenerChain()
          ->GetPrimaryMainFrame();
  ASSERT_TRUE(empty_guest_opener);
  ASSERT_NE(empty_guest_opener, empty_guest_embedder->GetPrimaryMainFrame());

  // The JS part of this test, we've already checked the opener relationship of
  // the two webviews. We also need to check the window reference from the
  // initial window.open call in the opener. We need to do this from the C++
  // part in order to run script in the main world.
  EXPECT_EQ(true,
            content::EvalJs(new_window_guest_frame, "!!window.newWindow"));
  EXPECT_EQ(url::kAboutBlankURL,
            content::EvalJs(new_window_guest_frame,
                            "window.newWindow.location.href"));
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest,
                       Shim_TestWebViewAndEmbedderInNewWindow_Noopener) {
  TestHelper("testWebViewAndEmbedderInNewWindow_Noopener", "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest,
                       Shim_TestNewWindowAttachToExisting) {
  TestHelper("testNewWindowAttachToExisting", "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest, Shim_TestNewWindowNoDeadlock) {
  TestHelper("testNewWindowNoDeadlock", "web_view/shim", NEEDS_TEST_SERVER);
}

// This is a regression test for crbug.com/1309302. It launches an app
// with two iframes and a webview within each of the iframes. The
// purpose of the test is to ensure that webRequest subevent names are
// unique across all webviews within the app.
IN_PROC_BROWSER_TEST_F(WebViewTest, TwoIframesWebRequest) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving webview pages.
  ExtensionTestMessageListener ready1("ready1", ReplyBehavior::kWillReply);
  ExtensionTestMessageListener ready2("ready2", ReplyBehavior::kWillReply);

  LoadAndLaunchPlatformApp("web_view/two_iframes_web_request", "Launched");
  EXPECT_TRUE(ready1.WaitUntilSatisfied());
  EXPECT_TRUE(ready2.WaitUntilSatisfied());

  ExtensionTestMessageListener finished1("success1");
  finished1.set_failure_message("fail1");
  ExtensionTestMessageListener finished2("success2");
  finished2.set_failure_message("fail2");

  // Reply to the listeners to start the navigations and wait for the
  // results.
  ready1.Reply("");
  ready2.Reply("");
  EXPECT_TRUE(finished1.WaitUntilSatisfied());
  EXPECT_TRUE(finished2.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest,
                       NewWindow_AttachAfterOpenerDestroyed) {
  TestHelper("testNewWindowAttachAfterOpenerDestroyed", "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest, NewWindow_AttachInSubFrame) {
  TestHelper("testNewWindowAttachInSubFrame", "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest,
                       NewWindow_NewWindowNameTakesPrecedence) {
  TestHelper("testNewWindowNameTakesPrecedence", "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest,
                       NewWindow_WebViewNameTakesPrecedence) {
  TestHelper("testNewWindowWebViewNameTakesPrecedence", "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest, NewWindow_NoName) {
  TestHelper("testNewWindowNoName", "web_view/newwindow", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest, NewWindow_Redirect) {
  TestHelper("testNewWindowRedirect", "web_view/newwindow", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest, NewWindow_Close) {
  TestHelper("testNewWindowClose", "web_view/newwindow", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest, NewWindow_DeferredAttachment) {
  TestHelper("testNewWindowDeferredAttachment", "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest, NewWindow_ExecuteScript) {
  TestHelper("testNewWindowExecuteScript", "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest, NewWindow_DeclarativeWebRequest) {
  TestHelper("testNewWindowDeclarativeWebRequest", "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest,
                       NewWindow_DiscardAfterOpenerDestroyed) {
  TestHelper("testNewWindowDiscardAfterOpenerDestroyed", "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest, NewWindow_WebRequest) {
  TestHelper("testNewWindowWebRequest", "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

// A custom elements bug needs to be addressed to enable this test:
// See http://crbug.com/282477 for more information.
IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest,
                       DISABLED_NewWindow_WebRequestCloseWindow) {
  TestHelper("testNewWindowWebRequestCloseWindow", "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest,
                       NewWindow_WebRequestRemoveElement) {
  TestHelper("testNewWindowWebRequestRemoveElement", "web_view/newwindow",
             NEEDS_TEST_SERVER);
}

// Ensure that when one <webview> makes a window.open() call that references
// another <webview> by name, the opener is updated without a crash. Regression
// test for https://crbug.com/1013553.
IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest, NewWindow_UpdateOpener) {
  TestHelper("testNewWindowAndUpdateOpener", "web_view/newwindow",
             NEEDS_TEST_SERVER);

  // The first <webview> tag in the test will run window.open(), which the
  // embedder will translate into an injected second <webview> tag, after which
  // test control will return here.  Wait until there are two guests; i.e.,
  // until the second <webview>'s guest is also created.
  GetGuestViewManager()->WaitForNumGuestsCreated(2);

  std::vector<content::RenderFrameHost*> guest_rfh_list;
  GetGuestViewManager()->GetGuestRenderFrameHostList(&guest_rfh_list);
  ASSERT_EQ(2u, guest_rfh_list.size());
  content::RenderFrameHost* guest1 = guest_rfh_list[0];
  content::RenderFrameHost* guest2 = guest_rfh_list[1];
  ASSERT_NE(guest1, guest2);

  // Change first guest's window.name to "foo" and check that it does not
  // have an opener to start with.
  EXPECT_TRUE(content::ExecJs(guest1, "window.name = 'foo'"));
  EXPECT_EQ("foo", content::EvalJs(guest1, "window.name"));
  EXPECT_EQ(true, content::EvalJs(guest1, "window.opener == null"));

  // Create a subframe in the second guest.  This is needed because the crash
  // in crbug.com/1013553 only happened when trying to incorrectly create
  // proxies for a subframe.
  EXPECT_TRUE(content::ExecJs(
      guest2, "document.body.appendChild(document.createElement('iframe'));"));

  // Update the opener of |guest1| to point to |guest2|.  This triggers
  // creation of proxies on the new opener chain, which should not crash.
  EXPECT_TRUE(content::ExecJs(guest2, "window.open('', 'foo');"));

  // Ensure both guests have the proper opener relationship set up.  Namely,
  // each guest's opener should point to the other guest, creating a cycle.
  EXPECT_EQ(true, content::EvalJs(guest1, "window.opener.opener === window"));
  EXPECT_EQ(true, content::EvalJs(guest2, "window.opener.opener === window"));
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest,
                       NewWindow_OpenerDestroyedWhileUnattached) {
  TestHelper("testNewWindowOpenerDestroyedWhileUnattached",
             "web_view/newwindow", NEEDS_TEST_SERVER);
  ASSERT_EQ(2u, GetGuestViewManager()->num_guests_created());

  // We have two guests in this test, one is the initial one, the other
  // is the newwindow one.
  // Before the embedder goes away, both the guests should go away.
  // This ensures that unattached guests are gone if opener is gone.
  GetGuestViewManager()->WaitForAllGuestsDeleted();
}

// Creates a guest in a unattached state, then confirms that calling
// |RenderFrameHost::ForEachRenderFrameHost| on the embedder will include the
// guest's frame.
IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest,
                       NewWindow_UnattachedVisitedByForEachRenderFrameHost) {
  TestHelper("testNewWindowDeferredAttachmentIndefinitely",
             "web_view/newwindow", NEEDS_TEST_SERVER);
  // The test creates two guests, one of which is created but left in an
  // unattached state.
  GetGuestViewManager()->WaitForNumGuestsCreated(2);

  content::WebContents* embedder = GetEmbedderWebContents();
  auto* unattached_guest = GetGuestViewManager()->GetLastGuestViewCreated();
  ASSERT_TRUE(unattached_guest);
  ASSERT_EQ(embedder, unattached_guest->owner_web_contents());
  ASSERT_FALSE(unattached_guest->attached());
  ASSERT_FALSE(unattached_guest->embedder_web_contents());

  std::vector<content::RenderFrameHost*> guest_rfh_list;
  GetGuestViewManager()->GetGuestRenderFrameHostList(&guest_rfh_list);
  ASSERT_EQ(2u, guest_rfh_list.size());
  content::RenderFrameHost* unattached_guest_rfh =
      unattached_guest->GetGuestMainFrame();
  content::RenderFrameHost* other_guest_rfh =
      (guest_rfh_list[0] == unattached_guest_rfh) ? guest_rfh_list[1]
                                                  : guest_rfh_list[0];

  content::RenderFrameHost* embedder_main_frame =
      embedder->GetPrimaryMainFrame();
  EXPECT_THAT(content::CollectAllRenderFrameHosts(embedder_main_frame),
              testing::UnorderedElementsAre(
                  embedder_main_frame, other_guest_rfh, unattached_guest_rfh));

  // In either case, GetParentOrOuterDocument does not escape GuestViews.
  EXPECT_EQ(nullptr, other_guest_rfh->GetParentOrOuterDocument());
  EXPECT_EQ(nullptr, unattached_guest_rfh->GetParentOrOuterDocument());
  EXPECT_EQ(other_guest_rfh, other_guest_rfh->GetOutermostMainFrame());
  EXPECT_EQ(unattached_guest_rfh,
            unattached_guest_rfh->GetOutermostMainFrame());
  // GetParentOrOuterDocumentOrEmbedder does escape GuestViews.
  EXPECT_EQ(embedder_main_frame,
            other_guest_rfh->GetParentOrOuterDocumentOrEmbedder());
  EXPECT_EQ(embedder_main_frame,
            other_guest_rfh->GetOutermostMainFrameOrEmbedder());
  // The unattached guest should still be considered to have an embedder.
  EXPECT_EQ(embedder_main_frame,
            unattached_guest_rfh->GetParentOrOuterDocumentOrEmbedder());
  EXPECT_EQ(embedder_main_frame,
            unattached_guest_rfh->GetOutermostMainFrameOrEmbedder());
  EXPECT_EQ(embedder,
            unattached_guest->web_contents()->GetResponsibleWebContents());
}

// Creates a guest in a unattached state, then confirms that calling
// the various view methods return null.
IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest,
                       NewWindow_UnattachedVerifyViewMethods) {
  TestHelper("testNewWindowDeferredAttachmentIndefinitely",
             "web_view/newwindow", NEEDS_TEST_SERVER);
  GetGuestViewManager()->WaitForNumGuestsCreated(2);

  content::WebContents* embedder = GetEmbedderWebContents();
  auto* unattached_guest = GetGuestViewManager()->GetLastGuestViewCreated();
  ASSERT_TRUE(unattached_guest);
  ASSERT_EQ(embedder, unattached_guest->owner_web_contents());
  ASSERT_FALSE(unattached_guest->attached());
  ASSERT_FALSE(unattached_guest->embedder_web_contents());
  ASSERT_FALSE(unattached_guest->web_contents()->GetNativeView());
  ASSERT_FALSE(unattached_guest->web_contents()->GetContentNativeView());
  ASSERT_FALSE(unattached_guest->web_contents()->GetTopLevelNativeWindow());
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

  // Run the test and wait until the guest WebContents is available and has
  // finished loading.
  ExtensionTestMessageListener guest_loaded_listener("guest-loaded");
  EXPECT_TRUE(content::ExecJs(embedder_web_contents,
                              "runTest('testRemoveWebviewOnExit')"));

  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  EXPECT_TRUE(guest_view);
  EXPECT_TRUE(guest_view->GetGuestMainFrame()->GetProcess()->IsForGuestsOnly());
  ASSERT_TRUE(guest_loaded_listener.WaitUntilSatisfied());

  // Tell the embedder to kill the guest.
  EXPECT_TRUE(
      content::ExecJs(embedder_web_contents, "removeWebviewOnExitDoCrash();"));

  // Wait until the guest WebContents is destroyed.
  GetGuestViewManager()->WaitForLastGuestDeleted();
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

class WebViewSSLErrorTest : public WebViewTest {
 public:
  WebViewSSLErrorTest() = default;
  ~WebViewSSLErrorTest() override = default;

  // Loads the guest at "web_view/ssl/https_page.html" with an SSL error, and
  // asserts the security interstitial is displayed within the guest instead of
  // through the embedder's WebContents.
  void SSLTestHelper() {
    // Starts a HTTPS server so we can load a page with a SSL error inside
    // guest.
    net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
    https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
    https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server.Start());

    LoadAndLaunchPlatformApp("web_view/ssl", "EmbedderLoaded");

    LoadEmptyGuest();

    const auto target_url = https_server.GetURL(
        "/extensions/platform_apps/web_view/ssl/https_page.html");
    SetGuestURL(target_url, /*expect_successful_navigation=*/false);

    // Guest's `target_url` is served by an HTTP server with a cert error.
    // A security error within a guest should not cause an interstitial to be
    // shown in the embedder.
    ASSERT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(
        GetFirstAppWindowWebContents()));

    auto* guest = GetGuestViewManager()->GetLastGuestViewCreated();
    ASSERT_TRUE(guest->GetGuestMainFrame()->IsErrorDocument());
    ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
        guest->web_contents()));
  }

  void LoadEmptyGuest() {
    // Creates the guest, and asserts its successful creation.
    content::WebContents* embedder_web_contents =
        GetFirstAppWindowWebContents();
    ExtensionTestMessageListener guest_added("GuestAddedToDom");
    EXPECT_TRUE(content::ExecJs(embedder_web_contents, "createGuest();"));
    ASSERT_TRUE(guest_added.WaitUntilSatisfied());
    auto* guest_main_frame =
        GetGuestViewManager()->WaitForSingleGuestRenderFrameHostCreated();
    ASSERT_TRUE(guest_main_frame->GetProcess()->IsForGuestsOnly());
  }

  // Loads the `guest_url` by setting the `src` of the guest. This helper
  // assumes the app is loaded, and assumes the app already has a guest created.
  void SetGuestURL(const GURL& guest_url, bool expect_successful_navigation) {
    auto* embedder_web_contents = GetFirstAppWindowWebContents();
    ASSERT_TRUE(embedder_web_contents);
    auto* guest_main_frame =
        GetGuestViewManager()->GetLastGuestRenderFrameHostCreated();
    ASSERT_TRUE(guest_main_frame);

    content::TestFrameNavigationObserver guest_navi_obs(guest_main_frame);
    ASSERT_TRUE(
        content::ExecJs(embedder_web_contents,
                        content::JsReplace("loadGuestUrl($1);", guest_url)));
    guest_navi_obs.Wait();

    // Do not dereference `guest_main_frame` beyond here as it can be destroyed
    // at this point.

    ASSERT_EQ(guest_navi_obs.last_navigation_succeeded(),
              expect_successful_navigation);
    if (expect_successful_navigation) {
      ASSERT_EQ(guest_navi_obs.last_net_error_code(), net::Error::OK);
      ASSERT_EQ(guest_navi_obs.last_committed_url(), guest_url);
    } else {
      // `https_server` in `WebViewSSLErrorTest::SSLTestHelper` is configured
      // with `CERT_MISMATCHED_NAME`.
      ASSERT_EQ(guest_navi_obs.last_net_error_code(),
                net::Error::ERR_CERT_COMMON_NAME_INVALID);
      // `TestFrameNavigationObserver`'s `last_committed_url_` is only set if
      // the navigation does not result in an error page.
      ASSERT_EQ(guest_navi_obs.last_committed_url(), GURL());
    }
  }
};

// Test makes sure that an interstitial is shown in `<webview>` with an SSL
// error.
// Flaky on Win dbg: crbug.com/779973
#if BUILDFLAG(IS_WIN) && !defined(NDEBUG)
#define MAYBE_ShowInterstitialForSSLError DISABLED_ShowInterstitialForSSLError
#else
#define MAYBE_ShowInterstitialForSSLError ShowInterstitialForSSLError
#endif
IN_PROC_BROWSER_TEST_F(WebViewSSLErrorTest, MAYBE_ShowInterstitialForSSLError) {
  SSLTestHelper();
}

// Ensure that when a guest is created and navigated to a URL that triggers an
// SSL interstitial, and then the "Back to safety" button is activated on the
// interstitial, the guest doesn't crash trying to load the NTP (the usual
// known-safe page used to navigate back from such interstitials when there's
// no other page in history to go to).  See https://crbug.com/1444221.
IN_PROC_BROWSER_TEST_F(WebViewSSLErrorTest, NavigateBackFromSSLError) {
  // Starts a HTTPS server so we can load a page with a SSL error inside a
  // guest.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  LoadAndLaunchPlatformApp("web_view/ssl", "EmbedderLoaded");

  const auto failure_url = https_server.GetURL(
      "/extensions/platform_apps/web_view/ssl/https_page.html");
  EXPECT_TRUE(content::ExecJs(
      GetFirstAppWindowWebContents(),
      content::JsReplace("var w = document.createElement('webview');"
                         "w.src = $1;"
                         "document.body.appendChild(w);",
                         failure_url)));
  GetGuestViewManager()->WaitForSingleGuestRenderFrameHostCreated();

  // The navigation should fail and show an interstitial in the guest.
  auto* guest = GetGuestViewManager()->GetLastGuestViewCreated();
  EXPECT_FALSE(WaitForLoadStop(guest->web_contents()));
  ASSERT_TRUE(guest->GetGuestMainFrame()->IsErrorDocument());
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
      guest->web_contents()));

  // Simulate invoking the "Back to safety" button.  This should dismiss the
  // interstitial and navigate the guest to a known safe URL that can always
  // load in a guest (in this case, about:blank).
  security_interstitials::SecurityInterstitialTabHelper* helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          guest->web_contents());
  ASSERT_TRUE(helper);
  auto* interstitial =
      helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting();
  ASSERT_TRUE(interstitial);
  interstitial->CommandReceived(base::NumberToString(
      security_interstitials::SecurityInterstitialCommand::CMD_DONT_PROCEED));

  EXPECT_TRUE(WaitForLoadStop(guest->web_contents()));
  ASSERT_FALSE(guest->GetGuestMainFrame()->IsErrorDocument());
  ASSERT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(
      guest->web_contents()));
}

// Test makes sure that the interstitial is registered in the
// `RenderWidgetHostInputEventRouter` when inside a `<webview>`.
// Flaky on Win dbg: crbug.com/779973
#if BUILDFLAG(IS_WIN) && !defined(NDEBUG)
#define MAYBE_InterstitialPageRouteEvents DISABLED_InterstitialPageRouteEvents
#else
#define MAYBE_InterstitialPageRouteEvents InterstitialPageRouteEvents
#endif
IN_PROC_BROWSER_TEST_F(WebViewSSLErrorTest, MAYBE_InterstitialPageRouteEvents) {
  SSLTestHelper();

  std::vector<content::RenderWidgetHostView*> hosts =
      content::GetInputEventRouterRenderWidgetHostViews(
          GetFirstAppWindowWebContents());

  ASSERT_TRUE(base::Contains(
      hosts, GetFirstAppWindowWebContents()->GetPrimaryMainFrame()->GetView()));

  auto* guest_main_frame =
      GetGuestViewManager()->GetLastGuestRenderFrameHostCreated();
  ASSERT_TRUE(guest_main_frame);
  ASSERT_TRUE(base::Contains(hosts, guest_main_frame->GetView()));
}

// Test makes sure that the browser does not crash when a `<webview>` navigates
// out of an interstitial caused by a SSL error.
// Flaky on Win dbg: crbug.com/779973
#if BUILDFLAG(IS_WIN) && !defined(NDEBUG)
#define MAYBE_InterstitialPageDetach DISABLED_InterstitialPageDetach
#else
#define MAYBE_InterstitialPageDetach InterstitialPageDetach
#endif
IN_PROC_BROWSER_TEST_F(WebViewSSLErrorTest, MAYBE_InterstitialPageDetach) {
  SSLTestHelper();

  // Navigate to about:blank
  const GURL blank(url::kAboutBlankURL);
  SetGuestURL(blank, /*expect_successful_navigation=*/true);
}

// This test makes sure the browser process does not crash if app is closed
// while an interstitial is being shown in guest.
// Flaky on Win dbg: crbug.com/779973
#if BUILDFLAG(IS_WIN) && !defined(NDEBUG)
#define MAYBE_InterstitialTearDown DISABLED_InterstitialTearDown
#else
#define MAYBE_InterstitialTearDown InterstitialTearDown
#endif
IN_PROC_BROWSER_TEST_F(WebViewSSLErrorTest, MAYBE_InterstitialTearDown) {
  SSLTestHelper();

  // Now close the app while the interstitial is being shown in the guest.
  extensions::AppWindow* window = GetFirstAppWindow();
  window->GetBaseWindow()->Close();
}

// This test makes sure the browser process does not crash if browser is shut
// down while an interstitial is being shown in guest.
IN_PROC_BROWSER_TEST_F(WebViewSSLErrorTest,
                       InterstitialTearDownOnBrowserShutdown) {
  SSLTestHelper();

  // Now close the app while the interstitial is being shown in the guest.
  extensions::AppWindow* window = GetFirstAppWindow();
  window->GetBaseWindow()->Close();

  // The error page is not destroyed immediately, so the
  // `RenderWidgetHostViewChildFrame` for it is still there, closing all
  // renderer processes will cause the RWHVGuest's `RenderProcessGone()`
  // shutdown path to be exercised.
  chrome::CloseAllBrowsers();
}

// This allows us to specify URLs which trigger Safe Browsing.
class WebViewSafeBrowsingTest : public WebViewTest {
 public:
  WebViewSafeBrowsingTest()
      : safe_browsing_factory_(
            std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>()) {
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    WebViewTest::SetUpOnMainThread();
  }

 protected:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    fake_safe_browsing_database_manager_ =
        base::MakeRefCounted<safe_browsing::FakeSafeBrowsingDatabaseManager>(
            content::GetUIThreadTaskRunner({}));
    safe_browsing_factory_->SetTestDatabaseManager(
        fake_safe_browsing_database_manager_.get());
    safe_browsing::SafeBrowsingService::RegisterFactory(
        safe_browsing_factory_.get());
    WebViewTest::CreatedBrowserMainParts(browser_main_parts);
  }

  void TearDown() override {
    WebViewTest::TearDown();
    safe_browsing::SafeBrowsingService::RegisterFactory(nullptr);
  }

  void AddDangerousUrl(const GURL& dangerous_url) {
    fake_safe_browsing_database_manager_->AddDangerousUrl(
        dangerous_url, safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  }

 private:
  scoped_refptr<safe_browsing::FakeSafeBrowsingDatabaseManager>
      fake_safe_browsing_database_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;
};

IN_PROC_BROWSER_TEST_F(WebViewSafeBrowsingTest,
                       Shim_TestLoadAbortSafeBrowsing) {
  // We start the test server here, instead of in TestHelper, because we need
  // to know the URL to treat as dangerous before running the rest of the test.
  ASSERT_TRUE(StartEmbeddedTestServer());
  AddDangerousUrl(embedded_test_server()->GetURL("evil.com", "/title1.html"));
  TestHelper("testLoadAbortSafeBrowsing", "web_view/shim", NO_TEST_SERVER);
}

// Tests that loading an HTTPS page in a guest <webview> with HTTPS-First Mode
// enabled doesn't crash nor shows error page.
// Regression test for crbug.com/1233889
IN_PROC_BROWSER_TEST_F(WebViewSSLErrorTest, GuestLoadsHttpsWithoutError) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled,
                                               true);

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());
  GURL guest_url = https_server.GetURL("/simple.html");
  LoadAndLaunchPlatformApp("web_view/ssl", "EmbedderLoaded");
  LoadEmptyGuest();
  SetGuestURL(guest_url, /*expect_successful_navigation=*/true);

  // Page should load without any error / crash.
  auto* embedder_main_frame =
      GetFirstAppWindowWebContents()->GetPrimaryMainFrame();
  auto* guest_main_frame =
      GetGuestViewManager()->GetLastGuestRenderFrameHostCreated();

  ASSERT_FALSE(guest_main_frame->IsErrorDocument());
  ASSERT_FALSE(embedder_main_frame->IsErrorDocument());
  ASSERT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(
      GetFirstAppWindowWebContents()));
}

// Tests that loading an HTTP page in a guest <webview> with HTTPS-First Mode
// enabled doesn't crash and doesn't trigger the error page.
IN_PROC_BROWSER_TEST_F(WebViewSSLErrorTest, GuestLoadsHttpWithoutError) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kHttpsOnlyModeEnabled,
                                               true);

  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL guest_url = embedded_test_server()->GetURL("/simple.html");
  LoadAndLaunchPlatformApp("web_view/ssl", "EmbedderLoaded");
  LoadEmptyGuest();
  SetGuestURL(guest_url, /*expect_successful_navigation=*/true);

  // Page should load without any error / crash.
  auto* embedder_main_frame =
      GetFirstAppWindowWebContents()->GetPrimaryMainFrame();
  auto* guest_main_frame =
      GetGuestViewManager()->GetLastGuestRenderFrameHostCreated();

  ASSERT_FALSE(guest_main_frame->IsErrorDocument());
  ASSERT_FALSE(embedder_main_frame->IsErrorDocument());
  ASSERT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(
      GetFirstAppWindowWebContents()));
}

// Verify that guests cannot be navigated to disallowed URLs, such as
// chrome:// URLs, directly via the content/public API.  The enforcement for
// this typically happens in the embedder layer, catching cases where the
// embedder navigates a guest, but Chrome features could bypass that
// enforcement by directly navigating guests.  This test verifies that if that
// were to happen, //content would still gracefully disallow attempts to load
// disallowed URLs in guests without crashing.
IN_PROC_BROWSER_TEST_F(WebViewTest, CannotNavigateGuestToChromeURL) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load an app with a <webview> guest that starts at a data: URL.
  LoadAppWithGuest("web_view/simple");

  auto* guest = GetGuestViewManager()->GetLastGuestViewCreated();
  auto* guest_main_frame = guest->GetGuestMainFrame();
  GURL original_url = guest_main_frame->GetLastCommittedURL();

  // Try to navigate <webview> to a chrome: URL directly.
  GURL chrome_url(chrome::kChromeUINewTabURL);
  content::TestFrameNavigationObserver observer(guest_main_frame);
  guest->GetController().LoadURL(chrome_url, content::Referrer(),
                                 ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                 std::string());

  // The navigation should be aborted, and the last committed URL should
  // remain unchanged.
  EXPECT_FALSE(observer.navigation_started());
  EXPECT_EQ(original_url, guest_main_frame->GetLastCommittedURL());
  EXPECT_NE(chrome_url, guest_main_frame->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(WebViewTest, ShimSrcAttribute) {
  ASSERT_TRUE(RunExtensionTest("platform_apps/web_view/src_attribute",
                               {.launch_as_platform_app = true}))
      << message_;
}

// This test verifies that prerendering has been disabled inside <webview>.
// This test is here rather than in PrerenderBrowserTest for testing convenience
// only. If it breaks then this is a bug in the prerenderer.
IN_PROC_BROWSER_TEST_F(WebViewTest, NoPrerenderer) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  LoadAndLaunchPlatformApp("web_view/noprerenderer", "guest-loaded");
  auto* guest_rfh =
      GetGuestViewManager()->WaitForSingleGuestRenderFrameHostCreated();
  ASSERT_TRUE(guest_rfh);

  NoStatePrefetchLinkManager* no_state_prefetch_link_manager =
      NoStatePrefetchLinkManagerFactory::GetForBrowserContext(
          guest_rfh->GetBrowserContext());
  ASSERT_TRUE(no_state_prefetch_link_manager != nullptr);
  EXPECT_TRUE(no_state_prefetch_link_manager->IsEmpty());
}

// Verify that existing <webview>'s are detected when the task manager starts
// up.
IN_PROC_BROWSER_TEST_F(WebViewTest, TaskManagerExistingWebView) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  LoadAndLaunchPlatformApp("web_view/task_manager", "guest-loaded");
  ASSERT_TRUE(
      GetGuestViewManager()->WaitForSingleGuestRenderFrameHostCreated());

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

  LoadAndLaunchPlatformApp("web_view/task_manager", "guest-loaded");
  ASSERT_TRUE(
      GetGuestViewManager()->WaitForSingleGuestRenderFrameHostCreated());

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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), set_cookie_url));
  ASSERT_TRUE(RunExtensionTest("platform_apps/web_view/cookie_isolation",
                               {.launch_as_platform_app = true}))
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  ASSERT_TRUE(RunExtensionTest(
      "platform_apps/web_view/storage_persistence",
      {.custom_arg = "PRE_StoragePersistence", .launch_as_platform_app = true}))
      << message_;
  content::EnsureCookiesFlushed(profile());
}

// This is the post-reset portion of the StoragePersistence test.  See
// PRE_StoragePersistence for main comment.
IN_PROC_BROWSER_TEST_F(WebViewTest, StoragePersistence) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // We don't care where the main browser is on this test.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  ASSERT_TRUE(RunExtensionTest(
      "platform_apps/web_view/storage_persistence",
      {.custom_arg = "StoragePersistence", .launch_as_platform_app = true}))
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), navigate_to_url));
  ASSERT_TRUE(RunExtensionTest("platform_apps/web_view/dom_storage_isolation",
                               {.launch_as_platform_app = true}));
  // Verify that the browser tab's local/session storage does not have the same
  // values which were stored by the webviews.
  std::string get_local_storage(
      "window.localStorage.getItem('foo') || 'badval'");
  std::string get_session_storage(
      "window.localStorage.getItem('baz') || 'badval'");
  EXPECT_EQ("badval",
            content::EvalJs(browser()->tab_strip_model()->GetWebContentsAt(0),
                            get_local_storage));
  EXPECT_EQ("badval",
            content::EvalJs(browser()->tab_strip_model()->GetWebContentsAt(0),
                            get_session_storage));
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), navigate_to_url));
  ASSERT_TRUE(RunExtensionTest("platform_apps/web_view/findability_isolation",
                               {.launch_as_platform_app = true}));
}

// This tests IndexedDB isolation for packaged apps with webview tags. It loads
// an app with multiple webview tags and each tag creates an IndexedDB record,
// which the test checks to ensure proper storage isolation is enforced.
IN_PROC_BROWSER_TEST_F(WebViewTest, IndexedDBIsolation) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("platform_apps/web_view/isolation_indexeddb",
                               {.launch_as_platform_app = true}))
      << message_;
}

// This test ensures that closing app window on 'loadcommit' does not crash.
// The test launches an app with guest and closes the window on loadcommit. It
// then launches the app window again. The process is repeated 3 times.
// TODO(crbug.com/40621838): The test is flaky (crash) on ChromeOS debug and
// ASan/LSan
#if BUILDFLAG(IS_CHROMEOS_ASH) && \
    (!defined(NDEBUG) || defined(ADDRESS_SANITIZER))
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

  ExtensionTestMessageListener done_listener("TEST_PASSED");
  done_listener.set_failure_message("TEST_FAILED");
  EXPECT_TRUE(
      content::ExecJs(embedder_web_contents,
                      base::StrCat({"startAllowTest('", test_name, "')"})));
  ASSERT_TRUE(done_listener.WaitUntilSatisfied());

  mock->WaitForRequestMediaPermission();
}

IN_PROC_BROWSER_TEST_F(WebViewTest, OpenURLFromTab_CurrentTab_Abort) {
  LoadAppWithGuest("web_view/simple");

  // Verify that OpenURLFromTab with a window disposition of CURRENT_TAB will
  // navigate the current <webview>.
  ExtensionTestMessageListener load_listener("WebViewTest.LOADSTOP");

  // Navigating to a file URL is forbidden inside a <webview>.
  content::OpenURLParams params(GURL("file://foo"), content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                true /* is_renderer_initiated */);
  GetGuestWebContents()->GetDelegate()->OpenURLFromTab(
      GetGuestWebContents(), params, /*navigation_handle_callback=*/{});

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
  ExtensionTestMessageListener load_listener("WebViewTest.LOADSTOP");

  GURL test_url("http://www.google.com");
  content::OpenURLParams params(
      test_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false /* is_renderer_initiated */);
  GetGuestWebContents()->GetDelegate()->OpenURLFromTab(
      GetGuestWebContents(), params, /*navigation_handle_callback=*/{});

  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  EXPECT_EQ(test_url, GetGuestWebContents()->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest, OpenURLFromTab_NewWindow_Abort) {
  LoadAppWithGuest("web_view/simple");

  // Verify that OpenURLFromTab with a window disposition of NEW_BACKGROUND_TAB
  // will trigger the <webview>'s New Window API.
  ExtensionTestMessageListener new_window_listener("WebViewTest.NEWWINDOW");

  // Navigating to a file URL is forbidden inside a <webview>.
  content::OpenURLParams params(GURL("file://foo"), content::Referrer(),
                                WindowOpenDisposition::NEW_BACKGROUND_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                true /* is_renderer_initiated */);
  GetGuestWebContents()->GetDelegate()->OpenURLFromTab(
      GetGuestWebContents(), params, /*navigation_handle_callback=*/{});

  ASSERT_TRUE(new_window_listener.WaitUntilSatisfied());

  // Verify that a new guest was created.
  content::RenderFrameHost* new_guest_rfh =
      GetGuestViewManager()->GetLastGuestRenderFrameHostCreated();
  EXPECT_NE(GetGuestRenderFrameHost(), new_guest_rfh);

  // Verify that the new <webview> guest ends up at about:blank.
  EXPECT_EQ(GURL(url::kAboutBlankURL), new_guest_rfh->GetLastCommittedURL());
}

// Verify that we handle gracefully having two webviews in the same
// BrowsingInstance with COOP values that would normally make it impossible
// (meaning outside of webviews special case) to group them together.
// This is a regression test for https://crbug.com/1243711.
IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest,
                       NewWindow_DifferentCoopStatesInRelatedWebviews) {
  // Reusing testNewWindowAndUpdateOpener because it is a convenient way to
  // obtain 2 webviews in the same BrowsingInstance. The javascript does
  // nothing more than that.
  TestHelper("testNewWindowAndUpdateOpener", "web_view/newwindow",
             NEEDS_TEST_SERVER);
  GetGuestViewManager()->WaitForNumGuestsCreated(2);

  std::vector<content::RenderFrameHost*> guest_rfh_list;
  GetGuestViewManager()->GetGuestRenderFrameHostList(&guest_rfh_list);
  ASSERT_EQ(2u, guest_rfh_list.size());
  content::RenderFrameHost* guest1 = guest_rfh_list[0];
  content::RenderFrameHost* guest2 = guest_rfh_list[1];
  ASSERT_NE(guest1, guest2);

  // COOP headers are only served over HTTPS. Instantiate an HTTPS server.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  // Navigate one of the <webview> to a COOP: Same-Origin page.
  GURL coop_url(
      https_server.GetURL("/set-header?"
                          "Cross-Origin-Opener-Policy: same-origin"));

  // We should not crash trying to load the COOP page.
  EXPECT_TRUE(content::NavigateToURLFromRenderer(guest2, coop_url));
}

// This test creates a situation where we have two unattached webviews which
// have an opener relationship, and ensures that we can shutdown safely. See
// https://crbug.com/1450397.
IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest, DestroyOpenerBeforeAttachment) {
  TestHelper("testDestroyOpenerBeforeAttachment", "web_view/newwindow",
             NEEDS_TEST_SERVER);
  GetGuestViewManager()->WaitForNumGuestsCreated(2);

  content::RenderProcessHost* embedder_rph =
      GetEmbedderWebContents()->GetPrimaryMainFrame()->GetProcess();
  content::RenderProcessHostWatcher kill_observer(
      embedder_rph, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(embedder_rph->Shutdown(content::RESULT_CODE_KILLED));
  kill_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(WebViewTest, ContextMenuInspectElement) {
  LoadAppWithGuest("web_view/context_menus/basic");
  content::RenderFrameHost* guest_rfh = GetGuestRenderFrameHost();
  ASSERT_TRUE(guest_rfh);

  content::ContextMenuParams params;
  TestRenderViewContextMenu menu(*guest_rfh, params);
  menu.Init();

  // Expect "Inspect" to be shown as we are running webview in a chrome app.
  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class WebViewSettingsRevampTest : public WebViewTest,
                                  public testing::WithParamInterface<bool> {
 public:
  WebViewSettingsRevampTest() {
    scoped_feature_list_.InitWithFeatureState(
        ash::features::kOsSettingsRevampWayfinding,
        /*enabled=*/GetParam());
  }
  ~WebViewSettingsRevampTest() override = default;

  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "OsSettingsRevampWayfindingEnabled"
                      : "OsSettingsRevampWayfindingDisabled";
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(WebViewTests,
                         WebViewSettingsRevampTest,
                         testing::Bool(),
                         WebViewSettingsRevampTest::DescribeParams);
#endif

// This test executes the context menu command 'LanguageSettings'.
// On Ash, this will open the language settings in the OS Settings app.
// Elsewhere, it will load chrome://settings/languages in a browser window.
// In either case, this is a browser-initiated operation and so we expect it
// to succeed if the embedder is allowed to perform the operation.
#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(WebViewSettingsRevampTest, ContextMenuLanguageSettings) {
#else
IN_PROC_BROWSER_TEST_F(WebViewTest, ContextMenuLanguageSettings) {
#endif
  LoadAppWithGuest("web_view/context_menus/basic");
  content::WebContents* embedder = GetEmbedderWebContents();
  ASSERT_TRUE(embedder);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::SystemWebAppManager::Get(browser()->profile())
      ->InstallSystemAppsForTesting();
#endif

  content::WebContentsAddedObserver web_contents_added_observer;

  GURL page_url("http://www.google.com");
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetGuestRenderFrameHost(), page_url));
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS, 0);

  // Verify that a new WebContents has been created that is at the appropriate
  // Language Settings page.
  content::WebContents* new_contents =
      web_contents_added_observer.GetWebContents();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(
      GURL(chrome::kChromeUIOSSettingsURL)
          .Resolve(
              ash::features::IsOsSettingsRevampWayfindingEnabled()
                  ? chromeos::settings::mojom::kLanguagesSubpagePath
                  : chromeos::settings::mojom::kLanguagesAndInputSectionPath),
      new_contents->GetVisibleURL());
#else
  EXPECT_EQ(GURL(chrome::kChromeUISettingsURL)
                .Resolve(chrome::kLanguageOptionsSubPage),
            new_contents->GetVisibleURL());
#endif
}

IN_PROC_BROWSER_TEST_F(WebViewTest, ContextMenusAPI_Basic) {
  LoadAppWithGuest("web_view/context_menus/basic");

  content::WebContents* embedder = GetEmbedderWebContents();
  ASSERT_TRUE(embedder);

  // 1. Basic property test.
  ExecuteScriptWaitForTitle(embedder, "checkProperties()", "ITEM_CHECKED");

  // 2. Create a menu item and wait for created callback to be called.
  ExecuteScriptWaitForTitle(embedder, "createMenuItem()", "ITEM_CREATED");

  // 3. Click the created item, wait for the click handlers to fire from JS.
  ExtensionTestMessageListener click_listener("ITEM_CLICKED");
  GURL page_url("http://www.google.com");
  // Create and build our test context menu.
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(GetGuestRenderFrameHost(), page_url));
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
  auto* guest_main_frame =
      GetGuestViewManager()->GetLastGuestRenderFrameHostCreated();
  ASSERT_TRUE(guest_main_frame);
  content::WebContents* embedder = GetEmbedderWebContents();
  ASSERT_TRUE(embedder);

  // Add a preventDefault() call on context menu event so context menu
  // does not show up.
  ExtensionTestMessageListener prevent_default_listener(
      "WebViewTest.CONTEXT_MENU_DEFAULT_PREVENTED");
  EXPECT_TRUE(content::ExecJs(embedder, "registerPreventDefault()"));
  ContextMenuShownObserver context_menu_shown_observer;

  OpenContextMenu(guest_main_frame);

  EXPECT_TRUE(prevent_default_listener.WaitUntilSatisfied());
  // Expect the menu to not show up.
  EXPECT_EQ(false, context_menu_shown_observer.shown());

  // Now remove the preventDefault() and expect context menu to be shown.
  ExecuteScriptWaitForTitle(
      embedder, "removePreventDefault()", "PREVENT_DEFAULT_LISTENER_REMOVED");
  OpenContextMenu(guest_main_frame);

  // We expect to see a context menu for the second call to |OpenContextMenu|.
  context_menu_shown_observer.Wait();
  EXPECT_EQ(true, context_menu_shown_observer.shown());
}

// Tests that a context menu is created when right-clicking in the webview. This
// also tests that the 'contextmenu' event is handled correctly.
IN_PROC_BROWSER_TEST_F(WebViewTest, TestContextMenu) {
  LoadAppWithGuest("web_view/context_menus/basic");
  auto* guest_main_frame =
      GetGuestViewManager()->WaitForSingleGuestRenderFrameHostCreated();
  ASSERT_TRUE(guest_main_frame);

  auto close_menu_and_stop_run_loop = [](base::OnceClosure closure,
                                         RenderViewContextMenu* context_menu) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&RenderViewContextMenuBase::Cancel,
                                  base::Unretained(context_menu)));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(closure));
  };

  base::RunLoop run_loop;
  RenderViewContextMenu::RegisterMenuShownCallbackForTesting(
      base::BindOnce(close_menu_and_stop_run_loop, run_loop.QuitClosure()));

  OpenContextMenu(guest_main_frame);

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

  ExtensionTestMessageListener done_listener("TEST_PASSED");
  done_listener.set_failure_message("TEST_FAILED");
  EXPECT_TRUE(content::ExecJs(embedder_web_contents, "startCheckTest('')"));
  ASSERT_TRUE(done_listener.WaitUntilSatisfied());

  mock->WaitForCheckMediaPermission();
}

// Checks that window.screenX/screenY/screenLeft/screenTop works correctly for
// guests.
IN_PROC_BROWSER_TEST_F(WebViewTest, ScreenCoordinates) {
  ASSERT_TRUE(RunExtensionTest(
      "platform_apps/web_view/common",
      {.custom_arg = "screen_coordinates", .launch_as_platform_app = true}))
      << message_;
}

// TODO(crbug.com/40677344): This test leaks memory.
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
// Note that these are run separately because OverrideGeolocation() doesn't
// mock out geolocation for multiple navigator.geolocation calls properly and
// the tests become flaky.
//
// GeolocationAPI* test 1 of 3.
IN_PROC_BROWSER_TEST_F(WebViewTest, GeolocationAPIEmbedderHasAccessAllow) {
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
IN_PROC_BROWSER_TEST_F(WebViewTest,
                       GeolocationAPIEmbedderHasAccessMultipleBridgeIdAllow) {
  TestHelper("testMultipleBridgeIdAllow",
             "web_view/geolocation/embedder_has_permission", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       PermissionsAPIEmbedderHasAccessAllowGeolocation) {
  TestHelper("testAllowGeolocation",
             "web_view/permissions_test/embedder_has_permission",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       PermissionsAPIEmbedderHasAccessDenyGeolocation) {
  TestHelper("testDenyGeolocation",
             "web_view/permissions_test/embedder_has_permission",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       PermissionsAPIEmbedderHasAccessAllowCamera) {
  TestHelper("testAllowCamera",
             "web_view/permissions_test/embedder_has_permission",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, PermissionsAPIEmbedderHasAccessDenyCamera) {
  TestHelper("testDenyCamera",
             "web_view/permissions_test/embedder_has_permission",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       PermissionsAPIEmbedderHasAccessAllowMicrophone) {
  TestHelper("testAllowMicrophone",
             "web_view/permissions_test/embedder_has_permission",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       PermissionsAPIEmbedderHasAccessDenyMicrophone) {
  TestHelper("testDenyMicrophone",
             "web_view/permissions_test/embedder_has_permission",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, PermissionsAPIEmbedderHasAccessAllowMedia) {
  TestHelper("testAllowMedia",
             "web_view/permissions_test/embedder_has_permission",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, PermissionsAPIEmbedderHasAccessDenyMedia) {
  TestHelper("testDenyMedia",
             "web_view/permissions_test/embedder_has_permission",
             NEEDS_TEST_SERVER);
}

class MockHidDelegate : public ChromeHidDelegate {
 public:
  // Simulates opening the HID device chooser dialog and selecting an item. The
  // chooser automatically selects the device under index 0.
  void OnWebViewHidPermissionRequestCompleted(
      base::WeakPtr<HidChooser> chooser,
      content::GlobalRenderFrameHostId embedder_rfh_id,
      std::vector<blink::mojom::HidDeviceFilterPtr> filters,
      std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters,
      content::HidChooser::Callback callback,
      bool allow) override {
    if (!allow) {
      std::move(callback).Run(std::vector<device::mojom::HidDeviceInfoPtr>());
      return;
    }

    auto* render_frame_host = content::RenderFrameHost::FromID(embedder_rfh_id);
    ASSERT_TRUE(render_frame_host);

    chooser_controller_ = std::make_unique<HidChooserController>(
        render_frame_host, std::move(filters), std::move(exclusion_filters),
        std::move(callback));

    mock_chooser_view_ =
        std::make_unique<permissions::MockChooserControllerView>();
    chooser_controller_->set_view(mock_chooser_view_.get());

    EXPECT_CALL(*mock_chooser_view_.get(), OnOptionsInitialized)
        .WillOnce(
            testing::Invoke([this] { chooser_controller_->Select({0}); }));
  }

 private:
  std::unique_ptr<HidChooserController> chooser_controller_;
  std::unique_ptr<permissions::MockChooserControllerView> mock_chooser_view_;
};

class WebHidWebViewTest : public WebViewTest {
  class TestContentBrowserClient : public ChromeContentBrowserClient {
   public:
    // ContentBrowserClient:
    content::HidDelegate* GetHidDelegate() override { return &delegate_; }

   private:
    MockHidDelegate delegate_;
  };

 public:
  WebHidWebViewTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kEnableWebHidInWebView);
  }

  ~WebHidWebViewTest() override {
    content::SetBrowserClientForTesting(original_client_.get());
  }

  void SetUpOnMainThread() override {
    WebViewTest::SetUpOnMainThread();
    original_client_ = content::SetBrowserClientForTesting(&overriden_client_);
    BindHidManager();
    AddTestDevice();
  }

  void BindHidManager() {
    mojo::PendingRemote<device::mojom::HidManager> pending_remote;
    hid_manager_.Bind(pending_remote.InitWithNewPipeAndPassReceiver());
    base::test::TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>>
        devices_future;
    auto* chooser_context =
        HidChooserContextFactory::GetForProfile(browser()->profile());
    chooser_context->SetHidManagerForTesting(std::move(pending_remote),
                                             devices_future.GetCallback());
    EXPECT_TRUE(devices_future.Wait());
  }

  void AddTestDevice() {
    hid_manager_.CreateAndAddDevice("1", 0, 0, "Test HID Device", "",
                                    device::mojom::HidBusType::kHIDBusTypeUSB);
  }

 private:
  TestContentBrowserClient overriden_client_;
  raw_ptr<content::ContentBrowserClient> original_client_ = nullptr;
  device::FakeHidManager hid_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebHidWebViewTest,
                       PermissionsAPIEmbedderHasAccessAllowHid) {
  ExtensionTestMessageListener activation_provider(
      "performUserActivationInWebview");
  activation_provider.SetOnSatisfied(
      base::BindLambdaForTesting([&](const std::string&) {
        // Activate the web view frame by executing a no-op script.
        // This is needed because `requestDevice` method of HID API requires a
        // window to satisfy the user activation requirement.
        EXPECT_TRUE(content::ExecJs(
            GetGuestViewManager()->GetLastGuestRenderFrameHostCreated(),
            "// No-op script"));
      }));
  TestHelper("testAllowHid",
             "web_view/permissions_test/embedder_has_permission",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebHidWebViewTest,
                       PermissionsAPIEmbedderHasAccessDenyHid) {
  ExtensionTestMessageListener activation_provider(
      "performUserActivationInWebview");
  activation_provider.SetOnSatisfied(
      base::BindLambdaForTesting([&](const std::string&) {
        // Activate the web view frame by executing a no-op script.
        // This is needed because `requestDevice` method of HID API requires a
        // window to satisfy the user activation requirement.
        EXPECT_TRUE(content::ExecJs(
            GetGuestViewManager()->GetLastGuestRenderFrameHostCreated(),
            "// No-op script"));
      }));
  TestHelper("testDenyHid", "web_view/permissions_test/embedder_has_permission",
             NEEDS_TEST_SERVER);
}

// Tests that closing the app window before the HID request is answered will
// work correctly. This is meant to verify that no mojo callbacks will be
// dropped in such case.
IN_PROC_BROWSER_TEST_F(WebHidWebViewTest,
                       PermissionsAPIEmbedderHasAccessCloseWindowHid) {
  ExtensionTestMessageListener activation_provider(
      "performUserActivationInWebview");
  activation_provider.SetOnSatisfied(
      base::BindLambdaForTesting([&](const std::string&) {
        // Activate the web view frame by executing a no-op script.
        // This is needed because `requestDevice` method of HID API requires a
        // window to satisfy the user activation requirement.
        EXPECT_TRUE(content::ExecJs(
            GetGuestViewManager()->GetLastGuestRenderFrameHostCreated(),
            "// No-op script"));
      }));
  TestHelper("testHidCloseWindow",
             "web_view/permissions_test/embedder_has_permission",
             NEEDS_TEST_SERVER);
  extensions::AppWindow* window = GetFirstAppWindow();
  CloseAppWindow(window);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       PermissionsAPIEmbedderHasNoAccessAllowGeolocation) {
  TestHelper("testAllowGeolocation",
             "web_view/permissions_test/embedder_has_no_permission",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       PermissionsAPIEmbedderHasNoAccessDenyGeolocation) {
  TestHelper("testDenyGeolocation",
             "web_view/permissions_test/embedder_has_no_permission",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       PermissionsAPIEmbedderHasNoAccessAllowCamera) {
  TestHelper("testAllowCamera",
             "web_view/permissions_test/embedder_has_no_permission",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       PermissionsAPIEmbedderHasNoAccessDenyCamera) {
  TestHelper("testDenyCamera",
             "web_view/permissions_test/embedder_has_no_permission",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       PermissionsAPIEmbedderHasNoAccessAllowMicrophone) {
  TestHelper("testAllowMicrophone",
             "web_view/permissions_test/embedder_has_no_permission",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       PermissionsAPIEmbedderHasNoAccessDenyMicrophone) {
  TestHelper("testDenyMicrophone",
             "web_view/permissions_test/embedder_has_no_permission",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       PermissionsAPIEmbedderHasNoAccessAllowMedia) {
  TestHelper("testAllowMedia",
             "web_view/permissions_test/embedder_has_no_permission",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       PermissionsAPIEmbedderHasNoAccessDenyMedia) {
  TestHelper("testDenyMedia",
             "web_view/permissions_test/embedder_has_no_permission",
             NEEDS_TEST_SERVER);
}

// Tests that
// BrowserPluginGeolocationPermissionContext::CancelGeolocationPermissionRequest
// is handled correctly (and does not crash).
IN_PROC_BROWSER_TEST_F(WebViewTest, GeolocationAPICancelGeolocation) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(
      RunExtensionTest("platform_apps/web_view/geolocation/cancel_request",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, DISABLED_GeolocationRequestGone) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunExtensionTest(
      "platform_apps/web_view/geolocation/geolocation_request_gone",
      {.launch_as_platform_app = true}))
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
  ASSERT_TRUE(RunExtensionTest(
      "platform_apps/web_view/common",
      {.custom_arg = "cleardata", .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, ClearSessionCookies) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunExtensionTest(
      "platform_apps/web_view/common",
      {.custom_arg = "cleardata_session", .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, ClearPersistentCookies) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunExtensionTest(
      "platform_apps/web_view/common",
      {.custom_arg = "cleardata_persistent", .launch_as_platform_app = true}))
      << message_;
}

// Regression test for https://crbug.com/615429.
IN_PROC_BROWSER_TEST_F(WebViewTest, ClearDataTwice) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunExtensionTest(
      "platform_apps/web_view/common",
      {.custom_arg = "cleardata_twice", .launch_as_platform_app = true}))
      << message_;
}

#if BUILDFLAG(IS_WIN)
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
  ASSERT_TRUE(RunExtensionTest(
      "platform_apps/web_view/common",
      {.custom_arg = "console_messages", .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, DownloadPermission) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  LoadAndLaunchPlatformApp("web_view/download", "guest-loaded");
  auto* guest_view_base =
      GetGuestViewManager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view_base);

  auto* guest_render_frame_host = guest_view_base->GetGuestMainFrame();
  std::unique_ptr<content::DownloadTestObserver> completion_observer(
      new content::DownloadTestObserverTerminal(
          guest_render_frame_host->GetBrowserContext()->GetDownloadManager(), 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));

  // Replace WebContentsDelegate with mock version so we can intercept download
  // requests.
  std::unique_ptr<MockDownloadWebContentsDelegate> mock_delegate(
      new MockDownloadWebContentsDelegate(guest_view_base));
  guest_view_base->web_contents()->SetDelegate(mock_delegate.get());

  // Start test.
  // 1. Guest requests a download that its embedder denies.
  EXPECT_TRUE(content::ExecJs(guest_render_frame_host,
                              "startDownload('download-link-1')"));
  mock_delegate->WaitForCanDownload(false);  // Expect to not allow.
  mock_delegate->Reset();

  // 2. Guest requests a download that its embedder allows.
  EXPECT_TRUE(content::ExecJs(guest_render_frame_host,
                              "startDownload('download-link-2')"));
  mock_delegate->WaitForCanDownload(true);  // Expect to allow.
  mock_delegate->Reset();

  // 3. Guest requests a download that its embedder ignores, this implies deny.
  EXPECT_TRUE(content::ExecJs(guest_render_frame_host,
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
    return nullptr;
  }

  std::string cookie_to_expect = request.GetURL().query();
  const auto cookie_header_it = request.headers.find("cookie");
  std::unique_ptr<net::test_server::BasicHttpResponse> response;

  // Return a 403 if there's no cookie or if the cookie doesn't match.
  if (cookie_header_it == request.headers.end() ||
      cookie_header_it->second != cookie_to_expect) {
    response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_FORBIDDEN);
    response->set_content_type("text/plain");
    response->set_content("Forbidden");
    return std::move(response);
  }

  // We have a cookie. Send some content along with the next status code.
  response = std::make_unique<net::test_server::BasicHttpResponse>();
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
    if (initialized_ || download_manager_->IsManagerInitialized())
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
  base::OnceClosure quit_closure_;
  bool initialized_;
  raw_ptr<content::DownloadManager> download_manager_;
};

}  // namespace

// Downloads initiated from isolated guest parititons should use their
// respective cookie stores. In addition, if those downloads are resumed, they
// should continue to use their respective cookie stores.
IN_PROC_BROWSER_TEST_F(WebViewTest, DownloadCookieIsolation) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleDownloadRequestWithCookie));
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  LoadAndLaunchPlatformApp("web_view/download_cookie_isolation",
                           "created-webviews");

  content::WebContents* web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(web_contents);

  content::DownloadManager* download_manager =
      web_contents->GetBrowserContext()->GetDownloadManager();

  scoped_refptr<content::TestFileErrorInjector> error_injector(
      content::TestFileErrorInjector::Create(download_manager));

  content::TestFileErrorInjector::FileErrorInfo error_info(
      content::TestFileErrorInjector::FILE_OPERATION_STREAM_COMPLETE, 0,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED);
  error_info.stream_offset = 0;
  error_injector->InjectError(error_info);

  auto download_op = [&](std::string cookie) {
    // DownloadTestObserverInterrupted does not seem to reliably wait for
    // multiple failed downloads, so we perform one download at a time.
    content::DownloadTestObserverInterrupted interrupted_observer(
        download_manager, 1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
    EXPECT_TRUE(content::ExecJs(
        web_contents,
        base::StrCat(
            {"startDownload('", cookie, "', '",
             embedded_test_server()->GetURL(kDownloadPathPrefix).spec(),
             "?cookie=", cookie, "')"})));

    // This maps to DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED.
    interrupted_observer.WaitForFinished();
  };

  // Both downloads should fail due to the error that was injected above to the
  // download manager.
  download_op("first");

  // Note that the second webview uses an in-memory partition.
  download_op("second");

  error_injector->ClearError();

  content::DownloadManager::DownloadVector downloads;
  download_manager->GetAllDownloads(&downloads);
  ASSERT_EQ(2u, downloads.size());

  CloseAppWindow(GetFirstAppWindow());

  std::unique_ptr<content::DownloadTestObserver> completion_observer(
      new content::DownloadTestObserverTerminal(
          download_manager, 2,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));

  for (download::DownloadItem* download : downloads) {
    ASSERT_TRUE(download->CanResume());
    EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED,
              download->GetLastReason());
    download->Resume(false);
  }

  completion_observer->WaitForFinished();

  base::ScopedAllowBlockingForTesting allow_blocking;
  std::set<std::string> cookies;
  for (download::DownloadItem* download : downloads) {
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
      web_contents->GetBrowserContext()->GetDownloadManager();

  scoped_refptr<content::TestFileErrorInjector> error_injector(
      content::TestFileErrorInjector::Create(download_manager));

  content::TestFileErrorInjector::FileErrorInfo error_info(
      content::TestFileErrorInjector::FILE_OPERATION_STREAM_COMPLETE, 0,
      download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED);
  error_info.stream_offset = 0;
  error_injector->InjectError(error_info);

  auto download_op = [&](std::string cookie) {
    // DownloadTestObserverInterrupted does not seem to reliably wait for
    // multiple failed downloads, so we perform one download at a time.
    content::DownloadTestObserverInterrupted interrupted_observer(
        download_manager, 1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

    EXPECT_TRUE(content::ExecJs(
        web_contents,
        base::StrCat(
            {"startDownload('", cookie, "', '",
             embedded_test_server()->GetURL(kDownloadPathPrefix).spec(),
             "?cookie=", cookie, "')"})));

    // This maps to DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED.
    interrupted_observer.WaitForFinished();
  };

  // Both downloads should fail due to the error that was injected above to the
  // download manager.
  download_op("first");

  // Note that the second webview uses an in-memory partition.
  download_op("second");

  // Wait for both downloads to be stored.
  task_runner->FastForwardUntilNoTasksRemain();

  content::EnsureCookiesFlushed(profile());
}

IN_PROC_BROWSER_TEST_F(WebViewTest, DownloadCookieIsolation_CrossSession) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleDownloadRequestWithCookie));
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner(
      new base::TestMockTimeTaskRunner);
  download::SetDownloadDBTaskRunnerForTesting(task_runner);

  content::BrowserContext* browser_context = profile();
  content::DownloadManager* download_manager =
      browser_context->GetDownloadManager();

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
  for (download::DownloadItem* download : saved_downloads) {
    const std::string port_string =
        base::NumberToString(embedded_test_server()->port());
    GURL::Replacements replacements;
    replacements.SetPortStr(port_string);
    std::vector<GURL> url_chain;
    url_chain.push_back(download->GetURL().ReplaceComponents(replacements));

    downloads.push_back(download_manager->CreateDownloadItem(
        base::Uuid::GenerateRandomV4().AsLowercaseString(),
        download->GetId() + 2, download->GetFullPath(),
        download->GetTargetFilePath(), url_chain, download->GetReferrerUrl(),
        download_manager
            ->SerializedEmbedderDownloadDataToStoragePartitionConfig(
                download->GetSerializedEmbedderDownloadData()),
        download->GetTabUrl(), download->GetTabReferrerUrl(),
        download->GetRequestInitiator(), download->GetMimeType(),
        download->GetOriginalMimeType(), download->GetStartTime(),
        download->GetEndTime(), download->GetETag(),
        download->GetLastModifiedTime(), download->GetReceivedBytes(),
        download->GetTotalBytes(), download->GetHash(), download->GetState(),
        download->GetDangerType(), download->GetLastReason(),
        download->GetOpened(), download->GetLastAccessTime(),
        download->IsTransient(), download->GetReceivedSlices()));
  }

  content::DownloadTestObserverTerminal completion_observer(
      download_manager, 2,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

  for (download::DownloadItem* download : downloads) {
    ASSERT_TRUE(download->CanResume());
    ASSERT_TRUE(download->GetFullPath().empty());
    ASSERT_TRUE(download::DOWNLOAD_INTERRUPT_REASON_CRASH ==
                    download->GetLastReason() ||
                download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED ==
                    download->GetLastReason());
    download->Resume(true);
  }

  completion_observer.WaitForFinished();

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
      content::EvalJs(embedder_web_contents->GetPrimaryMainFrame(),
                      "window.guestProcessId")
          .ExtractInt();
  int guest_render_frame_routing_id =
      content::EvalJs(embedder_web_contents->GetPrimaryMainFrame(),
                      "window.guestRenderFrameRoutingId")
          .ExtractInt();

  // Verify that the guest related info (guest_process_id and
  // guest_render_frame_routing_id) actually points to a WebViewGuest.
  ASSERT_TRUE(extensions::WebViewGuest::FromRenderFrameHostId(
      content::GlobalRenderFrameHostId(guest_process_id,
                                       guest_render_frame_routing_id)));
}

IN_PROC_BROWSER_TEST_F(WebViewTest, SetPropertyOnDocumentReady) {
  ASSERT_TRUE(RunExtensionTest("platform_apps/web_view/document_ready",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, SetPropertyOnDocumentInteractive) {
  ASSERT_TRUE(RunExtensionTest("platform_apps/web_view/document_interactive",
                               {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewSpeechAPITest,
                       SpeechRecognitionAPI_HasPermissionAllow) {
  ASSERT_TRUE(RunExtensionTest(
      "platform_apps/web_view/speech_recognition_api",
      {.custom_arg = "allowTest", .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewSpeechAPITest,
                       SpeechRecognitionAPI_HasPermissionDeny) {
  ASSERT_TRUE(RunExtensionTest(
      "platform_apps/web_view/speech_recognition_api",
      {.custom_arg = "denyTest", .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewSpeechAPITest,
                       SpeechRecognitionAPI_NoPermission) {
  ASSERT_TRUE(
      RunExtensionTest("platform_apps/web_view/common",
                       {.custom_arg = "speech_recognition_api_no_permission",
                        .launch_as_platform_app = true}))
      << message_;
}

// Tests overriding user agent.
IN_PROC_BROWSER_TEST_F(WebViewTest, UserAgent) {
  ASSERT_TRUE(RunExtensionTest(
      "platform_apps/web_view/common",
      {.custom_arg = "useragent", .launch_as_platform_app = true}))
      << message_;
}

// TODO(crbug.com/40260430): Test is flaky.
#if BUILDFLAG(IS_MAC)
#define MAYBE_UserAgent_NewWindow DISABLED_UserAgent_NewWindow
#else
#define MAYBE_UserAgent_NewWindow UserAgent_NewWindow
#endif
IN_PROC_BROWSER_TEST_F(WebViewNewWindowTest, MAYBE_UserAgent_NewWindow) {
  ASSERT_TRUE(RunExtensionTest(
      "platform_apps/web_view/common",
      {.custom_arg = "useragent_newwindow", .launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, NoPermission) {
  ASSERT_TRUE(RunExtensionTest("platform_apps/web_view/nopermission",
                               {.launch_as_platform_app = true}))
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

IN_PROC_BROWSER_TEST_F(WebViewTest, Dialog_TestConfirmDialogDefaultGCCancel) {
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

class WebViewCaptureTest : public WebViewTest {
 public:
  WebViewCaptureTest() {}
  ~WebViewCaptureTest() override {}
  void SetUp() override {
    EnablePixelOutput();
    WebViewTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestZoomAPI) {
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

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestFindAfterTerminate) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  TestHelper("testFindAfterTerminate", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestLoadDataAPI) {
  TestHelper("testLoadDataAPI", "web_view/shim", NEEDS_TEST_SERVER);

  // Ensure that the guest process is locked after the loadDataWithBaseURL
  // navigation and is allowed to access resources belonging to the base URL's
  // origin.
  content::RenderFrameHost* guest_main_frame =
      GetGuestViewManager()->WaitForSingleGuestRenderFrameHostCreated();
  ASSERT_TRUE(guest_main_frame);
  EXPECT_TRUE(guest_main_frame->GetSiteInstance()->RequiresDedicatedProcess());
  EXPECT_TRUE(
      guest_main_frame->GetProcess()->IsProcessLockedToSiteForTesting());

  auto* security_policy = content::ChildProcessSecurityPolicy::GetInstance();
  url::Origin base_origin =
      url::Origin::Create(embedded_test_server()->GetURL("localhost", "/"));
  EXPECT_TRUE(security_policy->CanAccessDataForOrigin(
      guest_main_frame->GetProcess()->GetID(), base_origin));

  // Ensure the process doesn't have access to some other origin. This
  // verifies that site isolation is enforced.
  url::Origin another_origin =
      url::Origin::Create(embedded_test_server()->GetURL("foo.com", "/"));
  EXPECT_FALSE(security_policy->CanAccessDataForOrigin(
      guest_main_frame->GetProcess()->GetID(), another_origin));
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestLoadDataAPIAccessibleResources) {
  TestHelper("testLoadDataAPIAccessibleResources", "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, LoadDataAPINotRelativeToAnotherExtension) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const extensions::Extension* other_extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_file"));
  LoadAppWithGuest("web_view/simple");
  content::WebContents* embedder = GetEmbedderWebContents();
  content::RenderFrameHost* guest = GetGuestView()->GetGuestMainFrame();

  content::TestFrameNavigationObserver fail_if_webview_navigates(guest);
  ASSERT_TRUE(content::ExecJs(
      embedder, content::JsReplace(
                    "var webview = document.querySelector('webview'); "
                    "webview.loadDataWithBaseUrl('data:text/html,hello', $1);",
                    other_extension->url())));

  // We expect the call to loadDataWithBaseUrl to fail and not cause a
  // navigation. Since loadDataWithBaseUrl doesn't notify when it fails, we
  // resort to a timeout here. If |fail_if_webview_navigates| doesn't see a
  // navigation in that time, we consider the test to have passed.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();
  EXPECT_FALSE(fail_if_webview_navigates.navigation_started());
}

// This test verifies that the resize and contentResize events work correctly.
IN_PROC_BROWSER_TEST_F(WebViewSizeTest, Shim_TestResizeEvents) {
  TestHelper("testResizeEvents", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestPerOriginZoomMode) {
  TestHelper("testPerOriginZoomMode", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestPerViewZoomMode) {
  TestHelper("testPerViewZoomMode", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestDisabledZoomMode) {
  TestHelper("testDisabledZoomMode", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestZoomBeforeNavigation) {
  TestHelper("testZoomBeforeNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, HttpAuth) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  LoadAppWithGuest("web_view/simple");

  const GURL auth_url = embedded_test_server()->GetURL("/auth-basic");
  // There are two navigations occurring here. The first fails due to the need
  // for auth. After it's supplied, a second navigation will succeed.
  content::TestNavigationObserver nav_observer(GetGuestWebContents(), 2);
  nav_observer.set_wait_event(
      content::TestNavigationObserver::WaitEvent::kNavigationFinished);

  EXPECT_TRUE(
      content::ExecJs(GetGuestRenderFrameHost(),
                      content::JsReplace("location.href = $1;", auth_url)));
  ASSERT_TRUE(base::test::RunUntil(
      []() { return LoginHandler::GetAllLoginHandlersForTest().size() == 1; }));

  LoginHandler* login_handler =
      LoginHandler::GetAllLoginHandlersForTest().front();
  login_handler->SetAuth(u"basicuser", u"secret");
  nav_observer.WaitForNavigationFinished();
}

IN_PROC_BROWSER_TEST_F(WebViewTest, HttpAuthIdentical) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  LoadAppWithGuest("web_view/simple");

  const GURL auth_url = embedded_test_server()->GetURL("/auth-basic");
  content::NavigationController* tab_controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  // There are two navigations occurring here. The first fails due to the need
  // for auth. After it's supplied, a second navigation will succeed.
  content::TestNavigationObserver guest_nav_observer(GetGuestWebContents(), 2);
  guest_nav_observer.set_wait_event(
      content::TestNavigationObserver::WaitEvent::kNavigationFinished);

  EXPECT_TRUE(
      content::ExecJs(GetGuestRenderFrameHost(),
                      content::JsReplace("location.href = $1;", auth_url)));
  ASSERT_TRUE(base::test::RunUntil(
      []() { return LoginHandler::GetAllLoginHandlersForTest().size() == 1; }));

  // While the login UI is showing for the app, navigate a tab to the same URL
  // requiring auth.
  tab_controller->LoadURL(auth_url, content::Referrer(),
                          ui::PAGE_TRANSITION_TYPED, std::string());
  ASSERT_TRUE(base::test::RunUntil(
      []() { return LoginHandler::GetAllLoginHandlersForTest().size() == 2; }));

  // Both the guest and the tab should be prompting for credentials and the auth
  // challenge should be the same. Normally, the login code de-duplicates
  // identical challenges if multiple prompts are shown for them. However,
  // credentials can't be shared across StoragePartitions. So providing
  // credentials within the guest should not affect the tab.
  ASSERT_EQ(2u, LoginHandler::GetAllLoginHandlersForTest().size());
  LoginHandler* guest_login_handler =
      LoginHandler::GetAllLoginHandlersForTest().front();
  LoginHandler* tab_login_handler =
      LoginHandler::GetAllLoginHandlersForTest().back();
  EXPECT_EQ(tab_controller,
            &tab_login_handler->web_contents()->GetController());
  EXPECT_TRUE(guest_login_handler->auth_info().MatchesExceptPath(
      tab_login_handler->auth_info()));

  guest_login_handler->SetAuth(u"basicuser", u"secret");
  guest_nav_observer.WaitForNavigationFinished();

  // The tab should still be prompting for credentials.
  ASSERT_EQ(1u, LoginHandler::GetAllLoginHandlersForTest().size());
  EXPECT_EQ(tab_login_handler,
            LoginHandler::GetAllLoginHandlersForTest().front());
}

namespace {

class NullWebContentsDelegate : public content::WebContentsDelegate {
 public:
  NullWebContentsDelegate() = default;
  ~NullWebContentsDelegate() override = default;
};

// A stub ClientCertStore that returns a FakeClientCertIdentity.
class ClientCertStoreStub : public net::ClientCertStore {
 public:
  explicit ClientCertStoreStub(net::ClientCertIdentityList list)
      : list_(std::move(list)) {}

  ~ClientCertStoreStub() override = default;

  // net::ClientCertStore:
  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override {
    std::move(callback).Run(std::move(list_));
    if (quit_closure_) {
      // Call the quit closure asynchronously, so it's ordered after the cert
      // selector.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(quit_closure_));
    }
  }

  static void SetQuitClosure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

 private:
  net::ClientCertIdentityList list_;

  // Called the next time GetClientCerts is called.
  static base::OnceClosure quit_closure_;
};

// static
base::OnceClosure ClientCertStoreStub::quit_closure_;

}  // namespace

class WebViewCertificateSelectorTest : public WebViewTest {
 public:
  void SetUpOnMainThread() override {
    WebViewTest::SetUpOnMainThread();

    ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
        ->set_client_cert_store_factory_for_testing(base::BindRepeating(
            &WebViewCertificateSelectorTest::CreateCertStore));

    net::SSLServerConfig ssl_config;
    ssl_config.client_cert_type =
        net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  net::EmbeddedTestServer& https_server() { return https_server_; }

  web_modal::WebContentsModalDialogManager* GetModalDialogManager(
      content::WebContents* embedder_web_contents) {
    web_modal::WebContentsModalDialogManager* manager =
        web_modal::WebContentsModalDialogManager::FromWebContents(
            embedder_web_contents);
    EXPECT_TRUE(manager);
    return manager;
  }

 private:
  static std::unique_ptr<net::ClientCertStore> CreateCertStore() {
    net::ClientCertIdentityList cert_identity_list;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;

      std::unique_ptr<net::FakeClientCertIdentity> cert_identity =
          net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
              net::GetTestCertsDirectory(), "client_1.pem", "client_1.pk8");
      EXPECT_TRUE(cert_identity.get());
      if (cert_identity)
        cert_identity_list.push_back(std::move(cert_identity));
    }

    return std::make_unique<ClientCertStoreStub>(std::move(cert_identity_list));
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

// Ensure a guest triggering a client certificate dialog does not crash.
IN_PROC_BROWSER_TEST_F(WebViewCertificateSelectorTest,
                       CertificateSelectorForGuest) {
  LoadAppWithGuest("web_view/simple");
  content::RenderFrameHost* guest_rfh = GetGuestRenderFrameHost();

  const GURL client_cert_url =
      https_server().GetURL("/ssl/browser_use_client_cert_store.html");

  base::RunLoop run_loop;
  ClientCertStoreStub::SetQuitClosure(run_loop.QuitClosure());
  EXPECT_TRUE(content::ExecJs(
      guest_rfh, content::JsReplace("location.href = $1;", client_cert_url)));
  run_loop.Run();

  auto* manager = GetModalDialogManager(GetEmbedderWebContents());
  EXPECT_TRUE(manager->IsDialogActive());
  manager->CloseAllDialogs();
}

// Ensure a guest triggering a client certificate dialog does not crash.
// This considers the case where a guest view is in use that has been
// inadvertently broken by misuse of WebContentsDelegates. This has seemingly
// happened multiple times for various dialogs and signin flows (see
// https://crbug.com/1076696 and https://crbug.com/1306988 ), so let's test that
// if we are in this situation, we at least don't crash.
IN_PROC_BROWSER_TEST_F(WebViewCertificateSelectorTest,
                       CertificateSelectorForGuestMisconfigured) {
  LoadAppWithGuest("web_view/simple");
  content::WebContents* guest = GetGuestWebContents();

  const GURL client_cert_url =
      https_server().GetURL("/ssl/browser_use_client_cert_store.html");

  auto* guest_delegate = guest->GetDelegate();
  NullWebContentsDelegate null_delegate;
  // This is intentionally incorrect. The guest WebContents' delegate should
  // remain a guest_view::GuestViewBase.
  guest->SetDelegate(&null_delegate);

  base::RunLoop run_loop;
  ClientCertStoreStub::SetQuitClosure(run_loop.QuitClosure());
  EXPECT_TRUE(content::ExecJs(
      guest, content::JsReplace("location.href = $1;", client_cert_url)));
  run_loop.Run();

  auto* manager = GetModalDialogManager(GetEmbedderWebContents());
  EXPECT_TRUE(manager->IsDialogActive());
  manager->CloseAllDialogs();

  guest->SetDelegate(guest_delegate);
}

// Test fixture to run the test on multiple channels.
class WebViewChannelTest
    : public WebViewTest,
      public testing::WithParamInterface<version_info::Channel> {
 public:
  WebViewChannelTest() : channel_(GetChannelParam()) {}

  version_info::Channel GetChannelParam() { return GetParam(); }

  WebViewChannelTest(const WebViewChannelTest&) = delete;
  WebViewChannelTest& operator=(const WebViewChannelTest&) = delete;

  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param == version_info::Channel::STABLE ? "StableChannel"
                                                       : "NonStableChannel";
  }

 private:
  extensions::ScopedCurrentChannel channel_;
};

// This test verify that the set of rules registries of a webview will be
// removed from RulesRegistryService after the webview is gone.
// TODO(crbug.com/40231831): The test has the same callstack caused by the race
// with ScopedFeatureList as the issue describes.
#if BUILDFLAG(IS_MAC)
#define MAYBE_Shim_TestRulesRegistryIDAreRemovedAfterWebViewIsGone \
  DISABLED_Shim_TestRulesRegistryIDAreRemovedAfterWebViewIsGone
#else
#define MAYBE_Shim_TestRulesRegistryIDAreRemovedAfterWebViewIsGone \
  Shim_TestRulesRegistryIDAreRemovedAfterWebViewIsGone
#endif
IN_PROC_BROWSER_TEST_P(
    WebViewChannelTest,
    MAYBE_Shim_TestRulesRegistryIDAreRemovedAfterWebViewIsGone) {
  ASSERT_EQ(extensions::GetCurrentChannel(), GetChannelParam());
  SCOPED_TRACE(base::StrCat(
      {"Testing Channel ", version_info::GetChannelString(GetChannelParam())}));

  LoadAppWithGuest("web_view/rules_registry");

  content::WebContents* embedder_web_contents = GetEmbedderWebContents();
  ASSERT_TRUE(embedder_web_contents);
  std::unique_ptr<EmbedderWebContentsObserver> observer(
      new EmbedderWebContentsObserver(embedder_web_contents));

  guest_view::GuestViewBase* guest_view = GetGuestView();
  ASSERT_TRUE(guest_view);

  // Register rule for the guest.
  Profile* profile = browser()->profile();
  int rules_registry_id =
      extensions::WebViewGuest::GetOrGenerateRulesRegistryID(
          guest_view->owner_rfh()->GetProcess()->GetID(),
          guest_view->view_instance_id());

  extensions::RulesRegistryService* registry_service =
      extensions::RulesRegistryService::Get(profile);
  extensions::TestRulesRegistry* rules_registry =
      new extensions::TestRulesRegistry("ui", rules_registry_id);
  registry_service->RegisterRulesRegistry(base::WrapRefCounted(rules_registry));

  EXPECT_TRUE(
      registry_service->HasRulesRegistryForTesting(rules_registry_id, "ui"));

  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  // Kill the embedder's render process, so the webview will go as well.
  embedder_web_contents->GetPrimaryMainFrame()
      ->GetProcess()
      ->GetProcess()
      .Terminate(0, false);
  observer->WaitForEmbedderRenderProcessTerminate();

  EXPECT_FALSE(
      registry_service->HasRulesRegistryForTesting(rules_registry_id, "ui"));
}

IN_PROC_BROWSER_TEST_P(WebViewChannelTest,
                       Shim_WebViewWebRequestRegistryHasNoPersistentCache) {
  ASSERT_EQ(extensions::GetCurrentChannel(), GetChannelParam());
  SCOPED_TRACE(base::StrCat(
      {"Testing Channel ", version_info::GetChannelString(GetChannelParam())}));

  LoadAppWithGuest("web_view/rules_registry");

  guest_view::GuestViewBase* guest_view = GetGuestView();
  ASSERT_TRUE(guest_view);

  Profile* profile = browser()->profile();
  extensions::RulesRegistryService* registry_service =
      extensions::RulesRegistryService::Get(profile);
  int rules_registry_id =
      extensions::WebViewGuest::GetOrGenerateRulesRegistryID(
          guest_view->owner_rfh()->GetProcess()->GetID(),
          guest_view->view_instance_id());

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

INSTANTIATE_TEST_SUITE_P(WebViewTests,
                         WebViewChannelTest,
                         testing::Values(version_info::Channel::UNKNOWN,
                                         version_info::Channel::STABLE),
                         WebViewChannelTest::DescribeParams);

// This test verifies that webview.contentWindow works inside an iframe.
IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestWebViewInsideFrame) {
  LoadAppWithGuest("web_view/inside_iframe");
}

// <webview> screenshot capture fails with ubercomp.
// See http://crbug.com/327035.
IN_PROC_BROWSER_TEST_F(WebViewCaptureTest, DISABLED_Shim_ScreenshotCapture) {
  TestHelper("testScreenshotCapture", "web_view/shim", NO_TEST_SERVER);
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

  ASSERT_TRUE(GetGuestView());
  extensions::WebViewGuest* guest =
      extensions::WebViewGuest::FromGuestViewBase(GetGuestView());
  ASSERT_TRUE(guest->allow_transparency());
  ASSERT_TRUE(guest->allow_scaling());
}

IN_PROC_BROWSER_TEST_F(WebViewTest, BasicPostMessage) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunExtensionTest("platform_apps/web_view/post_message/basic",
                               {.launch_as_platform_app = true}))
      << message_;
}

// Tests that webviews do get garbage collected.
IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestGarbageCollect) {
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

#if BUILDFLAG(ENABLE_PDF)
class WebViewPdfTest : public base::test::WithFeatureOverride,
                       public WebViewTest {
 public:
  WebViewPdfTest()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {}

  bool UseOopif() const { return GetParam(); }

  pdf::TestPdfViewerStreamManager* GetTestPdfViewerStreamManager(
      content::WebContents* contents) {
    return factory_.GetTestPdfViewerStreamManager(contents);
  }

  // Waits until the PDF has loaded in the given `web_view_rfh`.
  testing::AssertionResult WaitUntilPdfLoaded(
      content::RenderFrameHost* web_view_rfh) {
    if (UseOopif()) {
      auto* web_contents =
          content::WebContents::FromRenderFrameHost(web_view_rfh);
      return GetTestPdfViewerStreamManager(web_contents)
          ->WaitUntilPdfLoaded(web_view_rfh);
    }
    return pdf_extension_test_util::EnsurePDFHasLoaded(web_view_rfh);
  }

 private:
  pdf::TestPdfViewerStreamManagerFactory factory_;
};

// Test that the PDF viewer has the same bounds as the WebView.
IN_PROC_BROWSER_TEST_P(WebViewPdfTest, PdfContainerBounds) {
  TestHelper("testPDFInWebview", "web_view/shim", NO_TEST_SERVER);

  // OOPIF PDF should only have one guest for the WebView. GuestView PDF should
  // have a second guest for the PDF (`MimeHandlerViewGuest`).
  const size_t expected_guest_count = UseOopif() ? 1u : 2u;
  std::vector<content::RenderFrameHost*> guest_rfh_list;
  GetGuestViewManager()->WaitForNumGuestsCreated(expected_guest_count);
  GetGuestViewManager()->GetGuestRenderFrameHostList(&guest_rfh_list);
  ASSERT_EQ(expected_guest_count, guest_rfh_list.size());

  content::RenderFrameHost* web_view_rfh = guest_rfh_list[0];

  // Make sure the PDF loaded.
  ASSERT_TRUE(WaitUntilPdfLoaded(web_view_rfh));

  content::RenderFrameHost* extension_rfh =
      UseOopif() ? pdf_extension_test_util::GetPdfExtensionHostFromEmbedder(
                       web_view_rfh, /*allow_multiple_frames=*/false)
                 : guest_rfh_list[1];
  ASSERT_TRUE(extension_rfh);

  gfx::Rect web_view_container_bounds =
      web_view_rfh->GetRenderWidgetHost()->GetView()->GetViewBounds();
  gfx::Rect extension_container_bounds =
      extension_rfh->GetRenderWidgetHost()->GetView()->GetViewBounds();
  EXPECT_EQ(web_view_container_bounds.origin(),
            extension_container_bounds.origin());
}

// Test that context menu Back/Forward items in a WebView affect the embedder
// WebContents. See crbug.com/587355.
IN_PROC_BROWSER_TEST_P(WebViewPdfTest, ContextMenuNavigationInWebView) {
  TestHelper("testNavigateToPDFInWebview", "web_view/shim", NO_TEST_SERVER);

  // OOPIF PDF should only have one guest for the WebView. GuestView PDF should
  // have a second guest for the PDF (`MimeHandlerViewGuest`).
  const size_t expected_guest_count = UseOopif() ? 1u : 2u;
  std::vector<content::RenderFrameHost*> guest_rfh_list;
  GetGuestViewManager()->WaitForNumGuestsCreated(expected_guest_count);
  GetGuestViewManager()->GetGuestRenderFrameHostList(&guest_rfh_list);
  ASSERT_EQ(expected_guest_count, guest_rfh_list.size());

  content::RenderFrameHost* web_view_rfh = guest_rfh_list[0];

  // Make sure the PDF loaded.
  ASSERT_TRUE(WaitUntilPdfLoaded(web_view_rfh));

  content::RenderFrameHost* extension_rfh =
      UseOopif() ? pdf_extension_test_util::GetPdfExtensionHostFromEmbedder(
                       web_view_rfh, /*allow_multiple_frames=*/false)
                 : guest_rfh_list[1];
  ASSERT_TRUE(extension_rfh);

  // Ensure the <webview> has a previous entry, so we can navigate back to it.
  EXPECT_EQ(true,
            content::EvalJs(GetEmbedderWebContents(),
                            "document.querySelector('webview').canGoBack();"));

  // Open a context menu for the PDF viewer. Since the <webview> can
  // navigate back, the Back item should be enabled.
  content::ContextMenuParams params;
  TestRenderViewContextMenu menu(*extension_rfh, params);
  menu.Init();
  ASSERT_TRUE(menu.IsCommandIdEnabled(IDC_BACK));

  // Verify that the Back item causes the <webview> to navigate back to the
  // previous entry.
  content::TestFrameNavigationObserver observer(web_view_rfh);
  menu.ExecuteCommand(IDC_BACK, 0);
  observer.Wait();

  std::vector<content::RenderFrameHost*> guest_rfh_list2;
  GetGuestViewManager()->GetGuestRenderFrameHostList(&guest_rfh_list2);
  ASSERT_EQ(1u, guest_rfh_list2.size());

  content::RenderFrameHost* web_view_rfh2 = guest_rfh_list2[0];
  EXPECT_EQ(GURL(url::kAboutBlankURL), web_view_rfh2->GetLastCommittedURL());

  // The following should not crash. See https://crbug.com/331796663
  GetFirstAppWindowWebContents()->GetFocusedFrame();
}

IN_PROC_BROWSER_TEST_P(WebViewPdfTest, Shim_TestDialogInPdf) {
  TestHelper("testDialogInPdf", "web_view/shim", NO_TEST_SERVER);
}

// TODO(crbug.com/40268279): Stop testing both modes after OOPIF PDF viewer
// launches.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(WebViewPdfTest);
#endif  // BUILDFLAG(ENABLE_PDF)

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

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestRemoveBeforeAttach) {
  TestHelper("testRemoveBeforeAttach", "web_view/shim", NO_TEST_SERVER);

  // Ensure browser side state for the immediately destroyed guest is cleared
  // without having to wait for the embedder to be closed.
  // If it's not cleared then this will timeout.
  GetGuestViewManager()->WaitForAllGuestsDeleted();
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

  // Ensure that the <webview> process isn't considered an extension process,
  // even though the last committed URL is an extension URL.
  content::RenderFrameHost* guest =
      GetGuestViewManager()->GetLastGuestRenderFrameHostCreated();
  GURL guest_url(guest->GetLastCommittedURL());
  EXPECT_TRUE(guest_url.SchemeIs(extensions::kExtensionScheme));

  auto* process_map = extensions::ProcessMap::Get(guest->GetBrowserContext());
  auto* guest_process = guest->GetProcess();
  EXPECT_FALSE(process_map->Contains(guest_process->GetID()));
  EXPECT_FALSE(process_map->GetExtensionIdForProcess(guest_process->GetID()));

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(guest_url.host());
  EXPECT_EQ(extensions::mojom::ContextType::kUnprivilegedExtension,
            process_map->GetMostLikelyContextType(
                extension, guest_process->GetID(), &guest_url));
}

// Tests that a WebView can reload a WebView accessible resource. See
// https://crbug.com/691941.
IN_PROC_BROWSER_TEST_F(WebViewTest, ReloadWebviewAccessibleResource) {
  TestHelper("testReloadWebviewAccessibleResource",
             "web_view/load_webview_accessible_resource", NEEDS_TEST_SERVER);

  content::WebContents* embedder_contents = GetEmbedderWebContents();
  content::RenderFrameHost* web_view_frame =
      GetGuestViewManager()->GetLastGuestRenderFrameHostCreated();
  ASSERT_TRUE(embedder_contents);
  ASSERT_TRUE(web_view_frame);

  GURL embedder_url(embedder_contents->GetLastCommittedURL());
  GURL webview_url(embedder_url.DeprecatedGetOriginAsURL().spec() +
                   "assets/foo.html");

  EXPECT_EQ(webview_url, web_view_frame->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       CookiesEnabledAfterWebviewAccessibleResource) {
  TestHelper("testCookiesEnabledAfterWebviewAccessibleResource",
             "web_view/load_webview_accessible_resource", NEEDS_TEST_SERVER);
}

// Tests that webviews cannot embed accessible resources in iframes.
// https://crbug.com/1430991.
IN_PROC_BROWSER_TEST_F(WebViewTest, CannotIframeWebviewAccessibleResource) {
  TestHelper("testIframeWebviewAccessibleResource",
             "web_view/load_webview_accessible_resource", NEEDS_TEST_SERVER);

  content::RenderFrameHost* web_view_frame =
      GetGuestViewManager()->GetLastGuestRenderFrameHostCreated();
  ASSERT_TRUE(web_view_frame);
  content::RenderFrameHost* child_frame =
      content::ChildFrameAt(web_view_frame, 0);
  ASSERT_TRUE(child_frame);

  // The frame should never have committed to the extension resource.
  // The JS file verifies the load error.
  EXPECT_EQ(GURL(), child_frame->GetLastCommittedURL());
}

// Tests that webviews navigated to accessible resources can call certain
// extension APIs.
IN_PROC_BROWSER_TEST_F(WebViewTest,
                       CallingExtensionAPIsFromWebviewAccessibleResource) {
  TestHelper("testNavigateGuestToWebviewAccessibleResource",
             "web_view/load_webview_accessible_resource", NO_TEST_SERVER);

  content::WebContents* embedder_contents = GetEmbedderWebContents();
  content::RenderFrameHost* web_view_frame =
      GetGuestViewManager()->GetLastGuestRenderFrameHostCreated();
  ASSERT_TRUE(embedder_contents);
  ASSERT_TRUE(web_view_frame);

  // The embedder and the webview should be in separate site instances and
  // processes, even though they're for the same extension.
  EXPECT_NE(embedder_contents->GetPrimaryMainFrame()->GetProcess(),
            web_view_frame->GetProcess());
  EXPECT_NE(embedder_contents->GetPrimaryMainFrame()->GetSiteInstance(),
            web_view_frame->GetSiteInstance());

  GURL embedder_url(embedder_contents->GetLastCommittedURL());
  GURL accessible_resource_url =
      embedder_url.GetWithEmptyPath().Resolve("assets/foo.html");

  EXPECT_EQ(accessible_resource_url, web_view_frame->GetLastCommittedURL());

  // Try calling an extension API function. The extension frame, being embedded
  // in a webview, has fewer permissions that other extension contexts*. Try the
  // i18n.getAcceptLanguages() API. We choose this API because:
  // - It is exposed to the embedded frame.
  // - It is a "regular" extension API function that goes through the request /
  //   response flow in ExtensionFunctionDispatcher, unlike extension message
  //   APIs.
  // *TODO(crbug.com/40263329): The exact set of APIs and type of
  // context this is is a bit fuzzy. In practice, it's basically the same set
  // as is exposed to content scripts.
  static constexpr char kGetAcceptLanguages[] =
      R"(new Promise(resolve => {
           chrome.i18n.getAcceptLanguages((languages) => {
             let result = 'success';
             if (chrome.runtime.lastError) {
               result = 'Error: ' + chrome.runtime.lastError;
             } else if (!languages || !Array.isArray(languages) ||
                        !languages.includes('en')) {
               result = 'Invalid return result: ' + JSON.stringify(languages);
             }
             resolve(result);
           });
         });)";
  EXPECT_EQ("success", content::EvalJs(web_view_frame, kGetAcceptLanguages));

  // Finally, try accessing a privileged API, which shouldn't be available to
  // the embedded resource.
  static constexpr char kCallAppWindowCreate[] =
      R"(var message;
         if (chrome.app && chrome.app.window) {
           message = 'chrome.app.window unexpectedly available.';
         } else {
           message = 'success';
         }
         message;)";
  EXPECT_EQ("success", content::EvalJs(web_view_frame, kCallAppWindowCreate));
}

// Tests that a WebView can navigate an iframe to a blob URL that it creates
// while its main frame is at a WebView accessible resource.
IN_PROC_BROWSER_TEST_F(WebViewTest, BlobInWebviewAccessibleResource) {
  TestHelper("testBlobInWebviewAccessibleResource",
             "web_view/load_webview_accessible_resource", NEEDS_TEST_SERVER);

  content::WebContents* embedder_contents = GetEmbedderWebContents();
  content::RenderFrameHost* webview_rfh =
      GetGuestViewManager()->GetLastGuestRenderFrameHostCreated();
  ASSERT_TRUE(embedder_contents);
  ASSERT_TRUE(webview_rfh);

  GURL embedder_url(embedder_contents->GetLastCommittedURL());
  GURL webview_url(embedder_url.DeprecatedGetOriginAsURL().spec() +
                   "assets/foo.html");

  EXPECT_EQ(webview_url, webview_rfh->GetLastCommittedURL());

  content::RenderFrameHost* blob_frame = ChildFrameAt(webview_rfh, 0);
  EXPECT_TRUE(blob_frame->GetLastCommittedURL().SchemeIsBlob());

  EXPECT_EQ("Blob content",
            content::EvalJs(blob_frame, "document.body.innerText;"));
}

// Tests that a WebView cannot load a webview-inaccessible resource. See
// https://crbug.com/640072.
IN_PROC_BROWSER_TEST_F(WebViewTest, LoadWebviewInaccessibleResource) {
  TestHelper("testLoadWebviewInaccessibleResource",
             "web_view/load_webview_accessible_resource", NEEDS_TEST_SERVER);

  content::WebContents* embedder_contents = GetEmbedderWebContents();
  content::RenderFrameHost* web_view_frame =
      GetGuestViewManager()->GetLastGuestRenderFrameHostCreated();
  ASSERT_TRUE(embedder_contents);
  ASSERT_TRUE(web_view_frame);

  // Check that the webview stays at the first page that it loaded (foo.html),
  // and does not commit inaccessible.html.
  GURL embedder_url(embedder_contents->GetLastCommittedURL());
  GURL foo_url(embedder_url.DeprecatedGetOriginAsURL().spec() +
               "assets/foo.html");

  EXPECT_EQ(foo_url, web_view_frame->GetLastCommittedURL());
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
// TODO(crbug.com/40286295): Flaky on Win,Mac,ChromeOS,Linux.
IN_PROC_BROWSER_TEST_F(WebViewTest, DISABLED_ReloadAfterCrash) {
  // Load guest and wait for it to appear.
  LoadAppWithGuest("web_view/simple");

  EXPECT_TRUE(GetGuestView()->GetGuestMainFrame()->GetView());
  content::RenderFrameSubmissionObserver frame_observer(
      GetGuestView()->GetGuestMainFrame());
  frame_observer.WaitForMetadataChange();

  // Kill guest.
  auto* rph = GetGuestView()->GetGuestMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      rph, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(rph->Shutdown(content::RESULT_CODE_KILLED));
  crash_observer.Wait();
  EXPECT_FALSE(GetGuestView()->GetGuestMainFrame()->GetView());

  // Reload guest and make sure it appears.
  content::TestFrameNavigationObserver load_observer(
      GetGuestView()->GetGuestMainFrame());
  EXPECT_TRUE(ExecJs(GetEmbedderWebContents(),
                     "document.querySelector('webview').reload()"));
  load_observer.Wait();
  EXPECT_TRUE(GetGuestView()->GetGuestMainFrame()->GetView());
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

  ASSERT_TRUE(GetGuestViewManager()->GetLastGuestRenderFrameHostCreated());

  // Remove the iframe.
  content::ExecuteScriptAsync(GetEmbedderWebContents(),
                              "document.querySelector('iframe').remove()");

  // Wait for guest to be destroyed.
  GetGuestViewManager()->WaitForLastGuestDeleted();
}

IN_PROC_BROWSER_TEST_F(WebViewAccessibilityTest, LoadWebViewAccessibility) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  LoadAppWithGuest("web_view/focus_accessibility");
  content::WebContents* web_contents = GetFirstAppWindowWebContents();
  content::WaitForAccessibilityTreeToContainNodeWithName(web_contents,
                                                         "Guest button");
}

IN_PROC_BROWSER_TEST_F(WebViewAccessibilityTest, FocusAccessibility) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  LoadAppWithGuest("web_view/focus_accessibility");
  content::WebContents* web_contents = GetFirstAppWindowWebContents();

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

// Validate that an inner frame within a guest WebContents correctly receives
// focus when requested by accessibility. Previously the root
// BrowserAccessibilityManager would not be updated due to how we were updating
// the AXTreeData.
// The test was disabled. See crbug.com/1141313.
IN_PROC_BROWSER_TEST_F(WebViewAccessibilityTest,
                       DISABLED_FocusAccessibilityNestedFrame) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  LoadAppWithGuest("web_view/focus_accessibility");
  content::WebContents* web_contents = GetFirstAppWindowWebContents();

  // Wait for focus to land on the "root web area" role, representing
  // focus on the main document itself.
  while (content::GetFocusedAccessibilityNodeInfo(web_contents).role !=
         ax::mojom::Role::kRootWebArea) {
    content::WaitForAccessibilityFocusChange();
  }

  // Now keep pressing the Tab key until focus lands on a text field.
  // This is testing that the inner frame within the guest WebContents receives
  // focus, and that the focus state is accurately reflected in the accessiblity
  // state.
  while (content::GetFocusedAccessibilityNodeInfo(web_contents).role !=
         ax::mojom::Role::kTextField) {
    content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('\t'),
                              ui::DomCode::TAB, ui::VKEY_TAB, false, false,
                              false, false);
    content::WaitForAccessibilityFocusChange();
  }

  ui::AXNodeData node_data =
      content::GetFocusedAccessibilityNodeInfo(web_contents);
  EXPECT_EQ("InnerFrameTextField",
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
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
    }
  }

  void AccessibilityEventReceived(
      const ui::AXUpdatesAndEvents& event_bundle) override {
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
          run_loop_->Quit();
          return;
        }
      }
    }
  }

  size_t count() const { return count_; }

  const ui::AXNodeData& node_data() const { return node_data_; }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  ax::mojom::Event event_;
  ui::AXNodeData node_data_;
  size_t count_;
};

IN_PROC_BROWSER_TEST_F(WebViewAccessibilityTest, DISABLED_TouchAccessibility) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  LoadAppWithGuest("web_view/touch_accessibility");
  content::WebContents* web_contents = GetFirstAppWindowWebContents();
  content::WebContents* guest_web_contents = GetGuestWebContents();

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
  web_contents->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(accessibility_touch_event);

  // Ensure that we got just a single hover event on the guest WebContents,
  // and that it was fired on a button.
  guest_event_watcher.Wait();
  ui::AXNodeData hit_node = guest_event_watcher.node_data();
  EXPECT_EQ(1U, guest_event_watcher.count());
  EXPECT_EQ(ax::mojom::Role::kButton, hit_node.role);
  EXPECT_EQ(0U, main_event_watcher.count());
}

class WebViewGuestScrollTest : public WebViewTest,
                               public testing::WithParamInterface<bool> {
 public:
  bool GetScrollParam() { return GetParam(); }

  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "ScrollDisabled" : "ScrollEnabled";
  }
};

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
                         testing::Bool(),
                         WebViewGuestScrollTest::DescribeParams);

IN_PROC_BROWSER_TEST_P(WebViewGuestScrollTest, TestGuestWheelScrollsBubble) {
  LoadAppWithGuest("web_view/scrollable_embedder_and_guest");

  if (GetScrollParam())
    SendMessageToGuestAndWait("set_overflow_hidden", "overflow_is_hidden");

  content::WebContents* embedder_contents = GetEmbedderWebContents();
  content::RenderFrameSubmissionObserver embedder_frame_observer(
      embedder_contents);

  std::vector<content::RenderFrameHost*> guest_rfh_list;
  GetGuestViewManager()->WaitForNumGuestsCreated(1u);
  GetGuestViewManager()->GetGuestRenderFrameHostList(&guest_rfh_list);
  ASSERT_EQ(1u, guest_rfh_list.size());

  content::RenderFrameHost* guest_rfh = guest_rfh_list[0];
  content::RenderFrameSubmissionObserver guest_frame_observer(guest_rfh);

  gfx::Rect embedder_rect = embedder_contents->GetContainerBounds();
  gfx::Rect guest_rect = guest_rfh->GetView()->GetViewBounds();

  guest_rect.set_x(guest_rect.x() - embedder_rect.x());
  guest_rect.set_y(guest_rect.y() - embedder_rect.y());
  embedder_rect.set_x(0);
  embedder_rect.set_y(0);

  gfx::PointF default_offset;
  embedder_frame_observer.WaitForScrollOffset(default_offset);

  // Send scroll gesture to embedder & verify.
  // Make sure wheel events don't get filtered.
  float scroll_magnitude = 15.f;

  display::ScreenInfo screen_info =
      GetGuestWebContents()->GetRenderWidgetHostView()->GetScreenInfo();
  {
    // Scroll the embedder from a position in the embedder that is not over
    // the guest.
    gfx::Point embedder_scroll_location = gfx::ScaleToRoundedPoint(
        gfx::Point(embedder_rect.x() + embedder_rect.width() / 2,
                   (embedder_rect.y() + guest_rect.y()) / 2),
        1 / screen_info.device_scale_factor);

    gfx::PointF expected_offset(
        0.f, scroll_magnitude * screen_info.device_scale_factor);

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
  guest_rect = guest_rfh->GetView()->GetViewBounds();
  embedder_rect = embedder_contents->GetContainerBounds();
  guest_rect.set_x(guest_rect.x() - embedder_rect.x());
  guest_rect.set_y(guest_rect.y() - embedder_rect.y());
  {
    gfx::Point guest_scroll_location = gfx::ScaleToRoundedPoint(
        gfx::Point(guest_rect.x() + guest_rect.width() / 2, guest_rect.y()),
        1 / screen_info.device_scale_factor);

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

  std::vector<content::RenderFrameHost*> guest_rfh_list;
  GetGuestViewManager()->WaitForNumGuestsCreated(1u);
  GetGuestViewManager()->GetGuestRenderFrameHostList(&guest_rfh_list);
  ASSERT_EQ(1u, guest_rfh_list.size());

  content::RenderFrameHost* guest_rfh = guest_rfh_list[0];
  content::RenderFrameSubmissionObserver guest_frame_observer(guest_rfh);
  content::RenderWidgetHostView* guest_host_view = guest_rfh->GetView();

  gfx::PointF default_offset;
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
  content::SimulateGestureEvent(guest_rfh->GetRenderWidgetHost(), scroll_begin,
                                ui::LatencyInfo());

  content::InputEventAckWaiter update_waiter(
      guest_rfh->GetRenderWidgetHost(),
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
  content::SimulateGestureEvent(guest_rfh->GetRenderWidgetHost(), scroll_update,
                                ui::LatencyInfo());
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
  content::SimulateGestureEvent(guest_rfh->GetRenderWidgetHost(), scroll_update,
                                ui::LatencyInfo());
  update_waiter.Wait();

  guest_frame_observer.WaitForScrollOffset(default_offset);
}

INSTANTIATE_TEST_SUITE_P(WebViewScrollBubbling,
                         WebViewGuestScrollTouchTest,
                         testing::Bool(),
                         WebViewGuestScrollTouchTest::DescribeParams);

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

  if (GetScrollParam())
    SendMessageToGuestAndWait("set_overflow_hidden", "overflow_is_hidden");

  content::WebContents* embedder_contents = GetEmbedderWebContents();
  content::RenderFrameSubmissionObserver embedder_frame_observer(
      embedder_contents);

  std::vector<content::RenderFrameHost*> guest_rfh_list;
  GetGuestViewManager()->WaitForNumGuestsCreated(1u);
  GetGuestViewManager()->GetGuestRenderFrameHostList(&guest_rfh_list);
  ASSERT_EQ(1u, guest_rfh_list.size());

  content::RenderFrameHost* guest_frame = guest_rfh_list[0];
  content::RenderFrameSubmissionObserver guest_frame_observer(guest_frame);

  gfx::Rect embedder_rect = embedder_contents->GetContainerBounds();
  gfx::Rect guest_rect = guest_frame->GetView()->GetViewBounds();

  guest_rect.set_x(guest_rect.x() - embedder_rect.x());
  guest_rect.set_y(guest_rect.y() - embedder_rect.y());
  embedder_rect.set_x(0);
  embedder_rect.set_y(0);

  gfx::PointF default_offset;
  embedder_frame_observer.WaitForScrollOffset(default_offset);

  // Send scroll gesture to embedder & verify.
  float gesture_distance = 15.f;
  {
    // Scroll the embedder from a position in the embedder that is not over
    // the guest.
    gfx::Point embedder_scroll_location(
        embedder_rect.x() + embedder_rect.width() / 2,
        (embedder_rect.y() + guest_rect.y()) / 2);

    gfx::PointF expected_offset(0.f, gesture_distance);

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
  guest_rect = guest_frame->GetView()->GetViewBounds();
  {
    gfx::Point guest_scroll_location(guest_rect.width() / 2,
                                     guest_rect.height() / 2);

    content::SimulateGestureScrollSequence(guest_frame->GetRenderWidgetHost(),
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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// This verifies the fix for http://crbug.com/667708.
IN_PROC_BROWSER_TEST_F(ChromeSignInWebViewTest,
                       ClosingChromeSignInShouldNotCrash) {
  GURL signin_url{"chrome://chrome-signin/?reason=5"};

  ASSERT_TRUE(AddTabAtIndex(0, signin_url, ui::PAGE_TRANSITION_TYPED));
  ASSERT_TRUE(AddTabAtIndex(1, signin_url, ui::PAGE_TRANSITION_TYPED));
  WaitForWebViewInDom();

  chrome::CloseTab(browser());
}
#endif

// This test verifies that unattached guests are not included as the inner
// WebContents. The test verifies this by triggering a find-in-page request on a
// page with both an attached and an unattached <webview> and verifies that,
// unlike the attached guest, no find requests are sent for the unattached
// guest. For more context see https://crbug.com/897465.
// TODO(mcnee): chrome://chrome-signin is not currently supported on Lacros.
// Instead of repurposing existing webui pages to be able to create webviews
// within a tabbed browser, create a dedicated test webui with the necessary
// guest view permissions.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_NoFindInPageForUnattachedGuest \
  DISABLED_NoFindInPageForUnattachedGuest
#else
#define MAYBE_NoFindInPageForUnattachedGuest NoFindInPageForUnattachedGuest
#endif
IN_PROC_BROWSER_TEST_F(ChromeSignInWebViewTest,
                       MAYBE_NoFindInPageForUnattachedGuest) {
  GURL signin_url{"chrome://chrome-signin/?reason=5"};
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), signin_url));

  // Navigate a tab to a page with a <webview>.
  auto* embedder_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  auto* attached_guest_view =
      GetGuestViewManager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(attached_guest_view);

  auto* find_helper =
      find_in_page::FindTabHelper::FromWebContents(embedder_web_contents);

  // Wait until a first GuestView is attached.
  GetGuestViewManager()->WaitUntilAttached(attached_guest_view);

  base::RunLoop run_loop;
  // This callback is called before attaching a second GuestView.
  GetGuestViewManager()->SetWillAttachCallback(
      base::BindLambdaForTesting([&](guest_view::GuestViewBase* guest_view) {
        ASSERT_TRUE(guest_view);
        ASSERT_FALSE(guest_view->attached());

        auto* attached_guest_rfh = attached_guest_view->GetGuestMainFrame();
        auto* unattached_guest_rfh = guest_view->GetGuestMainFrame();
        EXPECT_NE(unattached_guest_rfh, attached_guest_rfh);
        find_helper->StartFinding(u"doesn't matter", true, true, false);
        auto pending = content::GetRenderFrameHostsWithPendingFindResults(
            embedder_web_contents);
        // Request for main frame of the tab.
        EXPECT_EQ(1U,
                  pending.count(embedder_web_contents->GetPrimaryMainFrame()));
        // Request for main frame of the attached guest.
        EXPECT_EQ(1U, pending.count(attached_guest_rfh));
        // No request for the unattached guest.
        EXPECT_EQ(0U, pending.count(unattached_guest_rfh));
        run_loop.Quit();
      }));
  // Now add a new <webview> and wait until its guest WebContents is created.
  ExecuteScriptAsync(embedder_web_contents,
                     "var webview = document.createElement('webview');"
                     "webview.src = 'data:text/html,foo';"
                     "document.body.appendChild(webview);");
  run_loop.Run();
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
  guest_view::GuestViewBase* guest = GetGuestView();

  // Navigate <webview> to an isolated origin.
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.com", "/title1.html"));
  {
    content::TestFrameNavigationObserver load_observer(
        guest->GetGuestMainFrame());
    EXPECT_TRUE(ExecJs(guest->GetGuestMainFrame(),
                       "location.href = '" + isolated_url.spec() + "';"));
    load_observer.Wait();
  }

  EXPECT_TRUE(guest->GetGuestMainFrame()->GetSiteInstance()->IsGuest());

  // Now, navigate <webview> to a regular page with a subframe.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/iframe.html"));
  {
    content::TestFrameNavigationObserver load_observer(
        guest->GetGuestMainFrame());
    EXPECT_TRUE(ExecJs(guest->GetGuestMainFrame(),
                       "location.href = '" + foo_url.spec() + "';"));
    load_observer.Wait();
  }

  // Navigate subframe in <webview> to an isolated origin.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      ChildFrameAt(guest->GetGuestMainFrame(), 0), isolated_url));

  // Since <webview> supports site isolation, the subframe will be in its own
  // SiteInstance and process.
  content::RenderFrameHost* webview_subframe =
      ChildFrameAt(guest->GetGuestMainFrame(), 0);
  EXPECT_NE(webview_subframe->GetProcess(),
            guest->GetGuestMainFrame()->GetProcess());
  EXPECT_NE(webview_subframe->GetSiteInstance(),
            guest->GetGuestMainFrame()->GetSiteInstance());

  // Load a page with subframe in a regular tab.
  ASSERT_TRUE(AddTabAtIndex(0, foo_url, ui::PAGE_TRANSITION_TYPED));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate that subframe to an isolated origin.  This should not join the
  // WebView process, which has isolated.foo.com committed in a different
  // storage partition.
  EXPECT_TRUE(NavigateIframeToURL(tab, "test", isolated_url));
  content::RenderFrameHost* subframe =
      ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  EXPECT_NE(guest->GetGuestMainFrame()->GetProcess(), subframe->GetProcess());

  // Check that the guest process hasn't crashed.
  EXPECT_TRUE(guest->GetGuestMainFrame()->IsRenderFrameLive());

  // Check that accessing a foo.com cookie from the WebView doesn't result in a
  // renderer kill. This might happen if we erroneously applied an isolated.com
  // origin lock to the WebView process when committing isolated.com.
  EXPECT_EQ(true, EvalJs(guest->GetGuestMainFrame(),
                         "document.cookie = 'foo=bar';\n"
                         "document.cookie == 'foo=bar';\n"));
}

// This test is similar to IsolatedOriginInWebview above, but loads an isolated
// origin in a <webview> subframe *after* loading the same isolated origin in a
// regular tab's subframe.  The isolated origin's subframe in the <webview>
// subframe should not reuse the regular tab's subframe process.  See
// https://crbug.com/751916 and https://crbug.com/751920.
IN_PROC_BROWSER_TEST_F(IsolatedOriginWebViewTest,
                       LoadIsolatedOriginInWebviewAfterLoadingInRegularTab) {
  LoadAppWithGuest("web_view/simple");
  guest_view::GuestViewBase* guest = GetGuestView();

  // Load a page with subframe in a regular tab.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/iframe.html"));
  ASSERT_TRUE(AddTabAtIndex(0, foo_url, ui::PAGE_TRANSITION_TYPED));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate that subframe to an isolated origin.
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(tab, "test", isolated_url));
  content::RenderFrameHost* subframe =
      ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  EXPECT_NE(tab->GetPrimaryMainFrame()->GetProcess(), subframe->GetProcess());

  // Navigate <webview> to a regular page with an isolated origin subframe.
  {
    content::TestFrameNavigationObserver load_observer(
        guest->GetGuestMainFrame());
    EXPECT_TRUE(ExecJs(guest->GetGuestMainFrame(),
                       "location.href = '" + foo_url.spec() + "';"));
    load_observer.Wait();
  }
  EXPECT_TRUE(NavigateToURLFromRenderer(
      ChildFrameAt(guest->GetGuestMainFrame(), 0), isolated_url));

  // Since <webview> supports site isolation, the subframe will be in its own
  // SiteInstance and process.
  content::RenderFrameHost* webview_subframe =
      ChildFrameAt(guest->GetGuestMainFrame(), 0);
  EXPECT_NE(webview_subframe->GetProcess(),
            guest->GetGuestMainFrame()->GetProcess());
  EXPECT_NE(webview_subframe->GetSiteInstance(),
            guest->GetGuestMainFrame()->GetSiteInstance());

  // The isolated origin subframe in <webview> shouldn't share the process with
  // the isolated origin subframe in the regular tab.
  EXPECT_NE(webview_subframe->GetProcess(), subframe->GetProcess());

  // Check that the guest and regular tab processes haven't crashed.
  EXPECT_TRUE(guest->GetGuestMainFrame()->IsRenderFrameLive());
  EXPECT_TRUE(tab->GetPrimaryMainFrame()->IsRenderFrameLive());
  EXPECT_TRUE(subframe->IsRenderFrameLive());

  // Check that accessing a foo.com cookie from the WebView doesn't result in a
  // renderer kill. This might happen if we erroneously applied an isolated.com
  // origin lock to the WebView process when committing isolated.com.
  EXPECT_EQ(true, EvalJs(guest->GetGuestMainFrame(),
                         "document.cookie = 'foo=bar';\n"
                         "document.cookie == 'foo=bar';\n"));
}

// Sends an auto-resize message to the RenderWidgetHost and ensures that the
// auto-resize transaction is handled and produces a single response message
// from guest to embedder.
IN_PROC_BROWSER_TEST_F(WebViewTest, AutoResizeMessages) {
  LoadAppWithGuest("web_view/simple");

  // Helper function as this test requires inspecting a number of content::
  // internal objects.
  EXPECT_TRUE(content::TestGuestAutoresize(GetEmbedderWebContents(),
                                           GetGuestRenderFrameHost()));
}

// Test that a guest sees the synthetic wheel events of a touchpad pinch.
IN_PROC_BROWSER_TEST_F(WebViewTest, TouchpadPinchSyntheticWheelEvents) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  LoadAppWithGuest("web_view/touchpad_pinch");

  content::RenderFrameHost* render_frame_host =
      GetGuestView()->GetGuestMainFrame();
  content::WaitForHitTestData(render_frame_host);

  content::RenderWidgetHost* render_widget_host =
      render_frame_host->GetRenderWidgetHost();

  // Ensure the compositor thread is aware of the wheel listener.
  content::MainThreadFrameObserver synchronize_threads(render_widget_host);
  synchronize_threads.Wait();

  ExtensionTestMessageListener synthetic_wheel_listener("Seen wheel event");

  const gfx::Rect guest_rect = render_widget_host->GetView()->GetViewBounds();
  const gfx::Point pinch_position(guest_rect.width() / 2,
                                  guest_rect.height() / 2);
  content::SimulateGesturePinchSequence(render_widget_host, pinch_position,
                                        1.23,
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

// Tests that random extensions cannot inject content scripts into a platform
// app's own webview, but the owner platform app can. Regression test for
// crbug.com/1205675.
IN_PROC_BROWSER_TEST_F(WebViewTest, NoExtensionScriptsInjectedInWebview) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.

  // Load an extension which injects a content script at document_end. The
  // script injects a new element into the DOM.
  LoadExtension(
      test_data_dir_.AppendASCII("api_test/content_scripts/inject_div"));

  // Load a platform app which creates a webview and injects a content script
  // into it at document_idle, after document_end. The script expects that the
  // webview's DOM has not been modified (in this case, by the extension's
  // content script).
  ExtensionTestMessageListener app_content_script_listener(
      "WebViewTest.NO_ELEMENT_INJECTED");
  app_content_script_listener.set_failure_message(
      "WebViewTest.UNKNOWN_ELEMENT_INJECTED");
  LoadAppWithGuest("web_view/a_com_webview");

  // The app's content script should have been injected, but the extension's
  // content script should not have.
  EXPECT_TRUE(app_content_script_listener.WaitUntilSatisfied())
      << "'" << app_content_script_listener.message()
      << "' message was not receieved";
}

// Regression test for https://crbug.com/1014385
// We load an extension whose background page attempts to declare variables with
// names that are the same as guest view types. The declarations should not be
// syntax errors.
using GuestViewExtensionNameCollisionTest = extensions::ExtensionBrowserTest;
IN_PROC_BROWSER_TEST_F(GuestViewExtensionNameCollisionTest,
                       GuestViewNamesDoNotCollideWithExtensions) {
  ExtensionTestMessageListener loaded_listener("LOADED");
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "platform_apps/web_view/no_extension_name_collision"));
  ASSERT_TRUE(loaded_listener.WaitUntilSatisfied());

  const std::string script =
      "chrome.test.sendScriptResult("
      "    window.testPassed ? 'PASSED' : 'FAILED');";
  const base::Value test_passed =
      ExecuteScriptInBackgroundPage(extension->id(), script);
  EXPECT_EQ("PASSED", test_passed);
}

class PrivateNetworkAccessWebViewTest : public WebViewTest {
 public:
  PrivateNetworkAccessWebViewTest() {
    features_.InitAndEnableFeature(
        features::kBlockInsecurePrivateNetworkRequests);
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Verify that Private Network Access has the correct understanding of guests.
// The local/private/public classification should not be affected by being
// within a guest. See https://crbug.com/1167698 for details.
//
// Note: This test is put in this file for convenience of reusing the entire
// app testing infrastructure. Other similar tests that do not require that
// infrastructure live in PrivateNetworkAccessBrowserTest.*
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWebViewTest, ClassificationInGuest) {
  LoadAppWithGuest("web_view/simple");
  content::RenderFrameHost* guest_frame_host = GetGuestRenderFrameHost();
  ASSERT_TRUE(guest_frame_host);
  EXPECT_TRUE(guest_frame_host->GetSiteInstance()->IsGuest());

  // We'll try to fetch a local page with Access-Control-Allow-Origin: *, to
  // avoid having origin issues on top of privateness issues.
  auto server = std::make_unique<net::EmbeddedTestServer>();
  server->AddDefaultHandlers(GetChromeTestDataDir());
  EXPECT_TRUE(server->Start());
  GURL fetch_url = server->GetURL("/cors-ok.txt");

  // The webview "simple" page is a first navigation to a raw data url. It is
  // currently considered public (internally
  // `network::mojom::IPAddressSpace::kUnknown`).
  // For now, unknown -> local is not blocked, so this fetch succeeds. See also
  // https://crbug.com/1178814.
  EXPECT_EQ(true, content::EvalJs(guest_frame_host,
                                  content::JsReplace(
                                      "fetch($1).then(response => response.ok)",
                                      fetch_url)));
}

// Verify that navigating a <webview> subframe to a disallowed extension
// resource (where the extension ID doesn't match the <webview> owner) doesn't
// result in a renderer kill.  See https://crbug.com/1204094.
IN_PROC_BROWSER_TEST_F(WebViewTest, LoadDisallowedExtensionURLInSubframe) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  base::RunLoop run_loop;

  LoadAppWithGuest("web_view/simple");
  content::RenderFrameHost* guest = GetGuestView()->GetGuestMainFrame();

  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("web_accessible_resources/subframe"));
  ASSERT_TRUE(extension);
  GURL extension_url = extension->GetResourceURL("web_accessible_page.html");

  GURL iframe_url(embedded_test_server()->GetURL("/title1.html"));

  std::string setup_iframe_script = R"(
          var iframe = document.createElement('iframe');
          iframe.id = 'subframe';
          document.body.appendChild(iframe);
      )";

  EXPECT_TRUE(content::ExecJs(guest, setup_iframe_script));
  content::RenderFrameHost* webview_subframe = ChildFrameAt(guest, 0);
  EXPECT_TRUE(content::NavigateToURLFromRenderer(webview_subframe, iframe_url));

  // Navigate the subframe to an unrelated extension URL.  This shouldn't
  // terminate the renderer. If it does, this test will fail via
  // content::NoRendererCrashesAssertion().
  webview_subframe = ChildFrameAt(guest, 0);
  EXPECT_FALSE(
      content::NavigateToURLFromRenderer(webview_subframe, extension_url));

  // The navigation should be aborted and the iframe should be left at its old
  // URL.
  EXPECT_EQ(webview_subframe->GetLastCommittedURL(), iframe_url);
}

class PopupWaiter : public content::WebContentsObserver {
 public:
  PopupWaiter(content::WebContents* opener, base::OnceClosure on_popup)
      : content::WebContentsObserver(opener), on_popup_(std::move(on_popup)) {}

  // WebContentsObserver:
  // Note that `DidOpenRequestedURL` is used as it fires precisely after a new
  // WebContents is created but before it is shown. This timing is necessary for
  // the `ShutdownWithUnshownPopup` test.
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override {
    if (on_popup_) {
      std::move(on_popup_).Run();
    }
  }

 private:
  base::OnceClosure on_popup_;
};

// Test destroying an opener webview while the created window has not been
// shown by the renderer. Between the time of the renderer creating and showing
// the new window, the created guest WebContents is owned by content/ and not by
// WebViewGuest. See `WebContentsImpl::pending_contents_` for details. When we
// destroy the new WebViewGuest, content/ must ensure that the guest WebContents
// is destroyed safely.
//
// This test is conceptually similar to
// testNewWindowOpenerDestroyedWhileUnattached, but for this test, we have
// precise timing requirements that need to be controlled by the browser such
// that we shutdown while the new window is pending.
//
// Regression test for https://crbug.com/1442516
IN_PROC_BROWSER_TEST_F(WebViewTest, ShutdownWithUnshownPopup) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Core classes in content often record trace events during destruction.
  // Enable tracing to test that writing traces with partially destructed
  // objects is done safely.
  ASSERT_TRUE(tracing::BeginTracing("content,navigation"));

  LoadAppWithGuest("web_view/simple");

  base::RunLoop run_loop;
  PopupWaiter popup_waiter(GetGuestWebContents(), run_loop.QuitClosure());
  content::ExecuteScriptAsync(GetGuestRenderFrameHost(),
                              "window.open(location.href);");
  run_loop.Run();
  CloseAppWindow(GetFirstAppWindow());
}

IN_PROC_BROWSER_TEST_F(WebViewTest, InsertIntoIframe) {
  TestHelper("testInsertIntoIframe", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, CreateAndInsertInIframe) {
  TestHelper("testCreateAndInsertInIframe", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, InsertIntoMainFrameFromIframe) {
  TestHelper("testInsertIntoMainFrameFromIframe", "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, InsertIntoOtherWindow) {
  TestHelper("testInsertIntoOtherWindow", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, CreateAndInsertInOtherWindow) {
  TestHelper("testCreateAndInsertInOtherWindow", "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, InsertFromOtherWindow) {
  TestHelper("testInsertFromOtherWindow", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, InsertIntoDetachedIframe) {
  TestHelper("testInsertIntoDetachedIframe", "web_view/shim",
             NEEDS_TEST_SERVER);
  // Round-trip to ensure the embedder did not crash.
  EXPECT_EQ(true, content::EvalJs(GetFirstAppWindowWebContents(), "true"));
}

// Ensure that if a <webview>'s name is set, the guest preserves the
// corresponding window.name across navigations and after a crash and reload.
IN_PROC_BROWSER_TEST_F(WebViewTest, PreserveNameAcrossNavigationsAndCrashes) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  LoadAppWithGuest("web_view/simple");
  GetGuestViewManager()->WaitForSingleGuestRenderFrameHostCreated();

  content::WebContents* embedder = GetEmbedderWebContents();
  EXPECT_TRUE(
      ExecJs(embedder, "document.querySelector('webview').name = 'foo';"));
  extensions::WebViewGuest* guest =
      extensions::WebViewGuest::FromGuestViewBase(GetGuestView());
  EXPECT_EQ("foo", guest->name());

  // Changing the <webview> attribute also changes the current guest
  // document's window.name (see webViewInternal.setName).
  EXPECT_EQ("foo", content::EvalJs(GetGuestRenderFrameHost(), "window.name"));

  // Ensure that the guest's new window.name is preserved across navigations.
  const GURL url_1 = embedded_test_server()->GetURL("a.test", "/title1.html");
  EXPECT_TRUE(
      content::NavigateToURLFromRenderer(GetGuestRenderFrameHost(), url_1));
  EXPECT_EQ("foo", content::EvalJs(GetGuestRenderFrameHost(), "window.name"));

  const GURL url_2 = embedded_test_server()->GetURL("b.test", "/title1.html");
  EXPECT_TRUE(
      content::NavigateToURLFromRenderer(GetGuestRenderFrameHost(), url_2));
  EXPECT_EQ("foo", content::EvalJs(GetGuestRenderFrameHost(), "window.name"));

  // Crash the guest.
  auto* rph = GetGuestRenderFrameHost()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      rph, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(rph->Shutdown(content::RESULT_CODE_KILLED));
  crash_observer.Wait();

  // Reload guest and make sure its window.name is preserved.
  content::TestFrameNavigationObserver load_observer(GetGuestRenderFrameHost());
  EXPECT_TRUE(ExecJs(embedder, "document.querySelector('webview').reload()"));
  load_observer.Wait();
  EXPECT_EQ("foo", content::EvalJs(GetGuestRenderFrameHost(), "window.name"));
}

#if BUILDFLAG(ENABLE_PPAPI)
class WebViewPPAPITest : public WebViewTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebViewTest::SetUpCommandLine(command_line);
    ASSERT_TRUE(ppapi::RegisterTestPlugin(command_line));
  }
};

IN_PROC_BROWSER_TEST_F(WebViewPPAPITest, Shim_TestPlugin) {
  TestHelper("testPlugin", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewPPAPITest, Shim_TestPluginLoadPermission) {
  TestHelper("testPluginLoadPermission", "web_view/shim", NO_TEST_SERVER);
}
#endif  // BUILDFLAG(ENABLE_PPAPI)

// Domain which the Webstore hosted app is associated with in production.
constexpr char kWebstoreURL[] = "https://chrome.google.com/";
// Domain which the new Webstore is associated with in production.
constexpr char kNewWebstoreURL[] = "https://chromewebstore.google.com/";
// Domain for testing an overridden Webstore URL.
constexpr char kWebstoreURLOverride[] = "https://webstore.override.test.com/";

// Helper class for setting up and testing webview behavior with the Chrome
// Webstore. The test param contains the Webstore URL to test.
class WebstoreWebViewTest : public WebViewTest,
                            public testing::WithParamInterface<GURL> {
 public:
  WebstoreWebViewTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        webstore_url_(GetParam()) {}

  WebstoreWebViewTest(const WebstoreWebViewTest&) = delete;
  WebstoreWebViewTest& operator=(const WebstoreWebViewTest&) = delete;

  ~WebstoreWebViewTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Serve files from the extensions test directory as it has a
    // /webstore/ directory, which the Webstore hosted app expects for the URL
    // it is associated with.
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data/extensions");
    ASSERT_TRUE(https_server_.InitializeAndListen());

    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        "MAP * " + https_server_.host_port_pair().ToString());
    // Only override the webstore URL if this test case is testing the override.
    if (webstore_url().spec() == kWebstoreURLOverride) {
      command_line->AppendSwitchASCII(::switches::kAppsGalleryURL,
                                      kWebstoreURLOverride);
    }
    mock_cert_verifier_.SetUpCommandLine(command_line);
    WebViewTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    https_server_.StartAcceptingConnections();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    WebViewTest::SetUpOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    WebViewTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    WebViewTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }
  GURL webstore_url() { return webstore_url_; }

  // Provides meaningful param names.
  static std::string DescribeParams(const testing::TestParamInfo<GURL>& info) {
    GURL webstore_url(info.param);
    if (webstore_url.spec() == kWebstoreURL)
      return "OldWebstore";
    if (webstore_url.spec() == kWebstoreURLOverride)
      return "WebstoreOverride";
    return "NewWebstore";
  }

 private:
  net::EmbeddedTestServer https_server_;
  content::ContentMockCertVerifier mock_cert_verifier_;
  GURL webstore_url_;
};

INSTANTIATE_TEST_SUITE_P(WebViewTests,
                         WebstoreWebViewTest,
                         testing::Values(GURL(kWebstoreURL),
                                         GURL(kWebstoreURLOverride),
                                         GURL(kNewWebstoreURL)),
                         WebstoreWebViewTest::DescribeParams);

// Ensure that an attempt to load Chrome Web Store in a <webview> is blocked
// and does not result in a renderer kill.  See https://crbug.com/1197674.
IN_PROC_BROWSER_TEST_P(WebstoreWebViewTest, NoRendererKillWithChromeWebStore) {
  LoadAppWithGuest("web_view/simple");
  content::RenderFrameHost* guest = GetGuestRenderFrameHost();
  ASSERT_TRUE(guest);

  // Navigate <webview> to a Chrome Web Store URL.  This should result in an
  // error and shouldn't lead to a renderer kill. Note: the webstore hosted app
  // requires the path to start with /webstore/, so for simplicity we serve a
  // page from this path for all the different webstore URLs under test.
  const GURL url = webstore_url().Resolve("/webstore/mock_store.html");

  content::TestNavigationObserver error_observer(
      url, content::MessageLoopRunner::QuitMode::IMMEDIATE,
      /*ignore_uncommitted_navigations=*/false);
  error_observer.WatchExistingWebContents();
  EXPECT_TRUE(ExecJs(guest, "location.href = '" + url.spec() + "';"));
  error_observer.Wait();
  EXPECT_FALSE(error_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, error_observer.last_net_error_code());

  guest = GetGuestRenderFrameHost();
  EXPECT_TRUE(guest->IsRenderFrameLive());

  // Double-check that after the attempted navigation the <webview> is not
  // considered an extension process and does not have the privileged webstore
  // API.
  auto* process_map = extensions::ProcessMap::Get(guest->GetBrowserContext());
  EXPECT_FALSE(process_map->Contains(guest->GetProcess()->GetID()));
  EXPECT_FALSE(
      process_map->GetExtensionIdForProcess(guest->GetProcess()->GetID()));
  EXPECT_EQ(false, content::EvalJs(guest, "!!chrome.webstorePrivate"));
}

// This is a group of tests that check site isolation properties in <webview>
// guests.  Note that site isolation in <webview> is always enabled.
using SitePerProcessWebViewTest = WebViewTest;

// Checks basic site isolation properties when a <webview> main frame and
// subframe navigate cross-site.
IN_PROC_BROWSER_TEST_F(SitePerProcessWebViewTest, SimpleNavigations) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load an app with a <webview> guest that starts at a data: URL.
  LoadAppWithGuest("web_view/simple");
  content::WebContents* guest = GetGuestWebContents();
  ASSERT_TRUE(guest);

  // Ensure the <webview>'s SiteInstance is for a guest.
  content::RenderFrameHost* main_frame = guest->GetPrimaryMainFrame();
  auto original_id = main_frame->GetGlobalId();
  scoped_refptr<content::SiteInstance> starting_instance =
      main_frame->GetSiteInstance();
  EXPECT_TRUE(starting_instance->IsGuest());
  EXPECT_TRUE(starting_instance->GetProcess()->IsForGuestsOnly());
  EXPECT_FALSE(starting_instance->GetStoragePartitionConfig().is_default());

  // Navigate <webview> to a cross-site page with a same-site iframe.
  const GURL start_url =
      embedded_test_server()->GetURL("a.test", "/iframe.html");
  {
    content::TestNavigationObserver load_observer(guest);
    EXPECT_TRUE(ExecJs(guest, "location.href = '" + start_url.spec() + "';"));
    load_observer.Wait();
  }

  // Expect that the main frame swapped SiteInstances and RenderFrameHosts but
  // stayed in the same BrowsingInstance and StoragePartition.
  main_frame = guest->GetPrimaryMainFrame();
  EXPECT_TRUE(main_frame->GetSiteInstance()->IsGuest());
  EXPECT_TRUE(main_frame->GetProcess()->IsForGuestsOnly());
  EXPECT_NE(main_frame->GetGlobalId(), original_id);
  EXPECT_NE(starting_instance, main_frame->GetSiteInstance());
  EXPECT_TRUE(
      starting_instance->IsRelatedSiteInstance(main_frame->GetSiteInstance()));
  EXPECT_EQ(starting_instance->GetStoragePartitionConfig(),
            main_frame->GetSiteInstance()->GetStoragePartitionConfig());
  EXPECT_EQ(starting_instance->GetProcess()->GetStoragePartition(),
            main_frame->GetProcess()->GetStoragePartition());

  // Ensure the guest SiteInstance reflects the proper site and actually uses
  // site isolation.
  EXPECT_EQ("http://a.test/",
            main_frame->GetSiteInstance()->GetSiteURL().spec());
  EXPECT_TRUE(main_frame->GetSiteInstance()->RequiresDedicatedProcess());
  EXPECT_TRUE(main_frame->GetProcess()->IsProcessLockedToSiteForTesting());

  // Navigate <webview> subframe cross-site.  Check that it ends up in a
  // separate guest SiteInstance and process, but same StoragePartition.
  const GURL frame_url =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  EXPECT_TRUE(NavigateIframeToURL(guest, "test", frame_url));
  content::RenderFrameHost* subframe = content::ChildFrameAt(main_frame, 0);

  EXPECT_NE(main_frame->GetSiteInstance(), subframe->GetSiteInstance());
  EXPECT_NE(main_frame->GetProcess(), subframe->GetProcess());
  EXPECT_TRUE(subframe->GetSiteInstance()->IsGuest());
  EXPECT_TRUE(subframe->GetProcess()->IsForGuestsOnly());
  EXPECT_TRUE(main_frame->GetSiteInstance()->IsRelatedSiteInstance(
      subframe->GetSiteInstance()));
  EXPECT_EQ(subframe->GetSiteInstance()->GetStoragePartitionConfig(),
            main_frame->GetSiteInstance()->GetStoragePartitionConfig());
  EXPECT_EQ(subframe->GetProcess()->GetStoragePartition(),
            main_frame->GetProcess()->GetStoragePartition());
  EXPECT_EQ("http://b.test/", subframe->GetSiteInstance()->GetSiteURL().spec());
  EXPECT_TRUE(subframe->GetSiteInstance()->RequiresDedicatedProcess());
  EXPECT_TRUE(subframe->GetProcess()->IsProcessLockedToSiteForTesting());
}

// Check that site-isolated guests can navigate to error pages.  Due to error
// page isolation, error pages in guests should end up in a new error
// SiteInstance, which should still be a guest SiteInstance in the guest's
// StoragePartition.
IN_PROC_BROWSER_TEST_F(SitePerProcessWebViewTest, ErrorPageIsolation) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(content::SiteIsolationPolicy::IsErrorPageIsolationEnabled(
      /*in_main_frame=*/true));

  // Load an app with a <webview> guest that starts at a data: URL.
  LoadAppWithGuest("web_view/simple");
  ASSERT_TRUE(GetGuestRenderFrameHost());

  scoped_refptr<content::SiteInstance> first_instance =
      GetGuestRenderFrameHost()->GetSiteInstance();
  EXPECT_TRUE(first_instance->IsGuest());

  // Navigate <webview> to an error page.
  const GURL error_url =
      embedded_test_server()->GetURL("a.test", "/iframe.html");
  auto interceptor = content::URLLoaderInterceptor::SetupRequestFailForURL(
      error_url, net::ERR_NAME_NOT_RESOLVED);
  {
    content::TestFrameNavigationObserver load_observer(
        GetGuestRenderFrameHost());
    EXPECT_TRUE(ExecJs(GetGuestRenderFrameHost(),
                       "location.href = '" + error_url.spec() + "';"));
    load_observer.Wait();
    EXPECT_FALSE(load_observer.last_navigation_succeeded());
    EXPECT_TRUE(GetGuestRenderFrameHost()->IsErrorDocument());
  }

  // The error page's SiteInstance should require a dedicated process due to
  // error page isolation, but it should still be considered a guest and should
  // stay in the guest's StoragePartition.
  scoped_refptr<content::SiteInstance> error_instance =
      GetGuestRenderFrameHost()->GetSiteInstance();
  EXPECT_TRUE(error_instance->RequiresDedicatedProcess());
  EXPECT_NE(error_instance, first_instance);
  EXPECT_TRUE(error_instance->IsGuest());
  EXPECT_EQ(first_instance->GetStoragePartitionConfig(),
            error_instance->GetStoragePartitionConfig());

  // Navigate to a normal page and then repeat the above with an
  // embedder-initiated navigation to an error page.
  EXPECT_TRUE(BrowserInitNavigationToUrl(
      GetGuestView(),
      embedded_test_server()->GetURL("b.test", "/iframe.html")));
  EXPECT_FALSE(GetGuestRenderFrameHost()->IsErrorDocument());
  EXPECT_NE(GetGuestRenderFrameHost()->GetSiteInstance(), error_instance);

  content::WebContents* embedder = GetEmbedderWebContents();
  {
    content::TestFrameNavigationObserver load_observer(
        GetGuestRenderFrameHost());
    EXPECT_TRUE(ExecJs(embedder, "document.querySelector('webview').src = '" +
                                     error_url.spec() + "';"));
    load_observer.Wait();
    EXPECT_FALSE(load_observer.last_navigation_succeeded());
    EXPECT_TRUE(GetGuestRenderFrameHost()->IsErrorDocument());
  }

  scoped_refptr<content::SiteInstance> second_error_instance =
      GetGuestRenderFrameHost()->GetSiteInstance();
  EXPECT_TRUE(second_error_instance->RequiresDedicatedProcess());
  EXPECT_TRUE(second_error_instance->IsGuest());
  EXPECT_EQ(first_instance->GetStoragePartitionConfig(),
            second_error_instance->GetStoragePartitionConfig());

  // Because we swapped BrowsingInstances above, this error page SiteInstance
  // should be different from the first error page SiteInstance, but it should
  // be in the same StoragePartition.
  EXPECT_NE(error_instance, second_error_instance);
  EXPECT_EQ(error_instance->GetStoragePartitionConfig(),
            second_error_instance->GetStoragePartitionConfig());
}

// Ensure that the browser doesn't crash when a subframe in a <webview> is
// navigated to an unknown scheme.  This used to be the case due to a mismatch
// between the error page's SiteInstance and the origin to commit as calculated
// in NavigationRequest.  See https://crbug.com/1366450.
IN_PROC_BROWSER_TEST_F(SitePerProcessWebViewTest, ErrorPageInSubframe) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load an app with a <webview> guest that starts at a data: URL.
  LoadAppWithGuest("web_view/simple");
  ASSERT_TRUE(GetGuestRenderFrameHost());

  scoped_refptr<content::SiteInstance> first_instance =
      GetGuestRenderFrameHost()->GetSiteInstance();
  EXPECT_TRUE(first_instance->IsGuest());

  // Navigate <webview> to a page with an iframe.
  const GURL first_url =
      embedded_test_server()->GetURL("a.test", "/iframe.html");
  {
    content::TestFrameNavigationObserver load_observer(
        GetGuestRenderFrameHost());
    EXPECT_TRUE(ExecJs(GetGuestRenderFrameHost(),
                       "location.href = '" + first_url.spec() + "';"));
    load_observer.Wait();
    EXPECT_TRUE(load_observer.last_navigation_succeeded());
  }

  // At this point, the guest's iframe should already be loaded.  Navigate
  // it to an unknown scheme, which will result in an error. This shouldn't
  // crash the browser.
  content::RenderFrameHost* guest_subframe =
      ChildFrameAt(GetGuestRenderFrameHost(), 0);
  int initial_process_id = guest_subframe->GetProcess()->GetID();
  const GURL error_url = GURL("unknownscheme:foo");
  {
    content::TestFrameNavigationObserver load_observer(guest_subframe);
    EXPECT_TRUE(
        ExecJs(guest_subframe, "location.href = '" + error_url.spec() + "';"));
    load_observer.Wait();
    EXPECT_FALSE(load_observer.last_navigation_succeeded());
    auto* error_rfh = ChildFrameAt(GetGuestRenderFrameHost(), 0);
    EXPECT_TRUE(error_rfh->IsErrorDocument());

    // Double-check that the error page has an opaque origin, and that the
    // precursor doesn't point to a.test.
    url::Origin error_origin = error_rfh->GetLastCommittedOrigin();
    EXPECT_TRUE(error_origin.opaque());
    EXPECT_FALSE(error_origin.GetTupleOrPrecursorTupleIfOpaque().IsValid());

    // The error page should not load in the initiator's process.
    EXPECT_NE(initial_process_id, error_rfh->GetProcess()->GetID());
  }
}

// Checks that a main frame navigation in a <webview> can swap
// BrowsingInstances while staying in the same StoragePartition.
IN_PROC_BROWSER_TEST_F(SitePerProcessWebViewTest, BrowsingInstanceSwap) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load an app with a <webview> guest that starts at a data: URL.
  LoadAppWithGuest("web_view/simple");
  ASSERT_TRUE(GetGuestRenderFrameHost());

  // Navigate <webview> to a page on a.test.
  const GURL first_url =
      embedded_test_server()->GetURL("a.test", "/iframe.html");
  {
    content::TestFrameNavigationObserver load_observer(
        GetGuestRenderFrameHost());
    EXPECT_TRUE(ExecJs(GetGuestRenderFrameHost(),
                       "location.href = '" + first_url.spec() + "';"));
    load_observer.Wait();
  }

  scoped_refptr<content::SiteInstance> first_instance =
      GetGuestRenderFrameHost()->GetSiteInstance();
  EXPECT_TRUE(first_instance->IsGuest());
  EXPECT_TRUE(first_instance->GetProcess()->IsForGuestsOnly());

  // Navigate <webview> to a cross-site page and use a browser-initiated
  // navigation to force a BrowsingInstance swap.
  const GURL second_url =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  EXPECT_TRUE(BrowserInitNavigationToUrl(GetGuestView(), second_url));
  scoped_refptr<content::SiteInstance> second_instance =
      GetGuestRenderFrameHost()->GetSiteInstance();

  // Ensure that a new unrelated guest SiteInstance was created, and that the
  // StoragePartition didn't change.
  EXPECT_TRUE(second_instance->IsGuest());
  EXPECT_TRUE(second_instance->GetProcess()->IsForGuestsOnly());
  EXPECT_NE(first_instance, second_instance);
  EXPECT_FALSE(first_instance->IsRelatedSiteInstance(second_instance.get()));
  EXPECT_EQ(first_instance->GetStoragePartitionConfig(),
            second_instance->GetStoragePartitionConfig());
  EXPECT_EQ(first_instance->GetProcess()->GetStoragePartition(),
            second_instance->GetProcess()->GetStoragePartition());
}

// Helper class to count the number of guest processes created.
class GuestProcessCreationObserver
    : public content::RenderProcessHostCreationObserver {
 public:
  GuestProcessCreationObserver() = default;
  ~GuestProcessCreationObserver() override = default;
  GuestProcessCreationObserver(const GuestProcessCreationObserver&) = delete;
  GuestProcessCreationObserver& operator=(const GuestProcessCreationObserver&) =
      delete;

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(
      content::RenderProcessHost* process_host) override {
    if (process_host->IsForGuestsOnly())
      guest_process_count_++;
  }

  size_t guess_process_count() { return guest_process_count_; }

 private:
  size_t guest_process_count_ = 0U;
};

// Checks that a cross-process navigation in a <webview> does not unnecessarily
// recreate the guest process at OnResponseStarted time.
IN_PROC_BROWSER_TEST_F(SitePerProcessWebViewTest,
                       NoExtraGuestProcessAtResponseTime) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load an app with a <webview> guest that starts at a data: URL.
  LoadAppWithGuest("web_view/simple");
  ASSERT_TRUE(GetGuestView());

  // Start a navigation in the <webview> to a cross-site page and use a
  // browser-initiated navigation to force a BrowsingInstance swap.
  const GURL guest_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  GuestProcessCreationObserver observer;
  EXPECT_TRUE(BrowserInitNavigationToUrl(GetGuestView(), guest_url));

  // This should only trigger creation of one additional guest process. There
  // used to be a bug where a speculative RenderFrameHost that was created
  // initially was incorrectly thrown away and recreated when the response was
  // received, leading to an additional wasted guest process.  Note that since
  // speculative RenderFrameHosts aren't exposed outside of content/, we can't
  // directly observe them here.
  EXPECT_EQ(1U, observer.guess_process_count());
}

// Test that both webview-initiated and embedder-initiated navigations to
// about:blank behave sanely.
IN_PROC_BROWSER_TEST_F(SitePerProcessWebViewTest, NavigateToAboutBlank) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load an app with a <webview> guest that starts at a data: URL.
  LoadAppWithGuest("web_view/simple");
  ASSERT_TRUE(GetGuestRenderFrameHost());
  scoped_refptr<content::SiteInstance> first_instance =
      GetGuestRenderFrameHost()->GetSiteInstance();
  EXPECT_TRUE(first_instance->IsGuest());
  EXPECT_TRUE(first_instance->GetProcess()->IsForGuestsOnly());

  // Ask <webview> to navigate itself to about:blank.  This should stay in the
  // same SiteInstance.
  const GURL blank_url(url::kAboutBlankURL);
  EXPECT_TRUE(
      content::NavigateToURLFromRenderer(GetGuestRenderFrameHost(), blank_url));
  scoped_refptr<content::SiteInstance> second_instance =
      GetGuestRenderFrameHost()->GetSiteInstance();
  EXPECT_EQ(first_instance, second_instance);

  // Navigate <webview> away to another page.  This should swap
  // BrowsingInstances as it's a cross-site browser-initiated navigation.
  const GURL second_url =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  EXPECT_TRUE(BrowserInitNavigationToUrl(GetGuestView(), second_url));
  ASSERT_TRUE(GetGuestRenderFrameHost());
  scoped_refptr<content::SiteInstance> third_instance =
      GetGuestRenderFrameHost()->GetSiteInstance();
  EXPECT_TRUE(third_instance->IsGuest());
  EXPECT_TRUE(third_instance->GetProcess()->IsForGuestsOnly());
  EXPECT_NE(first_instance, third_instance);
  EXPECT_FALSE(first_instance->IsRelatedSiteInstance(third_instance.get()));
  EXPECT_EQ(first_instance->GetStoragePartitionConfig(),
            third_instance->GetStoragePartitionConfig());
  EXPECT_EQ(first_instance->GetProcess()->GetStoragePartition(),
            third_instance->GetProcess()->GetStoragePartition());

  // Ask embedder to navigate the webview back to about:blank.  This should
  // stay in the same SiteInstance.
  content::WebContents* embedder = GetEmbedderWebContents();
  {
    content::TestFrameNavigationObserver load_observer(
        GetGuestRenderFrameHost());
    EXPECT_TRUE(ExecJs(embedder, "document.querySelector('webview').src = '" +
                                     blank_url.spec() + "';"));
    load_observer.Wait();
  }
  scoped_refptr<content::SiteInstance> fourth_instance =
      GetGuestRenderFrameHost()->GetSiteInstance();
  EXPECT_EQ(fourth_instance, third_instance);
}

// Test that site-isolated <webview> doesn't crash when its initial navigation
// is to an about:blank URL.
IN_PROC_BROWSER_TEST_F(SitePerProcessWebViewTest, Shim_BlankWebview) {
  TestHelper("testBlankWebview", "web_view/shim", NO_TEST_SERVER);

  content::RenderFrameHost* guest_rfh =
      GetGuestViewManager()->WaitForSingleGuestRenderFrameHostCreated();
  ASSERT_TRUE(guest_rfh);
  scoped_refptr<content::SiteInstance> site_instance =
      guest_rfh->GetSiteInstance();
  EXPECT_TRUE(site_instance->IsGuest());
  EXPECT_TRUE(site_instance->GetProcess()->IsForGuestsOnly());
}

// Checks that content scripts work when a <webview> navigates across multiple
// processes.
IN_PROC_BROWSER_TEST_F(SitePerProcessWebViewTest, ContentScript) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Load an app with a <webview> guest that starts at a data: URL.
  LoadAppWithGuest("web_view/simple");
  content::WebContents* embedder = GetEmbedderWebContents();
  content::RenderFrameHost* main_frame = GetGuestRenderFrameHost();
  ASSERT_TRUE(main_frame);
  auto* web_view_renderer_state =
      extensions::WebViewRendererState::GetInstance();

  // Ensure the <webview>'s SiteInstance is for a guest.
  scoped_refptr<content::SiteInstance> starting_instance =
      main_frame->GetSiteInstance();
  EXPECT_TRUE(starting_instance->IsGuest());
  // There should be no <webview> content scripts yet.
  {
    extensions::WebViewRendererState::WebViewInfo info;
    ASSERT_TRUE(web_view_renderer_state->GetInfo(
        main_frame->GetProcess()->GetID(), main_frame->GetRoutingID(), &info));
    EXPECT_TRUE(info.content_script_ids.empty());
  }

  // WebViewRendererState should have an entry for a single guest instance.
  ASSERT_EQ(1u, web_view_renderer_state->guest_count_for_testing());

  // Navigate <webview> to a.test.  This should swap processes.  Wait for the
  // old RenderFrameHost to be destroyed and check that there's still a single
  // guest instance.
  const GURL start_url =
      embedded_test_server()->GetURL("a.test", "/title1.html");
  {
    content::RenderFrameDeletedObserver deleted_observer(main_frame);
    EXPECT_TRUE(BrowserInitNavigationToUrl(GetGuestView(), start_url));
    deleted_observer.WaitUntilDeleted();
    ASSERT_EQ(1u, web_view_renderer_state->guest_count_for_testing());
  }

  // Inject a content script.
  {
    const char kContentScriptTemplate[] = R"(
        var webview = document.querySelector('webview');
        webview.addContentScripts([{
            name: 'rule',
            matches: ['*://*/*'],
            js: { code: $1 },
            run_at: 'document_start'}]);
    )";
    const char kContentScript[] = R"(
        chrome.test.sendMessage("Hello from content script!");
    )";

    EXPECT_TRUE(content::ExecJs(
        embedder, content::JsReplace(kContentScriptTemplate, kContentScript)));

    // Ensure the new content script is now tracked for the <webview> in the
    // browser process.
    main_frame = GetGuestRenderFrameHost();
    {
      extensions::WebViewRendererState::WebViewInfo info;
      ASSERT_TRUE(
          web_view_renderer_state->GetInfo(main_frame->GetProcess()->GetID(),
                                           main_frame->GetRoutingID(), &info));
      EXPECT_EQ(1U, info.content_script_ids.size());
    }
  }

  // Navigate <webview> cross-site and ensure the new content script runs.
  ExtensionTestMessageListener script_listener("Hello from content script!");
  const GURL second_url =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  {
    content::RenderFrameDeletedObserver deleted_observer(main_frame);
    EXPECT_TRUE(BrowserInitNavigationToUrl(GetGuestView(), second_url));
    deleted_observer.WaitUntilDeleted();
    ASSERT_EQ(1u, web_view_renderer_state->guest_count_for_testing());
  }
  EXPECT_TRUE(script_listener.WaitUntilSatisfied());

  // Check that the content script is tracked for the new <webview> process.
  main_frame = GetGuestRenderFrameHost();
  EXPECT_TRUE(main_frame->GetSiteInstance()->IsGuest());
  EXPECT_NE(main_frame->GetSiteInstance(), starting_instance);
  {
    extensions::WebViewRendererState::WebViewInfo info;
    ASSERT_TRUE(web_view_renderer_state->GetInfo(
        main_frame->GetProcess()->GetID(), main_frame->GetRoutingID(), &info));
    EXPECT_EQ(1U, info.content_script_ids.size());
  }

  // Remove the <webview> and ensure no guests remain in WebViewRendererState.
  {
    content::RenderFrameDeletedObserver deleted_observer(main_frame);
    EXPECT_TRUE(content::ExecJs(embedder,
                                "document.querySelector('webview').remove()"));
    deleted_observer.WaitUntilDeleted();
    ASSERT_EQ(0u, web_view_renderer_state->guest_count_for_testing());
  }
}

// Checks that content scripts work in an out-of-process iframe in a <webview>
// tag.
// TODO(crbug.com/40864752): Fix flakiness on win-rel. The test is also disabled
// on mac11-arm64-rel using filter.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ContentScriptInOOPIF DISABLED_ContentScriptInOOPIF
#else
#define MAYBE_ContentScriptInOOPIF ContentScriptInOOPIF
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessWebViewTest, MAYBE_ContentScriptInOOPIF) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Load an app with a <webview> guest that starts at a data: URL.
  LoadAppWithGuest("web_view/simple");
  content::WebContents* embedder = GetEmbedderWebContents();
  content::RenderFrameHost* main_frame = GetGuestRenderFrameHost();
  ASSERT_TRUE(main_frame);
  auto* web_view_renderer_state =
      extensions::WebViewRendererState::GetInstance();

  // WebViewRendererState should have an entry for a single guest instance.
  ASSERT_EQ(1u, web_view_renderer_state->guest_count_for_testing());

  // Inject a content script that targets title1.html and is enabled for all
  // frames (so that it works in subframes).
  {
    const char kContentScriptTemplate[] = R"(
        var webview = document.querySelector('webview');
        webview.addContentScripts([{
            name: 'rule',
            matches: ['*://*/title1.html'],
            all_frames: true,
            js: { code: $1 },
            run_at: 'document_start'}]);
    )";
    const char kContentScript[] = R"(
        chrome.test.sendMessage("Hello from content script!");
    )";

    EXPECT_TRUE(content::ExecJs(
        embedder, content::JsReplace(kContentScriptTemplate, kContentScript)));
  }

  // Navigate <webview> to a page with a same-site subframe.
  const GURL start_url =
      embedded_test_server()->GetURL("a.test", "/iframe.html");
  {
    content::RenderFrameDeletedObserver deleted_observer(main_frame);
    EXPECT_TRUE(BrowserInitNavigationToUrl(GetGuestView(), start_url));
    deleted_observer.WaitUntilDeleted();

    // There should be two guest frames at this point.
    ASSERT_EQ(2u, web_view_renderer_state->guest_count_for_testing());
  }

  main_frame = GetGuestRenderFrameHost();
  content::RenderFrameHost* subframe = content::ChildFrameAt(main_frame, 0);

  // Navigate <webview> subframe cross-site to a URL that matches the content
  // script pattern and ensure the new content script runs.
  ExtensionTestMessageListener script_listener("Hello from content script!");
  const GURL frame_url =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  {
    content::RenderFrameDeletedObserver deleted_observer(subframe);
    EXPECT_TRUE(NavigateToURLFromRenderer(subframe, frame_url));
    deleted_observer.WaitUntilDeleted();
    subframe = content::ChildFrameAt(main_frame, 0);
    EXPECT_TRUE(subframe->IsCrossProcessSubframe());
    // There should still be two guest frames (main frame and new subframe).
    ASSERT_EQ(2u, web_view_renderer_state->guest_count_for_testing());
  }
  EXPECT_TRUE(script_listener.WaitUntilSatisfied());
}

// Check that with site-isolated <webview>, two same-site OOPIFs in two
// unrelated <webview> tags share the same process due to the subframe process
// reuse policy.
IN_PROC_BROWSER_TEST_F(SitePerProcessWebViewTest, SubframeProcessReuse) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load an app with a <webview> guest that starts at a data: URL.
  LoadAppWithGuest("web_view/simple");
  guest_view::GuestViewBase* guest = GetGuestView();
  ASSERT_TRUE(guest);

  // Navigate <webview> to a cross-site page with a same-site iframe.
  const GURL start_url =
      embedded_test_server()->GetURL("a.test", "/iframe.html");
  EXPECT_TRUE(BrowserInitNavigationToUrl(guest, start_url));

  // Navigate <webview> subframe cross-site.
  const GURL frame_url =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  content::RenderFrameHost* subframe =
      content::ChildFrameAt(guest->GetGuestMainFrame(), 0);
  EXPECT_TRUE(NavigateToURLFromRenderer(subframe, frame_url));
  // Attach a second <webview>.
  ASSERT_TRUE(content::ExecJs(
      GetEmbedderWebContents(),
      base::StrCat({"const w = document.createElement('webview'); w.src = '",
                    start_url.spec(), "'; document.body.appendChild(w);"})));
  GetGuestViewManager()->WaitForNumGuestsCreated(2u);
  auto* guest2 = GetGuestViewManager()->GetLastGuestViewCreated();
  ASSERT_NE(guest, guest2);

  // Navigate second <webview> cross-site.  Use NavigateToURL() to swap
  // BrowsingInstances.
  const GURL second_guest_url =
      embedded_test_server()->GetURL("c.test", "/iframe.html");
  EXPECT_TRUE(BrowserInitNavigationToUrl(guest2, second_guest_url));
  EXPECT_NE(guest->GetGuestMainFrame()->GetSiteInstance(),
            guest2->GetGuestMainFrame()->GetSiteInstance());
  EXPECT_NE(guest->GetGuestMainFrame()->GetProcess(),
            guest2->GetGuestMainFrame()->GetProcess());
  EXPECT_FALSE(
      guest->GetGuestMainFrame()->GetSiteInstance()->IsRelatedSiteInstance(
          guest2->GetGuestMainFrame()->GetSiteInstance()));
  // Navigate second <webview> subframe to the same site as the first <webview>
  // subframe, ending up with A(B) in `guest` and C(B) in `guest2`.  These
  // subframes should be in the same (guest's) StoragePartition, but different
  // SiteInstances and BrowsingInstances. Nonetheless, due to the subframe
  // reuse policy, they should share the same process.
  content::RenderFrameHost* subframe2 =
      content::ChildFrameAt(guest2->GetGuestMainFrame(), 0);
  ASSERT_TRUE(subframe2);
  EXPECT_TRUE(NavigateToURLFromRenderer(subframe2, frame_url));

  subframe = content::ChildFrameAt(guest->GetGuestMainFrame(), 0);
  subframe2 = content::ChildFrameAt(guest2->GetGuestMainFrame(), 0);
  EXPECT_NE(subframe->GetSiteInstance(), subframe2->GetSiteInstance());
  EXPECT_EQ(subframe->GetSiteInstance()->GetStoragePartitionConfig(),
            subframe2->GetSiteInstance()->GetStoragePartitionConfig());
  EXPECT_EQ(subframe->GetProcess(), subframe2->GetProcess());
}

// Helper class to turn off strict site isolation while still using site
// isolation paths for <webview>.  This forces <webview> to use the default
// SiteInstance paths. The helper also defines one isolated origin at
// isolated.com, which takes precedence over the command-line switch to disable
// site isolation and can be used to test a combination of SiteInstances that
// require and don't require dedicated processes.
class WebViewWithDefaultSiteInstanceTest : public SitePerProcessWebViewTest {
 public:
  WebViewWithDefaultSiteInstanceTest() = default;
  ~WebViewWithDefaultSiteInstanceTest() override = default;
  WebViewWithDefaultSiteInstanceTest(
      const WebViewWithDefaultSiteInstanceTest&) = delete;
  WebViewWithDefaultSiteInstanceTest& operator=(
      const WebViewWithDefaultSiteInstanceTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
    command_line->AppendSwitchASCII(switches::kIsolateOrigins,
                                    "http://isolated.com");
    SitePerProcessWebViewTest::SetUpCommandLine(command_line);
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
};

// Check that when strict site isolation is turned off (via a command-line flag
// or from chrome://flags), the <webview> site isolation paths still work. In
// particular, <webview> navigations should use a default SiteInstance which
// should still be considered a guest SiteInstance in the guest's
// StoragePartition.  Cross-site navigations in the guest should stay in the
// same SiteInstance, and the guest process shouldn't be locked.
IN_PROC_BROWSER_TEST_F(WebViewWithDefaultSiteInstanceTest, SimpleNavigations) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load an app with a <webview> guest that starts at a data: URL.
  LoadAppWithGuest("web_view/simple");
  content::RenderFrameHost* main_frame = GetGuestRenderFrameHost();
  ASSERT_TRUE(main_frame);
  auto original_id = main_frame->GetGlobalId();
  scoped_refptr<content::SiteInstance> starting_instance =
      main_frame->GetSiteInstance();

  // Because this test runs without strict site isolation, the <webview>
  // process shouldn't be locked.  However, the <webview>'s process and
  // SiteInstance should still be for a guest.
  EXPECT_FALSE(
      starting_instance->GetProcess()->IsProcessLockedToSiteForTesting());
  EXPECT_FALSE(starting_instance->RequiresDedicatedProcess());
  EXPECT_TRUE(starting_instance->IsGuest());
  EXPECT_TRUE(starting_instance->GetProcess()->IsForGuestsOnly());
  EXPECT_FALSE(starting_instance->GetStoragePartitionConfig().is_default());

  // Navigate <webview> to a cross-site page with a same-site iframe.
  const GURL start_url =
      embedded_test_server()->GetURL("a.test", "/iframe.html");
  {
    content::TestFrameNavigationObserver load_observer(main_frame);
    EXPECT_TRUE(
        ExecJs(main_frame, "location.href = '" + start_url.spec() + "';"));
    load_observer.Wait();
  }

  // Expect that we stayed in the same (default) SiteInstance.
  main_frame = GetGuestRenderFrameHost();
  ASSERT_TRUE(main_frame);
  if (!main_frame->ShouldChangeRenderFrameHostOnSameSiteNavigation()) {
    // The RenderFrameHost will stay the same when we don't change
    // RenderFrameHosts on same-SiteInstance navigations.
    EXPECT_EQ(main_frame->GetGlobalId(), original_id);
  }
  EXPECT_EQ(starting_instance, main_frame->GetSiteInstance());
  EXPECT_FALSE(main_frame->GetSiteInstance()->RequiresDedicatedProcess());
  EXPECT_FALSE(main_frame->GetProcess()->IsProcessLockedToSiteForTesting());

  // Navigate <webview> subframe cross-site.  Check that it stays in the same
  // SiteInstance and process.
  const GURL frame_url =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  content::RenderFrameHost* subframe = content::ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(subframe);
  EXPECT_TRUE(NavigateToURLFromRenderer(subframe, frame_url));
  subframe = content::ChildFrameAt(main_frame, 0);
  EXPECT_EQ(main_frame->GetSiteInstance(), subframe->GetSiteInstance());
  EXPECT_EQ(main_frame->GetProcess(), subframe->GetProcess());
  EXPECT_TRUE(subframe->GetSiteInstance()->IsGuest());
  EXPECT_FALSE(subframe->GetSiteInstance()->RequiresDedicatedProcess());
  EXPECT_FALSE(subframe->GetProcess()->IsProcessLockedToSiteForTesting());
}

// Similar to the test above, but also exercises navigations to an isolated
// origin, which takes precedence over switches::kDisableSiteIsolation. In this
// setup, navigations to the isolated origin should use a normal SiteInstance
// that requires a dedicated process, while all other navigations should use
// the default SiteInstance and an unlocked process.
IN_PROC_BROWSER_TEST_F(WebViewWithDefaultSiteInstanceTest, IsolatedOrigin) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load an app with a <webview> guest that starts at a data: URL.
  LoadAppWithGuest("web_view/simple");
  content::RenderFrameHost* main_frame = GetGuestRenderFrameHost();
  ASSERT_TRUE(main_frame);
  auto original_id = main_frame->GetGlobalId();
  scoped_refptr<content::SiteInstance> starting_instance =
      main_frame->GetSiteInstance();

  // Because this test runs without strict site isolation, the <webview>
  // process shouldn't be locked.  However, the <webview>'s process and
  // SiteInstance should still be for a guest.
  EXPECT_FALSE(
      starting_instance->GetProcess()->IsProcessLockedToSiteForTesting());
  EXPECT_FALSE(starting_instance->RequiresDedicatedProcess());
  EXPECT_TRUE(starting_instance->IsGuest());
  EXPECT_TRUE(starting_instance->GetProcess()->IsForGuestsOnly());
  EXPECT_FALSE(starting_instance->GetStoragePartitionConfig().is_default());

  // Navigate to an isolated origin.  Isolated origins take precedence over
  // switches::kDisableSiteIsolation, so we should swap SiteInstances and
  // processes, ending up in a locked process.
  const GURL start_url =
      embedded_test_server()->GetURL("isolated.com", "/iframe.html");
  {
    content::TestFrameNavigationObserver load_observer(main_frame);
    EXPECT_TRUE(
        ExecJs(main_frame, "location.href = '" + start_url.spec() + "';"));
    load_observer.Wait();
  }

  main_frame = GetGuestRenderFrameHost();
  ASSERT_TRUE(main_frame);
  EXPECT_NE(main_frame->GetGlobalId(), original_id);
  EXPECT_NE(starting_instance, main_frame->GetSiteInstance());
  EXPECT_TRUE(main_frame->GetSiteInstance()->RequiresDedicatedProcess());
  EXPECT_TRUE(main_frame->GetProcess()->IsProcessLockedToSiteForTesting());

  // Navigate a subframe on the isolated origin cross-site to a non-isolated
  // URL. The subframe should go back into a default SiteInstance in a
  // different unlocked process.
  const GURL frame_url =
      embedded_test_server()->GetURL("b.test", "/title1.html");
  content::RenderFrameHost* subframe = content::ChildFrameAt(main_frame, 0);
  {
    content::TestFrameNavigationObserver subframe_load_observer(subframe);
    EXPECT_TRUE(
        ExecJs(subframe, "location.href = '" + frame_url.spec() + "';"));
    subframe_load_observer.Wait();
  }
  subframe = content::ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(subframe);
  EXPECT_NE(main_frame->GetSiteInstance(), subframe->GetSiteInstance());
  EXPECT_NE(main_frame->GetProcess(), subframe->GetProcess());
  EXPECT_TRUE(subframe->GetSiteInstance()->IsGuest());
  EXPECT_FALSE(subframe->GetSiteInstance()->RequiresDedicatedProcess());
  EXPECT_FALSE(subframe->GetProcess()->IsProcessLockedToSiteForTesting());

  // Check that all frames stayed in the same guest StoragePartition.
  EXPECT_EQ(main_frame->GetSiteInstance()->GetStoragePartitionConfig(),
            subframe->GetSiteInstance()->GetStoragePartitionConfig());
  EXPECT_EQ(main_frame->GetSiteInstance()->GetStoragePartitionConfig(),
            starting_instance->GetStoragePartitionConfig());
}

IN_PROC_BROWSER_TEST_F(WebViewWithDefaultSiteInstanceTest, FencedFrame) {
  TestHelper("testAddFencedFrame", "web_view/shim", NEEDS_TEST_SERVER);

  auto* guest_rfh =
      GetGuestViewManager()->WaitForSingleGuestRenderFrameHostCreated();
  std::vector<content::RenderFrameHost*> rfhs =
      content::CollectAllRenderFrameHosts(guest_rfh);
  ASSERT_EQ(rfhs.size(), 2u);
  ASSERT_EQ(rfhs[0], guest_rfh);
  content::RenderFrameHostWrapper fenced_frame(rfhs[1]);
  EXPECT_TRUE(fenced_frame->IsFencedFrameRoot());

  content::SiteInstance* fenced_frame_site_instance =
      fenced_frame->GetSiteInstance();
  EXPECT_FALSE(fenced_frame->IsErrorDocument());
  EXPECT_NE(fenced_frame_site_instance, guest_rfh->GetSiteInstance());
  EXPECT_TRUE(fenced_frame_site_instance->IsGuest());
  EXPECT_EQ(fenced_frame_site_instance->GetStoragePartitionConfig(),
            guest_rfh->GetSiteInstance()->GetStoragePartitionConfig());
  EXPECT_EQ(fenced_frame->GetProcess(), guest_rfh->GetProcess());
}

class WebViewFencedFrameTest : public WebViewTest,
                               public testing::WithParamInterface<bool> {
 public:
  WebViewFencedFrameTest() {
    scoped_feature_list_.InitWithFeatureState(features::kIsolateFencedFrames,
                                              /*enabled=*/GetParam());
  }
  ~WebViewFencedFrameTest() override = default;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "IsolateFencedFramesEnabled"
                      : "IsolateFencedFramesDisabled";
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(WebViewTests,
                         WebViewFencedFrameTest,
                         testing::Bool(),
                         WebViewFencedFrameTest::DescribeParams);

IN_PROC_BROWSER_TEST_P(WebViewFencedFrameTest,
                       FencedFrameInGuestHasGuestSiteInstance) {
  TestHelper("testAddFencedFrame", "web_view/shim", NEEDS_TEST_SERVER);

  auto* guest_rfh =
      GetGuestViewManager()->WaitForSingleGuestRenderFrameHostCreated();
  std::vector<content::RenderFrameHost*> rfhs =
      content::CollectAllRenderFrameHosts(guest_rfh);
  ASSERT_EQ(rfhs.size(), 2u);
  ASSERT_EQ(rfhs[0], guest_rfh);
  content::RenderFrameHostWrapper ff_rfh(rfhs[1]);

  EXPECT_NE(ff_rfh->GetSiteInstance(), guest_rfh->GetSiteInstance());
  EXPECT_TRUE(guest_rfh->GetSiteInstance()->IsGuest());
  EXPECT_TRUE(ff_rfh->GetSiteInstance()->IsGuest());
  EXPECT_EQ(ff_rfh->GetSiteInstance()->GetStoragePartitionConfig(),
            guest_rfh->GetSiteInstance()->GetStoragePartitionConfig());

  // The fenced frame will be in a different process from the embedding guest
  // only if Process Isolation for Fenced Frames is enabled.
  if (content::SiteIsolationPolicy::
          IsProcessIsolationForFencedFramesEnabled()) {
    EXPECT_NE(ff_rfh->GetProcess(), guest_rfh->GetProcess());
  } else {
    EXPECT_EQ(ff_rfh->GetProcess(), guest_rfh->GetProcess());
  }

  // Add a second fenced frame (same-site with the first fenced frame).
  auto* ff_rfh_2 = fenced_frame_test_helper().CreateFencedFrame(
      guest_rfh, ff_rfh->GetLastCommittedURL());
  EXPECT_NE(ff_rfh_2->GetSiteInstance(), ff_rfh->GetSiteInstance());
  EXPECT_EQ(ff_rfh->GetProcess(), ff_rfh_2->GetProcess());
}

class WebViewUsbTest : public WebViewTest {
 public:
  WebViewUsbTest() = default;
  ~WebViewUsbTest() override = default;

  void SetUpOnMainThread() override {
    WebViewTest::SetUpOnMainThread();
    fake_device_info_ = device_manager_.CreateAndAddDevice(
        0, 0, "Test Manufacturer", "Test Device", "123456");
    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
    device_manager_.AddReceiver(
        device_manager.InitWithNewPipeAndPassReceiver());
    UsbChooserContextFactory::GetForProfile(browser()->profile())
        ->SetDeviceManagerForTesting(std::move(device_manager));

    test_content_browser_client_.SetAsBrowserClient();
  }

  void TearDownOnMainThread() override {
    test_content_browser_client_.UnsetAsBrowserClient();
    WebViewTest::TearDownOnMainThread();
  }

  void UseFakeChooser() {
    test_content_browser_client_.delegate().UseFakeChooser();
  }

 private:
  device::FakeUsbDeviceManager device_manager_;
  device::mojom::UsbDeviceInfoPtr fake_device_info_;
  TestUsbContentBrowserClient test_content_browser_client_;
};

IN_PROC_BROWSER_TEST_F(WebViewUsbTest, Shim_TestCannotRequestUsb) {
  TestHelper("testCannotRequestUsb", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewUsbTest, Shim_TestCannotReuseUsbPairedInTab) {
  // We start the test server here, instead of in TestHelper, because we need
  // to know the origin used in both the tab and webview before running the rest
  // of the test.
  ASSERT_TRUE(StartEmbeddedTestServer());

  const GURL url = embedded_test_server()->GetURL("localhost", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  UseFakeChooser();
  // Request permission to access the fake device in the tab. The fake chooser
  // will automatically select the item representing the fake device, granting
  // the permission.
  EXPECT_EQ("123456", EvalJs(tab_web_contents,
                             R"((async () => {
        let device =
            await navigator.usb.requestDevice({filters: []});
        return device.serialNumber;
      })())"));
  EXPECT_EQ(content::ListValueOf("123456"), EvalJs(tab_web_contents,
                                                   R"((async () => {
        let devices = await navigator.usb.getDevices();
        return devices.map(device => device.serialNumber);
      })())"));

  // Have the embedder create a webview which navigates to the same origin and
  // attempts to use the paired device. The granted permission should not be
  // available for that context.
  TestHelper("testCannotReuseUsbPairedInTab", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestCannotRequestFonts) {
  TestHelper("testCannotRequestFonts", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestCannotRequestFontsGrantedInTab) {
  // We start the test server here, instead of in TestHelper, because we need
  // to know the origin used in both the tab and webview before running the rest
  // of the test.
  ASSERT_TRUE(StartEmbeddedTestServer());

  const GURL url = embedded_test_server()->GetURL("localhost", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Grant access to fonts from a tab.
  permissions::MockPermissionPromptFactory tab_prompt_factory(
      permissions::PermissionRequestManager::FromWebContents(tab_web_contents));
  tab_prompt_factory.set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);
  EXPECT_TRUE(content::ExecJs(tab_web_contents,
                              R"((async () => {
        await window.queryLocalFonts();
      })())"));

  // Have the embedder create a webview which navigates to the same origin and
  // attempts to access fonts. The granted permission should not be
  // available for that context.
  TestHelper("testCannotRequestFonts", "web_view/shim", NO_TEST_SERVER);
}

class WebViewSerialTest : public WebViewTest {
 public:
  void SetUpOnMainThread() override {
    WebViewTest::SetUpOnMainThread();
    mojo::PendingRemote<device::mojom::SerialPortManager> port_manager;
    port_manager_.AddReceiver(port_manager.InitWithNewPipeAndPassReceiver());
    context()->SetPortManagerForTesting(std::move(port_manager));
  }

  void TearDownOnMainThread() override { WebViewTest::TearDownOnMainThread(); }

  device::FakeSerialPortManager& port_manager() { return port_manager_; }
  SerialChooserContext* context() {
    return SerialChooserContextFactory::GetForProfile(browser()->profile());
  }

  void CreatePortAndGrantPermissionToOrigin(const url::Origin& origin) {
    // Create port and grant permission to it.
    auto port = device::mojom::SerialPortInfo::New();
    port->token = base::UnguessableToken::Create();
    context()->GrantPortPermission(origin, *port);
    port_manager().AddPort(std::move(port));
  }

 private:
  device::FakeSerialPortManager port_manager_;
};

IN_PROC_BROWSER_TEST_F(WebViewSerialTest,
                       Shim_TestEnabledInTabButNotInWebView) {
  // We start the test server here, instead of in TestHelper, because we need
  // to know the origin used in both the tab and webview.
  ASSERT_TRUE(StartEmbeddedTestServer());

  const GURL url = embedded_test_server()->GetURL("localhost", "/title1.html");
  url::Origin origin = url::Origin::Create(url);
  CreatePortAndGrantPermissionToOrigin(origin);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Test that serial works in a tab.
  EXPECT_EQ(1, EvalJs(tab_web_contents,
                      R"(
(async () => {
  const ports = await navigator.serial.getPorts().then(ports => ports.length);
  return ports;
})();
      )"));

  // Have the embedder create a webview which navigates to the same origin and
  // attempts to use serial.
  TestHelper("testSerialDisabled", "web_view/shim", NO_TEST_SERVER);
}

class WebViewBluetoothTest : public WebViewTest {
 public:
  void SetUpOnMainThread() override {
    WebViewTest::SetUpOnMainThread();
    // Hook up the test bluetooth delegate.
    SetFakeBlueboothAdapter();
    old_browser_client_ = content::SetBrowserClientForTesting(&browser_client_);
  }

  void TearDownOnMainThread() override {
    content::SetBrowserClientForTesting(old_browser_client_);
    WebViewTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets up the blink runtime feature for accessing to navigator.bluetooth.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    WebViewTest::SetUpCommandLine(command_line);
  }

  void SetFakeBlueboothAdapter() {
    adapter_ = new FakeBluetoothAdapter();
    EXPECT_CALL(*adapter_, IsPresent()).WillRepeatedly(Return(true));
    EXPECT_CALL(*adapter_, IsPowered()).WillRepeatedly(Return(true));
    content::SetBluetoothAdapter(adapter_);
  }

  void AddFakeDevice(const std::string& device_address) {
    const device::BluetoothUUID kHeartRateUUID(kHeartRateUUIDString);
    auto fake_device =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            adapter_.get(), /*bluetooth_class=*/0u, kFakeBluetoothDeviceName,
            device_address,
            /*paired=*/true,
            /*connected=*/true);
    fake_device->AddUUID(kHeartRateUUID);
    fake_device->AddMockService(
        std::make_unique<testing::NiceMock<device::MockBluetoothGattService>>(
            fake_device.get(), kHeartRateUUIDString, kHeartRateUUID,
            /*is_primary=*/true));
    adapter_->AddMockDevice(std::move(fake_device));
  }

  void SetDeviceToSelect(const std::string& device_address) {
    browser_client_.bluetooth_delegate()->SetDeviceToSelect(device_address);
  }

 private:
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  BluetoothTestContentBrowserClient browser_client_;
  raw_ptr<content::ContentBrowserClient> old_browser_client_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(WebViewBluetoothTest,
                       Shim_TestEnabledInTabButNotInWebView) {
  // We start the test server here, instead of in TestHelper, because we
  // need to know the origin used in the tab.
  ASSERT_TRUE(StartEmbeddedTestServer());

  const GURL url = embedded_test_server()->GetURL("localhost", "/title1.html");
  url::Origin origin = url::Origin::Create(url);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  AddFakeDevice(kDeviceAddress);
  SetDeviceToSelect(kDeviceAddress);

  // Test that Bluetooth works in a tab.
  constexpr char kBluetoothTestScript[] = R"(
(async () => {
  try {
    const device = await navigator.bluetooth.requestDevice({
      filters: [{services: ['heart_rate']}]
    });
    return device.name;
  } catch (e) {
    return e.name + ': ' + e.message;
  }
})();
  )";
  EXPECT_EQ(kFakeBluetoothDeviceName,
            EvalJs(tab_web_contents, kBluetoothTestScript));

  // Have the embedder create a webview which navigates to the same origin
  // and attempts to use Bluetooth.
  TestHelper("testBluetoothDisabled", "web_view/shim", NO_TEST_SERVER);
}

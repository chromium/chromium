// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace {

std::string GetErrorCodePermissionDenied() {
  return base::NumberToString(
      static_cast<int>(device::mojom::GeopositionErrorCode::kPermissionDenied));
}

std::string RunScript(content::RenderFrameHost* render_frame_host,
                      const std::string& script) {
  return content::EvalJs(render_frame_host, script).ExtractString();
}

// IFrameLoader ---------------------------------------------------------------

// Used to block until an iframe is loaded via a javascript call.
// Note: NavigateToURLBlockUntilNavigationsComplete doesn't seem to work for
// multiple embedded iframes, as notifications seem to be 'batched'. Instead, we
// load and wait one single frame here by calling a javascript function.
class IFrameLoader : public content::WebContentsObserver {
 public:
  IFrameLoader(Browser* browser, int iframe_id, const GURL& url);

  IFrameLoader(const IFrameLoader&) = delete;
  IFrameLoader& operator=(const IFrameLoader&) = delete;

  ~IFrameLoader() override;

  // content::WebContentsObserver
  void DidStopLoading() override;
  void DomOperationResponse(content::RenderFrameHost* render_frame_host,
                            const std::string& json_string) override;

  const GURL& iframe_url() const { return iframe_url_; }

 private:
  // If true the navigation has completed.
  bool navigation_completed_;

  // If true the javascript call has completed.
  bool javascript_completed_;

  std::string javascript_response_;

  // The URL for the iframe we just loaded.
  GURL iframe_url_;
  base::RunLoop run_loop;
  base::OnceClosure quit_closure_;
};

IFrameLoader::IFrameLoader(Browser* browser, int iframe_id, const GURL& url)
    : navigation_completed_(false),
      javascript_completed_(false) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  content::WebContentsObserver::Observe(web_contents);
  std::string script(base::StringPrintf(
      "window.domAutomationController.send(addIFrame(%d, \"%s\"));",
      iframe_id, url.spec().c_str()));
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(script), base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);

  quit_closure_ = run_loop.QuitWhenIdleClosure();
  run_loop.Run();

  EXPECT_EQ(base::StringPrintf("\"%d\"", iframe_id), javascript_response_);
  content::WebContentsObserver::Observe(nullptr);
  // Now that we loaded the iframe, let's fetch its src.
  script = base::StringPrintf("getIFrameSrc(%d)", iframe_id);
  iframe_url_ = GURL(RunScript(web_contents->GetPrimaryMainFrame(), script));
}

IFrameLoader::~IFrameLoader() {
}

void IFrameLoader::DidStopLoading() {
  navigation_completed_ = true;
  if (javascript_completed_ && navigation_completed_)
    std::move(quit_closure_).Run();
}

void IFrameLoader::DomOperationResponse(
    content::RenderFrameHost* render_frame_host,
    const std::string& json_string) {
  javascript_response_ = json_string;
  javascript_completed_ = true;
  if (javascript_completed_ && navigation_completed_)
    std::move(quit_closure_).Run();
}

}  // namespace


// GeolocationBrowserTest -----------------------------------------------------

// This is a browser test for Geolocation.
// It exercises various integration points from javascript <-> browser:
// 1. The user is prompted when a position is requested from an unauthorized
//    origin.
// 2. Denying the request triggers the correct error callback.
// 3. Granting permission does not trigger an error, and allows a position to
//    be passed to javascript.
// 4. Permissions persisted in disk are respected.
// 5. Incognito profiles don't persist permissions on disk, but they do inherit
//    them from their regular parent profile.
class GeolocationBrowserTest : public InProcessBrowserTest {
 public:
  enum InitializationOptions {
    // The default profile and browser window will be used.
    INITIALIZATION_DEFAULT,

    // An incognito profile and browser window will be used.
    INITIALIZATION_OFFTHERECORD,

    // A new tab will be created using the default profile and browser window.
    INITIALIZATION_NEWTAB,
  };

  GeolocationBrowserTest();

  GeolocationBrowserTest(const GeolocationBrowserTest&) = delete;
  GeolocationBrowserTest& operator=(const GeolocationBrowserTest&) = delete;

  ~GeolocationBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownInProcessBrowserTestFixture() override;

  Browser* current_browser() { return current_browser_; }
  void set_html_for_tests(const std::string& html_for_tests) {
    html_for_tests_ = html_for_tests;
  }
  std::string html_for_tests() { return html_for_tests_; }
  const GURL& iframe_url(size_t i) const { return iframe_urls_[i]; }
  double fake_latitude() const { return fake_latitude_; }
  double fake_longitude() const { return fake_longitude_; }

  GURL GetTestURL() const {
    // Return the current test url for the top level page.
    return https_test_server_.GetURL(html_for_tests_);
  }

  GURL GetTestURLForHostname(std::string hostname) const {
    // Return the current test url for the top level page.
    return https_test_server_.GetURL(hostname, html_for_tests_);
  }

  content::WebContents* web_contents() {
    return current_browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Initializes the test server and navigates to `target`
  void Initialize(InitializationOptions options, GURL target);

  // Initializes the test server and navigates to the return value of GetTestUrl
  void Initialize(InitializationOptions options);

  // Loads two iframes with different origins: http://127.0.0.1 and
  // http://localhost.
  void LoadIFrames();

  // Specifies which frame to use for executing JavaScript.
  void SetFrameForScriptExecution(const std::string& frame_name);

  // Gets the HostContentSettingsMap for the current profile.
  HostContentSettingsMap* GetHostContentSettingsMap();

  // Calls watchPosition in JavaScript and accepts or denies the resulting
  // permission request. Returns |true| if the expected behavior happened.
  [[nodiscard]] bool WatchPositionAndGrantPermission();
  [[nodiscard]] bool WatchPositionAndDenyPermission();

  // Calls watchPosition in JavaScript and observes whether the permission
  // request is shown without interacting with it. Callers should set
  // |request_should_display| to |true| if they expect a request to display.
  void WatchPositionAndObservePermissionRequest(bool request_should_display);

  // Checks that no errors have been received in JavaScript, and checks that the
  // position most recently received matches |latitude| and |longitude|.
  void ExpectPosition(double latitude, double longitude);

  // Executes |function| in |render_frame_host| and checks that the return value
  // matches |expected|.
  void ExpectValueFromScriptForFrame(
      const std::string& expected,
      const std::string& function,
      content::RenderFrameHost* render_frame_host);

  // Executes |function| and checks that the return value matches |expected|.
  void ExpectValueFromScript(const std::string& expected,
                             const std::string& function);

  // Sets a new (second) position and runs all callbacks currently registered
  // with the Geolocation system. Returns |true| if the new position is updated
  // successfully in JavaScript.
  bool SetPositionAndWaitUntilUpdated(double latitude, double longitude);

 protected:
  // BrowserTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // The values used for the position override.
  double fake_latitude_ = 1.23;
  double fake_longitude_ = 4.56;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;

  // The current Browser as set in Initialize. May be for an incognito profile.
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> current_browser_ = nullptr;

  // The https server used for the tests
  net::EmbeddedTestServer https_test_server_{
      net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  // Calls watchPosition() in JavaScript and accepts or denies the resulting
  // permission request. Returns the JavaScript response.
  std::string WatchPositionAndRespondToPermissionRequest(
      permissions::PermissionRequestManager::AutoResponseType request_response);

  // The embedded test server handle.
  net::test_server::EmbeddedTestServerHandle test_server_handle_;

  // The path element of a URL referencing the html content for this test.
  std::string html_for_tests_ = "/geolocation/simple.html";

  // The frame where the JavaScript calls will run.
  raw_ptr<content::RenderFrameHost, AcrossTasksDanglingUntriaged>
      render_frame_host_ = nullptr;

  // The urls for the iframes loaded by LoadIFrames.
  std::vector<GURL> iframe_urls_;
};

// WebContentImpl tries to connect Device Service earlier than
// of SetUpOnMainThread(), so create the |geolocation_overrider_| here.
GeolocationBrowserTest::GeolocationBrowserTest()
    : geolocation_overrider_(
          std::make_unique<device::ScopedGeolocationOverrider>(
              fake_latitude_,
              fake_longitude_)) {}

void GeolocationBrowserTest::SetUpOnMainThread() {
  current_browser_ = browser();
  host_resolver()->AddRule("*", "127.0.0.1");
  https_test_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_test_server_.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(test_server_handle_ = https_test_server_.StartAndReturnHandle());
}

void GeolocationBrowserTest::TearDownInProcessBrowserTestFixture() {
  LOG(WARNING) << "TearDownInProcessBrowserTestFixture. Test Finished.";
}

void GeolocationBrowserTest::Initialize(InitializationOptions options) {
  Initialize(options, GetTestURL());
}

void GeolocationBrowserTest::Initialize(InitializationOptions options,
                                        GURL target) {
  if (options == INITIALIZATION_OFFTHERECORD) {
    current_browser_ = OpenURLOffTheRecord(browser()->profile(), target);
  } else {
    current_browser_ = browser();
    if (options == INITIALIZATION_NEWTAB)
      chrome::NewTab(current_browser_);
  }
  ASSERT_TRUE(current_browser_);
  if (options != INITIALIZATION_OFFTHERECORD)
    ASSERT_TRUE(ui_test_utils::NavigateToURL(current_browser_, target));

  // By default the main frame is used for JavaScript execution.
  SetFrameForScriptExecution("");
}

void GeolocationBrowserTest::LoadIFrames() {
  int number_iframes = 2;
  iframe_urls_.resize(number_iframes);
  for (int i = 0; i < number_iframes; ++i) {
    IFrameLoader loader(current_browser_, i, GURL());
    iframe_urls_[i] = loader.iframe_url();
  }
}

void GeolocationBrowserTest::SetFrameForScriptExecution(
    const std::string& frame_name) {
  render_frame_host_ = nullptr;

  if (frame_name.empty()) {
    render_frame_host_ = web_contents()->GetPrimaryMainFrame();
  } else {
    render_frame_host_ = content::FrameMatchingPredicate(
        web_contents()->GetPrimaryPage(),
        base::BindRepeating(&content::FrameMatchesName, frame_name));
  }
  DCHECK(render_frame_host_);
}

HostContentSettingsMap* GeolocationBrowserTest::GetHostContentSettingsMap() {
  return HostContentSettingsMapFactory::GetForProfile(browser()->profile());
}

bool GeolocationBrowserTest::WatchPositionAndGrantPermission() {
  std::string result = WatchPositionAndRespondToPermissionRequest(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  return "request-callback-success" == result;
}

bool GeolocationBrowserTest::WatchPositionAndDenyPermission() {
  std::string result = WatchPositionAndRespondToPermissionRequest(
      permissions::PermissionRequestManager::DENY_ALL);
  return "request-callback-error" == result;
}

std::string GeolocationBrowserTest::WatchPositionAndRespondToPermissionRequest(
    permissions::PermissionRequestManager::AutoResponseType request_response) {
  permissions::PermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(request_response);
  return RunScript(render_frame_host_, "geoStartWithAsyncResponse()");
}

void GeolocationBrowserTest::WatchPositionAndObservePermissionRequest(
    bool request_should_display) {
  permissions::PermissionRequestObserver observer(web_contents());
  if (request_should_display) {
    // Control will return as soon as the API call is made, and then the
    // observer will wait for the request to display.
    RunScript(render_frame_host_, "geoStartWithSyncResponse()");
    observer.Wait();
  } else {
    // Control will return once one of the callbacks fires.
    RunScript(render_frame_host_, "geoStartWithAsyncResponse()");
  }
  EXPECT_EQ(request_should_display, observer.request_shown());
}

void GeolocationBrowserTest::ExpectPosition(double latitude, double longitude) {
  // Checks we have no error.
  ExpectValueFromScript("0", "geoGetLastError()");
  ExpectValueFromScript(base::NumberToString(latitude),
                        "geoGetLastPositionLatitude()");
  ExpectValueFromScript(base::NumberToString(longitude),
                        "geoGetLastPositionLongitude()");
}

void GeolocationBrowserTest::ExpectValueFromScriptForFrame(
    const std::string& expected,
    const std::string& function,
    content::RenderFrameHost* render_frame_host) {
  EXPECT_EQ(expected, RunScript(render_frame_host, function));
}

void GeolocationBrowserTest::ExpectValueFromScript(
    const std::string& expected,
    const std::string& function) {
  ExpectValueFromScriptForFrame(expected, function, render_frame_host_);
}

bool GeolocationBrowserTest::SetPositionAndWaitUntilUpdated(double latitude,
                                                            double longitude) {
  fake_latitude_ = latitude;
  fake_longitude_ = longitude;

  geolocation_overrider_->UpdateLocation(fake_latitude_, fake_longitude_);

  return content::EvalJs(render_frame_host_, "geopositionUpdates.pop();")
             .ExtractString() == "geoposition-updated";
}

void GeolocationBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  // For using an HTTPS server.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kIgnoreCertificateErrors);
}

// Tests ----------------------------------------------------------------------

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, DisplaysPrompt) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  ASSERT_TRUE(WatchPositionAndGrantPermission());

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetHostContentSettingsMap()->GetContentSetting(
                GetTestURL(), GetTestURL(), ContentSettingsType::GEOLOCATION));

  // Ensure a second request doesn't create a prompt in this tab.
  WatchPositionAndObservePermissionRequest(false);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, Geoposition) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, ErrorOnPermissionDenied) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  EXPECT_TRUE(WatchPositionAndDenyPermission());
  ExpectValueFromScript(GetErrorCodePermissionDenied(), "geoGetLastError()");

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetHostContentSettingsMap()->GetContentSetting(
                GetTestURL(), GetTestURL(), ContentSettingsType::GEOLOCATION));

  // Ensure a second request doesn't create a prompt in this tab.
  WatchPositionAndObservePermissionRequest(false);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoPromptForSecondTab) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  ASSERT_TRUE(WatchPositionAndGrantPermission());

  // Checks request is not needed in a second tab.
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_NEWTAB));
  WatchPositionAndObservePermissionRequest(false);
  ExpectPosition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoPromptForDeniedOrigin) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      GetTestURL(), GetTestURL(), ContentSettingsType::GEOLOCATION,
      CONTENT_SETTING_BLOCK);

  // Check that the request wasn't shown but we get an error for this origin.
  WatchPositionAndObservePermissionRequest(false);
  ExpectValueFromScript(GetErrorCodePermissionDenied(), "geoGetLastError()");

  // Checks prompt will not be created a second tab.
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_NEWTAB));
  WatchPositionAndObservePermissionRequest(false);
  ExpectValueFromScript(GetErrorCodePermissionDenied(), "geoGetLastError()");
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoPromptForAllowedOrigin) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      GetTestURL(), GetTestURL(), ContentSettingsType::GEOLOCATION,
      CONTENT_SETTING_ALLOW);
  // The request is not shown, there is no error, and the position gets to the
  // script.
  WatchPositionAndObservePermissionRequest(false);
  ExpectPosition(fake_latitude(), fake_longitude());
}

// Crashes on Win only.  http://crbug.com/1014506
#if BUILDFLAG(IS_WIN)
#define MAYBE_PromptForOffTheRecord DISABLED_PromptForOffTheRecord
#else
#define MAYBE_PromptForOffTheRecord PromptForOffTheRecord
#endif

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, MAYBE_PromptForOffTheRecord) {
  // For a regular profile the user is prompted, and when granted the position
  // gets to the script.
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());

  // The permission from the regular profile is not inherited because it is more
  // permissive than the initial default for geolocation. This prevents
  // identifying information to be sent to a server without explicit consent by
  // the user.
  // Go incognito, and check that the user is prompted again and when granted,
  // the position gets to the script.
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_OFFTHERECORD));
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoLeakFromOffTheRecord) {
  // The user is prompted in a fresh incognito profile, and when granted the
  // position gets to the script.
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_OFFTHERECORD));
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());

  // The regular profile knows nothing of what happened in incognito. It is
  // prompted and when granted the position gets to the script.
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, IFramesWithCachedPosition) {
  set_html_for_tests("/geolocation/two_iframes.html");
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  LoadIFrames();

  // Grant permission in the first frame, the position gets to the script.
  SetFrameForScriptExecution("iframe_0");
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());

  // Refresh the position, but let's not yet create the watch on the second
  // frame so that it'll fetch from cache.
  double cached_position_latitude = 5.67;
  double cached_position_lognitude = 8.09;
  ASSERT_TRUE(SetPositionAndWaitUntilUpdated(cached_position_latitude,
                                             cached_position_lognitude));
  ExpectPosition(cached_position_latitude, cached_position_lognitude);

  // Now check the second frame gets cached values as well.
  SetFrameForScriptExecution("iframe_1");
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(cached_position_latitude, cached_position_lognitude);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, InvalidUrlRequest) {
  // Tests that an invalid URL (e.g. from a popup window) is rejected
  // correctly. Also acts as a regression test for http://crbug.com/40478
  set_html_for_tests("/geolocation/invalid_request_url.html");
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));

  content::WebContents* original_tab = web_contents();
  ExpectValueFromScript(GetErrorCodePermissionDenied(),
                        "requestGeolocationFromInvalidUrl()");
  ExpectValueFromScriptForFrame("1", "isAlive()",
                                original_tab->GetPrimaryMainFrame());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoPromptBeforeStart) {
  // See http://crbug.com/42789
  set_html_for_tests("/geolocation/two_iframes.html");
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  LoadIFrames();

  // In the second iframe, access the navigator.geolocation object, but don't
  // call any methods yet so it won't request permission yet.
  SetFrameForScriptExecution("iframe_1");
  ExpectValueFromScript("object", "geoAccessNavigatorGeolocation()");

  // In the first iframe, call watchPosition, grant permission, and verify that
  // the position gets to the script.
  SetFrameForScriptExecution("iframe_0");
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());

  // Back to the second frame. The user is prompted now (it has a different
  // origin). When permission is granted the position gets to the script.
  SetFrameForScriptExecution("iframe_1");
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, TwoWatchesInOneFrame) {
  set_html_for_tests("/geolocation/two_watches.html");
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));

  // Tell the script what to expect as the final coordinates.
  double final_position_latitude = 3.17;
  double final_position_longitude = 4.23;
  std::string script =
      base::StringPrintf("geoSetFinalPosition(%f, %f)", final_position_latitude,
                         final_position_longitude);
  ExpectValueFromScript("ok", script);

  // Request permission and set two watches for the initial success callback.
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());

  // The second watch will now have cancelled. Ensure an update still makes
  // its way through to the first watcher.
  ASSERT_TRUE(SetPositionAndWaitUntilUpdated(final_position_latitude,
                                             final_position_longitude));
  ExpectPosition(final_position_latitude, final_position_longitude);
}

// TODO(felt): Disabled because the second permission request hangs.
IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, DISABLED_PendingChildFrames) {
  set_html_for_tests("/geolocation/two_iframes.html");
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  LoadIFrames();

  SetFrameForScriptExecution("iframe_0");
  WatchPositionAndObservePermissionRequest(true);

  SetFrameForScriptExecution("iframe_1");
  WatchPositionAndObservePermissionRequest(true);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, TabDestroyed) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  WatchPositionAndObservePermissionRequest(true);

  // TODO(mvanouwerkerk): Can't close a window you did not open. Maybe this was
  // valid when the test was written, but now it just prints "Scripts may close
  // only the windows that were opened by it."
  std::string script = "window.domAutomationController.send(window.close())";
  ASSERT_TRUE(content::ExecJs(web_contents(), script));
}

class GeolocationPrerenderBrowserTest : public GeolocationBrowserTest {
 public:
  GeolocationPrerenderBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&GeolocationPrerenderBrowserTest::web_contents,
                                base::Unretained(this))) {}
  ~GeolocationPrerenderBrowserTest() override = default;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(&https_test_server_);
    GeolocationBrowserTest::SetUp();
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(GeolocationPrerenderBrowserTest,
                       DeferredBeforePrerenderActivation) {
  // Navigate to an initial page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      current_browser(), https_test_server_.GetURL("/empty.html")));

  // Start a prerender with the geolocation test URL.
  content::FrameTreeNodeId host_id =
      prerender_helper_.AddPrerender(GetTestURL());
  content::test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                          host_id);
  ASSERT_TRUE(prerender_helper_.GetHostForUrl(GetTestURL()));

  permissions::PermissionRequestObserver observer(web_contents());
  content::RenderFrameHost* prerender_rfh =
      prerender_helper_.GetPrerenderedMainFrameHost(host_id);
  // Execute script on the prerendered page and wait to ensure that the Mojo
  // capability control defers the binding of blink::mojom::GeolocationService
  // during prerendering.
  ExecuteScriptAsync(prerender_rfh, "geoStartWithAsyncResponse()");
  base::RunLoop().RunUntilIdle();

  // The prerendered page shouldn't show up a permission request's bubble.
  EXPECT_FALSE(observer.request_shown());

  prerender_helper_.NavigatePrimaryPage(GetTestURL());
  // Make sure that the prerender was activated.
  ASSERT_TRUE(prerender_observer.was_activated());

  // The activated page should create GeolocationService and show up the
  // permission request's bubble.
  observer.Wait();
  EXPECT_TRUE(observer.request_shown());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest,
                       GrantToDenyStopsGeolocationWatch) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      GetTestURL(), GetTestURL(), ContentSettingsType::GEOLOCATION,
      CONTENT_SETTING_BLOCK);

  EXPECT_TRUE(SetPositionAndWaitUntilUpdated(1, 2));
  ExpectValueFromScript(GetErrorCodePermissionDenied(), "geoGetLastError()");
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest,
                       GrantToRevokeStopsGeolocationWatch) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      GetTestURL(), GetTestURL(), ContentSettingsType::GEOLOCATION,
      CONTENT_SETTING_ASK);

  EXPECT_TRUE(SetPositionAndWaitUntilUpdated(1, 2));
  ExpectValueFromScript(GetErrorCodePermissionDenied(), "geoGetLastError()");
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest,
                       GrantToDenyToGrantDoesNotRemainBlocked) {
  // https://crbug.com/1475743
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      GetTestURL(), GetTestURL(), ContentSettingsType::GEOLOCATION,
      CONTENT_SETTING_BLOCK);

  EXPECT_TRUE(SetPositionAndWaitUntilUpdated(1, 2));
  ExpectValueFromScript(GetErrorCodePermissionDenied(), "geoGetLastError()");

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      GetTestURL(), GetTestURL(), ContentSettingsType::GEOLOCATION,
      CONTENT_SETTING_ALLOW);
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest,
                       ToggleToDenyDoesNotLeakCrossOrigin) {
  GURL a_test_gurl = GetTestURLForHostname("a.test");
  GURL b_test_gurl = GetTestURLForHostname("b.test");

  // Open a.test and allow geolocation.
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT, a_test_gurl));
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());

  // Toggle to deny on a.test
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      a_test_gurl, a_test_gurl, ContentSettingsType::GEOLOCATION,
      CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(SetPositionAndWaitUntilUpdated(1, 2));
  ExpectValueFromScript(GetErrorCodePermissionDenied(), "geoGetLastError()");

  // Navigate to b.test, allow geolocation and verify we can access it.
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT, b_test_gurl));
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest,
                       ToggleToDenyDoesNotOverrideGrantOnOtherOrigin) {
  GURL a_test_gurl = GetTestURLForHostname("a.test");
  GURL b_test_gurl = GetTestURLForHostname("b.test");

  // Set up geolocation as allowed on b.test
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      b_test_gurl, b_test_gurl, ContentSettingsType::GEOLOCATION,
      CONTENT_SETTING_ALLOW);

  // Open a.test and allow geolocation.
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT, a_test_gurl));
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());

  // Toggle grant to block.
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      a_test_gurl, a_test_gurl, ContentSettingsType::GEOLOCATION,
      CONTENT_SETTING_BLOCK);

  EXPECT_TRUE(SetPositionAndWaitUntilUpdated(1, 2));
  ExpectValueFromScript(GetErrorCodePermissionDenied(), "geoGetLastError()");

  // Navigate to b.test which has geolocation enabled
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT, b_test_gurl));

  // Ensure no prompt is shown on geolocation access and expect position.
  WatchPositionAndObservePermissionRequest(/*request_should_display=*/false);
  ExpectPosition(fake_latitude(), fake_longitude());
}

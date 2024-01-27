// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/service_worker_apitest.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_transport_simple_test_server.h"
#include "extensions/browser/api/web_request/extension_web_request_event_router.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/base/net_errors.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"

namespace controlled_frame {

namespace {

constexpr char kWebRequestOnBeforeRequestEventName[] =
    "webViewInternal.onBeforeRequest";
constexpr char kWebRequestOnAuthRequiredEventName[] =
    "webViewInternal.onAuthRequired";
constexpr char kEvalSuccessStr[] = "SUCCESS";

const extensions::MenuItem::Id CreateMenuItemId(
    const extensions::MenuItem::ExtensionKey& extension_key,
    const std::string& string_uid) {
  extensions::MenuItem::Id id;
  id.extension_key = extension_key;
  id.string_uid = string_uid;
  return id;
}

const content::EvalJsResult CreateContextMenuItem(
    content::WebContents* app_contents,
    const std::string& id,
    const std::string& title) {
  return content::EvalJs(app_contents, content::JsReplace(R"(
      (async function() {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame || !frame.contextMenus || !frame.contextMenus.create) {
          return 'FAIL: frame, frame.contextMenus, or ' +
              'frame.contextMenus.create is undefined';
        }
        return await new Promise((resolve, reject) => {
          frame.contextMenus.create(
              { title: $2, id: $1 },
              () => { resolve('SUCCESS'); });
        });
      })();
    )",
                                                          id, title));
}

const content::EvalJsResult UpdateContextMenuItemTitle(
    content::WebContents* app_contents,
    const std::string& id,
    const std::string& new_title) {
  return content::EvalJs(app_contents, content::JsReplace(R"(
    (async function() {
      const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame || !frame.contextMenus || !frame.contextMenus.update) {
          return 'FAIL: frame, frame.contextMenus, or ' +
              'frame.contextMenus.update is undefined';
        }

      return await new Promise((resolve, reject) =>{
        frame.contextMenus.update(
            /*id=*/$1,
            { title: $2 },
            () => { resolve('SUCCESS'); });
      });
    })();
  )",
                                                          id, new_title));
}

const content::EvalJsResult RemoveContextMenuItem(
    content::WebContents* app_contents,
    const std::string& id) {
  return content::EvalJs(app_contents, content::JsReplace(R"(
    (async function() {
      const frame = document.getElementsByTagName('controlledframe')[0];
      if (!frame || !frame.contextMenus || !frame.contextMenus.remove) {
        return 'FAIL: frame, frame.contextMenus, or ' +
            'frame.contextMenus.remove is undefined';
      }

      return await new Promise((resolve, reject) =>{
        frame.contextMenus.remove(
            /*id=*/$1,
            () => { resolve('SUCCESS'); });
      });
    })();
  )",
                                                          id));
}

const content::EvalJsResult RemoveAllContextMenuItems(
    content::WebContents* app_contents) {
  return content::EvalJs(app_contents, R"(
    (async function() {
      const frame = document.getElementsByTagName('controlledframe')[0];
      if (!frame || !frame.contextMenus || !frame.contextMenus.removeAll) {
        return 'FAIL: frame, frame.contextMenus, or ' +
            'frame.contextMenus.removeAll is undefined';
      }

      return await new Promise((resolve, reject) =>{
        frame.contextMenus.removeAll(() => { resolve('SUCCESS'); });
      });
    })();
  )");
}

const content::EvalJsResult SetBackgroundColorToWhite(
    content::WebContents* app_contents) {
  return content::EvalJs(app_contents, content::JsReplace(
                                           R"(
    (function() {
      document.body.style.backgroundColor = 'white';
      return 'SUCCESS';
    })();
  )"));
}

const content::EvalJsResult ExecuteScriptRedBackgroundCode(
    content::WebContents* app_contents) {
  return content::EvalJs(app_contents, content::JsReplace(
                                           R"(
    (async function() {
      return await new Promise((resolve, reject) => {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame || !frame.request) {
          reject('FAIL');
        }
        frame.executeScript(
          {code: "document.body.style.backgroundColor = 'red';"},
          () => { resolve('SUCCESS') });
      });
    })();
  )"));
}

const content::EvalJsResult ExecuteScriptRedBackgroundFile(
    content::WebContents* app_contents) {
  return content::EvalJs(app_contents, content::JsReplace(
                                           R"(
    (async function() {
      return await new Promise((resolve, reject) => {
        const frame = document.getElementsByTagName('controlledframe')[0];
        if (!frame || !frame.request) {
          reject('FAIL');
        }
        frame.executeScript(
          {file: "/execute_script.input.js"},
          () => { resolve('SUCCESS') });
      });
    })();
  )"));
}

const content::EvalJsResult VerifyBackgroundColorIsRed(
    content::WebContents* app_contents) {
  return content::EvalJs(app_contents, content::JsReplace(
                                           R"(
    (function() {
      if (document.body.style.backgroundColor === 'red') {
        return 'SUCCESS';
      } else {
        return 'FAIL';
      }
    })();
  )"));
}

}  // namespace

class ControlledFrameApiTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  ControlledFrameApiTest() {
    isolated_web_app_dev_server_ =
        CreateAndStartServer(FILE_PATH_LITERAL("web_apps/simple_isolated_app"));
  }

  void SetUpOnMainThread() override {
    web_app::IsolatedWebAppUrlInfo url_info = InstallDevModeProxyIsolatedWebApp(
        isolated_web_app_dev_server().GetOrigin());
    Browser* app_browser = LaunchWebAppBrowserAndWait(url_info.app_id());
    app_contents_ = app_browser->tab_strip_model()->GetActiveWebContents();
  }

  void TearDownOnMainThread() override { app_contents_ = nullptr; }

  [[nodiscard]] bool CreateControlledFrame(content::WebContents* web_contents,
                                           const GURL& src) {
    static std::string kCreateControlledFrame = R"(
      (async function() {
        const controlledframe = document.createElement('controlledframe');
        controlledframe.setAttribute('src', $1);
        await new Promise((resolve, reject) => {
          controlledframe.addEventListener('loadstop', resolve);
          controlledframe.addEventListener('loadabort', reject);
          document.body.appendChild(controlledframe);
        });
      })();
    )";
    return ExecJs(web_contents,
                  content::JsReplace(kCreateControlledFrame, src));
  }

  extensions::WebViewGuest* GetWebViewGuest(
      content::WebContents* embedder_web_contents) {
    auto inner_web_contents = embedder_web_contents->GetInnerWebContents();
    if (inner_web_contents.size() == 0) {
      return nullptr;
    }
    content::WebContents* guest_web_contents = inner_web_contents[0];
    auto* web_view_guest = extensions::WebViewGuest::FromRenderFrameHost(
        guest_web_contents->GetPrimaryMainFrame());
    return web_view_guest;
  }

  void ExpectMenuItemWithIdAndTitle(
      const extensions::MenuItem::ExtensionKey& extension_key,
      const std::string& expected_id,
      const std::string& expected_title) {
    auto* menu_manager = extensions::MenuManager::Get(browser_context());
    extensions::MenuItem* menu_item =
        menu_manager->GetItemById(CreateMenuItemId(extension_key, expected_id));

    ASSERT_TRUE(menu_item);
    EXPECT_EQ(expected_title, menu_item->title());
  }

  const net::EmbeddedTestServer& isolated_web_app_dev_server() {
    return *isolated_web_app_dev_server_.get();
  }

  content::WebContents* app_contents() { return app_contents_; }

  content::BrowserContext* browser_context() {
    return app_contents_->GetBrowserContext();
  }

 private:
  raw_ptr<content::WebContents> app_contents_;
  std::unique_ptr<net::EmbeddedTestServer> isolated_web_app_dev_server_;
};

IN_PROC_BROWSER_TEST_F(ControlledFrameApiTest, ContextMenusCreate) {
  const GURL& kOriginalControlledFrameUrl =
      isolated_web_app_dev_server().GetURL("/controlled_frame.html");
  ASSERT_TRUE(
      CreateControlledFrame(app_contents(), kOriginalControlledFrameUrl));
  extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_contents());
  auto* menu_manager = extensions::MenuManager::Get(browser_context());

  const extensions::MenuItem::ExtensionKey extension_key(
      /*extension_id=*/"", web_view_guest->owner_rfh()->GetProcess()->GetID(),
      web_view_guest->owner_rfh()->GetRoutingID(),
      web_view_guest->view_instance_id());
  EXPECT_EQ(0u, menu_manager->MenuItemsSize(extension_key));

  static constexpr std::string kItem1ID = "1";
  static constexpr std::string kItem1Title = "Test";
  EXPECT_EQ(kEvalSuccessStr,
            CreateContextMenuItem(app_contents(), kItem1ID, kItem1Title));
  ASSERT_EQ(1u, menu_manager->MenuItemsSize(extension_key));
  ExpectMenuItemWithIdAndTitle(extension_key, kItem1ID, kItem1Title);

  static constexpr std::string kItem2ID = "2";
  static constexpr std::string kItem2Title = "Test2";
  EXPECT_EQ(kEvalSuccessStr,
            CreateContextMenuItem(app_contents(), kItem2ID, kItem2Title));
  ASSERT_EQ(2u, menu_manager->MenuItemsSize(extension_key));
  ExpectMenuItemWithIdAndTitle(extension_key, kItem2ID, kItem2Title);

  static constexpr std::string kItem3ID = "3";
  static constexpr std::string kItem3Title = "Test3";
  EXPECT_EQ(kEvalSuccessStr,
            CreateContextMenuItem(app_contents(), kItem3ID, kItem3Title));
  ASSERT_EQ(3u, menu_manager->MenuItemsSize(extension_key));
  ExpectMenuItemWithIdAndTitle(extension_key, kItem3ID, kItem3Title);
}

IN_PROC_BROWSER_TEST_F(ControlledFrameApiTest, ContextMenusUpdate) {
  const GURL& kOriginalControlledFrameUrl =
      isolated_web_app_dev_server().GetURL("/controlled_frame.html");
  ASSERT_TRUE(
      CreateControlledFrame(app_contents(), kOriginalControlledFrameUrl));
  extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_contents());
  auto* menu_manager = extensions::MenuManager::Get(browser_context());

  static constexpr std::string kItem1ID = "1";
  static constexpr std::string kItem1Title = "Test";
  EXPECT_EQ(kEvalSuccessStr,
            CreateContextMenuItem(app_contents(), kItem1ID, kItem1Title));

  const extensions::MenuItem::ExtensionKey extension_key(
      /*extension_id=*/"", web_view_guest->owner_rfh()->GetProcess()->GetID(),
      web_view_guest->owner_rfh()->GetRoutingID(),
      web_view_guest->view_instance_id());
  ASSERT_EQ(1u, menu_manager->MenuItemsSize(extension_key));
  ExpectMenuItemWithIdAndTitle(extension_key, kItem1ID, kItem1Title);

  static constexpr std::string kItem1NewTitle = "Test1";
  EXPECT_EQ(kEvalSuccessStr, UpdateContextMenuItemTitle(
                                 app_contents(), kItem1ID, kItem1NewTitle));

  ASSERT_EQ(1u, menu_manager->MenuItemsSize(extension_key));
  ExpectMenuItemWithIdAndTitle(extension_key, kItem1ID, kItem1NewTitle);
}

IN_PROC_BROWSER_TEST_F(ControlledFrameApiTest, ContextMenusRemove) {
  const GURL& kOriginalControlledFrameUrl =
      isolated_web_app_dev_server().GetURL("/controlled_frame.html");
  ASSERT_TRUE(
      CreateControlledFrame(app_contents(), kOriginalControlledFrameUrl));
  extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_contents());
  auto* menu_manager = extensions::MenuManager::Get(browser_context());

  static constexpr std::string kItem1ID = "1";
  static constexpr std::string kItem1Title = "Test1";
  EXPECT_EQ(kEvalSuccessStr,
            CreateContextMenuItem(app_contents(), kItem1ID, kItem1Title));
  EXPECT_EQ(kEvalSuccessStr, CreateContextMenuItem(app_contents(), /*id=*/"2",
                                                   /*title=*/"Test2"));

  EXPECT_EQ(kEvalSuccessStr, RemoveContextMenuItem(app_contents(), kItem1ID));

  const extensions::MenuItem::ExtensionKey extension_key(
      /*extension_id=*/"", web_view_guest->owner_rfh()->GetProcess()->GetID(),
      web_view_guest->owner_rfh()->GetRoutingID(),
      web_view_guest->view_instance_id());
  ASSERT_EQ(1u, menu_manager->MenuItemsSize(extension_key));

  extensions::MenuItem* deleted_item =
      menu_manager->GetItemById(CreateMenuItemId(extension_key, kItem1ID));
  EXPECT_FALSE(deleted_item);
}

IN_PROC_BROWSER_TEST_F(ControlledFrameApiTest, ContextMenusRemoveAll) {
  const GURL& kOriginalControlledFrameUrl =
      isolated_web_app_dev_server().GetURL("/controlled_frame.html");
  ASSERT_TRUE(
      CreateControlledFrame(app_contents(), kOriginalControlledFrameUrl));
  extensions::WebViewGuest* web_view_guest = GetWebViewGuest(app_contents());
  auto* menu_manager = extensions::MenuManager::Get(browser_context());

  EXPECT_EQ(kEvalSuccessStr, CreateContextMenuItem(app_contents(), /*id=*/"1",
                                                   /*title=*/"Test1"));
  EXPECT_EQ(kEvalSuccessStr, CreateContextMenuItem(app_contents(), /*id=*/"2",
                                                   /*title=*/"Test2"));

  EXPECT_EQ(kEvalSuccessStr, RemoveAllContextMenuItems(app_contents()));

  const extensions::MenuItem::ExtensionKey extension_key(
      /*extension_id=*/"", web_view_guest->owner_rfh()->GetProcess()->GetID(),
      web_view_guest->owner_rfh()->GetRoutingID(),
      web_view_guest->view_instance_id());
  ASSERT_EQ(0u, menu_manager->MenuItemsSize(extension_key));
}

// This test checks if the Controlled Frame is able to intercept URL navigation
// requests.
IN_PROC_BROWSER_TEST_F(ControlledFrameApiTest, URLLoaderIsProxied) {
  const GURL& kOriginalControlledFrameUrl =
      isolated_web_app_dev_server().GetURL("/controlled_frame.html");
  ASSERT_TRUE(
      CreateControlledFrame(app_contents(), kOriginalControlledFrameUrl));

  auto* web_request_event_router =
      extensions::WebRequestEventRouter::Get(browser_context());
  EXPECT_EQ(0u, web_request_event_router->GetListenerCountForTesting(
                    browser_context(), kWebRequestOnBeforeRequestEventName));

  const std::string& kServerHostPort =
      isolated_web_app_dev_server().host_port_pair().ToString();
  EXPECT_EQ("SUCCESS", content::EvalJs(app_contents(),
                                       content::JsReplace(R"(
    (function() {
      const frame = document.getElementsByTagName('controlledframe')[0];
      if (!frame || !frame.request) {
        return 'FAIL: frame or frame.request is undefined';
      }
      frame.request.onBeforeRequest.addListener(() => {
        return { cancel: true };
      }, { urls: ['http://*/controlled_frame_cancel.html'] }, ['blocking']);
      frame.request.onBeforeRequest.addListener(() => {
        return { cancel: false };
      }, { urls: ['http://*/controlled_frame_success.html'] }, ['blocking']);
      frame.request.onBeforeRequest.addListener(() => {
        return {
          redirectUrl: 'http://' + $1 + '/controlled_frame_redirect_target.html'
        };
      }, { urls: ['http://*/controlled_frame_redirect.html'] }, ['blocking']);
      return 'SUCCESS';
    })();
  )",
                                                          kServerHostPort)));
  EXPECT_EQ(3u, web_request_event_router->GetListenerCountForTesting(
                    browser_context(), kWebRequestOnBeforeRequestEventName));

  auto* web_view_guest = GetWebViewGuest(app_contents());
  content::WebContents* guest_web_contents = web_view_guest->web_contents();

  // Check that navigations can be cancelled.
  {
    content::TestNavigationObserver navigation_observer(
        guest_web_contents, net::Error::ERR_BLOCKED_BY_CLIENT,
        content::MessageLoopRunner::QuitMode::IMMEDIATE,
        /*ignore_uncommitted_navigations=*/false);
    web_view_guest->NavigateGuest(isolated_web_app_dev_server()
                                      .GetURL("/controlled_frame_cancel.html")
                                      .spec(),
                                  /*force_navigation=*/false);
    navigation_observer.WaitForNavigationFinished();
    EXPECT_EQ(net::Error::ERR_BLOCKED_BY_CLIENT,
              navigation_observer.last_net_error_code());
    EXPECT_EQ(kOriginalControlledFrameUrl,
              guest_web_contents->GetLastCommittedURL());
    EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
  }

  // Check that navigations can be redirected.
  {
    content::TestNavigationObserver navigation_observer(
        guest_web_contents, /*expected_number_of_navigations=*/1u);
    web_view_guest->NavigateGuest(isolated_web_app_dev_server()
                                      .GetURL("/controlled_frame_redirect.html")
                                      .spec(),
                                  /*force_navigation=*/false);
    navigation_observer.WaitForNavigationFinished();
    EXPECT_EQ(isolated_web_app_dev_server().GetURL(
                  "/controlled_frame_redirect_target.html"),
              guest_web_contents->GetLastCommittedURL());
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  }

  // Check that navigations can succeed.
  {
    content::TestNavigationObserver navigation_observer(
        guest_web_contents, /*expected_number_of_navigations=*/1u);
    const GURL& kControlledFrameSuccessUrl =
        isolated_web_app_dev_server().GetURL("/controlled_frame_success.html");
    web_view_guest->NavigateGuest(kControlledFrameSuccessUrl.spec(),
                                  /*force_navigation=*/false);
    navigation_observer.WaitForNavigationFinished();
    EXPECT_EQ(kControlledFrameSuccessUrl,
              guest_web_contents->GetLastCommittedURL());
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  }
}

IN_PROC_BROWSER_TEST_F(ControlledFrameApiTest, AuthRequestIsProxied) {
  const GURL& kOriginalControlledFrameUrl =
      isolated_web_app_dev_server().GetURL("/controlled_frame.html");
  ASSERT_TRUE(
      CreateControlledFrame(app_contents(), kOriginalControlledFrameUrl));

  auto* web_request_event_router =
      extensions::WebRequestEventRouter::Get(browser_context());
  EXPECT_EQ(0u, web_request_event_router->GetListenerCountForTesting(
                    browser_context(), kWebRequestOnAuthRequiredEventName));

  const std::string& kServerHostPort =
      isolated_web_app_dev_server().host_port_pair().ToString();
  EXPECT_EQ(true, content::EvalJs(app_contents(), R"(
    (function() {
      const frame = document.getElementsByTagName('controlledframe')[0];
      if (!frame || !frame.request) {
        return false;
      }

      const expectedUsername = 'test';
      const expectedPassword = 'pass';
      frame.request.onAuthRequired.addListener(() => {
        return {
          authCredentials: {
            username: expectedUsername,
            password: expectedPassword
          }
        };
      }, { urls: [`http://*/auth-basic*`] }, ['blocking']);
      return true;
    })();
  )"));
  EXPECT_EQ(1u, web_request_event_router->GetListenerCountForTesting(
                    browser_context(), kWebRequestOnAuthRequiredEventName));

  auto* web_view_guest = GetWebViewGuest(app_contents());
  content::WebContents* guest_web_contents = web_view_guest->web_contents();

  // Check that the injecting the credentials through WebRequest produces a
  // successful navigation.
  {
    content::TestNavigationObserver navigation_observer(
        guest_web_contents,
        /*expected_number_of_navigations=*/1u);
    const GURL& kAuthBasicUrl =
        isolated_web_app_dev_server().GetURL("/auth-basic?password=pass");
    web_view_guest->NavigateGuest(kAuthBasicUrl.spec(),
                                  /*force_navigation=*/false);
    navigation_observer.WaitForNavigationFinished();
    EXPECT_EQ(kAuthBasicUrl, guest_web_contents->GetLastCommittedURL());
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  }

  // Check that the injecting the wrong credentials through WebRequest produces
  // an error.
  {
    content::TestNavigationObserver navigation_observer(
        guest_web_contents,
        /*expected_number_of_navigations=*/1u);
    const GURL& kAuthBasicUrl =
        isolated_web_app_dev_server().GetURL("/auth-basic?password=badpass");
    web_view_guest->NavigateGuest(kAuthBasicUrl.spec(),
                                  /*force_navigation=*/false);
    navigation_observer.WaitForNavigationFinished();
    EXPECT_EQ(kAuthBasicUrl, guest_web_contents->GetLastCommittedURL());
    // The auth request fails but keeps retrying until this error is produced.
    // TODO(https://crbug.com/1502580): The error produced here should be
    // authentication related.
    EXPECT_EQ(net::Error::ERR_TOO_MANY_RETRIES,
              navigation_observer.last_net_error_code());
    EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
  }
}

class ControlledFrameWebSocketApiTest : public ControlledFrameApiTest {
 public:
  ControlledFrameWebSocketApiTest() = default;
  ControlledFrameWebSocketApiTest(const ControlledFrameWebSocketApiTest&) =
      delete;
  ControlledFrameWebSocketApiTest& operator=(
      const ControlledFrameWebSocketApiTest&) = delete;

  void SetUpOnMainThread() override {
    ControlledFrameApiTest::SetUpOnMainThread();
    websocket_test_server_ = std::make_unique<net::SpawnedTestServer>(
        net::SpawnedTestServer::TYPE_WS, net::GetWebSocketTestDataDirectory());
    ASSERT_TRUE(websocket_test_server_->Start());
  }

  net::SpawnedTestServer* websocket_test_server() {
    return websocket_test_server_.get();
  }

  GURL GetWebSocketUrl(const std::string& path) {
    GURL::Replacements replacements;
    replacements.SetSchemeStr("ws");
    return websocket_test_server_->GetURL(path).ReplaceComponents(replacements);
  }

 private:
  std::unique_ptr<net::SpawnedTestServer> websocket_test_server_;
};

IN_PROC_BROWSER_TEST_F(ControlledFrameWebSocketApiTest, WebSocketIsProxied) {
  const GURL& kOriginalControlledFrameUrl =
      isolated_web_app_dev_server().GetURL("/controlled_frame.html");
  ASSERT_TRUE(
      CreateControlledFrame(app_contents(), kOriginalControlledFrameUrl));

  auto* web_request_event_router =
      extensions::WebRequestEventRouter::Get(browser_context());
  EXPECT_EQ(0u, web_request_event_router->GetListenerCountForTesting(
                    browser_context(), kWebRequestOnBeforeRequestEventName));

  // Use Web Sockets before installing a WebRequest event listener to verify
  // that it works inside of the Controlled Frame.
  auto* web_view_guest = GetWebViewGuest(app_contents());
  content::WebContents* guest_web_contents = web_view_guest->web_contents();
  GURL::Replacements http_scheme_replacement;
  http_scheme_replacement.SetSchemeStr("http");
  const GURL& kWebSocketConnectCheckUrl =
      websocket_test_server()
          ->GetURL("/connect_check.html")
          .ReplaceComponents(http_scheme_replacement);
  {
    content::TitleWatcher title_watcher(guest_web_contents, u"PASS");
    title_watcher.AlsoWaitForTitle(u"FAIL");
    content::TestNavigationObserver navigation_observer(
        guest_web_contents,
        /*expected_number_of_navigations=*/1u);
    web_view_guest->NavigateGuest(kWebSocketConnectCheckUrl.spec(),
                                  /*force_navigation=*/false);
    navigation_observer.WaitForNavigationFinished();
    EXPECT_EQ(kWebSocketConnectCheckUrl,
              guest_web_contents->GetLastCommittedURL());
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
    EXPECT_EQ(u"PASS", title_watcher.WaitAndGetTitle());
  }

  {
    content::TestNavigationObserver navigation_observer(
        guest_web_contents,
        /*expected_number_of_navigations=*/1u);
    web_view_guest->NavigateGuest(kOriginalControlledFrameUrl.spec(),
                                  /*force_navigation=*/false);
    navigation_observer.WaitForNavigationFinished();
    EXPECT_EQ(kOriginalControlledFrameUrl,
              guest_web_contents->GetLastCommittedURL());
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  }

  // Set up a WebRequest event listener that cancels any requests to the Web
  // Socket server.
  EXPECT_EQ(true, content::EvalJs(app_contents(),
                                  R"(
    (function() {
      const frame = document.getElementsByTagName('controlledframe')[0];
      if (!frame || !frame.request) {
        return false;
      }
      frame.request.onBeforeRequest.addListener(() => {
        return { cancel: true };
      }, { urls: ['ws://*/*'] }, ['blocking']);
      return true;
    })();
  )"));
  EXPECT_EQ(1u, web_request_event_router->GetListenerCountForTesting(
                    browser_context(), kWebRequestOnBeforeRequestEventName));
  {
    content::TitleWatcher title_watcher(guest_web_contents, u"PASS");
    title_watcher.AlsoWaitForTitle(u"FAIL");
    content::TestNavigationObserver navigation_observer(
        guest_web_contents,
        /*expected_number_of_navigations=*/1u);
    web_view_guest->NavigateGuest(kWebSocketConnectCheckUrl.spec(),
                                  /*force_navigation=*/false);
    navigation_observer.WaitForNavigationFinished();
    EXPECT_EQ(kWebSocketConnectCheckUrl,
              guest_web_contents->GetLastCommittedURL());
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
    EXPECT_EQ(u"FAIL", title_watcher.WaitAndGetTitle());
  }
}

class ControlledFrameWebTransportApiTest : public ControlledFrameApiTest {
 public:
  ControlledFrameWebTransportApiTest() { webtransport_server_.Start(); }

  ControlledFrameWebTransportApiTest(
      const ControlledFrameWebTransportApiTest&) = delete;
  ControlledFrameWebTransportApiTest& operator=(
      const ControlledFrameWebTransportApiTest&) = delete;
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ControlledFrameApiTest::SetUpCommandLine(command_line);
    webtransport_server_.SetUpCommandLine(command_line);
  }

  content::WebTransportSimpleTestServer& webtransport_server() {
    return webtransport_server_;
  }

 protected:
  content::WebTransportSimpleTestServer webtransport_server_;
};

IN_PROC_BROWSER_TEST_F(ControlledFrameWebTransportApiTest,
                       WebTransportIsProxied) {
  const GURL& kOriginalControlledFrameUrl =
      isolated_web_app_dev_server().GetURL("/controlled_frame.html");
  ASSERT_TRUE(
      CreateControlledFrame(app_contents(), kOriginalControlledFrameUrl));

  auto* web_request_event_router =
      extensions::WebRequestEventRouter::Get(browser_context());
  EXPECT_EQ(0u, web_request_event_router->GetListenerCountForTesting(
                    browser_context(), kWebRequestOnBeforeRequestEventName));

  // Use WebTransport before installing a WebRequest event listener to verify
  // that it works inside of the Controlled Frame.
  auto* web_view_guest = GetWebViewGuest(app_contents());
  content::WebContents* guest_web_contents = web_view_guest->web_contents();
  EXPECT_EQ(true, content::EvalJs(
                      guest_web_contents,
                      content::JsReplace(
                          R"(
    (async function() {
      const url = 'https://localhost:' + $1 + '/echo_test';
      try {
        const transport = new WebTransport(url);
        await transport.ready;
      } catch (e) {
        console.log(url + ': ' + e.name + ': ' + e.message);
        return false;
      }
      return true;
    })();
  )",
                          webtransport_server().server_address().port())));

  // Set up a WebRequest event listener that cancels any requests to the
  // WebTransport server.
  EXPECT_EQ(true, content::EvalJs(app_contents(),
                                  R"(
    let cancelRequest = false;
    (function() {
      const frame = document.getElementsByTagName('controlledframe')[0];
      if (!frame || !frame.request) {
        return false;
      }
      const onBeforeRequestHandler =
      frame.request.onBeforeRequest.addListener(() => {
        return { cancel: true };
      }, { urls: ['https://localhost/*'] }, ['blocking']);
      return true;
    })();
  )"));
  EXPECT_EQ(1u, web_request_event_router->GetListenerCountForTesting(
                    browser_context(), kWebRequestOnBeforeRequestEventName));

  EXPECT_EQ(false, content::EvalJs(
                       guest_web_contents,
                       content::JsReplace(
                           R"(
    (async function() {
      cancelRequest = true;
      const url = 'https://localhost:' + $1 + '/echo_test';
      try {
        const transport = new WebTransport(url);
        await transport.ready;
      } catch (e) {
        console.log(url + ': ' + e.name + ': ' + e.message);
        return false;
      }
      return true;
    })();
  )",
                           webtransport_server().server_address().port())));
}

class ControlledFrameServiceWorkerTest
    : public extensions::ServiceWorkerBasedBackgroundTest {
 public:
  ControlledFrameServiceWorkerTest(const ControlledFrameServiceWorkerTest&) =
      delete;
  ControlledFrameServiceWorkerTest& operator=(
      const ControlledFrameServiceWorkerTest&) = delete;

 protected:
  ControlledFrameServiceWorkerTest() {
    feature_list.InitWithFeatures(
        /*enabled_features=*/{features::kIsolatedWebApps,
                              features::kIsolatedWebAppDevMode},
        /*disabled_features=*/{});
  }

  ~ControlledFrameServiceWorkerTest() override = default;

  base::test::ScopedFeatureList feature_list;
};

// This test ensures that loading an extension Service Worker does not cause a
// crash, and that Controlled Frame is not allowed in the Service Worker
// context. For more details, see https://crbug.com/1462384.
// This test is the same as ServiceWorkerBasedBackgroundTest.Basic.
IN_PROC_BROWSER_TEST_F(ControlledFrameServiceWorkerTest, PRE_Basic) {
  ExtensionTestMessageListener newtab_listener("CREATED");
  newtab_listener.set_failure_message("CREATE_FAILED");
  ExtensionTestMessageListener worker_listener("WORKER_RUNNING");
  worker_listener.set_failure_message("NON_WORKER_SCOPE");
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "service_worker/worker_based_background/basic"));
  ASSERT_TRUE(extension);
  const extensions::ExtensionId extension_id = extension->id();
  EXPECT_TRUE(worker_listener.WaitUntilSatisfied());

  const GURL url = embedded_test_server()->GetURL("/extensions/test_file.html");
  content::WebContents* new_web_contents =
      extensions::browsertest_util::AddTab(browser(), url);
  EXPECT_TRUE(new_web_contents);
  EXPECT_TRUE(newtab_listener.WaitUntilSatisfied());

  // Service Worker extension does not have ExtensionHost.
  EXPECT_FALSE(process_manager()->GetBackgroundHostForExtension(extension_id));
}

// After browser restarts, this test step ensures that opening a tab fires
// tabs.onCreated event listener to the extension without explicitly loading the
// extension. This is because the extension registered a listener before browser
// restarted in PRE_Basic.
IN_PROC_BROWSER_TEST_F(ControlledFrameServiceWorkerTest, Basic) {
  ExtensionTestMessageListener newtab_listener("CREATED");
  newtab_listener.set_failure_message("CREATE_FAILED");
  const GURL url = embedded_test_server()->GetURL("/extensions/test_file.html");
  content::WebContents* new_web_contents =
      extensions::browsertest_util::AddTab(browser(), url);
  EXPECT_TRUE(new_web_contents);
  EXPECT_TRUE(newtab_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ControlledFrameApiTest, ExecuteScript) {
  const GURL& kOriginalControlledFrameUrl =
      isolated_web_app_dev_server().GetURL("/controlled_frame.html");
  ASSERT_TRUE(
      CreateControlledFrame(app_contents(), kOriginalControlledFrameUrl));

  auto* web_view_guest = GetWebViewGuest(app_contents());
  content::WebContents* guest_web_contents = web_view_guest->web_contents();

  // Verify that executeScript() using JS code can change the background color.
  EXPECT_EQ(kEvalSuccessStr, SetBackgroundColorToWhite(guest_web_contents));
  EXPECT_EQ(kEvalSuccessStr, ExecuteScriptRedBackgroundCode(app_contents()));
  EXPECT_EQ(kEvalSuccessStr, VerifyBackgroundColorIsRed(guest_web_contents));

  // Verify that executeScript() using a JS file changes the background color.
  EXPECT_EQ(kEvalSuccessStr, SetBackgroundColorToWhite(guest_web_contents));
  EXPECT_EQ(kEvalSuccessStr, ExecuteScriptRedBackgroundFile(app_contents()));
  EXPECT_EQ(kEvalSuccessStr, VerifyBackgroundColorIsRed(guest_web_contents));
}

}  // namespace controlled_frame

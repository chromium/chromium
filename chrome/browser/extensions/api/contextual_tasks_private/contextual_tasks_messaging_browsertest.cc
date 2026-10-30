// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/common/logger.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_features.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::NiceMock;
using ::testing::Return;

namespace extensions {

class MockContextualTasksEligibilityManager
    : public contextual_tasks::ContextualTasksEligibilityManager {
 public:
  MockContextualTasksEligibilityManager(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      AimEligibilityService* aim_eligibility_service,
      bool is_eligible)
      : contextual_tasks::ContextualTasksEligibilityManager(
            pref_service,
            identity_manager,
            aim_eligibility_service),
        stub_is_eligible_(is_eligible) {
    MaybeNotifyEligibilityChanged();
  }
  ~MockContextualTasksEligibilityManager() override = default;

  bool IsEligibleWithoutIdentity() const override { return stub_is_eligible_; }
  bool CalculateEligibility() const override { return stub_is_eligible_; }

 private:
  bool stub_is_eligible_;
};

class ContextualTasksExtensionMessagingTest : public ExtensionApiTest {
 public:
  explicit ContextualTasksExtensionMessagingTest(
      const std::vector<base::test::FeatureRef>& enabled_features = {},
      const std::vector<base::test::FeatureRef>& disabled_features = {}) {
    std::vector<base::test::FeatureRef> enabled = {
        extensions_features::kApiContextualTasksPrivate,
        contextual_tasks::kContextualTasks,
        contextual_tasks::kContextualTasksRearchitecture};
    enabled.insert(enabled.end(), enabled_features.begin(),
                   enabled_features.end());
    feature_list_.InitWithFeatures(enabled, disabled_features);
    ComponentLoader::EnableBackgroundExtensionsForTesting();
    UseHttpsTestServer();

    net::EmbeddedTestServer::ServerCertificateConfig cert_config;
    cert_config.dns_names = {"google.com", "www.google.com", "sub.google.com",
                             "test.c.googlers.com", "example.com"};
    embedded_test_server()->SetSSLConfig(cert_config);
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        [](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url.starts_with("/search")) {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content("<html><body>Search Page</body></html>");
            response->set_content_type("text/html");
            return response;
          }
          return nullptr;
        }));
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    EXPECT_TRUE(embedded_test_server()->Start());
  }

  void SetUpOnMainThread() override { ExtensionApiTest::SetUpOnMainThread(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    int port = embedded_test_server()->port();
    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        base::StringPrintf("MAP * 127.0.0.1:%d, EXCLUDE localhost", port));
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    ExtensionApiTest::SetUpBrowserContextKeyedServices(context);

    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          Profile* profile = Profile::FromBrowserContext(context);
          auto mock_aim_service =
              std::make_unique<NiceMock<MockAimEligibilityService>>(
                  *profile->GetPrefs(), /*template_url_service=*/nullptr,
                  /*url_loader_factory=*/nullptr,
                  /*identity_manager=*/nullptr);
          ON_CALL(*mock_aim_service, IsAimEligible())
              .WillByDefault(Return(true));
          ON_CALL(*mock_aim_service, IsAimUrl(testing::_, testing::_))
              .WillByDefault(Return(true));
          return mock_aim_service;
        }));

    contextual_tasks::ContextualTasksUiServiceFactory::GetInstance()
        ->SetTestingFactory(
            context, base::BindRepeating([](content::BrowserContext* context)
                                             -> std::unique_ptr<KeyedService> {
              Profile* profile = Profile::FromBrowserContext(context);
              auto* aim_eligibility_service =
                  AimEligibilityServiceFactory::GetForProfile(profile);

              auto mock_eligibility_manager =
                  std::make_unique<MockContextualTasksEligibilityManager>(
                      /*pref_service=*/nullptr,
                      /*identity_manager=*/nullptr, aim_eligibility_service,
                      /*is_eligible=*/true);

              auto mock_ui_service = std::make_unique<
                  NiceMock<contextual_tasks::MockContextualTasksUiService>>(
                  profile,
                  /*service=*/nullptr,
                  /*identity_manager=*/nullptr, aim_eligibility_service,
                  std::move(mock_eligibility_manager),
                  /*cookie_synchronizer=*/nullptr);

              ON_CALL(*mock_ui_service, GetEligibilityManager())
                  .WillByDefault([service_ptr = mock_ui_service.get()]() {
                    return service_ptr
                        ->ContextualTasksUiService::GetEligibilityManager();
                  });
              ON_CALL(*mock_ui_service, IsAiUrl(testing::_))
                  .WillByDefault([service_ptr =
                                      mock_ui_service.get()](const GURL& url) {
                    return service_ptr->ContextualTasksUiService::IsAiUrl(url);
                  });
              ON_CALL(*mock_ui_service, IsSearchResultsUrl(testing::_))
                  .WillByDefault(Return(true));
              ON_CALL(*mock_ui_service, GetDefaultAiPageUrl())
                  .WillByDefault(Return(GURL("https://google.com/aim")));

              return std::unique_ptr<KeyedService>(std::move(mock_ui_service));
            }));
  }

  NiceMock<contextual_tasks::MockContextualTasksUiService>* GetMockUiService() {
    return static_cast<
        NiceMock<contextual_tasks::MockContextualTasksUiService>*>(
        contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
            profile()));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the Contextual Tasks component extension's background script
// correctly allows external messages from allowed origins and blocks
// unauthorized origins.
IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionMessagingTest,
                       ExternalConnectableAllowlist) {
  // Verify that the component extension is loaded.
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kContextualTasksExtensionId);
  ASSERT_TRUE(extension);

  net::EmbeddedTestServer http_server;
  http_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(http_server.Start());

  constexpr bool kAllowDebugOrigins = !version_info::IsOfficialBuild();

  struct {
    const std::string host_and_path;
    bool expected_to_connect;
  } test_cases[] = {
      // example.com is not allowed.
      {"https://example.com/search?q=foo", false},
      // google.com/search and *.google.com/search match manifest and allowlist.
      {"https://google.com/search?q=foo", true},
      {"https://sub.google.com/search?q=foo", true},
      // *.googlers.com/search matches manifest and allowlist.
      {"https://test.c.googlers.com/search?q=foo", true},
      // http://localhost and file:/// are allowed only in non-official builds.
      {http_server.GetURL("localhost", "/title1.html").spec(),
       kAllowDebugOrigins},
      {net::FilePathToFileURL(
           test_data_dir_.DirName().DirName().AppendASCII("title1.html"))
           .spec(),
       kAllowDebugOrigins},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.host_and_path);
    ASSERT_TRUE(
        NavigateToURL(GetActiveWebContents(), GURL(test_case.host_and_path)));

    // Try to send a message to the extension from the page.
    std::string script = base::StringPrintf(
        R"(
        (async () => {
          if (!chrome.runtime || !chrome.runtime.sendMessage) {
            return 'no_runtime';
          }
          return new Promise((resolve) => {
            chrome.runtime.sendMessage(
                '%s', {type: 'contextualTasksPrivate.getState'}, (response) => {
                  if (chrome.runtime.lastError) {
                    resolve(chrome.runtime.lastError.message);
                  } else if (response && response.error) {
                    resolve('error: ' + response.error);
                  } else {
                    resolve('success');
                  }
                });
          });
        })()
        )",
        extension_misc::kContextualTasksExtensionId);

    content::EvalJsResult result =
        content::EvalJs(GetActiveWebContents(), script);
    std::string result_string = result.ExtractString();

    // This test verifies that only allowlisted pages have access to external
    // messaging and pass the origin check in `background.ts`.
    if (test_case.expected_to_connect) {
      EXPECT_EQ("success", result_string);
    } else {
      EXPECT_EQ("no_runtime", result_string);
    }
  }
}

class ContextualTasksExtensionMessagingSearchQueryEnabledTest
    : public ContextualTasksExtensionMessagingTest {
 public:
  ContextualTasksExtensionMessagingSearchQueryEnabledTest()
      : ContextualTasksExtensionMessagingTest(
            /*enabled_features=*/{contextual_tasks::
                                      kContextualTasksSearchQuery},
            /*disabled_features=*/{}) {}
};

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionMessagingSearchQueryEnabledTest,
                       SendMessageWithQ) {
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kContextualTasksExtensionId);
  ASSERT_TRUE(extension);

  auto* mock_ui_service = GetMockUiService();

  EXPECT_CALL(
      *mock_ui_service,
      StartTaskUiInSidePanelImpl(
          testing::_, testing::_,
          testing::Property(&GURL::spec,
                            testing::AllOf(testing::HasSubstr("/aim"),
                                           testing::HasSubstr("q=some_query"),
                                           testing::HasSubstr("mstk=dummy"))),
          testing::_,
          testing::AllOf(
              testing::Field(
                  &contextual_tasks::StartTaskUiOptions::associate_web_contents,
                  false),
              testing::Field(&contextual_tasks::StartTaskUiOptions::
                                 use_mstk_for_task_association,
                             true),
              testing::Field(
                  &contextual_tasks::StartTaskUiOptions::use_no_animation,
                  false))))
      .Times(1);

  ASSERT_TRUE(
      NavigateToURL(GetActiveWebContents(), GURL("https://google.com/search")));

  std::string script = base::StringPrintf(
      R"(
      (async () => {
        return new Promise((resolve) => {
          chrome.runtime.sendMessage(
              '%s', {
                type: 'contextualTasksPrivate.launchPanelInNewTab',
                args: {
                  aimParams: {
                    mstk: 'dummy',
                    q: 'some_query'
                  },
                  targetUrl: 'https://example.com'
                }
              }, (response) => {
                if (chrome.runtime.lastError) {
                  resolve(chrome.runtime.lastError.message);
                } else {
                  resolve('success');
                }
              });
        });
      })()
      )",
      extension_misc::kContextualTasksExtensionId);

  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), script));
}

class ContextualTasksExtensionMessagingSearchQueryDisabledTest
    : public ContextualTasksExtensionMessagingTest {
 public:
  ContextualTasksExtensionMessagingSearchQueryDisabledTest()
      : ContextualTasksExtensionMessagingTest(
            /*enabled_features=*/{},
            /*disabled_features=*/{
                contextual_tasks::kContextualTasksSearchQuery}) {}
};

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionMessagingSearchQueryDisabledTest,
                       SendMessageWithoutQ) {
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kContextualTasksExtensionId);
  ASSERT_TRUE(extension);

  auto* mock_ui_service = GetMockUiService();

  EXPECT_CALL(
      *mock_ui_service,
      StartTaskUiInSidePanelImpl(
          testing::_, testing::_,
          testing::Property(
              &GURL::spec,
              testing::AllOf(testing::HasSubstr("/aim"),
                             testing::Not(testing::HasSubstr("q=")),
                             testing::HasSubstr("mstk=dummy"))),
          testing::_,
          testing::AllOf(
              testing::Field(
                  &contextual_tasks::StartTaskUiOptions::associate_web_contents,
                  false),
              testing::Field(&contextual_tasks::StartTaskUiOptions::
                                 use_mstk_for_task_association,
                             true),
              testing::Field(
                  &contextual_tasks::StartTaskUiOptions::use_no_animation,
                  false))))
      .Times(1);

  ASSERT_TRUE(
      NavigateToURL(GetActiveWebContents(), GURL("https://google.com/search")));

  std::string script = base::StringPrintf(
      R"(
      (async () => {
        return new Promise((resolve) => {
          chrome.runtime.sendMessage(
              '%s', {
                type: 'contextualTasksPrivate.launchPanelInNewTab',
                args: {
                  aimParams: {
                    mstk: 'dummy'
                  },
                  targetUrl: 'https://example.com'
                }
              }, (response) => {
                if (chrome.runtime.lastError) {
                  resolve(chrome.runtime.lastError.message);
                } else {
                  resolve('success');
                }
              });
        });
      })()
      )",
      extension_misc::kContextualTasksExtensionId);

  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), script));
}

class ContextualTasksExtensionMessagingOnlyApiPrivateTest
    : public ContextualTasksExtensionMessagingTest {
 public:
  ContextualTasksExtensionMessagingOnlyApiPrivateTest()
      : ContextualTasksExtensionMessagingTest(
            /*enabled_features=*/{extensions_features::
                                      kApiContextualTasksPrivate},
            /*disabled_features=*/{
                contextual_tasks::kContextualTasksRearchitecture}) {}
};

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionMessagingOnlyApiPrivateTest,
                       GetStateReturnsValidStateObject) {
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kContextualTasksExtensionId);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(
      NavigateToURL(GetActiveWebContents(), GURL("https://google.com/search")));

  std::string script = base::StringPrintf(
      R"(
      (async () => {
        return new Promise((resolve) => {
          chrome.runtime.sendMessage(
              '%s', {type: 'contextualTasksPrivate.getState'}, (response) => {
                if (chrome.runtime.lastError) {
                  resolve('lastError: ' + chrome.runtime.lastError.message);
                } else if (
                    response && response.state &&
                    typeof response.state.isEligible === 'boolean') {
                  resolve('valid_state');
                } else {
                  resolve('invalid_response: ' + JSON.stringify(response));
                }
              });
        });
      })()
      )",
      extension_misc::kContextualTasksExtensionId);

  EXPECT_EQ("valid_state", content::EvalJs(GetActiveWebContents(), script));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionMessagingTest,
    ServiceWorkerPortHandshakeAndBidirectionalSearchMessage) {
  base::HistogramTester histogram_tester;
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kContextualTasksExtensionId);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(
      NavigateToURL(GetActiveWebContents(), GURL("https://google.com/search")));

  // Verify 0 iframes exist on the page.
  EXPECT_EQ(0, content::EvalJs(GetActiveWebContents(),
                               "document.querySelectorAll('iframe').length"));

  auto* user_data = contextual_tasks::ContextualTasksWebContentsUserData::
      GetOrCreateForWebContents(GetActiveWebContents());
  ASSERT_NE(user_data, nullptr);
  user_data->OnLensThumbnailCreated("data:image/png;base64,zero_iframe_crop");

  // Connect via chrome.runtime.connect(EXT_ID, {name:
  // 'chrome-search-channel'}), verify receipt of HandshakePing, reply with
  // HandshakeResponse (auth_user_index = 2), and wait for the subsequent
  // InjectChromeInput message over the Service Worker Port.
  std::string handshake_and_receive_script = base::StringPrintf(
      R"(
      (async () => {
        return new Promise((resolve) => {
          const port = chrome.runtime.connect(
              '%s', {name: 'chrome-search-channel'});
          window.__aimSearchPort = port;
          let gotHandshakePing = false;
          port.onMessage.addListener((msg) => {
            if (!msg || msg.type !== 'chrome-search-channel' ||
                !Array.isArray(msg.payload)) {
              return;
            }
            // HandshakePing is field 1 (tag 0x0a, length 0x00).
            if (!gotHandshakePing && msg.payload.length >= 2 &&
                msg.payload[0] === 0x0a) {
              gotHandshakePing = true;
              // Reply with SearchToClientMessage { handshake_response {
              // auth_user_index: 2 } } -> [0x0a, 0x02, 0x08, 0x02]
              port.postMessage({
                type: 'chrome-search-channel',
                payload: [0x0a, 0x02, 0x08, 0x02]
              });
              return;
            }
            // InjectChromeInput is field 2 (tag 0x12).
            if (gotHandshakePing && msg.payload.length >= 2 &&
                msg.payload[0] === 0x12) {
              resolve('got_inject_chrome_input');
            }
          });
        });
      })()
      )",
      extension_misc::kContextualTasksExtensionId);

  EXPECT_EQ(
      "got_inject_chrome_input",
      content::EvalJs(GetActiveWebContents(), handshake_and_receive_script));
  EXPECT_TRUE(user_data->IsHandshakeCompleteForTesting());
  ASSERT_NE(user_data->GetOrCreateContextualSessionHandle(), nullptr);
  EXPECT_EQ(2u,
            user_data->GetOrCreateContextualSessionHandle()->auth_user_index());
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.ExtensionHandshake.Status",
      contextual_tasks::ExtensionHandshakeStatus::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "ContextualTasks.ExtensionHandshake.Latency", 1);

  // Send a malformed message over the Service Worker Port and verify that the
  // Service Worker records kUnexpectedDataType via chrome.metricsPrivate.
  std::string invalid_message_script = R"(
    (() => {
      window.__aimSearchPort.postMessage({
        type: 'chrome-search-channel',
        payload: 'not-an-array'
      });
      return 'sent_invalid';
    })()
  )";
  EXPECT_EQ("sent_invalid",
            content::EvalJs(GetActiveWebContents(), invalid_message_script));

  // Verify AIM -> Chrome SearchToClientMessage.OnSubmitQueryRequest over the
  // Service Worker Port returns ClientToSearchMessage.OnSubmitQueryResponse
  // with 0 iframes mounted.
  std::string submit_query_script = R"(
    (async () => {
      return new Promise((resolve) => {
        window.__aimSearchPort.onMessage.addListener((msg) => {
          if (!msg || msg.type !== 'chrome-search-channel' ||
              !Array.isArray(msg.payload)) {
            return;
          }
          // OnSubmitQueryResponse is field 3 (tag 0x1a).
          if (msg.payload.length >= 2 && msg.payload[0] === 0x1a) {
            resolve('got_submit_response');
          }
        });
        // SearchToClientMessage { on_submit_query_request {} } -> [0x12, 0x00]
        window.__aimSearchPort.postMessage({
          type: 'chrome-search-channel',
          payload: [0x12, 0x00]
        });
      });
    })()
  )";
  content::SimulateMouseClick(GetActiveWebContents(), 0,
                              blink::WebMouseEvent::Button::kLeft);
  EXPECT_EQ("got_submit_response",
            content::EvalJs(GetActiveWebContents(), submit_query_script));
  histogram_tester.ExpectBucketCount(
      "ContextualTasks.ExtensionPostMessage.Error",
      contextual_tasks::ExtensionPostMessageError::kUnexpectedDataType, 1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionMessagingTest,
                       ServiceWorkerPortReconnectionAfterDisconnect) {
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kContextualTasksExtensionId);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(
      NavigateToURL(GetActiveWebContents(), GURL("https://google.com/search")));

  auto* user_data = contextual_tasks::ContextualTasksWebContentsUserData::
      GetOrCreateForWebContents(GetActiveWebContents());
  ASSERT_NE(user_data, nullptr);
  user_data->OnLensThumbnailCreated("data:image/png;base64,zero_iframe_crop");

  std::string reconnect_script = base::StringPrintf(
      R"(
      (async () => {
        function connectAndCompleteHandshake() {
          return new Promise((resolve) => {
            const port = chrome.runtime.connect(
                '%s', {name: 'chrome-search-channel'});
            let gotPing = false;
            port.onMessage.addListener((msg) => {
              if (msg && msg.type === 'chrome-search-channel' &&
                  Array.isArray(msg.payload)) {
                if (!gotPing && msg.payload.length >= 2 &&
                    msg.payload[0] === 0x0a) {
                  gotPing = true;
                  port.postMessage({
                    type: 'chrome-search-channel',
                    payload: [0x0a, 0x02, 0x08, 0x01]
                  });
                  return;
                }
                if (gotPing && msg.payload.length >= 2 &&
                    msg.payload[0] === 0x12) {
                  resolve(port);
                }
              }
            });
          });
        }
        const firstPort = await connectAndCompleteHandshake();
        firstPort.disconnect();
        const secondPort = await connectAndCompleteHandshake();
        window.__reconnectedPort = secondPort;
        return 'reconnected';
      })()
      )",
      extension_misc::kContextualTasksExtensionId);

  EXPECT_EQ("reconnected",
            content::EvalJs(GetActiveWebContents(), reconnect_script));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionMessagingTest,
                       ServiceWorkerPortSecondConnectBeforeFirstDisconnect) {
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kContextualTasksExtensionId);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(
      NavigateToURL(GetActiveWebContents(), GURL("https://google.com/search")));

  auto* user_data = contextual_tasks::ContextualTasksWebContentsUserData::
      GetOrCreateForWebContents(GetActiveWebContents());
  ASSERT_NE(user_data, nullptr);
  user_data->OnLensThumbnailCreated("data:image/png;base64,zero_iframe_crop");

  std::string connect_second_before_disconnect_script = base::StringPrintf(
      R"(
      (async () => {
        function connectAndCompleteHandshake() {
          return new Promise((resolve) => {
            const port = chrome.runtime.connect(
                '%s', {name: 'chrome-search-channel'});
            let gotPing = false;
            port.onMessage.addListener((msg) => {
              if (msg && msg.type === 'chrome-search-channel' &&
                  Array.isArray(msg.payload)) {
                if (!gotPing && msg.payload.length >= 2 &&
                    msg.payload[0] === 0x0a) {
                  gotPing = true;
                  port.postMessage({
                    type: 'chrome-search-channel',
                    payload: [0x0a, 0x02, 0x08, 0x01]
                  });
                  return;
                }
                if (gotPing && msg.payload.length >= 2 &&
                    msg.payload[0] === 0x12) {
                  resolve(port);
                }
              }
            });
          });
        }
        const firstPort = await connectAndCompleteHandshake();
        const secondPort = await connectAndCompleteHandshake();
        firstPort.disconnect();
        window.__secondSearchPort = secondPort;
        return 'second_connected';
      })()
      )",
      extension_misc::kContextualTasksExtensionId);

  EXPECT_EQ("second_connected",
            content::EvalJs(GetActiveWebContents(),
                            connect_second_before_disconnect_script));

  // Verify the second port remains active in portsByDocumentId after the
  // superseded first port disconnects.
  std::string submit_query_script = R"(
    (async () => {
      return new Promise((resolve) => {
        window.__secondSearchPort.onMessage.addListener((msg) => {
          if (!msg || msg.type !== 'chrome-search-channel' ||
              !Array.isArray(msg.payload)) {
            return;
          }
          // OnSubmitQueryResponse is field 3 (tag 0x1a).
          if (msg.payload.length >= 2 && msg.payload[0] === 0x1a) {
            resolve('got_submit_response');
          }
        });
        // SearchToClientMessage { on_submit_query_request {} } -> [0x12, 0x00]
        window.__secondSearchPort.postMessage({
          type: 'chrome-search-channel',
          payload: [0x12, 0x00]
        });
      });
    })()
  )";
  content::SimulateMouseClick(GetActiveWebContents(), 0,
                              blink::WebMouseEvent::Button::kLeft);
  EXPECT_EQ("got_submit_response",
            content::EvalJs(GetActiveWebContents(), submit_query_script));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionMessagingTest,
                       LogsServiceWorkerPortMessagesToOmniboxLogger) {
  struct CapturedLog {
    std::string tag;
    std::string message;
    std::optional<std::string> proto_type;
  };
  class TestLoggerObserver : public omnibox::Logger::Observer {
   public:
    TestLoggerObserver() {
      observation_.Observe(omnibox::Logger::GetInstance());
    }
    void OnLogMessageAdded(
        base::Time event_time,
        const std::string& tag,
        const std::string& source_file,
        uint32_t source_line,
        const std::string& message,
        const std::optional<std::string>& proto_type,
        const std::optional<std::string>& proto_base64) override {
      logs_.push_back({tag, message, proto_type});
    }
    const std::vector<CapturedLog>& logs() const { return logs_; }

   private:
    std::vector<CapturedLog> logs_;
    base::ScopedObservation<omnibox::Logger, omnibox::Logger::Observer>
        observation_{this};
  } logger_observer;

  ASSERT_TRUE(
      NavigateToURL(GetActiveWebContents(), GURL("https://google.com/search")));

  // Connect persistent Service Worker port: HandshakePing -> HandshakeResponse
  // -> InjectChromeInput -> OnSubmitQueryRequest -> OnSubmitQueryResponse.
  auto* user_data = contextual_tasks::ContextualTasksWebContentsUserData::
      GetOrCreateForWebContents(GetActiveWebContents());
  ASSERT_NE(user_data, nullptr);
  user_data->OnLensThumbnailCreated("data:image/png;base64,zero_iframe_crop");

  std::string port_exchange_script = base::StringPrintf(
      R"(
      (async () => {
        return new Promise((resolve) => {
          const port = chrome.runtime.connect(
              '%s', {name: 'chrome-search-channel'});
          window.__loggedPort = port;
          let gotPing = false;
          port.onMessage.addListener((msg) => {
            if (!msg || msg.type !== 'chrome-search-channel' ||
                !Array.isArray(msg.payload)) {
              return;
            }
            if (!gotPing && msg.payload[0] === 0x0a) {
              gotPing = true;
              port.postMessage({
                type: 'chrome-search-channel',
                payload: [0x0a, 0x02, 0x08, 0x01]
              });
              return;
            }
            if (gotPing && msg.payload[0] === 0x12) {
              resolve('ready');
            }
          });
        });
      })()
      )",
      extension_misc::kContextualTasksExtensionId);
  ASSERT_EQ("ready",
            content::EvalJs(GetActiveWebContents(), port_exchange_script));

  std::string submit_script = R"(
    (async () => {
      return new Promise((resolve) => {
        window.__loggedPort.onMessage.addListener((msg) => {
          if (msg && msg.type === 'chrome-search-channel' &&
              Array.isArray(msg.payload) && msg.payload[0] === 0x1a) {
            resolve('submitted');
          }
        });
        window.__loggedPort.postMessage({
          type: 'chrome-search-channel',
          payload: [0x12, 0x00]
        });
      });
    })()
  )";
  content::SimulateMouseClick(GetActiveWebContents(), 0,
                              blink::WebMouseEvent::Button::kLeft);
  ASSERT_EQ("submitted",
            content::EvalJs(GetActiveWebContents(), submit_script));

  auto has_log = [&](const std::string& tag, const std::string& substr,
                     std::optional<std::string> expected_proto = std::nullopt) {
    for (const auto& entry : logger_observer.logs()) {
      if (entry.tag == tag && entry.message.find(substr) != std::string::npos &&
          (!expected_proto.has_value() || entry.proto_type == expected_proto)) {
        return true;
      }
    }
    return false;
  };

  EXPECT_TRUE(has_log("PostServiceWorkerMessage", "HandshakePing",
                      "lens.chrome.ClientToAimMessage"));
  EXPECT_TRUE(has_log("OnServiceWorkerMessage", "HandshakeResponse",
                      "lens.chrome.SearchToClientMessage"));
  EXPECT_TRUE(has_log("PostServiceWorkerMessage", "InjectChromeInput",
                      "lens.chrome.ClientToSearchMessage"));
  EXPECT_TRUE(has_log("OnServiceWorkerMessage", "OnSubmitQueryRequest",
                      "lens.chrome.SearchToClientMessage"));
  EXPECT_TRUE(has_log("PostServiceWorkerMessage", "OnSubmitQueryResponse",
                      "lens.chrome.ClientToSearchMessage"));

  // Ensure none of the Service Worker messages log under legacy webview/iframe
  // tags.
  for (const auto& entry : logger_observer.logs()) {
    EXPECT_NE(entry.tag, "OnWebviewMessage");
    EXPECT_NE(entry.tag, "PostAimMessage");
  }
}

}  // namespace extensions

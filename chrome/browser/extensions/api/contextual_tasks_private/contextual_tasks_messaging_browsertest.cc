// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_features.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/network_switches.h"
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
    cert_config.dns_names = {"google.com", "www.google.com", "example.com"};
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
        base::StringPrintf("MAP * 127.0.0.1:%d", port));
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
// correctly allows external messages from allowed origins (google.com) and
// blocks unauthorized origins (example.com).
IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionMessagingTest,
                       ExternalConnectableAllowlist) {
  // Verify that the component extension is loaded.
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kContextualTasksExtensionId);
  ASSERT_TRUE(extension);

  struct {
    const std::string host_and_path;
    bool expected_to_connect;
  } test_cases[] = {
      // example.com is not allowed.
      {"https://example.com/search?q=foo", false},
      // google.com/search matches manifest and allowlist.
      {"https://google.com/search?q=foo", true},
  };

  for (const auto& test_case : test_cases) {
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

    // This test is only verifying that the correct pages have access to
    // extension functions rather than the actual result of
    // `contextualTasksPrivate.getState`.
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

}  // namespace extensions

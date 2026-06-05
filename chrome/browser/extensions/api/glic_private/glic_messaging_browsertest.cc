// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/api/glic_private/glic_private_api_test_base.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/tabs/tab_enums.h"
#endif

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/contextual_tasks/public/features.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_features.h"
#include "services/network/test/test_url_loader_factory.h"
#include "url/gurl.h"

namespace extensions {

class GlicMessagingBrowserTest : public GlicPrivateApiTestBase {
 public:
  GlicMessagingBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{contextual_tasks::kContextualTasks, {}},
         {extensions_features::kApiGlicPrivate, {}},
         {extensions_features::kApiGlicAccessFromGoogleWebpage, {}},
         {extensions_features::kApiGlicAccessFromPromotionPage, {}},
         {features::kGlicActor,
          {{"glic_actor_policy_control_exemption", "true"}}}},
        {});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};


namespace {

#if !BUILDFLAG(IS_ANDROID)
content::EvalJsResult ExecuteInvoke(content::WebContents* web_contents,
                                    const std::string& prompt_id,
                                    const std::string& invocation_source) {
  std::string script = base::StringPrintf(
      R"(
      (async () => {
        if (!chrome.runtime || !chrome.runtime.sendMessage) {
          return 'no_runtime';
        }
        return new Promise((resolve) => {
          chrome.runtime.sendMessage(
              '%s', {type: 'glicPrivate.invoke', args: {
                promptId: '%s',
                invocationSource: '%s'
              }}, (response) => {
                if (chrome.runtime.lastError) {
                  resolve(chrome.runtime.lastError.message);
                } else {
                  resolve('success');
                }
              });
        });
      })()
      )",
      extension_misc::kGlicExtensionId, prompt_id.c_str(),
      invocation_source.c_str());

  return content::EvalJs(web_contents, script);
}
#endif  // !BUILDFLAG(IS_ANDROID)

content::EvalJsResult ExecuteActivateTabWithConversation(
    content::WebContents* web_contents,
    const std::string& conversation_id) {
  std::string script = base::StringPrintf(
      R"(
      (async () => {
        return new Promise((resolve) => {
          chrome.runtime.sendMessage(
              '%s', {type: 'glicPrivate.activateTabWithConversation', args: {
                conversationId: '%s'
              }}, (response) => {
                if (chrome.runtime.lastError) {
                  resolve('error: ' + chrome.runtime.lastError.message);
                } else {
                  resolve('success');
                }
              });
        });
      })()
      )",
      extension_misc::kGlicExtensionId, conversation_id.c_str());

  return content::EvalJs(web_contents, script);
}

}  // namespace

// Tests that gemini.google.com can successfully send a getState message to the
// glic component extension over HTTPS, but example.com cannot.
IN_PROC_BROWSER_TEST_F(GlicMessagingBrowserTest, ExternalConnectable) {
  // The glic extension should be loaded as a component extension.
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kGlicExtensionId);
  ASSERT_TRUE(extension);

  struct {
    const std::string_view host;
    bool expected_to_connect;
  } test_cases[] = {
      // example.com is not in the externally_connectable.matches list.
      {"https://example.com/empty.html", false},
      // gemini.google.com is allowed over HTTPS in the manifest.
      {"https://gemini.google.com/empty.html", true},
  };

  for (const auto& test_case : test_cases) {
    ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), GURL(test_case.host)));

    // Try to send a message to the extension.
    std::string script = base::StringPrintf(
        R"(
        (async () => {
          if (!chrome.runtime || !chrome.runtime.sendMessage) {
            return 'no_runtime';
          }
          return new Promise((resolve) => {
            chrome.runtime.sendMessage(
                '%s', {type: 'glicPrivate.getState'}, (response) => {
                  if (chrome.runtime.lastError) {
                    resolve(chrome.runtime.lastError.message);
                  } else {
                    resolve('success');
                  }
                });
          });
        })()
        )",
        extension_misc::kGlicExtensionId);

    content::EvalJsResult result =
        content::EvalJs(GetActiveWebContents(), script);

    std::string result_string = result.ExtractString();
    if (test_case.expected_to_connect) {
      EXPECT_EQ("success", result_string)
          << "Expected success for " << test_case.host
          << " but got: " << result_string;
    } else {
      // It should fail with "Could not establish connection" or no runtime.
      EXPECT_TRUE(
          result_string == "no_runtime" ||
          result_string ==
              "Could not establish connection. Receiving end does not exist.")
          << "Unexpected result for " << test_case.host << ": "
          << result_string;
    }
  }
}

// Invoke is not supported in Android yet.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicMessagingBrowserTest, InvokeAPI) {
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kGlicExtensionId);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            GURL("https://gemini.google.com/empty.html")));

  content::EvalJsResult result =
      ExecuteInvoke(GetActiveWebContents(), "1", "universal-cart");

  std::string result_string = result.ExtractString();
  EXPECT_EQ("Uncaught Error: local-glic-not-enabled", result_string);
}


// Tests that the extension cannot query the state of a tab if the tab's URL
// indicates a different user account (e.g., authuser=1) than the one the
// extension is bound to. This is a security check to prevent cross-account
// data leakage.
IN_PROC_BROWSER_TEST_F(GlicMessagingBrowserTest, AccountMismatch) {
  Profile* test_profile = profile();
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(test_profile), "test@example.com",
      signin::ConsentLevel::kSignin);

  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kGlicExtensionId);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(
      NavigateToURL(GetActiveWebContents(),
                    GURL("https://gemini.google.com/empty.html?authuser=1")));

  // Wake up the service worker by sending a message.
  ExecuteInvoke(GetActiveWebContents(), "dummy", "dummy");

  content::RenderFrameHost* rfh = GetActiveWebContents()->GetPrimaryMainFrame();
  std::string document_id =
      ExtensionApiFrameIdMap::GetDocumentId(rfh).ToString();

  std::string script = base::StringPrintf(
      R"(
      (async () => {
        try {
          await chrome.glicPrivate.getState('%s');
          chrome.test.sendScriptResult('unexpected_success');
        } catch (e) {
          chrome.test.sendScriptResult(e.message);
        }
      })()
      )",
      document_id.c_str());

  base::Value result = BackgroundScriptExecutor::ExecuteScript(
      profile(), extension->id(), script,
      BackgroundScriptExecutor::ResultCapture::kSendScriptResult);

  EXPECT_TRUE(result.is_string());
  EXPECT_TRUE(result.GetString().find("local-account-mismatch") !=
              std::string::npos)
      << "Expected 'local-account-mismatch' but got: " << result.GetString();
}

class GlicMessagingWebViewGuestTest : public GlicMessagingBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    GlicMessagingBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&GlicMessagingWebViewGuestTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    GlicMessagingBrowserTest::SetUpOnMainThread();
    SetupIdentityAndCapabilities();
    IdentityTestEnvironmentProfileAdaptor adaptor(profile());
    adaptor.identity_test_env()->SetTestURLLoaderFactory(
        &test_url_loader_factory_);
    adaptor.identity_test_env()->SetCookieAccounts(
        {{.email = "test@example.com",
          .gaia_id = signin::GetTestGaiaIdForEmail("test@example.com")}});
  }

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    ChromeSigninClientFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                     &test_url_loader_factory_));
  }

  base::CallbackListSubscription create_services_subscription_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  glic::GlicTestEnvironment glic_test_environment_;
};

IN_PROC_BROWSER_TEST_F(GlicMessagingWebViewGuestTest, WebViewGuest) {
  // Ensure Glic component extension is loaded.
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kGlicExtensionId);
  ASSERT_TRUE(extension);

  // Set up the guest view manager factory.
  guest_view::TestGuestViewManagerFactory factory;
  auto* guest_manager = static_cast<guest_view::TestGuestViewManager*>(
      factory.GetOrCreateTestGuestViewManager(
          profile(),
          ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate()));
  ASSERT_TRUE(guest_manager);

  // Navigate the active tab to chrome://contextual-tasks/.
  GURL embedder_url(chrome::kChromeUIContextualTasksURL);
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), embedder_url));
  content::WebContents* embedder_contents = GetActiveWebContents();
  ASSERT_TRUE(embedder_contents);

  // Clear the Glic DOM and programmatically add a webview guest.
  std::string setup_webview_script = R"(
    (async () => {
      document.body.replaceChildren();
      const webview = document.createElement('webview');
      webview.setAttribute('src', 'https://gemini.google.com/empty.html');
      document.body.appendChild(webview);
      await new Promise((resolve) => {
        webview.addEventListener('loadstop', () => {
          resolve();
        }, {once: true});
      });
      return 'success';
    })()
  )";

  content::EvalJsResult setup_result =
      content::EvalJs(embedder_contents, setup_webview_script);
  ASSERT_EQ("success", setup_result.ExtractString());

  // Get the guest's RenderFrameHost.
  auto* guest_view = guest_manager->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view);
  guest_manager->WaitUntilAttached(guest_view);
  content::RenderFrameHost* guest_rfh = guest_view->GetGuestMainFrame();
  ASSERT_TRUE(guest_rfh);

  // Try to call the Glic private API by sending a message from the guest.
  std::string script = base::StringPrintf(
      R"(
      (async () => {
        if (!chrome.runtime || !chrome.runtime.sendMessage) {
          return 'no_runtime';
        }
        return new Promise((resolve) => {
          chrome.runtime.sendMessage(
              '%s', {type: 'glicPrivate.getState'}, (response) => {
                if (chrome.runtime.lastError) {
                  resolve(chrome.runtime.lastError.message);
                } else {
                  if (response && response.state) {
                    resolve(response.state.readyState);
                  } else {
                    resolve('invalid response: ' + JSON.stringify(response));
                  }
                }
              });
        });
      })()
      )",
      extension_misc::kGlicExtensionId);

  content::EvalJsResult result = content::EvalJs(guest_rfh, script);
  EXPECT_EQ("ready", result.ExtractString());
}

IN_PROC_BROWSER_TEST_F(GlicMessagingWebViewGuestTest, WebViewGuestInvoke) {
  // Ensure Glic component extension is loaded.
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kGlicExtensionId);
  ASSERT_TRUE(extension);

  auto interceptor = CreateMockPromptResponseInterceptor();

  // Set up the guest view manager factory.
  guest_view::TestGuestViewManagerFactory factory;
  auto* guest_manager = static_cast<guest_view::TestGuestViewManager*>(
      factory.GetOrCreateTestGuestViewManager(
          profile(),
          ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate()));
  ASSERT_TRUE(guest_manager);

  // Navigate to Contextual Tasks WebUI context.
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            GURL(chrome::kChromeUIContextualTasksURL)));
  content::WebContents* embedder_contents = GlicPrivateApiTestBase::browser()
                                                ->tab_strip_model()
                                                ->GetActiveWebContents();
  ASSERT_TRUE(embedder_contents);

  // Clear the Glic DOM and programmatically add a webview guest.
  std::string setup_webview_script = R"(
    (async () => {
      document.body.replaceChildren();
      const webview = document.createElement('webview');
      webview.src = 'https://gemini.google.com/empty.html';
      document.body.appendChild(webview);
      await new Promise((resolve) => {
        webview.addEventListener('loadstop', () => {
          resolve();
        }, {once: true});
      });
      return 'success';
    })()
  )";

  content::EvalJsResult setup_result =
      content::EvalJs(embedder_contents, setup_webview_script);
  ASSERT_EQ("success", setup_result.ExtractString());

  // Get the guest's RenderFrameHost.
  auto* guest_view = guest_manager->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view);
  guest_manager->WaitUntilAttached(guest_view);
  content::RenderFrameHost* guest_rfh = guest_view->GetGuestMainFrame();
  ASSERT_TRUE(guest_rfh);

  // Try to call the Glic private API by sending a message from the guest.
  std::string script = base::StringPrintf(
      R"(
      (async () => {
        if (!chrome.runtime || !chrome.runtime.sendMessage) {
          return 'no_runtime';
        }
        return new Promise((resolve) => {
          chrome.runtime.sendMessage(
              '%s', {type: 'glicPrivate.invoke', args: {
                promptId: 'TEST_PROMPT_ID',
                invocationSource: 'universal-cart'
              }}, (response) => {
                if (chrome.runtime.lastError) {
                  resolve(chrome.runtime.lastError.message);
                } else {
                  resolve('success');
                }
              });
        });
      })()
      )",
      extension_misc::kGlicExtensionId);

  content::EvalJsResult result = content::EvalJs(guest_rfh, script);
  EXPECT_EQ("success", result.ExtractString());
}
#endif  // !BUILDFLAG(IS_ANDROID)

class GlicMessagingFullyEnabledBrowserTest
    : public glic::GlicBrowserTestMixin<GlicMessagingBrowserTest> {
 public:
  void SetUpOnMainThread() override {
    GlicMessagingBrowserTest::SetUpOnMainThread();
    SetupIdentityAndCapabilities();
  }
};

IN_PROC_BROWSER_TEST_F(GlicMessagingFullyEnabledBrowserTest, HasConversation) {
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kGlicExtensionId);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            GURL("https://gemini.google.com/empty.html")));

  // Test hasConversation with valid args.
  {
    std::string script = base::StringPrintf(
        R"(
        (async () => {
          return new Promise((resolve) => {
            chrome.runtime.sendMessage(
                '%s', {type: 'glicPrivate.hasConversation', args: {
                  conversationId: 'test-conv-id'
                }}, (response) => {
                  if (chrome.runtime.lastError) {
                    resolve('error: ' + chrome.runtime.lastError.message);
                  } else {
                    resolve('success: ' + response.isPresent);
                  }
                });
          });
        })()
        )",
        extension_misc::kGlicExtensionId);

    content::EvalJsResult result =
        content::EvalJs(GetActiveWebContents(), script);
    EXPECT_EQ("success: false", result.ExtractString());
  }

  // Test hasConversation with conversation found.
  {
    ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
    RegisterConversation(instance, "test-conv-id-found");

    std::string script = base::StringPrintf(
        R"(
        (async () => {
          return new Promise((resolve) => {
            chrome.runtime.sendMessage(
                '%s', {type: 'glicPrivate.hasConversation', args: {
                  conversationId: 'test-conv-id-found'
                }}, (response) => {
                  if (chrome.runtime.lastError) {
                    resolve('error: ' + chrome.runtime.lastError.message);
                  } else {
                    resolve('success: ' + response.isPresent);
                  }
                });
          });
        })()
        )",
        extension_misc::kGlicExtensionId);

    content::EvalJsResult result =
        content::EvalJs(GetActiveWebContents(), script);
    EXPECT_EQ("success: true", result.ExtractString());
  }

  // Test missing arguments for hasConversation.
  {
    std::string script = base::StringPrintf(
        R"(
        (async () => {
          return new Promise((resolve) => {
            chrome.runtime.sendMessage(
                '%s', {type: 'glicPrivate.hasConversation'}, (response) => {
                  if (chrome.runtime.lastError) {
                    resolve('error: ' + chrome.runtime.lastError.message);
                  } else if (response && response.error) {
                    resolve('error: ' + response.error);
                  } else {
                    resolve('success');
                  }
                });
          });
        })()
        )",
        extension_misc::kGlicExtensionId);

    content::EvalJsResult result =
        content::EvalJs(GetActiveWebContents(), script);
    EXPECT_EQ("error: missing conversationId", result.ExtractString());
  }
}

IN_PROC_BROWSER_TEST_F(GlicMessagingFullyEnabledBrowserTest,
                       ActivateTabWithConversation_ConversationNotFound) {
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kGlicExtensionId);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            GURL("https://gemini.google.com/empty.html")));

  // Test activateTabWithConversation with non-existent conversation.
  {
    content::EvalJsResult result = ExecuteActivateTabWithConversation(
        GetActiveWebContents(), "test-conv-id");
    EXPECT_EQ("error: Uncaught Error: local-conversation-not-found",
              result.ExtractString());
  }

  // Test missing arguments for activateTabWithConversation.
  {
    std::string script = base::StringPrintf(
        R"(
        (async () => {
          return new Promise((resolve) => {
            chrome.runtime.sendMessage(
                '%s',
                {type: 'glicPrivate.activateTabWithConversation'},
                (response) => {
                  if (chrome.runtime.lastError) {
                    resolve('error: ' + chrome.runtime.lastError.message);
                  } else if (response && response.error) {
                    resolve('error: ' + response.error);
                  } else {
                    resolve('success');
                  }
                });
          });
        })()
        )",
        extension_misc::kGlicExtensionId);

    content::EvalJsResult result =
        content::EvalJs(GetActiveWebContents(), script);
    EXPECT_EQ("error: missing conversationId", result.ExtractString());
  }
}

IN_PROC_BROWSER_TEST_F(GlicMessagingFullyEnabledBrowserTest,
                       ActivateTabWithConversation_Success) {
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kGlicExtensionId);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            GURL("https://gemini.google.com/empty.html")));

  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  RegisterConversation(instance, "test-conv-id-success");

  // Open a second tab to make the first one inactive.
  tabs::TabInterface* second_tab =
      CreateAndActivateTab(GURL("https://gemini.google.com/empty.html"));
  ASSERT_NE(nullptr, second_tab);

  content::EvalJsResult result = ExecuteActivateTabWithConversation(
      second_tab->GetContents(), "test-conv-id-success");
  EXPECT_EQ("success", result.ExtractString());
}

// These tests rely on detaching the Glic panel (floaty/detached mode) or
// desktop-only concepts like tabs::TabModel, which are not supported or
// disabled on Android.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicMessagingFullyEnabledBrowserTest,
                       ActivateTabWithConversation_NoBoundTabs) {
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kGlicExtensionId);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            GURL("https://gemini.google.com/empty.html")));

  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_NE(nullptr, tab);
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTabAndDetach());
  instance->UnbindEmbedder(tab);
  RegisterConversation(instance, "test-conv-no-bound");

  content::EvalJsResult result = ExecuteActivateTabWithConversation(
      GetActiveWebContents(), "test-conv-no-bound");
  EXPECT_EQ("error: Uncaught Error: local-no-bound-tabs",
            result.ExtractString());
}

IN_PROC_BROWSER_TEST_F(GlicMessagingFullyEnabledBrowserTest,
                       ActivateTabWithConversation_TabNotInWindow) {
  const Extension* extension =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          extension_misc::kGlicExtensionId);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            GURL("https://gemini.google.com/empty.html")));

  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_NE(nullptr, tab);
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTabAndDetach());
  RegisterConversation(instance, "test-conv-tab-not-in-window");

  // Open a second tab to make sure there's another tab in the window.
  tabs::TabInterface* second_tab =
      CreateAndActivateTab(GURL("https://gemini.google.com/empty.html"));
  ASSERT_NE(nullptr, second_tab);

  // Detach the first tab. It's now not in any window.
  std::unique_ptr<tabs::TabModel> detached_tab =
      InProcessBrowserTest::browser()
          ->tab_strip_model()
          ->DetachTabAtForInsertion(0);
  ASSERT_NE(nullptr, detached_tab);

  content::EvalJsResult result = ExecuteActivateTabWithConversation(
      second_tab->GetContents(), "test-conv-tab-not-in-window");
  EXPECT_EQ("error: Uncaught Error: local-tab-not-in-window",
            result.ExtractString());

  // Re-insert the detached tab back to the browser's tab strip model so that
  // it is properly destroyed during teardown, notifying observers and
  // preventing dangling pointer warnings.
  InProcessBrowserTest::browser()->tab_strip_model()->InsertDetachedTabAt(
      0, std::move(detached_tab), AddTabTypes::ADD_NONE);
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace extensions

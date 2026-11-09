// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_tab_context/tab_context_decryption_token_tab_helper.h"

#include <string>

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/tab_context_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync_tab_context/container_id.h"
#include "components/sync_tab_context/fake_tab_context_sync_service.h"
#include "components/sync_tab_context/http_rpc_constants.h"
#include "components/sync_tab_context/tab_context_sync_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

constexpr char kMismatchGaiaId[] = "987654321";

class TabContextDecryptionTokenTabHelperBrowserTest
    : public InProcessBrowserTest {
 public:
  TabContextDecryptionTokenTabHelperBrowserTest() = default;

  void SetUp() override {
    https_server_.SetCertHostnames({"chromestorage.goog", "example.com"});
    ASSERT_TRUE(https_server_.InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        sync_tab_context::kTabContextAllowedOriginSwitch,
        https_server_.GetURL("chromestorage.goog", "/").spec());
    command_line->AppendSwitchASCII(
        switches::kIsolateOrigins,
        https_server_.GetURL("chromestorage.goog", "/").spec());
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &TabContextDecryptionTokenTabHelperBrowserTest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    TabContextSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              sync_tab_context::FakeTabContextSyncService>();
        }));
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    https_server_.StartAcceptingConnections();

    // Sign in primary account with fake GAIA ID.
    signin::IdentityManager* const identity_manager =
        IdentityManagerFactory::GetForProfile(GetProfile());
    const AccountInfo account_info = signin::MakePrimaryAccountAvailable(
        identity_manager, "user@gmail.com", signin::ConsentLevel::kSignin);
    primary_gaia_id_ = account_info.GetGaiaId().ToString();
  }

 protected:
  sync_tab_context::FakeTabContextSyncService* GetService() {
    return static_cast<sync_tab_context::FakeTabContextSyncService*>(
        TabContextSyncServiceFactory::GetForProfile(GetProfile()));
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  bool IsApiDefined(content::WebContents* web_contents) {
    return content::EvalJs(web_contents,
                           "chrome.getContainerDecryptionToken !== undefined")
        .ExtractBool();
  }

  int GetContainerDecryptionTokenByteLength(content::WebContents* web_contents,
                                            const std::string& gaia_id,
                                            const std::string& container_id) {
    const std::string script = base::StringPrintf(
        R"(
        new Promise((resolve) => {
          chrome.getContainerDecryptionToken((buffer) => {
            resolve(buffer ? buffer.byteLength : -1);
          }, "%s", "%s");
        });
    )",
        gaia_id.c_str(), container_id.c_str());
    return content::EvalJs(web_contents, script).ExtractInt();
  }

  std::string GetContainerDecryptionTokenString(
      content::WebContents* web_contents,
      const std::string& gaia_id,
      const std::string& container_id) {
    const std::string script = base::StringPrintf(
        R"(
        new Promise((resolve) => {
          chrome.getContainerDecryptionToken((buffer) => {
            if (!buffer) {
              resolve(null);
              return;
            }
            const bytes = new Uint8Array(buffer);
            const decoder = new TextDecoder();
            resolve(decoder.decode(bytes));
          }, "%s", "%s");
        });
    )",
        gaia_id.c_str(), container_id.c_str());
    return content::EvalJs(web_contents, script).ExtractString();
  }

  base::test::ScopedFeatureList feature_list_{syncer::kSyncEncryptedTabContext};
  base::CallbackListSubscription create_services_subscription_;
  std::string primary_gaia_id_;
  content::ContentMockCertVerifier mock_cert_verifier_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(TabContextDecryptionTokenTabHelperBrowserTest,
                       ShouldNotExposeApiOnDisallowedOrigin) {
  const GURL url = https_server_.GetURL("example.com", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_FALSE(IsApiDefined(GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(TabContextDecryptionTokenTabHelperBrowserTest,
                       ShouldExposeApiOnAllowedOrigin) {
  const GURL url = https_server_.GetURL("chromestorage.goog", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_TRUE(IsApiDefined(GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(TabContextDecryptionTokenTabHelperBrowserTest,
                       ShouldReturnNullOnGaiaIdMismatch) {
  const GURL url = https_server_.GetURL("chromestorage.goog", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  const sync_tab_context::ContainerId container_id =
      CHECK_DEREF(GetService()->CreateContainer());

  EXPECT_EQ(-1, GetContainerDecryptionTokenByteLength(
                    GetActiveWebContents(), kMismatchGaiaId,
                    container_id.value().AsLowercaseString()));
}

IN_PROC_BROWSER_TEST_F(TabContextDecryptionTokenTabHelperBrowserTest,
                       ShouldReturnTokenWhenAvailable) {
  const GURL url = https_server_.GetURL("chromestorage.goog", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  const sync_tab_context::ContainerId container_id =
      CHECK_DEREF(GetService()->CreateContainer());

  EXPECT_TRUE(GetService()->IsValidToken(
      container_id, GetContainerDecryptionTokenString(
                        GetActiveWebContents(), primary_gaia_id_,
                        container_id.value().AsLowercaseString())));
}

}  // namespace

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/service_worker_payment_app_factory.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/remoting/remote_test_helper.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/core/features.h"
#include "components/payments/core/test_payment_manifest_downloader.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

namespace payments {
namespace {

static const char kDefaultScope[] = "/app1/";

}  // namespace

// Tests for the service worker payment app factory.
class ServiceWorkerPaymentAppFactoryBrowserTest : public InProcessBrowserTest {
 public:
  ServiceWorkerPaymentAppFactoryBrowserTest()
      : alicepay_(net::EmbeddedTestServer::TYPE_HTTPS),
        bobpay_(net::EmbeddedTestServer::TYPE_HTTPS),
        frankpay_(net::EmbeddedTestServer::TYPE_HTTPS),
        georgepay_(net::EmbeddedTestServer::TYPE_HTTPS),
        kylepay_(net::EmbeddedTestServer::TYPE_HTTPS),
        larrypay_(net::EmbeddedTestServer::TYPE_HTTPS),
        charlie_example_(net::EmbeddedTestServer::TYPE_HTTPS),
        david_example_(net::EmbeddedTestServer::TYPE_HTTPS),
        frank_example_(net::EmbeddedTestServer::TYPE_HTTPS),
        george_example_(net::EmbeddedTestServer::TYPE_HTTPS),
        harry_example_(net::EmbeddedTestServer::TYPE_HTTPS),
        ike_example_(net::EmbeddedTestServer::TYPE_HTTPS),
        john_example_(net::EmbeddedTestServer::TYPE_HTTPS),
        kyle_example_(net::EmbeddedTestServer::TYPE_HTTPS),
        larry_example_(net::EmbeddedTestServer::TYPE_HTTPS) {
    scoped_feature_list_.InitWithFeatures(
        {::features::kServiceWorkerPaymentApps,
         features::kWebPaymentsJustInTimePaymentApp},
        {});
  }

  ~ServiceWorkerPaymentAppFactoryBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from the test servers with custom hostnames without an
    // interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  PermissionRequestManager* GetPermissionRequestManager() {
    return PermissionRequestManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  // Starts the test severs and opens a test page on alicepay.com.
  void SetUpOnMainThread() override {
    ASSERT_TRUE(StartTestServer("alicepay.com", &alicepay_));
    ASSERT_TRUE(StartTestServer("bobpay.com", &bobpay_));
    ASSERT_TRUE(StartTestServer("frankpay.com", &frankpay_));
    ASSERT_TRUE(StartTestServer("georgepay.com", &georgepay_));
    ASSERT_TRUE(StartTestServer("kylepay.com", &kylepay_));
    ASSERT_TRUE(StartTestServer("larrypay.com", &larrypay_));
    ASSERT_TRUE(StartTestServer("charlie.example.com", &charlie_example_));
    ASSERT_TRUE(StartTestServer("david.example.com", &david_example_));
    ASSERT_TRUE(StartTestServer("frank.example.com", &frank_example_));
    ASSERT_TRUE(StartTestServer("george.example.com", &george_example_));
    ASSERT_TRUE(StartTestServer("harry.example.com", &harry_example_));
    ASSERT_TRUE(StartTestServer("ike.example.com", &ike_example_));
    ASSERT_TRUE(StartTestServer("john.example.com", &john_example_));
    ASSERT_TRUE(StartTestServer("kyle.example.com", &kyle_example_));
    ASSERT_TRUE(StartTestServer("larry.example.com", &larry_example_));

    GetPermissionRequestManager()->set_auto_response_for_test(
        PermissionRequestManager::ACCEPT_ALL);
  }

  // Invokes the JavaScript function install(|method_name|) in
  // components/test/data/payments/alicepay.com/app1/index.js, which responds
  // back via domAutomationController.
  void InstallPaymentAppForMethod(const std::string& method_name) {
    InstallPaymentAppInScopeForMethod(kDefaultScope, method_name);
  }

  // Invokes the JavaScript function install(|method_name|) in
  // components/test/data/payments/alicepay.com|scope|index.js, which responds
  // back via domAutomationController.
  void InstallPaymentAppInScopeForMethod(const std::string& scope,
                                         const std::string& method_name) {
    ui_test_utils::NavigateToURL(browser(),
                                 alicepay_.GetURL("alicepay.com", scope));
    std::string contents;
    std::string script = "install('" + method_name + "');";
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(), script,
        &contents))
        << "Script execution failed: " << script;
    ASSERT_NE(std::string::npos,
              contents.find("Payment app for \"" + method_name +
                            "\" method installed."))
        << method_name << " method install message not found in:\n"
        << contents;
  }

  // Retrieves all valid payment apps that can handle the methods in
  // |payment_method_identifiers_set|. Blocks until the factory has finished
  // using all resources.
  void GetAllPaymentAppsForMethods(
      const std::vector<std::string>& payment_method_identifiers) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::BrowserContext* context = web_contents->GetBrowserContext();
    auto downloader = std::make_unique<TestDownloader>(
        content::BrowserContext::GetDefaultStoragePartition(context)
            ->GetURLLoaderFactoryForBrowserProcess());
    downloader->AddTestServerURL("https://alicepay.com/",
                                 alicepay_.GetURL("alicepay.com", "/"));
    downloader->AddTestServerURL("https://bobpay.com/",
                                 bobpay_.GetURL("bobpay.com", "/"));
    downloader->AddTestServerURL("https://frankpay.com/",
                                 frankpay_.GetURL("frankpay.com", "/"));
    downloader->AddTestServerURL("https://georgepay.com/",
                                 georgepay_.GetURL("georgepay.com", "/"));
    downloader->AddTestServerURL("https://kylepay.com/",
                                 kylepay_.GetURL("kylepay.com", "/"));
    downloader->AddTestServerURL("https://larrypay.com/",
                                 larrypay_.GetURL("larrypay.com", "/"));
    downloader->AddTestServerURL(
        "https://charlie.example.com/",
        charlie_example_.GetURL("charlie.example.com", "/"));
    downloader->AddTestServerURL(
        "https://david.example.com/",
        david_example_.GetURL("david.example.com", "/"));
    downloader->AddTestServerURL(
        "https://frank.example.com/",
        frank_example_.GetURL("frank.example.com", "/"));
    downloader->AddTestServerURL(
        "https://george.example.com/",
        george_example_.GetURL("george.example.com", "/"));
    downloader->AddTestServerURL(
        "https://harry.example.com/",
        harry_example_.GetURL("harry.example.com", "/"));
    downloader->AddTestServerURL("https://ike.example.com/",
                                 ike_example_.GetURL("ike.example.com", "/"));
    downloader->AddTestServerURL("https://john.example.com/",
                                 john_example_.GetURL("john.example.com", "/"));
    downloader->AddTestServerURL("https://kyle.example.com/",
                                 kyle_example_.GetURL("kyle.example.com", "/"));
    downloader->AddTestServerURL(
        "https://larry.example.com/",
        larry_example_.GetURL("larry.example.com", "/"));
    ServiceWorkerPaymentAppFactory::GetInstance()
        ->SetDownloaderAndIgnorePortInOriginComparisonForTesting(
            std::move(downloader));

    std::vector<mojom::PaymentMethodDataPtr> method_data;
    for (const auto& identifier : payment_method_identifiers) {
      method_data.emplace_back(mojom::PaymentMethodData::New());
      method_data.back()->supported_method = identifier;
    }

    base::RunLoop run_loop;
    ServiceWorkerPaymentAppFactory::GetInstance()->GetAllPaymentApps(
        web_contents,
        WebDataServiceFactory::GetPaymentManifestWebDataForProfile(
            Profile::FromBrowserContext(context),
            ServiceAccessType::EXPLICIT_ACCESS),
        method_data,
        /*may_crawl_for_installable_payment_apps=*/true,
        base::BindOnce(
            &ServiceWorkerPaymentAppFactoryBrowserTest::OnGotAllPaymentApps,
            base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  // Returns the installed apps that have been found in
  // GetAllPaymentAppsForMethods().
  const content::PaymentAppProvider::PaymentApps& apps() const { return apps_; }

  // Returns the installable apps that have been found in
  // GetAllPaymentAppsForMethods().
  const ServiceWorkerPaymentAppFactory::InstallablePaymentApps&
  installable_apps() const {
    return installable_apps_;
  }

  // Returns the error message from the service worker payment app factory.
  const std::string& error_message() const { return error_message_; }

  // Expects that the first app has the |expected_method|.
  void ExpectPaymentAppWithMethod(const std::string& expected_method) {
    ExpectPaymentAppFromScopeWithMethod(kDefaultScope, expected_method);
  }

  // Expects that the app from |scope| has the |expected_method|.
  void ExpectPaymentAppFromScopeWithMethod(const std::string& scope,
                                           const std::string& expected_method) {
    ASSERT_FALSE(apps().empty());
    content::StoredPaymentApp* app = nullptr;
    for (const auto& it : apps()) {
      if (it.second->scope.path() == scope) {
        app = it.second.get();
        break;
      }
    }
    ASSERT_NE(nullptr, app) << "No app found in scope " << scope;
    EXPECT_TRUE(base::Contains(app->enabled_methods, expected_method))
        << "Unable to find payment method " << expected_method
        << " in the list of enabled methods for the app installed from "
        << app->scope;
  }

  // Expects an installable payment app in |scope|. The |scope| is also the
  // payment method.
  void ExpectInstallablePaymentAppInScope(const std::string& scope) {
    ASSERT_FALSE(installable_apps().empty());
    WebAppInstallationInfo* app = nullptr;

    const GURL expected_scope(scope);
    url::Replacements<char> clear_port;
    clear_port.ClearPort();

    for (const auto& it : installable_apps()) {
      // |sw_scope| may contain the test server port. Ignore it in comparison.
      if (GURL(it.second->sw_scope).ReplaceComponents(clear_port) ==
          expected_scope) {
        app = it.second.get();
        break;
      }
    }
    ASSERT_NE(nullptr, app) << "No installable app found in scope " << scope;
  }

 private:
  // Called by the factory upon completed app lookup. These |apps| have only
  // valid payment methods.
  void OnGotAllPaymentApps(
      content::PaymentAppProvider::PaymentApps apps,
      ServiceWorkerPaymentAppFactory::InstallablePaymentApps installable_apps,
      const std::string& error_message) {
    apps_ = std::move(apps);
    installable_apps_ = std::move(installable_apps);
    error_message_ = error_message;
  }

  // Starts the |test_server| for |hostname|. Returns true on success.
  bool StartTestServer(const std::string& hostname,
                       net::EmbeddedTestServer* test_server) {
    host_resolver()->AddRule(hostname, "127.0.0.1");
    if (!test_server->InitializeAndListen())
      return false;
    test_server->ServeFilesFromSourceDirectory(
        "components/test/data/payments/" + hostname);
    test_server->StartAcceptingConnections();
    return true;
  }

  // https://alicepay.com hosts the payment app.
  net::EmbeddedTestServer alicepay_;

  // https://bobpay.com/webpay does not permit any other origin to use this
  // payment method.
  net::EmbeddedTestServer bobpay_;

  // https://frankpay.com/webpay supports payment apps from any origin.
  net::EmbeddedTestServer frankpay_;

  // https://georgepay.com/webpay supports payment apps only from
  // https://alicepay.com.
  net::EmbeddedTestServer georgepay_;

  // https://kylepay.com hosts a payment handler that can be installed "just in
  // time."
  net::EmbeddedTestServer kylepay_;

  // https://larrypay.com/webpay cross-site redirects to
  // https://kylepay.com/webpay.
  net::EmbeddedTestServer larrypay_;

  // https://charlie.example.com/webpay same-site redirects to
  // https://david.example.com/webpay.
  net::EmbeddedTestServer charlie_example_;

  // https://david.example.com/webpay same-site redirects to
  // https://frank.example.com/webpay.
  net::EmbeddedTestServer david_example_;

  // https://frank.example.com/webpay same-site redirects to
  // https://george.example.com/webpay.
  net::EmbeddedTestServer frank_example_;

  // https://george.example.com/webpay same-site redirects to
  // https://harry.example.com/webpay.
  net::EmbeddedTestServer george_example_;

  // https://harry.example.com/webpay hosts a payment handler that can be
  // installed "just in time."
  net::EmbeddedTestServer harry_example_;

  // https://ike.example.com/webpay has a cross-origin HTTP Link header to
  // https://harry.example.com/payment-manifest.json.
  net::EmbeddedTestServer ike_example_;

  // https://john.example.com/payment-method.json has a cross-origin default
  // application https://harry.example.com/app.json.
  net::EmbeddedTestServer john_example_;

  // https://kyle.example.com/app.json has a cross-origin service worker
  // https://harry.example.com/app.js.
  net::EmbeddedTestServer kyle_example_;

  // https://larry.example.com/app.json has a cross-origin service worker scope
  // https://harry.example.com/webpay/.
  net::EmbeddedTestServer larry_example_;

  // The installed apps that have been found by the factory in
  // GetAllPaymentAppsForMethods() method.
  content::PaymentAppProvider::PaymentApps apps_;

  // The installable apps that have been found by the factory in
  // GetAllPaymentAppsForMethods() method.
  ServiceWorkerPaymentAppFactory::InstallablePaymentApps installable_apps_;

  // The error message returned by the service worker factory.
  std::string error_message_;

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerPaymentAppFactoryBrowserTest);
};

// A payment app has to be installed first.
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFactoryBrowserTest, NoApps) {
  {
    GetAllPaymentAppsForMethods({"basic-card", "https://alicepay.com/webpay",
                                 "https://bobpay.com/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"basic-card", "https://alicepay.com/webpay",
                                 "https://bobpay.com/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Unknown payment method names are not permitted.
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFactoryBrowserTest,
                       UnknownMethod) {
  InstallPaymentAppForMethod("unknown-payment-method");

  {
    GetAllPaymentAppsForMethods({"unknown-payment-method", "basic-card",
                                 "https://alicepay.com/webpay",
                                 "https://bobpay.com/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"unknown-payment-method", "basic-card",
                                 "https://alicepay.com/webpay",
                                 "https://bobpay.com/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// A payment app can use "basic-card" payment method.
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFactoryBrowserTest, BasicCard) {
  InstallPaymentAppForMethod("basic-card");

  {
    GetAllPaymentAppsForMethods({"basic-card", "https://alicepay.com/webpay",
                                 "https://bobpay.com/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(1U, apps().size());
    ExpectPaymentAppWithMethod("basic-card");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"basic-card", "https://alicepay.com/webpay",
                                 "https://bobpay.com/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(1U, apps().size());
    ExpectPaymentAppWithMethod("basic-card");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// A payment app can use any payment method name from its own origin.
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFactoryBrowserTest, OwnOrigin) {
  InstallPaymentAppForMethod("https://alicepay.com/webpay");

  {
    GetAllPaymentAppsForMethods({"basic-card", "https://alicepay.com/webpay",
                                 "https://bobpay.com/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(1U, apps().size());
    ExpectPaymentAppWithMethod("https://alicepay.com/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"basic-card", "https://alicepay.com/webpay",
                                 "https://bobpay.com/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(1U, apps().size());
    ExpectPaymentAppWithMethod("https://alicepay.com/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// A payment app from https://alicepay.com cannot use the payment method
// https://bobpay.com/webpay, because https://bobpay.com/payment-method.json
// does not have an entry for "supported_origins".
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFactoryBrowserTest,
                       NotSupportedOrigin) {
  InstallPaymentAppForMethod("https://bobpay.com/webpay");

  {
    GetAllPaymentAppsForMethods({"basic-card", "https://alicepay.com/webpay",
                                 "https://bobpay.com/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"basic-card", "https://alicepay.com/webpay",
                                 "https://bobpay.com/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// A payment app from https://alicepay.com can use the payment method
// https://frankpay.com/webpay, because https://frankpay.com/payment-method.json
// specifies "supported_origins": "*" (all origins supported).
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFactoryBrowserTest,
                       AllOringsSupported) {
  InstallPaymentAppForMethod("https://frankpay.com/webpay");

  {
    GetAllPaymentAppsForMethods({"https://frankpay.com/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(1U, apps().size());
    ExpectPaymentAppWithMethod("https://frankpay.com/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://frankpay.com/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(1U, apps().size());
    ExpectPaymentAppWithMethod("https://frankpay.com/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// A payment app from https://alicepay.com can use the payment method
// https://georgepay.com/webpay, because
// https://georgepay.com/payment-method.json explicitly includes
// "https://alicepay.com" as one of the "supported_origins".
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFactoryBrowserTest,
                       SupportedOrigin) {
  InstallPaymentAppForMethod("https://georgepay.com/webpay");

  {
    GetAllPaymentAppsForMethods({"https://georgepay.com/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(1U, apps().size());
    ExpectPaymentAppWithMethod("https://georgepay.com/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://georgepay.com/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(1U, apps().size());
    ExpectPaymentAppWithMethod("https://georgepay.com/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Multiple payment apps from https://alicepay.com can use the payment method
// https://georgepay.com/webpay at the same time, because
// https://georgepay.com/payment-method.json explicitly includes
// "https://alicepay.com" as on of the "supported_origins".
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFactoryBrowserTest,
                       TwoAppsSameMethod) {
  InstallPaymentAppInScopeForMethod("/app1/", "https://georgepay.com/webpay");
  InstallPaymentAppInScopeForMethod("/app2/", "https://georgepay.com/webpay");

  {
    GetAllPaymentAppsForMethods({"https://georgepay.com/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(2U, apps().size());
    ExpectPaymentAppFromScopeWithMethod("/app1/",
                                        "https://georgepay.com/webpay");
    ExpectPaymentAppFromScopeWithMethod("/app2/",
                                        "https://georgepay.com/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://georgepay.com/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(2U, apps().size());
    ExpectPaymentAppFromScopeWithMethod("/app1/",
                                        "https://georgepay.com/webpay");
    ExpectPaymentAppFromScopeWithMethod("/app2/",
                                        "https://georgepay.com/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Multiple payment apps from https://alicepay.com can use either
// https://georgepay.com/webpay or https://frankepay.com/webpay payment method,
// because https://frankpay.com/payment-method.json specifies
// "supported_origins": "*" (all origins supported) and
// https://georgepay.com/payment-method.json explicitly includes
// "https://alicepay.com" as on of the "supported_origins".
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFactoryBrowserTest,
                       TwoAppsDifferentMethods) {
  InstallPaymentAppInScopeForMethod("/app1/", "https://georgepay.com/webpay");
  InstallPaymentAppInScopeForMethod("/app2/", "https://frankpay.com/webpay");

  {
    GetAllPaymentAppsForMethods(
        {"https://georgepay.com/webpay", "https://frankpay.com/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(2U, apps().size());
    ExpectPaymentAppFromScopeWithMethod("/app1/",
                                        "https://georgepay.com/webpay");
    ExpectPaymentAppFromScopeWithMethod("/app2/",
                                        "https://frankpay.com/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods(
        {"https://georgepay.com/webpay", "https://frankpay.com/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(2U, apps().size());
    ExpectPaymentAppFromScopeWithMethod("/app1/",
                                        "https://georgepay.com/webpay");
    ExpectPaymentAppFromScopeWithMethod("/app2/",
                                        "https://frankpay.com/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// The payment method https://kylepay.com/webpay does not require explicit
// installation, because the webapp manifest https://kylepay.com/app.json
// includes enough information for just in time installation of the service
// worker https://kylepay.com/app.js with scope https://kylepay.com/webpay.
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFactoryBrowserTest,
                       InstallablePaymentApp) {
  {
    GetAllPaymentAppsForMethods({"https://kylepay.com/webpay"});

    EXPECT_TRUE(apps().empty());
    ASSERT_EQ(1U, installable_apps().size());
    ExpectInstallablePaymentAppInScope("https://kylepay.com/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://kylepay.com/webpay"});

    EXPECT_TRUE(apps().empty());
    ASSERT_EQ(1U, installable_apps().size());
    ExpectInstallablePaymentAppInScope("https://kylepay.com/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// The payment method https://larrypay.com/webpay is not valid, because it
// redirects to a different site (https://kylepay.com/webpay).
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFactoryBrowserTest,
                       InvalidDifferentSiteRedirect) {
  std::string expected_pattern =
      "Cross-site redirect from \"https://larrypay.com:\\d+/webpay\" to "
      "\"https://kylepay.com/webpay\" not allowed for payment manifests.";

  {
    GetAllPaymentAppsForMethods({"https://larrypay.com/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://larrypay.com/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }
}

// The payment method https://charlie.example.com/webpay is not valid, because
// it redirects 4 times (charlie -> david -> frank -> george -> harry).
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFactoryBrowserTest,
                       FourRedirectsIsNotValid) {
  std::string expected_error_message =
      "Unable to download the payment manifest because reached the maximum "
      "number of redirects.";
  {
    GetAllPaymentAppsForMethods({"https://charlie.example.com/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_EQ(expected_error_message, error_message());
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://charlie.example.com/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_EQ(expected_error_message, error_message());
  }
}

// The payment method https://david.example.com/webpay is valid, because it
// redirects 3 times (david -> frank -> george -> harry).
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFactoryBrowserTest,
                       ThreeRedirectsIsValid) {
  {
    GetAllPaymentAppsForMethods({"https://david.example.com/webpay"});

    EXPECT_TRUE(apps().empty());
    ASSERT_EQ(1U, installable_apps().size());
    ExpectInstallablePaymentAppInScope("https://harry.example.com/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://david.example.com/webpay"});

    EXPECT_TRUE(apps().empty());
    ASSERT_EQ(1U, installable_apps().size());
    ExpectInstallablePaymentAppInScope("https://harry.example.com/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// The payment method https://george.example.com/webpay is valid, because it
// redirects once (george -> harry).
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFactoryBrowserTest,
                       OneRedirectIsValid) {
  {
    GetAllPaymentAppsForMethods({"https://george.example.com/webpay"});

    EXPECT_TRUE(apps().empty());
    ASSERT_EQ(1U, installable_apps().size());
    ExpectInstallablePaymentAppInScope("https://harry.example.com/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://george.example.com/webpay"});

    EXPECT_TRUE(apps().empty());
    ASSERT_EQ(1U, installable_apps().size());
    ExpectInstallablePaymentAppInScope("https://harry.example.com/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// The payment method https://ike.example.com/webpay is not valid, because of
// its cross-origin HTTP Link to
// https://harry.example.com/payment-manifest.json.
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFactoryBrowserTest,
                       CrossOriginHttpLinkHeaderIsInvalid) {
  std::string expected_pattern =
      "Cross-origin payment method manifest "
      "\"https://harry.example.com/payment-manifest.json\" not allowed for the "
      "payment method \"https://ike.example.com:\\d+/webpay\".";
  {
    GetAllPaymentAppsForMethods({"https://ike.example.com/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://ike.example.com/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }
}

// The payment method https://john.example.com/webpay is not valid, because of
// its cross-origin default application https://harry.example.com/app.json.
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFactoryBrowserTest,
                       CrossOriginDefaultApplicationIsInvalid) {
  std::string expected_pattern =
      "Cross-origin default application https://harry.example.com/app.json not "
      "allowed in payment method manifest "
      "https://john.example.com:\\d+/payment-manifest.json.";
  {
    GetAllPaymentAppsForMethods({"https://john.example.com/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://john.example.com/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }
}

// The payment method https://kyle.example.com/webpay is not valid, because of
// its cross-origin service worker location https://harry.example.com/app.js.
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFactoryBrowserTest,
                       CrossOriginServiceWorkerIsInvalid) {
  std::string expected_error_message =
      "Cross-origin \"serviceworker\".\"src\" https://harry.example.com/app.js "
      "not allowed in web app manifest https://kyle.example.com/app.json.";
  {
    GetAllPaymentAppsForMethods({"https://kyle.example.com/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_EQ(expected_error_message, error_message());
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://kyle.example.com/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_EQ(expected_error_message, error_message());
  }
}

// The payment method https://larry.example.com/webpay is not valid, because of
// its cross-origin service worker scope https://harry.example.com/webpay/".
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFactoryBrowserTest,
                       CrossOriginServiceWorkerScopeIsInvalid) {
  std::string expected_error_message =
      "Cross-origin \"serviceworker\".\"scope\" "
      "https://harry.example.com/webpay not allowed in web app manifest "
      "https://larry.example.com/app.json.";
  {
    GetAllPaymentAppsForMethods({"https://larry.example.com/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_EQ(expected_error_message, error_message());
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://larry.example.com/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_EQ(expected_error_message, error_message());
  }
}

}  // namespace payments

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/service_worker_payment_app_finder.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/payments/payment_app_install_util.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/core/const_csp_checker.h"
#include "components/payments/core/features.h"
#include "components/payments/core/test_payment_manifest_downloader.h"
#include "components/permissions/permission_request_manager.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace payments {
namespace {
static const char kDefaultScope[] = "/app1/";

void GetAllInstalledPaymentAppsCallback(
    base::OnceClosure done_callback,
    content::InstalledPaymentAppsFinder::PaymentApps* out_apps,
    content::InstalledPaymentAppsFinder::PaymentApps apps) {
  *out_apps = std::move(apps);
  std::move(done_callback).Run();
}
}  // namespace

// Tests for the service worker payment app finder.
class ServiceWorkerPaymentAppFinderBrowserTest : public InProcessBrowserTest {
 public:
  ServiceWorkerPaymentAppFinderBrowserTest()
      : alicepay_(net::EmbeddedTestServer::TYPE_HTTPS),
        bobpay_(net::EmbeddedTestServer::TYPE_HTTPS),
        charliepay_(net::EmbeddedTestServer::TYPE_HTTPS),
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

  ServiceWorkerPaymentAppFinderBrowserTest(
      const ServiceWorkerPaymentAppFinderBrowserTest&) = delete;
  ServiceWorkerPaymentAppFinderBrowserTest& operator=(
      const ServiceWorkerPaymentAppFinderBrowserTest&) = delete;

  ~ServiceWorkerPaymentAppFinderBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from the test servers with custom hostnames without an
    // interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  permissions::PermissionRequestManager* GetPermissionRequestManager() {
    return permissions::PermissionRequestManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  // Starts the test severs and opens a test page on alicepay.test.
  void SetUpOnMainThread() override {
    ASSERT_TRUE(StartTestServer("", &https_server_));
    ASSERT_TRUE(StartTestServer("alicepay.test", &alicepay_));
    ASSERT_TRUE(StartTestServer("bobpay.test", &bobpay_));
    ASSERT_TRUE(StartTestServer("charliepay.test", &charliepay_));
    ASSERT_TRUE(StartTestServer("frankpay.test", &frankpay_));
    ASSERT_TRUE(StartTestServer("georgepay.test", &georgepay_));
    ASSERT_TRUE(StartTestServer("kylepay.test", &kylepay_));
    ASSERT_TRUE(StartTestServer("larrypay.test", &larrypay_));
    ASSERT_TRUE(StartTestServer("charlie.example.test", &charlie_example_));
    ASSERT_TRUE(StartTestServer("david.example.test", &david_example_));
    ASSERT_TRUE(StartTestServer("frank.example.test", &frank_example_));
    ASSERT_TRUE(StartTestServer("george.example.test", &george_example_));
    ASSERT_TRUE(StartTestServer("harry.example.test", &harry_example_));
    ASSERT_TRUE(StartTestServer("ike.example.test", &ike_example_));
    ASSERT_TRUE(StartTestServer("john.example.test", &john_example_));
    ASSERT_TRUE(StartTestServer("kyle.example.test", &kyle_example_));
    ASSERT_TRUE(StartTestServer("larry.example.test", &larry_example_));

    GetPermissionRequestManager()->set_auto_response_for_test(
        permissions::PermissionRequestManager::ACCEPT_ALL);
  }

  // Invokes the JavaScript function install(|method_name|) in
  // components/test/data/payments/alicepay.test/app1/index.js, which responds
  // back via domAutomationController.
  void InstallPaymentAppForMethod(const std::string& method_name) {
    InstallPaymentAppInScopeForMethod(kDefaultScope, method_name);
  }

  // Invokes the JavaScript function install(|method_name|) in
  // components/test/data/payments/alicepay.test|scope|index.js, which responds
  // back via domAutomationController.
  void InstallPaymentAppInScopeForMethod(const std::string& scope,
                                         const std::string& method_name) {
    ASSERT_TRUE(
        PaymentAppInstallUtil::InstallPaymentAppForPaymentMethodIdentifier(
            *browser()->tab_strip_model()->GetActiveWebContents(),
            alicepay_.GetURL("alicepay.test", scope + "app.js"), method_name,
            PaymentAppInstallUtil::IconInstall::kWithIcon));
  }

  // Retrieves all valid payment apps that can handle the methods in
  // |payment_method_identifiers_set|. Blocks until the finder has finished
  // using all resources.
  void GetAllPaymentAppsForMethods(
      const std::vector<std::string>& payment_method_identifiers) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::BrowserContext* context = web_contents->GetBrowserContext();
    auto downloader = std::make_unique<TestDownloader>(
        GetCSPChecker(), context->GetDefaultStoragePartition()
                             ->GetURLLoaderFactoryForBrowserProcess());
    downloader->AddTestServerURL("https://alicepay.test/",
                                 alicepay_.GetURL("alicepay.test", "/"));
    downloader->AddTestServerURL("https://bobpay.test/",
                                 bobpay_.GetURL("bobpay.test", "/"));
    downloader->AddTestServerURL("https://charliepay.test/",
                                 charliepay_.GetURL("charliepay.test", "/"));
    downloader->AddTestServerURL("https://frankpay.test/",
                                 frankpay_.GetURL("frankpay.test", "/"));
    downloader->AddTestServerURL("https://georgepay.test/",
                                 georgepay_.GetURL("georgepay.test", "/"));
    downloader->AddTestServerURL("https://kylepay.test/",
                                 kylepay_.GetURL("kylepay.test", "/"));
    downloader->AddTestServerURL("https://larrypay.test/",
                                 larrypay_.GetURL("larrypay.test", "/"));
    downloader->AddTestServerURL(
        "https://charlie.example.test/",
        charlie_example_.GetURL("charlie.example.test", "/"));
    downloader->AddTestServerURL(
        "https://david.example.test/",
        david_example_.GetURL("david.example.test", "/"));
    downloader->AddTestServerURL(
        "https://frank.example.test/",
        frank_example_.GetURL("frank.example.test", "/"));
    downloader->AddTestServerURL(
        "https://george.example.test/",
        george_example_.GetURL("george.example.test", "/"));
    downloader->AddTestServerURL(
        "https://harry.example.test/",
        harry_example_.GetURL("harry.example.test", "/"));
    downloader->AddTestServerURL("https://ike.example.test/",
                                 ike_example_.GetURL("ike.example.test", "/"));
    downloader->AddTestServerURL(
        "https://john.example.test/",
        john_example_.GetURL("john.example.test", "/"));
    downloader->AddTestServerURL(
        "https://kyle.example.test/",
        kyle_example_.GetURL("kyle.example.test", "/"));
    downloader->AddTestServerURL(
        "https://larry.example.test/",
        larry_example_.GetURL("larry.example.test", "/"));

    auto* finder = ServiceWorkerPaymentAppFinder::GetOrCreateForCurrentDocument(
        browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetPrimaryMainFrame());
    finder->SetDownloaderAndIgnorePortInOriginComparisonForTesting(
        std::move(downloader));

    std::vector<mojom::PaymentMethodDataPtr> method_data;
    for (const auto& identifier : payment_method_identifiers) {
      method_data.emplace_back(mojom::PaymentMethodData::New());
      method_data.back()->supported_method = identifier;
    }

    base::RunLoop run_loop;
    finder->GetAllPaymentApps(
        url::Origin::Create(GURL("https://chromium.org")),
        webdata_services::WebDataServiceWrapperFactory::
            GetPaymentManifestWebDataServiceForBrowserContext(
                context, ServiceAccessType::EXPLICIT_ACCESS),
        std::move(method_data), GetCSPChecker(),
        base::BindOnce(
            &ServiceWorkerPaymentAppFinderBrowserTest::OnGotAllPaymentApps,
            base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  // Returns the installed apps that have been found in
  // GetAllPaymentAppsForMethods().
  const content::InstalledPaymentAppsFinder::PaymentApps& apps() const {
    return apps_;
  }

  // Returns the installable apps that have been found in
  // GetAllPaymentAppsForMethods().
  const ServiceWorkerPaymentAppFinder::InstallablePaymentApps&
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
    GURL::Replacements clear_port;
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

  virtual base::WeakPtr<CSPChecker> GetCSPChecker() {
    return const_csp_checker_.GetWeakPtr();
  }

 private:
  // Called by the factory upon completed app lookup. These |apps| have only
  // valid payment methods.
  void OnGotAllPaymentApps(
      content::InstalledPaymentAppsFinder::PaymentApps apps,
      ServiceWorkerPaymentAppFinder::InstallablePaymentApps installable_apps,
      const std::string& error_message) {
    apps_ = std::move(apps);
    installable_apps_ = std::move(installable_apps);
    error_message_ = error_message;
  }

  // Starts the |test_server| for |hostname|. Returns true on success.
  bool StartTestServer(const std::string& hostname,
                       net::EmbeddedTestServer* test_server) {
    host_resolver()->AddRule(hostname, "127.0.0.1");
    if (!test_server->InitializeAndListen()) {
      return false;
    }
    test_server->ServeFilesFromSourceDirectory(
        "components/test/data/payments/" + hostname);
    test_server->StartAcceptingConnections();
    return true;
  }

 protected:
  // Main test server, which serves components/test/data/payments.
  net::EmbeddedTestServer https_server_;

  // https://alicepay.test hosts the payment app.
  net::EmbeddedTestServer alicepay_;

  // https://bobpay.test/webpay does not permit any other origin to use this
  // payment method.
  net::EmbeddedTestServer bobpay_;

  // https://charliepay.test/webpay has a payment method manifest file that
  // specifies multiple web app manifests (i.e., multiple payment handlers for
  // the method).
  net::EmbeddedTestServer charliepay_;

  // https://frankpay.test/webpay supports payment apps from any origin.
  net::EmbeddedTestServer frankpay_;

  // https://georgepay.test/webpay supports payment apps only from
  // https://alicepay.test.
  net::EmbeddedTestServer georgepay_;

  // https://kylepay.test hosts a payment handler that can be installed "just in
  // time."
  net::EmbeddedTestServer kylepay_;

  // https://larrypay.test/webpay cross-site redirects to
  // https://kylepay.test/webpay.
  net::EmbeddedTestServer larrypay_;

  // https://charlie.example.test/webpay same-site redirects to
  // https://david.example.test/webpay.
  net::EmbeddedTestServer charlie_example_;

  // https://david.example.test/webpay same-site redirects to
  // https://frank.example.test/webpay.
  net::EmbeddedTestServer david_example_;

  // https://frank.example.test/webpay same-site redirects to
  // https://george.example.test/webpay.
  net::EmbeddedTestServer frank_example_;

  // https://george.example.test/webpay same-site redirects to
  // https://harry.example.test/webpay.
  net::EmbeddedTestServer george_example_;

  // https://harry.example.test/webpay hosts a payment handler that can be
  // installed "just in time."
  net::EmbeddedTestServer harry_example_;

  // https://ike.example.test/webpay has a cross-origin HTTP Link header to
  // https://harry.example.test/payment-manifest.json.
  net::EmbeddedTestServer ike_example_;

  // https://john.example.test/payment-method.json has a cross-origin default
  // application https://harry.example.test/app.json.
  net::EmbeddedTestServer john_example_;

  // https://kyle.example.test/app.json has a cross-origin service worker
  // https://harry.example.test/app.js.
  net::EmbeddedTestServer kyle_example_;

  // https://larry.example.test/app.json has a cross-origin service worker scope
  // https://harry.example.test/webpay/.
  net::EmbeddedTestServer larry_example_;

 private:
  // The installed apps that have been found by the factory in
  // GetAllPaymentAppsForMethods() method.
  content::InstalledPaymentAppsFinder::PaymentApps apps_;

  // The installable apps that have been found by the factory in
  // GetAllPaymentAppsForMethods() method.
  ServiceWorkerPaymentAppFinder::InstallablePaymentApps installable_apps_;

  // The error message returned by the service worker factory.
  std::string error_message_;

  ConstCSPChecker const_csp_checker_{/*allow=*/true};

  base::test::ScopedFeatureList scoped_feature_list_;
};

// A payment app has to be installed first.
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFinderBrowserTest, NoApps) {
  {
    GetAllPaymentAppsForMethods({"basic-card", "https://alicepay.test/webpay",
                                 "https://bobpay.test/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"basic-card", "https://alicepay.test/webpay",
                                 "https://bobpay.test/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Unknown payment method names are not permitted.
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFinderBrowserTest,
                       UnknownMethod) {
  InstallPaymentAppForMethod("unknown-payment-method");

  {
    GetAllPaymentAppsForMethods({"unknown-payment-method", "basic-card",
                                 "https://alicepay.test/webpay",
                                 "https://bobpay.test/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"unknown-payment-method", "basic-card",
                                 "https://alicepay.test/webpay",
                                 "https://bobpay.test/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// A payment app can use any payment method name from its own origin.
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFinderBrowserTest, OwnOrigin) {
  InstallPaymentAppForMethod("https://alicepay.test/webpay");

  {
    GetAllPaymentAppsForMethods({"basic-card", "https://alicepay.test/webpay",
                                 "https://bobpay.test/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(1U, apps().size());
    ExpectPaymentAppWithMethod("https://alicepay.test/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"basic-card", "https://alicepay.test/webpay",
                                 "https://bobpay.test/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(1U, apps().size());
    ExpectPaymentAppWithMethod("https://alicepay.test/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// A payment app from https://alicepay.test cannot use the payment method
// https://bobpay.test/webpay, because https://bobpay.test/payment-method.json
// does not have an entry for "supported_origins".
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFinderBrowserTest,
                       NotSupportedOrigin) {
  InstallPaymentAppForMethod("https://bobpay.test/webpay");

  {
    GetAllPaymentAppsForMethods({"basic-card", "https://alicepay.test/webpay",
                                 "https://bobpay.test/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"basic-card", "https://alicepay.test/webpay",
                                 "https://bobpay.test/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// A payment app from https://alicepay.test can not use the payment method
// https://frankpay.test/webpay, because
// https://frankpay.test/payment-method.json invalid "supported_origins": "*".
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFinderBrowserTest,
                       OriginWildcardNotSupportedInPaymentMethodManifest) {
  InstallPaymentAppForMethod("https://frankpay.test/webpay");

  {
    GetAllPaymentAppsForMethods({"https://frankpay.test/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(0U, apps().size());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://frankpay.test/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(0U, apps().size());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// A payment app from https://alicepay.test can use the payment method
// https://georgepay.test/webpay, because
// https://georgepay.test/payment-method.json explicitly includes
// "https://alicepay.test" as one of the "supported_origins".
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFinderBrowserTest,
                       SupportedOrigin) {
  InstallPaymentAppForMethod("https://georgepay.test/webpay");

  {
    GetAllPaymentAppsForMethods({"https://georgepay.test/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(1U, apps().size());
    ExpectPaymentAppWithMethod("https://georgepay.test/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://georgepay.test/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(1U, apps().size());
    ExpectPaymentAppWithMethod("https://georgepay.test/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Multiple payment apps from https://alicepay.test can use the payment method
// https://georgepay.test/webpay at the same time, because
// https://georgepay.test/payment-method.json explicitly includes
// "https://alicepay.test" as on of the "supported_origins".
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFinderBrowserTest,
                       TwoAppsSameMethod) {
  InstallPaymentAppInScopeForMethod("/app1/", "https://georgepay.test/webpay");
  InstallPaymentAppInScopeForMethod("/app2/", "https://georgepay.test/webpay");

  {
    GetAllPaymentAppsForMethods({"https://georgepay.test/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(2U, apps().size());
    ExpectPaymentAppFromScopeWithMethod("/app1/",
                                        "https://georgepay.test/webpay");
    ExpectPaymentAppFromScopeWithMethod("/app2/",
                                        "https://georgepay.test/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://georgepay.test/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(2U, apps().size());
    ExpectPaymentAppFromScopeWithMethod("/app1/",
                                        "https://georgepay.test/webpay");
    ExpectPaymentAppFromScopeWithMethod("/app2/",
                                        "https://georgepay.test/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// A Payment app from https://alicepay.test can use only the payment method
// https://georgepay.test/webpay. Because
// https://georgepay.test/payment-method.json explicitly includes
// "https://alicepay.test" as on of the "supported_origins". Also
// https://frankpay.test/payment-method.json does not explicitly authorize any
// payment app.
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFinderBrowserTest,
                       TwoAppsDifferentMethods) {
  InstallPaymentAppInScopeForMethod("/app1/", "https://georgepay.test/webpay");
  InstallPaymentAppInScopeForMethod("/app2/", "https://frankpay.test/webpay");

  {
    GetAllPaymentAppsForMethods(
        {"https://georgepay.test/webpay", "https://frankpay.test/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(1U, apps().size());
    ExpectPaymentAppFromScopeWithMethod("/app1/",
                                        "https://georgepay.test/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods(
        {"https://georgepay.test/webpay", "https://frankpay.test/webpay"});

    EXPECT_TRUE(installable_apps().empty());
    ASSERT_EQ(1U, apps().size());
    ExpectPaymentAppFromScopeWithMethod("/app1/",
                                        "https://georgepay.test/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// The payment method https://kylepay.test/webpay does not require explicit
// installation, because the webapp manifest https://kylepay.test/app.json
// includes enough information for just in time installation of the service
// worker https://kylepay.test/app.js with scope https://kylepay.test/webpay.
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFinderBrowserTest,
                       InstallablePaymentApp) {
  {
    GetAllPaymentAppsForMethods({"https://kylepay.test/webpay"});

    EXPECT_TRUE(apps().empty());
    ASSERT_EQ(1U, installable_apps().size());
    ExpectInstallablePaymentAppInScope("https://kylepay.test/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://kylepay.test/webpay"});

    EXPECT_TRUE(apps().empty());
    ASSERT_EQ(1U, installable_apps().size());
    ExpectInstallablePaymentAppInScope("https://kylepay.test/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// The payment method https://larrypay.test/webpay is not valid, because it
// redirects to a different site (https://kylepay.test/webpay).
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFinderBrowserTest,
                       InvalidDifferentSiteRedirect) {
  std::string expected_pattern =
      "Cross-site redirect from \"https://larrypay.test:\\d+/webpay\" to "
      "\"https://kylepay.test/webpay\" not allowed for payment manifests.";

  {
    GetAllPaymentAppsForMethods({"https://larrypay.test/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://larrypay.test/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }
}

// The payment method https://charlie.example.test/webpay is not valid, because
// it redirects 4 times (charlie -> david -> frank -> george -> harry).
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFinderBrowserTest,
                       FourRedirectsIsNotValid) {
  std::string expected_error_message =
      "Unable to download the payment manifest because reached the maximum "
      "number of redirects.";
  {
    GetAllPaymentAppsForMethods({"https://charlie.example.test/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_EQ(expected_error_message, error_message());
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://charlie.example.test/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_EQ(expected_error_message, error_message());
  }
}

// The payment method https://david.example.test/webpay is valid, because it
// redirects 3 times (david -> frank -> george -> harry).
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFinderBrowserTest,
                       ThreeRedirectsIsValid) {
  {
    GetAllPaymentAppsForMethods({"https://david.example.test/webpay"});

    EXPECT_TRUE(apps().empty());
    ASSERT_EQ(1U, installable_apps().size());
    ExpectInstallablePaymentAppInScope("https://harry.example.test/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://david.example.test/webpay"});

    EXPECT_TRUE(apps().empty());
    ASSERT_EQ(1U, installable_apps().size());
    ExpectInstallablePaymentAppInScope("https://harry.example.test/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// The payment method https://george.example.test/webpay is valid, because it
// redirects once (george -> harry).
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFinderBrowserTest,
                       OneRedirectIsValid) {
  {
    GetAllPaymentAppsForMethods({"https://george.example.test/webpay"});

    EXPECT_TRUE(apps().empty());
    ASSERT_EQ(1U, installable_apps().size());
    ExpectInstallablePaymentAppInScope("https://harry.example.test/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://george.example.test/webpay"});

    EXPECT_TRUE(apps().empty());
    ASSERT_EQ(1U, installable_apps().size());
    ExpectInstallablePaymentAppInScope("https://harry.example.test/webpay");
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// The payment method https://ike.example.test/webpay is not valid, because of
// its cross-origin HTTP Link to
// https://harry.example.test/payment-manifest.json.
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFinderBrowserTest,
                       CrossOriginHttpLinkHeaderIsInvalid) {
  std::string expected_pattern =
      "Cross-origin payment method manifest "
      "\"https://harry.example.test/payment-manifest.json\" not allowed for "
      "the "
      "payment method \"https://ike.example.test:\\d+/webpay\".";
  {
    GetAllPaymentAppsForMethods({"https://ike.example.test/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://ike.example.test/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }
}

// The payment method https://john.example.test/webpay is not valid, because of
// its cross-origin default application https://harry.example.test/app.json.
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFinderBrowserTest,
                       CrossOriginDefaultApplicationIsInvalid) {
  std::string expected_pattern =
      "Cross-origin default application https://harry.example.test/app.json "
      "not "
      "allowed in payment method manifest "
      "https://john.example.test:\\d+/payment-manifest.json.";
  {
    GetAllPaymentAppsForMethods({"https://john.example.test/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://john.example.test/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }
}

// The payment method https://kyle.example.test/webpay is not valid, because of
// its cross-origin service worker location https://harry.example.test/app.js.
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFinderBrowserTest,
                       CrossOriginServiceWorkerIsInvalid) {
  std::string expected_error_message =
      "Cross-origin \"serviceworker\".\"src\" "
      "https://harry.example.test/app.js "
      "not allowed in web app manifest https://kyle.example.test/app.json.";
  {
    GetAllPaymentAppsForMethods({"https://kyle.example.test/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_EQ(expected_error_message, error_message());
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://kyle.example.test/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_EQ(expected_error_message, error_message());
  }
}

// The payment method https://larry.example.test/webpay is not valid, because of
// its cross-origin service worker scope https://harry.example.test/webpay/".
IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFinderBrowserTest,
                       CrossOriginServiceWorkerScopeIsInvalid) {
  std::string expected_error_message =
      "Cross-origin \"serviceworker\".\"scope\" "
      "https://harry.example.test/webpay not allowed in web app manifest "
      "https://larry.example.test/app.json.";
  {
    GetAllPaymentAppsForMethods({"https://larry.example.test/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_EQ(expected_error_message, error_message());
  }

  // Repeat lookups should have identical results.
  {
    GetAllPaymentAppsForMethods({"https://larry.example.test/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
    EXPECT_EQ(expected_error_message, error_message());
  }
}

// Tests that service worker payment apps are able to respond to the icon
// changing in their manifest file.
class ServiceWorkerPaymentAppFinderIconRefreshBrowserTest
    : public ServiceWorkerPaymentAppFinderBrowserTest {
 public:
  ServiceWorkerPaymentAppFinderIconRefreshBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kPaymentHandlerAlwaysRefreshIcon);
  }
  ~ServiceWorkerPaymentAppFinderIconRefreshBrowserTest() override = default;

  content::InstalledPaymentAppsFinder::PaymentApps GetInstalledPaymentApps() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    base::RunLoop run_loop;
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    content::InstalledPaymentAppsFinder::GetInstance(
        web_contents->GetBrowserContext())
        ->GetAllPaymentApps(base::BindOnce(&GetAllInstalledPaymentAppsCallback,
                                           run_loop.QuitClosure(), &apps));
    run_loop.Run();

    return apps;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFinderIconRefreshBrowserTest,
                       PaymentAppUpdatesWhenIconChanges) {
  // Start by installing the KylePay app directly, with an initial icon.
  ASSERT_TRUE(
      PaymentAppInstallUtil::InstallPaymentAppForPaymentMethodIdentifier(
          *browser()->tab_strip_model()->GetActiveWebContents(),
          kylepay_.GetURL("kylepay.test", "/app.js"),
          "https://kylepay.test/webpay",
          PaymentAppInstallUtil::IconInstall::kWithIcon));

  content::InstalledPaymentAppsFinder::PaymentApps original_apps =
      GetInstalledPaymentApps();
  ASSERT_EQ(original_apps.size(), 1u);
  SkBitmap original_icon = *original_apps.begin()->second->icon;

  // Next, initialize a lookup against KylePay. This should trigger a manifest
  // fetch, and asynchronously pick up the icon specified in KylePay's manifest
  // - which is different than InstallPaymentAppForPaymentMethodIdentifier.
  GetAllPaymentAppsForMethods({"https://kylepay.test/webpay"});

  // Because icon update is asynchronous, the app returned by
  // GetAllPaymentAppsForMethods will still have the old icon.
  ASSERT_EQ(apps().size(), 1u);
  EXPECT_TRUE(
      gfx::test::AreBitmapsEqual(*apps().begin()->second->icon, original_icon));
  EXPECT_TRUE(installable_apps().empty());
  EXPECT_TRUE(error_message().empty());

  // But if we now get updated information on the installed app, it should have
  // the new icon associated with it.
  content::InstalledPaymentAppsFinder::PaymentApps updated_apps =
      GetInstalledPaymentApps();
  ASSERT_EQ(updated_apps.size(), 1u);
  EXPECT_FALSE(gfx::test::AreBitmapsEqual(*updated_apps.begin()->second->icon,
                                          original_icon));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerPaymentAppFinderIconRefreshBrowserTest,
                       FailedIconFetchDoesNotOverrideOldIcon) {
  // Start by installing the KylePay app directly, with an initial icon.
  ASSERT_TRUE(
      PaymentAppInstallUtil::InstallPaymentAppForPaymentMethodIdentifier(
          *browser()->tab_strip_model()->GetActiveWebContents(),
          kylepay_.GetURL("kylepay.test", "/app.js"),
          "https://kylepay.test/webpay",
          PaymentAppInstallUtil::IconInstall::kWithIcon));

  content::InstalledPaymentAppsFinder::PaymentApps original_apps =
      GetInstalledPaymentApps();
  ASSERT_EQ(original_apps.size(), 1u);
  SkBitmap original_icon = *original_apps.begin()->second->icon;

  // Navigate to a page with strict CSP so that Kylepay's icon fetch fails.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/csp_prevent_icon_download.html")));

  // Next, initialize a lookup against KylePay. This should trigger a manifest
  // fetch, and asynchronously try to fetch the icon specified in KylePay's
  // manifest - but the fetch will fail due to CSP.
  GetAllPaymentAppsForMethods({"https://kylepay.test/webpay"});

  // If we now get updated information on the installed app, it should still
  // have the origin icon - the failed fetch should have no effect.
  content::InstalledPaymentAppsFinder::PaymentApps updated_apps =
      GetInstalledPaymentApps();
  ASSERT_EQ(updated_apps.size(), 1u);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*updated_apps.begin()->second->icon,
                                         original_icon));
}

// The parameterized test fixture that resets the CSP checker after N=GetParam()
// calls to AllowConnectToSource().
class ServiceWorkerPaymentAppFinderCSPCheckerBrowserTest
    : public ServiceWorkerPaymentAppFinderBrowserTest,
      public ConstCSPChecker,
      public testing::WithParamInterface<int> {
 public:
  ServiceWorkerPaymentAppFinderCSPCheckerBrowserTest()
      : ConstCSPChecker(/*allow=*/true) {
    MaybeInvalidateCSPCheckerWeakPtrs();
  }

  ~ServiceWorkerPaymentAppFinderCSPCheckerBrowserTest() override = default;

  int GetNumberOfLookupsBeforeCSPCheckerReset() { return GetParam(); }

  void reset_number_of_lookups() { number_of_lookups_ = 0; }

 private:
  // ConstCSPChecker:
  void AllowConnectToSource(
      const GURL& url,
      const GURL& url_before_redirects,
      bool did_follow_redirect,
      base::OnceCallback<void(bool)> result_callback) override {
    ConstCSPChecker::AllowConnectToSource(url, url_before_redirects,
                                          did_follow_redirect,
                                          std::move(result_callback));
    number_of_lookups_++;
    MaybeInvalidateCSPCheckerWeakPtrs();
  }

  // ServiceWorkerPaymentAppFinderBrowserTest:
  base::WeakPtr<CSPChecker> GetCSPChecker() override {
    return number_of_lookups_ >= GetNumberOfLookupsBeforeCSPCheckerReset()
               ? base::WeakPtr<ConstCSPChecker>()
               : ConstCSPChecker::GetWeakPtr();
  }

  void MaybeInvalidateCSPCheckerWeakPtrs() {
    if (number_of_lookups_ >= GetNumberOfLookupsBeforeCSPCheckerReset()) {
      ConstCSPChecker::InvalidateWeakPtrsForTesting();
    }
  }

  int number_of_lookups_ = 0;
};

// A CSP checker reset during the download flow should not cause a crash.
IN_PROC_BROWSER_TEST_P(ServiceWorkerPaymentAppFinderCSPCheckerBrowserTest,
                       CSPCheckerResetDoesNotCrash) {
  // The lookups for finding the app are:
  // 1) Payment method identifier.
  // 2) Payment method manifest.
  // 3) Web app manifest.
  constexpr int kNumLookupsToFindTheApp = 3;

  // Repeat lookups should have identical results, regardless of manifest cache
  // state. To ensure that the cache has no effect, we repeat the test twice.
  for (int i = 0; i < 2; ++i) {
    reset_number_of_lookups();

    GetAllPaymentAppsForMethods({"https://kylepay.test/webpay"});

    EXPECT_TRUE(apps().empty());
    if (GetNumberOfLookupsBeforeCSPCheckerReset() >= kNumLookupsToFindTheApp) {
      EXPECT_EQ(1U, installable_apps().size());
    } else {
      EXPECT_TRUE(installable_apps().empty());
    }
  }
}

// Variant of CSPCheckerResetDoesNotCrash, but with a payment method manifest
// that has multiple web applications specified (i.e., |default_applications|
// will have multiple entries).
IN_PROC_BROWSER_TEST_P(ServiceWorkerPaymentAppFinderCSPCheckerBrowserTest,
                       CSPCheckerResetDoesNotCrashWithTwoWebAppManifests) {
  // Repeat lookups should have identical results, regardless of manifest cache
  // state. To ensure that the cache has no effect, we repeat the test twice.
  for (int i = 0; i < 2; ++i) {
    reset_number_of_lookups();

    GetAllPaymentAppsForMethods({"https://charliepay.test/webpay"});

    EXPECT_TRUE(apps().empty());
    EXPECT_TRUE(installable_apps().empty());
  }
}

// A range from 0 (inclusive) to 6 (exclusive) will test CSP checker reset:
// 0: Before any CSP lookups.
// 1: After CSP lookup for payment method URL (e.g.,
//    https://kylepay.test/webpay).
// 2: After CSP lookup for payment method manifest (e.g.,
//    https://kylepay.test/payment-method.json).
// 3: After CSP lookup for first web app manifest (e.g.,
//    https://kylepay.test/app.json).
// 4: After CSP lookup for second web app manifest, if it exists (e.g.,
//    https://charliepay.test/prod.json).
// 5: No CSP checker reset at all, tested just in case.
INSTANTIATE_TEST_SUITE_P(Test,
                         ServiceWorkerPaymentAppFinderCSPCheckerBrowserTest,
                         testing::Range(0, 6));

}  // namespace payments

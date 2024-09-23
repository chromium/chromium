// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/manifest_verifier.h"

#include <stdint.h>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/utility/payment_manifest_parser.h"
#include "components/payments/core/const_csp_checker.h"
#include "components/payments/core/test_payment_manifest_downloader.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/origin.h"

namespace payments {
namespace {

// Tests for the manifest verifier.
class ManifestVerifierBrowserTest : public InProcessBrowserTest {
 public:
  ManifestVerifierBrowserTest() {}

  ManifestVerifierBrowserTest(const ManifestVerifierBrowserTest&) = delete;
  ManifestVerifierBrowserTest& operator=(const ManifestVerifierBrowserTest&) =
      delete;

  ~ManifestVerifierBrowserTest() override {}

  // Starts the HTTPS test server on localhost.
  void SetUpOnMainThread() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    ASSERT_TRUE(https_server_->InitializeAndListen());
    https_server_->ServeFilesFromSourceDirectory(
        "components/test/data/payments");
    https_server_->StartAcceptingConnections();

    const_csp_checker_ = std::make_unique<ConstCSPChecker>(/*allow=*/true);
    content::BrowserContext* context = browser()->profile();
    test_downloader_ = std::make_unique<TestDownloader>(
        const_csp_checker_->GetWeakPtr(),
        context->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess());
    test_downloader_->AddTestServerURL("https://", https_server_->GetURL("/"));
  }

  // Runs the verifier on the |apps| and blocks until the verifier has finished
  // using all resources.
  void Verify(content::InstalledPaymentAppsFinder::PaymentApps apps) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::BrowserContext* context = browser()->profile();
    auto parser = std::make_unique<payments::PaymentManifestParser>(
        std::make_unique<ErrorLogger>());
    auto cache = webdata_services::WebDataServiceWrapperFactory::
        GetPaymentManifestWebDataServiceForBrowserContext(
            context, ServiceAccessType::EXPLICIT_ACCESS);

    ManifestVerifier verifier(url::Origin::Create(GURL("https://chromium.org")),
                              web_contents, test_downloader_.get(),
                              parser.get(), cache.get());

    base::RunLoop run_loop;
    verifier.Verify(
        std::move(apps),
        base::BindOnce(&ManifestVerifierBrowserTest::OnPaymentAppsVerified,
                       base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  // Returns the apps that have been verified by the Verify() method.
  const content::InstalledPaymentAppsFinder::PaymentApps& verified_apps()
      const {
    return verified_apps_;
  }

  const std::string& error_message() const { return error_message_; }

  // Expects that the verified payment app with |id| has the |expected_scope|
  // and the |expected_methods| and the
  // |expect_has_explicitly_verified_methods|.
  void ExpectApp(int64_t id,
                 const std::string& expected_scope,
                 const std::set<std::string>& expected_methods,
                 bool expect_has_explicitly_verified_methods) {
    const auto& it = verified_apps().find(id);
    ASSERT_NE(verified_apps().end(), it);
    EXPECT_EQ(GURL(expected_scope), it->second->scope);
    std::set<std::string> actual_methods(it->second->enabled_methods.begin(),
                                         it->second->enabled_methods.end());
    EXPECT_EQ(expected_methods, actual_methods);
    EXPECT_EQ(expect_has_explicitly_verified_methods,
              it->second->has_explicitly_verified_methods);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

 protected:
  std::unique_ptr<TestDownloader> test_downloader_;

 private:
  // Called by the verifier upon completed verification. These |apps| have only
  // valid payment methods.
  void OnPaymentAppsVerified(
      content::InstalledPaymentAppsFinder::PaymentApps apps,
      const std::string& error_message) {
    verified_apps_ = std::move(apps);
    error_message_ = error_message;
  }

  // Serves the payment method manifest files.
  std::unique_ptr<net::EmbeddedTestServer> https_server_;

  std::unique_ptr<ConstCSPChecker> const_csp_checker_;

  // The apps that have been verified by the Verify() method.
  content::InstalledPaymentAppsFinder::PaymentApps verified_apps_;

  std::string error_message_;
};

// Absence of payment handlers should result in absence of verified payment
// handlers.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest, NoApps) {
  {
    Verify(content::InstalledPaymentAppsFinder::PaymentApps());

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    Verify(content::InstalledPaymentAppsFinder::PaymentApps());

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// A payment handler without any payment method names is not valid.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest, NoMethods) {
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.test/webpay");

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.test/webpay");

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// A payment handler with an unknown non-URL payment method name is not valid.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest,
                       UnknownPaymentMethodNameIsRemoved) {
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.test/webpay");
    apps[0]->enabled_methods.push_back("unknown");

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.test/webpay");
    apps[0]->enabled_methods.push_back("unknown");

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Verify that a payment handler from https://bobpay.test/webpay can not use the
// payment method name https://frankpay.test/webpay, because
// https://frankpay.test/payment-manifest.json does not explicitly authorize
// any payment app.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest,
                       BobPayHandlerCanNotUseMethodWithOriginWildcard) {
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.test/webpay");
    apps[0]->enabled_methods.push_back("https://frankpay.test/webpay");

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.test/webpay");
    apps[0]->enabled_methods.push_back("https://frankpay.test/webpay");
    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Verify that a payment handler from an unreachable website can not use the
// payment method name https://frankpay.test/webpay, because
// https://frankpay.test/payment-manifest.json does not explicitly authorize
// any payment app.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest,
                       Handler404CanNotUseMethodWithOriginWildcard) {
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://404.com/webpay");
    apps[0]->enabled_methods.push_back("https://frankpay.test/webpay");

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://404.com/webpay");
    apps[0]->enabled_methods.push_back("https://frankpay.test/webpay");
    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Verify that a payment handler from anywhere on https://bobpay.test can use
// the payment method name from anywhere else on https://bobpay.test, because of
// the origin match.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest,
                       BobPayCanUseAnyMethodOnOwnOrigin) {
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.test/anything/here");
    apps[0]->enabled_methods.push_back(
        "https://bobpay.test/does/not/matter/whats/here");

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://bobpay.test/anything/here",
              {"https://bobpay.test/does/not/matter/whats/here"}, true);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.test/anything/here");
    apps[0]->enabled_methods = {
        "https://bobpay.test/does/not/matter/whats/here"};
    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://bobpay.test/anything/here",
              {"https://bobpay.test/does/not/matter/whats/here"}, true);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Verify that a payment handler from anywhere on an unreachable website can use
// the payment method name from anywhere else on the same unreachable website,
// because they have identical origin.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest,
                       Handler404CanUseAnyMethodOnOwnOrigin) {
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://404.com/anything/here");
    apps[0]->enabled_methods = {"https://404.com/does/not/matter/whats/here"};

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://404.com/anything/here",
              {"https://404.com/does/not/matter/whats/here"}, true);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://404.com/anything/here");
    apps[0]->enabled_methods = {"https://404.com/does/not/matter/whats/here"};
    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://404.com/anything/here",
              {"https://404.com/does/not/matter/whats/here"}, true);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Verify that only the payment handler from https://alicepay.test/webpay can
// use payment methods https://georgepay.test/webpay and
// https://ikepay.test/webpay, because both
// https://georgepay.test/payment-manifest.json and
// https://ikepay.test/payment-manifest.json contain "supported_origins":
// ["https://alicepay.test"]. The payment handler from
// https://bobpay.test/webpay cannot use these payment methods, however.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest, OneSupportedOrigin) {
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://alicepay.test/webpay");
    apps[0]->enabled_methods = {"https://georgepay.test/webpay",
                                "https://ikepay.test/webpay"};
    apps[1] = std::make_unique<content::StoredPaymentApp>();
    apps[1]->scope = GURL("https://bobpay.test/webpay");
    apps[1]->enabled_methods = {"https://georgepay.test/webpay",
                                "https://ikepay.test/webpay"};

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://alicepay.test/webpay",
              {"https://georgepay.test/webpay", "https://ikepay.test/webpay"},
              true);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://alicepay.test/webpay");
    apps[0]->enabled_methods = {"https://georgepay.test/webpay",
                                "https://ikepay.test/webpay"};
    apps[1] = std::make_unique<content::StoredPaymentApp>();
    apps[1]->scope = GURL("https://bobpay.test/webpay");
    apps[1]->enabled_methods = {"https://georgepay.test/webpay",
                                "https://ikepay.test/webpay"};

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://alicepay.test/webpay",
              {"https://georgepay.test/webpay", "https://ikepay.test/webpay"},
              true);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Verify that a payment handler from https://alicepay.test/webpay can use both
// same-origin URL payment method name and different-origin URL payment method
// name.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest, ThreeTypesOfMethods) {
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://alicepay.test/webpay");
    apps[0]->enabled_methods = {"basic-card", "https://alicepay.test/webpay2",
                                "https://ikepay.test/webpay"};

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://alicepay.test/webpay",
              {"https://alicepay.test/webpay2", "https://ikepay.test/webpay"},
              true);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://alicepay.test/webpay");
    apps[0]->enabled_methods = {"basic-card", "https://alicepay.test/webpay2",
                                "https://ikepay.test/webpay"};

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://alicepay.test/webpay",
              {"https://alicepay.test/webpay2", "https://ikepay.test/webpay"},
              true);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Verify that a payment handler from https://bobpay.test/webpay cannot use
// payment method names that are unreachable websites, the origin of which does
// not match that of the payment handler.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest,
                       SinglePaymentMethodName404) {
  std::string expected_pattern =
      "Unable to download payment manifest "
      "\"https://127.0.0.1:\\d+/404.test/webpay\". HTTP 404 Not Found.";
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.test/webpay");
    apps[0]->enabled_methods = {"https://404.test/webpay"};

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }

  // Repeat verifications should have identical results.
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.test/webpay");
    apps[0]->enabled_methods = {"https://404.test/webpay"};

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }
}

// Verify that a payment handler from https://bobpay.test/webpay cannot use
// payment method names that are unreachable websites, the origin of which does
// not match that of the payment handler. Since multiple downloads fail, the
// error message will describe the first failure.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest,
                       MultiplePaymentMethodName404) {
  std::string expected_pattern =
      "Unable to download payment manifest "
      "\"https://127.0.0.1:\\d+/404(aswell)?.test/webpay\". HTTP 404 Not "
      "Found.";
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.test/webpay");
    apps[0]->enabled_methods = {"https://404.test/webpay",
                                "https://404aswell.test/webpay"};

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }

  // Repeat verifications should have identical results.
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.test/webpay");
    apps[0]->enabled_methods = {"https://404.test/webpay",
                                "https://404aswell.test/webpay"};

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }
}

// Non-URL payment method names are not valid.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest,
                       NonUrlPaymentMethodNamesAreNotValid) {
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.test/webpay");
    apps[0]->enabled_methods = {"basic-card",
                                "interledger",
                                "payee-credit-transfer",
                                "payer-credit-transfer",
                                "tokenized-card",
                                "not-supported"};

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::InstalledPaymentAppsFinder::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.test/webpay");
    apps[0]->enabled_methods = {"basic-card",
                                "interledger",
                                "payee-credit-transfer",
                                "payer-credit-transfer",
                                "tokenized-card",
                                "not-supported"};

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

}  // namespace
}  // namespace payments

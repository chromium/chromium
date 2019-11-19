// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/manifest_verifier.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/utility/payment_manifest_parser.h"
#include "components/payments/core/test_payment_manifest_downloader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

namespace payments {
namespace {

// Tests for the manifest verifier.
class ManifestVerifierBrowserTest : public InProcessBrowserTest {
 public:
  ManifestVerifierBrowserTest() {}
  ~ManifestVerifierBrowserTest() override {}

  // Starts the HTTPS test server on localhost.
  void SetUpOnMainThread() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    ASSERT_TRUE(https_server_->InitializeAndListen());
    https_server_->ServeFilesFromSourceDirectory(
        "components/test/data/payments");
    https_server_->StartAcceptingConnections();
  }

  // Runs the verifier on the |apps| and blocks until the verifier has finished
  // using all resources.
  void Verify(content::PaymentAppProvider::PaymentApps apps) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::BrowserContext* context = web_contents->GetBrowserContext();
    auto downloader = std::make_unique<TestDownloader>(
        content::BrowserContext::GetDefaultStoragePartition(context)
            ->GetURLLoaderFactoryForBrowserProcess());
    downloader->AddTestServerURL("https://", https_server_->GetURL("/"));
    auto parser = std::make_unique<payments::PaymentManifestParser>(
        std::make_unique<ErrorLogger>());
    auto cache = WebDataServiceFactory::GetPaymentManifestWebDataForProfile(
        Profile::FromBrowserContext(context),
        ServiceAccessType::EXPLICIT_ACCESS);

    ManifestVerifier verifier(web_contents, downloader.get(), parser.get(),
                              cache.get());

    base::RunLoop run_loop;
    verifier.Verify(
        std::move(apps),
        base::BindOnce(&ManifestVerifierBrowserTest::OnPaymentAppsVerified,
                       base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  // Returns the apps that have been verified by the Verify() method.
  const content::PaymentAppProvider::PaymentApps& verified_apps() const {
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

 private:
  // Called by the verifier upon completed verification. These |apps| have only
  // valid payment methods.
  void OnPaymentAppsVerified(content::PaymentAppProvider::PaymentApps apps,
                             const std::string& error_message) {
    verified_apps_ = std::move(apps);
    error_message_ = error_message;
  }

  // Serves the payment method manifest files.
  std::unique_ptr<net::EmbeddedTestServer> https_server_;

  // The apps that have been verified by the Verify() method.
  content::PaymentAppProvider::PaymentApps verified_apps_;

  std::string error_message_;

  DISALLOW_COPY_AND_ASSIGN(ManifestVerifierBrowserTest);
};

// Absence of payment handlers should result in absence of verified payment
// handlers.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest, NoApps) {
  {
    Verify(content::PaymentAppProvider::PaymentApps());

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    Verify(content::PaymentAppProvider::PaymentApps());

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// A payment handler without any payment method names is not valid.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest, NoMethods) {
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.com/webpay");

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.com/webpay");

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// A payment handler with an unknown non-URL payment method name is not valid.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest,
                       UnknownPaymentMethodNameIsRemoved) {
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.com/webpay");
    apps[0]->enabled_methods.push_back("unknown");

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.com/webpay");
    apps[0]->enabled_methods.push_back("unknown");

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// A payment handler with "basic-card" payment method name is valid.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest, KnownPaymentMethodName) {
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.com/webpay");
    apps[0]->enabled_methods.push_back("basic-card");

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://bobpay.com/webpay", {"basic-card"}, false);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.com/webpay");
    apps[0]->enabled_methods.push_back("basic-card");

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://bobpay.com/webpay", {"basic-card"}, false);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// A payment handler with both "basic-card" and "interledger" payment method
// names is valid.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest,
                       TwoKnownPaymentMethodNames) {
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.com/webpay");
    apps[0]->enabled_methods.push_back("basic-card");
    apps[0]->enabled_methods.push_back("interledger");

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://bobpay.com/webpay", {"basic-card", "interledger"},
              false);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.com/webpay");
    apps[0]->enabled_methods.push_back("basic-card");
    apps[0]->enabled_methods.push_back("interledger");

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://bobpay.com/webpay", {"basic-card", "interledger"},
              false);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Two payment handlers with "basic-card" payment method names are both valid.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest,
                       TwoAppsWithKnownPaymentMethodNames) {
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.com/webpay");
    apps[0]->enabled_methods.push_back("basic-card");
    apps[1] = std::make_unique<content::StoredPaymentApp>();
    apps[1]->scope = GURL("https://alicepay.com/webpay");
    apps[1]->enabled_methods.push_back("basic-card");

    Verify(std::move(apps));

    EXPECT_EQ(2U, verified_apps().size());
    ExpectApp(0, "https://bobpay.com/webpay", {"basic-card"}, false);
    ExpectApp(1, "https://alicepay.com/webpay", {"basic-card"}, false);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.com/webpay");
    apps[0]->enabled_methods.push_back("basic-card");
    apps[1] = std::make_unique<content::StoredPaymentApp>();
    apps[1]->scope = GURL("https://alicepay.com/webpay");
    apps[1]->enabled_methods.push_back("basic-card");
    Verify(std::move(apps));

    EXPECT_EQ(2U, verified_apps().size());
    ExpectApp(0, "https://bobpay.com/webpay", {"basic-card"}, false);
    ExpectApp(1, "https://alicepay.com/webpay", {"basic-card"}, false);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Verify that a payment handler from https://bobpay.com/webpay can use the
// payment method name https://frankpay.com/webpay, because
// https://frankpay.com/payment-manifest.json contains "supported_origins": "*".
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest,
                       BobPayHandlerCanUseMethodThatSupportsAllOrigins) {
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.com/webpay");
    apps[0]->enabled_methods.push_back("https://frankpay.com/webpay");

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://bobpay.com/webpay", {"https://frankpay.com/webpay"},
              false);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.com/webpay");
    apps[0]->enabled_methods.push_back("https://frankpay.com/webpay");
    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://bobpay.com/webpay", {"https://frankpay.com/webpay"},
              false);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Verify that a payment handler from an unreachable website can use the payment
// method name https://frankpay.com/webpay, because
// https://frankpay.com/payment-manifest.json contains "supported_origins": "*".
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest,
                       Handler404CanUseMethodThatSupportsAllOrigins) {
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://404.com/webpay");
    apps[0]->enabled_methods.push_back("https://frankpay.com/webpay");

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://404.com/webpay", {"https://frankpay.com/webpay"},
              false);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://404.com/webpay");
    apps[0]->enabled_methods.push_back("https://frankpay.com/webpay");
    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://404.com/webpay", {"https://frankpay.com/webpay"},
              false);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Verify that a payment handler from anywhere on https://bobpay.com can use the
// payment method name from anywhere else on https://bobpay.com, because of the
// origin match.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest,
                       BobPayCanUseAnyMethodOnOwnOrigin) {
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.com/anything/here");
    apps[0]->enabled_methods.push_back(
        "https://bobpay.com/does/not/matter/whats/here");

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://bobpay.com/anything/here",
              {"https://bobpay.com/does/not/matter/whats/here"}, true);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.com/anything/here");
    apps[0]->enabled_methods.push_back(
        "https://bobpay.com/does/not/matter/whats/here");
    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://bobpay.com/anything/here",
              {"https://bobpay.com/does/not/matter/whats/here"}, true);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Verify that a payment handler from anywhere on an unreachable website can use
// the payment method name from anywhere else on the same unreachable website,
// because they have identical origin.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest,
                       Handler404CanUseAnyMethodOnOwnOrigin) {
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://404.com/anything/here");
    apps[0]->enabled_methods.push_back(
        "https://404.com/does/not/matter/whats/here");

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://404.com/anything/here",
              {"https://404.com/does/not/matter/whats/here"}, true);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://404.com/anything/here");
    apps[0]->enabled_methods.push_back(
        "https://404.com/does/not/matter/whats/here");
    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://404.com/anything/here",
              {"https://404.com/does/not/matter/whats/here"}, true);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Verify that only the payment handler from https://alicepay.com/webpay can use
// payment methods https://georgepay.com/webpay and https://ikepay.com/webpay,
// because both https://georgepay.com/payment-manifest.json and
// https://ikepay.com/payment-manifest.json contain "supported_origins":
// ["https://alicepay.com"]. The payment handler from https://bobpay.com/webpay
// cannot use these payment methods, however.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest, OneSupportedOrigin) {
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://alicepay.com/webpay");
    apps[0]->enabled_methods.push_back("https://georgepay.com/webpay");
    apps[0]->enabled_methods.push_back("https://ikepay.com/webpay");
    apps[1] = std::make_unique<content::StoredPaymentApp>();
    apps[1]->scope = GURL("https://bobpay.com/webpay");
    apps[1]->enabled_methods.push_back("https://georgepay.com/webpay");
    apps[1]->enabled_methods.push_back("https://ikepay.com/webpay");

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://alicepay.com/webpay",
              {"https://georgepay.com/webpay", "https://ikepay.com/webpay"},
              true);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://alicepay.com/webpay");
    apps[0]->enabled_methods.push_back("https://georgepay.com/webpay");
    apps[0]->enabled_methods.push_back("https://ikepay.com/webpay");
    apps[1] = std::make_unique<content::StoredPaymentApp>();
    apps[1]->scope = GURL("https://bobpay.com/webpay");
    apps[1]->enabled_methods.push_back("https://georgepay.com/webpay");
    apps[1]->enabled_methods.push_back("https://ikepay.com/webpay");

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://alicepay.com/webpay",
              {"https://georgepay.com/webpay", "https://ikepay.com/webpay"},
              true);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Verify that a payment handler from https://alicepay.com/webpay can use all
// three of non-URL payment method name, same-origin URL payment method name,
// and different-origin URL payment method name.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest, ThreeTypesOfMethods) {
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://alicepay.com/webpay");
    apps[0]->enabled_methods.push_back("basic-card");
    apps[0]->enabled_methods.push_back("https://alicepay.com/webpay2");
    apps[0]->enabled_methods.push_back("https://ikepay.com/webpay");

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://alicepay.com/webpay",
              {"basic-card", "https://alicepay.com/webpay2",
               "https://ikepay.com/webpay"},
              true);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://alicepay.com/webpay");
    apps[0]->enabled_methods.push_back("basic-card");
    apps[0]->enabled_methods.push_back("https://alicepay.com/webpay2");
    apps[0]->enabled_methods.push_back("https://ikepay.com/webpay");

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://alicepay.com/webpay",
              {"basic-card", "https://alicepay.com/webpay2",
               "https://ikepay.com/webpay"},
              true);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

// Verify that a payment handler from https://bobpay.com/webpay cannot use
// payment method names that are unreachable websites, the origin of which does
// not match that of the payment handler.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest,
                       SinglePaymentMethodName404) {
  std::string expected_pattern =
      "Unable to make a HEAD request to "
      "\"https://127.0.0.1:\\d+/404.test/webpay\" for payment method manifest.";
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.test/webpay");
    apps[0]->enabled_methods.push_back("https://404.test/webpay");

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }

  // Repeat verifications should have identical results.
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.test/webpay");
    apps[0]->enabled_methods.push_back("https://404.test/webpay");

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }
}

// Verify that a payment handler from https://bobpay.com/webpay cannot use
// payment method names that are unreachable websites, the origin of which does
// not match that of the payment handler. Since multiple downloads fail, the
// error message will describe the first failure.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest,
                       MultiplePaymentMethodName404) {
  std::string expected_pattern =
      "Unable to make a HEAD request to "
      "\"https://127.0.0.1:\\d+/404(aswell)?.test/webpay\" for payment method "
      "manifest.";
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.test/webpay");
    apps[0]->enabled_methods.push_back("https://404.test/webpay");
    apps[0]->enabled_methods.push_back("https://404aswell.test/webpay");

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }

  // Repeat verifications should have identical results.
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.test/webpay");
    apps[0]->enabled_methods.push_back("https://404.test/webpay");
    apps[0]->enabled_methods.push_back("https://404aswell.test/webpay");

    Verify(std::move(apps));

    EXPECT_TRUE(verified_apps().empty());
    EXPECT_TRUE(RE2::FullMatch(error_message(), expected_pattern))
        << "Actual error message \"" << error_message()
        << "\" did not match expected pattern \"" << expected_pattern << "\".";
  }
}

// All known payment method names are valid.
IN_PROC_BROWSER_TEST_F(ManifestVerifierBrowserTest,
                       AllKnownPaymentMethodNames) {
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.com/webpay");
    apps[0]->enabled_methods.push_back("basic-card");
    apps[0]->enabled_methods.push_back("interledger");
    apps[0]->enabled_methods.push_back("payee-credit-transfer");
    apps[0]->enabled_methods.push_back("payer-credit-transfer");
    apps[0]->enabled_methods.push_back("tokenized-card");
    apps[0]->enabled_methods.push_back("not-supported");

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://bobpay.com/webpay",
              {"basic-card", "interledger", "payee-credit-transfer",
               "payer-credit-transfer", "tokenized-card"},
              false);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }

  // Repeat verifications should have identical results.
  {
    content::PaymentAppProvider::PaymentApps apps;
    apps[0] = std::make_unique<content::StoredPaymentApp>();
    apps[0]->scope = GURL("https://bobpay.com/webpay");
    apps[0]->enabled_methods.push_back("basic-card");
    apps[0]->enabled_methods.push_back("interledger");
    apps[0]->enabled_methods.push_back("payee-credit-transfer");
    apps[0]->enabled_methods.push_back("payer-credit-transfer");
    apps[0]->enabled_methods.push_back("tokenized-card");
    apps[0]->enabled_methods.push_back("not-supported");

    Verify(std::move(apps));

    EXPECT_EQ(1U, verified_apps().size());
    ExpectApp(0, "https://bobpay.com/webpay",
              {"basic-card", "interledger", "payee-credit-transfer",
               "payer-credit-transfer", "tokenized-card"},
              false);
    EXPECT_TRUE(error_message().empty()) << error_message();
  }
}

}  // namespace
}  // namespace payments

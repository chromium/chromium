// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/cart/cart_db_content.pb.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/persisted_state_db/profile_proto_db.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"

namespace {
cart_db::ChromeCartContentProto BuildProto(const char* domain,
                                           const char* merchant_url) {
  cart_db::ChromeCartContentProto proto;
  proto.set_key(domain);
  proto.set_merchant_cart_url(merchant_url);
  proto.set_timestamp(base::Time::Now().ToDoubleT());
  return proto;
}

const char kMockMerchant[] = "walmart.com";
const char kMockMerchantURL[] = "https://www.walmart.com";
using ShoppingCarts =
    std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>;
}  // namespace

// Tests CartService.
class CartServiceBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeatures(
        {ntp_features::kNtpChromeCartModule,
         optimization_guide::features::kOptimizationHints},
        {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    service_ = CartServiceFactory::GetForProfile(profile);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // This bloom filter rejects "walmart.com" as a shopping site.
    command_line->AppendSwitchASCII("optimization_guide_hints_override",
                                    "Eg8IDxILCBsQJxoFiUzKeE4=");
  }

  void OperationEvaluation(base::OnceClosure closure,
                           bool expected_success,
                           bool actual_success) {
    EXPECT_EQ(expected_success, actual_success);
    std::move(closure).Run();
  }

  void GetEvaluationURL(base::OnceClosure closure,
                        ShoppingCarts expected,
                        bool result,
                        ShoppingCarts found) {
    EXPECT_EQ(found.size(), expected.size());
    for (size_t i = 0; i < expected.size(); i++) {
      EXPECT_EQ(found[i].first, expected[i].first);
      EXPECT_EQ(found[i].second.merchant_cart_url(),
                expected[i].second.merchant_cart_url());
      for (int j = 0; j < expected[i].second.product_image_urls().size(); j++) {
        EXPECT_EQ(expected[i].second.product_image_urls()[j],
                  found[i].second.product_image_urls()[j]);
      }
    }
    std::move(closure).Run();
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  CartService* service_;
};

IN_PROC_BROWSER_TEST_F(CartServiceBrowserTest, TestNotShowSkippedMerchants) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[6];
  cart_db::ChromeCartContentProto merchant_proto =
      BuildProto(kMockMerchant, kMockMerchantURL);
  ShoppingCarts merchant_res = {{kMockMerchant, merchant_proto}};
  ShoppingCarts empty_res = {};

  cart_db_->AddCart(
      kMockMerchant, merchant_proto,
      base::BindOnce(&CartServiceBrowserTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->LoadAllActiveCarts(base::BindOnce(
      &CartServiceBrowserTest::GetEvaluationURL, base::Unretained(this),
      run_loop[1].QuitClosure(), empty_res));
  run_loop[1].Run();

  cart_db_->LoadAllCarts(base::BindOnce(
      &CartServiceBrowserTest::GetEvaluationURL, base::Unretained(this),
      run_loop[2].QuitClosure(), empty_res));
  run_loop[2].Run();

  merchant_proto.set_is_removed(true);
  cart_db_->AddCart(
      kMockMerchant, merchant_proto,
      base::BindOnce(&CartServiceBrowserTest::OperationEvaluation,
                     base::Unretained(this), run_loop[3].QuitClosure(), true));
  run_loop[3].Run();

  service_->LoadAllActiveCarts(base::BindOnce(
      &CartServiceBrowserTest::GetEvaluationURL, base::Unretained(this),
      run_loop[4].QuitClosure(), empty_res));
  run_loop[4].Run();

  cart_db_->LoadAllCarts(base::BindOnce(
      &CartServiceBrowserTest::GetEvaluationURL, base::Unretained(this),
      run_loop[5].QuitClosure(), merchant_res));
  run_loop[5].Run();
}

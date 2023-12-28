// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/in_process_browser_test.h"

#include "base/callback_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/pref_names.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/proto/cart_db_content.pb.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/session_proto_db/session_proto_db.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
std::string BuildPartnerMerchantPattern(
    std::vector<std::string>& parter_merchant_list) {
  std::string pattern = "(";
  pattern += base::JoinString(parter_merchant_list, "|");
  pattern += ")";
  return pattern;
}

std::unique_ptr<net::test_server::HttpResponse> BasicResponse(
    bool return_empty_response,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == "/coupons/fl_codeless_discounts.json" &&
      !return_empty_response) {
    return nullptr;
  }
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content("{}");
  response->set_content_type("application/json; charset=UTF-8");
  return response;
}

cart_db::ChromeCartContentProto BuildCartProto(const char* domain,
                                               const char* merchant_url) {
  cart_db::ChromeCartContentProto proto;
  proto.set_key(domain);
  proto.set_merchant_cart_url(merchant_url);
  proto.set_timestamp(base::Time::Now().InSecondsFSinceUnixEpoch());
  return proto;
}

cart_db::ChromeCartContentProto BuildCartProtoWithCoupon(
    const char* domain,
    const char* merchant_url) {
  cart_db::ChromeCartContentProto proto = BuildCartProto(domain, merchant_url);
  proto.mutable_discount_info()->set_has_coupons(true);
  return proto;
}

using ShoppingCarts =
    std::vector<SessionProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>;

testing::Matcher<autofill::DisplayStrings> EqualsDisplayStrings(
    const autofill::DisplayStrings& display_strings) {
  return testing::Field("value_prop_text",
                        &autofill::DisplayStrings::value_prop_text,
                        testing::Eq(display_strings.value_prop_text));
}

}  // namespace

class FetchDiscountWorkerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&FetchDiscountWorkerBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    Profile* profile = browser()->profile();
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile);
    service_ = CartServiceFactory::GetForProfile(profile);

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/cart");
    embedded_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&BasicResponse, false /*return_empty_response*/));

    // Simulate discount consent is accepted.
    profile->GetPrefs()->SetBoolean(prefs::kCartDiscountEnabled, true);

    SignIn();
  }

  void TearDownOnMainThread() override {
    identity_test_environment_adaptor_.reset();
  }

  void CheckFLCodelessDiscounts(base::OnceClosure closure,
                                bool contained_coupon,
                                std::string expected_discount_text,
                                bool success,
                                ShoppingCarts found) {
    EXPECT_TRUE(success);
    EXPECT_EQ(1U, found.size());

    EXPECT_TRUE(found[0].second.has_discount_info());
    EXPECT_EQ(contained_coupon, found[0].second.discount_info().has_coupons());
    EXPECT_EQ(expected_discount_text,
              found[0].second.discount_info().discount_text());

    std::move(closure).Run();
  }

 protected:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  void SignIn() {
    identity_test_environment_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable("user@gmail.com",
                                      signin::ConsentLevel::kSync);
    identity_test_environment_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
  }

  void CreateCart(const std::string& domain,
                  cart_db::ChromeCartContentProto cart_proto) {
    CartDB* cart_db = service_->GetDB();
    base::RunLoop run_loop;

    cart_db->AddCart(
        domain, cart_proto,
        base::BindOnce(&FetchDiscountWorkerBrowserTest::OnCartAdded,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnCartAdded(base::OnceClosure closure, bool success) {
    EXPECT_TRUE(success);
    std::move(closure).Run();
  }

  void StartGettingDiscount() { service_->StartGettingDiscount(); }

  void waitForDiscounts(const std::string& cart_domain) {
    satisfied_ = false;
    while (true) {
      base::RunLoop().RunUntilIdle();
      base::RunLoop run_loop;
      service_->LoadCart(
          cart_domain,
          base::BindOnce(&FetchDiscountWorkerBrowserTest::CheckLastFetchTime,
                         base::Unretained(this), run_loop.QuitClosure()));
      run_loop.Run();
      if (satisfied_)
        break;
      base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
    }
  }

  void CheckLastFetchTime(base::OnceClosure closure,
                          bool success,
                          ShoppingCarts found) {
    ASSERT_TRUE(success);
    ASSERT_EQ(1U, found.size());

    if (found[0].second.has_discount_info()) {
      if (found[0].second.discount_info().last_fetched_timestamp() != 0) {
        satisfied_ = true;
      } else {
        VLOG(2) << "last_fetched_timestamp not set";
      }
    } else {
      VLOG(2) << "Not contain discount_info";
    }
    std::move(closure).Run();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
  base::CallbackListSubscription create_services_subscription_;
  raw_ptr<CartService, DanglingUntriaged> service_;
  bool satisfied_;
};

class FetchFLCodelessDiscountWorkerBrowserTest
    : public FetchDiscountWorkerBrowserTest {
 public:
  FetchFLCodelessDiscountWorkerBrowserTest() {
    parter_merchant_list_.push_back("merchant0.com");
    parter_merchant_list_.push_back("merchant1.com");
    parter_merchant_list_.push_back("merchant2.com");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    std::vector<base::test::FeatureRefAndParams> enabled_features;
    base::FieldTrialParams cart_params, coupon_params;
    cart_params[ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam] =
        "true";
    cart_params["CartDiscountFetcherEndpointParam"] =
        embedded_test_server()
            ->GetURL("/coupons/fl_codeless_discounts.json")
            .spec();
    enabled_features.emplace_back(ntp_features::kNtpChromeCartModule,
                                  cart_params);
    coupon_params["coupon-partner-merchant-pattern"] =
        BuildPartnerMerchantPattern(parter_merchant_list_);
    enabled_features.emplace_back(commerce::kRetailCoupons, coupon_params);
    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features,
        /*disabled_features*/ {
            optimization_guide::features::kOptimizationHints});
  }

 protected:
  std::vector<std::string> parter_merchant_list_;
};

IN_PROC_BROWSER_TEST_F(FetchFLCodelessDiscountWorkerBrowserTest,
                       SimplePercentOffTest) {
  embedded_test_server()->StartAcceptingConnections();

  CreateCart("merchant1.com",
             BuildCartProto("merchant1.com", "https://www.merchant1.com/cart"));

  StartGettingDiscount();
  waitForDiscounts("merchant1.com");

  // Verify discounts.
  base::RunLoop run_loop;
  std::string expected_discount_text = "10% off";
  service_->LoadCart(
      "merchant1.com",
      base::BindOnce(&FetchDiscountWorkerBrowserTest::CheckFLCodelessDiscounts,
                     base::Unretained(this), run_loop.QuitClosure(), true,
                     expected_discount_text));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FetchFLCodelessDiscountWorkerBrowserTest,
                       SimpleDollarOffTest) {
  embedded_test_server()->StartAcceptingConnections();

  CreateCart("merchant2.com",
             BuildCartProto("merchant2.com", "https://www.merchant2.com/cart"));

  StartGettingDiscount();
  waitForDiscounts("merchant2.com");

  // Verify discounts.
  base::RunLoop run_loop;
  std::string expected_discount_text = "$2 off";
  service_->LoadCart(
      "merchant2.com",
      base::BindOnce(&FetchDiscountWorkerBrowserTest::CheckFLCodelessDiscounts,
                     base::Unretained(this), run_loop.QuitClosure(), true,
                     expected_discount_text));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FetchFLCodelessDiscountWorkerBrowserTest,
                       NoDiscountForThisMerchantTest) {
  embedded_test_server()->StartAcceptingConnections();

  CreateCart("merchant0.com",
             BuildCartProto("merchant0.com", "https://www.merchant0.com/cart"));

  StartGettingDiscount();
  waitForDiscounts("merchant0.com");

  // Verify discounts.
  base::RunLoop run_loop;
  service_->LoadCart(
      "merchant0.com",
      base::BindOnce(&FetchDiscountWorkerBrowserTest::CheckFLCodelessDiscounts,
                     base::Unretained(this), run_loop.QuitClosure(), false,
                     ""));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FetchFLCodelessDiscountWorkerBrowserTest,
                       TwoCartsOneWithDiscountOneWithoutDiscount) {
  embedded_test_server()->StartAcceptingConnections();

  CreateCart("merchant0.com",
             BuildCartProto("merchant0.com", "https://www.merchant0.com/cart"));
  CreateCart("merchant1.com",
             BuildCartProto("merchant1.com", "https://www.merchant1.com/cart"));

  StartGettingDiscount();
  waitForDiscounts("merchant0.com");
  waitForDiscounts("merchant1.com");

  // Verify discounts.
  base::RunLoop run_loop[2];
  service_->LoadCart(
      "merchant0.com",
      base::BindOnce(&FetchDiscountWorkerBrowserTest::CheckFLCodelessDiscounts,
                     base::Unretained(this), run_loop[0].QuitClosure(), false,
                     ""));
  run_loop[0].Run();
  service_->LoadCart(
      "merchant1.com",
      base::BindOnce(&FetchDiscountWorkerBrowserTest::CheckFLCodelessDiscounts,
                     base::Unretained(this), run_loop[1].QuitClosure(), true,
                     "10% off" /*expected_discount_text*/));
  run_loop[1].Run();
}

IN_PROC_BROWSER_TEST_F(FetchFLCodelessDiscountWorkerBrowserTest,
                       CartDiscountBecomeUnavailable) {
  CreateCart("merchant1.com",
             BuildCartProtoWithCoupon("merchant1.com",
                                      "https://www.merchant1.com/cart"));

  // Config server to return empty response.
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, true /*return_empty_response*/));
  embedded_test_server()->StartAcceptingConnections();

  StartGettingDiscount();
  waitForDiscounts("merchant1.com");

  // Verify cart has no coupon discount.
  base::RunLoop run_loop;
  service_->LoadCart(
      "merchant1.com",
      base::BindOnce(&FetchDiscountWorkerBrowserTest::CheckFLCodelessDiscounts,
                     base::Unretained(this), run_loop.QuitClosure(), false,
                     ""));
  run_loop.Run();
}

class FetchFLCodeDiscountWorkerBrowserTest
    : public FetchDiscountWorkerBrowserTest {
 public:
  FetchFLCodeDiscountWorkerBrowserTest() {
    parter_merchant_list_.push_back("merchant0.com");
    parter_merchant_list_.push_back("merchant1.com");
    parter_merchant_list_.push_back("merchant2.com");
    parter_merchant_list_.push_back("merchant3.com");
  }

  void SetUpOnMainThread() override {
    FetchDiscountWorkerBrowserTest::SetUpOnMainThread();
    Profile* profile = browser()->profile();
    coupon_service_ = CouponServiceFactory::GetForProfile(profile);
    coupon_service_->MaybeFeatureStatusChanged(true);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    std::vector<base::test::FeatureRefAndParams> enabled_features;
    base::FieldTrialParams cart_params, coupon_params;
    cart_params[ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam] =
        "true";
    cart_params["CartDiscountFetcherEndpointParam"] =
        embedded_test_server()
            ->GetURL("/coupons/fl_code_discounts.json")
            .spec();
    enabled_features.emplace_back(ntp_features::kNtpChromeCartModule,
                                  cart_params);
    coupon_params["coupon-partner-merchant-pattern"] =
        BuildPartnerMerchantPattern(parter_merchant_list_);
    coupon_params[commerce::kRetailCouponsWithCodeParam] = "true";
    enabled_features.emplace_back(commerce::kRetailCoupons, coupon_params);
    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features,
        /*disabled_features*/ {
            optimization_guide::features::kOptimizationHints});
  }

 protected:
  std::vector<std::string> parter_merchant_list_;
  raw_ptr<CouponService, DanglingUntriaged> coupon_service_;
};

IN_PROC_BROWSER_TEST_F(FetchFLCodeDiscountWorkerBrowserTest,
                       TwoCartsOneWithDiscountOneWithoutDiscount) {
  embedded_test_server()->StartAcceptingConnections();

  CreateCart("merchant0.com",
             BuildCartProto("merchant0.com", "https://www.merchant0.com/cart"));
  CreateCart("merchant1.com",
             BuildCartProto("merchant1.com", "https://www.merchant1.com/cart"));

  StartGettingDiscount();
  waitForDiscounts("merchant0.com");
  waitForDiscounts("merchant1.com");

  // Verify discounts.
  EXPECT_THAT(coupon_service_->GetFreeListingCouponsForUrl(
                  GURL("https://www.merchant0.com/cart")),
              testing::IsEmpty());

  autofill::DisplayStrings expected_display_string;
  expected_display_string.value_prop_text = "Save $10 on Running shoes.";
  EXPECT_THAT(
      coupon_service_->GetFreeListingCouponsForUrl(
          GURL("https://www.merchant1.com/cart")),
      ElementsAre(testing::AllOf(
          testing::Pointee(testing::Property(
              "offer_id", &autofill::AutofillOfferData::GetOfferId,
              testing::Eq(1))),
          testing::Pointee(testing::Property(
              "promo_code", &autofill::AutofillOfferData::GetPromoCode,
              testing::Eq("SAVE$10"))),
          testing::Pointee(testing::Property(
              "expiry", &autofill::AutofillOfferData::GetExpiry,
              testing::Eq(base::Time::FromSecondsSinceUnixEpoch(1635204292)))),
          testing::Pointee(testing::Property(
              "display_strings",
              &autofill::AutofillOfferData::GetDisplayStrings,
              EqualsDisplayStrings(expected_display_string))))));
}

IN_PROC_BROWSER_TEST_F(FetchFLCodeDiscountWorkerBrowserTest,
                       SimplePercentDiscountWithCodeTest) {
  embedded_test_server()->StartAcceptingConnections();

  CreateCart("merchant2.com",
             BuildCartProto("merchant2.com", "https://www.merchant2.com/cart"));

  StartGettingDiscount();
  waitForDiscounts("merchant2.com");

  // Verify discounts.
  autofill::DisplayStrings expected_display_string;
  expected_display_string.value_prop_text = "Save 10% on Running shoes.";
  EXPECT_THAT(coupon_service_->GetFreeListingCouponsForUrl(
                  GURL("https://www.merchant2.com/cart")),
              ElementsAre(testing::AllOf(
                  testing::Pointee(testing::Property(
                      "offer_id", &autofill::AutofillOfferData::GetOfferId,
                      testing::Eq(1))),
                  testing::Pointee(testing::Property(
                      "promo_code", &autofill::AutofillOfferData::GetPromoCode,
                      testing::Eq("SAVE10"))),
                  testing::Pointee(testing::Property(
                      "expiry", &autofill::AutofillOfferData::GetExpiry,
                      testing::Eq(base::Time::FromSecondsSinceUnixEpoch(
                          1635204292.2)))),
                  testing::Pointee(testing::Property(
                      "display_strings",
                      &autofill::AutofillOfferData::GetDisplayStrings,
                      EqualsDisplayStrings(expected_display_string))))));
}

IN_PROC_BROWSER_TEST_F(FetchFLCodeDiscountWorkerBrowserTest,
                       IgnoreNotSupportedType_RBD_WITH_CODE) {
  embedded_test_server()->StartAcceptingConnections();

  CreateCart("merchant3.com",
             BuildCartProto("merchant3.com", "https://www.merchant3.com/cart"));

  StartGettingDiscount();
  waitForDiscounts("merchant3.com");

  // Verify discounts.
  EXPECT_THAT(coupon_service_->GetFreeListingCouponsForUrl(
                  GURL("https://www.merchant3.com/cart")),
              testing::IsEmpty());
}

class FetchCodeBasedDiscountWorkerBrowserTest
    : public FetchDiscountWorkerBrowserTest {
 public:
  FetchCodeBasedDiscountWorkerBrowserTest() {
    parter_merchant_list_.push_back("merchant0.com");
    parter_merchant_list_.push_back("merchant1.com");
    parter_merchant_list_.push_back("merchant2.com");
    parter_merchant_list_.push_back("merchant3.com");
    parter_merchant_list_.push_back("merchant4.com");
  }

  void SetUpOnMainThread() override {
    FetchDiscountWorkerBrowserTest::SetUpOnMainThread();
    Profile* profile = browser()->profile();
    coupon_service_ = CouponServiceFactory::GetForProfile(profile);
    coupon_service_->MaybeFeatureStatusChanged(true);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    std::vector<base::test::FeatureRefAndParams> enabled_features;
    base::FieldTrialParams cart_params, coupon_params, code_based_rbd_param;
    cart_params[ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam] =
        "true";
    cart_params["CartDiscountFetcherEndpointParam"] =
        embedded_test_server()
            ->GetURL("/coupons/codebased_discounts.json")
            .spec();
    enabled_features.emplace_back(ntp_features::kNtpChromeCartModule,
                                  cart_params);

    code_based_rbd_param[commerce::kCodeBasedRuleDiscountParam] = "true";
    enabled_features.emplace_back(commerce::kCodeBasedRBD,
                                  code_based_rbd_param);

    coupon_params["coupon-partner-merchant-pattern"] =
        BuildPartnerMerchantPattern(parter_merchant_list_);
    coupon_params[commerce::kRetailCouponsWithCodeParam] = "true";
    enabled_features.emplace_back(commerce::kRetailCoupons, coupon_params);
    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled_features,
        /*disabled_features*/ {
            optimization_guide::features::kOptimizationHints});
  }

 protected:
  std::vector<std::string> parter_merchant_list_;
  raw_ptr<CouponService, DanglingUntriaged> coupon_service_;
};

IN_PROC_BROWSER_TEST_F(FetchCodeBasedDiscountWorkerBrowserTest,
                       TwoCartsOneWithDiscountOneWithoutDiscount) {
  embedded_test_server()->StartAcceptingConnections();

  CreateCart("merchant0.com",
             BuildCartProto("merchant0.com", "https://www.merchant0.com/cart"));
  CreateCart("merchant1.com",
             BuildCartProto("merchant1.com", "https://www.merchant1.com/cart"));

  StartGettingDiscount();
  waitForDiscounts("merchant0.com");
  waitForDiscounts("merchant1.com");

  // Verify discounts.
  EXPECT_THAT(coupon_service_->GetFreeListingCouponsForUrl(
                  GURL("https://www.merchant0.com/cart")),
              testing::IsEmpty());

  autofill::DisplayStrings expected_display_string;
  expected_display_string.value_prop_text = "Save $10 on Running shoes.";
  EXPECT_THAT(
      coupon_service_->GetFreeListingCouponsForUrl(
          GURL("https://www.merchant1.com/cart")),
      ElementsAre(testing::AllOf(
          testing::Pointee(testing::Property(
              "offer_id", &autofill::AutofillOfferData::GetOfferId,
              testing::Eq(1))),
          testing::Pointee(testing::Property(
              "promo_code", &autofill::AutofillOfferData::GetPromoCode,
              testing::Eq("SAVE$10"))),
          testing::Pointee(testing::Property(
              "expiry", &autofill::AutofillOfferData::GetExpiry,
              testing::Eq(base::Time::FromSecondsSinceUnixEpoch(1635204292)))),
          testing::Pointee(testing::Property(
              "display_strings",
              &autofill::AutofillOfferData::GetDisplayStrings,
              EqualsDisplayStrings(expected_display_string))))));
}

IN_PROC_BROWSER_TEST_F(FetchCodeBasedDiscountWorkerBrowserTest,
                       SimplePercentDiscountWithCodeTest) {
  embedded_test_server()->StartAcceptingConnections();

  CreateCart("merchant2.com",
             BuildCartProto("merchant2.com", "https://www.merchant2.com/cart"));

  StartGettingDiscount();
  waitForDiscounts("merchant2.com");

  // Verify discounts.
  autofill::DisplayStrings expected_display_string;
  expected_display_string.value_prop_text = "Save 10% on Running shoes.";
  EXPECT_THAT(coupon_service_->GetFreeListingCouponsForUrl(
                  GURL("https://www.merchant2.com/cart")),
              ElementsAre(testing::AllOf(
                  testing::Pointee(testing::Property(
                      "offer_id", &autofill::AutofillOfferData::GetOfferId,
                      testing::Eq(1))),
                  testing::Pointee(testing::Property(
                      "promo_code", &autofill::AutofillOfferData::GetPromoCode,
                      testing::Eq("SAVE10"))),
                  testing::Pointee(testing::Property(
                      "expiry", &autofill::AutofillOfferData::GetExpiry,
                      testing::Eq(base::Time::FromSecondsSinceUnixEpoch(
                          1635204292.2)))),
                  testing::Pointee(testing::Property(
                      "display_strings",
                      &autofill::AutofillOfferData::GetDisplayStrings,
                      EqualsDisplayStrings(expected_display_string))))));
}

IN_PROC_BROWSER_TEST_F(
    FetchCodeBasedDiscountWorkerBrowserTest,
    SimulateServerFlagIsOffByNotReturningTheRBDWithCodeType) {
  embedded_test_server()->StartAcceptingConnections();

  CreateCart("merchant3.com",
             BuildCartProto("merchant3.com", "https://www.merchant3.com/cart"));
  CreateCart("merchant4.com",
             BuildCartProto("merchant4.com", "https://www.merchant4.com/cart"));

  StartGettingDiscount();
  waitForDiscounts("merchant3.com");
  waitForDiscounts("merchant4.com");

  // Verify discounts.
  autofill::DisplayStrings expected_display_string;
  expected_display_string.value_prop_text = "Save 10% on Running shoes.";
  EXPECT_THAT(
      coupon_service_->GetFreeListingCouponsForUrl(
          GURL("https://www.merchant3.com/cart")),
      ElementsAre(testing::AllOf(
          testing::Pointee(testing::Property(
              "offer_id", &autofill::AutofillOfferData::GetOfferId,
              testing::Eq(1))),
          testing::Pointee(testing::Property(
              "promo_code", &autofill::AutofillOfferData::GetPromoCode,
              testing::Eq("SAVE10"))),
          testing::Pointee(testing::Property(
              "expiry", &autofill::AutofillOfferData::GetExpiry,
              testing::Eq(base::Time::FromSecondsSinceUnixEpoch(1635204293)))),
          testing::Pointee(testing::Property(
              "display_strings",
              &autofill::AutofillOfferData::GetDisplayStrings,
              EqualsDisplayStrings(expected_display_string))))));

  EXPECT_THAT(coupon_service_->GetFreeListingCouponsForUrl(
                  GURL("https://www.merchant4.com/cart")),
              testing::IsEmpty());
}

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_handler.h"

#include "base/optional.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/cart/cart_db_content.pb.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/persisted_state_db/profile_proto_db.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
void GetEvaluationMerchantCarts(
    base::OnceClosure closure,
    std::vector<chrome_cart::mojom::MerchantCartPtr> expected,
    std::vector<chrome_cart::mojom::MerchantCartPtr> found) {
  ASSERT_EQ(expected.size(), found.size());
  for (size_t i = 0; i < expected.size(); i++) {
    ASSERT_EQ(expected[i]->merchant, found[i]->merchant);
    ASSERT_EQ(expected[i]->cart_url, found[i]->cart_url);
    ASSERT_EQ(expected[i]->product_image_urls.size(),
              found[i]->product_image_urls.size());
    for (size_t j = 0; j < expected[i]->product_image_urls.size(); j++) {
      ASSERT_EQ(expected[i]->product_image_urls[i],
                found[i]->product_image_urls[i]);
    }
  }
  std::move(closure).Run();
}

cart_db::ChromeCartContentProto BuildProto(const char* key,
                                           const char* domain,
                                           const char* merchant_url) {
  cart_db::ChromeCartContentProto proto;
  proto.set_key(domain);
  proto.set_merchant(domain);
  proto.set_merchant_cart_url(merchant_url);
  proto.set_timestamp(base::Time::Now().ToDoubleT());
  return proto;
}

const char kFakeMerchantKey[] = "Fake:foo.com";
const char kFakeMerchant[] = "foo.com";
const char kFakeMerchantURL[] = "https://www.foo.com";
const char kMockMerchantBKey[] = "bar.com";
const char kMockMerchantB[] = "bar.com";
const char kMockMerchantURLB[] = "https://www.bar.com";
const cart_db::ChromeCartContentProto kFakeProto =
    BuildProto(kFakeMerchantKey, kFakeMerchant, kFakeMerchantURL);
const cart_db::ChromeCartContentProto kMockProtoB =
    BuildProto(kMockMerchantBKey, kMockMerchantB, kMockMerchantURLB);
const std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
    kExpectedFake = {{kFakeMerchant, kFakeProto}};
const std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
    kExpectedAllData = {
        {kFakeMerchant, kFakeProto},
        {kMockMerchantB, kMockProtoB},
};
const std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
    kEmptyExpected = {};
}  // namespace

class CartHandlerTest : public testing::Test {
 public:
  CartHandlerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    testing::Test::SetUp();

    handler_ = std::make_unique<CartHandler>(
        mojo::PendingReceiver<chrome_cart::mojom::CartHandler>(), &profile_);
    service_ = CartServiceFactory::GetForProfile(&profile_);
  }

  void OperationEvaluation(base::OnceClosure closure,
                           bool expected_success,
                           bool actual_success) {
    EXPECT_EQ(expected_success, actual_success);
    std::move(closure).Run();
  }

  void GetEvaluationCartHiddenStatus(
      base::OnceClosure closure,
      bool isHidden,
      bool result,
      std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
          found) {
    EXPECT_EQ(1U, found.size());
    EXPECT_EQ(isHidden, found[0].second.is_hidden());
    std::move(closure).Run();
  }

  void GetEvaluationCartRemovedStatus(
      base::OnceClosure closure,
      bool isRemoved,
      bool result,
      std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
          found) {
    EXPECT_EQ(1U, found.size());
    EXPECT_EQ(isRemoved, found[0].second.is_removed());
    std::move(closure).Run();
  }

  void GetEvaluationShouldShowWelcomSurface(base::OnceClosure closure,
                                            bool expected_show,
                                            bool actual_show) {
    EXPECT_EQ(expected_show, actual_show);
    std::move(closure).Run();
  }

  void TearDown() override {}

 protected:
  // Required to run tests from UI thread.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<CartHandler> handler_;
  CartService* service_;
  base::HistogramTester histogram_tester_;
};

// Verifies the hide status is flipped by hiding and restoring.
TEST_F(CartHandlerTest, TestHideStatusChange) {
  ASSERT_FALSE(service_->IsHidden());

  handler_->HideCartModule();
  ASSERT_TRUE(service_->IsHidden());

  handler_->RestoreHiddenCartModule();
  ASSERT_FALSE(service_->IsHidden());
}

// Verifies the remove status is flipped by removing and restoring.
TEST_F(CartHandlerTest, TestRemoveStatusChange) {
  ASSERT_FALSE(service_->IsRemoved());

  handler_->RemoveCartModule();
  ASSERT_TRUE(service_->IsRemoved());

  handler_->RestoreRemovedCartModule();
  ASSERT_FALSE(service_->IsRemoved());
}

// Verifies GetMerchantCarts loads fake data with feature parameter.
//
// Disabled due to excessive flakiness. http://crbug.com/1175279
TEST_F(CartHandlerTest, DISABLED_TestEnableFakeData) {
  base::RunLoop run_loop;
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpChromeCartModule,
      {{"NtpChromeCartModuleDataParam", "fake"}});
  service_->AddCart(kFakeMerchantKey, base::nullopt, kFakeProto);
  service_->AddCart(kMockMerchantBKey, base::nullopt, kMockProtoB);
  task_environment_.RunUntilIdle();

  std::vector<chrome_cart::mojom::MerchantCartPtr> carts;
  auto dummy_cart1 = chrome_cart::mojom::MerchantCart::New();
  dummy_cart1->merchant = kFakeMerchant;
  dummy_cart1->cart_url = GURL(kFakeMerchantURL);
  carts.push_back(std::move(dummy_cart1));

  handler_->GetMerchantCarts(base::BindOnce(
      &GetEvaluationMerchantCarts, run_loop.QuitClosure(), std::move(carts)));
  run_loop.Run();
}

// Verifies GetMerchantCarts loads real data without fake data parameter.
TEST_F(CartHandlerTest, TestDisableFakeData) {
  base::RunLoop run_loop;
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(ntp_features::kNtpChromeCartModule);
  service_->AddCart(kFakeMerchantKey, base::nullopt, kFakeProto);
  service_->AddCart(kMockMerchantBKey, base::nullopt, kMockProtoB);
  task_environment_.RunUntilIdle();

  std::vector<chrome_cart::mojom::MerchantCartPtr> carts;
  auto dummy_cart1 = chrome_cart::mojom::MerchantCart::New();
  dummy_cart1->merchant = kFakeMerchant;
  dummy_cart1->cart_url = GURL(kFakeMerchantURL);
  auto dummy_cart2 = chrome_cart::mojom::MerchantCart::New();
  dummy_cart2->merchant = kMockMerchantB;
  dummy_cart2->cart_url = GURL(kMockMerchantURLB);
  carts.push_back(std::move(dummy_cart2));
  carts.push_back(std::move(dummy_cart1));

  handler_->GetMerchantCarts(base::BindOnce(
      &GetEvaluationMerchantCarts, run_loop.QuitClosure(), std::move(carts)));
  run_loop.Run();
}

// Tests hiding a single cart and undoing the hide.
TEST_F(CartHandlerTest, TestHideCart) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[6];
  cart_db_->AddCart(
      kMockMerchantBKey, kMockProtoB,
      base::BindOnce(&CartHandlerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->LoadCart(
      kMockMerchantB,
      base::BindOnce(&CartHandlerTest::GetEvaluationCartHiddenStatus,
                     base::Unretained(this), run_loop[1].QuitClosure(), false));
  run_loop[1].Run();

  handler_->HideCart(
      GURL(kMockMerchantURLB),
      base::BindOnce(&CartHandlerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), true));
  run_loop[2].Run();

  service_->LoadCart(
      kMockMerchantB,
      base::BindOnce(&CartHandlerTest::GetEvaluationCartHiddenStatus,
                     base::Unretained(this), run_loop[3].QuitClosure(), true));
  run_loop[3].Run();

  handler_->RestoreHiddenCart(
      GURL(kMockMerchantURLB),
      base::BindOnce(&CartHandlerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[4].QuitClosure(), true));
  run_loop[4].Run();

  service_->LoadCart(
      kMockMerchantB,
      base::BindOnce(&CartHandlerTest::GetEvaluationCartHiddenStatus,
                     base::Unretained(this), run_loop[5].QuitClosure(), false));
  run_loop[5].Run();
}

// Tests removing a single cart and undoing the remove.
TEST_F(CartHandlerTest, TestRemoveCart) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[6];
  cart_db_->AddCart(
      kMockMerchantB, kMockProtoB,
      base::BindOnce(&CartHandlerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->LoadCart(
      kMockMerchantB,
      base::BindOnce(&CartHandlerTest::GetEvaluationCartRemovedStatus,
                     base::Unretained(this), run_loop[1].QuitClosure(), false));
  run_loop[1].Run();

  handler_->RemoveCart(
      GURL(kMockMerchantURLB),
      base::BindOnce(&CartHandlerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), true));
  run_loop[2].Run();

  service_->LoadCart(
      kMockMerchantB,
      base::BindOnce(&CartHandlerTest::GetEvaluationCartRemovedStatus,
                     base::Unretained(this), run_loop[3].QuitClosure(), true));
  run_loop[3].Run();

  handler_->RestoreRemovedCart(
      GURL(kMockMerchantURLB),
      base::BindOnce(&CartHandlerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[4].QuitClosure(), true));
  run_loop[4].Run();

  service_->LoadCart(
      kMockMerchantB,
      base::BindOnce(&CartHandlerTest::GetEvaluationCartRemovedStatus,
                     base::Unretained(this), run_loop[5].QuitClosure(), false));
  run_loop[5].Run();
}

// Tests show welcome surface for first three appearances of cart module.
TEST_F(CartHandlerTest, TestShowWelcomeSurface) {
  base::RunLoop run_loop[11];
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(ntp_features::kNtpChromeCartModule);

  // Add a cart with product image.
  CartDB* cart_db_ = service_->GetDB();
  const char image_url[] = "www.image1.com";
  cart_db::ChromeCartContentProto merchant_proto =
      BuildProto(kMockMerchantBKey, kMockMerchantB, kMockMerchantURLB);
  merchant_proto.add_product_image_urls(image_url);
  cart_db_->AddCart(
      kMockMerchantBKey, merchant_proto,
      base::BindOnce(&CartHandlerTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  // Show welcome surface for the first three appearances.
  for (int i = 0; i < CartService::kWelcomSurfaceShowLimit; i++) {
    // Build a callback result without product image.
    auto dummy_cart1 = chrome_cart::mojom::MerchantCart::New();
    dummy_cart1->merchant = kMockMerchantB;
    dummy_cart1->cart_url = GURL(kMockMerchantURLB);
    std::vector<chrome_cart::mojom::MerchantCartPtr> carts_without_product;
    carts_without_product.push_back(std::move(dummy_cart1));

    handler_->GetWarmWelcomeVisible(base::BindOnce(
        &CartHandlerTest::GetEvaluationShouldShowWelcomSurface,
        base::Unretained(this), run_loop[2 * i + 1].QuitClosure(), true));
    run_loop[2 * i + 1].Run();
    handler_->GetMerchantCarts(base::BindOnce(
        &GetEvaluationMerchantCarts, run_loop[2 * (i + 1)].QuitClosure(),
        std::move(carts_without_product)));
    run_loop[2 * (i + 1)].Run();
  }

  // Build a callback result with product image.
  auto dummy_cart2 = chrome_cart::mojom::MerchantCart::New();
  dummy_cart2->merchant = kMockMerchantB;
  dummy_cart2->cart_url = GURL(kMockMerchantURLB);
  dummy_cart2->product_image_urls.emplace_back(image_url);
  std::vector<chrome_cart::mojom::MerchantCartPtr> carts_with_product;
  carts_with_product.push_back(std::move(dummy_cart2));

  // Not show welcome surface afterwards.
  handler_->GetWarmWelcomeVisible(
      base::BindOnce(&CartHandlerTest::GetEvaluationShouldShowWelcomSurface,
                     base::Unretained(this), run_loop[9].QuitClosure(), false));
  run_loop[9].Run();
  handler_->GetMerchantCarts(base::BindOnce(&GetEvaluationMerchantCarts,
                                            run_loop[10].QuitClosure(),
                                            std::move(carts_with_product)));
  run_loop[10].Run();
}

// Test cart click index histogram is properly recorded.
TEST_F(CartHandlerTest, TestOnCartItemClicked) {
  handler_->OnCartItemClicked(3);
  ASSERT_EQ(1,
            histogram_tester_.GetBucketCount("NewTabPage.Carts.ClickCart", 3));
  handler_->OnCartItemClicked(2);
  ASSERT_EQ(1,
            histogram_tester_.GetBucketCount("NewTabPage.Carts.ClickCart", 2));
  handler_->OnCartItemClicked(3);
  ASSERT_EQ(2,
            histogram_tester_.GetBucketCount("NewTabPage.Carts.ClickCart", 3));
}

// Test cart item count histogram is properly recorded.
TEST_F(CartHandlerTest, TestOnModuleCreated) {
  handler_->OnModuleCreated(0);
  ASSERT_EQ(1,
            histogram_tester_.GetBucketCount("NewTabPage.Carts.CartCount", 0));
  handler_->OnModuleCreated(1);
  ASSERT_EQ(1,
            histogram_tester_.GetBucketCount("NewTabPage.Carts.CartCount", 1));
  handler_->OnModuleCreated(0);
  ASSERT_EQ(2,
            histogram_tester_.GetBucketCount("NewTabPage.Carts.CartCount", 0));
}

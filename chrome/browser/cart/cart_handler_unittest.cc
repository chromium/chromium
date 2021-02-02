// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_handler.h"
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
  }
  std::move(closure).Run();
}

cart_db::ChromeCartContentProto BuildProto(const char* domain,
                                           const char* merchant_url) {
  cart_db::ChromeCartContentProto proto;
  proto.set_key(domain);
  proto.set_merchant(domain);
  proto.set_merchant_cart_url(merchant_url);
  return proto;
}

const char kFakeMerchant[] = "Fake:A_merchant";
const char kFakeMerchantURL[] = "www.foo.com";
const char kMockMerchantB[] = "B_merchant";
const char kMockMerchantURLB[] = "www.bar.com";
const cart_db::ChromeCartContentProto kFakeProto =
    BuildProto(kFakeMerchant, kFakeMerchantURL);
const cart_db::ChromeCartContentProto kMockProtoB =
    BuildProto(kMockMerchantB, kMockMerchantURLB);
const std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
    kExpectedFake = {{kFakeMerchant, kFakeProto}};
const std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
    kExpectedAllData = {{kFakeMerchant, kFakeProto},
                        {kMockMerchantB, kMockProtoB}};
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

  void TearDown() override {}

 protected:
  // Required to run tests from UI thread.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<CartHandler> handler_;
  CartService* service_;
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
TEST_F(CartHandlerTest, TestEnableFakeData) {
  base::RunLoop run_loop;
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpChromeCartModule,
      {{"NtpChromeCartModuleDataParam", "fake"}});
  service_->AddCart(kFakeMerchant, kFakeProto);
  service_->AddCart(kMockMerchantB, kMockProtoB);

  std::vector<chrome_cart::mojom::MerchantCartPtr> carts;
  auto dummy_cart1 = chrome_cart::mojom::MerchantCart::New();
  dummy_cart1->merchant = "Fake:A_merchant";
  dummy_cart1->cart_url = GURL("www.foo.com");
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
  service_->AddCart(kFakeMerchant, kFakeProto);
  service_->AddCart(kMockMerchantB, kMockProtoB);

  std::vector<chrome_cart::mojom::MerchantCartPtr> carts;
  auto dummy_cart1 = chrome_cart::mojom::MerchantCart::New();
  dummy_cart1->merchant = "B_merchant";
  dummy_cart1->cart_url = GURL("www.bar.com");
  auto dummy_cart2 = chrome_cart::mojom::MerchantCart::New();
  dummy_cart2->merchant = "Fake:A_merchant";
  dummy_cart2->cart_url = GURL("www.foo.com");
  carts.push_back(std::move(dummy_cart1));
  carts.push_back(std::move(dummy_cart2));

  handler_->GetMerchantCarts(base::BindOnce(
      &GetEvaluationMerchantCarts, run_loop.QuitClosure(), std::move(carts)));
  run_loop.Run();
}

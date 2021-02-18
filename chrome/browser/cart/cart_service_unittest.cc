// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/cart/cart_db_content.pb.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/persisted_state_db/profile_proto_db.h"
#include "chrome/browser/persisted_state_db/profile_proto_db_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
cart_db::ChromeCartContentProto BuildProto(const char* domain,
                                           const char* merchant_url) {
  cart_db::ChromeCartContentProto proto;
  proto.set_key(domain);
  proto.set_merchant_cart_url(merchant_url);
  proto.set_timestamp(base::Time::Now().ToDoubleT());
  return proto;
}

constexpr char kFakeDataPrefix[] = "Fake:";
const char kMockMerchantA[] = "foo.com";
const char kMockMerchantURLA[] = "https://www.foo.com";
const char kMockMerchantB[] = "bar.com";
const char kMockMerchantURLB[] = "https://www.bar.com";
const char kMockMerchantC[] = "baz.com";
const char kMockMerchantURLC[] = "https://www.baz.com";
const cart_db::ChromeCartContentProto kMockProtoA =
    BuildProto(kMockMerchantA, kMockMerchantURLA);
const cart_db::ChromeCartContentProto kMockProtoB =
    BuildProto(kMockMerchantB, kMockMerchantURLB);
const std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
    kExpectedA = {{kMockMerchantA, kMockProtoA}};
const std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
    kExpectedB = {{kMockMerchantB, kMockProtoB}};
const std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
    kExpectedAB = {
        {kMockMerchantB, kMockProtoB},
        {kMockMerchantA, kMockProtoA},
};
const std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
    kEmptyExpected = {};
}  // namespace

class CartServiceTest : public testing::Test {
 public:
  CartServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    testing::Test::SetUp();

    service_ = CartServiceFactory::GetForProfile(&profile_);
    DCHECK(profile_.CreateHistoryService());
  }

  void OperationEvaluation(base::OnceClosure closure,
                           bool expected_success,
                           bool actual_success) {
    EXPECT_EQ(expected_success, actual_success);
    std::move(closure).Run();
  }

  void GetEvaluationURL(
      base::OnceClosure closure,
      std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
          expected,
      bool result,
      std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
          found) {
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

  void GetEvaluationFakeDataDB(
      base::OnceClosure closure,
      bool result,
      std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
          found) {
    EXPECT_EQ(found.size(), 6U);
    for (CartDB::KeyAndValue proto_pair : found) {
      EXPECT_EQ(proto_pair.second.key().rfind(kFakeDataPrefix, 0), 0U);
    }
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

  void GetEvaluationCartTimeStamp(
      base::OnceClosure closure,
      double expect_timestamp,
      bool result,
      std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
          found) {
    EXPECT_EQ(1U, found.size());
    EXPECT_EQ(expect_timestamp, found[0].second.timestamp());
    std::move(closure).Run();
  }

  std::string getDomainName(base::StringPiece domain) {
    std::string* res = service_->domain_name_mapping_->FindStringKey(domain);
    if (!res)
      return "";
    return *res;
  }

  std::string getDomainCartURL(base::StringPiece domain) {
    std::string* res =
        service_->domain_cart_url_mapping_->FindStringKey(domain);
    if (!res)
      return "";
    return *res;
  }

  void TearDown() override {}

 protected:
  // This needs to be destroyed after task_environment, so that any tasks on
  // other threads that might check if features are enabled complete first.
  base::test::ScopedFeatureList features_;

  // Required to run tests from UI thread.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  CartService* service_;
};

// Verifies the hide status is flipped by hiding and restoring.
TEST_F(CartServiceTest, TestHideStatusChange) {
  ASSERT_FALSE(service_->IsHidden());

  service_->Hide();
  ASSERT_TRUE(service_->IsHidden());

  service_->RestoreHidden();
  ASSERT_FALSE(service_->IsHidden());
}

// Verifies the remove status is flipped by removing and restoring.
TEST_F(CartServiceTest, TestRemoveStatusChange) {
  ASSERT_FALSE(service_->IsRemoved());

  service_->Remove();
  ASSERT_TRUE(service_->IsRemoved());

  service_->RestoreRemoved();
  ASSERT_FALSE(service_->IsRemoved());
}

// Tests adding one cart to the service.
TEST_F(CartServiceTest, TestAddCart) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[2];
  cart_db_->LoadAllCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[0].QuitClosure(), kEmptyExpected));
  run_loop[0].Run();

  service_->AddCart(kMockMerchantA, kMockProtoA);
  task_environment_.RunUntilIdle();

  cart_db_->LoadAllCarts(base::BindOnce(&CartServiceTest::GetEvaluationURL,
                                        base::Unretained(this),
                                        run_loop[1].QuitClosure(), kExpectedA));
  run_loop[1].Run();
}

// Test adding a cart with the same key and no product image won't overwrite
// existing proto.
TEST_F(CartServiceTest, TestAddCartWithNoProductImages) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[3];
  cart_db::ChromeCartContentProto merchant_A_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  merchant_A_proto.set_timestamp(0);
  merchant_A_proto.add_product_image_urls("https://image1.com");
  merchant_A_proto.set_is_hidden(true);
  service_->AddCart(kMockMerchantA, merchant_A_proto);
  task_environment_.RunUntilIdle();

  // Add a new proto with the same key and no product images.
  cart_db::ChromeCartContentProto new_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  new_proto.set_timestamp(1);
  service_->AddCart(kMockMerchantA, new_proto);
  task_environment_.RunUntilIdle();

  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartHiddenStatus,
                     base::Unretained(this), run_loop[0].QuitClosure(), false));
  run_loop[0].Run();
  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartTimeStamp,
                     base::Unretained(this), run_loop[1].QuitClosure(), 1));
  run_loop[1].Run();
  const std::vector<
      ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
      result = {{kMockMerchantA, merchant_A_proto}};
  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), result));
  run_loop[2].Run();
}

// Test adding a cart with the same key and some product images would overwrite
// existing proto.
TEST_F(CartServiceTest, TestAddCartWithProductImages) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[3];
  cart_db::ChromeCartContentProto merchant_A_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  merchant_A_proto.set_timestamp(0);
  merchant_A_proto.add_product_image_urls("https://image1.com");
  merchant_A_proto.set_is_hidden(true);
  service_->AddCart(kMockMerchantA, merchant_A_proto);
  task_environment_.RunUntilIdle();

  // Add a new proto with the same key and some product images.
  cart_db::ChromeCartContentProto new_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  new_proto.set_timestamp(1);
  new_proto.add_product_image_urls("https://image2.com");
  service_->AddCart(kMockMerchantA, new_proto);
  task_environment_.RunUntilIdle();

  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartHiddenStatus,
                     base::Unretained(this), run_loop[0].QuitClosure(), false));
  run_loop[0].Run();
  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartTimeStamp,
                     base::Unretained(this), run_loop[1].QuitClosure(), 1));
  run_loop[1].Run();
  const std::vector<
      ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
      result = {{kMockMerchantA, new_proto}};
  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), result));
  run_loop[2].Run();
}

// Test adding a cart that has been removed would not take effect.
TEST_F(CartServiceTest, TestAddRemovedCart) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[3];
  cart_db::ChromeCartContentProto merchant_A_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  merchant_A_proto.set_timestamp(0);
  merchant_A_proto.add_product_image_urls("https://image1.com");
  merchant_A_proto.set_is_removed(true);
  service_->AddCart(kMockMerchantA, merchant_A_proto);
  task_environment_.RunUntilIdle();

  // Add a new proto with the same key and some product images.
  cart_db::ChromeCartContentProto new_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  new_proto.set_timestamp(2);
  new_proto.add_product_image_urls("https://image2.com");
  service_->AddCart(kMockMerchantA, new_proto);
  task_environment_.RunUntilIdle();

  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartRemovedStatus,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();
  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartTimeStamp,
                     base::Unretained(this), run_loop[1].QuitClosure(), 0));
  run_loop[1].Run();
  const std::vector<
      ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
      result = {{kMockMerchantA, merchant_A_proto}};
  cart_db_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), result));
  run_loop[2].Run();
}

// Tests deleting one cart from the service.
TEST_F(CartServiceTest, TestDeleteCart) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[3];
  cart_db_->AddCart(
      kMockMerchantA, kMockProtoA,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  cart_db_->LoadAllCarts(base::BindOnce(&CartServiceTest::GetEvaluationURL,
                                        base::Unretained(this),
                                        run_loop[1].QuitClosure(), kExpectedA));
  run_loop[1].Run();

  service_->DeleteCart(kMockMerchantA);

  cart_db_->LoadAllCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), kEmptyExpected));
  run_loop[2].Run();
}

// Tests loading one cart from the service.
TEST_F(CartServiceTest, TestLoadCart) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[3];
  cart_db_->AddCart(
      kMockMerchantA, kMockProtoA,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->LoadCart(
      kMockMerchantB,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[1].QuitClosure(), kEmptyExpected));
  run_loop[1].Run();

  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), kExpectedA));
  run_loop[2].Run();
}

// Tests loading all active carts from the service.
TEST_F(CartServiceTest, TestLoadAllActiveCarts) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[8];
  cart_db_->AddCart(
      kMockMerchantA, kMockProtoA,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[1].QuitClosure(), kExpectedA));
  run_loop[1].Run();

  cart_db_->AddCart(
      kMockMerchantB, kMockProtoB,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), true));
  run_loop[2].Run();

  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[3].QuitClosure(), kExpectedAB));
  run_loop[3].Run();

  service_->HideCart(
      GURL(kMockMerchantURLB),
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[4].QuitClosure(), true));
  run_loop[4].Run();

  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[5].QuitClosure(), kExpectedA));
  run_loop[5].Run();

  service_->RemoveCart(
      GURL(kMockMerchantURLA),
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[6].QuitClosure(), true));
  run_loop[6].Run();

  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[7].QuitClosure(), kEmptyExpected));
  run_loop[7].Run();
}

// Verifies the database is cleared when detected history deletion.
TEST_F(CartServiceTest, TestOnHistoryDeletion) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[3];
  cart_db_->AddCart(
      kMockMerchantA, kMockProtoA,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  task_environment_.RunUntilIdle();
  run_loop[0].Run();

  cart_db_->LoadAllCarts(base::BindOnce(&CartServiceTest::GetEvaluationURL,
                                        base::Unretained(this),
                                        run_loop[1].QuitClosure(), kExpectedA));
  task_environment_.RunUntilIdle();
  run_loop[1].Run();

  service_->OnURLsDeleted(
      HistoryServiceFactory::GetForProfile(&profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      history::DeletionInfo(history::DeletionTimeRange::Invalid(), false,
                            history::URLRows(), std::set<GURL>(),
                            base::nullopt));

  cart_db_->LoadAllCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), kEmptyExpected));
  task_environment_.RunUntilIdle();
  run_loop[2].Run();
}

// Tests hiding a single cart and undoing the hide.
TEST_F(CartServiceTest, TestHideCart) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[6];
  cart_db_->AddCart(
      kMockMerchantA, kMockProtoA,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartHiddenStatus,
                     base::Unretained(this), run_loop[1].QuitClosure(), false));
  run_loop[1].Run();

  service_->HideCart(
      GURL(kMockMerchantURLA),
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), true));
  run_loop[2].Run();

  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartHiddenStatus,
                     base::Unretained(this), run_loop[3].QuitClosure(), true));
  run_loop[3].Run();

  service_->RestoreHiddenCart(
      GURL(kMockMerchantURLA),
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[4].QuitClosure(), true));
  run_loop[4].Run();

  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartHiddenStatus,
                     base::Unretained(this), run_loop[5].QuitClosure(), false));
  run_loop[5].Run();
}

// Tests removing a single cart and undoing the remove.
TEST_F(CartServiceTest, TestRemoveCart) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[6];
  cart_db_->AddCart(
      kMockMerchantA, kMockProtoA,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartRemovedStatus,
                     base::Unretained(this), run_loop[1].QuitClosure(), false));
  run_loop[1].Run();

  service_->RemoveCart(
      GURL(kMockMerchantURLA),
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[2].QuitClosure(), true));
  run_loop[2].Run();

  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartRemovedStatus,
                     base::Unretained(this), run_loop[3].QuitClosure(), true));
  run_loop[3].Run();

  service_->RestoreRemovedCart(
      GURL(kMockMerchantURLA),
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[4].QuitClosure(), true));
  run_loop[4].Run();

  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationCartRemovedStatus,
                     base::Unretained(this), run_loop[5].QuitClosure(), false));
  run_loop[5].Run();
}

// Tests after service shutdown, content of removed cart entries are deleted
// from database except for the removed status data.
TEST_F(CartServiceTest, TestRemovedCartsDeleted) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[6];
  cart_db::ChromeCartContentProto merchant_A_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  merchant_A_proto.add_product_image_urls("https://image1.com");
  cart_db_->AddCart(
      kMockMerchantA, merchant_A_proto,
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->RemoveCart(
      GURL(kMockMerchantURLA),
      base::BindOnce(&CartServiceTest::OperationEvaluation,
                     base::Unretained(this), run_loop[1].QuitClosure(), true));
  run_loop[1].Run();

  cart_db_->LoadAllCarts(base::BindOnce(&CartServiceTest::GetEvaluationURL,
                                        base::Unretained(this),
                                        run_loop[2].QuitClosure(), kExpectedA));
  run_loop[2].Run();

  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[3].QuitClosure(), kEmptyExpected));
  run_loop[3].Run();

  service_->Shutdown();
  task_environment_.RunUntilIdle();

  // After shut down, cart content is removed and only the removed status is
  // kept.
  cart_db::ChromeCartContentProto empty_proto;
  empty_proto.set_key(kMockMerchantA);
  empty_proto.set_is_removed(true);
  const std::vector<
      ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
      result = {{kMockMerchantA, empty_proto}};
  cart_db_->LoadAllCarts(base::BindOnce(&CartServiceTest::GetEvaluationURL,
                                        base::Unretained(this),
                                        run_loop[4].QuitClosure(), result));
  run_loop[4].Run();
  cart_db_->LoadAllCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationCartRemovedStatus,
                     base::Unretained(this), run_loop[5].QuitClosure(), true));
  run_loop[5].Run();
}

// Tests whether to show the welcome surface is correctly controlled.
TEST_F(CartServiceTest, TestControlShowWelcomeSurface) {
  const int limit = CartService::kWelcomSurfaceShowLimit;
  for (int i = 0; i < limit; i++) {
    EXPECT_EQ(i, profile_.GetPrefs()->GetInteger(
                     prefs::kCartModuleWelcomeSurfaceShownTimes));
    EXPECT_TRUE(service_->ShouldShowWelcomSurface());
    service_->IncreaseWelcomeSurfaceCounter();
  }
  EXPECT_FALSE(service_->ShouldShowWelcomSurface());
  EXPECT_EQ(limit, profile_.GetPrefs()->GetInteger(
                       prefs::kCartModuleWelcomeSurfaceShownTimes));
}

// Tests cart data is loaded in the order of timestamp.
TEST_F(CartServiceTest, TestOrderInTimestamp) {
  base::RunLoop run_loop[3];
  double time_now = base::Time::Now().ToDoubleT();
  cart_db::ChromeCartContentProto merchant_A_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);
  merchant_A_proto.set_timestamp(time_now);
  cart_db::ChromeCartContentProto merchant_B_proto =
      BuildProto(kMockMerchantB, kMockMerchantURLB);
  merchant_B_proto.set_timestamp(time_now + 1);
  cart_db::ChromeCartContentProto merchant_C_proto =
      BuildProto(kMockMerchantC, kMockMerchantURLC);
  merchant_C_proto.set_timestamp(time_now + 2);
  service_->AddCart(kMockMerchantA, merchant_A_proto);
  service_->AddCart(kMockMerchantB, merchant_B_proto);
  service_->AddCart(kMockMerchantC, merchant_C_proto);
  task_environment_.RunUntilIdle();

  const std::vector<
      ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
      result1 = {{kMockMerchantC, merchant_C_proto},
                 {kMockMerchantB, merchant_B_proto},
                 {kMockMerchantA, merchant_A_proto}};
  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[0].QuitClosure(), result1));
  run_loop[0].Run();

  merchant_A_proto.set_timestamp(time_now + 3);
  service_->AddCart(kMockMerchantA, merchant_A_proto);
  task_environment_.RunUntilIdle();
  const std::vector<
      ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
      result2 = {{kMockMerchantA, merchant_A_proto},
                 {kMockMerchantC, merchant_C_proto},
                 {kMockMerchantB, merchant_B_proto}};
  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[1].QuitClosure(), result2));
  run_loop[1].Run();

  merchant_C_proto.set_timestamp(time_now + 4);
  service_->AddCart(kMockMerchantC, merchant_C_proto);
  task_environment_.RunUntilIdle();
  const std::vector<
      ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
      result3 = {{kMockMerchantC, merchant_C_proto},
                 {kMockMerchantA, merchant_A_proto},
                 {kMockMerchantB, merchant_B_proto}};
  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), result3));
  run_loop[2].Run();
}

// Tests domain to merchant name mapping.
TEST_F(CartServiceTest, TestDomainToNameMapping) {
  EXPECT_EQ("Amazon", getDomainName("amazon.com"));

  EXPECT_EQ("eBay", getDomainName("ebay.com"));

  EXPECT_EQ("", getDomainName("example.com"));
}

// Tests domain to cart URL mapping.
TEST_F(CartServiceTest, TestDomainToCartURLMapping) {
  EXPECT_EQ("https://www.amazon.com/gp/cart/view.html?ref_=nav_cart",
            getDomainCartURL("amazon.com"));

  EXPECT_EQ("https://cart.ebay.com", getDomainCartURL("ebay.com"));

  EXPECT_EQ("", getDomainCartURL("example.com"));
}

class CartServiceTestWithFeature : public CartServiceTest {
 public:
  // Features need to be initialized before CartServiceTest::SetUp runs, in
  // order to avoid tsan data race error on FeatureList.
  CartServiceTestWithFeature() {
    features_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpChromeCartModule,
        {{"NtpChromeCartModuleDataParam", "fake"}});
  }
};

TEST_F(CartServiceTestWithFeature, TestFakeData) {
  base::RunLoop run_loop[2];
  TestingProfile fake_profile;
  CartService* fake_service = CartServiceFactory::GetForProfile(&fake_profile);
  CartDB* fake_db = fake_service->GetDB();

  fake_service->LoadCartsWithFakeData(
      base::BindOnce(&CartServiceTest::GetEvaluationFakeDataDB,
                     base::Unretained(this), run_loop[0].QuitClosure()));
  run_loop[0].Run();

  fake_service->Shutdown();

  fake_db->LoadAllCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[1].QuitClosure(), kEmptyExpected));
  run_loop[1].Run();
}

// Tests expired entries are deleted when data is loaded.
TEST_F(CartServiceTest, TestExpiredDataDeleted) {
  base::RunLoop run_loop[3];
  cart_db::ChromeCartContentProto merchant_proto =
      BuildProto(kMockMerchantA, kMockMerchantURLA);

  merchant_proto.set_timestamp(
      (base::Time::Now() - base::TimeDelta::FromDays(16)).ToDoubleT());
  service_->AddCart(kMockMerchantA, merchant_proto);
  task_environment_.RunUntilIdle();

  // The expired entry is deleted in load results.
  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[0].QuitClosure(), kEmptyExpected));
  run_loop[0].Run();

  // The expired entry is deleted in database.
  service_->LoadCart(
      kMockMerchantA,
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[1].QuitClosure(), kEmptyExpected));
  run_loop[1].Run();

  merchant_proto.set_timestamp(
      (base::Time::Now() - base::TimeDelta::FromDays(13)).ToDoubleT());
  service_->AddCart(kMockMerchantA, merchant_proto);
  task_environment_.RunUntilIdle();

  const std::vector<
      ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>
      result = {{kMockMerchantA, merchant_proto}};
  service_->LoadAllActiveCarts(
      base::BindOnce(&CartServiceTest::GetEvaluationURL, base::Unretained(this),
                     run_loop[2].QuitClosure(), result));
  run_loop[2].Run();
}

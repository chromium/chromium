// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/coupons/coupon_service.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/commerce/coupons/coupon_service_factory.h"
#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/proto/coupon_db_content.pb.h"
#include "components/session_proto_db/session_proto_db.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

coupon_db::CouponContentProto BuildProtoWithOneCoupon(
    const int64_t id,
    const char* origin,
    const char* coupon_description,
    const char* coupon_code) {
  coupon_db::CouponContentProto proto;
  proto.set_key(origin);
  coupon_db::FreeListingCouponInfoProto* coupon_proto =
      proto.add_free_listing_coupons();
  coupon_proto->set_coupon_id(id);
  coupon_proto->set_coupon_code(coupon_code);
  coupon_proto->set_coupon_description(coupon_description);
  return proto;
}

autofill::AutofillOfferData BuildCouponOfferData(const int64_t id,
                                                 const char* origin,
                                                 const char* coupon_description,
                                                 const char* coupon_code) {
  base::Time expiry;
  std::vector<GURL> merchant_origins;
  merchant_origins.emplace_back(origin);
  autofill::DisplayStrings display_strings;
  display_strings.value_prop_text = coupon_description;
  return autofill::AutofillOfferData::FreeListingCouponOffer(
      id, expiry, merchant_origins, /*offer_details_url=*/GURL(),
      display_strings, coupon_code);
}

const int64_t kMockCouponIdA = 135;
const int64_t kMockCouponIdB = 357;
const char kMockMerchantA[] = "https://www.foo.com";
const char kMockMerchantB[] = "https://www.bar.com";
const char kMockCouponDescriptionA[] = "15% off";
const char kMockCouponDescriptionB[] = "$25 off";
const char kMockCouponCodeA[] = "123";
const char kMockCouponCodeB[] = "456";
const coupon_db::CouponContentProto kMockProtoA =
    BuildProtoWithOneCoupon(kMockCouponIdA,
                            kMockMerchantA,
                            kMockCouponDescriptionA,
                            kMockCouponCodeA);
const coupon_db::CouponContentProto kMockProtoB =
    BuildProtoWithOneCoupon(kMockCouponIdA,
                            kMockMerchantB,
                            kMockCouponDescriptionB,
                            kMockCouponCodeB);

using CouponProto =
    std::vector<SessionProtoDB<coupon_db::CouponContentProto>::KeyAndValue>;
using Coupons = std::vector<autofill::AutofillOfferData*>;
using CouponsMap =
    base::flat_map<GURL,
                   std::vector<std::unique_ptr<autofill::AutofillOfferData>>>;

const CouponProto kExpectedA = {{kMockMerchantA, kMockProtoA}};
const CouponProto kExpectedB = {{kMockMerchantB, kMockProtoB}};
const CouponProto kEmptyExpected = {};

struct CouponDataStruct {
  const int64_t id;
  const raw_ref<const GURL> origin;
  // RAW_PTR_EXCLUSION: Tricky to convert from string literal.
  RAW_PTR_EXCLUSION const std::string& description;
  RAW_PTR_EXCLUSION const std::string& coupon_code;
};

}  // namespace

class CouponServiceTest : public testing::Test {
 public:
  CouponServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {
    feature_list_.InitAndEnableFeatureWithParameters(
        commerce::kRetailCoupons,
        {{commerce::kRetailCouponsWithCodeParam, "true"}});
  }

  void SetUp() override {
    testing::Test::SetUp();

    service_ = CouponServiceFactory::GetForProfile(&profile_);
    coupon_db_ = service_->GetDB();
    coupon_data_a_ =
        BuildCouponOfferData(kMockCouponIdA, kMockMerchantA,
                             kMockCouponDescriptionA, kMockCouponCodeA);
    coupon_data_b_ =
        BuildCouponOfferData(kMockCouponIdB, kMockMerchantB,
                             kMockCouponDescriptionB, kMockCouponCodeB);

    // Assume that relevant features are enabled initially.
    service_->MaybeFeatureStatusChanged(true);
  }

  void OperationEvaluation(base::OnceClosure closure,
                           bool expected_success,
                           bool actual_success) {
    EXPECT_EQ(expected_success, actual_success);
    std::move(closure).Run();
  }

  void GetEvaluationCoupons(base::OnceClosure closure,
                            CouponProto expected,
                            bool result,
                            CouponProto found) {
    EXPECT_TRUE(result);
    EXPECT_EQ(found.size(), expected.size());
    for (size_t i = 0; i < expected.size(); i++) {
      EXPECT_EQ(GURL(found[i].first), GURL(expected[i].first));
      EXPECT_EQ(found[i].second.free_listing_coupons_size(),
                expected[i].second.free_listing_coupons_size());
      for (int j = 0; j < found[i].second.free_listing_coupons_size(); j++) {
        coupon_db::FreeListingCouponInfoProto expected_coupon =
            expected[i].second.free_listing_coupons()[j];
        coupon_db::FreeListingCouponInfoProto found_coupon =
            found[i].second.free_listing_coupons()[j];
        EXPECT_EQ(expected_coupon.coupon_description(),
                  found_coupon.coupon_description());
        EXPECT_EQ(expected_coupon.coupon_code(), found_coupon.coupon_code());
      }
    }
    std::move(closure).Run();
  }

  void GetEvaluationCouponTimestamp(base::OnceClosure closure,
                                    base::Time time_to_compare,
                                    bool should_be_equal,
                                    bool result,
                                    CouponProto found) {
    EXPECT_TRUE(result);
    DCHECK_EQ(found.size(), 1u);
    DCHECK_EQ(found[0].second.free_listing_coupons()[0].last_display_time() ==
                  time_to_compare.InMillisecondsSinceUnixEpoch(),
              should_be_equal);
    std::move(closure).Run();
  }

  void TearDown() override {
    coupon_db_->DeleteAllCoupons();
    task_environment_.RunUntilIdle();
  }

 protected:
  class MockCouponServiceObserver : public CouponServiceObserver {
   public:
    MOCK_METHOD(void,
                OnCouponInvalidated,
                (const autofill::AutofillOfferData& offer_data),
                (override));
  };

  void SetUpCouponMap(std::vector<CouponDataStruct> coupons) {
    CouponsMap coupon_map;
    for (auto coupon : coupons) {
      int64_t offer_id = coupon.id;
      base::Time expiry;
      autofill::DisplayStrings display_strings;
      display_strings.value_prop_text = coupon.description;
      std::string promo_code = coupon.coupon_code;
      std::vector<GURL> merchant_origins;
      merchant_origins.emplace_back(GURL(*coupon.origin));
      auto offer = std::make_unique<autofill::AutofillOfferData>(
          autofill::AutofillOfferData::FreeListingCouponOffer(
              offer_id, expiry, merchant_origins, /*offer_details_url=*/GURL(),
              display_strings, promo_code));
      coupon_map[GURL(*coupon.origin)].emplace_back(std::move(offer));
    }
    service_->UpdateFreeListingCoupons(coupon_map);
    task_environment_.RunUntilIdle();
  }

  void InitializeCoupons() {
    service_->InitializeCouponsMap();
    task_environment_.RunUntilIdle();
  }

  bool IsFeatureEnabled() { return service_->features_enabled_; }

  void CheckCouponServiceObservers(
      std::vector<CouponServiceObserver*> expected_observers) {
    std::vector<CouponServiceObserver*> current_observers;
    for (CouponServiceObserver& observer : service_->observers_) {
      current_observers.emplace_back(&observer);
    }
    EXPECT_EQ(expected_observers, current_observers);
  }

  // This needs to be declared before |task_environment_|, so that it will be
  // destroyed after |task_environment_| has run all the tasks on other threads
  // that might check if a feature is enabled.
  base::test::ScopedFeatureList feature_list_;
  // Required to run tests from UI thread.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  raw_ptr<CouponService> service_;
  raw_ptr<CouponDB> coupon_db_;
  autofill::AutofillOfferData coupon_data_a_;
  autofill::AutofillOfferData coupon_data_b_;
};

TEST_F(CouponServiceTest, TestGetCouponForUrl) {
  GURL orgin_a(kMockMerchantA);
  GURL orgin_b(kMockMerchantB);
  SetUpCouponMap({{kMockCouponIdA, raw_ref(orgin_a), kMockCouponDescriptionA,
                   kMockCouponCodeA},
                  {kMockCouponIdB, raw_ref(orgin_b), kMockCouponDescriptionB,
                   kMockCouponCodeB}});

  Coupons result = service_->GetFreeListingCouponsForUrl(orgin_a);
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(*result[0], coupon_data_a_);

  result = service_->GetFreeListingCouponsForUrl(orgin_b);
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(*result[0], coupon_data_b_);

  result = service_->GetFreeListingCouponsForUrl(
      GURL(std::string(kMockMerchantA) + "/cart"));
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(*result[0], coupon_data_a_);
}

TEST_F(CouponServiceTest, TestUpdateCoupons) {
  base::RunLoop run_loop[1];
  GURL origin = GURL(kMockMerchantA);
  SetUpCouponMap({{kMockCouponIdA, raw_ref(origin), kMockCouponDescriptionA,
                   kMockCouponCodeA}});

  Coupons result = service_->GetFreeListingCouponsForUrl(origin);
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(*result[0], coupon_data_a_);
  coupon_db_->LoadCoupon(
      origin, base::BindOnce(&CouponServiceTest::GetEvaluationCoupons,
                             base::Unretained(this), run_loop[0].QuitClosure(),
                             kExpectedA));
  run_loop[0].Run();
}

TEST_F(CouponServiceTest, TestDeleteCouponForUrl) {
  base::RunLoop run_loop[4];
  GURL orgin_a(kMockMerchantA);
  GURL orgin_b(kMockMerchantB);
  SetUpCouponMap({{kMockCouponIdA, raw_ref(orgin_a), kMockCouponDescriptionA,
                   kMockCouponCodeA},
                  {kMockCouponIdB, raw_ref(orgin_b), kMockCouponDescriptionB,
                   kMockCouponCodeB}});

  Coupons result = service_->GetFreeListingCouponsForUrl(orgin_a);
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(*result[0], coupon_data_a_);
  coupon_db_->LoadCoupon(
      orgin_a, base::BindOnce(&CouponServiceTest::GetEvaluationCoupons,
                              base::Unretained(this), run_loop[0].QuitClosure(),
                              kExpectedA));
  run_loop[0].Run();

  result = service_->GetFreeListingCouponsForUrl(orgin_b);
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(*result[0], coupon_data_b_);
  coupon_db_->LoadCoupon(
      orgin_b, base::BindOnce(&CouponServiceTest::GetEvaluationCoupons,
                              base::Unretained(this), run_loop[1].QuitClosure(),
                              kExpectedB));
  run_loop[1].Run();

  service_->DeleteFreeListingCouponsForUrl(orgin_a);

  result = service_->GetFreeListingCouponsForUrl(orgin_a);
  EXPECT_EQ(result.size(), 0u);
  coupon_db_->LoadCoupon(
      orgin_a, base::BindOnce(&CouponServiceTest::GetEvaluationCoupons,
                              base::Unretained(this), run_loop[2].QuitClosure(),
                              kEmptyExpected));
  run_loop[2].Run();

  GURL url_b(std::string(kMockMerchantB) + "/cart");
  service_->DeleteFreeListingCouponsForUrl(url_b);

  result = service_->GetFreeListingCouponsForUrl(orgin_b);
  EXPECT_EQ(result.size(), 0u);
  coupon_db_->LoadCoupon(
      orgin_b, base::BindOnce(&CouponServiceTest::GetEvaluationCoupons,
                              base::Unretained(this), run_loop[3].QuitClosure(),
                              kEmptyExpected));
  run_loop[3].Run();
}

TEST_F(CouponServiceTest, TestInitialization) {
  GURL origin = GURL(kMockMerchantA);
  coupon_db_->AddCoupon(origin, kMockProtoA);
  Coupons result = service_->GetFreeListingCouponsForUrl(origin);
  EXPECT_EQ(result.size(), 0u);

  InitializeCoupons();

  result = service_->GetFreeListingCouponsForUrl(origin);
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(*result[0], coupon_data_a_);
}

TEST_F(CouponServiceTest, TestDeleteAllCoupons) {
  base::RunLoop run_loop[1];
  GURL orgin_a(kMockMerchantA);
  GURL orgin_b(kMockMerchantB);
  SetUpCouponMap({{kMockCouponIdA, raw_ref(orgin_a), kMockCouponDescriptionA,
                   kMockCouponCodeA},
                  {kMockCouponIdB, raw_ref(orgin_b), kMockCouponDescriptionB,
                   kMockCouponCodeB}});

  service_->DeleteAllFreeListingCoupons();

  Coupons result = service_->GetFreeListingCouponsForUrl(orgin_a);
  EXPECT_EQ(result.size(), 0u);
  result = service_->GetFreeListingCouponsForUrl(orgin_b);
  EXPECT_EQ(result.size(), 0u);
  coupon_db_->LoadAllCoupons(base::BindOnce(
      &CouponServiceTest::GetEvaluationCoupons, base::Unretained(this),
      run_loop[0].QuitClosure(), kEmptyExpected));
  run_loop[0].Run();
}

TEST_F(CouponServiceTest, TestIsUrlEligible) {
  GURL origin = GURL("https://www.example.com");
  SetUpCouponMap({{kMockCouponIdA, raw_ref(origin), kMockCouponDescriptionA,
                   kMockCouponCodeA}});

  EXPECT_TRUE(service_->IsUrlEligible(GURL("https://www.example.com")));
  EXPECT_TRUE(service_->IsUrlEligible(GURL("https://www.example.com/first")));
  EXPECT_FALSE(service_->IsUrlEligible(GURL("https://www.test.com")));
}

TEST_F(CouponServiceTest, TestRecordCouponDisplayTimestamp) {
  base::RunLoop run_loop[2];
  GURL origin = GURL(kMockMerchantA);
  SetUpCouponMap({{kMockCouponIdA, raw_ref(origin), kMockCouponDescriptionA,
                   kMockCouponCodeA}});
  Coupons result = service_->GetFreeListingCouponsForUrl(origin);
  EXPECT_EQ(result.size(), 1u);
  autofill::AutofillOfferData* offer = result[0];
  EXPECT_EQ(*offer, coupon_data_a_);
  EXPECT_EQ(service_->GetCouponDisplayTimestamp(*offer), base::Time());
  coupon_db_->LoadCoupon(
      origin, base::BindOnce(&CouponServiceTest::GetEvaluationCouponTimestamp,
                             base::Unretained(this), run_loop[0].QuitClosure(),
                             base::Time(), true));
  run_loop[0].Run();

  service_->RecordCouponDisplayTimestamp(*offer);
  task_environment_.RunUntilIdle();

  result = service_->GetFreeListingCouponsForUrl(origin);
  EXPECT_EQ(result.size(), 1u);
  offer = result[0];
  EXPECT_EQ(*offer, coupon_data_a_);
  EXPECT_GT(service_->GetCouponDisplayTimestamp(*offer), base::Time());
  EXPECT_LT(service_->GetCouponDisplayTimestamp(*offer), base::Time::Now());
  coupon_db_->LoadCoupon(
      origin,
      base::BindOnce(&CouponServiceTest::GetEvaluationCouponTimestamp,
                     base::Unretained(this), run_loop[1].QuitClosure(),
                     service_->GetCouponDisplayTimestamp(*offer), true));
  run_loop[1].Run();
}

TEST_F(CouponServiceTest, TestUpdateCoupons_NotOverwriteLastDisplayTime) {
  base::RunLoop run_loop[1];
  GURL origin = GURL(kMockMerchantA);
  SetUpCouponMap({{kMockCouponIdA, raw_ref(origin), kMockCouponDescriptionA,
                   kMockCouponCodeA}});
  Coupons result = service_->GetFreeListingCouponsForUrl(origin);
  base::Time timestamp = service_->GetCouponDisplayTimestamp(*result[0]);
  EXPECT_EQ(timestamp, base::Time());

  service_->RecordCouponDisplayTimestamp(*result[0]);
  task_environment_.RunUntilIdle();
  result = service_->GetFreeListingCouponsForUrl(origin);
  EXPECT_EQ(result.size(), 1u);
  timestamp = service_->GetCouponDisplayTimestamp(*result[0]);
  EXPECT_NE(timestamp, base::Time());

  SetUpCouponMap({{kMockCouponIdA, raw_ref(origin), kMockCouponDescriptionA,
                   kMockCouponCodeA}});

  result = service_->GetFreeListingCouponsForUrl(origin);
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(service_->GetCouponDisplayTimestamp(*result[0]), timestamp);
  coupon_db_->LoadCoupon(
      origin, base::BindOnce(&CouponServiceTest::GetEvaluationCouponTimestamp,
                             base::Unretained(this), run_loop[0].QuitClosure(),
                             timestamp, true));
  run_loop[0].Run();
}

TEST_F(CouponServiceTest, TestUpdateCoupons_OldCouponDisplayTimeRemoved) {
  GURL origin_A = GURL(kMockMerchantA);
  SetUpCouponMap({{kMockCouponIdA, raw_ref(origin_A), kMockCouponDescriptionA,
                   kMockCouponCodeA}});
  Coupons result = service_->GetFreeListingCouponsForUrl(origin_A);
  EXPECT_EQ(service_->GetCouponDisplayTimestamp(*result[0]), base::Time());

  service_->RecordCouponDisplayTimestamp(*result[0]);
  task_environment_.RunUntilIdle();
  result = service_->GetFreeListingCouponsForUrl(origin_A);
  EXPECT_EQ(result.size(), 1u);
  autofill::AutofillOfferData offer_A = *result[0];
  EXPECT_NE(service_->GetCouponDisplayTimestamp(offer_A), base::Time());

  // Set up with new coupons where the existing coupon is no longer valid.
  GURL origin_B = GURL(kMockMerchantB);
  SetUpCouponMap({{kMockCouponIdB, raw_ref(origin_B), kMockCouponDescriptionB,
                   kMockCouponCodeB}});

  EXPECT_EQ(service_->GetFreeListingCouponsForUrl(origin_A).size(), 0u);
  EXPECT_EQ(service_->GetCouponDisplayTimestamp(offer_A), base::Time());
}

TEST_F(CouponServiceTest, MaybeFeatureStatusChanged_FeatureDisabled) {
  base::RunLoop run_loop[2];
  const GURL origin(kMockMerchantA);
  SetUpCouponMap({{kMockCouponIdA, raw_ref(origin), kMockCouponDescriptionA,
                   kMockCouponCodeA}});
  EXPECT_TRUE(IsFeatureEnabled());
  EXPECT_TRUE(service_->IsUrlEligible(origin));
  Coupons result = service_->GetFreeListingCouponsForUrl(origin);
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(*result[0], coupon_data_a_);
  coupon_db_->LoadCoupon(
      origin, base::BindOnce(&CouponServiceTest::GetEvaluationCoupons,
                             base::Unretained(this), run_loop[0].QuitClosure(),
                             kExpectedA));
  run_loop[0].Run();

  service_->MaybeFeatureStatusChanged(false);

  EXPECT_FALSE(IsFeatureEnabled());
  EXPECT_FALSE(service_->IsUrlEligible(origin));
  EXPECT_EQ(service_->GetFreeListingCouponsForUrl(origin).size(), 0u);
  coupon_db_->LoadCoupon(
      origin, base::BindOnce(&CouponServiceTest::GetEvaluationCoupons,
                             base::Unretained(this), run_loop[1].QuitClosure(),
                             kEmptyExpected));
  run_loop[1].Run();
  SetUpCouponMap({{kMockCouponIdA, raw_ref(origin), kMockCouponDescriptionA,
                   kMockCouponCodeA}});
  EXPECT_EQ(service_->GetFreeListingCouponsForUrl(origin).size(), 0u);
}

// Test CouponServiceObservers are correctly registered/unregistered.
TEST_F(CouponServiceTest, TestSetupCouponServiceObserver) {
  MockCouponServiceObserver observer;
  service_->AddObserver(&observer);
  CheckCouponServiceObservers({&observer});
  service_->RemoveObserver(&observer);
  CheckCouponServiceObservers({});
}

// Test CouponServiceObservers should be notified when a coupon is deleted.
TEST_F(CouponServiceTest, TestDeleteCouponForUrl_NotifyObserver) {
  MockCouponServiceObserver observer;
  service_->AddObserver(&observer);
  CheckCouponServiceObservers({&observer});
  GURL origin(kMockMerchantA);
  SetUpCouponMap({{kMockCouponIdA, raw_ref(origin), kMockCouponDescriptionA,
                   kMockCouponCodeA}});
  Coupons result = service_->GetFreeListingCouponsForUrl(origin);
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(*result[0], coupon_data_a_);

  EXPECT_CALL(observer, OnCouponInvalidated(*result[0])).Times(1);
  service_->DeleteFreeListingCouponsForUrl(origin);
}

// Test CouponServiceObservers should be notified when all coupons are deleted.
TEST_F(CouponServiceTest, TestDeleteAllCoupons_NotifyObserver) {
  MockCouponServiceObserver observer;
  service_->AddObserver(&observer);
  CheckCouponServiceObservers({&observer});
  GURL orgin_a(kMockMerchantA);
  GURL orgin_b(kMockMerchantB);
  SetUpCouponMap({{kMockCouponIdA, raw_ref(orgin_a), kMockCouponDescriptionA,
                   kMockCouponCodeA},
                  {kMockCouponIdB, raw_ref(orgin_b), kMockCouponDescriptionB,
                   kMockCouponCodeB}});
  Coupons result = service_->GetFreeListingCouponsForUrl(orgin_a);
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(*result[0], coupon_data_a_);
  auto* couponA = result[0];
  result = service_->GetFreeListingCouponsForUrl(orgin_b);
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(*result[0], coupon_data_b_);
  auto* couponB = result[0];

  EXPECT_CALL(observer, OnCouponInvalidated(*couponA)).Times(1);
  EXPECT_CALL(observer, OnCouponInvalidated(*couponB)).Times(1);
  service_->DeleteAllFreeListingCoupons();
}

// Test CouponServiceObservers should be notified when there is a new batch of
// coupon data arrives, and some of the existing coupons are no longer valid.
TEST_F(CouponServiceTest, TestUpdateCoupons_NotifyObserver) {
  MockCouponServiceObserver observer;
  service_->AddObserver(&observer);
  CheckCouponServiceObservers({&observer});
  GURL orgin_a(kMockMerchantA);
  GURL orgin_b(kMockMerchantB);
  SetUpCouponMap({{kMockCouponIdA, raw_ref(orgin_a), kMockCouponDescriptionA,
                   kMockCouponCodeA},
                  {kMockCouponIdB, raw_ref(orgin_b), kMockCouponDescriptionB,
                   kMockCouponCodeB}});
  Coupons result = service_->GetFreeListingCouponsForUrl(orgin_a);
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(*result[0], coupon_data_a_);
  auto* couponA = result[0];
  result = service_->GetFreeListingCouponsForUrl(orgin_b);
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(*result[0], coupon_data_b_);
  auto* couponB = result[0];

  EXPECT_CALL(observer, OnCouponInvalidated(*couponA)).Times(1);
  EXPECT_CALL(observer, OnCouponInvalidated(*couponB)).Times(0);
  SetUpCouponMap({{kMockCouponIdB, raw_ref(orgin_b), kMockCouponDescriptionB,
                   kMockCouponCodeB}});
}

// Test when a new batch of coupon data arrives, existing coupons that are also
// in the new batch won't be reset.
TEST_F(CouponServiceTest, TestUpdateCoupons_SkipExisting) {
  GURL orgin_a(kMockMerchantA);
  GURL orgin_b(kMockMerchantB);
  SetUpCouponMap({{kMockCouponIdA, raw_ref(orgin_a), kMockCouponDescriptionA,
                   kMockCouponCodeA},
                  {kMockCouponIdB, raw_ref(orgin_b), kMockCouponDescriptionB,
                   kMockCouponCodeB}});
  Coupons result = service_->GetFreeListingCouponsForUrl(orgin_b);
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(*result[0], coupon_data_b_);
  auto* couponB = result[0];

  SetUpCouponMap({{kMockCouponIdB, raw_ref(orgin_b), kMockCouponDescriptionB,
                   kMockCouponCodeB}});
  result = service_->GetFreeListingCouponsForUrl(orgin_b);
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0], couponB);
}

// Test for when coupon feature is disabled.
class CouponServiceFeatureDisabledTest : public CouponServiceTest {
 public:
  CouponServiceFeatureDisabledTest() {
    feature_list_.Reset();
    feature_list_.InitAndDisableFeature(commerce::kRetailCoupons);
  }

  void SetUp() override {
    CouponServiceTest::SetUp();

    // Assume that relevant features are disabled initially.
    service_->MaybeFeatureStatusChanged(false);
  }
};

TEST_F(CouponServiceFeatureDisabledTest, FeatureDisabled) {
  base::RunLoop run_loop[1];
  EXPECT_FALSE(IsFeatureEnabled());
  const GURL origin(kMockMerchantA);

  SetUpCouponMap({{kMockCouponIdA, raw_ref(origin), kMockCouponDescriptionA,
                   kMockCouponCodeA}});
  EXPECT_FALSE(service_->IsUrlEligible(origin));
  Coupons result = service_->GetFreeListingCouponsForUrl(origin);
  EXPECT_EQ(result.size(), 0u);
  coupon_db_->LoadCoupon(
      origin, base::BindOnce(&CouponServiceTest::GetEvaluationCoupons,
                             base::Unretained(this), run_loop[0].QuitClosure(),
                             kEmptyExpected));
  run_loop[0].Run();
}

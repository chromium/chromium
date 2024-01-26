// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/promos/promo_service.h"

#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/new_tab_page/promos/promo_data.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"
#include "url/gurl.h"

using testing::Eq;
using testing::StartsWith;

class PromoServiceTest : public testing::Test {
 public:
  PromoServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void SetUp() override {
    testing::Test::SetUp();

    service_ =
        std::make_unique<PromoService>(test_shared_loader_factory_, &profile_);
  }

  void SetUpResponseWithData(const GURL& load_url,
                             const std::string& response) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {}));
    test_url_loader_factory_.AddResponse(load_url.spec(), response);
  }

  void SetUpResponseWithNetworkError(const GURL& load_url) {
    test_url_loader_factory_.AddResponse(
        load_url, network::mojom::URLResponseHead::New(), std::string(),
        network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));
  }

  PromoService* service() { return service_.get(); }
  PrefService* prefs() { return profile_.GetPrefs(); }
  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }

 protected:
  // Required to run tests from UI and threads.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};

 private:
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  std::unique_ptr<PromoService> service_;

  TestingProfile profile_;
};

TEST_F(PromoServiceTest, PromoDataNetworkError) {
  SetUpResponseWithNetworkError(service()->GetLoadURLForTesting());

  ASSERT_EQ(service()->promo_data(), std::nullopt);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::NOT_UPDATED);

  service()->Refresh();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(service()->promo_data(), std::nullopt);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::TRANSIENT_ERROR);
}

TEST_F(PromoServiceTest, BadPromoResponse) {
  SetUpResponseWithData(service()->GetLoadURLForTesting(),
                        "{\"update\":{\"promotions\":{}}}");

  ASSERT_EQ(service()->promo_data(), std::nullopt);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::NOT_UPDATED);

  service()->Refresh();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(service()->promo_data(), std::nullopt);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::FATAL_ERROR);
}

TEST_F(PromoServiceTest, PromoResponseMissingData) {
  SetUpResponseWithData(service()->GetLoadURLForTesting(),
                        "{\"update\":{\"promos\":{}}}");

  ASSERT_EQ(service()->promo_data(), std::nullopt);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::NOT_UPDATED);

  service()->Refresh();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(service()->promo_data(), PromoData());
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_WITHOUT_PROMO);
}

TEST_F(PromoServiceTest, GoodPromoResponse) {
  std::string response_string =
      "{\"update\":{\"promos\":{\"middle_announce_payload\":"
      "{\"part\":[{\"text\":{\"text\":\"Foo\"}}]},"
      "\"log_url\":\"/log_url?id=42\",\"id\":\"42\"}}}";
  SetUpResponseWithData(service()->GetLoadURLForTesting(), response_string);

  ASSERT_EQ(service()->promo_data(), std::nullopt);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::NOT_UPDATED);

  service()->Refresh();
  task_environment_.RunUntilIdle();

  PromoData promo;
  promo.middle_slot_json = "{\"part\":[{\"text\":{\"text\":\"Foo\"}}]}";
  promo.promo_log_url = GURL("https://www.google.com/log_url?id=42");
  promo.promo_id = "42";

  EXPECT_EQ(service()->promo_data(), promo);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_WITH_PROMO);
}

TEST_F(PromoServiceTest, GoodPromoResponseCanDismiss) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ntp_features::kNtpMiddleSlotPromoDismissal);

  std::string response_string =
      "{\"update\":{\"promos\":{\"middle_announce_payload\":"
      "{\"part\":[{\"text\":{\"text\":\"Foo\"}}]},"
      "\"log_url\":\"/log_url?id=42\",\"id\":\"42\"}}}";
  SetUpResponseWithData(service()->GetLoadURLForTesting(), response_string);

  ASSERT_EQ(service()->promo_data(), std::nullopt);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::NOT_UPDATED);

  service()->Refresh();
  task_environment_.RunUntilIdle();

  PromoData promo;
  promo.middle_slot_json = "{\"part\":[{\"text\":{\"text\":\"Foo\"}}]}";
  promo.promo_log_url = GURL("https://www.google.com/log_url?id=42");
  promo.promo_id = "42";

  EXPECT_EQ(service()->promo_data(), promo);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_WITH_PROMO);
}

TEST_F(PromoServiceTest, GoodPromoResponseNoIdField) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ntp_features::kNtpMiddleSlotPromoDismissal);

  std::string response_string =
      "{\"update\":{\"promos\":{\"middle_announce_payload\":"
      "{\"part\":[{\"text\":{\"text\":\"Foo\"}}]},"
      "\"log_url\":\"/log_url?id=42\"}}}";
  SetUpResponseWithData(service()->GetLoadURLForTesting(), response_string);

  ASSERT_EQ(service()->promo_data(), std::nullopt);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::NOT_UPDATED);

  service()->Refresh();
  task_environment_.RunUntilIdle();

  PromoData promo;
  promo.middle_slot_json = "{\"part\":[{\"text\":{\"text\":\"Foo\"}}]}";
  promo.promo_log_url = GURL("https://www.google.com/log_url?id=42");
  promo.promo_id = "42";

  EXPECT_EQ(service()->promo_data(), promo);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_WITH_PROMO);
}

TEST_F(PromoServiceTest, GoodPromoResponseNoIdFieldNorLogUrl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ntp_features::kNtpMiddleSlotPromoDismissal);

  std::string response_string =
      "{\"update\":{\"promos\":{\"middle_announce_payload\":"
      "{\"part\":[{\"text\":{\"text\":\"Foo\"}}]}}}}";
  SetUpResponseWithData(service()->GetLoadURLForTesting(), response_string);

  ASSERT_EQ(service()->promo_data(), std::nullopt);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::NOT_UPDATED);

  service()->Refresh();
  task_environment_.RunUntilIdle();

  PromoData promo;
  promo.middle_slot_json = "{\"part\":[{\"text\":{\"text\":\"Foo\"}}]}";

  EXPECT_EQ(service()->promo_data(), promo);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_WITH_PROMO);
}

TEST_F(PromoServiceTest, GoodPromoWithBlockedID) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ntp_features::kNtpMiddleSlotPromoDismissal);

  {
    ScopedDictPrefUpdate update(prefs(), prefs::kNtpPromoBlocklist);
    base::Time recent = base::Time::Now() - base::Hours(2);
    update->Set("42", recent.ToDeltaSinceWindowsEpoch().InSecondsF());
  }

  std::string response_string =
      "{\"update\":{\"promos\":{\"middle_announce_payload\":"
      "{\"part\":[{\"text\":{\"text\":\"Foo\"}}]},"
      "\"log_url\":\"/log_url?id=42\",\"id\":\"42\"}}}";
  SetUpResponseWithData(service()->GetLoadURLForTesting(), response_string);

  ASSERT_EQ(service()->promo_data(), std::nullopt);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::NOT_UPDATED);

  service()->Refresh();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(service()->promo_data(), PromoData());
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_BUT_BLOCKED);
}

TEST_F(PromoServiceTest, BlocklistPromo) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ntp_features::kNtpMiddleSlotPromoDismissal);

  std::string response_string =
      "{\"update\":{\"promos\":{\"middle_announce_payload\":"
      "{\"part\":[{\"text\":{\"text\":\"Foo\"}}]},"
      "\"log_url\":\"/log_url?id=42\",\"id\":\"42\"}}}";
  SetUpResponseWithData(service()->GetLoadURLForTesting(), response_string);

  ASSERT_EQ(service()->promo_data(), std::nullopt);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::NOT_UPDATED);

  service()->Refresh();
  task_environment_.RunUntilIdle();

  PromoData promo;
  promo.middle_slot_json = "{\"part\":[{\"text\":{\"text\":\"Foo\"}}]}";
  promo.promo_log_url = GURL("https://www.google.com/log_url?id=42");
  promo.promo_id = "42";

  EXPECT_EQ(service()->promo_data(), promo);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_WITH_PROMO);

  ASSERT_EQ(0u, prefs()->GetDict(prefs::kNtpPromoBlocklist).size());

  service()->BlocklistPromo("42");

  EXPECT_EQ(service()->promo_data(), PromoData());
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_BUT_BLOCKED);

  const auto& blocklist = prefs()->GetDict(prefs::kNtpPromoBlocklist);
  ASSERT_EQ(1u, blocklist.size());
  ASSERT_TRUE(blocklist.Find("42"));
}

TEST_F(PromoServiceTest, BlocklistExpiration) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ntp_features::kNtpMiddleSlotPromoDismissal);

  {
    ScopedDictPrefUpdate update(prefs(), prefs::kNtpPromoBlocklist);
    ASSERT_EQ(0u, update->size());
    base::Time past = base::Time::Now() - base::Days(365);
    update->Set("42", past.ToDeltaSinceWindowsEpoch().InSecondsF());
  }

  ASSERT_EQ(1u, prefs()->GetDict(prefs::kNtpPromoBlocklist).size());

  std::string response_string =
      "{\"update\":{\"promos\":{\"middle_announce_payload\":"
      "{\"part\":[{\"text\":{\"text\":\"Foo\"}}]},"
      "\"log_url\":\"/log_url?id=42\",\"id\":\"42\"}}}";
  SetUpResponseWithData(service()->GetLoadURLForTesting(), response_string);

  service()->Refresh();
  task_environment_.RunUntilIdle();

  // The year-old entry of {promo_id: "42", time: <1y ago>} should be gone.
  ASSERT_EQ(0u, prefs()->GetDict(prefs::kNtpPromoBlocklist).size());

  // The promo should've still been shown, as expiration should take precedence.
  PromoData promo;
  promo.middle_slot_json = "{\"part\":[{\"text\":{\"text\":\"Foo\"}}]}";
  promo.promo_log_url = GURL("https://www.google.com/log_url?id=42");
  promo.promo_id = "42";

  EXPECT_EQ(service()->promo_data(), promo);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_WITH_PROMO);
}

TEST_F(PromoServiceTest, BlocklistWrongExpiryType) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ntp_features::kNtpMiddleSlotPromoDismissal);

  {
    ScopedDictPrefUpdate update(prefs(), prefs::kNtpPromoBlocklist);
    ASSERT_EQ(0u, update->size());
    update->Set("42", 5);
    update->Set("84", "wrong type");
  }

  ASSERT_GT(prefs()->GetDict(prefs::kNtpPromoBlocklist).size(), 0u);

  std::string response_string =
      "{\"update\":{\"promos\":{\"middle_announce_payload\":"
      "{\"part\":[{\"text\":{\"text\":\"Foo\"}}]},"
      "\"log_url\":\"/log_url?id=42\",\"id\":\"42\"}}}";
  SetUpResponseWithData(service()->GetLoadURLForTesting(), response_string);

  service()->Refresh();
  task_environment_.RunUntilIdle();

  // All the invalid formats should've been removed from the pref.
  ASSERT_EQ(0u, prefs()->GetDict(prefs::kNtpPromoBlocklist).size());
}

TEST_F(PromoServiceTest, UndoBlocklistPromo) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ntp_features::kNtpMiddleSlotPromoDismissal);

  std::string response_string =
      "{\"update\":{\"promos\":{\"middle_announce_payload\":"
      "{\"part\":[{\"text\":{\"text\":\"Foo\"}}]},"
      "\"log_url\":\"/log_url?id=42\",\"id\":\"42\"}}}";
  SetUpResponseWithData(service()->GetLoadURLForTesting(), response_string);

  ASSERT_EQ(service()->promo_data(), std::nullopt);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::NOT_UPDATED);

  service()->Refresh();
  task_environment_.RunUntilIdle();

  PromoData promo;
  promo.middle_slot_json = "{\"part\":[{\"text\":{\"text\":\"Foo\"}}]}";
  promo.promo_log_url = GURL("https://www.google.com/log_url?id=42");
  promo.promo_id = "42";

  ASSERT_TRUE(prefs()->GetDict(prefs::kNtpPromoBlocklist).empty());

  service()->BlocklistPromo("42");

  const auto& blocklist = prefs()->GetDict(prefs::kNtpPromoBlocklist);
  ASSERT_EQ(1u, blocklist.size());
  ASSERT_TRUE(blocklist.contains("42"));

  service()->UndoBlocklistPromo("42");

  ASSERT_TRUE(prefs()->GetDict(prefs::kNtpPromoBlocklist).empty());
}

TEST_F(PromoServiceTest, ReturnFakeData) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpMiddleSlotPromoDismissal,
      {{ntp_features::kNtpMiddleSlotPromoDismissalParam, "fake"}});

  service()->Refresh();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(0, test_url_loader_factory().NumPending());
  ASSERT_TRUE(service()->promo_data().has_value());
  EXPECT_EQ("test" + base::NumberToString(static_cast<int>(
                         browser_command::mojom::Command::kNoOpCommand)),
            service()->promo_data()->promo_id);
}

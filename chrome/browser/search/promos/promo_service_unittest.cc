// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/promos/promo_service.h"

#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/search/promos/promo_data.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
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

  void SetUpExtensionTest() {
    // Creates an extension system and adds one non policy-install extension.
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    extensions::TestExtensionSystem* test_ext_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(&profile_));
    extensions::ExtensionService* service =
        test_ext_system->CreateExtensionService(&command_line, base::FilePath(),
                                                false);
    EXPECT_TRUE(service->extensions_enabled());
    service->Init();
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder("foo").Build();
    service->AddExtension(extension.get());
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

 private:
  // Required to run tests from UI and threads.
  content::BrowserTaskEnvironment task_environment_;

  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  std::unique_ptr<PromoService> service_;

  TestingProfile profile_;
};

TEST_F(PromoServiceTest, PromoDataNetworkError) {
  SetUpResponseWithNetworkError(service()->GetLoadURLForTesting());

  ASSERT_EQ(service()->promo_data(), base::nullopt);

  service()->Refresh();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(service()->promo_data(), base::nullopt);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::TRANSIENT_ERROR);
}

TEST_F(PromoServiceTest, BadPromoResponse) {
  SetUpResponseWithData(service()->GetLoadURLForTesting(),
                        "{\"update\":{\"promotions\":{}}}");

  ASSERT_EQ(service()->promo_data(), base::nullopt);

  service()->Refresh();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(service()->promo_data(), base::nullopt);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::FATAL_ERROR);
}

TEST_F(PromoServiceTest, PromoResponseMissingData) {
  SetUpResponseWithData(service()->GetLoadURLForTesting(),
                        "{\"update\":{\"promos\":{}}}");

  ASSERT_EQ(service()->promo_data(), base::nullopt);

  service()->Refresh();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(service()->promo_data(), PromoData());
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_WITHOUT_PROMO);
}

TEST_F(PromoServiceTest, GoodPromoResponse) {
  std::string response_string =
      "{\"update\":{\"promos\":{\"middle\":\"<style></style><div><script></"
      "script></div>\", \"log_url\":\"/log_url?id=42\", \"id\": \"42\"}}}";
  SetUpResponseWithData(service()->GetLoadURLForTesting(), response_string);

  ASSERT_EQ(service()->promo_data(), base::nullopt);

  service()->Refresh();
  base::RunLoop().RunUntilIdle();

  PromoData promo;
  promo.promo_html = "<style></style><div><script></script></div>";
  promo.promo_log_url = GURL("https://www.google.com/log_url?id=42");

  EXPECT_EQ(service()->promo_data(), promo);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_WITH_PROMO);
}

TEST_F(PromoServiceTest, GoodPromoResponseCanDismiss) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ntp_features::kDismissPromos);

  std::string response_string =
      "{\"update\":{\"promos\":{\"middle\":\"<style></style><div><script></"
      "script></div>\", \"log_url\":\"/log_url?id=42\", \"id\": \"42\"}}}";
  SetUpResponseWithData(service()->GetLoadURLForTesting(), response_string);

  ASSERT_EQ(service()->promo_data(), base::nullopt);

  service()->Refresh();
  base::RunLoop().RunUntilIdle();

  PromoData promo;
  promo.promo_html = "<style></style><div><script></script></div>";
  promo.promo_log_url = GURL("https://www.google.com/log_url?id=42");
  promo.promo_id = "42";

  EXPECT_EQ(service()->promo_data(), promo);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_WITH_PROMO);
}

TEST_F(PromoServiceTest, GoodPromoResponseNoIdField) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ntp_features::kDismissPromos);

  std::string response_string =
      "{\"update\":{\"promos\":{\"middle\":\"<style></style><div><script></"
      "script></div>\", \"log_url\":\"/log_url?id=42\"}}}";
  SetUpResponseWithData(service()->GetLoadURLForTesting(), response_string);

  ASSERT_EQ(service()->promo_data(), base::nullopt);

  service()->Refresh();
  base::RunLoop().RunUntilIdle();

  PromoData promo;
  promo.promo_html = "<style></style><div><script></script></div>";
  promo.promo_log_url = GURL("https://www.google.com/log_url?id=42");
  promo.promo_id = "42";

  EXPECT_EQ(service()->promo_data(), promo);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_WITH_PROMO);
}

TEST_F(PromoServiceTest, GoodPromoResponseNoIdFieldNorLogUrl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ntp_features::kDismissPromos);

  std::string response_string =
      "{\"update\":{\"promos\":{\"middle\":\"<style></style><div><script></"
      "script></div>\"}}}";
  SetUpResponseWithData(service()->GetLoadURLForTesting(), response_string);

  ASSERT_EQ(service()->promo_data(), base::nullopt);

  service()->Refresh();
  base::RunLoop().RunUntilIdle();

  PromoData promo;
  promo.promo_html = "<style></style><div><script></script></div>";

  EXPECT_EQ(service()->promo_data(), promo);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_WITH_PROMO);
}

TEST_F(PromoServiceTest, GoodPromoWithBlockedID) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ntp_features::kDismissPromos);

  {
    DictionaryPrefUpdate update(prefs(), prefs::kNtpPromoBlocklist);
    base::Time recent = base::Time::Now() - base::TimeDelta::FromHours(2);
    update->SetDoubleKey("42", recent.ToDeltaSinceWindowsEpoch().InSecondsF());
  }

  std::string response_string =
      "{\"update\":{\"promos\":{\"middle\":\"<style></style><div><script></"
      "script></div>\", \"log_url\":\"/log_url?id=42\", \"id\": \"42\"}}}";
  SetUpResponseWithData(service()->GetLoadURLForTesting(), response_string);

  ASSERT_EQ(service()->promo_data(), base::nullopt);

  service()->Refresh();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(service()->promo_data(), PromoData());
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_BUT_BLOCKED);
}

TEST_F(PromoServiceTest, BlocklistPromo) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ntp_features::kDismissPromos);

  std::string response_string =
      "{\"update\":{\"promos\":{\"middle\":\"<style></style><div><script></"
      "script></div>\", \"log_url\":\"/log_url?id=42\", \"id\": \"42\"}}}";
  SetUpResponseWithData(service()->GetLoadURLForTesting(), response_string);

  ASSERT_EQ(service()->promo_data(), base::nullopt);

  service()->Refresh();
  base::RunLoop().RunUntilIdle();

  PromoData promo;
  promo.promo_html = "<style></style><div><script></script></div>";
  promo.promo_log_url = GURL("https://www.google.com/log_url?id=42");
  promo.promo_id = "42";

  EXPECT_EQ(service()->promo_data(), promo);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_WITH_PROMO);

  ASSERT_EQ(0u, prefs()->GetDictionary(prefs::kNtpPromoBlocklist)->size());

  service()->BlocklistPromo("42");

  EXPECT_EQ(service()->promo_data(), PromoData());
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_BUT_BLOCKED);

  const auto* blocklist = prefs()->GetDictionary(prefs::kNtpPromoBlocklist);
  ASSERT_EQ(1u, blocklist->size());
  ASSERT_TRUE(blocklist->HasKey("42"));
}

TEST_F(PromoServiceTest, BlocklistExpiration) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ntp_features::kDismissPromos);

  {
    DictionaryPrefUpdate update(prefs(), prefs::kNtpPromoBlocklist);
    ASSERT_EQ(0u, update->size());
    base::Time past = base::Time::Now() - base::TimeDelta::FromDays(365);
    update->SetDoubleKey("42", past.ToDeltaSinceWindowsEpoch().InSecondsF());
  }

  ASSERT_EQ(1u, prefs()->GetDictionary(prefs::kNtpPromoBlocklist)->size());

  std::string response_string =
      "{\"update\":{\"promos\":{\"middle\":\"<style></style><div><script></"
      "script></div>\", \"log_url\":\"/log_url?id=42\", \"id\": \"42\"}}}";
  SetUpResponseWithData(service()->GetLoadURLForTesting(), response_string);

  service()->Refresh();
  base::RunLoop().RunUntilIdle();

  // The year-old entry of {promo_id: "42", time: <1y ago>} should be gone.
  ASSERT_EQ(0u, prefs()->GetDictionary(prefs::kNtpPromoBlocklist)->size());

  // The promo should've still been shown, as expiration should take precedence.
  PromoData promo;
  promo.promo_html = "<style></style><div><script></script></div>";
  promo.promo_log_url = GURL("https://www.google.com/log_url?id=42");
  promo.promo_id = "42";

  EXPECT_EQ(service()->promo_data(), promo);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_WITH_PROMO);
}

TEST_F(PromoServiceTest, BlocklistWrongExpiryType) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ntp_features::kDismissPromos);

  {
    DictionaryPrefUpdate update(prefs(), prefs::kNtpPromoBlocklist);
    ASSERT_EQ(0u, update->size());
    update->SetDoubleKey("42", 5);
    update->SetStringKey("84", "wrong type");
  }

  ASSERT_GT(prefs()->GetDictionary(prefs::kNtpPromoBlocklist)->size(), 0u);

  std::string response_string =
      "{\"update\":{\"promos\":{\"middle\":\"<style></style><div><script></"
      "script></div>\", \"log_url\":\"/log_url?id=42\", \"id\": \"42\"}}}";
  SetUpResponseWithData(service()->GetLoadURLForTesting(), response_string);

  service()->Refresh();
  base::RunLoop().RunUntilIdle();

  // All the invalid formats should've been removed from the pref.
  ASSERT_EQ(0u, prefs()->GetDictionary(prefs::kNtpPromoBlocklist)->size());
}

TEST_F(PromoServiceTest, ServeExtensionsPromo) {
  SetUpExtensionTest();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      extensions_features::kExtensionsCheckup,
      {{extensions_features::kExtensionsCheckupEntryPointParameter,
        extensions_features::kNtpPromoEntryPoint},
       {extensions_features::kExtensionsCheckupBannerMessageParameter,
        extensions_features::kPerformanceMessage}});

  service()->Refresh();
  base::RunLoop().RunUntilIdle();

  PromoData promo;
  promo.promo_html =
      "<div>" + l10n_util::GetStringUTF8(IDS_EXTENSIONS_PROMO_PERFORMANCE) +
      "</div>";
  promo.can_open_extensions_page = true;

  EXPECT_EQ(service()->promo_data(), promo);
  EXPECT_EQ(service()->promo_status(), PromoService::Status::OK_WITH_PROMO);
}

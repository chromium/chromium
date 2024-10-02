// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/companion_url_builder.h"

#include "base/base64url.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/companion/core/mock_signin_delegate.h"
#include "chrome/browser/companion/core/promo_handler.h"
#include "chrome/browser/companion/core/proto/companion_url_params.pb.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ContainsRegex;
using ::testing::MatchesRegex;

namespace companion {
namespace {

constexpr char kValidUrl[] = "https://foo.com/";
constexpr char kTextQuery[] = "Apples";
constexpr char kOrigin[] = "chrome-untrusted://companion-side-panel.top-chrome";

}  // namespace

class CompanionUrlBuilderTest : public testing::Test {
 public:
  ~CompanionUrlBuilderTest() override = default;

  void SetUp() override {
    scoped_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                               /** disabled_features= */ {});

    pref_service_.registry()->RegisterBooleanPref(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        false);

    pref_service_.registry()->RegisterBooleanPref(
        prefs::kSidePanelCompanionEntryPinnedToToolbar,
        EntryPointDefaultPinned());

    pref_service_.registry()->RegisterBooleanPref(
        unified_consent::prefs::kPageContentCollectionEnabled, false);

    PromoHandler::RegisterProfilePrefs(pref_service_.registry());

    pref_service_.SetUserPref(kSigninPromoDeclinedCountPref, base::Value(1));
    SetSignInAndMsbbExpectations(/*is_sign_in_allowed=*/true,
                                 /*is_signed_in=*/true,
                                 /*msbb_pref_enabled=*/true);
    EXPECT_CALL(signin_delegate_, ShouldShowRegionSearchIPH())
        .WillRepeatedly(testing::Return(true));
    url_builder_ = std::make_unique<CompanionUrlBuilder>(&pref_service_,
                                                         &signin_delegate_);
  }

  virtual std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {
        {features::internal::kSidePanelCompanion,
         {{"open-links-in-current-tab", "false"}}},
        {features::kCompanionEnablePageContent, {}},
    };
  }

  virtual bool EntryPointDefaultPinned() { return true; }

 protected:
  void VerifyPageUrlSent(GURL page_url, bool expect_was_sent) {
    GURL companion_url = url_builder_->BuildCompanionURL(page_url);

    std::string companion_query_param;
    EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "companion_query",
                                           &companion_query_param));

    // Deserialize the query param into protobuf.
    companion::proto::CompanionUrlParams proto =
        DeserializeCompanionRequest(companion_query_param);

    if (expect_was_sent) {
      EXPECT_EQ(proto.page_url(), page_url.spec());
    } else {
      EXPECT_EQ(proto.page_url(), std::string());
    }

    EXPECT_TRUE(proto.is_page_url_sharing_enabled());
    EXPECT_FALSE(proto.is_page_content_sharing_enabled());
  }
  // Deserialize the query param into proto::CompanionUrlParams.
  proto::CompanionUrlParams DeserializeCompanionRequest(
      const std::string& companion_url_param) {
    std::string serialized_proto;
    EXPECT_TRUE(base::Base64UrlDecode(
        companion_url_param, base::Base64UrlDecodePolicy::DISALLOW_PADDING,
        &serialized_proto));

    companion::proto::CompanionUrlParams proto;
    EXPECT_TRUE(proto.ParseFromString(serialized_proto));
    return proto;
  }

  void SetSignInAndMsbbExpectations(bool is_sign_in_allowed,
                                    bool is_signed_in,
                                    bool msbb_pref_enabled) {
    EXPECT_CALL(signin_delegate_, AllowedSignin())
        .WillRepeatedly(testing::Return(is_sign_in_allowed));
    EXPECT_CALL(signin_delegate_, IsSignedIn())
        .WillRepeatedly(testing::Return(is_signed_in));
    pref_service_.SetUserPref(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        base::Value(msbb_pref_enabled));
  }

  base::test::ScopedFeatureList scoped_list_;
  TestingPrefServiceSimple pref_service_;
  MockSigninDelegate signin_delegate_;
  std::unique_ptr<CompanionUrlBuilder> url_builder_;
};

TEST_F(CompanionUrlBuilderTest, SignIn) {
  GURL page_url(kValidUrl);

  // Not signed in, no msbb.
  SetSignInAndMsbbExpectations(/*is_sign_in_allowed=*/false,
                               /*is_signed_in=*/false,
                               /*msbb_pref_enabled=*/false);

  std::string encoded_proto =
      url_builder_->BuildCompanionUrlParamProto(page_url);
  companion::proto::CompanionUrlParams proto =
      DeserializeCompanionRequest(encoded_proto);

  EXPECT_EQ(proto.page_url(), std::string());
  EXPECT_FALSE(proto.is_sign_in_allowed());
  EXPECT_FALSE(proto.is_signed_in());
  EXPECT_FALSE(proto.is_page_url_sharing_enabled());
  EXPECT_FALSE(proto.is_page_content_sharing_enabled());
  EXPECT_TRUE(proto.is_page_content_enabled());

  // Allowed to sign-in, but not signed in, no msbb.
  SetSignInAndMsbbExpectations(/*is_sign_in_allowed=*/true,
                               /*is_signed_in=*/false,
                               /*msbb_pref_enabled=*/false);
  encoded_proto = url_builder_->BuildCompanionUrlParamProto(page_url);
  proto = DeserializeCompanionRequest(encoded_proto);

  EXPECT_EQ(proto.page_url(), std::string());
  EXPECT_TRUE(proto.is_sign_in_allowed());
  EXPECT_FALSE(proto.is_signed_in());
  EXPECT_FALSE(proto.is_page_url_sharing_enabled());
  EXPECT_FALSE(proto.is_page_content_sharing_enabled());
  EXPECT_TRUE(proto.is_page_content_enabled());
}

TEST_F(CompanionUrlBuilderTest, MsbbAndPcOff) {
  pref_service_.SetUserPref(kSigninPromoDeclinedCountPref, base::Value(1));
  SetSignInAndMsbbExpectations(/*is_sign_in_allowed=*/true,
                               /*is_signed_in=*/true,
                               /*msbb_pref_enabled=*/false);
  pref_service_.SetUserPref(kSigninPromoDeclinedCountPref, base::Value(1));

  GURL page_url(kValidUrl);
  GURL companion_url = url_builder_->BuildCompanionURL(page_url);

  std::string value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(companion_url, "url", &value));

  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "origin", &value));
  EXPECT_EQ(value, kOrigin);

  std::string companion_url_param;
  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "companion_query",
                                         &companion_url_param));

  // Verify that both helper methods generate the same proto.
  std::string encoded_proto =
      url_builder_->BuildCompanionUrlParamProto(page_url);
  EXPECT_EQ(encoded_proto, companion_url_param);

  // Deserialize the query param into protobuf.
  companion::proto::CompanionUrlParams proto =
      DeserializeCompanionRequest(companion_url_param);

  // URL shouldn't be sent when MSBB is off.
  EXPECT_EQ(proto.page_url(), std::string());
  EXPECT_TRUE(proto.is_signed_in());
  EXPECT_TRUE(proto.is_sign_in_allowed());
  EXPECT_FALSE(proto.is_page_url_sharing_enabled());
  EXPECT_FALSE(proto.is_page_content_sharing_enabled());
  EXPECT_TRUE(proto.is_page_content_enabled());
  EXPECT_TRUE(proto.is_upload_dialog_supported());
  EXPECT_TRUE(proto.is_hard_refresh_supported());
}

TEST_F(CompanionUrlBuilderTest, MsbbAndPcOn) {
  EXPECT_CALL(signin_delegate_, IsSignedIn())
      .WillRepeatedly(testing::Return(true));
  pref_service_.SetUserPref(kExpsPromoShownCountPref, base::Value(3));
  pref_service_.SetUserPref(kPcoPromoShownCountPref, base::Value(2));
  pref_service_.SetUserPref(kPcoPromoDeclinedCountPref, base::Value(1));
  pref_service_.SetUserPref(
      unified_consent::prefs::kPageContentCollectionEnabled, base::Value(true));

  GURL page_url(kValidUrl);
  GURL companion_url = url_builder_->BuildCompanionURL(page_url);

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "url", &value));
  EXPECT_EQ(value, page_url.spec());

  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "origin", &value));
  EXPECT_EQ(value, kOrigin);

  std::string companion_url_param;
  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "companion_query",
                                         &companion_url_param));

  // Verify that both helper methods generate the same proto.
  std::string encoded_proto =
      url_builder_->BuildCompanionUrlParamProto(page_url);
  EXPECT_EQ(encoded_proto, companion_url_param);

  // Deserialize the query param into protobuf.
  companion::proto::CompanionUrlParams proto =
      DeserializeCompanionRequest(companion_url_param);

  // Verify fields inside protobuf.
  EXPECT_EQ(proto.page_url(), page_url.spec());
  EXPECT_TRUE(proto.is_page_url_sharing_enabled());
  EXPECT_TRUE(proto.is_page_content_sharing_enabled());
  EXPECT_TRUE(proto.is_page_content_enabled());
  EXPECT_TRUE(proto.is_signed_in());
  EXPECT_TRUE(proto.is_entrypoint_pinned_by_default());
  EXPECT_TRUE(proto.links_open_in_new_tab());
  EXPECT_FALSE(proto.is_vqs_enabled_on_chrome());
  EXPECT_TRUE(proto.is_upload_dialog_supported());
  EXPECT_TRUE(proto.is_hard_refresh_supported());

  // Verify promo state.
  EXPECT_TRUE(proto.has_promo_state());
  EXPECT_EQ(1, proto.promo_state().signin_promo_denial_count());
  EXPECT_EQ(0, proto.promo_state().msbb_promo_denial_count());
  EXPECT_EQ(0, proto.promo_state().exps_promo_denial_count());
  EXPECT_EQ(3, proto.promo_state().exps_promo_shown_count());
  EXPECT_EQ(2, proto.promo_state().pco_promo_shown_count());
  EXPECT_EQ(1, proto.promo_state().pco_promo_denial_count());
  EXPECT_TRUE(proto.promo_state().should_show_region_search_iph());
}

TEST_F(CompanionUrlBuilderTest, MsbbAndPcCombinations) {
  EXPECT_CALL(signin_delegate_, IsSignedIn())
      .WillRepeatedly(testing::Return(true));

  auto test = [&](bool msbb_pref_enabled, bool pc_pref_enabled,
                  bool pc_feature_enabled, bool expect_url_param) {
    SCOPED_TRACE("MSBB pref: " + base::NumberToString(msbb_pref_enabled) +
                 ", PC pref: " + base::NumberToString(pc_pref_enabled) +
                 ", PC feature: " + base::NumberToString(pc_feature_enabled));

    pref_service_.SetUserPref(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        base::Value(msbb_pref_enabled));
    pref_service_.SetUserPref(
        unified_consent::prefs::kPageContentCollectionEnabled,
        base::Value(pc_pref_enabled));

    base::test::ScopedFeatureList scoped_list;
    scoped_list.InitWithFeatureState(features::kCompanionEnablePageContent,
                                     pc_feature_enabled);

    GURL companion_url = url_builder_->BuildCompanionURL(GURL{kValidUrl});

    std::string value;
    EXPECT_EQ(net::GetValueForKeyInQuery(companion_url, "url", &value),
              expect_url_param);

    std::string companion_url_param;
    EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "companion_query",
                                           &companion_url_param));
    companion::proto::CompanionUrlParams proto =
        DeserializeCompanionRequest(companion_url_param);
    EXPECT_EQ(proto.is_page_url_sharing_enabled(), expect_url_param);
    EXPECT_EQ(proto.is_page_content_sharing_enabled(), pc_pref_enabled);
    EXPECT_EQ(proto.is_page_content_enabled(), pc_feature_enabled);
  };

  // When neither MSBB nor PC is enabled, page URL should not be included.
  test(false, false, false, false);
  // Even if the PC feature is enabled, if the pref hasn't also been enabled,
  // page URL should not be included.
  test(false, false, true, false);
  // When either MSBB or PC is enabled, page URL should be included.
  test(true, false, true, true);
  test(false, true, true, true);
  test(true, true, true, true);
}

TEST_F(CompanionUrlBuilderTest, NonProtobufParams) {
  GURL page_url(kValidUrl);
  GURL companion_url = url_builder_->BuildCompanionURL(page_url);

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "url", &value));
  EXPECT_EQ(value, page_url.spec());

  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "origin", &value));
  EXPECT_EQ(value, kOrigin);
}

TEST_F(CompanionUrlBuilderTest, ValidPageUrls) {
  VerifyPageUrlSent(GURL(kValidUrl), true);
  VerifyPageUrlSent(GURL("chrome://new-tab"), false);
  VerifyPageUrlSent(GURL("https://192.168.0.1"), false);
  VerifyPageUrlSent(GURL("https://localhost:8888"), false);
}

TEST_F(CompanionUrlBuilderTest, WithTextQuery) {
  GURL page_url(kValidUrl);
  GURL companion_url = url_builder_->BuildCompanionURL(page_url, kTextQuery);

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "url", &value));
  EXPECT_EQ(value, page_url.spec());

  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "q", &value));
  EXPECT_EQ(value, kTextQuery);

  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "origin", &value));
  EXPECT_EQ(value, kOrigin);
}

TEST_F(CompanionUrlBuilderTest, WithoutTextQuery) {
  GURL page_url(kValidUrl);
  GURL companion_url = url_builder_->BuildCompanionURL(page_url);

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "url", &value));
  EXPECT_EQ(value, page_url.spec());

  EXPECT_FALSE(net::GetValueForKeyInQuery(companion_url, "q", &value));

  EXPECT_TRUE(net::GetValueForKeyInQuery(companion_url, "origin", &value));
  EXPECT_EQ(value, kOrigin);
}

TEST_F(CompanionUrlBuilderTest, WithQueryStartTime) {
  auto time = base::Time::Now();
  auto timestamp = std::make_unique<base::Time>(time);
  int64_t nanoseconds_in_milliseconds = 1e6;
  int64_t time_nanoseconds =
      time.InMillisecondsSinceUnixEpoch() * nanoseconds_in_milliseconds;
  GURL page_url(kValidUrl);
  std::string encoded_proto =
      url_builder_->BuildCompanionUrlParamProto(page_url, std::move(timestamp));

  // Deserialize the query param into protobuf.
  companion::proto::CompanionUrlParams proto =
      DeserializeCompanionRequest(encoded_proto);

  companion::proto::Timestamp* query_start_time =
      proto.mutable_query_start_time();
  EXPECT_TRUE(query_start_time);
  EXPECT_EQ(query_start_time->seconds(),
            time_nanoseconds / base::Time::kNanosecondsPerSecond);
  EXPECT_EQ(query_start_time->nanos(),
            time_nanoseconds % base::Time::kNanosecondsPerSecond);
}

class CompanionUrlBuilderCurrentTabTest : public CompanionUrlBuilderTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {base::test::FeatureRefAndParams(
        features::internal::kSidePanelCompanion,
        {{"open-links-in-current-tab", "true"}})};
  }
};

TEST_F(CompanionUrlBuilderCurrentTabTest, CurrentTab) {
  GURL page_url(kValidUrl);
  std::string encoded_proto =
      url_builder_->BuildCompanionUrlParamProto(page_url);

  // Deserialize the query param into protobuf.
  companion::proto::CompanionUrlParams proto =
      DeserializeCompanionRequest(encoded_proto);

  EXPECT_FALSE(proto.links_open_in_new_tab());
}

class CompanionUrlBuilderDefaultUnpinnedTest : public CompanionUrlBuilderTest {
 public:
  bool EntryPointDefaultPinned() override { return false; }
};

TEST_F(CompanionUrlBuilderDefaultUnpinnedTest, DefaultUnpinned) {
  GURL page_url(kValidUrl);
  std::string encoded_proto =
      url_builder_->BuildCompanionUrlParamProto(page_url);

  // Deserialize the query param into protobuf.
  companion::proto::CompanionUrlParams proto =
      DeserializeCompanionRequest(encoded_proto);

  EXPECT_FALSE(proto.is_entrypoint_pinned_by_default());
}

}  // namespace companion

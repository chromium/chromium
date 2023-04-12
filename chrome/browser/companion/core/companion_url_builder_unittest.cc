// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/companion_url_builder.h"

#include "base/base64.h"
#include "base/logging.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/promo_handler.h"
#include "chrome/browser/companion/core/proto/companion_url_params.pb.h"
#include "chrome/browser/companion/core/signin_delegate.h"
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

class MockSigninDelegate : public SigninDelegate {
 public:
  MOCK_METHOD0(AllowedSignin, bool());
  MOCK_METHOD0(StartSigninFlow, void());
};

}  // namespace

class CompanionUrlBuilderTest : public testing::Test {
 public:
  CompanionUrlBuilderTest() = default;
  ~CompanionUrlBuilderTest() override = default;

  void SetUp() override {
    pref_service_.registry()->RegisterBooleanPref(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        false);

    PromoHandler::RegisterProfilePrefs(pref_service_.registry());

    pref_service_.SetUserPref(kSigninPromoDeclinedCountPref, base::Value(1));
    pref_service_.SetUserPref(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        base::Value(true));
    EXPECT_CALL(signin_delegate_, AllowedSignin())
        .WillRepeatedly(testing::Return(false));
    url_builder_ = std::make_unique<CompanionUrlBuilder>(&pref_service_,
                                                         &signin_delegate_);
  }

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

    EXPECT_TRUE(proto.has_msbb_enabled());
  }
  // Deserialize the query param into proto::CompanionUrlParams.
  proto::CompanionUrlParams DeserializeCompanionRequest(
      std::string companion_url_param) {
    companion::proto::CompanionUrlParams proto;
    auto base64_decoded = base::Base64Decode(companion_url_param);
    auto serialized_proto = std::string(base64_decoded.value().begin(),
                                        base64_decoded.value().end());
    EXPECT_TRUE(proto.ParseFromString(serialized_proto));
    return proto;
  }

  TestingPrefServiceSimple pref_service_;
  MockSigninDelegate signin_delegate_;
  std::unique_ptr<CompanionUrlBuilder> url_builder_;
};

TEST_F(CompanionUrlBuilderTest, MsbbOff) {
  pref_service_.SetUserPref(kSigninPromoDeclinedCountPref, base::Value(1));
  pref_service_.SetUserPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      base::Value(false));
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
  EXPECT_FALSE(proto.signin_allowed_and_required());
  EXPECT_FALSE(proto.has_msbb_enabled());
}

TEST_F(CompanionUrlBuilderTest, MsbbOn) {
  EXPECT_CALL(signin_delegate_, AllowedSignin())
      .WillRepeatedly(testing::Return(true));
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
  EXPECT_TRUE(proto.has_msbb_enabled());
  EXPECT_TRUE(proto.signin_allowed_and_required());

  // Verify promo state.
  EXPECT_TRUE(proto.has_promo_state());
  EXPECT_EQ(1, proto.promo_state().signin_promo_denial_count());
  EXPECT_EQ(0, proto.promo_state().msbb_promo_denial_count());
  EXPECT_EQ(0, proto.promo_state().exps_promo_denial_count());
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

}  // namespace companion

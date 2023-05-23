// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/core/promo_handler.h"

#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/mojom/companion.mojom.h"
#include "chrome/browser/companion/core/signin_delegate.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace companion {
namespace {

class MockSigninDelegate : public SigninDelegate {
 public:
  MOCK_METHOD0(AllowedSignin, bool());
  MOCK_METHOD0(IsSignedIn, bool());
  MOCK_METHOD0(StartSigninFlow, void());
  MOCK_METHOD1(EnableMsbb, void(bool));
  MOCK_METHOD1(LoadUrlInNewTab, void(const GURL&));
};

}  // namespace

class PromoHandlerTest : public testing::Test {
 public:
  PromoHandlerTest() = default;
  ~PromoHandlerTest() override = default;

  void SetUp() override {
    PromoHandler::RegisterProfilePrefs(pref_service_.registry());
    promo_handler_ =
        std::make_unique<PromoHandler>(&pref_service_, &signin_delegate_);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
  MockSigninDelegate signin_delegate_;
  std::unique_ptr<PromoHandler> promo_handler_;
};

TEST_F(PromoHandlerTest, MsbbPromo) {
  promo_handler_->OnPromoAction(PromoType::kMsbb, PromoAction::kRejected,
                                absl::nullopt);
  EXPECT_EQ(1, pref_service_.GetInteger(kMsbbPromoDeclinedCountPref));

  EXPECT_CALL(signin_delegate_, EnableMsbb(true)).Times(1);
  promo_handler_->OnPromoAction(PromoType::kMsbb, PromoAction::kAccepted,
                                absl::nullopt);
}

TEST_F(PromoHandlerTest, SigninPromo) {
  promo_handler_->OnPromoAction(PromoType::kSignin, PromoAction::kRejected,
                                absl::nullopt);
  EXPECT_EQ(1, pref_service_.GetInteger(kSigninPromoDeclinedCountPref));

  EXPECT_CALL(signin_delegate_, StartSigninFlow()).Times(1);
  promo_handler_->OnPromoAction(PromoType::kSignin, PromoAction::kAccepted,
                                absl::nullopt);
}

TEST_F(PromoHandlerTest, ExpsPromo) {
  promo_handler_->OnPromoAction(PromoType::kExps, PromoAction::kShown,
                                absl::nullopt);
  EXPECT_EQ(1, pref_service_.GetInteger(kExpsPromoShownCountPref));

  promo_handler_->OnPromoAction(PromoType::kExps, PromoAction::kRejected,
                                absl::nullopt);
  EXPECT_EQ(1, pref_service_.GetInteger(kExpsPromoDeclinedCountPref));

  EXPECT_CALL(signin_delegate_, LoadUrlInNewTab(testing::_)).Times(0);
  promo_handler_->OnPromoAction(PromoType::kExps, PromoAction::kAccepted,
                                absl::nullopt);

  EXPECT_CALL(signin_delegate_, LoadUrlInNewTab(testing::_)).Times(1);
  promo_handler_->OnPromoAction(PromoType::kExps, PromoAction::kAccepted,
                                GURL());
}

}  // namespace companion

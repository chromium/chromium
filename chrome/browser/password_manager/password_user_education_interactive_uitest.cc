// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/element_identifier.h"
#include "url/gurl.h"

namespace {

constexpr char kPageWithLogin[] = "/login/saml_login.html";

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabElementId);

}  // namespace

class PasswordUserEducationUiTest : public InteractiveFeaturePromoTest {
 public:
  PasswordUserEducationUiTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHPasswordsSavePrimingPromoFeature})) {}
  ~PasswordUserEducationUiTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_https_test_server().Start());
    InteractiveFeaturePromoTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(PasswordUserEducationUiTest, TriggerPromo) {
  GURL base_url = embedded_https_test_server().GetURL("a.com", kPageWithLogin);
  RunTestSequence(
      InstrumentTab(kTabElementId),
      NavigateWebContents(kTabElementId, base_url),
      WaitForPromo(feature_engagement::kIPHPasswordsSavePrimingPromoFeature));
}

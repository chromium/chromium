// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/digital_credentials/digital_identity_low_risk_origins.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/digital_credentials/digital_credentials_keyed_service.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace digital_credentials {

url::Origin CreateOrigin(const std::string& spec) {
  return url::Origin::Create(GURL(spec));
}

class DigitalIdentityLowRiskOriginsTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  DigitalIdentityLowRiskOriginsTest() {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                RegisterMockOptimizationGuideKeyedServiceFactory));
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    mock_optimization_guide_keyed_service_ =
        static_cast<testing::NiceMock<MockOptimizationGuideKeyedService>*>(
            OptimizationGuideKeyedServiceFactory::GetForProfile(profile()));
  }

  void TearDown() override {
    mock_optimization_guide_keyed_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  static void RegisterMockOptimizationGuideKeyedServiceFactory(
      content::BrowserContext* context) {
    OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              testing::NiceMock<MockOptimizationGuideKeyedService>>();
        }));
  }

 protected:
  raw_ptr<testing::NiceMock<MockOptimizationGuideKeyedService>>
      mock_optimization_guide_keyed_service_;
  base::CallbackListSubscription subscription_;
};

// Test IsLowRiskOrigin when the kKnownLowRiskOrigins list is empty (default)
// and the OptimizationGuideKeyedService reports the URL as low friction.
TEST_F(DigitalIdentityLowRiskOriginsTest,
       IsLowRiskOrigin_KnownOriginsEmpty_LowFrictionUrlIsTrue) {
  GURL url("https://example_low_friction.com");
  NavigateAndCommit(url);
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(url,
                                   optimization_guide::proto::OptimizationType::
                                       DIGITAL_CREDENTIALS_LOW_FRICTION,
                                   /*optimization_metadata=*/nullptr))
      .WillOnce(testing::Return(
          optimization_guide::OptimizationGuideDecision::kTrue));

  EXPECT_TRUE(IsLowRiskOrigin(*main_rfh()));
}

// Test IsLowRiskOrigin when the kKnownLowRiskOrigins list is empty (default)
// and the OptimizationGuideKeyedService reports the URL as NOT low friction.
TEST_F(DigitalIdentityLowRiskOriginsTest,
       IsLowRiskOrigin_KnownOriginsEmpty_LowFrictionUrlIsFalse) {
  NavigateAndCommit(GURL("https://example_not_low_friction.com"));
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(GURL("https://example_not_low_friction.com"),
                                   optimization_guide::proto::OptimizationType::
                                       DIGITAL_CREDENTIALS_LOW_FRICTION,
                                   /*optimization_metadata=*/nullptr))
      .WillOnce(testing::Return(
          optimization_guide::OptimizationGuideDecision::kFalse));

  EXPECT_FALSE(IsLowRiskOrigin(*main_rfh()));
}

TEST_F(DigitalIdentityLowRiskOriginsTest,
       IsLowRiskOrigin_OTRProfile_LowFrictionUrlIsTrue) {
  Profile* otr_profile = profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);
  ASSERT_TRUE(otr_profile);
  ASSERT_TRUE(otr_profile->IsOffTheRecord());

  // This mock will be specifically for the OTR profile.
  raw_ptr<testing::NiceMock<MockOptimizationGuideKeyedService>>
      otr_mock_service =
          static_cast<testing::NiceMock<MockOptimizationGuideKeyedService>*>(
              OptimizationGuideKeyedServiceFactory::GetForProfile(otr_profile));

  // Ensure the OptimizationGuideKeyedService for the OTR profile is created,
  ASSERT_TRUE(otr_mock_service);

  // Create a WebContents and RenderFrameHost for the OTR profile.
  std::unique_ptr<content::WebContents> otr_web_contents =
      content::WebContentsTester::CreateTestWebContents(otr_profile, nullptr);

  GURL url("https://otr.example_low_friction.com");
  // Navigate the OTR WebContents to make its RFH have the URL.
  content::WebContentsTester::For(otr_web_contents.get())
      ->NavigateAndCommit(url);
  content::RenderFrameHost* otr_rfh = otr_web_contents->GetPrimaryMainFrame();

  // Set expectation on the OTR profile's mock.
  EXPECT_CALL(*otr_mock_service,
              CanApplyOptimization(url,
                                   optimization_guide::proto::OptimizationType::
                                       DIGITAL_CREDENTIALS_LOW_FRICTION,
                                   /*optimization_metadata=*/nullptr))
      .WillOnce(testing::Return(
          optimization_guide::OptimizationGuideDecision::kTrue));

  EXPECT_TRUE(IsLowRiskOrigin(*otr_rfh));
}

// Test fixture for scenarios where DigitalCredentialsKeyedService is not
// available.
class DigitalIdentityLowRiskOriginsServiceNotAvailableTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // Configure the factory to return nullptr, simulating service
    // unavailability.
    // Note: We use profile() from ChromeRenderViewHostTestHarness here,
    // as TestingProfileManager is not set up for this specific fixture.
    DigitalCredentialsKeyedServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return nullptr;
        }));
  }
};

// Test IsLowRiskOrigin when DigitalCredentialsKeyedService is not available.
// In this case, the internal IsLowFrictionUrl helper should return false.
TEST_F(DigitalIdentityLowRiskOriginsServiceNotAvailableTest,
       IsLowRiskOrigin_ServiceNotAvailable) {
  NavigateAndCommit(GURL("https://example.com"));
  // Since kKnownLowRiskOrigins is empty and the service isn't available (so
  // low friction check is false), the result should be false.
  EXPECT_FALSE(IsLowRiskOrigin(*main_rfh()));
}

// Tests for IsLowRiskOriginMatcherForTesting (allows custom lists)

TEST(IsLowRiskOriginMatcherTest, ExactMatch) {
  std::vector<std::string> test_origins = {"https://example.com"};
  EXPECT_TRUE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("https://example.com"), test_origins));
}

TEST(IsLowRiskOriginMatcherTest, WwwMatchesNonWwwInList) {
  std::vector<std::string> test_origins = {
      "https://example.com"};  // Non-www in list
  EXPECT_TRUE(
      IsLowRiskOriginMatcherForTesting(CreateOrigin("https://www.example.com"),
                                       test_origins));  // Check www
}

TEST(IsLowRiskOriginMatcherTest, NonWwwMatchesWwwInList) {
  std::vector<std::string> test_origins = {
      "https://www.example.com"};  // Www in list
  EXPECT_TRUE(
      IsLowRiskOriginMatcherForTesting(CreateOrigin("https://example.com"),
                                       test_origins));  // Check non-www
}

TEST(IsLowRiskOriginMatcherTest, NoMatchDifferentDomain) {
  std::vector<std::string> test_origins = {"https://example.com"};
  EXPECT_FALSE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("https://different.com"), test_origins));
}

TEST(IsLowRiskOriginMatcherTest, SchemeMismatch) {
  std::vector<std::string> test_origins = {"https://example.com"};
  EXPECT_FALSE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("http://example.com"), test_origins));
  EXPECT_FALSE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("http://www.example.com"), test_origins));
}

TEST(IsLowRiskOriginMatcherTest, PortMismatch) {
  std::vector<std::string> test_origins = {
      "https://example.com"};  // Default port 443
  EXPECT_FALSE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("https://example.com:8080"), test_origins));
}

TEST(IsLowRiskOriginMatcherTest, SubdomainMismatch) {
  std::vector<std::string> test_origins = {"https://example.com"};
  // "www" is handled, other subdomains are not.
  EXPECT_FALSE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("https://sub.example.com"), test_origins));
}

TEST(IsLowRiskOriginMatcherTest, PathIsIgnored) {
  std::vector<std::string> test_origins = {"https://example.com"};
  EXPECT_TRUE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("https://example.com/some/path"), test_origins));
}

TEST(IsLowRiskOriginMatcherTest, EmptyList) {
  std::vector<std::string> empty_origins_list = {};
  EXPECT_FALSE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("https://example.com"), empty_origins_list));
}

TEST(IsLowRiskOriginMatcherTest, MultipleEntriesInList_Match) {
  std::vector<std::string> test_origins = {"https://another.com",
                                           "https://example.com"};
  EXPECT_TRUE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("https://example.com"), test_origins));
}

TEST(IsLowRiskOriginMatcherTest, MultipleEntriesInList_NoMatch) {
  std::vector<std::string> test_origins = {"https://another.com",
                                           "https://yetanother.com"};
  EXPECT_FALSE(IsLowRiskOriginMatcherForTesting(
      CreateOrigin("https://example.com"), test_origins));
}

}  // namespace digital_credentials

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_observer.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/dictation_metadata.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace dictation {

namespace {

using ::optimization_guide::OptimizationGuideDecision;
using ::optimization_guide::OptimizationMetadata;
using ::optimization_guide::PageContextEligibilityStatus;
using ::optimization_guide::proto::DICTATION_ELIGIBLE;
using ::optimization_guide::proto::DictationMetadata;
using ::optimization_guide::proto::OptimizationType;
using ::testing::An;
using ::testing::ElementsAre;

using MockOptimizationGuide =
    testing::NiceMock<MockOptimizationGuideKeyedService>;

constexpr char kPageUrl[] = "https://example.com/page";
constexpr int kPolicyValueEnabledWithLogging = 0;
constexpr int kPolicyValueDisabled = 2;

}  // namespace

// Tests how DictationKeyedService registers and interprets the Optimization
// Guide DICTATION_ELIGIBLE hint that gates page context extraction.
class DictationPageEligibilityTest : public testing::Test {
 protected:
  DictationPageEligibilityTest()
      : dictation_feature_list_(CreateEnablingFeatureList()) {
    OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<MockOptimizationGuide>();
        }));
    optimization_guide_ = static_cast<MockOptimizationGuide*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(&profile_));
  }

  // Expects one DICTATION_ELIGIBLE query for `kPageUrl` and answers it with
  // `decision`. If `eligible` is set, attaches DictationMetadata with that
  // verdict, as the server does.
  void ExpectDecision(OptimizationGuideDecision decision,
                      std::optional<bool> eligible = std::nullopt) {
    EXPECT_CALL(*optimization_guide_,
                CanApplyOptimization(GURL(kPageUrl), DICTATION_ELIGIBLE,
                                     An<OptimizationMetadata*>()))
        .WillOnce([decision, eligible](const GURL&, OptimizationType,
                                       OptimizationMetadata* metadata) {
          if (eligible.has_value()) {
            DictationMetadata dictation_metadata;
            dictation_metadata.set_eligible(*eligible);
            metadata->set_any_metadata(
                optimization_guide::AnyWrapProto(dictation_metadata));
          }
          return decision;
        });
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::test::ScopedFeatureList dictation_feature_list_;
  raw_ptr<MockOptimizationGuide> optimization_guide_;
};

TEST_F(DictationPageEligibilityTest, RegistersOptimizationTypeOnInit) {
  EXPECT_CALL(*optimization_guide_,
              RegisterOptimizationTypes(ElementsAre(DICTATION_ELIGIBLE)));
  MockDictationKeyedService service(&profile_);
}

TEST_F(DictationPageEligibilityTest, SkipsRegistrationWhileDisabledByPolicy) {
  profile_.GetPrefs()->SetInteger(prefs::kVoiceTypingSettings,
                                  kPolicyValueDisabled);
  EXPECT_CALL(*optimization_guide_, RegisterOptimizationTypes).Times(0);
  MockDictationKeyedService service(&profile_);
  testing::Mock::VerifyAndClearExpectations(optimization_guide_);

  // Enabling via policy registers the optimization type.
  EXPECT_CALL(*optimization_guide_,
              RegisterOptimizationTypes(ElementsAre(DICTATION_ELIGIBLE)));
  profile_.GetPrefs()->SetInteger(prefs::kVoiceTypingSettings,
                                  kPolicyValueEnabledWithLogging);
}

TEST_F(DictationPageEligibilityTest, EligibleHintAllowsPageContext) {
  MockDictationKeyedService service(&profile_);
  ExpectDecision(OptimizationGuideDecision::kTrue, /*eligible=*/true);

  EXPECT_EQ(service.CheckOptimizationGuideHint(GURL(kPageUrl)),
            PageContextEligibilityStatus::kEligible);
}

// The server sends a hint for pages that failed its safety checks too, with the
// verdict in the metadata.
TEST_F(DictationPageEligibilityTest, IneligibleHintBlocksPageContext) {
  MockDictationKeyedService service(&profile_);
  ExpectDecision(OptimizationGuideDecision::kTrue, /*eligible=*/false);

  EXPECT_EQ(service.CheckOptimizationGuideHint(GURL(kPageUrl)),
            PageContextEligibilityStatus::kNotEligible);
}

TEST_F(DictationPageEligibilityTest, HintWithoutMetadataBlocksPageContext) {
  MockDictationKeyedService service(&profile_);
  ExpectDecision(OptimizationGuideDecision::kTrue);

  EXPECT_EQ(service.CheckOptimizationGuideHint(GURL(kPageUrl)),
            PageContextEligibilityStatus::kNotEligible);
}

TEST_F(DictationPageEligibilityTest, NoHintStatus) {
  MockDictationKeyedService service(&profile_);

  // The hint has not been fetched yet.
  ExpectDecision(OptimizationGuideDecision::kUnknown);
  EXPECT_EQ(service.CheckOptimizationGuideHint(GURL(kPageUrl)),
            PageContextEligibilityStatus::kUnknown);

  // The hint was fetched but has no DICTATION_ELIGIBLE optimization.
  ExpectDecision(OptimizationGuideDecision::kFalse);
  EXPECT_EQ(service.CheckOptimizationGuideHint(GURL(kPageUrl)),
            PageContextEligibilityStatus::kNotEligible);
}

TEST_F(DictationPageEligibilityTest,
       MissingOptimizationGuideBlocksPageContext) {
  // Reset the pointer before the mock it points to is destroyed.
  optimization_guide_ = nullptr;
  OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactory(
      &profile_, OptimizationGuideKeyedServiceFactory::TestingFactory());
  MockDictationKeyedService service(&profile_);

  EXPECT_EQ(service.CheckOptimizationGuideHint(GURL(kPageUrl)),
            PageContextEligibilityStatus::kNotEligible);
}

}  // namespace dictation

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_invoke_preparation.h"

#include <optional>

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicInvokePreparationTest : public testing::Test {
 protected:
  static Floating MakeFloatingSurface() { return Floating(); }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

namespace {

using ::base::test::ErrorIs;
using ::base::test::ValueIs;
using ::testing::Eq;

TEST_F(GlicInvokePreparationTest, ValidateInvokeOptions_ValidDefaultOptions) {
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  EXPECT_EQ(ValidateInvokeOptions(options), std::nullopt);
}

TEST_F(GlicInvokePreparationTest, ValidateInvokeOptions_NullTabHandleInvalid) {
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.target.surface = tabs::TabHandle::Null();
  EXPECT_THAT(ValidateInvokeOptions(options),
              testing::Optional(Eq(GlicInvokeError::kInvalidTab)));
}

TEST_F(GlicInvokePreparationTest, ValidateInvokeOptions_LiveModeBehaviorRules) {
  {
    GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
    options.target.surface = MakeFloatingSurface();
    options.target.live_mode_behavior =
        LiveModeBehavior::kForceSidePanelTextMode;
    EXPECT_THAT(ValidateInvokeOptions(options),
                testing::Optional(Eq(GlicInvokeError::kInvalidConfiguration)));
  }

  {
    GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
    options.target.surface = MakeFloatingSurface();
    options.target.live_mode_behavior =
        LiveModeBehavior::kForceFloatingTextMode;
    EXPECT_EQ(ValidateInvokeOptions(options), std::nullopt);
  }

  {
    GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
    options.target.surface = LastActiveOrNew{nullptr, true};
    options.target.live_mode_behavior =
        LiveModeBehavior::kForceSidePanelTextMode;
    EXPECT_THAT(ValidateInvokeOptions(options),
                testing::Optional(Eq(GlicInvokeError::kInvalidConfiguration)));
  }

  {
    GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
    options.target.surface = LastActiveOrNew{nullptr, true};
    options.target.live_mode_behavior =
        LiveModeBehavior::kForceFloatingTextMode;
    EXPECT_THAT(ValidateInvokeOptions(options),
                testing::Optional(Eq(GlicInvokeError::kInvalidConfiguration)));
  }

  {
    GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
    options.target.surface = LastActiveOrNew{nullptr, true};
    options.target.live_mode_behavior = LiveModeBehavior::kFail;
    EXPECT_EQ(ValidateInvokeOptions(options), std::nullopt);
  }
}

TEST_F(GlicInvokePreparationTest,
       ValidateInvokeOptions_ConflictingClipboardPolicyChecksInvalid) {
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.additional_context.emplace(mojom::AdditionalContext::New(),
                                     content::GlobalRenderFrameHostId(1, 1),
                                     PolicyCheck::kClipboard);
  options.prompts = {"prompt"};
  options.prompt_policy = PromptPolicy{content::GlobalRenderFrameHostId(1, 1),
                                       PolicyCheck::kClipboard};

  EXPECT_THAT(ValidateInvokeOptions(options),
              testing::Optional(Eq(GlicInvokeError::kInvalidConfiguration)));
}

TEST_F(GlicInvokePreparationTest, ResolveFloatyPreservation_NotInLiveMode) {
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.preserve_active_surface = false;
  EXPECT_THAT(ResolveFloatyPreservation(/*is_showing_live_mode=*/false,
                                        /*is_floating=*/true, options),
              ValueIs(false));

  options.preserve_active_surface = true;
  EXPECT_THAT(ResolveFloatyPreservation(/*is_showing_live_mode=*/false,
                                        /*is_floating=*/true, options),
              ValueIs(true));
  EXPECT_THAT(ResolveFloatyPreservation(/*is_showing_live_mode=*/false,
                                        /*is_floating=*/false, options),
              ValueIs(false));

  // `live_mode_behavior` is ignored when the instance is not showing in live
  // mode.
  options.target.live_mode_behavior = LiveModeBehavior::kFail;
  EXPECT_THAT(ResolveFloatyPreservation(/*is_showing_live_mode=*/false,
                                        /*is_floating=*/false, options),
              ValueIs(false));

  options.target.live_mode_behavior = LiveModeBehavior::kForceFloatingTextMode;
  EXPECT_THAT(ResolveFloatyPreservation(/*is_showing_live_mode=*/false,
                                        /*is_floating=*/false, options),
              ValueIs(false));
}

TEST_F(GlicInvokePreparationTest, ResolveFloatyPreservation_InLiveMode) {
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);

  options.target.live_mode_behavior = LiveModeBehavior::kFail;
  EXPECT_THAT(ResolveFloatyPreservation(/*is_showing_live_mode=*/true,
                                        /*is_floating=*/true, options),
              ErrorIs(GlicInvokeError::kLiveModeActive));

  options.target.live_mode_behavior = LiveModeBehavior::kProceedInLiveMode;
  EXPECT_THAT(ResolveFloatyPreservation(/*is_showing_live_mode=*/true,
                                        /*is_floating=*/true, options),
              ValueIs(true));
  EXPECT_THAT(ResolveFloatyPreservation(/*is_showing_live_mode=*/true,
                                        /*is_floating=*/false, options),
              ValueIs(false));

  options.target.live_mode_behavior = LiveModeBehavior::kForceFloatingTextMode;
  EXPECT_THAT(ResolveFloatyPreservation(/*is_showing_live_mode=*/true,
                                        /*is_floating=*/false, options),
              ValueIs(true));

  options.target.live_mode_behavior = LiveModeBehavior::kForceSidePanelTextMode;
  EXPECT_THAT(ResolveFloatyPreservation(/*is_showing_live_mode=*/true,
                                        /*is_floating=*/true, options),
              ValueIs(false));
}

TEST_F(GlicInvokePreparationTest, ShouldUseTrustFirstInlineFre) {
  profile_.GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kNotStarted));
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {}, {features::kGlicMessageFirstFre, features::kGlicActionFirstFRE});
    EXPECT_FALSE(ShouldUseTrustFirstInlineFre(&profile_));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({features::kGlicMessageFirstFre},
                                  {features::kGlicActionFirstFRE});
    EXPECT_TRUE(ShouldUseTrustFirstInlineFre(&profile_));

    profile_.GetPrefs()->SetInteger(
        prefs::kGlicCompletedFre,
        static_cast<int>(prefs::FreStatus::kCompleted));
    EXPECT_FALSE(ShouldUseTrustFirstInlineFre(&profile_));
  }

  profile_.GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kNotStarted));
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({features::kGlicActionFirstFRE},
                                  {features::kGlicMessageFirstFre});
    EXPECT_TRUE(ShouldUseTrustFirstInlineFre(&profile_));

    profile_.GetPrefs()->SetInteger(
        prefs::kGlicCompletedFre,
        static_cast<int>(prefs::FreStatus::kCompleted));
    EXPECT_FALSE(ShouldUseTrustFirstInlineFre(&profile_));
  }
}

TEST_F(GlicInvokePreparationTest, HasSpecialCaseWebFre) {
  GlicInvokeOptions default_options(mojom::InvocationSource::kOsButton);
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {}, {features::kGlicMessageFirstFre, features::kGlicActionFirstFRE});
    EXPECT_FALSE(HasSpecialCaseWebFre(default_options));

    GlicInvokeOptions feature_mode_options(mojom::InvocationSource::kOsButton);
    feature_mode_options.feature_mode = mojom::FeatureMode::kActuation;
    EXPECT_TRUE(HasSpecialCaseWebFre(feature_mode_options));

    GlicInvokeOptions special_source_options(
        mojom::InvocationSource::kUniversalCart);
    EXPECT_TRUE(HasSpecialCaseWebFre(special_source_options));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({features::kGlicMessageFirstFre},
                                  {features::kGlicActionFirstFRE});
    EXPECT_TRUE(HasSpecialCaseWebFre(default_options));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({features::kGlicActionFirstFRE},
                                  {features::kGlicMessageFirstFre});
    EXPECT_TRUE(HasSpecialCaseWebFre(default_options));
  }
}

}  // namespace
}  // namespace glic

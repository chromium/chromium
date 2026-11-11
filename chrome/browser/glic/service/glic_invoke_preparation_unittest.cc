// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_invoke_preparation.h"

#include <cstdint>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/context/glic_delegating_sharing_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

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

class FakeSharingManager : public GlicDelegatingSharingManagerBase {
 public:
  int32_t GetMaxPinnedTabs() const override { return max_pinned_tabs_; }
  int32_t GetNumPinnedTabs() const override {
    return static_cast<int32_t>(pinned_tabs_.size());
  }
  bool IsTabPinned(tabs::TabHandle tab_handle) const override {
    return pinned_tabs_.contains(tab_handle);
  }

  void set_max_pinned_tabs(int32_t max_pinned) {
    max_pinned_tabs_ = max_pinned;
  }
  void AddPinnedTab(tabs::TabHandle handle) { pinned_tabs_.insert(handle); }

 private:
  int32_t max_pinned_tabs_ = 10;
  std::set<tabs::TabHandle> pinned_tabs_;
};

class GlicInvokeSelectedTabsPreparationTest : public GlicInvokePreparationTest {
 protected:
  void SetUp() override {
    ON_CALL(std::as_const(browser_window_), GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(user_data_host_));
    tab_list_registration_ =
        std::make_unique<ui::ScopedUnownedUserData<TabListInterface>>(
            browser_window_.GetUnownedUserDataHost(), tab_list_);
  }

  void ConfigureTab(tabs::MockTabInterface& tab, bool selected) {
    ON_CALL(tab, GetBrowserWindowInterface())
        .WillByDefault(testing::Return(&browser_window_));
    ON_CALL(tab, IsSelected()).WillByDefault(testing::Return(selected));
  }

  ui::UnownedUserDataHost user_data_host_;
  testing::NiceMock<MockBrowserWindowInterface> browser_window_;
  testing::NiceMock<MockTabListInterface> tab_list_;
  std::unique_ptr<ui::ScopedUnownedUserData<TabListInterface>>
      tab_list_registration_;
  FakeSharingManager sharing_manager_;
};

TEST_F(GlicInvokeSelectedTabsPreparationTest,
       PopulatesSelectedAndActiveTabsForDefaultInvoke) {
  tabs::MockTabInterface tab0;
  tabs::MockTabInterface tab1;
  tabs::MockTabInterface tab2;
  tabs::MockTabInterface tab3;
  ConfigureTab(tab0, /*selected=*/true);
  ConfigureTab(tab1, /*selected=*/false);
  ConfigureTab(tab2, /*selected=*/true);
  ConfigureTab(tab3, /*selected=*/true);

  ON_CALL(tab_list_, GetAllTabs())
      .WillByDefault(testing::Return(
          std::vector<tabs::TabInterface*>{&tab0, &tab1, &tab2, &tab3}));
  ON_CALL(tab_list_, GetActiveTab()).WillByDefault(testing::Return(&tab2));

  GlicInvokeOptions options(mojom::InvocationSource::kTopChromeButton);
  MaybePopulateSelectedTabsToPin(
      &profile_, sharing_manager_,
      GlicInvokeHandler::TabSurface{&tab2, /*is_new=*/false},
      /*has_auto_submit_passkey=*/false, options);

  EXPECT_THAT(options.tab_sharing.tabs_to_pin,
              testing::ElementsAre(tab0.GetHandle(), tab2.GetHandle(),
                                   tab3.GetHandle()));
  EXPECT_EQ(options.tab_sharing.pin_trigger, GlicPinTrigger::kInstanceCreation);
}

TEST_F(GlicInvokeSelectedTabsPreparationTest,
       SingleSelectedActiveTabLeavesTabsToPinEmpty) {
  tabs::MockTabInterface tab0;
  tabs::MockTabInterface tab1;
  ConfigureTab(tab0, /*selected=*/true);
  ConfigureTab(tab1, /*selected=*/false);

  ON_CALL(tab_list_, GetAllTabs())
      .WillByDefault(
          testing::Return(std::vector<tabs::TabInterface*>{&tab0, &tab1}));
  ON_CALL(tab_list_, GetActiveTab()).WillByDefault(testing::Return(&tab0));

  GlicInvokeOptions options(mojom::InvocationSource::kTopChromeButton);
  MaybePopulateSelectedTabsToPin(
      &profile_, sharing_manager_,
      GlicInvokeHandler::TabSurface{&tab0, /*is_new=*/false},
      /*has_auto_submit_passkey=*/false, options);

  EXPECT_THAT(options.tab_sharing.tabs_to_pin, testing::IsEmpty());
}

TEST_F(GlicInvokeSelectedTabsPreparationTest,
       SingleActiveSplitTabLeavesTabsToPinEmpty) {
  const split_tabs::SplitTabId split_id = split_tabs::SplitTabId::GenerateNew();
  tabs::MockTabInterface split_tab0;
  tabs::MockTabInterface split_tab1;
  tabs::MockTabInterface other_tab;
  ConfigureTab(split_tab0, /*selected=*/true);
  ConfigureTab(split_tab1, /*selected=*/true);
  ConfigureTab(other_tab, /*selected=*/false);
  ON_CALL(split_tab0, GetSplit()).WillByDefault(testing::Return(split_id));
  ON_CALL(split_tab1, GetSplit()).WillByDefault(testing::Return(split_id));

  ON_CALL(tab_list_, GetAllTabs())
      .WillByDefault(testing::Return(std::vector<tabs::TabInterface*>{
          &split_tab0, &split_tab1, &other_tab}));
  ON_CALL(tab_list_, GetActiveTab())
      .WillByDefault(testing::Return(&split_tab0));

  GlicInvokeOptions options(mojom::InvocationSource::kTopChromeButton);
  MaybePopulateSelectedTabsToPin(
      &profile_, sharing_manager_,
      GlicInvokeHandler::TabSurface{&split_tab0, /*is_new=*/false},
      /*has_auto_submit_passkey=*/false, options);

  EXPECT_THAT(options.tab_sharing.tabs_to_pin, testing::IsEmpty());

  // Multi-selecting an additional tab outside the split populates all selected
  // tabs.
  ON_CALL(other_tab, IsSelected()).WillByDefault(testing::Return(true));
  MaybePopulateSelectedTabsToPin(
      &profile_, sharing_manager_,
      GlicInvokeHandler::TabSurface{&split_tab0, /*is_new=*/false},
      /*has_auto_submit_passkey=*/false, options);

  EXPECT_THAT(
      options.tab_sharing.tabs_to_pin,
      testing::ElementsAre(split_tab0.GetHandle(), split_tab1.GetHandle(),
                           other_tab.GetHandle()));
}

TEST_F(GlicInvokeSelectedTabsPreparationTest,
       FiltersAlreadyPinnedAndPreservesActiveTabUnderLimit) {
  tabs::MockTabInterface tab0;
  tabs::MockTabInterface tab1;
  tabs::MockTabInterface tab2;
  tabs::MockTabInterface tab3;
  ConfigureTab(tab0, /*selected=*/true);
  ConfigureTab(tab1, /*selected=*/true);
  ConfigureTab(tab2, /*selected=*/true);
  ConfigureTab(tab3, /*selected=*/true);

  sharing_manager_.set_max_pinned_tabs(3);
  sharing_manager_.AddPinnedTab(tab0.GetHandle());

  ON_CALL(tab_list_, GetAllTabs())
      .WillByDefault(testing::Return(
          std::vector<tabs::TabInterface*>{&tab0, &tab1, &tab2, &tab3}));
  ON_CALL(tab_list_, GetActiveTab()).WillByDefault(testing::Return(&tab3));

  GlicInvokeOptions options(mojom::InvocationSource::kTopChromeButton);
  MaybePopulateSelectedTabsToPin(
      &profile_, sharing_manager_,
      GlicInvokeHandler::TabSurface{&tab3, /*is_new=*/false},
      /*has_auto_submit_passkey=*/false, options);

  // `tab0` is already pinned, leaving 2 available slots for `{tab1, tab2,
  // tab3}`. Active `tab3` is rotated into slot 1 so `PinTabs` pins `tab1` and
  // `tab3`.
  ASSERT_GE(options.tab_sharing.tabs_to_pin.size(), 2u);
  EXPECT_EQ(options.tab_sharing.tabs_to_pin[0], tab1.GetHandle());
  EXPECT_EQ(options.tab_sharing.tabs_to_pin[1], tab3.GetHandle());
}

TEST_F(GlicInvokeSelectedTabsPreparationTest,
       SkipsClientInvokeNewTabOrOptOutConfigurations) {
  tabs::MockTabInterface tab0;
  tabs::MockTabInterface tab1;
  ConfigureTab(tab0, /*selected=*/true);
  ConfigureTab(tab1, /*selected=*/true);
  ON_CALL(tab_list_, GetAllTabs())
      .WillByDefault(
          testing::Return(std::vector<tabs::TabInterface*>{&tab0, &tab1}));
  ON_CALL(tab_list_, GetActiveTab()).WillByDefault(testing::Return(&tab0));

  // Prompts (client invoke) should not expand multi-selection.
  {
    GlicInvokeOptions options(mojom::InvocationSource::kTopChromeButton);
    options.prompts = {"Summarize"};
    MaybePopulateSelectedTabsToPin(
        &profile_, sharing_manager_,
        GlicInvokeHandler::TabSurface{&tab0, /*is_new=*/false},
        /*has_auto_submit_passkey=*/false, options);
    EXPECT_THAT(options.tab_sharing.tabs_to_pin, testing::IsEmpty());
  }

  // Newly opened surface (`is_new = true`) should not expand multi-selection.
  {
    GlicInvokeOptions options(mojom::InvocationSource::kTopChromeButton);
    MaybePopulateSelectedTabsToPin(
        &profile_, sharing_manager_,
        GlicInvokeHandler::TabSurface{&tab0, /*is_new=*/true},
        /*has_auto_submit_passkey=*/false, options);
    EXPECT_THAT(options.tab_sharing.tabs_to_pin, testing::IsEmpty());
  }

  // Killswitch disabled should not expand multi-selection.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitFromCommandLine(
        /*enable_features=*/"",
        /*disable_features=*/"GlicPinSelectedTabsOnInvoke");

    GlicInvokeOptions options(mojom::InvocationSource::kTopChromeButton);
    MaybePopulateSelectedTabsToPin(
        &profile_, sharing_manager_,
        GlicInvokeHandler::TabSurface{&tab0, /*is_new=*/false},
        /*has_auto_submit_passkey=*/false, options);
    EXPECT_THAT(options.tab_sharing.tabs_to_pin, testing::IsEmpty());
  }

  // Default tab context pref disabled should not expand multi-selection.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({features::kGlicDefaultTabContextSetting,
                                   features::kGlicDefaultContextPinOnBind},
                                  {});
    profile_.GetPrefs()->SetBoolean(prefs::kGlicDefaultTabContextEnabled,
                                    false);

    GlicInvokeOptions options(mojom::InvocationSource::kTopChromeButton);
    MaybePopulateSelectedTabsToPin(
        &profile_, sharing_manager_,
        GlicInvokeHandler::TabSurface{&tab0, /*is_new=*/false},
        /*has_auto_submit_passkey=*/false, options);
    EXPECT_THAT(options.tab_sharing.tabs_to_pin, testing::IsEmpty());
  }
}

}  // namespace
}  // namespace glic

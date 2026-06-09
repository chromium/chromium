// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/suggestions/glic_cue_target.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/new_glic_api_test.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicCueTargetBrowserTest : public GlicApiBrowserTest {
 public:
  GlicCueTargetBrowserTest()
      : GlicApiBrowserTest("./glic_cue_target_browsertest.js") {}
  ~GlicCueTargetBrowserTest() override = default;

  void SetUpOnMainThread() override {
    GlicApiBrowserTest::SetUpOnMainThread();
    GlicEnabling::SetBypassEnablementChecksForTesting(true);
  }

  void TearDownOnMainThread() override {
    GlicEnabling::SetBypassEnablementChecksForTesting(false);
    GlicApiBrowserTest::TearDownOnMainThread();
  }
};

class GlicCueTargetBrowserTestAutoSubmitEnabled
    : public GlicCueTargetBrowserTest {
 public:
  GlicCueTargetBrowserTestAutoSubmitEnabled() {
    features_.InitAndEnableFeature(features::kGlicContextualCueingV2AutoSubmit);
  }

 private:
  base::test::ScopedFeatureList features_;
};

class GlicCueTargetBrowserTestAutoSubmitDisabled
    : public GlicCueTargetBrowserTest {
 public:
  GlicCueTargetBrowserTestAutoSubmitDisabled() {
    features_.InitAndDisableFeature(
        features::kGlicContextualCueingV2AutoSubmit);
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(GlicCueTargetBrowserTest, testIsEligible) {
  GlicCueTarget target(*service(), nullptr, *GetBrowser());

  // Eligible by default (enablement bypass is set in SetUpOnMainThread).
  EXPECT_TRUE(target.IsEligible());

  // Ineligible when the Glic panel is showing.
  ASSERT_OK(OpenGlicForActiveTab());
  EXPECT_FALSE(target.IsEligible());

  // Eligible again once the panel is closed.
  ASSERT_OK(CloseGlicForTabAndWait(GetTabListInterface()->GetActiveTab()));
  EXPECT_TRUE(target.IsEligible());

  // Ineligible if Glic is not pinned to the tabstrip.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, false);
  EXPECT_FALSE(target.IsEligible());

  // Ineligible if tab context sharing is disabled.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, true);
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicDefaultTabContextEnabled,
                                       false);
  EXPECT_FALSE(target.IsEligible());

  // Clean up.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicDefaultTabContextEnabled,
                                       true);
  EXPECT_TRUE(target.IsEligible());
}

IN_PROC_BROWSER_TEST_F(GlicCueTargetBrowserTestAutoSubmitEnabled,
                       testOnClickAutoSubmitEnabled) {
  GlicCueTarget target(*service(), nullptr, *GetBrowser());

  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  ActivateTab(tab1);

  contextual_cueing::GlicCueActionData glic_data;
  glic_data.prompt = "test prompt auto submit";
  glic_data.tabs_to_share.emplace_back(tab2->GetHandle());

  contextual_cueing::CueActionData data = glic_data;

  // Clicking should invoke and auto-open.
  target.OnClick(data);

  // Verifies that the JS client receives the correct prompt and
  // autoSubmit=true.
  ExecuteJsTest();

  // Verify tab2 was pinned.
  auto* instance = GetOnlyGlicInstance();
  ASSERT_TRUE(instance);
  EXPECT_TRUE(instance->IsShowing());
  EXPECT_TRUE(instance->sharing_manager().IsTabPinned(tab2->GetHandle()));
}

IN_PROC_BROWSER_TEST_F(GlicCueTargetBrowserTestAutoSubmitDisabled,
                       testOnClickAutoSubmitDisabled) {
  GlicCueTarget target(*service(), nullptr, *GetBrowser());

  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  ActivateTab(tab1);

  contextual_cueing::GlicCueActionData glic_data;
  glic_data.prompt = "test prompt no auto submit";
  glic_data.tabs_to_share.emplace_back(tab2->GetHandle());

  contextual_cueing::CueActionData data = glic_data;

  // Clicking should invoke but not auto-submit.
  target.OnClick(data);

  // Verifies that the JS client receives the correct prompt and
  // autoSubmit=false.
  ExecuteJsTest();

  // Verify tab2 was pinned.
  auto* instance = GetOnlyGlicInstance();
  ASSERT_TRUE(instance);
  EXPECT_TRUE(instance->IsShowing());
  EXPECT_TRUE(instance->sharing_manager().IsTabPinned(tab2->GetHandle()));
}

IN_PROC_BROWSER_TEST_F(GlicCueTargetBrowserTest, testOnEditPrompt) {
  GlicCueTarget target(*service(), nullptr, *GetBrowser());

  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));
  ActivateTab(tab1);

  contextual_cueing::GlicCueActionData glic_data;
  glic_data.prompt = "test prompt edit prompt";
  glic_data.tabs_to_share.emplace_back(tab2->GetHandle());

  contextual_cueing::CueActionData data = glic_data;

  // OnEditPrompt should invoke but not auto-submit.
  target.OnEditPrompt(data);

  // Verifies that the JS client receives the correct prompt and
  // autoSubmit=false.
  ExecuteJsTest();

  // Verify tab2 was pinned.
  auto* instance = GetOnlyGlicInstance();
  ASSERT_TRUE(instance);
  EXPECT_TRUE(instance->IsShowing());
  EXPECT_TRUE(instance->sharing_manager().IsTabPinned(tab2->GetHandle()));
}

IN_PROC_BROWSER_TEST_F(GlicCueTargetBrowserTest, testGetIcon) {
  GlicCueTarget target(*service(), nullptr, *GetBrowser());
  EXPECT_FALSE(target.GetAnchoredMessageIcon().IsEmpty());
  EXPECT_FALSE(target.GetOmniboxChipIcon().IsEmpty());
}

IN_PROC_BROWSER_TEST_F(GlicCueTargetBrowserTest,
                       testCueActionDataFromResponse) {
  GlicCueTarget target(*service(), nullptr, *GetBrowser());

  optimization_guide::proto::ContextualCue cue;
  auto* surface = cue.mutable_gemini_in_chrome_surface();
  surface->set_prompt("response prompt");

  contextual_cueing::CueActionData data =
      target.CueActionDataFromResponse(cue, {});
  ASSERT_TRUE(
      std::holds_alternative<contextual_cueing::GlicCueActionData>(data));
  auto& glic_data = std::get<contextual_cueing::GlicCueActionData>(data);
  EXPECT_EQ("response prompt", glic_data.prompt);
}

}  // namespace glic

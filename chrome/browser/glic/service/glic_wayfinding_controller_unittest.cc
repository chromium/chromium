// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_wayfinding_controller.h"

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

using mojom::InvocationSource;

class GlicWayfindingControllerTest : public testing::Test {
 protected:
  GlicWayfindingControllerTest() {
    feature_list_.InitAndEnableFeature(features::kGlicAskGeminiWayfinding);
    prefs::RegisterProfilePrefs(pref_service_.registry());
  }

  InvocationSource FirstSource() {
    return static_cast<InvocationSource>(
        pref_service_.GetInteger(prefs::kGlicWayfindingFirstInvocationSource));
  }

  bool UsedSelfStartedOpen() {
    return pref_service_.GetBoolean(prefs::kGlicWayfindingUsedSelfStartedOpen);
  }

  void Invoke(InvocationSource source) { controller_.OnInvoke(source); }

  base::test::ScopedFeatureList feature_list_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  GlicWayfindingController controller_{&pref_service_,
                                       /*was_opened_before=*/false};
};

TEST_F(GlicWayfindingControllerTest, RecordsFirstInvocationSource) {
  Invoke(InvocationSource::kWebContentsContextMenuPage);
  Invoke(InvocationSource::kThreeDotsMenu);
  EXPECT_EQ(FirstSource(), InvocationSource::kWebContentsContextMenuPage);
  EXPECT_FALSE(UsedSelfStartedOpen());
}

TEST_F(GlicWayfindingControllerTest, ExistingUserHasNoFirstInvocationSource) {
  GlicWayfindingController controller(&pref_service_,
                                      /*was_opened_before=*/true);
  controller.OnInvoke(InvocationSource::kWebContentsContextMenuPage);
  EXPECT_EQ(FirstSource(), InvocationSource::kUnsupported);
}

TEST_F(GlicWayfindingControllerTest, RecordsButtonOrHotkeyOpen) {
  for (InvocationSource source :
       {InvocationSource::kTopChromeButton, InvocationSource::kToolbarButton,
        InvocationSource::kNudge, InvocationSource::kOsHotkey}) {
    pref_service_.ClearPref(prefs::kGlicWayfindingUsedSelfStartedOpen);
    Invoke(source);
    EXPECT_TRUE(UsedSelfStartedOpen()) << "source=" << source;
  }
}

TEST_F(GlicWayfindingControllerTest, DoesNothingWhenFeatureDisabled) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(features::kGlicAskGeminiWayfinding);
  Invoke(InvocationSource::kTopChromeButton);
  EXPECT_EQ(FirstSource(), InvocationSource::kUnsupported);
  EXPECT_FALSE(UsedSelfStartedOpen());
}

}  // namespace
}  // namespace glic

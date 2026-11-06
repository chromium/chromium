// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/host/glic_no_webview_contents_manager.h"
#include "chrome/browser/glic/host/glic_overlay.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

struct RelaunchPanelVariant {
  mojom::ErrorPanelType error_type;
  const char* name;
};

// Opens Glic and shows the relaunch-required panel of its NoWebview overlay
// for each pending provider change.
class GlicRelaunchPanelBrowserTest
    : public GlicBrowserTest,
      public testing::WithParamInterface<RelaunchPanelVariant> {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kGlicNoWebview};
};

IN_PROC_BROWSER_TEST_P(GlicRelaunchPanelBrowserTest, ShowsRelaunchPanel) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  auto* manager = static_cast<GlicNoWebviewContentsManager*>(
      instance->host().contents_manager());
  ASSERT_TRUE(manager);

  manager->SetErrorState(GetParam().error_type,
                         ClientLoadErrorReason::kPendingProviderChange);

  ASSERT_OK(WaitForDisplayState(
      GlicNoWebviewContentsManager::DisplayState::kShowingOverlay, instance));
  EXPECT_OK(WaitForOverlayErrorPanel(GetParam().error_type, instance));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    GlicRelaunchPanelBrowserTest,
    testing::Values(
        RelaunchPanelVariant{
            mojom::ErrorPanelType::kPendingProviderChangeGeminiToGeic,
            "GeminiToGeic"},
        RelaunchPanelVariant{
            mojom::ErrorPanelType::kPendingProviderChangeGeicToGemini,
            "GeicToGemini"},
        RelaunchPanelVariant{
            mojom::ErrorPanelType::kPendingProviderChangeGeicToOtherGeic,
            "GeicToOtherGeic"}),
    [](const testing::TestParamInfo<RelaunchPanelVariant>& info) {
      return std::string(info.param.name);
    });

}  // namespace
}  // namespace glic

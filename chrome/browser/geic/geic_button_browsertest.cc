// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geic/geic_button.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/geic/geic_enabling.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view_class_properties.h"

namespace geic {

struct GeicButtonTestParams {
  bool geic_switch_enabled = false;
  bool disable_web_security_switch_enabled = false;
  bool glic_feature_enabled = false;
  bool expect_geic_enabled = false;
};

class GeicButtonBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<GeicButtonTestParams> {
 public:
  GeicButtonBrowserTest() {
    if (GetParam().glic_feature_enabled) {
      feature_list_.InitAndEnableFeature(features::kGlic);
    } else {
      feature_list_.InitAndDisableFeature(features::kGlic);
    }
  }

  ~GeicButtonBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    if (GetParam().geic_switch_enabled) {
      command_line->AppendSwitch(geic::switches::kGeicEnabled);
    }
    if (GetParam().disable_web_security_switch_enabled) {
      command_line->AppendSwitch(::switches::kDisableWebSecurity);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(GeicButtonBrowserTest, IsGeicEnabled) {
  EXPECT_EQ(IsGeicEnabled(browser()->GetProfile()),
            GetParam().expect_geic_enabled);
}

IN_PROC_BROWSER_TEST_P(GeicButtonBrowserTest, GeicButtonCreatedAndConfigured) {
  if (!GetParam().expect_geic_enabled) {
    return;
  }

  TabStripActionContainer* container =
      BrowserElementsViews::From(browser())->GetViewAs<TabStripActionContainer>(
          kTabStripActionContainerElementId);
  ASSERT_TRUE(container);

  GeicButton* button = container->GetGeicButtonForTesting();
  ASSERT_NE(container->GetGeicButtonForTesting(), nullptr);
  EXPECT_EQ(container->GetGlicButton(), nullptr);

  // Check tooltip text.
  EXPECT_EQ(button->GetTooltipText(),
            l10n_util::GetStringUTF16(IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP));

  // Check layout properties.
  views::LayoutAlignment* alignment =
      button->GetProperty(views::kCrossAxisAlignmentKey);
  ASSERT_NE(alignment, nullptr);
  EXPECT_EQ(*alignment, views::LayoutAlignment::kCenter);

  // Check default visual / animation states.
  EXPECT_FALSE(button->GetIsShowingNudge());
}

IN_PROC_BROWSER_TEST_P(GeicButtonBrowserTest,
                       ContainerNotCreatedWhenGeicAndGlicDisabled) {
  if (GetParam().expect_geic_enabled || GetParam().glic_feature_enabled) {
    return;
  }

  TabStripActionContainer* container =
      BrowserElementsViews::From(browser())->GetViewAs<TabStripActionContainer>(
          kTabStripActionContainerElementId);
  EXPECT_EQ(container, nullptr);
}

IN_PROC_BROWSER_TEST_P(GeicButtonBrowserTest,
                       GeicButtonNotCreatedWhenGlicEnabled) {
  if (!GetParam().glic_feature_enabled) {
    return;
  }

  EXPECT_FALSE(IsGeicEnabled(browser()->GetProfile()));

  TabStripActionContainer* container =
      BrowserElementsViews::From(browser())->GetViewAs<TabStripActionContainer>(
          kTabStripActionContainerElementId);

#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, Glic is not eligible in test profiles without an active user
  // session, so the action container is not created.
  EXPECT_EQ(container, nullptr);
#else
  // On desktop platforms, Glic is enabled, so the container exists with the
  // Glic button and no GEiC button.
  ASSERT_TRUE(container);
  EXPECT_EQ(container->GetGeicButtonForTesting(), nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GeicButtonBrowserTest,
    testing::Values(
        // All conditions met: GEiC button is enabled.
        GeicButtonTestParams{.geic_switch_enabled = true,
                             .disable_web_security_switch_enabled = true,
                             .glic_feature_enabled = false,
                             .expect_geic_enabled = true},
        // Missing --geic-enabled switch.
        GeicButtonTestParams{.geic_switch_enabled = false,
                             .disable_web_security_switch_enabled = true,
                             .glic_feature_enabled = false,
                             .expect_geic_enabled = false},
        // Missing --disable-web-security switch.
        GeicButtonTestParams{.geic_switch_enabled = true,
                             .disable_web_security_switch_enabled = false,
                             .glic_feature_enabled = false,
                             .expect_geic_enabled = false},
        // GLIC feature enabled (mutual exclusivity).
        GeicButtonTestParams{.geic_switch_enabled = true,
                             .disable_web_security_switch_enabled = true,
                             .glic_feature_enabled = true,
                             .expect_geic_enabled = false}));

}  // namespace geic

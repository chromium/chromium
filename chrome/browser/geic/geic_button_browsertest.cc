// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geic/geic_button.h"

#include <optional>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"

// Mirrors the GN condition gating //chrome/test/base:scoped_channel_override —
// the class is declared for all branded builds but only linked on these
// platforms.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
#define GEIC_SUPPORTS_CHANNEL_OVERRIDE 1
#else
#define GEIC_SUPPORTS_CHANNEL_OVERRIDE 0
#endif

#include "chrome/browser/geic/geic_enabling.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/base/in_process_browser_test.h"
#if GEIC_SUPPORTS_CHANNEL_OVERRIDE
#include "chrome/test/base/scoped_channel_override.h"
#endif
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/test_event.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view_class_properties.h"

namespace geic {

struct GeicButtonTestParams {
  bool geic_switch_enabled = false;
  bool disable_web_security_switch_enabled = false;
  bool glic_feature_enabled = false;
#if GEIC_SUPPORTS_CHANNEL_OVERRIDE
  std::optional<chrome::ScopedChannelOverride::Channel> channel_override;
#endif
  bool expect_enabled = false;
};

class GeicButtonBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<GeicButtonTestParams> {
 public:
  GeicButtonBrowserTest() {
#if GEIC_SUPPORTS_CHANNEL_OVERRIDE
    if (GetParam().channel_override.has_value()) {
      channel_override_.emplace(*GetParam().channel_override);
    }
#endif
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

  bool ExpectGeicEnabled() const { return GetParam().expect_enabled; }

 private:
#if GEIC_SUPPORTS_CHANNEL_OVERRIDE
  std::optional<chrome::ScopedChannelOverride> channel_override_;
#endif
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(GeicButtonBrowserTest, IsGeicEnabled) {
  EXPECT_EQ(IsGeicEnabled(browser()->GetProfile()), ExpectGeicEnabled());
}

IN_PROC_BROWSER_TEST_P(GeicButtonBrowserTest, GeicButtonCreatedAndConfigured) {
  if (!ExpectGeicEnabled()) {
    return;
  }

  TabStripActionContainer* container =
      BrowserElementsViews::From(browser())->GetViewAs<TabStripActionContainer>(
          kTabStripActionContainerElementId);
  ASSERT_TRUE(container);

  GeicButton* button = container->GetGeicButtonForTesting();
  ASSERT_NE(container->GetGeicButtonForTesting(), nullptr);
  EXPECT_EQ(container->GetGlicButtonForTesting(), nullptr);

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
  if (ExpectGeicEnabled() || GetParam().glic_feature_enabled) {
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

IN_PROC_BROWSER_TEST_P(GeicButtonBrowserTest, GeicButtonTogglesSidePanel) {
  if (!ExpectGeicEnabled()) {
    return;
  }

  SidePanelUI* side_panel_ui = SidePanelUI::From(browser());
  ASSERT_TRUE(side_panel_ui);
  EXPECT_FALSE(side_panel_ui->IsSidePanelShowing());

  TabStripActionContainer* container =
      BrowserElementsViews::From(browser())->GetViewAs<TabStripActionContainer>(
          kTabStripActionContainerElementId);
  ASSERT_TRUE(container);
  GeicButton* button = container->GetGeicButtonForTesting();
  ASSERT_NE(button, nullptr);

  // First click opens the GEiC side panel.
  views::test::ButtonTestApi(button).NotifyClick(ui::test::TestEvent());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return side_panel_ui->IsSidePanelShowing(); }));
  EXPECT_TRUE(side_panel_ui->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kGeic)));

  SidePanel* side_panel =
      BrowserView::GetBrowserViewForBrowser(browser())->side_panel();
  ASSERT_TRUE(side_panel);
  EXPECT_EQ(side_panel->GetHeaderView<SidePanelHeader>(), nullptr);

  // Second click closes the side panel.
  views::test::ButtonTestApi(button).NotifyClick(ui::test::TestEvent());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !side_panel_ui->IsSidePanelShowing(); }));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GeicButtonBrowserTest,
    testing::Values(
        // All conditions met: GEiC switch and security switch enabled.
        GeicButtonTestParams{.geic_switch_enabled = true,
                             .disable_web_security_switch_enabled = true,
                             .glic_feature_enabled = false,
                             .expect_enabled = true},
        // Missing --geic-enabled switch.
        GeicButtonTestParams{.geic_switch_enabled = false,
                             .disable_web_security_switch_enabled = true,
                             .glic_feature_enabled = false,
                             .expect_enabled = false},
        // GLIC feature enabled (mutual exclusivity).
        GeicButtonTestParams{.geic_switch_enabled = true,
                             .disable_web_security_switch_enabled = true,
                             .glic_feature_enabled = true,
                             .expect_enabled = false}));

#if GEIC_SUPPORTS_CHANNEL_OVERRIDE
INSTANTIATE_TEST_SUITE_P(
    ChannelOverride,
    GeicButtonBrowserTest,
    testing::Values(
        // Branded Stable channel without --disable-web-security:
        // Enabled on non-official developer builds, disabled on official
        // builds.
        GeicButtonTestParams{
            .geic_switch_enabled = true,
            .disable_web_security_switch_enabled = false,
            .glic_feature_enabled = false,
            .channel_override = chrome::ScopedChannelOverride::Channel::kStable,
            .expect_enabled = !version_info::IsOfficialBuild()},
        // Branded Stable channel with --disable-web-security -> enabled.
        GeicButtonTestParams{
            .geic_switch_enabled = true,
            .disable_web_security_switch_enabled = true,
            .glic_feature_enabled = false,
            .channel_override = chrome::ScopedChannelOverride::Channel::kStable,
            .expect_enabled = true},
        // Branded Canary channel without --disable-web-security -> enabled.
        GeicButtonTestParams{
            .geic_switch_enabled = true,
            .disable_web_security_switch_enabled = false,
            .glic_feature_enabled = false,
            .channel_override = chrome::ScopedChannelOverride::Channel::kCanary,
            .expect_enabled = true}));
#else
INSTANTIATE_TEST_SUITE_P(
    Default,
    GeicButtonBrowserTest,
    testing::Values(
        // Unbranded/developer builds without --disable-web-security -> enabled.
        GeicButtonTestParams{.geic_switch_enabled = true,
                             .disable_web_security_switch_enabled = false,
                             .glic_feature_enabled = false,
                             .expect_enabled = true}));
#endif

}  // namespace geic

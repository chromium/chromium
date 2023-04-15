// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/new_tab_page/modules/modules_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabPageElementId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementReadyEvent);
}  // namespace

class NewTabPageModulesInteractiveUiTest : public InteractiveBrowserTest {
 public:
  NewTabPageModulesInteractiveUiTest() = default;
  ~NewTabPageModulesInteractiveUiTest() override = default;
  NewTabPageModulesInteractiveUiTest(
      const NewTabPageModulesInteractiveUiTest&) = delete;
  void operator=(const NewTabPageModulesInteractiveUiTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    // TODO(crbug.com/1430493): This test should include signing into an
    // account.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSignedOutNtpModulesSwitch);
    const unsigned long kNumVisits = 2;
    const unsigned long kNumVisitsWithImages = 2;
    features.InitWithFeaturesAndParameters(
        {
            {ntp_features::kNtpHistoryClustersModule,
             {{ntp_features::kNtpHistoryClustersModuleDataParam,
               base::StringPrintf("%lu,%lu", kNumVisits,
                                  kNumVisitsWithImages)}}},
        },
        {});
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList features;
};

IN_PROC_BROWSER_TEST_F(NewTabPageModulesInteractiveUiTest,
                       ClickingHistoryClustersTileNavigatesToCorrectPage) {
  const DeepQuery kMainHistoryClusterTileLink = {"ntp-app",
                                                 "ntp-modules",
                                                 "ntp-module-wrapper",
                                                 "ntp-history-clusters",
                                                 ".main-tile",
                                                 "a"};
  StateChange main_history_cluster_tile_loaded;
  main_history_cluster_tile_loaded.event = kElementReadyEvent;
  main_history_cluster_tile_loaded.type =
      StateChange::Type::kExistsAndConditionTrue;
  main_history_cluster_tile_loaded.where = kMainHistoryClusterTileLink;
  main_history_cluster_tile_loaded.test_function =
      "(el) => { return el.clientWidth > 0 && el.clientHeight > 0 && "
      "el.getAttribute('href') == "
      "'https://store.google.com/product/pixel_7?hl=en-US'; }";

  RunTestSequence(
      // 1. Wait for new tab page to load.
      InstrumentTab(kNewTabPageElementId, 0),
      NavigateWebContents(kNewTabPageElementId,
                          GURL(chrome::kChromeUINewTabPageURL)),
      WaitForWebContentsReady(kNewTabPageElementId,
                              GURL(chrome::kChromeUINewTabPageURL)),
      // 2. Wait for tile to load.
      WaitForStateChange(kNewTabPageElementId,
                         std::move(main_history_cluster_tile_loaded)),
      // 3. Scroll to tile. Modules may sometimes load below the fold.
      ScrollIntoView(kNewTabPageElementId, kMainHistoryClusterTileLink),
      // 4. Click the tile.
      MoveMouseTo(kNewTabPageElementId, kMainHistoryClusterTileLink),
      ClickMouse(),
      // 5. Verify that the tab navigates to the tile's link.
      WaitForWebContentsNavigation(
          kNewTabPageElementId,
          GURL("https://store.google.com/product/pixel_7?hl=en-US")));
}

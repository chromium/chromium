// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/new_tab_page/modules/modules_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabPageElementId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementReadyEvent);

const char kMainHistoryClusterTileLinkUrl[] =
    "https://store.google.com/product/pixel_7?hl=en-US";
const char kHistoryClustersCartTileLinkUrl[] = "https://store.google.com/cart";
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
    const unsigned long kNumClusters = 1;
    const unsigned long kNumVisits = 2;
    const unsigned long kNumVisitsWithImages = 2;
    features.InitWithFeaturesAndParameters(
        {
            {ntp_features::kNtpHistoryClustersModule,
             {{ntp_features::kNtpHistoryClustersModuleDataParam,
               base::StringPrintf("%lu,%lu,%lu", kNumClusters, kNumVisits,
                                  kNumVisitsWithImages)}}},
            {ntp_features::kNtpChromeCartInHistoryClusterModule,
             {{ntp_features::kNtpChromeCartInHistoryClustersModuleDataParam,
               "6"}}},
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

  InteractiveTestApi::MultiStep LoadNewTabPage() {
    return Steps(InstrumentTab(kNewTabPageElementId, 0),
                 NavigateWebContents(kNewTabPageElementId,
                                     GURL(chrome::kChromeUINewTabPageURL)),
                 WaitForWebContentsReady(kNewTabPageElementId,
                                         GURL(chrome::kChromeUINewTabPageURL)));
  }

  InteractiveTestApi::MultiStep WaitForLinkToLoad(const DeepQuery& link,
                                                  const char* url) {
    StateChange tile_loaded;
    tile_loaded.event = kElementReadyEvent;
    tile_loaded.type = StateChange::Type::kExistsAndConditionTrue;
    tile_loaded.where = link;
    tile_loaded.test_function = base::StrCat(
        {"(el) => { return el.clientWidth > 0 && el.clientHeight > "
         "0 && el.getAttribute('href') == '",
         url, "'; }"});
    return WaitForStateChange(kNewTabPageElementId, std::move(tile_loaded));
  }

  InteractiveTestApi::MultiStep ClickElement(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    return Steps(MoveMouseTo(contents_id, element), ClickMouse());
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
  RunTestSequence(
      // 1. Wait for new tab page to load.
      LoadNewTabPage(),
      // 2. Wait for tile to load.
      WaitForLinkToLoad(kMainHistoryClusterTileLink,
                        kMainHistoryClusterTileLinkUrl),
      // 3. Scroll to tile. Modules may sometimes load below the fold.
      ScrollIntoView(kNewTabPageElementId, kMainHistoryClusterTileLink),
      // 4. Click the tile.
      ClickElement(kNewTabPageElementId, kMainHistoryClusterTileLink),
      // 5. Verify that the tab navigates to the tile's link.
      WaitForWebContentsNavigation(kNewTabPageElementId,
                                   GURL(kMainHistoryClusterTileLinkUrl)));
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesInteractiveUiTest,
                       ClickingHistoryClustersCartTileNavigatesToCorrectPage) {
  const DeepQuery kHistoryClustersCartTileLink = {
      "ntp-app",
      "ntp-modules",
      "ntp-module-wrapper",
      "ntp-history-clusters",
      "ntp-history-clusters-cart-tile",
      "a"};

  RunTestSequence(
      // 1. Wait for new tab page to load.
      LoadNewTabPage(),
      // 2. Wait for tile to load.
      WaitForLinkToLoad(kHistoryClustersCartTileLink,
                        kHistoryClustersCartTileLinkUrl),
      // 3. Scroll to tile. Modules may sometimes load below the fold.
      ScrollIntoView(kNewTabPageElementId, kHistoryClustersCartTileLink),
      // 4. Click the tile.
      ClickElement(kNewTabPageElementId, kHistoryClustersCartTileLink),
      // 5. Verify that the tab navigates to the tile's link.
      WaitForWebContentsNavigation(kNewTabPageElementId,
                                   GURL(kHistoryClustersCartTileLinkUrl)));
}

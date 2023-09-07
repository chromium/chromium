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
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementChildrenReadyEvent);

const char kMainHistoryClusterTileLinkUrl[] =
    "https://store.google.com/product/pixel_7?hl=en-US";
const char kHistoryClustersCartTileLinkUrl[] = "https://store.google.com/cart";
const std::string kHistoryClusterSuggestTileLinkUrl =
    "https://www.google.com/search?q=new%20google%20products";
}  // namespace

class NewTabPageModulesInteractiveUiBaseTest : public InteractiveBrowserTest {
 public:
  NewTabPageModulesInteractiveUiBaseTest() = default;
  ~NewTabPageModulesInteractiveUiBaseTest() override = default;
  NewTabPageModulesInteractiveUiBaseTest(
      const NewTabPageModulesInteractiveUiBaseTest&) = delete;
  void operator=(const NewTabPageModulesInteractiveUiBaseTest&) = delete;

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

  InteractiveTestApi::MultiStep WaitForElementToLoad(const DeepQuery& element) {
    StateChange element_loaded;
    element_loaded.event = kElementReadyEvent;
    element_loaded.type = StateChange::Type::kExists;
    element_loaded.where = element;
    return WaitForStateChange(kNewTabPageElementId, std::move(element_loaded));
  }

  InteractiveTestApi::MultiStep WaitForElementChildElementCount(
      const DeepQuery& element,
      int count) {
    StateChange element_loaded;
    element_loaded.event = kElementChildrenReadyEvent;
    element_loaded.type = StateChange::Type::kExistsAndConditionTrue;
    element_loaded.where = element;
    element_loaded.test_function = base::StringPrintf(
        "(el) => { return el.childElementCount == %d; }", count);
    return WaitForStateChange(kNewTabPageElementId, std::move(element_loaded));
  }

  InteractiveTestApi::MultiStep WaitForElementStyleSet(
      const DeepQuery& element) {
    StateChange element_loaded;
    element_loaded.event = kElementReadyEvent;
    element_loaded.type = StateChange::Type::kExistsAndConditionTrue;
    element_loaded.where = element;
    element_loaded.test_function = "(el) => { return el.style.length > 0; }";
    return WaitForStateChange(kNewTabPageElementId, std::move(element_loaded));
  }

  InteractiveTestApi::MultiStep ClickElement(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    return Steps(MoveMouseTo(contents_id, element), ClickMouse());
  }

 protected:
  base::test::ScopedFeatureList features;
};

class NewTabPageModulesInteractiveUiTest
    : public NewTabPageModulesInteractiveUiBaseTest,
      public testing::WithParamInterface<bool> {
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

    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    enabled_features.push_back(
        {ntp_features::kNtpHistoryClustersModule,
         {{ntp_features::kNtpHistoryClustersModuleDataParam,
           base::StringPrintf("%lu,%lu,%lu", kNumClusters, kNumVisits,
                              kNumVisitsWithImages)}}});
    enabled_features.push_back(
        {ntp_features::kNtpChromeCartInHistoryClusterModule,
         {{ntp_features::kNtpChromeCartInHistoryClustersModuleDataParam,
           "6"}}});

    if (Redesigned()) {
      enabled_features.push_back({ntp_features::kNtpModulesRedesigned, {}});
    } else {
      disabled_features.push_back(ntp_features::kNtpModulesRedesigned);
    }

    features.InitWithFeaturesAndParameters(std::move(enabled_features),
                                           std::move(disabled_features));
    InteractiveBrowserTest::SetUp();
  }

  bool Redesigned() const { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(NewTabPageModulesInteractiveUiTest,
                       ClickingHistoryClustersTileNavigatesToCorrectPage) {
  const DeepQuery tileLink = {"ntp-app",
                              "ntp-modules",
                              "ntp-module-wrapper",
                              "ntp-history-clusters",
                              ".main-tile",
                              "a"};
  const DeepQuery tileLinkRedesigned = {"ntp-app",
                                        "ntp-modules-v2",
                                        "ntp-module-wrapper",
                                        "ntp-history-clusters-redesigned",
                                        "ntp-history-clusters-visit-tile",
                                        "a"};
  const DeepQuery kHistoryClusterVisitTileLink =
      Redesigned() ? tileLinkRedesigned : tileLink;

  RunTestSequence(
      // 1. Wait for new tab page to load.
      LoadNewTabPage(),
      // 2. Wait for tile to load.
      WaitForLinkToLoad(kHistoryClusterVisitTileLink,
                        kMainHistoryClusterTileLinkUrl),
      // 3. Scroll to tile. Modules may sometimes load below the fold.
      ScrollIntoView(kNewTabPageElementId, kHistoryClusterVisitTileLink),
      // 4. Click the tile.
      ClickElement(kNewTabPageElementId, kHistoryClusterVisitTileLink),
      // 5. Verify that the tab navigates to the tile's link.
      WaitForWebContentsNavigation(kNewTabPageElementId,
                                   GURL(kMainHistoryClusterTileLinkUrl)));
}

IN_PROC_BROWSER_TEST_P(
    NewTabPageModulesInteractiveUiTest,
    ClickingHistoryClustersRelatedSearchNavigatesToCorrectPage) {
  const DeepQuery relatedSearchLink = {"ntp-app",
                                       "ntp-modules",
                                       "ntp-module-wrapper",
                                       "ntp-history-clusters",
                                       "ntp-history-clusters-suggest-tile",
                                       "a"};
  const DeepQuery relatedSearchLinkRedesigned = {
      "ntp-app",
      "ntp-modules-v2",
      "ntp-module-wrapper",
      "ntp-history-clusters-redesigned",
      "ntp-history-clusters-suggest-tile-v2",
      "a"};
  const DeepQuery kHistoryClusterRelatedSearchLink =
      Redesigned() ? relatedSearchLinkRedesigned : relatedSearchLink;

  RunTestSequence(
      // 1. Wait for new tab page to load.
      LoadNewTabPage(),
      // 2. Wait for tile to load.
      WaitForLinkToLoad(kHistoryClusterRelatedSearchLink,
                        kHistoryClusterSuggestTileLinkUrl.c_str()),
      // 3. Scroll to tile. Modules may sometimes load below the fold.
      ScrollIntoView(kNewTabPageElementId, kHistoryClusterRelatedSearchLink),
      // 4. Click the tile.
      ClickElement(kNewTabPageElementId, kHistoryClusterRelatedSearchLink),
      // 5. Verify that the tab navigates to the tile's link.
      WaitForWebContentsNavigation(
          kNewTabPageElementId,
          GURL(kHistoryClusterSuggestTileLinkUrl.c_str())));
}

IN_PROC_BROWSER_TEST_P(NewTabPageModulesInteractiveUiTest,
                       ClickingHistoryClustersCartTileNavigatesToCorrectPage) {
  const DeepQuery tileLink = {"ntp-app",
                              "ntp-modules",
                              "ntp-module-wrapper",
                              "ntp-history-clusters",
                              "ntp-history-clusters-cart-tile",
                              "a"};

  const DeepQuery kHistoryClustersCartTileLinkRedesigned = {
      "ntp-app",
      "ntp-modules-v2",
      "ntp-module-wrapper",
      "ntp-history-clusters-redesigned",
      "ntp-history-clusters-cart-tile-v2",
      "a"};

  const DeepQuery kHistoryClustersCartTileLink =
      Redesigned() ? kHistoryClustersCartTileLinkRedesigned : tileLink;

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

INSTANTIATE_TEST_SUITE_P(All,
                         NewTabPageModulesInteractiveUiTest,
                         ::testing::Bool());

constexpr unsigned long kSampleNumClusters = 3;

class NewTabPageModulesRedesignedInteractiveUiTest
    : public NewTabPageModulesInteractiveUiBaseTest {
 public:
  NewTabPageModulesRedesignedInteractiveUiTest() = default;
  ~NewTabPageModulesRedesignedInteractiveUiTest() override = default;
  NewTabPageModulesRedesignedInteractiveUiTest(
      const NewTabPageModulesRedesignedInteractiveUiTest&) = delete;
  void operator=(const NewTabPageModulesRedesignedInteractiveUiTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSignedOutNtpModulesSwitch);

    const unsigned long kNumVisits = 2;
    const unsigned long kNumVisitsWithImages = 2;
    features.InitWithFeaturesAndParameters(
        {{ntp_features::kNtpHistoryClustersModule,
          {{ntp_features::kNtpHistoryClustersModuleDataParam,
            base::StringPrintf("%lu,%lu,%lu", kSampleNumClusters, kNumVisits,
                               kNumVisitsWithImages)}}},
         {ntp_features::kNtpChromeCartInHistoryClusterModule,
          {{ntp_features::kNtpChromeCartInHistoryClustersModuleDataParam,
            "6"}}},
         {ntp_features::kNtpModulesRedesigned, {}}},
        {});
    InteractiveBrowserTest::SetUp();
  }
};

// TODO(crbug.com/1470367): Enable the test on a compatible version skew.
// TODO(crbug.com/1472077): Flaky on Linux ChromiumOS MSan.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ClickingHideButtonDismissesCluster \
  DISABLED_ClickingHideButtonDismissesCluster
#else
#define MAYBE_ClickingHideButtonDismissesCluster \
  ClickingHideButtonDismissesCluster
#endif
IN_PROC_BROWSER_TEST_F(NewTabPageModulesRedesignedInteractiveUiTest,
                       MAYBE_ClickingHideButtonDismissesCluster) {
  const DeepQuery kModulesContainer = {"ntp-app", "ntp-modules-v2",
                                       "#container"};
  const DeepQuery kHistoryClustersMoreButton = {
      "ntp-app",
      "ntp-modules-v2",
      "ntp-module-wrapper",
      "ntp-history-clusters-redesigned",
      "history-clusters-header-v2",
      "ntp-module-header-v2",
      "#menuButton"};
  const DeepQuery kHistoryClustersMoreActionMenu = {
      "ntp-app",
      "ntp-modules-v2",
      "ntp-module-wrapper",
      "ntp-history-clusters-redesigned",
      "history-clusters-header-v2",
      "ntp-module-header-v2",
      "cr-action-menu",
      "dialog"};
  const DeepQuery kHistoryClustersHideButton = {
      "ntp-app",
      "ntp-modules-v2",
      "ntp-module-wrapper",
      "ntp-history-clusters-redesigned",
      "history-clusters-header-v2",
      "ntp-module-header-v2",
      "#dismiss"};

  RunTestSequence(
      // 1. Wait for new tab page to load.
      LoadNewTabPage(),
      // 2. Wait for modules container to have an expected child count matching
      // the test setup.
      WaitForElementChildElementCount(kModulesContainer, kSampleNumClusters),
      // 3. Ensure the NTP history clusters module "hide" button exists, and
      // thus, that the module loaded successfully.
      WaitForElementToLoad(kHistoryClustersHideButton),
      // 4. Scroll to the "hide" element of the NTP history clusters module.
      // Modules may sometimes load below the fold.
      ScrollIntoView(kNewTabPageElementId, kHistoryClustersHideButton),
      // 5. Click the "more actions" menu button of the NTP history clusters
      // module.
      ClickElement(kNewTabPageElementId, kHistoryClustersMoreButton),
      // 6. Wait for history clusters menu dialog to be anchored.
      WaitForElementStyleSet(kHistoryClustersMoreActionMenu),
      // 7. Click the "hide" element of the NTP history clusters module.
      ClickElement(kNewTabPageElementId, kHistoryClustersHideButton),
      // 8. Wait for modules container to reflect an updated element count that
      // resulted from dismissing a module.
      WaitForElementChildElementCount(kModulesContainer,
                                      kSampleNumClusters - 1));
}

// TODO(crbug.com/1469698): Flaky on Linux Tests (dbg).
#if BUILDFLAG(IS_CHROMEOS) || (BUILDFLAG(IS_LINUX) && !defined(NDEBUG))
#define MAYBE_ClickingDoneButtonDismissesCluster \
  DISABLED_ClickingDoneButtonDismissesCluster
#else
#define MAYBE_ClickingDoneButtonDismissesCluster \
  ClickingDoneButtonDismissesCluster
#endif
IN_PROC_BROWSER_TEST_F(NewTabPageModulesRedesignedInteractiveUiTest,
                       MAYBE_ClickingDoneButtonDismissesCluster) {
  const DeepQuery kModulesContainer = {"ntp-app", "ntp-modules-v2",
                                       "#container"};
  const DeepQuery kHistoryClustersMoreButton = {
      "ntp-app",
      "ntp-modules-v2",
      "ntp-module-wrapper",
      "ntp-history-clusters-redesigned",
      "history-clusters-header-v2",
      "ntp-module-header-v2",
      "#menuButton"};
  const DeepQuery kHistoryClustersMoreActionMenu = {
      "ntp-app",
      "ntp-modules-v2",
      "ntp-module-wrapper",
      "ntp-history-clusters-redesigned",
      "history-clusters-header-v2",
      "ntp-module-header-v2",
      "cr-action-menu",
      "dialog"};
  const DeepQuery kHistoryClustersDoneButton = {
      "ntp-app",
      "ntp-modules-v2",
      "ntp-module-wrapper",
      "ntp-history-clusters-redesigned",
      "history-clusters-header-v2",
      "ntp-module-header-v2",
      "#done"};

  RunTestSequence(
      // 1. Wait for new tab page to load.
      LoadNewTabPage(),
      // 2. Wait for modules container to have an expected child count matching
      // the test setup.
      WaitForElementChildElementCount(kModulesContainer, kSampleNumClusters),
      // 3. Ensure the NTP history clusters module "done" button exists, and
      // thus, that the module loaded successfully.
      WaitForElementToLoad(kHistoryClustersDoneButton),
      // 4. Scroll to the "done" element of the NTP history clusters module.
      // Modules may sometimes load below the fold.
      ScrollIntoView(kNewTabPageElementId, kHistoryClustersDoneButton),
      // 5. Click the "more actions" menu button of the NTP history clusters
      // module.
      ClickElement(kNewTabPageElementId, kHistoryClustersMoreButton),
      // 6. Wait for history clusters menu dialog to be anchored.
      WaitForElementStyleSet(kHistoryClustersMoreActionMenu),
      // 7. Click the "done" element of the NTP history clusters module.
      ClickElement(kNewTabPageElementId, kHistoryClustersDoneButton),
      // 8. Wait for modules container to reflect an updated element count that
      // resulted from dismissing a module.
      WaitForElementChildElementCount(kModulesContainer,
                                      kSampleNumClusters - 1));
}

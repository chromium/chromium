// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/new_tab_page/modules/modules_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabPageElementId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementReadyEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementChildrenReadyEvent);

const char kMainHistoryClusterTileLinkUrl[] =
    "https://store.google.com/product/pixel_7?hl=en-US";
const char kHistoryClustersCartTileLinkUrl[] = "https://store.google.com/cart";
const char kHistoryClusterSuggestTileLinkUrl[] =
    "https://www.google.com/search?q=new%20google%20products";
const char kGooglePageUrl[] = "https://www.google.com/";

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;
const DeepQuery kModulesV2Container = {"ntp-app", "ntp-modules-v2",
                                       "#container"};
const DeepQuery kModulesV2Wrapper = {"ntp-app", "ntp-modules-v2", "#container",
                                     "ntp-module-wrapper"};

struct ModuleDetails {
  const std::vector<base::test::FeatureRefAndParams> features;
  const DeepQuery module_query;
  const DeepQuery more_button_query;
  const DeepQuery more_action_menu_query;
  const DeepQuery dismiss_button_query;
  const DeepQuery disable_button_query;
  const DeepQuery tile_link_query;
  const char* tile_link;
};

struct HistoryClustersModuleDetails : public ModuleDetails {
  const bool redesigned;
  const DeepQuery history_clusters_related_search_link;
  const DeepQuery history_clusters_cart_tile_link;
  const DeepQuery history_clusters_done_button;
};

const unsigned long kNumClusters = 1;
const unsigned long kRedesignedNumClusters = 3;
const unsigned long kNumVisits = 2;
const unsigned long kNumVisitsWithImages = 2;

HistoryClustersModuleDetails CreateHistoryClustersModuleDetails(
    bool redesigned,
    const unsigned long num_clusters) {
  if (redesigned) {
    return {
        {
            {{ntp_features::kNtpHistoryClustersModule,
              {{ntp_features::kNtpHistoryClustersModuleDataParam,
                base::StringPrintf("%lu,%lu,%lu", num_clusters, kNumVisits,
                                   kNumVisitsWithImages)}}},
             {ntp_features::kNtpModulesRedesigned, {}}},
            {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
             "ntp-history-clusters-redesigned"},
            {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
             "ntp-history-clusters-redesigned", "history-clusters-header-v2",
             "ntp-module-header-v2", "#menuButton"},
            {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
             "ntp-history-clusters-redesigned", "history-clusters-header-v2",
             "ntp-module-header-v2", "cr-action-menu", "dialog"},
            {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
             "ntp-history-clusters-redesigned", "history-clusters-header-v2",
             "ntp-module-header-v2", "#dismiss"},
            {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
             "ntp-history-clusters-redesigned", "history-clusters-header-v2",
             "ntp-module-header-v2", "#disable"},
            {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
             "ntp-history-clusters-redesigned",
             "ntp-history-clusters-visit-tile", "a"},
            kMainHistoryClusterTileLinkUrl,
        },
        true,
        {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
         "ntp-history-clusters-redesigned",
         "ntp-history-clusters-suggest-tile-v2", "a"},
        {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
         "ntp-history-clusters-redesigned", "ntp-history-clusters-cart-tile-v2",
         "a"},
        {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
         "ntp-history-clusters-redesigned", "history-clusters-header-v2",
         "cr-icon-button"},
    };
  } else {
    return {
        {
            {{ntp_features::kNtpHistoryClustersModule,
              {{ntp_features::kNtpHistoryClustersModuleDataParam,
                base::StringPrintf("%lu,%lu,%lu", num_clusters, kNumVisits,
                                   kNumVisitsWithImages)}}}},
            {"ntp-app", "ntp-modules", "ntp-module-wrapper",
             "ntp-history-clusters"},
            {"ntp-app", "ntp-modules", "ntp-module-wrapper",
             "ntp-history-clusters", "ntp-module-header", "#menuButton"},
            {"ntp-app", "ntp-modules", "ntp-module-wrapper",
             "ntp-history-clusters", "ntp-module-header", "cr-action-menu",
             "dialog"},
            {"ntp-app", "ntp-modules", "ntp-module-wrapper",
             "ntp-history-clusters", "ntp-module-header", "#dismiss"},
            {"ntp-app", "ntp-modules", "ntp-module-wrapper",
             "ntp-history-clusters", "ntp-module-header", "#disable"},
            {"ntp-app", "ntp-modules", "ntp-module-wrapper",
             "ntp-history-clusters", ".main-tile", "a"},
            kMainHistoryClusterTileLinkUrl,
        },
        false,
        {"ntp-app", "ntp-modules", "ntp-module-wrapper", "ntp-history-clusters",
         "ntp-history-clusters-suggest-tile", "a"},
        {"ntp-app", "ntp-modules", "ntp-module-wrapper", "ntp-history-clusters",
         "ntp-history-clusters-cart-tile", "a"},
        {},
    };
  }
}

ModuleDetails kTabResumptionModuleDetails = {
    {{ntp_features::kNtpTabResumptionModule,
      {{ntp_features::kNtpTabResumptionModuleDataParam, "Fake Data"}}},
     {ntp_features::kNtpModulesRedesigned, {}}},
    {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper", "ntp-tab-resumption"},
    {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper", "ntp-tab-resumption",
     "ntp-module-header-v2", "#menuButton"},
    {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper", "ntp-tab-resumption",
     "ntp-module-header-v2", "cr-action-menu", "dialog"},
    {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper", "ntp-tab-resumption",
     "ntp-module-header-v2", "#dismiss"},
    {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper", "ntp-tab-resumption",
     "ntp-module-header-v2", "#disable"},
    {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper", "ntp-tab-resumption",
     "#tabs", "a"},
    kGooglePageUrl,
};

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

  InteractiveTestApi::MultiStep WaitForElementHiddenSet(
      const DeepQuery& element) {
    StateChange element_loaded;
    element_loaded.event = kElementReadyEvent;
    element_loaded.type = StateChange::Type::kExistsAndConditionTrue;
    element_loaded.where = element;
    element_loaded.test_function = "(el) => { return el.hidden; }";
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
      public testing::WithParamInterface<ModuleDetails> {
 public:
  NewTabPageModulesInteractiveUiTest() = default;
  ~NewTabPageModulesInteractiveUiTest() override = default;
  NewTabPageModulesInteractiveUiTest(
      const NewTabPageModulesInteractiveUiTest&) = delete;
  void operator=(const NewTabPageModulesInteractiveUiTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSignedOutNtpModulesSwitch);

    features.InitWithFeaturesAndParameters(ModuleDetails().features, {});
    InteractiveBrowserTest::SetUp();
  }

  ModuleDetails ModuleDetails() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    NewTabPageModulesInteractiveUiTest,
    ::testing::Values(kTabResumptionModuleDetails,
                      CreateHistoryClustersModuleDetails(true, kNumClusters)));
// TODO(crbug.com/335214502): Flaky on Linux/ChromeOS Tests.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ClickingHideButtonDismissesModule \
  DISABLED_ClickingHideButtonDismissesModule
#else
#define MAYBE_ClickingHideButtonDismissesModule \
  ClickingHideButtonDismissesModule
#endif
IN_PROC_BROWSER_TEST_P(NewTabPageModulesInteractiveUiTest,
                       MAYBE_ClickingHideButtonDismissesModule) {
  RunTestSequence(
      // 1. Wait for new tab page to load.
      LoadNewTabPage(),
      // 2. Wait for modules container to have an expected child count matching
      // the test setup.
      WaitForElementChildElementCount(kModulesV2Container, 1),
      // 3. Ensure the module's dismiss button exists, and thus, that the module
      // loaded successfully.
      WaitForElementToLoad(ModuleDetails().dismiss_button_query),
      // 4. Scroll to the dismiss element of the NTP module's header. Modules
      // may sometimes load below the fold.
      ScrollIntoView(kNewTabPageElementId,
                     ModuleDetails().dismiss_button_query),
      // 5. Click the "more actions" menu button of the NTP history clusters
      // module.
      ClickElement(kNewTabPageElementId, ModuleDetails().more_button_query),
      // 6. Wait for module's menu dialog to be anchored.
      WaitForElementStyleSet(ModuleDetails().more_action_menu_query),
      // 7. Click the dismiss action element of the NTP module.
      ClickElement(kNewTabPageElementId, ModuleDetails().dismiss_button_query),
      // 8. Wait for modules container to reflect an updated element count that
      // resulted from dismissing a module.
      WaitForElementChildElementCount(kModulesV2Container, 0));
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ClickingDisableButtonDisablesModule \
  DISABLED_ClickingDisableButtonDisablesModule
#else
#define MAYBE_ClickingDisableButtonDisablesModule \
  ClickingDisableButtonDisablesModule
#endif
IN_PROC_BROWSER_TEST_P(NewTabPageModulesInteractiveUiTest,
                       MAYBE_ClickingDisableButtonDisablesModule) {
  const auto& module_details = ModuleDetails();
  RunTestSequence(
      // 1. Wait for new tab page to load.
      LoadNewTabPage(),
      // 2. Wait for modules container to have an expected child count matching
      // the test setup.
      WaitForElementChildElementCount(kModulesV2Container, 1),
      // 3. Ensure the module's dismiss button exists, and thus, that the module
      // loaded successfully.
      WaitForElementToLoad(module_details.disable_button_query),
      // 4. Scroll to the dismiss element of the NTP module's header. Modules
      // may sometimes load below the fold.
      ScrollIntoView(kNewTabPageElementId, module_details.disable_button_query),
      // 5. Click the "more actions" menu button of the NTP history clusters
      // module.
      ClickElement(kNewTabPageElementId, module_details.more_button_query),
      // 6. Wait for module's menu dialog to be anchored.
      WaitForElementStyleSet(module_details.more_action_menu_query),
      // 7. Click the disable action element of the NTP module.
      ClickElement(kNewTabPageElementId, module_details.disable_button_query),
      // 8. Wait for modules container to reflect an updated element count that
      // resulted from disabling a module.
      WaitForElementHiddenSet(kModulesV2Wrapper));
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ClickingEntryNavigatesToCorrectPage \
  DISABLED_ClickingEntryNavigatesToCorrectPage
#else
#define MAYBE_ClickingEntryNavigatesToCorrectPage \
  ClickingEntryNavigatesToCorrectPage
#endif
IN_PROC_BROWSER_TEST_P(NewTabPageModulesInteractiveUiTest,
                       MAYBE_ClickingEntryNavigatesToCorrectPage) {
  RunTestSequence(
      // 1. Wait for new tab page to load.
      LoadNewTabPage(),
      // 2. Wait for modules container to have an expected child count matching
      // the test setup.
      WaitForElementChildElementCount(kModulesV2Container, 1),
      // 3. Wait for the Tab resumption module to load.
      WaitForElementToLoad(ModuleDetails().module_query),
      // 4. Wait for tile to load.
      WaitForLinkToLoad(ModuleDetails().tile_link_query,
                        ModuleDetails().tile_link),
      // 5. Scroll to tile. Modules may sometimes load below the fold.
      ScrollIntoView(kNewTabPageElementId, ModuleDetails().tile_link_query),
      // 6. Click the element link.
      ClickElement(kNewTabPageElementId, ModuleDetails().tile_link_query),
      // 7. Verify that the tab navigates to the tile's link.
      WaitForWebContentsNavigation(kNewTabPageElementId,
                                   GURL(ModuleDetails().tile_link)));
}

class NewTabPageModulesHistoryClustersInteractiveUiTest
    : public NewTabPageModulesInteractiveUiBaseTest,
      public testing::WithParamInterface<HistoryClustersModuleDetails> {
 public:
  NewTabPageModulesHistoryClustersInteractiveUiTest() = default;
  ~NewTabPageModulesHistoryClustersInteractiveUiTest() override = default;
  NewTabPageModulesHistoryClustersInteractiveUiTest(
      const NewTabPageModulesHistoryClustersInteractiveUiTest&) = delete;
  void operator=(const NewTabPageModulesHistoryClustersInteractiveUiTest&) =
      delete;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    // TODO(crbug.com/40901780): This test should include signing into an
    // account.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSignedOutNtpModulesSwitch);

    std::vector<base::test::FeatureRefAndParams> enabled_features =
        ModuleDetails().features;

    enabled_features.push_back(
        {ntp_features::kNtpChromeCartInHistoryClusterModule,
         {{ntp_features::kNtpChromeCartInHistoryClustersModuleDataParam,
           "6"}}});

    features.InitWithFeaturesAndParameters(std::move(enabled_features), {});
    InteractiveBrowserTest::SetUp();
  }

  HistoryClustersModuleDetails ModuleDetails() const { return GetParam(); }
  bool Redesigned() const { return ModuleDetails().redesigned; }
};

// TODO(crbug.com/335214502): Flaky on Linux/ChromeOS Tests.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_ClickingHistoryClustersRelatedSearchNavigatesToCorrectPage \
  DISABLED_ClickingHistoryClustersRelatedSearchNavigatesToCorrectPage
#else
#define MAYBE_ClickingHistoryClustersRelatedSearchNavigatesToCorrectPage \
  ClickingHistoryClustersRelatedSearchNavigatesToCorrectPage
#endif
IN_PROC_BROWSER_TEST_P(
    NewTabPageModulesHistoryClustersInteractiveUiTest,
    MAYBE_ClickingHistoryClustersRelatedSearchNavigatesToCorrectPage) {
  const DeepQuery kHistoryClusterRelatedSearchLink =
      ModuleDetails().history_clusters_related_search_link;

  RunTestSequence(
      // 1. Wait for new tab page to load.
      LoadNewTabPage(),
      // 2. Wait for tile to load.
      WaitForLinkToLoad(kHistoryClusterRelatedSearchLink,
                        kHistoryClusterSuggestTileLinkUrl),
      // 3. Scroll to tile. Modules may sometimes load below the fold.
      ScrollIntoView(kNewTabPageElementId, kHistoryClusterRelatedSearchLink),
      // 4. Click the tile.
      ClickElement(kNewTabPageElementId, kHistoryClusterRelatedSearchLink),
      // 5. Verify that the tab navigates to the tile's link.
      WaitForWebContentsNavigation(kNewTabPageElementId,
                                   GURL(kHistoryClusterSuggestTileLinkUrl)));
}

// TODO(crbug.com/335214502): Flaky on Linux/ChromeOS Tests.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_ClickingHistoryClustersCartTileNavigatesToCorrectPage \
  DISABLED_ClickingHistoryClustersCartTileNavigatesToCorrectPage
#else
#define MAYBE_ClickingHistoryClustersCartTileNavigatesToCorrectPage \
  ClickingHistoryClustersCartTileNavigatesToCorrectPage
#endif
IN_PROC_BROWSER_TEST_P(
    NewTabPageModulesHistoryClustersInteractiveUiTest,
    MAYBE_ClickingHistoryClustersCartTileNavigatesToCorrectPage) {
  const DeepQuery kHistoryClustersCartTileLink =
      ModuleDetails().history_clusters_cart_tile_link;

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

// TODO(crbug.com/40925463): Flaky on Linux Tests (dbg, MSan).
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_ClickingDoneButtonDismissesCluster \
  DISABLED_ClickingDoneButtonDismissesCluster
#else
#define MAYBE_ClickingDoneButtonDismissesCluster \
  ClickingDoneButtonDismissesCluster
#endif
IN_PROC_BROWSER_TEST_P(NewTabPageModulesHistoryClustersInteractiveUiTest,
                       MAYBE_ClickingDoneButtonDismissesCluster) {
  if (Redesigned()) {
    const DeepQuery kHistoryClustersDoneButton =
        ModuleDetails().history_clusters_done_button;
    RunTestSequence(
        // 1. Wait for new tab page to load.
        LoadNewTabPage(),
        // 2. Wait for modules container to have an expected child count
        // matching the test setup.
        WaitForElementChildElementCount(kModulesV2Container,
                                        kRedesignedNumClusters),
        // 3. Ensure the NTP history clusters module "done" button exists, and
        // thus, that the module loaded successfully.
        WaitForElementToLoad(kHistoryClustersDoneButton),
        // 4. Scroll to the "done" element of the NTP history clusters module.
        // Modules may sometimes load below the fold.
        ScrollIntoView(kNewTabPageElementId, kHistoryClustersDoneButton),
        // 5. Click the "done" element of the NTP history clusters module.
        ClickElement(kNewTabPageElementId, kHistoryClustersDoneButton),
        // 6. Wait for modules container to reflect an updated element count
        // that resulted from dismissing a module.
        WaitForElementChildElementCount(kModulesV2Container,
                                        kRedesignedNumClusters - 1));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    NewTabPageModulesHistoryClustersInteractiveUiTest,
    testing::Values(
        CreateHistoryClustersModuleDetails(false, kNumClusters),
        CreateHistoryClustersModuleDetails(true, kRedesignedNumClusters)));

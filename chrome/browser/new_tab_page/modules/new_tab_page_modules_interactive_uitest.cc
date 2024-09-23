// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/new_tab_page/modules/modules_switches.h"
#include "chrome/browser/new_tab_page/modules/test_support.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabPageElementId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementReadyEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementChildrenReadyEvent);

const char kGooglePageUrl[] = "https://www.google.com/";

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;
const DeepQuery kModulesV2Container = {"ntp-app", "ntp-modules-v2",
                                       "#container"};
const DeepQuery kModulesV2Wrapper = {"ntp-app", "ntp-modules-v2", "#container",
                                     "ntp-module-wrapper"};

struct ModuleLink {
  const DeepQuery query;
  const char* url;
};

struct ModuleDetails {
  const base::test::FeatureRef module_feature;
  const std::vector<base::test::FeatureRefAndParams> features;
  const DeepQuery module_query;
  const DeepQuery more_button_query;
  const DeepQuery more_action_menu_query;
  const DeepQuery dismiss_button_query;
  const DeepQuery disable_button_query;
  const std::vector<ModuleLink> links;
};

ModuleDetails kMostRelevantTabResumptionModuleDetails = {
    ntp_features::kNtpMostRelevantTabResumptionModule,
    {{ntp_features::kNtpMostRelevantTabResumptionModule,
      {{ntp_features::kNtpMostRelevantTabResumptionModuleDataParam,
        "Fake Data"}}},
     {ntp_features::kNtpModulesRedesigned, {}}},
    {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
     "ntp-most-relevant-tab-resumption"},
    {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
     "ntp-most-relevant-tab-resumption", "ntp-module-header-v2", "#menuButton"},
    {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
     "ntp-most-relevant-tab-resumption", "ntp-module-header-v2",
     "cr-action-menu", "dialog"},
    {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
     "ntp-most-relevant-tab-resumption", "ntp-module-header-v2", "#dismiss"},
    {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
     "ntp-most-relevant-tab-resumption", "ntp-module-header-v2", "#disable"},
    {{{"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
       "ntp-most-relevant-tab-resumption", "#urlVisits", "a"},
      kGooglePageUrl}},
};

ModuleDetails kGoogleCalendarModuleDetails = {
    ntp_features::kNtpCalendarModule,
    {{ntp_features::kNtpCalendarModule,
      {{ntp_features::kNtpCalendarModuleDataParam, "fake"}}},
     {ntp_features::kNtpModulesRedesigned, {}}},
    {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
     "ntp-google-calendar-module"},
    {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
     "ntp-google-calendar-module", "ntp-module-header-v2", "#menuButton"},
    {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
     "ntp-google-calendar-module", "ntp-module-header-v2", "cr-action-menu",
     "dialog"},
    {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
     "ntp-google-calendar-module", "ntp-module-header-v2", "#dismiss"},
    {"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
     "ntp-google-calendar-module", "ntp-module-header-v2", "#disable"},
    {{{"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
       "ntp-google-calendar-module", "ntp-calendar", "ntp-calendar-event",
       "#header"},
      "https://foo.com/0"},
     {{"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
       "ntp-google-calendar-module", "ntp-calendar", "#seeMore", "a"},
      "https://calendar.google.com"},
     {{"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
       "ntp-google-calendar-module", "ntp-calendar", "ntp-calendar-event",
       "cr-chip"},
      "https://foo.com/attachment0"},
     {{"ntp-app", "ntp-modules-v2", "ntp-module-wrapper",
       "ntp-google-calendar-module", "ntp-calendar", "ntp-calendar-event",
       "cr-button"},
      "https://foo.com/conference0"}},
};

std::vector<ModuleDetails> kAllModules = {
    kGoogleCalendarModuleDetails,
    kMostRelevantTabResumptionModuleDetails,
};

std::vector<std::pair<std::vector<base::test::FeatureRefAndParams>, ModuleLink>>
GetAllModuleLinks(std::vector<ModuleDetails> modules) {
  std::vector<
      std::pair<std::vector<base::test::FeatureRefAndParams>, ModuleLink>>
      result;
  for (const auto& module : modules) {
    for (const auto& link : module.links) {
      result.push_back(std::make_pair(module.features, link));
    }
  }
  return result;
}

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

  InteractiveTestApi::MultiStep WaitForLinkToLoad(const DeepQuery& link) {
    StateChange tile_loaded;
    tile_loaded.event = kElementReadyEvent;
    tile_loaded.type = StateChange::Type::kExistsAndConditionTrue;
    tile_loaded.where = link;
    tile_loaded.test_function = base::StrCat(
        {"(el) => { return el.clientWidth > 0 && el.clientHeight > 0; }"});
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

    features.InitWithFeaturesAndParameters(
        ModuleDetails().features,
        /*disabled_features=*/ntp::ComputeDisabledFeaturesList(
            ntp::kAllModuleFeatures, {ModuleDetails().module_feature}));
    InteractiveBrowserTest::SetUp();
  }

  ModuleDetails ModuleDetails() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         NewTabPageModulesInteractiveUiTest,
                         ::testing::ValuesIn(kAllModules));

// TODO(crbug.com/335214502): Re-enable this test.
IN_PROC_BROWSER_TEST_P(NewTabPageModulesInteractiveUiTest,
                       DISABLED_ClickingHideButtonDismissesModule) {
  RunTestSequence(
      // 1. Wait for new tab page to load.
      LoadNewTabPage(),
      // 2. Wait for modules container to have an expected child count matching
      // the test setup.
      WaitForElementChildElementCount(kModulesV2Container, 1),
      // 3. Ensure the module's "more" button exists, and thus, that the module
      // header loaded successfully.
      WaitForElementToLoad(ModuleDetails().more_button_query),
      // 4. Scroll to the "more"  button lement of the NTP module's header.
      // Modules may sometimes load below the fold.
      ScrollIntoView(kNewTabPageElementId, ModuleDetails().more_button_query),
      // 5. Click the "more actions" menu button of the NTP module.
      ClickElement(kNewTabPageElementId, ModuleDetails().more_button_query),
      // 6. Wait for module's menu dialog to be anchored.
      WaitForElementStyleSet(ModuleDetails().more_action_menu_query),
      // 7. Ensure the module's dismiss button exists, and thus, that the module
      // loaded successfully.
      WaitForElementToLoad(ModuleDetails().dismiss_button_query),
      // 8. Scroll to the dismiss element of the NTP module's header. Modules
      // may sometimes load below the fold.
      ScrollIntoView(kNewTabPageElementId,
                     ModuleDetails().dismiss_button_query),
      // 9. Click the dismiss action element of the NTP module.
      ClickElement(kNewTabPageElementId, ModuleDetails().dismiss_button_query),
      // 10. Wait for modules container to reflect an updated element count that
      // resulted from dismissing a module.
      WaitForElementChildElementCount(kModulesV2Container, 0));
}

// TODO(crbug.com/335214502): Flaky on ChromeOS Tests.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_CHROMEOS_LACROS)
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
      // 3. Ensure the module's "more" button exists, and thus, that the module
      // header loaded successfully.
      WaitForElementToLoad(ModuleDetails().more_button_query),
      // 4. Scroll to the "more"  button lement of the NTP module's header.
      // Modules may sometimes load below the fold.
      ScrollIntoView(kNewTabPageElementId, ModuleDetails().more_button_query),
      // 5. Click the "more actions" menu button of the NTP module.
      ClickElement(kNewTabPageElementId, module_details.more_button_query),
      // 6. Wait for module's menu dialog to be anchored.
      WaitForElementStyleSet(module_details.more_action_menu_query),
      // 7. Ensure the module's dismiss button exists, and thus, that the module
      // loaded successfully.
      WaitForElementToLoad(module_details.disable_button_query),
      // 8. Scroll to the disable element of the NTP module's header. Modules
      // may sometimes load below the fold.
      ScrollIntoView(kNewTabPageElementId,
                     ModuleDetails().disable_button_query),
      // 9. Click the disable action element of the NTP module.
      ClickElement(kNewTabPageElementId, module_details.disable_button_query),
      // 10. Wait for modules container to reflect an updated element count that
      // resulted from disabling a module.
      WaitForElementHiddenSet(kModulesV2Wrapper));
}

class NewTabPageModulesInteractiveLinkUiTest
    : public NewTabPageModulesInteractiveUiBaseTest,
      public testing::WithParamInterface<
          std::pair<std::vector<base::test::FeatureRefAndParams>, ModuleLink>> {
 public:
  NewTabPageModulesInteractiveLinkUiTest() = default;
  ~NewTabPageModulesInteractiveLinkUiTest() override = default;
  NewTabPageModulesInteractiveLinkUiTest(
      const NewTabPageModulesInteractiveLinkUiTest&) = delete;
  void operator=(const NewTabPageModulesInteractiveLinkUiTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSignedOutNtpModulesSwitch);

    features.InitWithFeaturesAndParameters(GetParam().first, {});
    InteractiveBrowserTest::SetUp();
  }

  ModuleLink ModuleLink() const { return GetParam().second; }
};

INSTANTIATE_TEST_SUITE_P(All,
                         NewTabPageModulesInteractiveLinkUiTest,
                         ::testing::ValuesIn(GetAllModuleLinks(kAllModules)));

// TODO(crbug.com/347914816): Fix test failure on Mac.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
#define MAYBE_ClickingEntryNavigatesToCorrectPage \
  DISABLED_ClickingEntryNavigatesToCorrectPage
#else
#define MAYBE_ClickingEntryNavigatesToCorrectPage \
  ClickingEntryNavigatesToCorrectPage
#endif
IN_PROC_BROWSER_TEST_P(NewTabPageModulesInteractiveLinkUiTest,
                       MAYBE_ClickingEntryNavigatesToCorrectPage) {
  RunTestSequence(
      // 1. Wait for new tab page to load.
      LoadNewTabPage(),
      // 2. Wait for modules container to have an expected child count matching
      // the test setup.
      WaitForElementChildElementCount(kModulesV2Container, 1),
      // 3. Wait for link to load.
      WaitForLinkToLoad(ModuleLink().query),
      // 4. Scroll to link. Modules may sometimes load below the fold.
      ScrollIntoView(kNewTabPageElementId, ModuleLink().query),
      // 5. Click the element link.
      ClickElement(kNewTabPageElementId, ModuleLink().query),
      // 6. Verify that the tab navigates to the tile's link.
      WaitForWebContentsNavigation(kNewTabPageElementId,
                                   GURL(ModuleLink().url)));
}

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
  const DeepQuery menu_button_query;
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
      "https://www.google.com"}},
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

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTest::SetUpCommandLine(command_line);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kSignedOutNtpModulesSwitch);
  }

  InteractiveTestApi::MultiStep LoadNewTabPage() {
    return Steps(InstrumentTab(kNewTabPageElementId, 0),
                 NavigateWebContents(kNewTabPageElementId,
                                     GURL(chrome::kChromeUINewTabPageURL)),
                 WaitForWebContentsReady(kNewTabPageElementId,
                                         GURL(chrome::kChromeUINewTabPageURL)));
  }

  InteractiveTestApi::MultiStep WaitForElementToRender(
      const WebContentsInteractionTestUtil::DeepQuery& element) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementRendered);
    WebContentsInteractionTestUtil::StateChange element_rendered;
    element_rendered.event = kElementRendered;
    element_rendered.where = element;
    element_rendered.test_function =
        "(el) => { if (el !== null) { let rect = el.getBoundingClientRect(); "
        "return rect.width > 0 && rect.height > 0; } return false; }";
    return WaitForStateChange(kNewTabPageElementId, element_rendered);
  }

  InteractiveTestApi::MultiStep WaitForElementChildElementCount(
      const DeepQuery& element,
      int count) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementChildrenReadyEvent);
    StateChange element_children_ready;
    element_children_ready.event = kElementChildrenReadyEvent;
    element_children_ready.type = StateChange::Type::kExistsAndConditionTrue;
    element_children_ready.where = element;
    element_children_ready.test_function = base::StringPrintf(
        "(el) => { return el.childElementCount == %d; }", count);
    return WaitForStateChange(kNewTabPageElementId,
                              std::move(element_children_ready));
  }

  InteractiveTestApi::MultiStep WaitForElementStyleSet(
      const DeepQuery& element) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementStyleSetEvent);
    StateChange element_style_set;
    element_style_set.event = kElementStyleSetEvent;
    element_style_set.type = StateChange::Type::kExistsAndConditionTrue;
    element_style_set.where = element;
    element_style_set.test_function = "(el) => { return el.style.length > 0; }";
    return WaitForStateChange(kNewTabPageElementId,
                              std::move(element_style_set));
  }

  InteractiveTestApi::MultiStep WaitForElementHiddenSet(
      const DeepQuery& element) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementHiddenEvent);
    StateChange element_hidden;
    element_hidden.event = kElementHiddenEvent;
    element_hidden.type = StateChange::Type::kExistsAndConditionTrue;
    element_hidden.where = element;
    element_hidden.test_function = "(el) => { return el.hidden; }";
    return WaitForStateChange(kNewTabPageElementId, std::move(element_hidden));
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

IN_PROC_BROWSER_TEST_P(NewTabPageModulesInteractiveUiTest,
                       ClickingHideButtonDismissesModule) {
  RunTestSequence(
      // 1. Wait for new tab page to load.
      LoadNewTabPage(),
      // 2. Wait for modules container to have an expected child count matching
      // the test setup.
      WaitForElementChildElementCount(kModulesV2Container, 1),
      // 3. Ensure the module's "more" button exists, and thus, that the module
      // header loaded successfully.
      WaitForElementToRender(ModuleDetails().menu_button_query),
      // 4. Scroll to the "more"  button lement of the NTP module's header.
      // Modules may sometimes load below the fold.
      ScrollIntoView(kNewTabPageElementId, ModuleDetails().menu_button_query),
      // 5. Click the "more actions" menu button of the NTP module.
      ClickElement(kNewTabPageElementId, ModuleDetails().menu_button_query),
      // 6. Wait for module's menu dialog to be anchored.
      WaitForElementStyleSet(ModuleDetails().more_action_menu_query),
      // 7. Ensure the module's dismiss button exists, and thus, that the module
      // loaded successfully.
      WaitForElementToRender(ModuleDetails().dismiss_button_query),
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

IN_PROC_BROWSER_TEST_P(NewTabPageModulesInteractiveUiTest,
                       ClickingDisableButtonDisablesModule) {
  const auto& module_details = ModuleDetails();
  RunTestSequence(
      // 1. Wait for new tab page to load.
      LoadNewTabPage(),
      // 2. Wait for modules container to have an expected child count matching
      // the test setup.
      WaitForElementChildElementCount(kModulesV2Container, 1),
      // 3. Ensure the module's "more" button exists, and thus, that the module
      // header loaded successfully.
      WaitForElementToRender(ModuleDetails().menu_button_query),
      // 4. Scroll to the "more"  button lement of the NTP module's header.
      // Modules may sometimes load below the fold.
      ScrollIntoView(kNewTabPageElementId, ModuleDetails().menu_button_query),
      // 5. Click the "more actions" menu button of the NTP module.
      ClickElement(kNewTabPageElementId, module_details.menu_button_query),
      // 6. Wait for module's menu dialog to be anchored.
      WaitForElementStyleSet(module_details.more_action_menu_query),
      // 7. Ensure the module's dismiss button exists, and thus, that the module
      // loaded successfully.
      WaitForElementToRender(module_details.disable_button_query),
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
    features.InitWithFeaturesAndParameters(GetParam().first, {});
    InteractiveBrowserTest::SetUp();
  }

  ModuleLink ModuleLink() const { return GetParam().second; }
};

// TODO(crbug.com/347914816): Fix test failure for
// kMostRelevantTabResumptionModuleDetails.
#if BUILDFLAG(IS_MAC)
INSTANTIATE_TEST_SUITE_P(
    All,
    NewTabPageModulesInteractiveLinkUiTest,
    ::testing::ValuesIn(GetAllModuleLinks({kGoogleCalendarModuleDetails})));
#else
INSTANTIATE_TEST_SUITE_P(All,
                         NewTabPageModulesInteractiveLinkUiTest,
                         ::testing::ValuesIn(GetAllModuleLinks(kAllModules)));
#endif

IN_PROC_BROWSER_TEST_P(NewTabPageModulesInteractiveLinkUiTest,
                       ClickingEntryNavigatesToCorrectPage) {
  RunTestSequence(
      // 1. Wait for new tab page to load.
      LoadNewTabPage(),
      // 2. Wait for modules container to have an expected child count matching
      // the test setup.
      WaitForElementChildElementCount(kModulesV2Container, 1),
      // 3. Wait for link to load.
      WaitForElementToRender(ModuleLink().query),
      // 4. Scroll to link. Modules may sometimes load below the fold.
      ScrollIntoView(kNewTabPageElementId, ModuleLink().query),
      // 5. Click the element link.
      ClickElement(kNewTabPageElementId, ModuleLink().query),
      // 6. Verify that the tab navigates to the tile's link.
      WaitForWebContentsNavigation(kNewTabPageElementId,
                                   GURL(ModuleLink().url)));
}

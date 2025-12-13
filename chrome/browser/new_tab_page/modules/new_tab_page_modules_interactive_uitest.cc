// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/new_tab_page/modules/modules_switches.h"
#include "chrome/browser/new_tab_page/modules/test_support.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/interactive_test.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabPageElementId);

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;
const DeepQuery kModulesV2Container = {"ntp-app", "ntp-modules", "#container"};
const DeepQuery kModulesV2Wrapper = {"ntp-app", "ntp-modules", "#container",
                                     "ntp-module-wrapper"};
const DeepQuery kMicrosoftAuthIframe = {"ntp-app", "#microsoftAuth"};
const DeepQuery kTabGroupsModule = {"ntp-app", "ntp-modules", "ntp-tab-groups"};
const DeepQuery kTabGroupsModuleContainer = {
    "ntp-app", "ntp-modules", "ntp-tab-groups", "#tabGroupsContainer"};
const DeepQuery kCreateNewTabGroup = {
    "ntp-app", "ntp-modules", "ntp-tab-groups", ".create-new-tab-group"};
const DeepQuery kFirstTabGroup = {"ntp-app", "ntp-modules", "ntp-tab-groups",
                                  ".tab-group:nth-of-type(1)"};

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
        "Fake Data"}}}},
    {"ntp-app", "ntp-modules", "ntp-module-wrapper",
     "ntp-most-relevant-tab-resumption"},
    {"ntp-app", "ntp-modules", "ntp-module-wrapper",
     "ntp-most-relevant-tab-resumption", "ntp-module-header-v2", "#menuButton"},
    {"ntp-app", "ntp-modules", "ntp-module-wrapper",
     "ntp-most-relevant-tab-resumption", "ntp-module-header-v2",
     "cr-action-menu", "dialog"},
    {"ntp-app", "ntp-modules", "ntp-module-wrapper",
     "ntp-most-relevant-tab-resumption", "ntp-module-header-v2", "#dismiss"},
    {"ntp-app", "ntp-modules", "ntp-module-wrapper",
     "ntp-most-relevant-tab-resumption", "ntp-module-header-v2", "#disable"},
    {{{"ntp-app", "ntp-modules", "ntp-module-wrapper",
       "ntp-most-relevant-tab-resumption", "#urlVisits", "a"},
      "https://www.google.com"}},
};

ModuleDetails kGoogleCalendarModuleDetails = {
    ntp_features::kNtpCalendarModule,
    {{ntp_features::kNtpCalendarModule,
      {{ntp_features::kNtpCalendarModuleDataParam, "fake"}}}},
    {"ntp-app", "ntp-modules", "ntp-module-wrapper",
     "ntp-google-calendar-module"},
    {"ntp-app", "ntp-modules", "ntp-module-wrapper",
     "ntp-google-calendar-module", "ntp-module-header-v2", "#menuButton"},
    {"ntp-app", "ntp-modules", "ntp-module-wrapper",
     "ntp-google-calendar-module", "ntp-module-header-v2", "cr-action-menu",
     "dialog"},
    {"ntp-app", "ntp-modules", "ntp-module-wrapper",
     "ntp-google-calendar-module", "ntp-module-header-v2", "#dismiss"},
    {"ntp-app", "ntp-modules", "ntp-module-wrapper",
     "ntp-google-calendar-module", "ntp-module-header-v2", "#disable"},
    {{{"ntp-app", "ntp-modules", "ntp-module-wrapper",
       "ntp-google-calendar-module", "ntp-calendar", "ntp-calendar-event",
       "#header"},
      "https://foo.com/0"},
     {{"ntp-app", "ntp-modules", "ntp-module-wrapper",
       "ntp-google-calendar-module", "ntp-calendar", "#seeMore", "a"},
      "https://calendar.google.com"},
     {{"ntp-app", "ntp-modules", "ntp-module-wrapper",
       "ntp-google-calendar-module", "ntp-calendar", "ntp-calendar-event",
       "cr-chip"},
      "https://foo.com/attachment0"},
     {{"ntp-app", "ntp-modules", "ntp-module-wrapper",
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

  InteractiveTestApi::MultiStep WaitForElementToLoad(
      const WebContentsInteractionTestUtil::DeepQuery& element) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kElementLoaded);
    WebContentsInteractionTestUtil::StateChange element_loaded;
    element_loaded.event = kElementLoaded;
    element_loaded.where = element;
    element_loaded.test_function = "(el) => { return el !== null; }";
    return WaitForStateChange(kNewTabPageElementId, element_loaded);
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

// TODO(crbug.com/416206296): Re-enable once we have a workaround for querying
// the `module_wrapper.html` slotted element.
// @see chrome/browser/resources/new_tab_page/modules/module_wrapper.html
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

// TODO(crbug.com/416206296): Re-enable once we have a workaround for querying
// the `module_wrapper.html` slotted element.
// @see chrome/browser/resources/new_tab_page/modules/module_wrapper.html
IN_PROC_BROWSER_TEST_P(NewTabPageModulesInteractiveUiTest,
                       DISABLED_ClickingDisableButtonDisablesModule) {
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

// TODO(crbug.com/416206296): Re-enable once we have a workaround for querying
// the `module_wrapper.html` slotted element.
// @see chrome/browser/resources/new_tab_page/modules/module_wrapper.html
IN_PROC_BROWSER_TEST_P(NewTabPageModulesInteractiveLinkUiTest,
                       DISABLED_ClickingEntryNavigatesToCorrectPage) {
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

class NewTabPageModulesInteractiveMicrosoftAuthUiTest
    : public NewTabPageModulesInteractiveUiBaseTest {
 public:
  NewTabPageModulesInteractiveMicrosoftAuthUiTest() = default;
  ~NewTabPageModulesInteractiveMicrosoftAuthUiTest() override = default;
  NewTabPageModulesInteractiveMicrosoftAuthUiTest(
      const NewTabPageModulesInteractiveMicrosoftAuthUiTest&) = delete;
  void operator=(const NewTabPageModulesInteractiveMicrosoftAuthUiTest&) =
      delete;

  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    features.InitWithFeatures({ntp_features::kNtpMicrosoftAuthenticationModule,
                               ntp_features::kNtpSharepointModule},
                              {});
    InteractiveBrowserTest::SetUp();
  }

  policy::MockConfigurationPolicyProvider& policy_provider() {
    return policy_provider_;
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(NewTabPageModulesInteractiveMicrosoftAuthUiTest,
                       LoadMicrosoftAuthIframe) {
  policy::PolicyMap policies;
  policies.Set(policy::key::kNTPSharepointCardVisible,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);
  policy_provider().UpdateChromePolicy(policies);
  RunTestSequence(
      // 1. Wait for new tab page to load.
      LoadNewTabPage(),
      // 2. Wait for iframe to load.
      WaitForElementToLoad(kMicrosoftAuthIframe));
}

class NewTabPageModulesInteractiveTabGroupsUiTest
    : public TabStripInteractiveTestMixin<
          NewTabPageModulesInteractiveUiBaseTest> {
 public:
  NewTabPageModulesInteractiveTabGroupsUiTest() = default;
  ~NewTabPageModulesInteractiveTabGroupsUiTest() override = default;
  NewTabPageModulesInteractiveTabGroupsUiTest(
      const NewTabPageModulesInteractiveTabGroupsUiTest&) = delete;
  void operator=(const NewTabPageModulesInteractiveTabGroupsUiTest&) = delete;

  void SetUp() override {
    features.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kNtpTabGroupsModule,
                              ntp_features::kNtpTabGroupsModuleZeroState},
        /*disabled_features=*/{});
    InteractiveBrowserTest::SetUp();
  }

  InteractiveTestApi::MultiStep ClickElement(
      const ui::ElementIdentifier& contents_id,
      const DeepQuery& element) {
    return Steps(EnsurePresent(contents_id, element),
                 ScrollIntoView(contents_id, element),
                 ExecuteJsAt(contents_id, element, "el => el.click()"));
  }

  InteractiveTestApi::MultiStep OpenTabGroupEditorMenu(
      tab_groups::TabGroupId group_id) {
    return Steps(HoverTabGroupHeader(group_id), ClickMouse(ui_controls::RIGHT),
                 WaitForShow(kTabGroupEditorBubbleId));
  }

  size_t GetTabGroupCount(Browser* browser) {
    return browser->tab_strip_model()->group_model()->ListTabGroups().size();
  }

  size_t GetTabCount(Browser* browser) {
    return browser->tab_strip_model()->count();
  }
};

IN_PROC_BROWSER_TEST_F(NewTabPageModulesInteractiveTabGroupsUiTest,
                       CreateNewTabGroup) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);

  RunTestSequence(
      // Arrange.
      // 1. Create tabs to work with.
      AddInstrumentedTab(kSecondTab, GURL(url::kAboutBlankURL)),
      // 2. Create 2 tab groups using the existing tabs.
      Do([&]() {
        browser()->tab_strip_model()->AddToNewGroup({0});
        browser()->tab_strip_model()->AddToNewGroup({1});
      }),
      // 3. Verify the initial tab group count is 2.
      CheckResult([&]() { return GetTabGroupCount(browser()); }, 2),

      // Act.
      // 4. Wait for new tab page to load.
      LoadNewTabPage(),
      // 5. Wait for the module to render.
      WaitForElementToRender(kTabGroupsModule),
      // 6. Click the create new tab group link.
      ClickElement(kNewTabPageElementId, kCreateNewTabGroup),

      // Assert.
      // 7. Verify a new tab group has been created.
      CheckResult([&]() { return GetTabGroupCount(browser()); }, 3));
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesInteractiveTabGroupsUiTest,
                       CreateNewTabGroup_ZeroState) {
  RunTestSequence(
      // Arrange.
      // 1. Verify the initial tab group count is 0.
      CheckResult([&]() { return GetTabGroupCount(browser()); }, 0),

      // Act.
      // 2. Wait for new tab page to load.
      LoadNewTabPage(),
      // 3. Wait for the module to render.
      WaitForElementToRender(kTabGroupsModule),
      // 4. Click the create new tab group link.
      ClickElement(kNewTabPageElementId, kCreateNewTabGroup),

      // Assert.
      // 5. Verify a new tab group has been created.
      CheckResult([&]() { return GetTabGroupCount(browser()); }, 1));
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesInteractiveTabGroupsUiTest,
                       ResumeTabGroupInCurrentWindow) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      // Verify current widow: 1 tab group, 2 tabs.
      CheckResult([&]() { return GetTabGroupCount(browser()); }, 1),
      CheckResult([&]() { return GetTabCount(browser()); }, 2),
      // Open tab group editor bubble.
      OpenTabGroupEditorMenu(group_id),
      // Close tab group.
      Steps(EnsurePresent(kTabGroupEditorBubbleCloseGroupButtonId),
            MoveMouseTo(kTabGroupEditorBubbleCloseGroupButtonId), ClickMouse(),
            WaitForHide(kTabGroupEditorBubbleId)),
      // Verify current window: 0 tab groups, 1 tab.
      CheckResult([&]() { return GetTabGroupCount(browser()); }, 0),
      CheckResult([&]() { return GetTabCount(browser()); }, 1),
      // Load the New Tab Page.
      LoadNewTabPage(),
      // Wait for the module to render.
      WaitForElementToRender(kTabGroupsModule),
      // Click the first tab group in the module to re-open it.
      ClickElement(kNewTabPageElementId, kFirstTabGroup),
      // Verify the tab group header is visible.
      WaitForShow(kTabGroupHeaderElementId),
      // Verify current window: 1 tab group, 2 tabs.
      CheckResult([&] { return GetTabGroupCount(browser()); }, 1),
      CheckResult([&]() { return GetTabCount(browser()); }, 2));
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesInteractiveTabGroupsUiTest,
                       ResumeTabGroupInAnotherWindow) {
  ASSERT_TRUE(
      AddTabAtIndex(0, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_TYPED));
  const tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0});

  RunTestSequence(
      // Ensure browser() is active.
      Do([&]() {
        ASSERT_EQ(GetLastActiveBrowserWindowInterfaceWithAnyProfile(),
                  browser());
      }),
      // Current widow: 1 tab group, 2 tabs.
      CheckResult([&]() { return GetTabGroupCount(browser()); }, 1),
      CheckResult([&]() { return GetTabCount(browser()); }, 2),
      // Open tab group editor bubble.
      OpenTabGroupEditorMenu(group_id),
      // Move tab group to new window.
      Steps(EnsurePresent(kTabGroupEditorBubbleMoveGroupToNewWindowButtonId),
            MoveMouseTo(kTabGroupEditorBubbleMoveGroupToNewWindowButtonId),
            ClickMouse(), WaitForHide(kTabGroupEditorBubbleId)),
      // Verify current window: 0 tab groups, 1 tab.
      CheckResult([&]() { return GetTabGroupCount(browser()); }, 0),
      CheckResult([&]() { return GetTabCount(browser()); }, 1),
      // Verify browser() is not active.
      Do([&]() {
        ASSERT_NE(GetLastActiveBrowserWindowInterfaceWithAnyProfile(),
                  browser());
      }),
      // Load the New Tab Page in browser().
      LoadNewTabPage(),
      // Wait for the module to render.
      WaitForElementToRender(kTabGroupsModule),
      // Click the first tab group in the module to re-open it.
      ClickElement(kNewTabPageElementId, kFirstTabGroup),
      // Verify current window stays unchanged: 0 tab groups, 1 tab.
      CheckResult([&]() { return GetTabGroupCount(browser()); }, 0),
      CheckResult([&]() { return GetTabCount(browser()); }, 1),
      // Verify browser() is not active.
      Do([&]() {
        ASSERT_NE(GetLastActiveBrowserWindowInterfaceWithAnyProfile(),
                  browser());
      }));
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesInteractiveTabGroupsUiTest,
                       FilterActiveTabGroup) {
  RunTestSequence(
      // Set up two tab groups:
      // Group 1 contains one tab at index 0.
      // Group 2 contains two tabs at indices 1 and 2.
      Do([&]() {
        browser()->tab_strip_model()->AddToNewGroup({0});
        ASSERT_TRUE(AddTabAtIndex(1, GURL(url::kAboutBlankURL),
                                  ui::PAGE_TRANSITION_TYPED));
        ASSERT_TRUE(AddTabAtIndex(2, GURL(url::kAboutBlankURL),
                                  ui::PAGE_TRANSITION_TYPED));
        browser()->tab_strip_model()->AddToNewGroup({1, 2});
      }),

      // Verify that there are two tab groups and three tabs total.
      CheckResult([&]() { return GetTabGroupCount(browser()); }, 2),
      CheckResult([&]() { return GetTabCount(browser()); }, 3),

      // Activate the tab in group 1 and navigate to chrome://newtab/.
      Do([&]() { browser()->tab_strip_model()->ActivateTabAt(0); }),
      LoadNewTabPage(), WaitForElementToRender(kTabGroupsModule),

      // Verify that only one tab group is shown (i.e., the group that doesn't
      // include the currently active tab).
      CheckJsResultAt(kNewTabPageElementId, kTabGroupsModuleContainer,
                      "el => el.querySelectorAll('.tab-group').length", 1),
      CheckJsResultAt(kNewTabPageElementId, kFirstTabGroup,
                      "el => "
                      "el.querySelector('.tab-group-title').textContent.trim()."
                      "toLowerCase()",
                      "2 tabs"));
}

IN_PROC_BROWSER_TEST_F(NewTabPageModulesInteractiveTabGroupsUiTest,
                       FilterActiveTabGroup_ModuleNotShown) {
  RunTestSequence(
      // Set up a tab group containing one tab at index 0.
      Do([&]() { browser()->tab_strip_model()->AddToNewGroup({0}); }),

      // Verify that there is 1 tab group and 1 tab total.
      CheckResult([&]() { return GetTabGroupCount(browser()); }, 1),
      CheckResult([&]() { return GetTabCount(browser()); }, 1),

      // Navigate the current tab to chrome://newtab/.
      LoadNewTabPage(),

      // Verify that the tab groups module is not visible.
      EnsureNotPresent(kNewTabPageElementId, kTabGroupsModule));
}

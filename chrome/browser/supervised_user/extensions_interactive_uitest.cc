// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <tuple>

#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/supervised_user/family_live_test.h"
#include "chrome/test/supervised_user/family_member.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"

namespace supervised_user {
namespace {

static constexpr std::string_view kChromeManageExternsionsUrl =
    "chrome://extensions/";

// State of a Family Link switch.
enum class FamilyLinkSwitchState : int {
  kEnabled = 0,
  kDisabled,
};

// Family Link switch that governs the handling of extensions for SU.
enum class ExtensionHandlingMode : int {
  kExtensionsGovernedByPermissionsSwitch = 0,
  kExtensionsGovernedByExtensionsSwitch,
};

// Family Link swiches from Advance Settings.
enum class FamilyLinkSwitch : int {
  kPermissionsSwitch = 0,
  kExtensionsSwitch,
};

constexpr const char* kFamilyLinkSwitchStateToString[2] = {"true", "false"};

using BoolPreferenceStateObserver = ui::test::PollingStateObserver<bool>;

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BoolPreferenceStateObserver,
                                    kBoolPermissionsPreferenceObserver);
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BoolPreferenceStateObserver,
                                    kBoolExtensionsPreferenceObserver);

// TODO(b/321242366): Consider moving to helper class.
// Checks if a page title matches the given regexp in ecma script dialect.
InteractiveBrowserTestApi::StateChange PageWithMatchingTitle(
    std::string_view title_regexp) {
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kStateChange);
  InteractiveBrowserTestApi::StateChange state_change;
  state_change.type =
      InteractiveBrowserTestApi::StateChange::Type::kConditionTrue;
  state_change.event = kStateChange;
  state_change.test_function = base::StringPrintf(R"js(
    () => /%s/.test(document.title)
  )js",
                                                  title_regexp.data());
  state_change.continue_across_navigation = true;
  return state_change;
}

// Test the behavior of handling extensions for supervised users when parental
// controls apply on extensions (by default on Chrome OS, depending on the
// kEnableExtensionsPermissionsForSupervisedUsersOnDesktop feature on
// Win/Mac/Linux).
class SupervisedUserExtensionsParentalControlsUiTest
    : public InteractiveBrowserTestT<FamilyLiveTest>,
      public testing::WithParamInterface<std::tuple<
          FamilyIdentifier,
          /*permissions_switch_state=*/FamilyLinkSwitchState,
          /*extensions_switch_state=*/FamilyLinkSwitchState,
          // Depending on the ExtensionHandlingMode only one switch
          // should affect the behaviour of supervised user's extensions.
          // Toggling the other switch should have no effect to the result.
          /*extensions_handling_mode=*/ExtensionHandlingMode>> {
 public:
  SupervisedUserExtensionsParentalControlsUiTest()
      : InteractiveBrowserTestT<FamilyLiveTest>(
            /*family_identifier=*/std::get<0>(GetParam()),
            /*extra_enabled_hosts=*/std::vector<std::string>()) {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (GetExtensionHandlingMode() ==
        ExtensionHandlingMode::kExtensionsGovernedByExtensionsSwitch) {
      enabled_features.push_back(
              kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
    } else {
      disabled_features.push_back(
              kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
    }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    // Enable extensions parental controls.
    enabled_features.push_back(
            kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
#endif
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 protected:
  // Parent navigates to FL control page and waits for it to load.
  auto ParentOpensControlPage(ui::ElementIdentifier kParentTab,
                              const GURL& gurl) {
    return Steps(NavigateWebContents(kParentTab, gurl),
                 WaitForWebContentsReady(kParentTab, gurl));
  }

  // Child tries to enable a disabled extension (which is pending parent
  // approval) by clicking at the extension's toggle.
  auto ChildClicksEnableExtensionIfExtensionDisbaled(
      ui::ElementIdentifier kChildTab,
      bool expected_extension_enabled) {
    return Steps(ExecuteJs(kChildTab,
                           base::StringPrintf(
                               R"js(
                () => {
                  const view_manager =
                    document.querySelector("extensions-manager").shadowRoot
                      .querySelector("#container").querySelector("#viewManager");
                  if (!view_manager) {
                    throw Error("Path to view_manager element is invalid.");
                  }
                  const container = view_manager.querySelector("#items-list")
                    .shadowRoot.querySelector("#container");
                  if (!container) {
                    throw Error("Path to container element is invalid.");
                  }
                  const count = container.querySelectorAll("extensions-item").length;
                  if (count !== 1) {
                    throw Error("Encountered unexpected number of extensions: " + count);
                  }
                  const extn = container.querySelectorAll("extensions-item")[0];
                  if (!extn) {
                    throw Error("Path to extension element is invalid.");
                  }
                  const toggle = extn.shadowRoot.querySelector("#enableToggle");
                  if (!toggle) {
                    throw Error("Path to extension toggle is invalid.");
                  }
                  if (toggle.ariaPressed !== "%s") {
                    throw Error("Extension toggle in unexpected state: " + toggle.ariaPressed);
                  }
                  if (toggle.ariaPressed == "false") {
                    toggle.click();
                  }
                }
              )js",
                               expected_extension_enabled ? "true" : "false")),
                 Log("Child inspected extension toggle."));
  }

  // Installs programmatically (not through the UI) an extension for the given
  // user.
  void InstallExtension(const std::string_view& name, Profile* profile) {
    extensions::TestExtensionDir extension_dir;
    extension_dir.WriteManifest(base::StringPrintf(
        R"({
            "name": "%s",
            "manifest_version": 3,
            "version": "0.1"
          })",
        name.data()));

    extensions::ChromeTestExtensionLoader extension_loader(profile);
    extension_loader.set_ignore_manifest_warnings(true);
    extension_loader.LoadExtension(extension_dir.Pack());
  }

  // Parent toggles the "Permissions" switch in FL, if it does not have
  // the desired value.
  auto ParentSetsPermissionsSwitch(ui::ElementIdentifier kParentTab,
                                   FamilyLinkSwitchState target_state) {
    return Steps(ExecuteJs(
        kParentTab,
        base::StringPrintf(
            R"js(
          () => {
            const button = document.querySelector('[aria-label="Toggle permissions for sites, apps and extensions"]')
            if (!button) {
              throw Error("'Permissions' toggle not found.");
            }
            if (button.ariaChecked != "%s") {
              button.click();
            }
          }
        )js",
            kFamilyLinkSwitchStateToString[static_cast<int>(target_state)])));
  }

  // Polls the given preference value in Chrome.
  auto PollPreference(ui::test::StateIdentifier<BoolPreferenceStateObserver>
                          preference_observer,
                      std::string_view preference) {
    return Steps(PollState(preference_observer, [this, preference]() {
      return child().browser()->profile()->GetPrefs()->GetBoolean(preference);
    }));
  }

  FamilyLinkSwitch GetExtensionGoverningSwitch() {
    return GetExtensionHandlingMode() ==
                   ExtensionHandlingMode::kExtensionsGovernedByPermissionsSwitch
               ? FamilyLinkSwitch::kPermissionsSwitch
               : FamilyLinkSwitch::kExtensionsSwitch;
  }

  auto ParentsSetsFamilyLinkSwitchAndWaitsForChromeToReceiveIt(
      FamilyLinkSwitch family_link_switch,
      FamilyLinkSwitchState switch_state,
      ui::ElementIdentifier kParentControlsTab) {
    bool pref_target_value =
        switch_state == FamilyLinkSwitchState::kEnabled ? true : false;

    if (family_link_switch == FamilyLinkSwitch::kPermissionsSwitch) {
      // Parent sets the FL switch "Permissions" to ON.
      // TODO(b/303401498): Use chrome test state seeding rpc.
      return Steps(
          ParentSetsPermissionsSwitch(kParentControlsTab, switch_state),
          WaitForState(kBoolPermissionsPreferenceObserver, pref_target_value));
    }
    // TODO(b/318069335): Setting the corresponding preference directly in
    // Chrome, until the FL "Extensions" switch is fully released. Once
    // available, set it via UI or chrome test state seeding rpc.
    CHECK(family_link_switch == FamilyLinkSwitch::kExtensionsSwitch);
    return Steps(
        Do([this, pref_target_value]() -> void {
          supervised_user_test_util::
              SetSkipParentApprovalToInstallExtensionsPref(
                  child().browser()->profile(), pref_target_value);
        }),
        WaitForState(kBoolExtensionsPreferenceObserver, pref_target_value));
  }

  ui::ElementIdentifier GetTargetUIElement() {
    if (GetExtensionHandlingMode() ==
        ExtensionHandlingMode::kExtensionsGovernedByPermissionsSwitch) {
      // Depending on the "Permissions" switch's value either the "Parent
      // Approval Dialog" (switch ON) or the "Extensions Blocked by Parent"
      // error message will appear at the end.
      return GetPermissionsSwitchTargetState() ==
                     FamilyLinkSwitchState::kEnabled
                 ? ParentPermissionDialog::kDialogViewIdForTesting
                 : extensions::kParentBlockedDialogMessage;
    }
    // If goverved by extensions:
    CHECK(GetExtensionHandlingMode() ==
          ExtensionHandlingMode::kExtensionsGovernedByExtensionsSwitch);
    CHECK(GetExtensionsSwitchTargetState() == FamilyLinkSwitchState::kDisabled);
    // Parent approval dialog should appear.
    return ParentPermissionDialog::kDialogViewIdForTesting;
  }

  auto GetSwitchStateForRequestingParentApproval() {
    switch (GetExtensionGoverningSwitch()) {
      case (FamilyLinkSwitch::kPermissionsSwitch): {
        return FamilyLinkSwitchState::kEnabled;
      }
      case (FamilyLinkSwitch::kExtensionsSwitch): {
        return FamilyLinkSwitchState::kDisabled;
      }
      default: {
        NOTREACHED_NORETURN();
      }
    }
  }

  auto CheckForParentDialogIfExtensionDisabled(
      bool is_expected_extension_enabled) {
    if (is_expected_extension_enabled) {
      // No dialog appears in this case.
      return Steps();
    }
    auto target_ui_element_id = GetTargetUIElement();
    return Steps(
        WaitForShow(target_ui_element_id),
        Log(base::StringPrintf("The %s appears.",
                               (target_ui_element_id ==
                                ParentPermissionDialog::kDialogViewIdForTesting)
                                   ? "parent approval dialog"
                                   : "blocked extension message")));
  }

  FamilyLinkSwitchState GetPermissionsSwitchTargetState() {
    return std::get<1>(GetParam());
  }

  FamilyLinkSwitchState GetExtensionsSwitchTargetState() {
    return std::get<2>(GetParam());
  }

  ExtensionHandlingMode GetExtensionHandlingMode() {
    return std::get<3>(GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(SupervisedUserExtensionsParentalControlsUiTest,
                       ChildTogglesExtensionMissingParentApproval) {
  extensions::ScopedInstallVerifierBypassForTest install_verifier_bypass;

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kChildElementId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kParentControlsTab);
  const int child_tab_index = 0;
  const int parent_tab_index = 0;

  // The extensions should be disabled (pending parent approval) in all cases,
  // expect when the new "Extensions" FL switch is enabled and is used
  // in Chrome to manage extensions.
  const bool is_expected_extension_enabled =
      GetExtensionHandlingMode() ==
          ExtensionHandlingMode::kExtensionsGovernedByExtensionsSwitch &&
      GetExtensionsSwitchTargetState() == FamilyLinkSwitchState::kEnabled;

  TurnOnSyncFor(head_of_household());
  TurnOnSyncFor(child());

  RunTestSequence(
      InstrumentTab(kParentControlsTab, parent_tab_index,
                    head_of_household().browser()),
      ParentOpensControlPage(kParentControlsTab,
                             head_of_household().GetPermissionsUrlFor(child())),
      PollPreference(kBoolExtensionsPreferenceObserver,
                     prefs::kSkipParentApprovalToInstallExtensions),
      PollPreference(kBoolPermissionsPreferenceObserver,
                     prefs::kSupervisedUserExtensionsMayRequestPermissions),
      // Set the corresponding FL switch in "Require parent approval" mode.
      ParentsSetsFamilyLinkSwitchAndWaitsForChromeToReceiveIt(
          GetExtensionGoverningSwitch(),
          GetSwitchStateForRequestingParentApproval(), kParentControlsTab),
      Log("Given parental configuration that allows extensions."));

  InstallExtension("A Extension", child().browser()->profile());

  RunTestSequence(InAnyContext(Steps(
      Log("Given an installed disabled exetension."),
      // Parent sets both the FL Permissions and Extensions switches.
      // Only one of them impacts the handling of supervised user extensions.
      ParentsSetsFamilyLinkSwitchAndWaitsForChromeToReceiveIt(
          FamilyLinkSwitch::kPermissionsSwitch,
          GetPermissionsSwitchTargetState(), kParentControlsTab),
      ParentsSetsFamilyLinkSwitchAndWaitsForChromeToReceiveIt(
          FamilyLinkSwitch::kExtensionsSwitch, GetExtensionsSwitchTargetState(),
          kParentControlsTab),

      // Child navigates to the extensions page and tries to enable the
      // extension, if it is disabled.
      Log("When child visits the extensions management page."),
      InstrumentTab(kChildElementId, child_tab_index, child().browser()),
      NavigateWebContents(kChildElementId, GURL(kChromeManageExternsionsUrl)),
      WaitForStateChange(kChildElementId, PageWithMatchingTitle("Extensions")),
      Log("When child tries to enable the extension."),
      ChildClicksEnableExtensionIfExtensionDisbaled(
          kChildElementId, is_expected_extension_enabled),
      // If the extension is not already enabled, check that the expect UI
      // dialog appears.
      CheckForParentDialogIfExtensionDisabled(is_expected_extension_enabled))));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SupervisedUserExtensionsParentalControlsUiTest,
    testing::Combine(
        testing::Values(FamilyIdentifier("FAMILY_DMA_ELIGIBILE_NO_CONSENT"),
                        FamilyIdentifier("FAMILY_DMA_ELIGIBLE_WITH_CONSENT"),
                        FamilyIdentifier("FAMILY_DMA_INELIGIBLE")),
        /*permissions_switch_target_value=*/
        testing::Values(FamilyLinkSwitchState::kEnabled,
                        FamilyLinkSwitchState::kDisabled),
        /*extensions_switch_target_value==*/
        testing::Values(
            // TODO(b/321239324): Parametrize with Extensions switch ON once the
            // handling of extensions on switch flipping is added.
            FamilyLinkSwitchState::kDisabled),
        /*extensions_handling_mode=*/
        testing::Values(
            ExtensionHandlingMode::kExtensionsGovernedByPermissionsSwitch,
            ExtensionHandlingMode::kExtensionsGovernedByExtensionsSwitch)),
    [](const auto& info) {
      return std::string(std::get<0>(info.param)->data()) +
             std::string(
                 (std::get<1>(info.param) == FamilyLinkSwitchState::kEnabled
                      ? "WithPermissionsOn"
                      : "WithPermissionsOff")) +
             std::string(
                 (std::get<2>(info.param) == FamilyLinkSwitchState::kEnabled
                      ? "WithExtensionsOn"
                      : "WithExtensionsOff")) +
             std::string((std::get<3>(info.param) ==
                                  ExtensionHandlingMode::
                                      kExtensionsGovernedByPermissionsSwitch
                              ? "ManagedByPermissionsSwitch"
                              : "ManagedByExtensionsSwitch"));
    });
}  // namespace
}  // namespace supervised_user

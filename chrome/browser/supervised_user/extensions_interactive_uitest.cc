// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
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
#include "chrome/test/supervised_user/test_state_seeded_observer.h"
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

// Family Link switch that governs the handling of extensions for SU.
enum class ExtensionHandlingMode : int {
  kExtensionsGovernedByPermissionsSwitch = 0,
  kExtensionsGovernedByExtensionsSwitch = 1,
};

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
    : public InteractiveFamilyLiveTest,
      public testing::WithParamInterface<std::tuple<
          FamilyIdentifier,
          /*permissions_switch_state=*/FamilyLinkToggleState,
          /*extensions_switch_state=*/FamilyLinkToggleState,
          // Depending on the ExtensionHandlingMode only one switch
          // should affect the behaviour of supervised user's extensions.
          // Toggling the other switch should have no effect to the result.
          /*extensions_handling_mode=*/ExtensionHandlingMode>> {
 public:
  SupervisedUserExtensionsParentalControlsUiTest()
      : InteractiveFamilyLiveTest(std::get<0>(GetParam())) {
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
  // Child tries to enable a disabled extension (which is pending parent
  // approval) by clicking at the extension's toggle.
  // When the Extensions toggle is ON and used to manage the extensions,
  // the extension should be already enabled.
  // In that case the method only verifies the enabled state.
  auto ChildClicksEnableExtensionIfExtensionDisabled(
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

  ui::ElementIdentifier GetTargetUIElement() {
    if (GetExtensionHandlingMode() ==
        ExtensionHandlingMode::kExtensionsGovernedByPermissionsSwitch) {
      // Depending on the "Permissions" switch's value either the "Parent
      // Approval Dialog" (switch ON) or the "Extensions Blocked by Parent"
      // error message will appear at the end.
      return GetPermissionsSwitchTargetState() ==
                     FamilyLinkToggleState::kEnabled
                 ? ParentPermissionDialog::kDialogViewIdForTesting
                 : extensions::kParentBlockedDialogMessage;
    }
    // If governed by extensions:
    CHECK(GetExtensionHandlingMode() ==
          ExtensionHandlingMode::kExtensionsGovernedByExtensionsSwitch);
    CHECK(GetExtensionsSwitchTargetState() == FamilyLinkToggleState::kDisabled);
    // Parent approval dialog should appear.
    return ParentPermissionDialog::kDialogViewIdForTesting;
  }

  auto CheckForParentDialogIfExtensionDisabled(
      bool is_expected_extension_enabled) {
    if (is_expected_extension_enabled) {
      // No dialog appears in this case.
      return Steps(Log("No dialog check is done, the extension is enabled."));
    }
    auto target_ui_element_id = GetTargetUIElement();
    return Steps(
        Log(base::StringPrintf("Waiting for the %s to appear.",
                               (target_ui_element_id ==
                                ParentPermissionDialog::kDialogViewIdForTesting)
                                   ? "parent approval dialog"
                                   : "blocked extension message")),
        WaitForShow(target_ui_element_id),
        Log(base::StringPrintf("The %s appears.",
                               (target_ui_element_id ==
                                ParentPermissionDialog::kDialogViewIdForTesting)
                                   ? "parent approval dialog"
                                   : "blocked extension message")));
  }

  static FamilyLinkToggleState GetPermissionsSwitchTargetState() {
    return std::get<1>(GetParam());
  }

  static FamilyLinkToggleState GetExtensionsSwitchTargetState() {
    return std::get<2>(GetParam());
  }

  static ExtensionHandlingMode GetExtensionHandlingMode() {
    return std::get<3>(GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(SupervisedUserExtensionsParentalControlsUiTest,
                       ChildTogglesExtensionMissingParentApproval) {
  extensions::ScopedInstallVerifierBypassForTest install_verifier_bypass;

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kChildElementId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BrowserState::Observer,
                                      kDefineStateObserverId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BrowserState::Observer,
                                      kResetStateObserverId);
  const int child_tab_index = 0;

  // The extensions should be disabled (pending parent approval) in all cases,
  // expect when the new "Extensions" FL switch is enabled and is used
  // in Chrome to manage extensions.
  const bool should_be_enabled =
      GetExtensionHandlingMode() ==
          ExtensionHandlingMode::kExtensionsGovernedByExtensionsSwitch &&
      GetExtensionsSwitchTargetState() == FamilyLinkToggleState::kEnabled;

  TurnOnSyncFor(head_of_household());
  TurnOnSyncFor(child());

  // Set the FL switch in the value that require parent approvals for
  // extension installation.
  RunTestSequence(
      Log("Set config that requires parental approvals."),
      WaitForStateSeeding(kResetStateObserverId, head_of_household(), child(),
                          BrowserState::SetAdvancedSettingsDefault()));

  InstallExtension("An Extension", child().browser()->profile());

  RunTestSequence(InAnyContext(Steps(
      Log("Given an installed disabled extension."),
      // Parent sets both the FL Permissions and Extensions switches.
      // Only one of them impacts the handling of supervised user extensions.
      WaitForStateSeeding(
          kDefineStateObserverId, head_of_household(), child(),
          BrowserState::AdvancedSettingsToggles(
              {FamilyLinkToggleConfiguration(
                   {.type = FamilyLinkToggleType::kExtensionsToggle,
                    .state = GetExtensionsSwitchTargetState()}),
               FamilyLinkToggleConfiguration(
                   {.type = FamilyLinkToggleType::kPermissionsToggle,
                    .state = GetPermissionsSwitchTargetState()})})),
      // Child navigates to the extensions page and tries to enable the
      // extension, if it is disabled.
      Log("When child visits the extensions management page."),
      InstrumentTab(kChildElementId, child_tab_index, child().browser()),
      NavigateWebContents(kChildElementId, GURL(kChromeManageExternsionsUrl)),
      WaitForStateChange(kChildElementId, PageWithMatchingTitle("Extensions")),
      Log("When child tries to enable the extension."),
      ChildClicksEnableExtensionIfExtensionDisabled(kChildElementId,
                                                    should_be_enabled),
      // If the extension is not already enabled, check that the expect UI
      // dialog appears.
      CheckForParentDialogIfExtensionDisabled(should_be_enabled))));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SupervisedUserExtensionsParentalControlsUiTest,
    testing::Combine(
        testing::Values(FamilyIdentifier("FAMILY_DMA_ELIGIBLE_NO_CONSENT"),
                        FamilyIdentifier("FAMILY_DMA_ELIGIBLE_WITH_CONSENT"),
                        FamilyIdentifier("FAMILY_DMA_INELIGIBLE")),
        /*permissions_switch_target_value=*/
        testing::Values(FamilyLinkToggleState::kEnabled,
                        FamilyLinkToggleState::kDisabled),
        /*extensions_switch_target_value==*/
        testing::Values(FamilyLinkToggleState::kEnabled,
                        FamilyLinkToggleState::kDisabled),
        /*extensions_handling_mode=*/
        testing::Values(
            ExtensionHandlingMode::kExtensionsGovernedByPermissionsSwitch,
            ExtensionHandlingMode::kExtensionsGovernedByExtensionsSwitch)),
    [](const auto& info) {
      return std::string(std::get<0>(info.param)->data()) +
             std::string(
                 (std::get<1>(info.param) == FamilyLinkToggleState::kEnabled
                      ? "WithPermissionsOn"
                      : "WithPermissionsOff")) +
             std::string(
                 (std::get<2>(info.param) == FamilyLinkToggleState::kEnabled
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

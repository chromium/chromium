// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <tuple>

#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
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
#include "components/supervised_user/test_support/browser_state_management.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"

namespace supervised_user {
namespace {

static constexpr std::string_view kChromeManageExternsionsUrl =
    "chrome://extensions/";
static constexpr std::string_view kExtensionSiteSettingsUrl =
    "chrome://settings/content/siteDetails?site=chrome-extension://";
static constexpr std::string_view kExtensionName = "An Extension";

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
          FamilyLiveTest::RpcMode,
          /*permissions_switch_state=*/FamilyLinkToggleState,
          /*extensions_switch_state=*/FamilyLinkToggleState,
          // Depending on the ExtensionHandlingMode only one switch
          // should affect the behaviour of supervised user's extensions.
          // Toggling the other switch should have no effect to the result.
          /*extensions_handling_mode=*/ExtensionHandlingMode>> {
 public:
  SupervisedUserExtensionsParentalControlsUiTest()
      : InteractiveFamilyLiveTest(GetRpcMode()) {
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
                  if (toggle.ariaPressed != "%s") {
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
  void InstallExtension(Profile* profile) {
    std::string extension_manifest = base::StringPrintf(
        R"({
            "name": "%s",
            "manifest_version": 3,
            "version": "0.1",
            "host_permissions": ["<all_urls>"],
            "permissions": [ "geolocation" ]
          })",
        kExtensionName.data());
    extensions::TestExtensionDir extension_dir;
    extension_dir.WriteManifest(extension_manifest);

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

  // Navigates to the `Settings` page for the installed extension under test
  // and inspects the permissions granted to the `Location` setting.
  // Checks if the `Locations` attribute is editable or not (html attribute
  // should be disabled), respecting the configuration of the "Permissions"
  // switch in Family Link.
  auto CheckExtensionLocationPermissions(ui::ElementIdentifier kChildElementId,
                                         Profile* profile) {
    if (GetExtensionHandlingMode() ==
            ExtensionHandlingMode::kExtensionsGovernedByPermissionsSwitch &&
        GetPermissionsSwitchTargetState() == FamilyLinkToggleState::kDisabled) {
      // No extension has been installed on this mode, there are no permissions
      // to check.
      return Steps();
    }

    extensions::ExtensionId installed_extension_id;
    const auto& installed_extensions =
        extensions::ExtensionRegistry::Get(profile)
            ->GenerateInstalledExtensionsSet();
    for (const auto& extension : installed_extensions) {
      if (extension->name() == kExtensionName) {
        installed_extension_id = extension->id();
        break;
      }
    }
    CHECK(installed_extension_id.size() > 0)
        << "There must be an installed extension.";

    // When the Permissions FL switch is Off, the Location permissions button
    // should be disabled (unmodifiable).
    bool permissions_button_greyed_out =
        GetPermissionsSwitchTargetState() == FamilyLinkToggleState::kDisabled;
    return Steps(
        Log("With installed extension : " + installed_extension_id),
        NavigateWebContents(kChildElementId,
                            GURL(std::string(kExtensionSiteSettingsUrl) +
                                 std::string(installed_extension_id))),
        WaitForStateChange(kChildElementId, PageWithMatchingTitle("Settings")),
        Log("With extension settings page open."),
        // Detect the Location permission and check whether it's user
        // modifiable.
        ExecuteJs(kChildElementId,
                  base::StringPrintf(
                      R"js(
          () => { const location_permission = document.querySelector("body > settings-ui")
                .shadowRoot.querySelector("#main")
                .shadowRoot.querySelector("settings-basic-page")
                .shadowRoot.querySelector("#basicPage > settings-section.expanded > settings-privacy-page")
                .shadowRoot.querySelector("#pages > settings-subpage > site-details")
                .shadowRoot.querySelector('[label="Location"]')
                .shadowRoot.querySelector("#permission");
                if (!location_permission) {
                  throw Error('No location permission menu was found.');
                }
                if (location_permission.disabled === "%s") {
                  throw Error('Unexpected Location Permission state: ' + permission_drop.disabled);
                }
              }
          )js",
                      permissions_button_greyed_out ? "true" : "false")),
        Log("Child inspected Location Permission button."));
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

  static FamilyLiveTest::RpcMode GetRpcMode() {
    return std::get<0>(GetParam());
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
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(InIntendedStateObserver,
                                      kDefineStateObserverId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(InIntendedStateObserver,
                                      kResetStateObserverId);
  const int child_tab_index = 0;

  // The extensions should be disabled (pending parent approval) in all cases,
  // expect when the new "Extensions" FL switch is enabled and is used
  // in Chrome to manage extensions.
  const bool should_be_enabled =
      GetExtensionHandlingMode() ==
          ExtensionHandlingMode::kExtensionsGovernedByExtensionsSwitch &&
      GetExtensionsSwitchTargetState() == FamilyLinkToggleState::kEnabled;

  TurnOnSync();

  // Set the FL switch in the value that require parent approvals for
  // extension installation.
  RunTestSequence(
      Log("Set config that requires parental approvals."),
      WaitForStateSeeding(kResetStateObserverId, child(),
                          BrowserState::SetAdvancedSettingsDefault()));

  InstallExtension(&child().profile());

  RunTestSequence(InAnyContext(Steps(
      Log("Given an installed disabled extension."),
      // Parent sets both the FL Permissions and Extensions switches.
      // Only one of them impacts the handling of supervised user extensions.
      WaitForStateSeeding(
          kDefineStateObserverId, child(),
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
      InstrumentTab(kChildElementId, child_tab_index, &child().browser()),
      NavigateWebContents(kChildElementId, GURL(kChromeManageExternsionsUrl)),
      WaitForStateChange(kChildElementId, PageWithMatchingTitle("Extensions")),
      Log("When child tries to enable the extension."),
      ChildClicksEnableExtensionIfExtensionDisabled(kChildElementId,
                                                    should_be_enabled),
      // If the extension is not already enabled, check that the expect UI
      // dialog appears.
      CheckForParentDialogIfExtensionDisabled(should_be_enabled),
      CheckExtensionLocationPermissions(kChildElementId, &child().profile()))));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SupervisedUserExtensionsParentalControlsUiTest,
    testing::Combine(
        testing::Values(FamilyLiveTest::RpcMode::kProd,
                        FamilyLiveTest::RpcMode::kTestImpersonation),
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
      return ToString(std::get<0>(info.param)) +
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

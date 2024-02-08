// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <tuple>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/supervised_user/family_live_test.h"
#include "chrome/test/supervised_user/family_member.h"
#include "components/prefs/pref_service.h"
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

// State of a Family Link switch
enum class FamilyLinkSwitchState : int {
  kEnabled = 0,
  kDisabled,
};

constexpr const char* kFamilyLinkSwitchStateToString[2] = {"true", "false"};

using BoolPermissionsStateObserver = ui::test::PollingStateObserver<bool>;

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BoolPermissionsStateObserver,
                                    kBoolPermissionsPreferenceObserver);

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
      public testing::WithParamInterface<
          std::tuple<supervised_user::FamilyIdentifier,
                     FamilyLinkSwitchState>> {
 public:
  SupervisedUserExtensionsParentalControlsUiTest()
      : InteractiveBrowserTestT<FamilyLiveTest>(
            /*family_identifier=*/std::get<0>(GetParam()),
            /*extra_enabled_hosts=*/std::vector<std::string>()) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    // Enable extensions parental controls.
    feature_list_.InitAndEnableFeature(
        supervised_user::
            kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
#endif
  }

 protected:
  // Parenr navigates to FL control page and waits for it to load.
  auto ParentOpensControlPage(ui::ElementIdentifier kParentTab,
                              const GURL& gurl) {
    return Steps(NavigateWebContents(kParentTab, gurl),
                 WaitForWebContentsReady(kParentTab, gurl));
  }

  // Child tries to enable a disabled extension (which is pending parent
  // approval) by clicking at the extension's toggle.
  auto ChildClicksEnableExtension(ui::ElementIdentifier kChildTab) {
    return Steps(ExecuteJs(kChildTab,
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
                  const extn = container.querySelectorAll("extensions-item")[0];
                  if (!extn) {
                    throw Error("Path to extension element is invalid.");
                  }
                  const toggle = extn.shadowRoot.querySelector("#enableToggle");
                  if (!toggle) {
                    throw Error("Path to extension toggle is invalid.");
                  }
                  toggle.click();
                }
              )js"));
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

  // Polls the Permissions preference value in Chrome.
  auto PollPermissionsPreference(
      ui::test::StateIdentifier<BoolPermissionsStateObserver>
          permission_preference_observer) {
    return Steps(PollState(permission_preference_observer, [this]() {
      return child().browser()->profile()->GetPrefs()->GetBoolean(
          prefs::kSupervisedUserExtensionsMayRequestPermissions);
    }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(SupervisedUserExtensionsParentalControlsUiTest,
                       ChildTogglesExtensionMissingParentApproval) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kChildElementId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kParentControlsTab);
  int child_tab_index = 0;
  int parent_tab_index = 0;
  auto switch_target_state = std::get<1>(GetParam());
  TurnOnSyncFor(head_of_household());
  TurnOnSyncFor(child());

  // Depending on the "Permissions" switch's value either the "Parent Approval
  // Dialog" (switch ON) or the "Extensions Blocked by Parent" error message
  // will appear at the end.
  auto target_ui_element_id =
      switch_target_state == FamilyLinkSwitchState::kEnabled
          ? ParentPermissionDialog::kDialogViewIdForTesting
          : extensions::kParentBlockedDialogMessage;

  RunTestSequence(Steps(
      // Parent sets the FL switch "Permissions" to ON.
      // TODO(b/303401498): Use chrome test state seeding rpc.
      InstrumentTab(kParentControlsTab, parent_tab_index,
                    head_of_household().browser()),

      ParentOpensControlPage(kParentControlsTab,
                             head_of_household().GetPermissionsUrlFor(child())),
      PollPermissionsPreference(kBoolPermissionsPreferenceObserver),
      ParentSetsPermissionsSwitch(kParentControlsTab,
                                  FamilyLinkSwitchState::kEnabled),
      WaitForState(kBoolPermissionsPreferenceObserver, true)));

  RunTestSequence(InAnyContext(Steps(
      // Install programmatically an extension. It is pending parent approval.
      Do([this]() -> void {
        InstallExtension("A Extension", child().browser()->profile());
      }),
      // Parent sets the FL Permissions switch.
      ParentSetsPermissionsSwitch(kParentControlsTab, switch_target_state),
      WaitForState(kBoolPermissionsPreferenceObserver,
                   switch_target_state == FamilyLinkSwitchState::kEnabled
                       ? true
                       : false),

      // Child navigates to the extensions page and tries to enable the
      // extension.
      InstrumentTab(kChildElementId, child_tab_index, child().browser()),
      NavigateWebContents(kChildElementId, GURL(kChromeManageExternsionsUrl)),
      WaitForStateChange(kChildElementId, PageWithMatchingTitle("Extensions")),
      ChildClicksEnableExtension(kChildElementId),
      // The parent approval dialog or the Blocked extensions error message
      // appears.
      WaitForShow(target_ui_element_id))));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SupervisedUserExtensionsParentalControlsUiTest,
    testing::Combine(
        testing::Values(supervised_user::FamilyIdentifier("FAMILY_DMA_ALL")),
        testing::Values(FamilyLinkSwitchState::kEnabled,
                        FamilyLinkSwitchState::kDisabled)),
    [](const auto& info) {
      return std::string(std::get<0>(info.param)->data()) +
             std::string(
                 (std::get<1>(info.param) == FamilyLinkSwitchState::kEnabled
                      ? "WithPermissionsSwitchOn"
                      : "WithPermissionsSwitchOff"));
    });
}  // namespace
}  // namespace supervised_user

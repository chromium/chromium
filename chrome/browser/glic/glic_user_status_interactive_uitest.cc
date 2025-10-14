// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/glic_user_status_code.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/interaction/interactive_views_test.h"

namespace glic {
namespace {

// Interactive UI test for the Glic user status feature.
// This test verifies that the Glic button in the toolbar correctly shows and
// hides based on the user's Glic status. It also checks the state of the Glic
// settings bubble, ensuring that for signed-in users, a "cr-domain" icon is
// visible and relevant controls are disabled.
class GlicUserStatusInteractiveUiTest : public test::InteractiveGlicTest {
 public:
  GlicUserStatusInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicUserStatusCheck,
        {{features::kGlicUserStatusThrottleInterval.name, "100ms"}});
  }

  void SetUpOnMainThread() override {
    InteractiveGlicTestMixin::SetUpOnMainThread();
    glic_service()->enabling().SetUserStatusFetchOverrideForTest(
        base::BindRepeating(&GlicUserStatusInteractiveUiTest::UserStatusFetch,
                            base::Unretained(this)));
  }

  StateChange GlicSettingsPageExists() {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kGlicSettingsPageExists);
    StateChange event;
    event.event = kGlicSettingsPageExists;
    event.where = {"settings-ui", "settings-main", "settings-ai-page-index",
                   "settings-glic-page"};
    return event;
  }

  StateChange SettingsPageElementEnabled(ui::CustomElementEventType type,
                                         std::string_view element_id,
                                         bool enabled) {
    StateChange event;
    event.event = type;
    event.where = {"settings-ui", "settings-main", "settings-ai-page-index",
                   "settings-glic-subpage", std::string(element_id)};
    event.test_function =
        content::JsReplace("el => el.disabled === $1", !enabled);
    return event;
  }

  auto SetToggleState(ui::ElementIdentifier tab,
                      std::string_view element_id,
                      bool state) {
    WebContentsInteractionTestUtil::DeepQuery where{
        "settings-ui", "settings-main", "settings-ai-page-index",
        "settings-glic-subpage", std::string(element_id)};
    return ExecuteJsAt(
        tab, where,
        content::JsReplace("el => { if (el.checked !== $1) el.click(); }",
                           state));
  }

  auto CheckToggleState(ui::ElementIdentifier tab,
                        std::string_view element_id,
                        bool state) {
    WebContentsInteractionTestUtil::DeepQuery where{
        "settings-ui", "settings-main", "settings-ai-page-index",
        "settings-glic-subpage", std::string(element_id)};
    return CheckJsResultAt(
        tab, where, content::JsReplace("el => el.checked === $1", state));
  }

  StateChange DisabledByAdminNoticeShown() {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kDisabledByAdminNoticeShown);
    StateChange event;
    event.event = kDisabledByAdminNoticeShown;
    event.where = {"settings-ui", "settings-main", "settings-ai-page-index",
                   "settings-glic-subpage",
                   ".section:has(cr-icon[icon='cr:domain'])"};
    event.test_function = "el => el.textContent";
    event.check_callback = base::BindRepeating([](const base::Value& text) {
      std::string disabled_notice =
          l10n_util::GetStringUTF8(IDS_SETTINGS_GLIC_POLICY_DISABLED_MESSAGE);
      return text.is_string() &&
             text.GetString().find(disabled_notice) != std::string::npos;
    });
    return event;
  }

  StateChange DisabledByAdminNoticeNotShown() {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kDisabledByAdminNoticeNotShown);
    StateChange event;
    event.event = kDisabledByAdminNoticeNotShown;
    event.type = StateChange::Type::kDoesNotExist;
    event.where = {"settings-ui", "settings-main", "settings-ai-page-index",
                   "settings-glic-subpage",
                   ".section:has(cr-icon[icon='cr:domain'])"};
    return event;
  }

  void UserStatusFetch(
      base::OnceCallback<void(const CachedUserStatus&)> callback) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), user_status_));
  }

  CachedUserStatus user_status_{.user_status_code = UserStatusCode::ENABLED};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

void UpdatePrimaryAccountToBeManaged(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountInfo core_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_manager->FindExtendedAccountInfo(core_account_info);
  account_info.hosted_domain = gaia::ExtractDomainName(account_info.email);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);
}

IN_PROC_BROWSER_TEST_F(GlicUserStatusInteractiveUiTest,
                       GlicButtonVisibilityAndSettingsState) {
  Profile* profile = browser()->profile();
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile),
      policy::EnterpriseManagementAuthority::CLOUD);
  UpdatePrimaryAccountToBeManaged(profile);

  ASSERT_FALSE(GlicEnabling::EnablementForProfile(profile).DisallowedByAdmin());

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kLauncherToggleEnabled);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kLauncherToggleDisabled);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kGeolocationToggleEnabled);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kGeolocationToggleDisabled);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kMicrophoneToggleEnabled);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kMicrophoneToggleDisabled);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTabAccessToggleEnabled);
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTabAccessToggleDisabled);

  RunTestSequence(
      // Open the Glic settings page.
      InstrumentTab(kFirstTab), Do([this] {
        chrome::ShowSettingsSubPage(browser(), chrome::kGlicSettingsSubpage);
      }),
      WaitForWebContentsNavigation(
          kFirstTab, chrome::GetSettingsUrl(chrome::kGlicSettingsSubpage)),
      WaitForStateChange(kFirstTab, GlicSettingsPageExists()),

      // The Glic related controls should be available and enabled.
      WaitForShow(kGlicButtonElementId),
      WaitForStateChange(kFirstTab, DisabledByAdminNoticeNotShown()),
      WaitForStateChange(kFirstTab,
                         SettingsPageElementEnabled(kLauncherToggleEnabled,
                                                    "#launcherToggle", true)),
      WaitForStateChange(
          kFirstTab, SettingsPageElementEnabled(kGeolocationToggleEnabled,
                                                "#geolocationToggle", true)),
      WaitForStateChange(kFirstTab,
                         SettingsPageElementEnabled(kMicrophoneToggleEnabled,
                                                    "#microphoneToggle", true)),
      WaitForStateChange(kFirstTab,
                         SettingsPageElementEnabled(kTabAccessToggleEnabled,
                                                    "#tabAccessToggle", true)),

      // Flip some of these settings so we can check that these settings are
      // used when re-enabled.
      SetToggleState(kFirstTab, "#geolocationToggle", true),
      SetToggleState(kFirstTab, "#microphoneToggle", false),

      // Learn that Glic is disabled.
      Do([this] {
        user_status_.user_status_code = UserStatusCode::DISABLED_BY_ADMIN;
        glic_service()->enabling().UpdateUserStatusWithThrottling();
      }),

      // The Glic related controls should be hidden or disabled.
      WaitForHide(kGlicButtonElementId),
      WaitForStateChange(kFirstTab, DisabledByAdminNoticeShown()),
      WaitForStateChange(kFirstTab,
                         SettingsPageElementEnabled(kLauncherToggleDisabled,
                                                    "#launcherToggle", false)),
      WaitForStateChange(
          kFirstTab, SettingsPageElementEnabled(kGeolocationToggleDisabled,
                                                "#geolocationToggle", false)),
      WaitForStateChange(
          kFirstTab, SettingsPageElementEnabled(kMicrophoneToggleDisabled,
                                                "#microphoneToggle", false)),
      WaitForStateChange(kFirstTab,
                         SettingsPageElementEnabled(kTabAccessToggleDisabled,
                                                    "#tabAccessToggle", false)),

      // Learn that Glic is enabled.
      Do([this] {
        user_status_.user_status_code = UserStatusCode::ENABLED;
        glic_service()->enabling().UpdateUserStatusWithThrottling();
      }),

      // The Glic related controls should be available and enabled.
      WaitForShow(kGlicButtonElementId),
      WaitForStateChange(kFirstTab, DisabledByAdminNoticeNotShown()),
      WaitForStateChange(kFirstTab,
                         SettingsPageElementEnabled(kLauncherToggleEnabled,
                                                    "#launcherToggle", true)),
      WaitForStateChange(
          kFirstTab, SettingsPageElementEnabled(kGeolocationToggleEnabled,
                                                "#geolocationToggle", true)),
      WaitForStateChange(kFirstTab,
                         SettingsPageElementEnabled(kMicrophoneToggleEnabled,
                                                    "#microphoneToggle", true)),
      WaitForStateChange(kFirstTab,
                         SettingsPageElementEnabled(kTabAccessToggleEnabled,
                                                    "#tabAccessToggle", true)),

      // Check that the settings we flipped earlier are still there.
      CheckToggleState(kFirstTab, "#geolocationToggle", true),
      CheckToggleState(kFirstTab, "#microphoneToggle", false));
}

}  // namespace
}  // namespace glic

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/smart_card/chromeos_smart_card_delegate.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestTab);
constexpr char kFooReader[] = "foo reader";

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(
    ui::test::PollingStateObserver<std::optional<bool>>,
    kPermissionDecision);

class SmartCardPermissionUiTest : public InteractiveBrowserTest {
 public:
  void OnPermissionDecided(bool granted) {
    CHECK_EQ(permission_decision_.has_value(), false);
    permission_decision_ = granted;
  }

  auto CheckReaderPermission(bool has_permission) {
    return CheckResult(
        [this]() -> bool {
          return GetSmartCardDelegate()->HasReaderPermission(GetMainFrameHost(),
                                                             kFooReader);
        },
        has_permission);
  }

  auto RequestReaderPermission() {
    return Do([this]() {
      GetSmartCardDelegate()->RequestReaderPermission(
          GetMainFrameHost(), kFooReader,
          base::BindOnce(&SmartCardPermissionUiTest::OnPermissionDecided,
                         base::Unretained(this)));
    });
  }

  auto BlockPermission(const GURL& origin_url) {
    return Do([this, origin_url]() {
      auto* settings_map =
          HostContentSettingsMapFactory::GetForProfile(browser()->profile());
      settings_map->SetContentSettingDefaultScope(
          origin_url, GURL(), ContentSettingsType::SMART_CARD_GUARD,
          ContentSetting::CONTENT_SETTING_BLOCK);
    });
  }

  auto PressButtonAndWaitResult(ui::ElementIdentifier button_id, bool granted) {
    return Steps(PollState(kPermissionDecision,
                           [this]() { return permission_decision(); }),
                 PressButton(button_id),
                 Log("Wait for the prompt to be hidden."),
                 WaitForHide(PermissionPromptBubbleBaseView::kMainViewId),
                 Log("Wait for the permission decision."),
                 WaitForState(kPermissionDecision, granted));
  }

  std::optional<bool> permission_decision() { return permission_decision_; }

  content::SmartCardDelegate* GetSmartCardDelegate() {
    return content::GetContentClientForTesting()
        ->browser()
        ->GetSmartCardDelegate();
  }

  content::RenderFrameHost& GetMainFrameHost() {
    return *browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetPrimaryMainFrame();
  }

 private:
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  std::optional<bool> permission_decision_;

  base::test::ScopedFeatureList scoped_feature_list_{
      permissions::features::kOneTimePermission};
};

IN_PROC_BROWSER_TEST_F(SmartCardPermissionUiTest, AllowOnce) {
  RunTestSequence(
      InstrumentTab(kTestTab),
      NavigateWebContents(kTestTab, embedded_https_test_server().GetURL(
                                        "a.com", "/simple.html")),
      CheckReaderPermission(/*has_permission=*/false),
      RequestReaderPermission(),
      WaitForShow(PermissionPromptBubbleBaseView::kMainViewId),
      PressButtonAndWaitResult(
          PermissionPromptBubbleBaseView::kAllowOnceButtonElementId,
          /*granted=*/true),
      CheckReaderPermission(/*has_permission=*/true));
}

IN_PROC_BROWSER_TEST_F(SmartCardPermissionUiTest, AllowAlways) {
  RunTestSequence(
      InstrumentTab(kTestTab),
      NavigateWebContents(kTestTab, embedded_https_test_server().GetURL(
                                        "a.com", "/simple.html")),
      CheckReaderPermission(/*has_permission=*/false),
      RequestReaderPermission(),
      WaitForShow(PermissionPromptBubbleBaseView::kMainViewId),
      CheckViewProperty(
          PermissionPromptBubbleBaseView::kAllowButtonElementId,
          &views::LabelButton::GetText,
          l10n_util::GetStringUTF16(IDS_SMART_CARD_PERMISSION_ALWAYS_ALLOW)),
      PressButtonAndWaitResult(
          PermissionPromptBubbleBaseView::kAllowButtonElementId,
          /*granted=*/true),
      CheckReaderPermission(/*has_permission=*/true));
}

IN_PROC_BROWSER_TEST_F(SmartCardPermissionUiTest, Deny) {
  RunTestSequence(
      InstrumentTab(kTestTab),
      NavigateWebContents(kTestTab, embedded_https_test_server().GetURL(
                                        "a.com", "/simple.html")),
      CheckReaderPermission(/*has_permission=*/false),
      RequestReaderPermission(),
      WaitForShow(PermissionPromptBubbleBaseView::kMainViewId),
      PressButtonAndWaitResult(
          PermissionPromptBubbleBaseView::kBlockButtonElementId,
          /*granted=*/false),
      CheckReaderPermission(/*has_permission=*/false));
}

IN_PROC_BROWSER_TEST_F(SmartCardPermissionUiTest, Blocked) {
  auto simple_url =
      embedded_https_test_server().GetURL("a.com", "/simple.html");
  RunTestSequence(InstrumentTab(kTestTab),
                  NavigateWebContents(kTestTab, simple_url),
                  CheckReaderPermission(/*has_permission=*/false),
                  BlockPermission(simple_url),
                  PollState(kPermissionDecision,
                            [this]() { return permission_decision(); }),
                  RequestReaderPermission(),
                  WaitForState(kPermissionDecision, /*granted=*/false),
                  CheckReaderPermission(/*has_permission=*/false));
}

}  // anonymous namespace

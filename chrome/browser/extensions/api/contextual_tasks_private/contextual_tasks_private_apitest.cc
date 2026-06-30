// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::NiceMock;
using ::testing::Return;

namespace extensions {

class MockContextualTasksEligibilityManager
    : public contextual_tasks::ContextualTasksEligibilityManager {
 public:
  MockContextualTasksEligibilityManager(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      AimEligibilityService* aim_eligibility_service,
      bool is_eligible)
      : contextual_tasks::ContextualTasksEligibilityManager(
            pref_service,
            identity_manager,
            aim_eligibility_service),
        stub_is_eligible_(is_eligible) {
    MaybeNotifyEligibilityChanged();
  }
  ~MockContextualTasksEligibilityManager() override = default;

  bool IsEligibleWithoutIdentity() const override { return stub_is_eligible_; }
  bool CalculateEligibility() const override { return stub_is_eligible_; }

 private:
  bool stub_is_eligible_;
};

class ContextualTasksPrivateApiTest : public ExtensionApiTest {
 public:
  ContextualTasksPrivateApiTest() = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    EXPECT_TRUE(StartEmbeddedTestServer());
  }

  void SetupMockUiService(bool is_eligible) {
    contextual_tasks::ContextualTasksUiServiceFactory::GetInstance()
        ->SetTestingFactory(
            profile(),
            base::BindOnce(
                [](bool is_eligible, content::BrowserContext* context)
                    -> std::unique_ptr<KeyedService> {
                  Profile* profile = Profile::FromBrowserContext(context);
                  auto mock_eligibility_manager =
                      std::make_unique<MockContextualTasksEligibilityManager>(
                          /*pref_service=*/nullptr,
                          /*identity_manager=*/nullptr,
                          /*aim_eligibility_service=*/nullptr, is_eligible);

                  auto mock_ui_service = std::make_unique<
                      NiceMock<contextual_tasks::MockContextualTasksUiService>>(
                      profile,
                      /*service=*/nullptr,
                      /*identity_manager=*/nullptr,
                      /*aim_eligibility_service=*/nullptr,
                      std::move(mock_eligibility_manager),
                      /*cookie_synchronizer=*/nullptr);

                  ON_CALL(*mock_ui_service, GetEligibilityManager())
                      .WillByDefault([service_ptr = mock_ui_service.get()]() {
                        return service_ptr
                            ->ContextualTasksUiService::GetEligibilityManager();
                      });
                  ON_CALL(*mock_ui_service, IsAiUrl(testing::_))
                      .WillByDefault(Return(true));
                  ON_CALL(*mock_ui_service, IsSearchResultsUrl(testing::_))
                      .WillByDefault(Return(true));
                  ON_CALL(*mock_ui_service, GetDefaultAiPageUrl())
                      .WillByDefault(Return(GURL("https://google.com/aim")));

                  return std::unique_ptr<KeyedService>(
                      std::move(mock_ui_service));
                },
                is_eligible));
  }

  NiceMock<contextual_tasks::MockContextualTasksUiService>* GetMockUiService() {
    return static_cast<
        NiceMock<contextual_tasks::MockContextualTasksUiService>*>(
        contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
            profile()));
  }

 protected:
  base::test::ScopedFeatureList feature_list_{
      extensions_features::kApiContextualTasksPrivate};
};

class ContextualTasksPrivateApiEligibleTest
    : public ContextualTasksPrivateApiTest {
 public:
  void SetUpOnMainThread() override {
    ContextualTasksPrivateApiTest::SetUpOnMainThread();
    SetupMockUiService(/*is_eligible=*/true);
  }
};

IN_PROC_BROWSER_TEST_F(ContextualTasksPrivateApiEligibleTest, GetState) {
  EXPECT_TRUE(RunExtensionTest(
      "contextual_tasks_private",
      {.extension_url = "test.html", .custom_arg = "getstate_eligible"},
      {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPrivateApiEligibleTest,
                       LaunchPanelInNewTab) {
  auto* mock_ui_service = GetMockUiService();

  EXPECT_CALL(
      *mock_ui_service,
      StartTaskUiInSidePanel(
          testing::_, testing::_,
          testing::Property(
              &GURL::spec,
              testing::AllOf(
                  testing::HasSubstr("/aim"), testing::HasSubstr("ntc=1"),
                  testing::HasSubstr("mstk=abc"), testing::HasSubstr("aioh=1"),
                  testing::HasSubstr("csuir=1"), testing::HasSubstr("ved=123"),
                  testing::HasSubstr("cs=1"), testing::HasSubstr("sxsrf=xyz"),
                  testing::HasSubstr("ei=456"))),
          testing::_, /*associate_web_contents=*/false, testing::_))
      .Times(testing::AtLeast(1));

  EXPECT_TRUE(RunExtensionTest(
      "contextual_tasks_private",
      {.extension_url = "test.html", .custom_arg = "launch_panel_eligible"},
      {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPrivateApiEligibleTest,
                       LaunchPanelInNewTabInvalidTargetUrl) {
  auto* mock_ui_service = GetMockUiService();

  EXPECT_CALL(*mock_ui_service,
              StartTaskUiInSidePanel(testing::_, testing::_, testing::_,
                                     testing::_, testing::_, testing::_))
      .Times(0);

  EXPECT_TRUE(
      RunExtensionTest("contextual_tasks_private",
                       {.extension_url = "test.html",
                        .custom_arg = "launch_panel_invalid_target_url"},
                       {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPrivateApiEligibleTest,
                       LaunchPanelInNewTabPopupWindow) {
  auto* mock_ui_service = GetMockUiService();

  EXPECT_CALL(*mock_ui_service,
              StartTaskUiInSidePanel(testing::_, testing::_, testing::_,
                                     testing::_, testing::_, testing::_))
      .Times(0);

  EXPECT_TRUE(RunExtensionTest(
      "contextual_tasks_private",
      {.extension_url = "test.html", .custom_arg = "launch_panel_popup_window"},
      {.load_as_component = true}))
      << message_;
}

class ContextualTasksPrivateApiIneligibleTest
    : public ContextualTasksPrivateApiTest {
 public:
  void SetUpOnMainThread() override {
    ContextualTasksPrivateApiTest::SetUpOnMainThread();
    SetupMockUiService(/*is_eligible=*/false);
  }
};

IN_PROC_BROWSER_TEST_F(ContextualTasksPrivateApiIneligibleTest, GetState) {
  EXPECT_TRUE(RunExtensionTest(
      "contextual_tasks_private",
      {.extension_url = "test.html", .custom_arg = "getstate_ineligible"},
      {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPrivateApiIneligibleTest,
                       LaunchPanelInNewTab) {
  EXPECT_TRUE(RunExtensionTest(
      "contextual_tasks_private",
      {.extension_url = "test.html", .custom_arg = "launch_panel_ineligible"},
      {.load_as_component = true}))
      << message_;
}

}  // namespace extensions

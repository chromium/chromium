// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_features.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"

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
  ContextualTasksPrivateApiTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{extensions_features::kApiContextualTasksPrivate},
        /*disabled_features=*/{});
  }

  explicit ContextualTasksPrivateApiTest(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features = {}) {
    std::vector<base::test::FeatureRef> enabled = {
        extensions_features::kApiContextualTasksPrivate};
    enabled.insert(enabled.end(), enabled_features.begin(),
                   enabled_features.end());
    feature_list_.InitWithFeatures(enabled, disabled_features);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    EXPECT_TRUE(StartEmbeddedTestServer());
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    ExtensionApiTest::SetUpBrowserContextKeyedServices(context);

    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          Profile* profile = Profile::FromBrowserContext(context);
          auto mock_aim_service =
              std::make_unique<NiceMock<MockAimEligibilityService>>(
                  *profile->GetPrefs(), /*template_url_service=*/nullptr,
                  /*url_loader_factory=*/nullptr,
                  /*identity_manager=*/nullptr);
          ON_CALL(*mock_aim_service, IsAimEligible())
              .WillByDefault(Return(true));
          ON_CALL(*mock_aim_service, IsAimUrl(testing::_, testing::_))
              .WillByDefault(Return(true));
          return mock_aim_service;
        }));

    contextual_tasks::ContextualTasksUiServiceFactory::GetInstance()
        ->SetTestingFactory(
            context,
            base::BindRepeating(
                [](bool is_eligible, content::BrowserContext* context)
                    -> std::unique_ptr<KeyedService> {
                  Profile* profile = Profile::FromBrowserContext(context);
                  auto* aim_eligibility_service =
                      AimEligibilityServiceFactory::GetForProfile(profile);

                  auto mock_eligibility_manager =
                      std::make_unique<MockContextualTasksEligibilityManager>(
                          /*pref_service=*/nullptr,
                          /*identity_manager=*/nullptr, aim_eligibility_service,
                          is_eligible);

                  auto mock_ui_service = std::make_unique<
                      NiceMock<contextual_tasks::MockContextualTasksUiService>>(
                      profile,
                      /*service=*/nullptr,
                      /*identity_manager=*/nullptr, aim_eligibility_service,
                      std::move(mock_eligibility_manager),
                      /*cookie_synchronizer=*/nullptr);

                  ON_CALL(*mock_ui_service, GetEligibilityManager())
                      .WillByDefault([service_ptr = mock_ui_service.get()]() {
                        return service_ptr
                            ->ContextualTasksUiService::GetEligibilityManager();
                      });
                  ON_CALL(*mock_ui_service, IsAiUrl(testing::_))
                      .WillByDefault([service_ptr = mock_ui_service.get()](
                                         const GURL& url) {
                        return service_ptr->ContextualTasksUiService::IsAiUrl(
                            url);
                      });
                  ON_CALL(*mock_ui_service, IsSearchResultsUrl(testing::_))
                      .WillByDefault(Return(true));
                  ON_CALL(*mock_ui_service, GetDefaultAiPageUrl())
                      .WillByDefault(Return(GURL("https://google.com/aim")));

                  return std::unique_ptr<KeyedService>(
                      std::move(mock_ui_service));
                },
                IsEligible()));
  }

  virtual bool IsEligible() const { return true; }

  NiceMock<contextual_tasks::MockContextualTasksUiService>* GetMockUiService() {
    return static_cast<
        NiceMock<contextual_tasks::MockContextualTasksUiService>*>(
        contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
            profile()));
  }

  MockAimEligibilityService* GetMockAimEligibilityService() {
    return static_cast<MockAimEligibilityService*>(
        AimEligibilityServiceFactory::GetForProfile(profile()));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

class ContextualTasksPrivateApiEligibleTest
    : public ContextualTasksPrivateApiTest {
 public:
  ContextualTasksPrivateApiEligibleTest()
      : ContextualTasksPrivateApiTest(
            {contextual_tasks::kContextualTasksSearchQuery}) {}
  bool IsEligible() const override { return true; }
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
      StartTaskUiInSidePanelImpl(
          testing::_, testing::_,
          testing::Property(
              &GURL::spec,
              testing::AllOf(
                  testing::HasSubstr("/aim"), testing::HasSubstr("ntc=1"),
                  testing::HasSubstr("mstk=abc"), testing::HasSubstr("aioh=1"),
                  testing::HasSubstr("csuir=1"), testing::HasSubstr("ved=123"),
                  testing::HasSubstr("cs=1"), testing::HasSubstr("sxsrf=xyz"),
                  testing::HasSubstr("ei=456"), testing::HasSubstr("aep=173"),
                  testing::HasSubstr("q=some_query"))),
          testing::_,
          testing::AllOf(
              testing::Field(
                  &contextual_tasks::StartTaskUiOptions::associate_web_contents,
                  false),
              testing::Field(&contextual_tasks::StartTaskUiOptions::entry_point,
                             omnibox::ChromeAimEntryPoint::
                                 DESKTOP_CHROME_COBROWSE_AIO_LINK),
              testing::Field(&contextual_tasks::StartTaskUiOptions::
                                 use_mstk_for_task_association,
                             true),
              testing::Field(
                  &contextual_tasks::StartTaskUiOptions::use_no_animation,
                  false))))
      .Times(testing::AtLeast(1));

  EXPECT_TRUE(RunExtensionTest(
      "contextual_tasks_private",
      {.extension_url = "test.html", .custom_arg = "launch_panel_eligible"},
      {.load_as_component = true}))
      << message_;
}

class ContextualTasksPrivateApiEligibleDisabledTest
    : public ContextualTasksPrivateApiTest {
 public:
  ContextualTasksPrivateApiEligibleDisabledTest()
      : ContextualTasksPrivateApiTest(
            /*enabled_features=*/{},
            /*disabled_features=*/{
                contextual_tasks::kContextualTasksSearchQuery}) {}
  bool IsEligible() const override { return true; }
};

IN_PROC_BROWSER_TEST_F(ContextualTasksPrivateApiEligibleDisabledTest,
                       LaunchPanelInNewTabWithoutQ) {
  auto* mock_ui_service = GetMockUiService();

  EXPECT_CALL(
      *mock_ui_service,
      StartTaskUiInSidePanelImpl(
          testing::_, testing::_,
          testing::Property(
              &GURL::spec,
              testing::AllOf(
                  testing::HasSubstr("/aim"), testing::HasSubstr("ntc=1"),
                  testing::HasSubstr("mstk=abc"), testing::HasSubstr("aioh=1"),
                  testing::HasSubstr("csuir=1"), testing::HasSubstr("ved=123"),
                  testing::HasSubstr("cs=1"), testing::HasSubstr("sxsrf=xyz"),
                  testing::HasSubstr("ei=456"), testing::HasSubstr("aep=173"),
                  testing::Not(testing::HasSubstr("q=")))),
          testing::_,
          testing::AllOf(
              testing::Field(
                  &contextual_tasks::StartTaskUiOptions::associate_web_contents,
                  false),
              testing::Field(&contextual_tasks::StartTaskUiOptions::entry_point,
                             omnibox::ChromeAimEntryPoint::
                                 DESKTOP_CHROME_COBROWSE_AIO_LINK),
              testing::Field(&contextual_tasks::StartTaskUiOptions::
                                 use_mstk_for_task_association,
                             true),
              testing::Field(
                  &contextual_tasks::StartTaskUiOptions::use_no_animation,
                  false))))
      .Times(testing::AtLeast(1));

  EXPECT_TRUE(RunExtensionTest(
      "contextual_tasks_private",
      {.extension_url = "test.html", .custom_arg = "launch_panel_eligible"},
      {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPrivateApiEligibleTest,
                       LaunchPanelInNewTabMissingMstk) {
  auto* mock_ui_service = GetMockUiService();

  EXPECT_CALL(*mock_ui_service,
              StartTaskUiInSidePanelImpl(testing::_, testing::_, testing::_,
                                         testing::_, testing::_))
      .Times(0);

  EXPECT_TRUE(RunExtensionTest(
      "contextual_tasks_private",
      {.extension_url = "test.html", .custom_arg = "launch_panel_missing_mstk"},
      {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPrivateApiEligibleTest,
                       LaunchPanelInNewTabInvalidTargetUrl) {
  auto* mock_ui_service = GetMockUiService();

  EXPECT_CALL(*mock_ui_service,
              StartTaskUiInSidePanelImpl(testing::_, testing::_, testing::_,
                                         testing::_, testing::_))
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
  auto* mock_aim_service = GetMockAimEligibilityService();
  ON_CALL(*mock_aim_service, IsAimUrl(testing::_, testing::_))
      .WillByDefault(Return(false));

  auto* mock_ui_service = GetMockUiService();

  EXPECT_CALL(*mock_ui_service,
              StartTaskUiInSidePanelImpl(testing::_, testing::_, testing::_,
                                         testing::_, testing::_))
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
  bool IsEligible() const override { return false; }
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

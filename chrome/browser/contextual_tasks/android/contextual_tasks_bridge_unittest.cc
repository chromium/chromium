// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/android/contextual_tasks_bridge.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/test/bind.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace contextual_tasks {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {

std::unique_ptr<KeyedService> BuildMockAimEligibilityService(
    content::BrowserContext* context) {
  auto* profile = Profile::FromBrowserContext(context);
  return std::make_unique<NiceMock<MockAimEligibilityService>>(
      *profile->GetPrefs(),
      /*template_url_service=*/nullptr,
      /*url_loader_factory=*/nullptr,
      /*identity_manager=*/nullptr, AimEligibilityService::Configuration{});
}

std::unique_ptr<KeyedService> BuildMockContextualTasksService(
    content::BrowserContext* context) {
  return std::make_unique<NiceMock<MockContextualTasksService>>();
}

}  // namespace

class ContextualTasksBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        AimEligibilityServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockAimEligibilityService));
    profile_builder.AddTestingFactory(
        ContextualTasksServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockContextualTasksService));
    profile_ = profile_builder.Build();

    auto* contextual_tasks_service =
        ContextualTasksServiceFactory::GetForProfile(profile_.get());

    auto mock_ui_service = std::make_unique<
        testing::NiceMock<contextual_tasks::MockContextualTasksUiService>>(
        profile_.get(), contextual_tasks_service,
        /*identity_manager=*/nullptr,
        /*aim_eligibility_service=*/nullptr,
        /*eligibility_manager=*/nullptr,
        /*cookie_synchronizer=*/nullptr);

    ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindLambdaForTesting([service = std::move(mock_ui_service)](
                                       content::BrowserContext* context) mutable
                                       -> std::unique_ptr<KeyedService> {
          return std::move(service);
        }));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  ui::UnownedUserDataHost unowned_user_data_host_;
};

TEST_F(ContextualTasksBridgeTest, InitPopulatesUserData) {
  NiceMock<MockBrowserWindowInterface> mock_browser;
  NiceMock<MockTabListInterface> mock_tab_list;
  ON_CALL(mock_browser, GetProfile()).WillByDefault(Return(profile_.get()));
  ON_CALL(mock_browser, GetUnownedUserDataHost())
      .WillByDefault(ReturnRef(unowned_user_data_host_));

  // Register mock tab list in the host.
  ui::ScopedUnownedUserData<TabListInterface> tab_list_registration(
      unowned_user_data_host_, mock_tab_list);

  // Initially, none of the window-scoped features should be in the host.
  EXPECT_EQ(nullptr,
            ContextualTasksSidePanelCoordinator::Get(unowned_user_data_host_));
  EXPECT_EQ(nullptr,
            EntryPointEligibilityManager::Get(unowned_user_data_host_));
  EXPECT_EQ(nullptr, ActiveTaskContextProvider::Get(unowned_user_data_host_));

  // Instantiate the bridge.
  JNIEnv* env = base::android::AttachCurrentThread();
  ContextualTasksBridge bridge(env,
                               base::android::ScopedJavaLocalRef<jobject>(),
                               &mock_browser, profile_.get());

  // After bridge construction, the features should be present in the host.
  EXPECT_NE(nullptr,
            ContextualTasksSidePanelCoordinator::Get(unowned_user_data_host_));
  EXPECT_NE(nullptr,
            EntryPointEligibilityManager::Get(unowned_user_data_host_));
  EXPECT_NE(nullptr, ActiveTaskContextProvider::Get(unowned_user_data_host_));

  // Verify Get() works.
  EXPECT_EQ(&bridge, ContextualTasksBridge::From(&mock_browser));
}

}  // namespace contextual_tasks

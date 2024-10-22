// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/boca/boca_manager.h"
#include "chrome/browser/ash/boca/boca_manager_factory.h"
#include "chrome/browser/ash/system_web_apps/apps/boca_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace {

class MockBocaSessionObserver : public ash::boca::BocaSessionManager::Observer {
 public:
  MOCK_METHOD(void,
              OnSessionStarted,
              (const std::string& session_id,
               const boca::UserIdentity& producer),
              (override));

  MOCK_METHOD(void,
              OnSessionEnded,
              (const std::string& session_id),
              (override));

  MOCK_METHOD(void, OnAppReloaded, (), (override));
};

class BocaAppIntegrationTest : public ash::SystemWebAppIntegrationTest {
 protected:
  void SetUpOnMainThread() override {
    SystemWebAppIntegrationTest::SetUpOnMainThread();
    WaitForTestSystemAppInstall();

    // Register mock observer for testing purposes.
    ash::BocaManager* const boca_manager =
        ash::BocaManagerFactory::GetInstance()->GetForProfile(profile());
    boca_manager->GetBocaSessionManager()->AddObserver(session_observer());
  }

  void LaunchAndWait() {
    content::TestNavigationObserver observer(
        (GURL(ash::boca::kChromeBocaAppUntrustedIndexURL)));
    observer.StartWatchingNewWebContents();
    ash::LaunchSystemWebAppAsync(profile(), ash::SystemWebAppType::BOCA);
    observer.Wait();
  }

  MockBocaSessionObserver* session_observer() { return &session_observer_; }

 private:
  MockBocaSessionObserver session_observer_;
};

class BocaAppProviderIntegrationTest : public BocaAppIntegrationTest {
 protected:
  BocaAppProviderIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kBoca},
        /*disabled_features=*/{ash::features::kBocaConsumer});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(BocaAppProviderIntegrationTest,
                       ShouldNotNotifyReloadOnLaunch) {
  EXPECT_CALL(*session_observer(), OnAppReloaded).Times(0);
  LaunchAndWait();
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    BocaAppProviderIntegrationTest);

class BocaAppConsumerIntegrationTest : public BocaAppIntegrationTest {
 protected:
  BocaAppConsumerIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kBoca,
                              ash::features::kBocaConsumer},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(BocaAppConsumerIntegrationTest,
                       ShouldNotifyReloadOnLaunch) {
  EXPECT_CALL(*session_observer(), OnAppReloaded).Times(1);
  LaunchAndWait();
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    BocaAppConsumerIntegrationTest);

}  // namespace

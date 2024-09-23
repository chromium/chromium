// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_METRICS_SERVICE_TEST_BASE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_METRICS_SERVICE_TEST_BASE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace apps {

// Helper structure for creating apps in unit tests.
struct TestApp {
  std::string app_id;
  AppType app_type;
  std::string publisher_id;
  Readiness readiness;
  InstallReason install_reason;
  InstallSource install_source;
  bool should_notify_initialized;
  bool is_platform_app;
  WindowMode window_mode = WindowMode::kUnknown;

  TestApp(std::string app_id,
          AppType app_type,
          std::string publisher_id,
          Readiness readiness,
          InstallReason install_reason,
          InstallSource install_source,
          bool should_notify_initialized = true,
          bool is_platform_app = false,
          WindowMode window_mode = WindowMode::kUnknown);

  TestApp();
  TestApp(const TestApp& other);
  TestApp(TestApp&& other);
};

// Helper method that creates an app object so it can be used with the
// `AppRegistryCache`.
AppPtr MakeApp(TestApp app);

// Helper method that adds a new app using the provided app metadata with the
// `AppRegistryCache`.
void AddApp(AppServiceProxy* proxy, TestApp app);

// Base class that performs appropriate test setup for tests that involve app
// platform metric collection. Also facilitates tests to simulate app
// installation and usage.
class AppPlatformMetricsServiceTestBase : public ::testing::Test {
 public:
  AppPlatformMetricsServiceTestBase();
  ~AppPlatformMetricsServiceTestBase() override;

  // ::testing::Test:
  void SetUp() override;

  // ::testing::Test:
  void TearDown() override;

  // Installs an app with specified metadata.
  void InstallOneApp(const std::string& app_id,
                     AppType app_type,
                     const std::string& publisher_id,
                     Readiness readiness,
                     InstallSource install_source,
                     bool is_platform_app = false,
                     WindowMode window_mode = WindowMode::kUnknown,
                     InstallReason install_reason = InstallReason::kUser);
  void InstallOneApp(TestApp app);

  // Clears and restarts the `AppPlatformMetricsService` for the test profile.
  void ResetAppPlatformMetricsService();

  // Modifies the app and window instance with specified state.
  void ModifyInstance(const std::string& app_id,
                      aura::Window* window,
                      InstanceState state);

  // Updates the app and window instance with the specified state for the
  // given instance.
  void ModifyInstance(const base::UnguessableToken& instance_id,
                      const std::string& app_id,
                      aura::Window* window,
                      InstanceState state);

  // Modifies webapp instance with the specified state.
  void ModifyWebAppInstance(const std::string& app_id,
                            aura::Window* window,
                            InstanceState state);

  // Returns pref service for the test profile.
  sync_preferences::TestingPrefServiceSyncable* GetPrefService();

  // Transfers ownership of the `AppPlatformMetricsService` instance for the
  // test profile.
  std::unique_ptr<AppPlatformMetricsService> GetAppPlatformMetricsService();

  // Returns the saved day_id from the pref store.
  int GetDayIdPref();

  // Returns a pointer to the `AppPlatformMetricsService` for the test profile.
  AppPlatformMetricsService* app_platform_metrics_service() {
    return app_platform_metrics_service_.get();
  }

  // Returns a pointer to the test profile.
  TestingProfile* profile() { return testing_profile_.get(); }

  // Returns a pointer to the sync service.
  syncer::TestSyncService* sync_service() { return sync_service_; }

  // Returns a pointer to the `HistogramTester` for testing purposes.
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  // Returns a pointer to the `TestAutoSetUkmRecorder` for testing purposes.
  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

  // Creates test user and profile with the specified email for testing
  // purposes.
  void AddRegularUser(const std::string& email);

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};

 private:
  std::unique_ptr<TestingProfile> testing_profile_;
  raw_ptr<syncer::TestSyncService> sync_service_ = nullptr;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<AppPlatformMetricsService> app_platform_metrics_service_;
  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> fake_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  apps::AppServiceTest app_service_test_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_METRICS_SERVICE_TEST_BASE_H_

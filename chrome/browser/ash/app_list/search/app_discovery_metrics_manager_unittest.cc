// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/app_discovery_metrics_manager.h"

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "components/metrics/structured/test/test_structured_metrics_provider.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;

std::unique_ptr<KeyedService> TestingSyncFactoryFunction(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

// Impl for testing structured metrics that forwards all writes to the recorder
// directly.
class TestRecorder
    : public metrics::structured::StructuredMetricsClient::RecordingDelegate {
 public:
  TestRecorder() {
    metrics::structured::StructuredMetricsClient::Get()->SetDelegate(this);
  }

  bool IsReadyToRecord() const override { return true; }

  void RecordEvent(metrics::structured::Event&& event) override {
    metrics::structured::Recorder::GetInstance()->RecordEvent(std::move(event));
  }
};

class TestSearchResult : public ChromeSearchResult {
 public:
  TestSearchResult(const std::string& id,
                   const std::u16string& title,
                   MetricsType metrics_type) {
    set_id(id);
    SetTitle(title);
    SetMetricsType(metrics_type);
  }

  TestSearchResult(const TestSearchResult&) = delete;
  TestSearchResult& operator=(const TestSearchResult&) = delete;

  ~TestSearchResult() override {}

  // ChromeSearchResult overrides:
  void Open(int event_flags) override {}
};

}  // namespace

class AppDiscoveryMetricsManagerTest : public testing::Test {
 public:
  void SetUp() override {
    test_recorder_ = std::make_unique<TestRecorder>();
    test_structured_metrics_provider_ =
        std::make_unique<metrics::structured::TestStructuredMetricsProvider>();
    test_structured_metrics_provider_->EnableRecording();
    metrics::structured::Recorder::GetInstance()->SetUiTaskRunner(
        task_environment_.GetMainThreadTaskRunner());

    TestingProfile::Builder builder;
    builder.AddTestingFactory(TrustedVaultServiceFactory::GetInstance(),
                              TrustedVaultServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              SyncServiceFactory::GetDefaultFactory());
    testing_profile_ = builder.Build();

    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            testing_profile_.get(),
            base::BindRepeating(&TestingSyncFactoryFunction)));

    app_discovery_metrics_ =
        std::make_unique<AppDiscoveryMetricsManager>(testing_profile_.get());
  }

  void TearDown() override {
    metrics::structured::StructuredMetricsClient::Get()->UnsetDelegate();
  }

  metrics::structured::TestStructuredMetricsProvider*
  test_structured_metrics_provider() {
    return test_structured_metrics_provider_.get();
  }

  void ValidateAppLaunchEvent(const metrics::structured::Event& event,
                              const std::string& app_id,
                              const std::u16string& app_title,
                              double match_score,
                              const ash::SearchResultType search_result_type) {
    cros_events::AppDiscovery_AppLauncherResultOpened expected_event;

    EXPECT_EQ(expected_event.project_name(), event.project_name());
    EXPECT_EQ(expected_event.event_name(), event.event_name());

    expected_event.SetAppId(app_id)
        .SetAppName(std::string(app_title.begin(), app_title.end()))
        .SetFuzzyStringMatch(match_score)
        .SetResultCategory(search_result_type);

    EXPECT_EQ(expected_event.metric_values(), event.metric_values());
  }

  void ValidateLauncherOpenEvent(const metrics::structured::Event& event) {
    cros_events::AppDiscovery_LauncherOpen expected_event;

    EXPECT_EQ(expected_event.project_name(), event.project_name());
    EXPECT_EQ(expected_event.event_name(), event.event_name());
  }

  TestingProfile* profile() { return testing_profile_.get(); }

  syncer::TestSyncService* sync_service() { return sync_service_; }

  AppDiscoveryMetricsManager* app_discovery_metrics() {
    return app_discovery_metrics_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> testing_profile_;
  raw_ptr<syncer::TestSyncService> sync_service_ = nullptr;
  std::unique_ptr<AppDiscoveryMetricsManager> app_discovery_metrics_;

  std::unique_ptr<TestRecorder> test_recorder_;
  std::unique_ptr<metrics::structured::TestStructuredMetricsProvider>
      test_structured_metrics_provider_;
};

TEST_F(AppDiscoveryMetricsManagerTest, OnOpenAppResult) {
  const std::u16string app_name = u"app_name";
  const std::u16string query = u"app_name";
  const std::string app_id = "id";
  const auto search_result_type =
      ash::SearchResultType::PLAY_STORE_UNINSTALLED_APP;

  TestSearchResult search_result(app_id, app_name, search_result_type);

  base::RunLoop run_loop;
  auto record_callback = base::BindLambdaForTesting(
      [&, this](const metrics::structured::Event& event) {
        ValidateAppLaunchEvent(event, app_id, app_name, /*match_score=*/1.0,
                               search_result_type);
        run_loop.Quit();
      });
  test_structured_metrics_provider()->SetOnEventsRecordClosure(record_callback);

  app_discovery_metrics()->OnOpenResult(&search_result, query);
  run_loop.Run();
}

TEST_F(AppDiscoveryMetricsManagerTest, OnOpenAppResultAppSyncDisabled) {
  const std::u16string app_name = u"app_name";
  const std::u16string query = u"app_name";
  const std::string app_id = "id";
  const auto search_result_type =
      ash::SearchResultType::PLAY_STORE_UNINSTALLED_APP;

  // Disable app-sync.
  sync_service()->SetAllowedByEnterprisePolicy(false);

  TestSearchResult search_result(app_id, app_name, search_result_type);

  base::RunLoop run_loop;
  auto record_callback = base::BindLambdaForTesting(
      [&, this](const metrics::structured::Event& event) {
        // App ID and app name will be stripped if app sync is disabled.
        ValidateAppLaunchEvent(event, /*app_id=*/"", /*app_title=*/u"",
                               /*match_score=*/1.0, search_result_type);
        run_loop.Quit();
      });
  test_structured_metrics_provider()->SetOnEventsRecordClosure(record_callback);

  app_discovery_metrics()->OnOpenResult(&search_result, query);
  run_loop.Run();
}

TEST_F(AppDiscoveryMetricsManagerTest, OnLauncherOpen) {
  base::RunLoop run_loop;
  auto record_callback = base::BindLambdaForTesting(
      [&, this](const metrics::structured::Event& event) {
        ValidateLauncherOpenEvent(event);
        run_loop.Quit();
      });
  test_structured_metrics_provider()->SetOnEventsRecordClosure(record_callback);

  app_discovery_metrics()->OnLauncherOpen();
  run_loop.Run();
}

}  // namespace app_list

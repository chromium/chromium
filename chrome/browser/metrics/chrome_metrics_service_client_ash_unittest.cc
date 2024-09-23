// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "base/memory/raw_ptr.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"

#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client_impl.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/metrics/log_decoder.h"
#include "components/metrics/metrics_logs_event_manager.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/metrics/unsent_log_store.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/ukm_pref_names.h"
#include "components/ukm/ukm_service.h"
#include "components/ukm/unsent_log_store_metrics_impl.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/variations/synthetic_trial_registry.h"
#include "content/public/test/browser_task_environment.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/ukm/report.pb.h"

namespace {
using TestEvent1 = ukm::builders::PageLoad;

// Needed to fake System Profile which is provided when ukm report is generated.
// TODO(crbug.com/40249492): Refactor to remove the classes needed to fake
// SystemProfile.
class FakeMultiDeviceSetupClientImplFactory
    : public ash::multidevice_setup::MultiDeviceSetupClientImpl::Factory {
 public:
  explicit FakeMultiDeviceSetupClientImplFactory(
      std::unique_ptr<ash::multidevice_setup::FakeMultiDeviceSetupClient>
          fake_multidevice_setup_client)
      : fake_multidevice_setup_client_(
            std::move(fake_multidevice_setup_client)) {}

  ~FakeMultiDeviceSetupClientImplFactory() override = default;

  // ash::multidevice_setup::MultiDeviceSetupClientImpl::Factory:
  std::unique_ptr<ash::multidevice_setup::MultiDeviceSetupClient>
  CreateInstance(
      mojo::PendingRemote<ash::multidevice_setup::mojom::MultiDeviceSetup>)
      override {
    // NOTE: At most, one client should be created per-test.
    EXPECT_TRUE(fake_multidevice_setup_client_);
    return std::move(fake_multidevice_setup_client_);
  }

 private:
  std::unique_ptr<ash::multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
};

class ChromeMetricsServiceClientTestWithoutUKMProviders
    : public ChromeMetricsServiceClient {
 public:
  // Equivalent to ChromeMetricsServiceClient::Create
  static std::unique_ptr<ChromeMetricsServiceClientTestWithoutUKMProviders>
  Create(metrics::MetricsStateManager* metrics_state_manager,
         variations::SyntheticTrialRegistry* synthetic_trial_registry) {
    // Needed because RegisterMetricsServiceProviders() checks for this.
    metrics::SubprocessMetricsProvider::CreateInstance();

    std::unique_ptr<ChromeMetricsServiceClientTestWithoutUKMProviders> client(
        new ChromeMetricsServiceClientTestWithoutUKMProviders(
            metrics_state_manager, synthetic_trial_registry));
    client->Initialize();

    return client;
  }

  bool notified_not_idle() const { return notified_not_idle_; }

 private:
  explicit ChromeMetricsServiceClientTestWithoutUKMProviders(
      metrics::MetricsStateManager* state_manager,
      variations::SyntheticTrialRegistry* synthetic_trial_registry)
      : ChromeMetricsServiceClient(state_manager, synthetic_trial_registry) {}

  void RegisterUKMProviders() override {}
  void NotifyApplicationNotIdle() override { notified_not_idle_ = true; }

  bool notified_not_idle_ = false;
};

class MockSyncService : public syncer::TestSyncService {
 public:
  MockSyncService() {
    SetMaxTransportState(TransportState::INITIALIZING);
    SetLastCycleSnapshot(syncer::SyncCycleSnapshot());
  }

  MockSyncService(const MockSyncService&) = delete;
  MockSyncService& operator=(const MockSyncService&) = delete;

  ~MockSyncService() override { Shutdown(); }

  void SetStatus(bool has_passphrase, bool history_enabled, bool active) {
    SetMaxTransportState(active ? TransportState::ACTIVE
                                : TransportState::INITIALIZING);
    SetIsUsingExplicitPassphrase(has_passphrase);

    GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        /*types=*/history_enabled ? syncer::UserSelectableTypeSet(
                                        {syncer::UserSelectableType::kHistory})
                                  : syncer::UserSelectableTypeSet());

    // It doesn't matter what exactly we set here, it's only relevant that the
    // SyncCycleSnapshot is initialized at all.
    SetLastCycleSnapshot(syncer::SyncCycleSnapshot(
        /*birthday=*/std::string(), /*bag_of_chips=*/std::string(),
        syncer::ModelNeutralState(), syncer::ProgressMarkerMap(), false, 0,
        true, base::Time::Now(), base::Time::Now(),
        sync_pb::SyncEnums::UNKNOWN_ORIGIN, base::Minutes(1), false));

    NotifyObserversOfStateChanged();

    SetAppSync(false);
  }

  void Shutdown() override {
    for (auto& observer : observers_) {
      observer.OnSyncShutdown(this);
    }
  }

  void SetAppSync(bool enabled) {
    auto selected_os_types = GetUserSettings()->GetSelectedOsTypes();

    if (enabled)
      selected_os_types.Put(syncer::UserSelectableOsType::kOsApps);
    else
      selected_os_types.Remove(syncer::UserSelectableOsType::kOsApps);

    GetUserSettings()->SetSelectedOsTypes(false, selected_os_types);

    NotifyObserversOfStateChanged();
  }

 private:
  // syncer::TestSyncService:
  void AddObserver(syncer::SyncServiceObserver* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(syncer::SyncServiceObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

  void NotifyObserversOfStateChanged() {
    for (auto& observer : observers_) {
      observer.OnStateChanged(this);
    }
  }

  // The list of observers of the SyncService state.
  base::ObserverList<syncer::SyncServiceObserver>::Unchecked observers_;
};

struct IndependentAppMetricsTestParams {
  IndependentAppMetricsTestParams(ukm::UkmConsentType purged,
                                  ukm::UkmConsentType remaining)
      : tested_type(purged), other_type(remaining) {}

  ukm::UkmConsentType tested_type;
  ukm::UkmConsentType other_type;
};

}  // namespace

class ChromeMetricsServiceClientTestIgnoredForAppMetrics
    : public testing::TestWithParam<IndependentAppMetricsTestParams> {
 public:
  ChromeMetricsServiceClientTestIgnoredForAppMetrics()
      : profile_manager_(std::make_unique<TestingProfileManager>(
            TestingBrowserProcess::GetGlobal())),
        enabled_state_provider_(false /* consent */, false /* enabled */) {}

  void SetUp() override {
    testing::Test::SetUp();
    metrics::MetricsService::RegisterPrefs(prefs_.registry());
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &prefs_, &enabled_state_provider_, std::wstring(), base::FilePath());
    metrics_state_manager_->InstantiateFieldTrialList();
    synthetic_trial_registry_ =
        std::make_unique<variations::SyntheticTrialRegistry>();
    ASSERT_TRUE(profile_manager_->SetUp());
    scoped_feature_list_.InitAndEnableFeature(features::kUmaStorageDimensions);

    // ChromeOs Metrics Provider require g_login_state and power manager client
    // initialized before they can be instantiated.
    chromeos::PowerManagerClient::InitializeFake();
    ash::LoginState::Initialize();
    chromeos::TpmManagerClient::InitializeFake();

    SetupMultiDeviceFactory();

    testing_profile_ = profile_manager_->CreateTestingProfile("test_name");

    // Set statistic provider for hardware class tests.
    ash::system::StatisticsProvider::SetTestProvider(
        &fake_statistics_provider_);
  }

  void TearDown() override {
    ash::LoginState::Shutdown();
    chromeos::PowerManagerClient::Shutdown();

    ash::multidevice_setup::MultiDeviceSetupClientImpl::Factory::
        SetFactoryForTesting(nullptr);

    // ChromeMetricsServiceClient::Initialize() initializes
    // IdentifiabilityStudySettings as part of creating the
    // PrivacyBudgetUkmEntryFilter. Reset them after the test.
    blink::IdentifiabilityStudySettings::ResetStateForTesting();
    profile_manager_.reset();
  }

  std::unique_ptr<ChromeMetricsServiceClientTestWithoutUKMProviders> Init(
      sync_preferences::TestingPrefServiceSyncable& prefs) {
    ChromeMetricsServiceClient::RegisterPrefs(prefs.registry());
    RegisterUrlKeyedAnonymizedDataCollectionPref(prefs);
    SetUrlKeyedAnonymizedDataCollectionEnabled(prefs, /*enabled=*/true);

    auto chrome_metrics_service_client =
        ChromeMetricsServiceClientTestWithoutUKMProviders::Create(
            metrics_state_manager_.get(), synthetic_trial_registry_.get());
    chrome_metrics_service_client->StartObserving(&sync_service_, &prefs);

    chrome_metrics_service_client_ = chrome_metrics_service_client.get();

    auto* ukm_service = chrome_metrics_service_client_->GetUkmService();
    ukm_service->SetSamplingForTesting(true);
    ukm_service->EnableRecording();
    ukm_service->EnableReporting();

    return chrome_metrics_service_client;
  }

  ChromeMetricsServiceClient& GetChromeMetricsServiceClient() {
    return *chrome_metrics_service_client_;
  }

  ukm::UkmService* GetUkmService() {
    return chrome_metrics_service_client_->GetUkmService();
  }

  std::vector<ukm::SourceId> GetSourceIdsForConsentType(
      ukm::UkmConsentType consent_type) {
    const auto filter_id_type =
        GetAppOrNavigationSourceIdTypeForConsent(consent_type);
    std::vector<ukm::SourceId> result;

    for (const auto& source_id : source_ids_) {
      if (ukm::GetSourceIdType(source_id) == filter_id_type) {
        result.push_back(source_id);
      }
    }

    return result;
  }

  static ukm::SourceIdType GetAppOrNavigationSourceIdTypeForConsent(
      ukm::UkmConsentType consent_type) {
    return consent_type == ukm::UkmConsentType::APPS
               ? ukm::SourceIdType::APP_ID
               : ukm::SourceIdType::NAVIGATION_ID;
  }

  void RegisterUrlKeyedAnonymizedDataCollectionPref(
      sync_preferences::TestingPrefServiceSyncable& prefs) {
    unified_consent::UnifiedConsentService::RegisterPrefs(prefs.registry());
  }

  void SetUrlKeyedAnonymizedDataCollectionEnabled(
      sync_preferences::TestingPrefServiceSyncable& prefs,
      bool enabled) {
    prefs.SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        enabled);
  }

  ukm::Report GetUkmReport() {
    metrics::UnsentLogStore* log_store =
        GetUkmService()->reporting_service_for_testing().ukm_log_store();
    EXPECT_GE(log_store->size(), 1ul);
    log_store->StageNextLog();

    ukm::Report report;
    EXPECT_TRUE(
        metrics::DecodeLogDataToProto(log_store->staged_log(), &report));
    return report;
  }

 protected:
  void SetupMultiDeviceFactory() {
    ash::multidevice_setup::MultiDeviceSetupClientFactory::GetInstance()
        ->SetServiceIsNULLWhileTestingForTesting(false);
    auto fake_multidevice_setup_client =
        std::make_unique<ash::multidevice_setup::FakeMultiDeviceSetupClient>();
    fake_multidevice_setup_client_ = fake_multidevice_setup_client.get();
    fake_multidevice_setup_client_impl_factory_ =
        std::make_unique<FakeMultiDeviceSetupClientImplFactory>(
            std::move(fake_multidevice_setup_client));
    ash::multidevice_setup::MultiDeviceSetupClientImpl::Factory::
        SetFactoryForTesting(fake_multidevice_setup_client_impl_factory_.get());
  }

  ukm::SourceId AddSourceId(ukm::SourceIdType source_id_type) {
    const auto source_id =
        ukm::ConvertToSourceId(source_ids_.size(), source_id_type);
    UpdateSourceUrl(source_id, source_id_type);
    source_ids_.push_back(source_id);
    return source_id;
  }

  void UpdateSourceUrl(ukm::SourceId source_id,
                       ukm::SourceIdType source_id_type) {
    GetUkmService()->UpdateSourceURL(
        source_id,
        source_id_type == ukm::SourceIdType::APP_ID ? kAppURL : kURL);
  }

  void RecordTestEvent1(ukm::SourceIdType source_id_type) {
    const auto source_id = AddSourceId(source_id_type);
    TestEvent1(source_id).Record(GetUkmService());
  }

  GURL kURL = GURL("https://google.com/foobar");
  GURL kAppURL = GURL("app://google.com/foobar");

  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  base::UserActionTester user_action_runner_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;
  metrics::TestEnabledStateProvider enabled_state_provider_;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::vector<ukm::SourceId> source_ids_;
  raw_ptr<ChromeMetricsServiceClient, DanglingUntriaged>
      chrome_metrics_service_client_;

  MockSyncService sync_service_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  raw_ptr<TestingProfile, DanglingUntriaged> testing_profile_ = nullptr;
  raw_ptr<ash::multidevice_setup::FakeMultiDeviceSetupClient, DanglingUntriaged>
      fake_multidevice_setup_client_;
  std::unique_ptr<FakeMultiDeviceSetupClientImplFactory>
      fake_multidevice_setup_client_impl_factory_;
};

TEST_P(ChromeMetricsServiceClientTestIgnoredForAppMetrics,
       NotifyNotIdleOnUserActivity) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  auto chrome_metrics_service_client = Init(prefs);
  EXPECT_FALSE(chrome_metrics_service_client->notified_not_idle());

  ui::UserActivityDetector::Get()->HandleExternalUserActivity();
  EXPECT_TRUE(chrome_metrics_service_client->notified_not_idle());
}

TEST_P(ChromeMetricsServiceClientTestIgnoredForAppMetrics,
       VerifyPurgeOnConsentChange) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  auto chrome_metrics_service_client = Init(prefs);

  // Get the params for this test.
  const auto purged_consent = GetParam().tested_type;
  const auto remaining_consent = GetParam().other_type;

  auto ukm_consent_state = GetChromeMetricsServiceClient().GetUkmConsentState();

  EXPECT_TRUE(ukm_consent_state.Has(ukm::MSBB));
  EXPECT_TRUE(ukm_consent_state.Has(ukm::APPS));

  // Record a mix of SourceId's and Events.
  RecordTestEvent1(ukm::SourceIdType::NAVIGATION_ID);
  RecordTestEvent1(ukm::SourceIdType::NAVIGATION_ID);
  RecordTestEvent1(ukm::SourceIdType::APP_ID);
  RecordTestEvent1(ukm::SourceIdType::NAVIGATION_ID);
  RecordTestEvent1(ukm::SourceIdType::APP_ID);

  GetUkmService()->Flush(
      metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  // Remove the consent for |purged_consent|. This will cause
  // UKM metrics associated with this type to be purged.
  if (purged_consent == ukm::UkmConsentType::APPS)
    sync_service_.SetAppSync(false);
  else
    SetUrlKeyedAnonymizedDataCollectionEnabled(prefs, /*enabled=*/false);

  // Verify the update has propagated by checking the current consent state.
  ukm_consent_state = GetChromeMetricsServiceClient().GetUkmConsentState();

  EXPECT_TRUE(ukm_consent_state.Has(remaining_consent));
  EXPECT_FALSE(ukm_consent_state.Has(purged_consent));

  // Generate the ukm report to valid its contents.
  ukm::Report report = GetUkmReport();

  const auto remaining_source_ids =
      GetSourceIdsForConsentType(remaining_consent);
  const int num_elements = remaining_source_ids.size();

  // Expected |num_elements| entries and sources. Only |remaining_consent|
  // entries and sources should remain. Entries and sources for |purged_consent|
  // were purged.
  EXPECT_EQ(num_elements, report.sources_size());
  EXPECT_EQ(num_elements, report.entries_size());

  const auto expected_source_id_type =
      GetAppOrNavigationSourceIdTypeForConsent(remaining_consent);
  std::vector<ukm::SourceId> report_source_ids;

  // Verify the SourceIdType associated with |remaining_consent| are the only
  // entries and sources remaining.
  for (int i = 0; i < num_elements; ++i) {
    report_source_ids.push_back(report.entries(i).source_id());
    EXPECT_EQ(expected_source_id_type,
              ukm::GetSourceIdType(report.entries(i).source_id()));
  }

  EXPECT_THAT(report_source_ids,
              testing::UnorderedElementsAreArray(remaining_source_ids));
}

INSTANTIATE_TEST_SUITE_P(
    ChromeMetricsServiceClientTestIgnoredForAppMetricsGroup,
    ChromeMetricsServiceClientTestIgnoredForAppMetrics,
    testing::Values(IndependentAppMetricsTestParams(ukm::UkmConsentType::APPS,
                                                    ukm::UkmConsentType::MSBB),
                    IndependentAppMetricsTestParams(ukm::UkmConsentType::MSBB,
                                                    ukm::UkmConsentType::APPS)),
    [](const testing::TestParamInfo<
        ChromeMetricsServiceClientTestIgnoredForAppMetrics::ParamType>& info) {
      if (info.param.tested_type == ukm::UkmConsentType::APPS)
        return "TestApps";
      else
        return "TestMSBB";
    });

TEST_P(ChromeMetricsServiceClientTestIgnoredForAppMetrics,
       VerifyRecordingWhenConsentAdded) {
  // The consent type that will be off. Events associated with this type will be
  // ignored when events are initially recorded.
  const auto ignored_consent = GetParam().tested_type;

  // The consent type that will remain on. Events associated with this type will
  // be successfully recorded.
  const auto existing_consent = GetParam().other_type;

  sync_preferences::TestingPrefServiceSyncable prefs;
  auto chrome_metrics_service_client = Init(prefs);

  // Make sure the consents are set as expected for the test.
  SetUrlKeyedAnonymizedDataCollectionEnabled(
      prefs, existing_consent == ukm::UkmConsentType::MSBB);
  sync_service_.SetAppSync(existing_consent == ukm::UkmConsentType::APPS);

  auto ukm_consent_state = chrome_metrics_service_client->GetUkmConsentState();

  EXPECT_TRUE(ukm_consent_state.Has(existing_consent));
  EXPECT_FALSE(ukm_consent_state.Has(ignored_consent));

  // Record a mix of SourceId's and Events.
  RecordTestEvent1(ukm::SourceIdType::NAVIGATION_ID);
  RecordTestEvent1(ukm::SourceIdType::NAVIGATION_ID);
  RecordTestEvent1(ukm::SourceIdType::APP_ID);
  RecordTestEvent1(ukm::SourceIdType::NAVIGATION_ID);
  RecordTestEvent1(ukm::SourceIdType::APP_ID);

  // Turn on the ignored consent type and make sure the events are
  // recorded.
  if (ignored_consent == ukm::UkmConsentType::APPS)
    sync_service_.SetAppSync(true);
  else
    SetUrlKeyedAnonymizedDataCollectionEnabled(prefs, /*enabled=*/true);

  ukm_consent_state = chrome_metrics_service_client->GetUkmConsentState();

  // Verify the both consents are granted.
  EXPECT_TRUE(ukm_consent_state.Has(existing_consent));
  EXPECT_TRUE(ukm_consent_state.Has(ignored_consent));

  // Re-add events and sources of ignored type.
  auto ignored_ids = GetSourceIdsForConsentType(ignored_consent);
  for (auto id : ignored_ids) {
    UpdateSourceUrl(id,
                    GetAppOrNavigationSourceIdTypeForConsent(ignored_consent));
    TestEvent1(id).Record(GetUkmService());
  }

  GetUkmService()->Flush(
      metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  // Build UKM report to verify that all of the events and sources have been
  // recorded.
  ukm::Report report = GetUkmReport();

  // Expect that all events and sources originally recorded are present.
  EXPECT_EQ(report.sources_size(), static_cast<int>(source_ids_.size()));
  EXPECT_EQ(report.entries_size(), static_cast<int>(source_ids_.size()));

  // The source type of the events that were not ignored.
  const auto remaining_source_id_type =
      GetAppOrNavigationSourceIdTypeForConsent(existing_consent);
  const auto remaining_source_ids =
      GetSourceIdsForConsentType(existing_consent);

  // The source type of the events that were ignored.
  const auto added_source_id_type =
      GetAppOrNavigationSourceIdTypeForConsent(ignored_consent);
  const auto added_source_ids = GetSourceIdsForConsentType(ignored_consent);

  // Sources that were in the report.
  std::vector<ukm::SourceId> actual_source_ids;

  // Verify the sources ids are the expected value.
  // Events and sources associated with |existing_consent| will be first because
  // the events and sources associated with |ignored_consent| were
  // dropped/ignored when recorded the first time.
  for (size_t i = 0; i < remaining_source_ids.size(); ++i) {
    actual_source_ids.push_back(report.entries(i).source_id());
    EXPECT_EQ(remaining_source_id_type,
              ukm::GetSourceIdType(report.entries(i).source_id()));
  }

  // Verify that the source id's are of the expected type.
  EXPECT_THAT(actual_source_ids,
              testing::UnorderedElementsAreArray(remaining_source_ids));
  actual_source_ids.clear();

  // The events that were re-recorded once the tested consent was added are the
  // remaining elements.
  for (size_t i = 0; i < added_source_ids.size(); ++i) {
    const auto source_id =
        report.entries(i + remaining_source_ids.size()).source_id();
    actual_source_ids.push_back(source_id);
    EXPECT_EQ(added_source_id_type, ukm::GetSourceIdType(source_id));
  }

  // Verify that the source id's are of the expected type.
  EXPECT_THAT(actual_source_ids,
              testing::UnorderedElementsAreArray(added_source_ids));
}

class ChromeMetricsServiceClientTestDemoModeRecordAppMetrics
    : public ChromeMetricsServiceClientTestIgnoredForAppMetrics {
 public:
  ChromeMetricsServiceClientTestDemoModeRecordAppMetrics() = default;

  void SetUp() override {
    ChromeMetricsServiceClientTestIgnoredForAppMetrics::SetUp();
    ash::DemoSession::SetDemoConfigForTesting(
        ash::DemoSession::DemoModeConfig::kOnline);
    testing_profile_->ScopedCrosSettingsTestHelper()
        ->InstallAttributes()
        ->SetDemoMode();
  }

  void TearDown() override {
    ash::DemoSession::ResetDemoConfigForTesting();
    ChromeMetricsServiceClientTestIgnoredForAppMetrics::TearDown();
  }
};

TEST_F(ChromeMetricsServiceClientTestDemoModeRecordAppMetrics,
       VerifyRecordingInDemoSession) {
  sync_preferences::TestingPrefServiceSyncable prefs;
  auto chrome_metrics_service_client = Init(prefs);

  // Make sure the MSBB consent is set to false initially.
  SetUrlKeyedAnonymizedDataCollectionEnabled(prefs, false);

  // Make sure the APP consent is set to false for sync service.
  sync_service_.SetAppSync(false);

  auto ukm_consent_state = GetChromeMetricsServiceClient().GetUkmConsentState();

  // Assert that UKM consent state contains only APPS in DemoSession.
  EXPECT_FALSE(ukm_consent_state.Has(ukm::UkmConsentType::MSBB));
  EXPECT_TRUE(ukm_consent_state.Has(ukm::UkmConsentType::APPS));

  // Record a mix of SourceId's and Events.
  RecordTestEvent1(ukm::SourceIdType::NAVIGATION_ID);
  RecordTestEvent1(ukm::SourceIdType::NAVIGATION_ID);
  RecordTestEvent1(ukm::SourceIdType::APP_ID);
  RecordTestEvent1(ukm::SourceIdType::NAVIGATION_ID);
  RecordTestEvent1(ukm::SourceIdType::APP_ID);

  GetUkmService()->Flush(
      metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  // Build UKM report to verify that all of the events and sources have been
  // recorded for Demo Session.
  ukm::Report report = GetUkmReport();

  // Expect that only APP events and sources originally recorded are present.
  EXPECT_EQ(report.sources_size(), 2);
  EXPECT_EQ(report.entries_size(), 2);
}

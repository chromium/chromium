// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/optin/arc_terms_of_service_default_negotiator.h"

#include <memory>
#include <ostream>
#include <vector>

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "base/functional/bind.h"
#include "base/hash/sha1.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/arc_support_host.h"
#include "chrome/browser/ash/arc/extensions/fake_arc_support.h"
#include "chrome/browser/ash/arc/optin/arc_optin_preference_handler.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/consent_auditor/consent_auditor_test_utils.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/consent_auditor/fake_consent_auditor.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;
using ::testing::Matches;
using ::testing::Mock;

using consent_auditor::ArcBackupAndRestoreConsentEq;
using consent_auditor::ArcGoogleLocationServiceConsentEq;
using consent_auditor::ArcPlayConsentEq;

using ArcBackupAndRestoreConsent =
    sync_pb::UserConsentTypes::ArcBackupAndRestoreConsent;
using ArcGoogleLocationServiceConsent =
    sync_pb::UserConsentTypes::ArcGoogleLocationServiceConsent;
using ArcPlayTermsOfServiceConsent =
    sync_pb::UserConsentTypes::ArcPlayTermsOfServiceConsent;
using sync_pb::UserConsentTypes;

using OwnershipStatus = ash::DeviceSettingsService::OwnershipStatus;

class TestUserMetricsServiceClient
    : public ::metrics::TestMetricsServiceClient {
 public:
  std::optional<bool> GetCurrentUserMetricsConsent() const override {
    if (should_use_user_consent_) {
      return current_user_metrics_consent_;
    }
    return std::nullopt;
  }

  void UpdateCurrentUserMetricsConsent(bool metrics_consent) override {
    current_user_metrics_consent_ = metrics_consent;
  }

  void SetShouldUseUserConsent(bool should_use_user_consent) {
    should_use_user_consent_ = should_use_user_consent;
  }

 private:
  bool should_use_user_consent_ = false;
  bool current_user_metrics_consent_ = false;
};

class MockErrorDelegate : public ArcSupportHost::ErrorDelegate {
 public:
  MOCK_METHOD0(OnWindowClosed, void());
  MOCK_METHOD0(OnRetryClicked, void());
  MOCK_METHOD0(OnSendFeedbackClicked, void());
  MOCK_METHOD0(OnRunNetworkTestsClicked, void());
  MOCK_METHOD1(OnErrorPageShown, void(bool network_tests_shown));
};

}  // namespace

namespace arc {

class ArcTermsOfServiceDefaultNegotiatorTest
    : public BrowserWithTestWindowTest {
 public:
  ArcTermsOfServiceDefaultNegotiatorTest()
      : owner_key_util_(new ownership::MockOwnerKeyUtil()) {
    ::ash::OwnerSettingsServiceAshFactory::GetInstance()
        ->SetOwnerKeyUtilForTesting(owner_key_util_);
  }

  ArcTermsOfServiceDefaultNegotiatorTest(
      const ArcTermsOfServiceDefaultNegotiatorTest&) = delete;
  ArcTermsOfServiceDefaultNegotiatorTest& operator=(
      const ArcTermsOfServiceDefaultNegotiatorTest&) = delete;

  ~ArcTermsOfServiceDefaultNegotiatorTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    ::ash::DeviceSettingsService::Get()->SetSessionManager(
        &session_manager_client_, owner_key_util_);

    // MetricsService.
    metrics::MetricsService::RegisterPrefs(local_state_.registry());
    test_enabled_state_provider_ =
        std::make_unique<metrics::TestEnabledStateProvider>(true, true);
    test_metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &local_state_, test_enabled_state_provider_.get(), std::wstring(),
        base::FilePath());
    test_metrics_service_client_ =
        std::make_unique<TestUserMetricsServiceClient>();
    test_metrics_service_ = std::make_unique<metrics::MetricsService>(
        test_metrics_state_manager_.get(), test_metrics_service_client_.get(),
        &local_state_);

    // Needs to be set for metrics service.
    base::SetRecordActionTaskRunner(
        task_environment()->GetMainThreadTaskRunner());

    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile()), "testing@account.com",
        signin::ConsentLevel::kSync);

    ash::StatsReportingController::RegisterLocalStatePrefs(
        local_state_.registry());
    ash::StatsReportingController::Initialize(&local_state_);

    support_host_ = std::make_unique<ArcSupportHost>(profile());
    fake_arc_support_ = std::make_unique<FakeArcSupport>(support_host_.get());
    negotiator_ = std::make_unique<ArcTermsOfServiceDefaultNegotiator>(
        profile()->GetPrefs(), support_host(), test_metrics_service_.get());
  }

  void TearDown() override {
    negotiator_.reset();
    fake_arc_support_.reset();
    support_host_->SetErrorDelegate(nullptr);
    support_host_.reset();
    owner_key_util_->Clear();

    test_metrics_service_.reset();
    test_metrics_service_client_.reset();
    test_metrics_state_manager_.reset();
    test_enabled_state_provider_.reset();

    ::ash::DeviceSettingsService::Get()->UnsetSessionManager();
    ash::StatsReportingController::Shutdown();
    BrowserWithTestWindowTest::TearDown();
  }

  void LoadOwnershipStatus() {
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());

    content::RunAllTasksUntilIdle();
  }

  ArcSupportHost* support_host() { return support_host_.get(); }
  FakeArcSupport* fake_arc_support() { return fake_arc_support_.get(); }
  ArcTermsOfServiceNegotiator* negotiator() { return negotiator_.get(); }
  TestUserMetricsServiceClient* metrics_service_client() {
    return test_metrics_service_client_.get();
  }

  consent_auditor::FakeConsentAuditor* consent_auditor() {
    return static_cast<consent_auditor::FakeConsentAuditor*>(
        ConsentAuditorFactory::GetForProfile(profile()));
  }

  CoreAccountId GetAuthenticatedAccountId() {
    return IdentityManagerFactory::GetForProfile(profile())
        ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
        .account_id;
  }

  bool GetUserMetricsState() {
    return *metrics_service_client()->GetCurrentUserMetricsConsent();
  }

  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {TestingProfile::TestingFactory{
        ConsentAuditorFactory::GetInstance(),
        base::BindRepeating(&BuildFakeConsentAuditor)}};
  }

 protected:
  policy::DevicePolicyBuilder device_policy_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;
  ::ash::FakeSessionManagerClient session_manager_client_;

  TestingPrefServiceSimple local_state_;

  // MetricsService.
  std::unique_ptr<metrics::MetricsStateManager> test_metrics_state_manager_;
  std::unique_ptr<TestUserMetricsServiceClient> test_metrics_service_client_;
  std::unique_ptr<metrics::TestEnabledStateProvider>
      test_enabled_state_provider_;
  std::unique_ptr<metrics::MetricsService> test_metrics_service_;

  std::unique_ptr<ArcSupportHost> support_host_;
  std::unique_ptr<FakeArcSupport> fake_arc_support_;
  std::unique_ptr<ArcTermsOfServiceDefaultNegotiator> negotiator_;
  std::unique_ptr<MockErrorDelegate> error_delegate_;
};

class ArcTermsOfServiceDefaultNegotiatorForNonOwnerTest
    : public ArcTermsOfServiceDefaultNegotiatorTest {
 protected:
  void SetUp() override {
    device_policy_.SetDefaultNewSigningKey();
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetNewSigningKey());

    device_policy_.payload().mutable_metrics_enabled()->set_metrics_enabled(
        true);
    device_policy_.Build();
    session_manager_client_.set_device_policy(device_policy_.GetBlob());

    ArcTermsOfServiceDefaultNegotiatorTest::SetUp();
  }
};

namespace {

const char kFakeToSContent[] = "fake ToS content";

ArcBackupAndRestoreConsent CreateBaseBackupAndRestoreConsent() {
  ArcBackupAndRestoreConsent backup_and_restore_consent;
  backup_and_restore_consent.set_confirmation_grd_id(
      IDS_ARC_OPT_IN_DIALOG_BUTTON_AGREE);
  backup_and_restore_consent.add_description_grd_ids(
      IDS_ARC_OPT_IN_DIALOG_BACKUP_RESTORE);
  return backup_and_restore_consent;
}

ArcGoogleLocationServiceConsent CreateBaseGoogleLocationServiceConsent() {
  ArcGoogleLocationServiceConsent google_location_service_consent;
  google_location_service_consent.set_confirmation_grd_id(
      IDS_ARC_OPT_IN_DIALOG_BUTTON_AGREE);
  google_location_service_consent.add_description_grd_ids(
      IDS_ARC_OPT_IN_LOCATION_SETTING);
  return google_location_service_consent;
}

ArcPlayTermsOfServiceConsent CreateBasePlayConsent() {
  ArcPlayTermsOfServiceConsent play_consent;
  play_consent.set_play_terms_of_service_hash(
      base::SHA1HashString(std::string(kFakeToSContent)));
  play_consent.set_play_terms_of_service_text_length(
      (std::string(kFakeToSContent).length()));
  play_consent.set_consent_flow(ArcPlayTermsOfServiceConsent::SETUP);
  play_consent.set_confirmation_grd_id(IDS_ARC_OPT_IN_DIALOG_BUTTON_AGREE);
  return play_consent;
}

enum class Status {
  PENDING,
  ACCEPTED,
  CANCELLED,
};

// For better logging.
std::ostream& operator<<(std::ostream& os, Status status) {
  switch (status) {
    case Status::PENDING:
      return os << "PENDING";
    case Status::ACCEPTED:
      return os << "ACCEPTED";
    case Status::CANCELLED:
      return os << "CANCELLED";
  }

  NOTREACHED_IN_MIGRATION();
  return os;
}

ArcTermsOfServiceNegotiator::NegotiationCallback UpdateStatusCallback(
    Status* status) {
  return base::BindOnce(
      [](Status* status, bool accepted) {
        *status = accepted ? Status::ACCEPTED : Status::CANCELLED;
      },
      status);
}

TEST_F(ArcTermsOfServiceDefaultNegotiatorTest, Accept) {
  // Configure mock expections for proper consent recording.
  consent_auditor::FakeConsentAuditor* auditor = consent_auditor();
  Mock::VerifyAndClearExpectations(auditor);

  ArcPlayTermsOfServiceConsent play_consent = CreateBasePlayConsent();
  play_consent.set_status(UserConsentTypes::GIVEN);
  EXPECT_CALL(*auditor, RecordArcPlayConsent(GetAuthenticatedAccountId(),
                                             ArcPlayConsentEq(play_consent)));

  ArcBackupAndRestoreConsent backup_and_restore_consent =
      CreateBaseBackupAndRestoreConsent();
  backup_and_restore_consent.set_status(UserConsentTypes::GIVEN);
  EXPECT_CALL(*auditor,
              RecordArcBackupAndRestoreConsent(
                  GetAuthenticatedAccountId(),
                  ArcBackupAndRestoreConsentEq(backup_and_restore_consent)));
  ArcGoogleLocationServiceConsent google_location_service_consent =
      CreateBaseGoogleLocationServiceConsent();
  google_location_service_consent.set_status(UserConsentTypes::GIVEN);
  EXPECT_CALL(
      *auditor,
      RecordArcGoogleLocationServiceConsent(
          GetAuthenticatedAccountId(),
          ArcGoogleLocationServiceConsentEq(google_location_service_consent)));

  // Show Terms of service page.
  Status status = Status::PENDING;
  negotiator()->StartNegotiation(UpdateStatusCallback(&status));

  // TERMS page should be shown.
  EXPECT_EQ(status, Status::PENDING);
  EXPECT_EQ(fake_arc_support()->ui_page(), ArcSupportHost::UIPage::TERMS);

  // Emulate showing of a ToS page with a hard-coded ToS.
  fake_arc_support()->set_tos_content(kFakeToSContent);
  fake_arc_support()->set_tos_shown(true);

  // By default, the preference related checkboxes are checked, despite that
  // the preferences default to false.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcLocationServiceEnabled));
  EXPECT_TRUE(fake_arc_support()->backup_and_restore_mode());
  EXPECT_TRUE(fake_arc_support()->location_service_mode());

  // The preferences are assigned to the managed false value, and the
  // corresponding checkboxes are unchecked.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, std::make_unique<base::Value>(false));
  EXPECT_FALSE(fake_arc_support()->backup_and_restore_mode());
  if (base::FeatureList::IsEnabled(ash::features::kCrosPrivacyHub)) {
    profile()->GetTestingPrefService()->SetInteger(
        ash::prefs::kUserGeolocationAccessLevel,
        static_cast<int>(ash::GeolocationAccessLevel::kDisallowed));
  }
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(false));
  EXPECT_FALSE(fake_arc_support()->location_service_mode());

  if (base::FeatureList::IsEnabled(ash::features::kCrosPrivacyHub)) {
    // Toggle kArcLocationServiceEnabled to trigger the computation again as we
    // are listening on it. Now even with kArcLocationServiceEnabled false, we
    // should still get true as we will now honor kUserGeolocationAccessLevel.
    profile()->GetTestingPrefService()->SetInteger(
        ash::prefs::kUserGeolocationAccessLevel,
        static_cast<int>(ash::GeolocationAccessLevel::kAllowed));
    profile()->GetTestingPrefService()->SetManagedPref(
        prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(true));
    profile()->GetTestingPrefService()->SetManagedPref(
        prefs::kArcLocationServiceEnabled,
        std::make_unique<base::Value>(false));
    EXPECT_TRUE(fake_arc_support()->location_service_mode());
  }

  // The managed preference values are removed, and the corresponding checkboxes
  // are checked again.
  profile()->GetTestingPrefService()->RemoveManagedPref(
      prefs::kArcBackupRestoreEnabled);
  EXPECT_TRUE(fake_arc_support()->backup_and_restore_mode());
  profile()->GetTestingPrefService()->RemoveManagedPref(
      prefs::kArcLocationServiceEnabled);
  // When CrosPrivacyHub is enabled this is true as we set
  // `kUserGeolocationAccessLevel` to be `AccessLevel::kAllowed`.
  EXPECT_TRUE(fake_arc_support()->location_service_mode());

  // Make sure preference values are not yet updated.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcLocationServiceEnabled));

  // Click the "AGREE" button so that the callback should be invoked
  // with |agreed| = true.
  fake_arc_support()->ClickAgreeButton();

  // Wait until async calls are all completed, which is triggered by ownership
  // status being loaded.
  LoadOwnershipStatus();
  EXPECT_EQ(status, Status::ACCEPTED);

  // Make sure preference values are now updated.
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcLocationServiceEnabled));
}

TEST_F(ArcTermsOfServiceDefaultNegotiatorTest, AcceptWithLocationDisabled) {
  if (base::FeatureList::IsEnabled(ash::features::kCrosPrivacyHub)) {
    profile()->GetTestingPrefService()->SetBoolean(
        prefs::kArcInitialLocationSettingSyncRequired, true);
    profile()->GetTestingPrefService()->SetBoolean(
        ash::prefs::kUserGeolocationAccuracyEnabled, true);
    profile()->GetTestingPrefService()->SetInteger(
        ash::prefs::kUserGeolocationAccessLevel,
        static_cast<int>(ash::GeolocationAccessLevel::kAllowed));
  }

  // Show Terms of service page.
  Status status = Status::PENDING;
  negotiator()->StartNegotiation(UpdateStatusCallback(&status));

  // TERMS page should be shown.
  EXPECT_EQ(status, Status::PENDING);
  EXPECT_EQ(fake_arc_support()->ui_page(), ArcSupportHost::UIPage::TERMS);

  // Emulate showing of a ToS page with a hard-coded ToS.
  fake_arc_support()->set_tos_content(kFakeToSContent);
  fake_arc_support()->set_tos_shown(true);

  fake_arc_support()->set_location_service_mode(false);

  // Click the "AGREE" button so that the callback should be invoked
  // with |agreed| = true.
  fake_arc_support()->ClickAgreeButton();

  // Wait until async calls are all completed, which is triggered by ownership
  // status being loaded.
  LoadOwnershipStatus();
  EXPECT_EQ(status, Status::ACCEPTED);

  // Make sure preference values are now updated.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcLocationServiceEnabled));
  if (base::FeatureList::IsEnabled(ash::features::kCrosPrivacyHub)) {
    EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
        prefs::kArcInitialLocationSettingSyncRequired));
    EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
        ash::prefs::kUserGeolocationAccuracyEnabled));
    EXPECT_EQ(ash::GeolocationAccessLevel::kDisallowed,
              static_cast<ash::GeolocationAccessLevel>(
                  profile()->GetPrefs()->GetInteger(
                      ash::prefs::kUserGeolocationAccessLevel)));
  }
}

TEST_F(ArcTermsOfServiceDefaultNegotiatorTest, AcceptWithUnchecked) {
  // Configure the mock consent auditor to make sure consent auditing records
  // the ToS accept as GIVEN, but the other consents as NOT_GIVEN.
  consent_auditor::FakeConsentAuditor* ca = consent_auditor();
  Mock::VerifyAndClearExpectations(ca);

  ArcPlayTermsOfServiceConsent play_consent = CreateBasePlayConsent();
  play_consent.set_status(UserConsentTypes::GIVEN);
  EXPECT_CALL(*ca, RecordArcPlayConsent(GetAuthenticatedAccountId(),
                                        ArcPlayConsentEq(play_consent)));

  ArcBackupAndRestoreConsent backup_and_restore_consent =
      CreateBaseBackupAndRestoreConsent();
  backup_and_restore_consent.clear_status();
  backup_and_restore_consent.set_status(UserConsentTypes::NOT_GIVEN);
  EXPECT_CALL(*ca,
              RecordArcBackupAndRestoreConsent(
                  GetAuthenticatedAccountId(),
                  ArcBackupAndRestoreConsentEq(backup_and_restore_consent)));

  ArcGoogleLocationServiceConsent google_location_service_consent =
      CreateBaseGoogleLocationServiceConsent();
  google_location_service_consent.clear_status();
  google_location_service_consent.set_status(UserConsentTypes::NOT_GIVEN);
  EXPECT_CALL(
      *ca,
      RecordArcGoogleLocationServiceConsent(
          GetAuthenticatedAccountId(),
          ArcGoogleLocationServiceConsentEq(google_location_service_consent)));

  // Show Terms of service page.
  Status status = Status::PENDING;
  negotiator()->StartNegotiation(UpdateStatusCallback(&status));

  // TERMS page should be shown.
  EXPECT_EQ(status, Status::PENDING);
  EXPECT_EQ(fake_arc_support()->ui_page(), ArcSupportHost::UIPage::TERMS);

  // Emulate showing of a ToS page with a hard-coded ID.
  fake_arc_support()->set_tos_content(kFakeToSContent);
  fake_arc_support()->set_tos_shown(true);

  // Override the preferences from the default values to true.
  profile()->GetPrefs()->SetBoolean(prefs::kArcBackupRestoreEnabled, true);
  profile()->GetPrefs()->SetBoolean(prefs::kArcLocationServiceEnabled, true);

  // Uncheck the preference related checkboxes.
  fake_arc_support()->set_backup_and_restore_mode(false);
  fake_arc_support()->set_location_service_mode(false);

  // Make sure preference values are not yet updated.
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcLocationServiceEnabled));

  // Click the "AGREE" button so that the callback should be invoked
  // with |agreed| = true.
  fake_arc_support()->ClickAgreeButton();

  // Wait until async calls are all completed, which is triggered by ownership
  // status being loaded.
  LoadOwnershipStatus();
  EXPECT_EQ(status, Status::ACCEPTED);

  // Make sure preference values are now updated.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcLocationServiceEnabled));
}

TEST_F(ArcTermsOfServiceDefaultNegotiatorTest, AcceptMetricsNoOwner) {
  // Show Terms of service page.
  Status status = Status::PENDING;
  negotiator()->StartNegotiation(UpdateStatusCallback(&status));

  // TERMS page should be shown.
  EXPECT_EQ(status, Status::PENDING);
  EXPECT_EQ(fake_arc_support()->ui_page(), ArcSupportHost::UIPage::TERMS);

  // Emulate showing of a ToS page with a hard-coded ID.
  fake_arc_support()->set_metrics_mode(false);

  bool expected_metrics_state = true;

  // Check the preference related checkboxes.
  fake_arc_support()->set_metrics_mode(expected_metrics_state);

  // Click the "AGREE" button so that the callback should be invoked
  // with |agreed| = true.
  fake_arc_support()->ClickAgreeButton();

  // Owners status has not been loaded yet. Changes should not be propagated.
  EXPECT_NE(expected_metrics_state,
            ash::StatsReportingController::Get()->IsEnabled());

  // Check owners opt-in once ownership status known.
  LoadOwnershipStatus();
  EXPECT_EQ(status, Status::ACCEPTED);
  EXPECT_EQ(ash::DeviceSettingsService::Get()->GetOwnershipStatus(),
            OwnershipStatus::kOwnershipNone);
  EXPECT_EQ(expected_metrics_state,
            ash::StatsReportingController::Get()->IsEnabled());
}

TEST_F(ArcTermsOfServiceDefaultNegotiatorForNonOwnerTest,
       AcceptMetricsUserOptIn) {
  // Show Terms of service page.
  Status status = Status::PENDING;
  negotiator()->StartNegotiation(UpdateStatusCallback(&status));

  // Setup metrics service to use user metrics.
  metrics_service_client()->SetShouldUseUserConsent(true);

  // TERMS page should be shown.
  EXPECT_EQ(status, Status::PENDING);
  EXPECT_EQ(fake_arc_support()->ui_page(), ArcSupportHost::UIPage::TERMS);

  // Load ownership and enable metrics by the owner. If owner has opted out of
  // metrics, per-user metrics opt-in is not allowed. Ensure that the ownership
  // is probably setup.
  LoadOwnershipStatus();
  EXPECT_EQ(ash::DeviceSettingsService::Get()->GetOwnershipStatus(),
            OwnershipStatus::kOwnershipTaken);
  EXPECT_TRUE(ash::StatsReportingController::Get()->IsEnabled());
  EXPECT_FALSE(user_manager::UserManager::Get()->IsCurrentUserOwner());

  // Emulate showing of a ToS page with a hard-coded ID.
  fake_arc_support()->set_metrics_mode(false);

  bool expected_metrics_state = true;

  // Check the preference related checkboxes.
  fake_arc_support()->set_metrics_mode(expected_metrics_state);

  // Click the "AGREE" button so that the callback should be invoked
  // with |agreed| = true.
  fake_arc_support()->ClickAgreeButton();

  // Wait for async calls to finish.
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(status, Status::ACCEPTED);
  EXPECT_EQ(expected_metrics_state, GetUserMetricsState());
}

TEST_F(ArcTermsOfServiceDefaultNegotiatorTest, AcceptWithManagedToS) {
  consent_auditor::FakeConsentAuditor* auditor = consent_auditor();
  Mock::VerifyAndClearExpectations(auditor);

  ArcPlayTermsOfServiceConsent play_consent = CreateBasePlayConsent();
  play_consent.clear_play_terms_of_service_text_length();
  play_consent.clear_play_terms_of_service_hash();
  play_consent.set_status(UserConsentTypes::GIVEN);
  EXPECT_CALL(*auditor, RecordArcPlayConsent(GetAuthenticatedAccountId(),
                                             ArcPlayConsentEq(play_consent)));

  ArcGoogleLocationServiceConsent google_location_service_consent =
      CreateBaseGoogleLocationServiceConsent();
  google_location_service_consent.set_status(UserConsentTypes::GIVEN);
  EXPECT_CALL(
      *auditor,
      RecordArcGoogleLocationServiceConsent(
          GetAuthenticatedAccountId(),
          ArcGoogleLocationServiceConsentEq(google_location_service_consent)));

  // Verifies that we record an empty ToS consent if the ToS is not shown due to
  // a managed user scenario.
  // Also verifies that a managed setting for Backup and Restore is not recorded
  // while an unmanaged setting for Location Services still is recorded.
  // Show Terms of service page.
  Status status = Status::PENDING;
  negotiator()->StartNegotiation(UpdateStatusCallback(&status));

  // TERMS page should be shown.
  EXPECT_EQ(status, Status::PENDING);
  EXPECT_EQ(fake_arc_support()->ui_page(), ArcSupportHost::UIPage::TERMS);

  // Emulate ToS not shown, and Backup and Restore as a managed setting.
  fake_arc_support()->set_tos_content(kFakeToSContent);
  fake_arc_support()->set_tos_shown(false);
  fake_arc_support()->set_backup_and_restore_managed(true);

  // Override the preferences from the default values to true.
  profile()->GetPrefs()->SetBoolean(prefs::kArcBackupRestoreEnabled, true);
  profile()->GetPrefs()->SetBoolean(prefs::kArcLocationServiceEnabled, true);

  // Click the "AGREE" button so that the callback should be invoked
  // with |agreed| = true.
  fake_arc_support()->ClickAgreeButton();

  // Wait until async calls are all completed, which is triggered by ownership
  // status being loaded.
  LoadOwnershipStatus();

  // Make sure preference values are now updated.
  EXPECT_EQ(status, Status::ACCEPTED);
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcLocationServiceEnabled));
}

TEST_F(ArcTermsOfServiceDefaultNegotiatorTest, Cancel) {
  consent_auditor::FakeConsentAuditor* auditor = consent_auditor();
  Mock::VerifyAndClearExpectations(auditor);

  ArcPlayTermsOfServiceConsent play_consent = CreateBasePlayConsent();
  play_consent.set_status(UserConsentTypes::NOT_GIVEN);
  EXPECT_CALL(*auditor, RecordArcPlayConsent(GetAuthenticatedAccountId(),
                                             ArcPlayConsentEq(play_consent)));

  ArcBackupAndRestoreConsent backup_and_restore_consent =
      CreateBaseBackupAndRestoreConsent();
  backup_and_restore_consent.set_status(UserConsentTypes::NOT_GIVEN);
  EXPECT_CALL(*auditor,
              RecordArcBackupAndRestoreConsent(
                  GetAuthenticatedAccountId(),
                  ArcBackupAndRestoreConsentEq(backup_and_restore_consent)));

  ArcGoogleLocationServiceConsent google_location_service_consent =
      CreateBaseGoogleLocationServiceConsent();
  google_location_service_consent.set_status(UserConsentTypes::NOT_GIVEN);
  EXPECT_CALL(
      *auditor,
      RecordArcGoogleLocationServiceConsent(
          GetAuthenticatedAccountId(),
          ArcGoogleLocationServiceConsentEq(google_location_service_consent)));

  // Show Terms of service page.
  Status status = Status::PENDING;
  negotiator()->StartNegotiation(UpdateStatusCallback(&status));

  // TERMS page should be shown.
  EXPECT_EQ(status, Status::PENDING);
  EXPECT_EQ(fake_arc_support()->ui_page(), ArcSupportHost::UIPage::TERMS);

  // Emulate showing of a ToS page with a hard-coded ToS.
  fake_arc_support()->set_tos_content(kFakeToSContent);
  fake_arc_support()->set_tos_shown(true);

  // Check the preference related checkbox.
  fake_arc_support()->set_metrics_mode(true);
  fake_arc_support()->set_backup_and_restore_mode(true);
  fake_arc_support()->set_location_service_mode(true);

  // Make sure preference values are not yet updated.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcLocationServiceEnabled));

  // Clicking "CANCEL" button closes the window.
  fake_arc_support()->ClickCancelButton();
  EXPECT_EQ(status, Status::CANCELLED);

  // Make sure preference checkbox values are discarded.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcLocationServiceEnabled));
}

TEST_F(ArcTermsOfServiceDefaultNegotiatorTest, Retry) {
  error_delegate_ = std::make_unique<MockErrorDelegate>();
  support_host()->SetErrorDelegate(error_delegate_.get());

  // Show Terms of service page.
  Status status = Status::PENDING;
  negotiator()->StartNegotiation(UpdateStatusCallback(&status));

  // TERMS page should be shown.
  EXPECT_EQ(status, Status::PENDING);
  EXPECT_EQ(fake_arc_support()->ui_page(), ArcSupportHost::UIPage::TERMS);

  // Switch to error page.
  EXPECT_CALL(*error_delegate_, OnErrorPageShown(true));
  support_host()->ShowError(
      ArcSupportHost::ErrorInfo(ArcSupportHost::Error::SIGN_IN_NETWORK_ERROR),
      false /* should_show_send_feedback */,
      true /* should_show_run_network_tests */);

  // The callback should not be called yet.
  EXPECT_EQ(status, Status::PENDING);
  EXPECT_EQ(fake_arc_support()->ui_page(), ArcSupportHost::UIPage::ERROR);

  // Click RETRY button on the page, then Terms of service page should be
  // re-shown.
  fake_arc_support()->ClickRetryButton();
  EXPECT_EQ(status, Status::PENDING);
  EXPECT_EQ(fake_arc_support()->ui_page(), ArcSupportHost::UIPage::TERMS);
}

}  //  namespace

}  // namespace arc

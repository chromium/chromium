// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <ostream>
#include <vector>

#include "base/bind.h"
#include "base/hash/sha1.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/chromeos/arc/extensions/fake_arc_support.h"
#include "chrome/browser/chromeos/arc/optin/arc_terms_of_service_default_negotiator.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/settings/stats_reporting_controller.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/consent_auditor/consent_auditor_test_utils.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_prefs.h"
#include "components/consent_auditor/fake_consent_auditor.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Matches;
using ::testing::Mock;
using ::testing::_;

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

namespace arc {

class ArcTermsOfServiceDefaultNegotiatorTest
    : public BrowserWithTestWindowTest {
 public:
  ArcTermsOfServiceDefaultNegotiatorTest() = default;
  ~ArcTermsOfServiceDefaultNegotiatorTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<chromeos::FakeChromeUserManager>());
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile()),
        "testing@account.com");

    chromeos::StatsReportingController::RegisterLocalStatePrefs(
        local_state_.registry());
    chromeos::StatsReportingController::Initialize(&local_state_);

    support_host_ = std::make_unique<ArcSupportHost>(profile());
    fake_arc_support_ = std::make_unique<FakeArcSupport>(support_host_.get());
    negotiator_ = std::make_unique<ArcTermsOfServiceDefaultNegotiator>(
        profile()->GetPrefs(), support_host());
  }

  void TearDown() override {
    negotiator_.reset();
    fake_arc_support_.reset();
    support_host_.reset();
    user_manager_enabler_.reset();

    chromeos::StatsReportingController::Shutdown();
    BrowserWithTestWindowTest::TearDown();
  }

  ArcSupportHost* support_host() { return support_host_.get(); }
  FakeArcSupport* fake_arc_support() { return fake_arc_support_.get(); }
  ArcTermsOfServiceNegotiator* negotiator() { return negotiator_.get(); }

  consent_auditor::FakeConsentAuditor* consent_auditor() {
    return static_cast<consent_auditor::FakeConsentAuditor*>(
        ConsentAuditorFactory::GetForProfile(profile()));
  }

  CoreAccountId GetAuthenticatedAccountId() {
    return IdentityManagerFactory::GetForProfile(profile())
        ->GetPrimaryAccountInfo()
        .account_id;
  }

  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{ConsentAuditorFactory::GetInstance(),
             base::BindRepeating(&BuildFakeConsentAuditor)}};
  }

 private:
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  std::unique_ptr<ArcSupportHost> support_host_;
  std::unique_ptr<FakeArcSupport> fake_arc_support_;
  std::unique_ptr<ArcTermsOfServiceNegotiator> negotiator_;

  DISALLOW_COPY_AND_ASSIGN(ArcTermsOfServiceDefaultNegotiatorTest);
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

  NOTREACHED();
  return os;
}

ArcTermsOfServiceNegotiator::NegotiationCallback UpdateStatusCallback(
    Status* status) {
  return base::Bind(
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
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, std::make_unique<base::Value>(false));
  EXPECT_FALSE(fake_arc_support()->location_service_mode());

  // The managed preference values are removed, and the corresponding checkboxes
  // are checked again.
  profile()->GetTestingPrefService()->RemoveManagedPref(
      prefs::kArcBackupRestoreEnabled);
  EXPECT_TRUE(fake_arc_support()->backup_and_restore_mode());
  profile()->GetTestingPrefService()->RemoveManagedPref(
      prefs::kArcLocationServiceEnabled);
  EXPECT_TRUE(fake_arc_support()->location_service_mode());

  // Make sure preference values are not yet updated.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcLocationServiceEnabled));

  // Click the "AGREE" button so that the callback should be invoked
  // with |agreed| = true.
  fake_arc_support()->ClickAgreeButton();
  EXPECT_EQ(status, Status::ACCEPTED);

  // Make sure preference values are now updated.
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcLocationServiceEnabled));
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
  EXPECT_EQ(status, Status::ACCEPTED);

  // Make sure preference values are now updated.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcLocationServiceEnabled));
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
  EXPECT_EQ(status, Status::ACCEPTED);

  // Make sure preference values are now updated.
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
  // Show Terms of service page.
  Status status = Status::PENDING;
  negotiator()->StartNegotiation(UpdateStatusCallback(&status));

  // TERMS page should be shown.
  EXPECT_EQ(status, Status::PENDING);
  EXPECT_EQ(fake_arc_support()->ui_page(), ArcSupportHost::UIPage::TERMS);

  // Switch to error page.
  support_host()->ShowError(ArcSupportHost::Error::SIGN_IN_NETWORK_ERROR,
                            false);

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

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_check/android/password_check_manager.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/password_check/android/password_check_ui_status.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using password_manager::BulkLeakCheckService;
using password_manager::InsecureType;
using password_manager::PasswordCheckUIStatus;
using password_manager::PasswordForm;
using password_manager::TestPasswordStore;
using password_manager::prefs::kLastTimePasswordCheckCompleted;
using testing::_;
using testing::AtLeast;
using testing::Each;
using testing::ElementsAre;
using testing::Field;
using testing::Invoke;
using testing::IsEmpty;
using testing::Key;
using testing::NiceMock;
using testing::Property;
using testing::Return;

using CompromisedCredentialForUI =
    PasswordCheckManager::CompromisedCredentialForUI;
using State = password_manager::BulkLeakCheckService::State;

namespace {

constexpr char kExampleCom[] = "https://example.com";
constexpr char kExampleOrg[] = "http://www.example.org";
constexpr char kExampleApp[] = "com.example.app";

constexpr char16_t kUsername1[] = u"alice";
constexpr char16_t kUsername2[] = u"bob";

constexpr char16_t kPassword1[] = u"s3cre3t";

constexpr char kTestEmail[] = "user@gmail.com";

MATCHER_P(MatchInsecureType,
          insecure_type,
          base::StrCat({negation ? "does not " : "", "match the type flag"})) {
  return arg.contains(insecure_type);
}

class MockPasswordCheckManagerObserver : public PasswordCheckManager::Observer {
 public:
  MOCK_METHOD(void, OnSavedPasswordsFetched, (int), (override));

  MOCK_METHOD(void, OnCompromisedCredentialsChanged, (int), (override));

  MOCK_METHOD(void,
              OnPasswordCheckStatusChanged,
              (password_manager::PasswordCheckUIStatus),
              (override));

  MOCK_METHOD(void, OnPasswordCheckProgressChanged, (int, int), (override));
};

BulkLeakCheckService* CreateAndUseBulkLeakCheckService(
    signin::IdentityManager* identity_manager,
    Profile* profile) {
  return BulkLeakCheckServiceFactory::GetInstance()
      ->SetTestingSubclassFactoryAndUse(
          profile, base::BindOnce(
                       [](signin::IdentityManager* identity_manager,
                          content::BrowserContext*) {
                         return std::make_unique<BulkLeakCheckService>(
                             identity_manager,
                             base::MakeRefCounted<
                                 network::TestSharedURLLoaderFactory>());
                       },
                       base::Unretained(identity_manager)));
}

syncer::TestSyncService* CreateAndUseSyncService(Profile* profile) {
  return SyncServiceFactory::GetInstance()->SetTestingSubclassFactoryAndUse(
      profile, base::BindOnce([](content::BrowserContext*) {
        return std::make_unique<syncer::TestSyncService>();
      }));
}

PasswordForm MakeSavedPassword(std::string_view signon_realm,
                               std::u16string_view username,
                               std::u16string_view password = kPassword1,
                               std::u16string_view username_element = u"") {
  PasswordForm form;
  form.signon_realm = std::string(signon_realm);
  form.url = GURL(signon_realm);
  form.username_value = std::u16string(username);
  form.password_value = std::u16string(password);
  form.username_element = std::u16string(username_element);
  return form;
}

std::string MakeAndroidRealm(std::string_view package_name) {
  return base::StrCat({"android://hash@", package_name});
}
PasswordForm MakeSavedAndroidPassword(
    std::string_view package_name,
    std::u16string_view username,
    std::string_view app_display_name = "",
    std::string_view affiliated_web_realm = "") {
  PasswordForm form;
  form.signon_realm = MakeAndroidRealm(package_name);
  form.username_value = std::u16string(username);
  form.app_display_name = std::string(app_display_name);
  form.affiliated_web_realm = std::string(affiliated_web_realm);
  return form;
}

void AddIssueToForm(PasswordForm* form,
                    InsecureType type = InsecureType::kLeaked,
                    base::TimeDelta time_since_creation = base::TimeDelta()) {
  form->password_issues.insert_or_assign(
      type, password_manager::InsecurityMetadata(
                base::Time::Now() - time_since_creation,
                password_manager::IsMuted(false),
                password_manager::TriggerBackendNotification(false)));
}

// Creates matcher for a given compromised credential
auto ExpectCompromisedCredentialForUI(
    const std::u16string& display_username,
    const std::u16string& display_origin,
    const GURL& url,
    const std::optional<std::string>& package_name,
    const std::optional<std::string>& change_password_url,
    InsecureType insecure_type) {
  auto package_name_field_matcher =
      package_name.has_value()
          ? Field(&CompromisedCredentialForUI::package_name,
                  package_name.value())
          : Field(&CompromisedCredentialForUI::package_name, IsEmpty());
  auto change_password_url_field_matcher =
      change_password_url.has_value()
          ? Field(&CompromisedCredentialForUI::change_password_url,
                  change_password_url.value())
          : Field(&CompromisedCredentialForUI::change_password_url, IsEmpty());
  return AllOf(
      Field(&CompromisedCredentialForUI::display_username, display_username),
      Field(&CompromisedCredentialForUI::display_origin, display_origin),
      Property(&CompromisedCredentialForUI::GetURL, url),
      package_name_field_matcher, change_password_url_field_matcher,
      Field(&CompromisedCredentialForUI::password_issues,
            MatchInsecureType(insecure_type)),
      Property(&CompromisedCredentialForUI::IsLeaked,
               insecure_type == InsecureType::kLeaked),
      Property(&CompromisedCredentialForUI::IsPhished,
               insecure_type == InsecureType::kPhished));
}

}  // namespace

class PasswordCheckManagerTest : public testing::Test {
 public:
  void InitializeManager() {
    manager_ =
        std::make_unique<PasswordCheckManager>(&profile_, &mock_observer_);
  }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

  signin::IdentityTestEnvironment& identity_test_env() {
    return identity_test_env_;
  }
  BulkLeakCheckService* service() { return service_; }
  TestPasswordStore& store() { return *store_; }
  MockPasswordCheckManagerObserver& mock_observer() { return mock_observer_; }
  PasswordCheckManager& manager() { return *manager_; }
  base::test::ScopedFeatureList& feature_list() { return feature_list_; }
  syncer::TestSyncService& sync_service() { return *sync_service_; }

 private:
  content::BrowserTaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_test_env_;
  TestingProfile profile_;
  raw_ptr<BulkLeakCheckService> service_ =
      CreateAndUseBulkLeakCheckService(identity_test_env_.identity_manager(),
                                       &profile_);
  scoped_refptr<TestPasswordStore> store_ =
      CreateAndUseTestPasswordStore(&profile_);
  raw_ptr<syncer::TestSyncService> sync_service_ =
      CreateAndUseSyncService(&profile_);
  NiceMock<MockPasswordCheckManagerObserver> mock_observer_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<PasswordCheckManager> manager_;
};

TEST_F(PasswordCheckManagerTest, SendsNoPasswordsMessageIfNoPasswordsAreSaved) {
  EXPECT_CALL(mock_observer(), OnPasswordCheckStatusChanged(
                                   PasswordCheckUIStatus::kErrorNoPasswords));
  InitializeManager();
  RunUntilIdle();
}

TEST_F(PasswordCheckManagerTest, OnSavedPasswordsFetched) {
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  EXPECT_CALL(mock_observer(), OnSavedPasswordsFetched(1));
  InitializeManager();
  RunUntilIdle();

  // Verify that OnSavedPasswordsFetched is not called after the initial fetch
  // even if the saved passwords change.
  EXPECT_CALL(mock_observer(), OnSavedPasswordsFetched(_)).Times(0);
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername2));
  RunUntilIdle();
}

TEST_F(PasswordCheckManagerTest, OnCompromisedCredentialsChanged) {
  // This is called on multiple events: once for saved passwords retrieval,
  // and once when the saved password is added.
  EXPECT_CALL(mock_observer(), OnCompromisedCredentialsChanged(0)).Times(2);
  InitializeManager();
  PasswordForm form = MakeSavedPassword(kExampleCom, kUsername1);
  store().AddLogin(form);
  RunUntilIdle();

  EXPECT_CALL(mock_observer(), OnCompromisedCredentialsChanged(1));
  AddIssueToForm(&form);
  store().UpdateLogin(form);
  RunUntilIdle();
}

TEST_F(PasswordCheckManagerTest, RunCheckAfterLastInitialization) {
  identity_test_env().MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);
  EXPECT_CALL(mock_observer(), OnPasswordCheckStatusChanged(_))
      .Times(AtLeast(1));
  EXPECT_CALL(mock_observer(), OnSavedPasswordsFetched(1));
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  InitializeManager();

  // Initialization is incomplete, so check shouldn't run.
  manager().StartCheck();  // Try to start a check — has no immediate effect.
  service()->set_state_and_notify(State::kIdle);
  // Since check hasn't started, the last completion time should remain 0.
  EXPECT_EQ(0.0, manager().GetLastCheckTimestamp().InSecondsFSinceUnixEpoch());

  // Complete pending initialization. The check should run now.
  EXPECT_CALL(mock_observer(), OnCompromisedCredentialsChanged(0))
      .Times(AtLeast(1));
  RunUntilIdle();
  service()->set_state_and_notify(State::kIdle);  // Complete check, if any.
  // Check should have started and the last completion time be non-zero.
  EXPECT_NE(0.0, manager().GetLastCheckTimestamp().InSecondsFSinceUnixEpoch());
}

TEST_F(PasswordCheckManagerTest, CorrectlyCreatesUIStructForSiteCredential) {
  InitializeManager();
  PasswordForm form = MakeSavedPassword(kExampleCom, kUsername1);
  AddIssueToForm(&form);
  store().AddLogin(form);
  RunUntilIdle();
  EXPECT_THAT(manager().GetCompromisedCredentials(),
              ElementsAre(ExpectCompromisedCredentialForUI(
                  kUsername1, u"example.com", GURL(kExampleCom), std::nullopt,
                  "https://example.com/.well-known/change-password",
                  InsecureType::kLeaked)));
}

TEST_F(PasswordCheckManagerTest, CorrectlyCreatesUIStructForAppCredentials) {
  InitializeManager();

  // A credential without affiliation information.
  PasswordForm form_no_affiliation =
      MakeSavedAndroidPassword(kExampleApp, kUsername1);
  AddIssueToForm(&form_no_affiliation);
  store().AddLogin(form_no_affiliation);

  // A credential for which affiliation information is known.
  PasswordForm form_with_affiliation = MakeSavedAndroidPassword(
      kExampleApp, kUsername2, "Example App", kExampleCom);
  AddIssueToForm(&form_with_affiliation);
  store().AddLogin(form_with_affiliation);
  RunUntilIdle();

  // Some weak, reused and secure credentials that should be ignored.
  PasswordForm form_weak = MakeSavedAndroidPassword(kExampleOrg, kUsername1);
  AddIssueToForm(&form_weak, InsecureType::kWeak);
  store().AddLogin(form_weak);
  PasswordForm form_reused = MakeSavedAndroidPassword(kExampleCom, kUsername2);
  AddIssueToForm(&form_reused, InsecureType::kReused);
  store().AddLogin(form_reused);
  store().AddLogin(MakeSavedAndroidPassword(kExampleOrg, kUsername2));

  EXPECT_THAT(manager().GetCompromisedCredentialsCount(), 2);
  EXPECT_THAT(manager().GetCompromisedCredentials(),
              UnorderedElementsAre(
                  ExpectCompromisedCredentialForUI(
                      kUsername1, u"App (com.example.app)", GURL(),
                      "com.example.app", std::nullopt, InsecureType::kLeaked),
                  ExpectCompromisedCredentialForUI(
                      kUsername2, u"Example App", GURL(kExampleCom),
                      "com.example.app", std::nullopt, InsecureType::kLeaked)));
}

TEST_F(PasswordCheckManagerTest, SetsTimestampOnSuccessfulCheck) {
  identity_test_env().MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);
  InitializeManager();
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  // Pretend to start the check so that the manager thinks a check is running.
  manager().StartCheck();

  // Change the state to idle to simulate a successful check finish.
  service()->set_state_and_notify(State::kIdle);
  EXPECT_NE(0.0, manager().GetLastCheckTimestamp().InSecondsFSinceUnixEpoch());
}

TEST_F(PasswordCheckManagerTest, DoesntRecordTimestampOfUnsuccessfulCheck) {
  identity_test_env().MakeAccountAvailable(kTestEmail);
  InitializeManager();
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1));
  RunUntilIdle();

  // Pretend to start the check so that the manager thinks a check is running.
  manager().StartCheck();

  // Change the state to an error state to simulate a unsuccessful check finish.
  service()->set_state_and_notify(State::kSignedOut);
  EXPECT_EQ(0.0, manager().GetLastCheckTimestamp().InSecondsFSinceUnixEpoch());
}

TEST_F(PasswordCheckManagerTest, CorrectlyCreatesUIStruct) {
  InitializeManager();
  // Disable password sync
  sync_service().GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());
  PasswordForm form = MakeSavedPassword(kExampleCom, kUsername1);
  AddIssueToForm(&form);
  store().AddLogin(form);
  RunUntilIdle();

  EXPECT_THAT(manager().GetCompromisedCredentials(),
              ElementsAre(ExpectCompromisedCredentialForUI(
                  kUsername1, u"example.com", GURL(kExampleCom), std::nullopt,
                  "https://example.com/.well-known/change-password",
                  InsecureType::kLeaked)));
}

TEST_F(PasswordCheckManagerTest, UpdatesProgressCorrectly) {
  identity_test_env().MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);
  InitializeManager();

  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  store().AddLogin(MakeSavedPassword(kExampleOrg, kUsername1, kPassword1));
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername2));
  RunUntilIdle();

  EXPECT_CALL(mock_observer(), OnPasswordCheckProgressChanged(0, 3));
  manager().StartCheck();

  // Expect that 2 credentials were processed, even if there is only one
  // reply, because of the deduplication logic.
  EXPECT_CALL(mock_observer(), OnPasswordCheckProgressChanged(2, 1));
  static_cast<password_manager::BulkLeakCheckDelegateInterface*>(service())
      ->OnFinishedCredential(
          password_manager::LeakCheckCredential(kUsername1, kPassword1),
          password_manager::IsLeaked(false));
}

TEST_F(PasswordCheckManagerTest, DoesntUpdateNonExistingProgress) {
  InitializeManager();
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername1, kPassword1));
  store().AddLogin(MakeSavedPassword(kExampleOrg, kUsername1, kPassword1));
  store().AddLogin(MakeSavedPassword(kExampleCom, kUsername2));
  RunUntilIdle();

  EXPECT_CALL(mock_observer(), OnPasswordCheckProgressChanged).Times(0);
  static_cast<password_manager::BulkLeakCheckDelegateInterface*>(service())
      ->OnFinishedCredential(
          password_manager::LeakCheckCredential(kUsername1, kPassword1),
          password_manager::IsLeaked(false));
}

TEST_F(PasswordCheckManagerTest, TurnsIdleIntoNoPasswords) {
  InitializeManager();
  RunUntilIdle();

  EXPECT_CALL(mock_observer(),
              OnPasswordCheckStatusChanged(PasswordCheckUIStatus::kRunning))
      .Times(1);
  EXPECT_CALL(mock_observer(),
              OnPasswordCheckStatusChanged(PasswordCheckUIStatus::kIdle))
      .Times(0);
  EXPECT_CALL(mock_observer(), OnPasswordCheckStatusChanged(
                                   PasswordCheckUIStatus::kErrorNoPasswords))
      .Times(1);

  manager().StartCheck();
}

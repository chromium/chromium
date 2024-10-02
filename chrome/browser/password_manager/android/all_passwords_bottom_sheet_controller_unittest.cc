// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/all_passwords_bottom_sheet_controller.h"

#include "base/android/build_info.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/pass_key.h"
#include "chrome/browser/password_manager/android/access_loss/mock_password_access_loss_warning_bridge.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/ui/android/passwords/all_passwords_bottom_sheet_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/plus_addresses/fake_plus_address_service.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/safe_browsing/core/browser/password_protection/stub_password_reuse_detection_manager_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

using autofill::mojom::FocusedFieldType;
using base::test::RunOnceCallback;
using device_reauth::MockDeviceAuthenticator;
using password_manager::PasswordForm;
using password_manager::TestPasswordStore;
using password_manager::UiCredential;
using plus_addresses::FakePlusAddressService;

using CallbackFunctionMock = testing::MockFunction<void()>;

using DismissCallback = base::MockCallback<base::OnceCallback<void()>>;

using RequestsToFillPassword =
    AllPasswordsBottomSheetController::RequestsToFillPassword;

constexpr char kExampleCom[] = "https://example.com";
constexpr char kExampleOrg[] = "http://www.example.org";
constexpr char kExampleDe[] = "https://www.example.de";

constexpr char16_t kUsername1[] = u"alice";
constexpr char16_t kUsername2[] = u"bob";

constexpr char16_t kPassword[] = u"password123";

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MOCK_METHOD(void,
              FillIntoFocusedField,
              (bool, const std::u16string&),
              (override));
};

class MockAllPasswordsBottomSheetView : public AllPasswordsBottomSheetView {
 public:
  MOCK_METHOD(void,
              Show,
              (const std::vector<std::unique_ptr<PasswordForm>>&,
               FocusedFieldType),
              (override));
};

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(std::unique_ptr<device_reauth::DeviceAuthenticator>,
              GetDeviceAuthenticator,
              (),
              (override));
  MOCK_METHOD(bool,
              IsReauthBeforeFillingRequired,
              (device_reauth::DeviceAuthenticator*),
              (override));
};

class MockPasswordReuseDetectionManagerClient
    : public safe_browsing::StubPasswordReuseDetectionManagerClient {
 public:
  MOCK_METHOD(void, OnPasswordSelected, (const std::u16string&), (override));
};

}  // namespace

PasswordForm MakeSavedPassword(const std::string& signon_realm,
                               const std::u16string& username) {
  PasswordForm form;
  form.signon_realm = signon_realm;
  form.url = GURL(signon_realm);
  form.username_value = username;
  form.password_value = kPassword;
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

PasswordForm MakePasswordException(const std::string& signon_realm) {
  PasswordForm form;
  form.blocked_by_user = true;
  form.signon_realm = signon_realm;
  form.url = GURL(signon_realm);
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

class AllPasswordsBottomSheetControllerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  AllPasswordsBottomSheetControllerTest() {
    // Make sure that the `kPlusAddressesEnabled` feature is known to be
    // enabled, such that `PlusAddressServiceFactory` doesn't bail early with a
    // null return.
    scoped_feature_list_.InitWithFeatures(
        {password_manager::features::kBiometricTouchToFill,
         plus_addresses::features::kPlusAddressesEnabled},
        {});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PlusAddressServiceFactory::GetInstance()->SetTestingFactory(
        browser_context(),
        base::BindRepeating(&AllPasswordsBottomSheetControllerTest::
                                PlusAddressServiceTestFactory,
                            base::Unretained(this)));
    profile_store_ = CreateAndUseTestPasswordStore(profile());
    profile_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
    createAllPasswordsController(FocusedFieldType::kFillablePasswordField);
  }

  void createAllPasswordsController(
      autofill::mojom::FocusedFieldType focused_field_type) {
    std::unique_ptr<MockAllPasswordsBottomSheetView> mock_view_unique_ptr =
        std::make_unique<MockAllPasswordsBottomSheetView>();
    mock_view_ = mock_view_unique_ptr.get();
    auto access_loss_bridge =
        std::make_unique<MockPasswordAccessLossWarningBridge>();
    mock_access_loss_warning_bridge_ = access_loss_bridge.get();
    all_passwords_controller_ =
        std::make_unique<AllPasswordsBottomSheetController>(
            base::PassKey<AllPasswordsBottomSheetControllerTest>(),
            web_contents(), std::move(mock_view_unique_ptr),
            driver_.AsWeakPtr(), profile_store_.get(), account_store_.get(),
            dissmissal_callback_.Get(), focused_field_type,
            mock_pwd_manager_client_.get(),
            mock_pwd_reuse_detection_manager_client_.get(),
            show_migration_warning_callback_.Get(),
            std::move(access_loss_bridge));
  }

  std::unique_ptr<KeyedService> PlusAddressServiceTestFactory(
      content::BrowserContext* context) {
    return std::make_unique<FakePlusAddressService>();
  }

  void TearDown() override {
    profile_store_->ShutdownOnUIThread();
    if (account_store_) {
      account_store_->ShutdownOnUIThread();
    }
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MockPasswordManagerDriver& driver() { return driver_; }

  TestPasswordStore& profile_store() { return *profile_store_; }
  TestPasswordStore& account_store() { return *account_store_; }

  MockAllPasswordsBottomSheetView& view() { return *mock_view_; }

  AllPasswordsBottomSheetController* all_passwords_controller() {
    return all_passwords_controller_.get();
  }

  DismissCallback& dismissal_callback() { return dissmissal_callback_; }

  void RunUntilIdle() { task_environment()->RunUntilIdle(); }

  MockPasswordManagerClient& client() {
    return *mock_pwd_manager_client_.get();
  }

  MockPasswordReuseDetectionManagerClient&
  password_reuse_detection_manager_client() {
    return *mock_pwd_reuse_detection_manager_client_.get();
  }

  base::MockCallback<
      AllPasswordsBottomSheetController::ShowMigrationWarningCallback>&
  show_migration_warning_callback() {
    return show_migration_warning_callback_;
  }

  MockPasswordAccessLossWarningBridge* mock_access_loss_warning_bridge() {
    return mock_access_loss_warning_bridge_;
  }

 protected:
  MockPasswordManagerDriver driver_;
  scoped_refptr<TestPasswordStore> profile_store_;
  scoped_refptr<TestPasswordStore> account_store_;

  raw_ptr<MockAllPasswordsBottomSheetView> mock_view_;
  DismissCallback dissmissal_callback_;
  std::unique_ptr<AllPasswordsBottomSheetController> all_passwords_controller_;
  std::unique_ptr<MockPasswordManagerClient> mock_pwd_manager_client_ =
      std::make_unique<MockPasswordManagerClient>();
  std::unique_ptr<MockPasswordReuseDetectionManagerClient>
      mock_pwd_reuse_detection_manager_client_ =
          std::make_unique<MockPasswordReuseDetectionManagerClient>();
  base::MockCallback<
      AllPasswordsBottomSheetController::ShowMigrationWarningCallback>
      show_migration_warning_callback_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<MockPasswordAccessLossWarningBridge> mock_access_loss_warning_bridge_;
};

TEST_F(AllPasswordsBottomSheetControllerTest, Show) {
  auto form1 = MakeSavedPassword(kExampleCom, kUsername1);
  auto form2 = MakeSavedPassword(kExampleCom, kUsername2);
  auto form3 = MakeSavedPassword(kExampleOrg, kUsername1);
  auto form4 = MakeSavedPassword(kExampleOrg, kUsername2);

  profile_store().AddLogin(form1);
  profile_store().AddLogin(form2);
  profile_store().AddLogin(form3);
  profile_store().AddLogin(form4);
  // Exceptions are not shown. Sites where saving is disabled still show pwds.
  profile_store().AddLogin(MakePasswordException(kExampleDe));
  profile_store().AddLogin(MakePasswordException(kExampleCom));

  EXPECT_CALL(view(),
              Show(UnorderedElementsAre(Pointee(Eq(form1)), Pointee(Eq(form2)),
                                        Pointee(Eq(form3)), Pointee(Eq(form4))),
                   FocusedFieldType::kFillablePasswordField));
  all_passwords_controller()->Show();

  // Show method uses the store which has async work.
  RunUntilIdle();
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       CallingShowMultipleTimesHasNoEffect) {
  auto form1 = MakeSavedPassword(kExampleCom, kUsername1);
  auto form2 = MakeSavedPassword(kExampleCom, kUsername2);

  profile_store().AddLogin(form1);
  profile_store().AddLogin(form2);

  EXPECT_CALL(view(),
              Show(UnorderedElementsAre(Pointee(Eq(form1)), Pointee(Eq(form2))),
                   FocusedFieldType::kFillablePasswordField))
      .Times(1);
  all_passwords_controller()->Show();
  all_passwords_controller()->Show();
  all_passwords_controller()->Show();

  // Show method uses the store which has async work.
  RunUntilIdle();
}

TEST_F(AllPasswordsBottomSheetControllerTest, FillsUsernameWithoutAuth) {
  createAllPasswordsController(FocusedFieldType::kFillableUsernameField);

  EXPECT_CALL(client(), GetDeviceAuthenticator).Times(0);
  EXPECT_CALL(driver(),
              FillIntoFocusedField(false, std::u16string(kUsername1)));
  EXPECT_CALL(dismissal_callback(), Run());

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(false));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       FillsOnlyUsernameIfNotPasswordFillRequested) {
  EXPECT_CALL(client(), GetDeviceAuthenticator).Times(0);

  EXPECT_CALL(driver(), FillIntoFocusedField(true, std::u16string(kUsername1)));

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(false));
}

TEST_F(AllPasswordsBottomSheetControllerTest, FillsPasswordIfAuthNotAvailable) {
  // Auth is required to fill passwords in Android automotive.
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP();
  }

  auto authenticator = std::make_unique<MockDeviceAuthenticator>();

  EXPECT_CALL(client(), IsReauthBeforeFillingRequired).WillOnce(Return(false));
  EXPECT_CALL(client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));
  EXPECT_CALL(driver(), FillIntoFocusedField(true, std::u16string(kPassword)));
  EXPECT_CALL(dismissal_callback(), Run());

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(true));
}

TEST_F(AllPasswordsBottomSheetControllerTest, FillsPasswordIfAuthSuccessful) {
  auto authenticator = std::make_unique<MockDeviceAuthenticator>();

  ON_CALL(client(), IsReauthBeforeFillingRequired).WillByDefault(Return(true));
  EXPECT_CALL(*authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(true));
  EXPECT_CALL(client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  EXPECT_CALL(driver(), FillIntoFocusedField(true, std::u16string(kPassword)));
  EXPECT_CALL(dismissal_callback(), Run());

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(true));
}

TEST_F(AllPasswordsBottomSheetControllerTest, DoesntFillPasswordIfAuthFailed) {
  auto authenticator = std::make_unique<MockDeviceAuthenticator>();

  ON_CALL(client(), IsReauthBeforeFillingRequired).WillByDefault(Return(true));
  EXPECT_CALL(*authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(false));
  EXPECT_CALL(client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  EXPECT_CALL(driver(), FillIntoFocusedField(true, std::u16string(kPassword)))
      .Times(0);
  EXPECT_CALL(dismissal_callback(), Run());

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(true));
}

TEST_F(AllPasswordsBottomSheetControllerTest, CancelsAuthIfDestroyed) {
  auto authenticator = std::make_unique<MockDeviceAuthenticator>();
  auto* authenticator_ptr = authenticator.get();

  ON_CALL(client(), IsReauthBeforeFillingRequired).WillByDefault(Return(true));
  EXPECT_CALL(*authenticator_ptr, AuthenticateWithMessage);
  EXPECT_CALL(client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(authenticator))));

  EXPECT_CALL(driver(), FillIntoFocusedField(true, std::u16string(kPassword)))
      .Times(0);

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(true));

  EXPECT_CALL(*authenticator_ptr, Cancel());
}

TEST_F(AllPasswordsBottomSheetControllerTest, OnDismiss) {
  EXPECT_CALL(dismissal_callback(), Run());
  all_passwords_controller()->OnDismiss();
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       OnCredentialSelectedTriggersPhishGuard) {
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    auto authenticator = std::make_unique<MockDeviceAuthenticator>();
    ON_CALL(*authenticator, AuthenticateWithMessage)
        .WillByDefault(
            base::test::RunOnceCallbackRepeatedly<1>(/*auth_succeeded=*/true));
    EXPECT_CALL(client(), GetDeviceAuthenticator)
        .WillOnce(Return(testing::ByMove(std::move(authenticator))));
  }

  EXPECT_CALL(password_reuse_detection_manager_client(),
              OnPasswordSelected(std::u16string(kPassword)));

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(true));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       PhishGuardIsNotCalledForUsernameInPasswordField) {
  EXPECT_CALL(password_reuse_detection_manager_client(), OnPasswordSelected)
      .Times(0);

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(false));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       PhishGuardIsNotCalledForUsername) {
  createAllPasswordsController(FocusedFieldType::kFillableUsernameField);
  EXPECT_CALL(password_reuse_detection_manager_client(), OnPasswordSelected)
      .Times(0);

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(false));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       FillsUsernameIfPasswordFillRequestedInNonPasswordField) {
  createAllPasswordsController(FocusedFieldType::kFillableUsernameField);
  EXPECT_CALL(driver(), FillIntoFocusedField(_, _)).Times(0);

  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(true));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       ShowMigrationWarningOnUsernameFillIfEnabled) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  createAllPasswordsController(FocusedFieldType::kFillableUsernameField);
  EXPECT_CALL(show_migration_warning_callback(), Run);
  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(false));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       ShowMigrationWarningOnPasswordFillIfEnabled) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  createAllPasswordsController(FocusedFieldType::kFillablePasswordField);
  EXPECT_CALL(show_migration_warning_callback(),
              Run(_, _,
                  password_manager::metrics_util::
                      PasswordMigrationWarningTriggers::kAllPasswords));
  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(true));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       DoesntTriggersMigrationWarningIfDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsMigrationWarning);
  createAllPasswordsController(FocusedFieldType::kFillableUsernameField);
  EXPECT_CALL(show_migration_warning_callback(), Run).Times(0);
  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(false));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       ShowAccessLossWarningOnUsernameFillIfEnabled) {
  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);
  createAllPasswordsController(FocusedFieldType::kFillableUsernameField);
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              ShouldShowAccessLossNoticeSheet(profile()->GetPrefs(),
                                              /*called_at_startup=*/false))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              MaybeShowAccessLossNoticeSheet(
                  profile()->GetPrefs(), _, profile(),
                  /*called_at_startup=*/false,
                  password_manager_android_util::
                      PasswordAccessLossWarningTriggers::kAllPasswords));
  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(false));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       ShowAccessLossWarningAfterReauthOnPasswordFillIfEnabled) {
  auto mock_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*mock_authenticator, AuthenticateWithMessage)
      .WillOnce([](const std::u16string&,
                   device_reauth::DeviceAuthenticator::AuthenticateCallback
                       callback) { std::move(callback).Run(true); });
  EXPECT_CALL(client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(mock_authenticator))));
  EXPECT_CALL(client(), IsReauthBeforeFillingRequired).WillOnce(Return(true));

  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);
  createAllPasswordsController(FocusedFieldType::kFillablePasswordField);
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              ShouldShowAccessLossNoticeSheet(profile()->GetPrefs(),
                                              /*called_at_startup=*/false))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              MaybeShowAccessLossNoticeSheet(
                  profile()->GetPrefs(), _, profile(),
                  /*called_at_startup=*/false,
                  password_manager_android_util::
                      PasswordAccessLossWarningTriggers::kAllPasswords));
  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(true));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       ShowAccessLossWarningWithoutReauthOnPasswordFillIfEnabled) {
  // Skipped for automotive because reauthentication is always needed there.
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP();
  }

  base::test::ScopedFeatureList scoped_feature_list(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);
  createAllPasswordsController(FocusedFieldType::kFillablePasswordField);
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              ShouldShowAccessLossNoticeSheet(profile()->GetPrefs(),
                                              /*called_at_startup=*/false))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              MaybeShowAccessLossNoticeSheet(
                  profile()->GetPrefs(), _, profile(),
                  /*called_at_startup=*/false,
                  password_manager_android_util::
                      PasswordAccessLossWarningTriggers::kAllPasswords));
  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(true));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       DoesntTriggersAccessLossWarningIfDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);
  createAllPasswordsController(FocusedFieldType::kFillableUsernameField);
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              ShouldShowAccessLossNoticeSheet(profile()->GetPrefs(),
                                              /*called_at_startup=*/false))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_access_loss_warning_bridge(),
              MaybeShowAccessLossNoticeSheet)
      .Times(0);
  all_passwords_controller()->OnCredentialSelected(
      kUsername1, kPassword, RequestsToFillPassword(false));
}

TEST_F(AllPasswordsBottomSheetControllerTest,
       IsPlusAddress_ManualFallbackDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {password_manager::features::kBiometricTouchToFill,
       plus_addresses::features::kPlusAddressesEnabled},
      {plus_addresses::features::kPlusAddressAndroidManualFallbackEnabled});

  // Not a plus address according to the `FakePlusAddressService`.
  EXPECT_FALSE(all_passwords_controller()->IsPlusAddress("exampe@gmail.com"));
  // `kPlusAddressAndroidManualFallbackEnabled` is disabled, `IsPlusAddress()`
  // should return `false` even for existing plus addresses.
  EXPECT_FALSE(all_passwords_controller()->IsPlusAddress(
      plus_addresses::test::kFakePlusAddress));
}

TEST_F(AllPasswordsBottomSheetControllerTest, IsPlusAddress) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {password_manager::features::kBiometricTouchToFill,
       plus_addresses::features::kPlusAddressesEnabled,
       plus_addresses::features::kPlusAddressAndroidManualFallbackEnabled},
      {});

  // Not a plus address according to the `FakePlusAddressService`.
  EXPECT_FALSE(all_passwords_controller()->IsPlusAddress("exampe@gmail.com"));
  // `kPlusAddressAndroidManualFallbackEnabled` is disabled, `IsPlusAddress()`
  // should return `false` even for existing plus addresses.
  EXPECT_TRUE(all_passwords_controller()->IsPlusAddress(
      plus_addresses::test::kFakePlusAddress));
}

class AllPasswordsBottomSheetControllerAccountStoreTest
    : public AllPasswordsBottomSheetControllerTest {
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile()->GetPrefs()->SetInteger(
        password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores, 2);
    profile_store_ = CreateAndUseTestPasswordStore(profile());
    profile_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
    account_store_ = CreateAndUseTestAccountPasswordStore(profile());
    account_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
    createAllPasswordsController(FocusedFieldType::kFillablePasswordField);
  }
};

TEST_F(AllPasswordsBottomSheetControllerAccountStoreTest,
       PasswordsFromBothStores) {
  auto form1 = MakeSavedPassword(kExampleCom, kUsername1);
  auto form2 = MakeSavedPassword(kExampleCom, kUsername2);
  auto form3 = MakeSavedPassword(kExampleOrg, kUsername1);
  auto form4 = MakeSavedPassword(kExampleOrg, kUsername2);

  profile_store().AddLogin(form1);
  account_store().AddLogin(form2);
  account_store().AddLogin(form3);
  profile_store().AddLogin(form4);
  // Exceptions are not shown.
  profile_store().AddLogin(MakePasswordException(kExampleCom));
  account_store().AddLogin(MakePasswordException(kExampleCom));

  form2.in_store = password_manager::PasswordForm::Store::kAccountStore;
  form3.in_store = password_manager::PasswordForm::Store::kAccountStore;

  EXPECT_CALL(view(),
              Show(UnorderedElementsAre(Pointee(Eq(form1)), Pointee(Eq(form2)),
                                        Pointee(Eq(form3)), Pointee(Eq(form4))),
                   FocusedFieldType::kFillablePasswordField));
  all_passwords_controller()->Show();

  // Show method uses the store which has async work.
  RunUntilIdle();
}

TEST_F(AllPasswordsBottomSheetControllerAccountStoreTest,
       PasswordsFromAccountStoreOnly) {
  auto form1 = MakeSavedPassword(kExampleCom, kUsername1);
  auto form2 = MakeSavedPassword(kExampleCom, kUsername2);

  account_store().AddLogin(form1);
  account_store().AddLogin(form2);
  // Exceptions are not shown.
  account_store().AddLogin(MakePasswordException(kExampleCom));

  form1.in_store = password_manager::PasswordForm::Store::kAccountStore;
  form2.in_store = password_manager::PasswordForm::Store::kAccountStore;

  EXPECT_CALL(view(),
              Show(UnorderedElementsAre(Pointee(Eq(form1)), Pointee(Eq(form2))),
                   FocusedFieldType::kFillablePasswordField));
  all_passwords_controller()->Show();

  // Show method uses the store which has async work.
  RunUntilIdle();
}

TEST_F(AllPasswordsBottomSheetControllerAccountStoreTest, BothStoresEmpty) {
  EXPECT_CALL(view(), Show(testing::IsEmpty(),
                           FocusedFieldType::kFillablePasswordField));
  all_passwords_controller()->Show();

  // Show method uses the store which has async work.
  RunUntilIdle();
}

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keyboard_accessory/android/password_accessory_controller_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/build_info.h"
#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/keyboard_accessory/android/accessory_controller.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_enums.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_affiliated_plus_profiles_provider.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_manual_filling_controller.h"
#include "chrome/browser/password_manager/android/access_loss/mock_password_access_loss_warning_bridge.h"
#include "chrome/browser/password_manager/android/password_generation_controller.h"
#include "chrome/browser/password_manager/android/password_generation_controller_impl.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/mock_password_manager.h"
#include "components/password_manager/core/browser/mock_webauthn_credentials_delegate.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/webauthn_credentials_delegate.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/plus_addresses/fake_plus_address_service.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/resources/android/theme_resources.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/webauthn/android/cred_man_support.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#include "components/webauthn/android/webauthn_cred_man_delegate_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
using autofill::AccessoryAction;
using autofill::AccessorySheetData;
using autofill::AccessorySheetField;
using autofill::AccessoryTabType;
using autofill::FooterCommand;
using autofill::UserInfo;
using autofill::mojom::FocusedFieldType;
using base::test::RunOnceCallback;
using device_reauth::MockDeviceAuthenticator;
using password_manager::CreateEntry;
using password_manager::CredentialCache;
using password_manager::MockPasswordManager;
using password_manager::MockPasswordStoreInterface;
using password_manager::OriginCredentialStore;
using password_manager::PasskeyCredential;
using password_manager::PasswordForm;
using password_manager::PasswordGenerationFrameHelper;
using password_manager::PasswordManagerClient;
using password_manager::PasswordManagerDriver;
using password_manager::PasswordManagerInterface;
using password_manager::PasswordStoreInterface;
using password_manager::TestPasswordStore;
using plus_addresses::FakePlusAddressService;
using plus_addresses::PlusProfile;
using testing::_;
using testing::ByMove;
using testing::Eq;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;
using testing::StrictMock;
using webauthn::CredManSupport;
using webauthn::WebAuthnCredManDelegate;
using FillingSource = ManualFillingController::FillingSource;
using IsFillingSourceAvailable = AccessoryController::IsFillingSourceAvailable;
using IsExactMatch = autofill::UserInfo::IsExactMatch;
using ShouldShowAction = ManualFillingController::ShouldShowAction;

constexpr char kExampleSite[] = "https://example.com";
constexpr char kExampleHttpSite[] = "http://example.com";
constexpr char16_t kExampleHttpSite16[] = u"http://example.com";
constexpr char kExampleSiteMobile[] = "https://m.example.com";
constexpr char kExampleSignonRealm[] = "https://example.com/";
constexpr char16_t kExampleDomain[] = u"example.com";
constexpr char16_t kUsername[] = u"alice";
constexpr char16_t kPassword[] = u"password123";
const std::optional<std::vector<PasskeyCredential>> kNoPasskeys = std::nullopt;

class MockPasswordGenerationController
    : public PasswordGenerationControllerImpl {
 public:
  static void CreateForWebContents(content::WebContents* web_contents);

  explicit MockPasswordGenerationController(content::WebContents* web_contents);

  MOCK_METHOD(void,
              OnGenerationRequested,
              (autofill::password_generation::PasswordGenerationType));
};

// static
void MockPasswordGenerationController::CreateForWebContents(
    content::WebContents* web_contents) {
  ASSERT_FALSE(FromWebContents(web_contents));
  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new MockPasswordGenerationController(web_contents)));
}

MockPasswordGenerationController::MockPasswordGenerationController(
    content::WebContents* web_contents)
    : PasswordGenerationControllerImpl(web_contents) {}

class MockPasswordGenerationFrameHelper : public PasswordGenerationFrameHelper {
 public:
  MockPasswordGenerationFrameHelper(PasswordManagerClient* client,
                                    PasswordManagerDriver* driver)
      : PasswordGenerationFrameHelper(client, driver) {}
  MOCK_METHOD(bool, IsGenerationEnabled, (bool), (const override));
};

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MockPasswordManagerClient(PasswordStoreInterface* account_password_store,
                            PasswordStoreInterface* profile_password_store)
      : account_password_store_(account_password_store),
        profile_password_store_(profile_password_store) {}

  MOCK_METHOD(void, UpdateFormManagers, (), (override));

  MOCK_METHOD(bool,
              IsSavingAndFillingEnabled,
              (const GURL&),
              (const, override));

  MOCK_METHOD(std::unique_ptr<device_reauth::DeviceAuthenticator>,
              GetDeviceAuthenticator,
              (),
              (override));

  MOCK_METHOD(bool,
              IsReauthBeforeFillingRequired,
              (device_reauth::DeviceAuthenticator*),
              (override));

  MOCK_METHOD(password_manager::WebAuthnCredentialsDelegate*,
              GetWebAuthnCredentialsDelegateForDriver,
              (password_manager::PasswordManagerDriver*),
              (override));

  MOCK_METHOD(webauthn::WebAuthnCredManDelegate*,
              GetWebAuthnCredManDelegateForDriver,
              (password_manager::PasswordManagerDriver*),
              (override));

  MOCK_METHOD(const PasswordManagerInterface*,
              GetPasswordManager,
              (),
              (const override));

  password_manager::PasswordStoreInterface* GetAccountPasswordStore()
      const override {
    return account_password_store_;
  }

  password_manager::PasswordStoreInterface* GetProfilePasswordStore()
      const override {
    return profile_password_store_;
  }

 private:
  raw_ptr<PasswordStoreInterface> account_password_store_;
  raw_ptr<PasswordStoreInterface> profile_password_store_;
};

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MOCK_METHOD(void,
              FillIntoFocusedField,
              (bool, const std::u16string&),
              (override));
  MOCK_METHOD(PasswordGenerationFrameHelper*,
              GetPasswordGenerationHelper,
              (),
              (override));
};

class MockAutofillClient : public autofill::TestContentAutofillClient {
 public:
  using autofill::TestContentAutofillClient::TestContentAutofillClient;
  MOCK_METHOD(void,
              OfferPlusAddressCreation,
              (const url::Origin&, autofill::PlusAddressCallback),
              (override));
};

std::u16string password_for_str(const std::u16string& user) {
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_DESCRIPTION, user);
}

std::u16string passwords_empty_str(const std::u16string& domain) {
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_EMPTY_MESSAGE, domain);
}

std::u16string passwords_title(const std::u16string& domain) {
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_TITLE, domain);
}

std::u16string plus_address_title(const std::u16string& domain) {
  return l10n_util::GetStringFUTF16(
      IDS_PLUS_ADDRESS_FALLBACK_MANUAL_FILLING_SHEET_TITLE, domain);
}

std::u16string no_user_str() {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
}

std::u16string show_other_passwords_str() {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_SELECT_PASSWORD);
}

std::u16string manage_passwords_str() {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_ALL_PASSWORDS_LINK);
}

std::u16string manage_passwords_and_passkeys_str() {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_ALL_PASSWORDS_AND_PASSKEYS_LINK);
}

std::u16string generate_password_str() {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_GENERATE_PASSWORD_BUTTON_TITLE);
}

std::u16string cross_device_passkeys_str() {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_USE_DEVICE_PASSKEY);
}

std::u16string select_passkey_str() {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_SELECT_PASSKEY);
}

std::u16string generate_plus_address_str() {
  return l10n_util::GetStringUTF16(
      IDS_PLUS_ADDRESS_CREATE_NEW_PLUS_ADDRESSES_LINK_ANDROID);
}

std::u16string select_plus_address_str() {
  return l10n_util::GetStringUTF16(
      IDS_PLUS_ADDRESS_SELECT_PLUS_ADDRESS_LINK_ANDROID);
}

// Creates a AccessorySheetDataBuilder object with a "Manage passwords..."
// footer.
AccessorySheetData::Builder PasswordAccessorySheetDataBuilder(
    const std::u16string& user_info_title,
    const std::u16string plus_address_title = u"") {
  return AccessorySheetData::Builder(AccessoryTabType::PASSWORDS,
                                     user_info_title, plus_address_title)
      .AppendFooterCommand(manage_passwords_str(),
                           autofill::AccessoryAction::MANAGE_PASSWORDS);
}

AccessorySheetData::Builder PasswordAccessorySheetDataBuilderEmptyTitle() {
  return PasswordAccessorySheetDataBuilder(std::u16string());
}

PasswordForm MakeSavedPassword() {
  PasswordForm form;
  form.signon_realm = std::string(kExampleSite);
  form.url = GURL(kExampleSite);
  form.username_value = kUsername;
  form.password_value = kPassword;
  form.username_element = u"";
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

std::unique_ptr<KeyedService> BuildFakePlusAddressService(
    content::BrowserContext* context) {
  return std::make_unique<FakePlusAddressService>();
}

}  // namespace

class PasswordAccessoryControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  PasswordAccessoryControllerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    features_.InitWithFeatures(
        {plus_addresses::features::kPlusAddressesEnabled,
         plus_addresses::features::kPlusAddressAndroidManualFallbackEnabled},
        {});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    PlusAddressServiceFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(), base::BindRepeating(&BuildFakePlusAddressService));

    NavigateAndCommit(GURL(kExampleSite));
    FocusWebContentsOnMainFrame();

    ASSERT_TRUE(web_contents()->GetFocusedFrame());
    ASSERT_EQ(url::Origin::Create(GURL(kExampleSite)),
              web_contents()->GetFocusedFrame()->GetLastCommittedOrigin());

    MockPasswordGenerationController::CreateForWebContents(web_contents());
    profile()->GetPrefs()->SetInteger(
        password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(
            password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));
    mock_pwd_manager_client_ =
        std::make_unique<NiceMock<MockPasswordManagerClient>>(
            CreateInternalAccountPasswordStore(),
            CreateInternalProfilePasswordStore());
    mock_frame_helper_ =
        std::make_unique<NiceMock<MockPasswordGenerationFrameHelper>>(
            mock_pwd_manager_client_.get(), &mock_driver_);
    ON_CALL(mock_driver_, GetPasswordGenerationHelper)
        .WillByDefault(Return(mock_frame_helper_.get()));
    ON_CALL(*mock_pwd_manager_client_, GetPasswordManager)
        .WillByDefault(Return(&mock_password_manager_));
    NavigateAndCommit(GURL(kExampleSite));

    webauthn_credentials_delegate_ = std::make_unique<
        NiceMock<password_manager::MockWebAuthnCredentialsDelegate>>();
    ON_CALL(*webauthn_credentials_delegate(), GetPasskeys)
        .WillByDefault(ReturnRef(kNoPasskeys));
    ON_CALL(*password_client(), GetWebAuthnCredentialsDelegateForDriver)
        .WillByDefault(Return(webauthn_credentials_delegate()));
    ON_CALL(*password_client(), GetWebAuthnCredManDelegateForDriver)
        .WillByDefault(Return(cred_man_delegate()));
    ON_CALL(*webauthn_credentials_delegate(),
            IsSecurityKeyOrHybridFlowAvailable)
        .WillByDefault(Return(false));
    ON_CALL(*password_client()->GetPasswordFeatureManager(),
            IsOptedInForAccountStorage)
        .WillByDefault(Return(false));
    ON_CALL(*password_client()->GetPasswordFeatureManager(),
            GetDefaultPasswordStore)
        .WillByDefault(Return(PasswordForm::Store::kProfileStore));
  }

  webauthn::WebAuthnCredManDelegate* cred_man_delegate() {
    return webauthn::WebAuthnCredManDelegateFactory::GetFactory(web_contents())
        ->GetRequestDelegate(web_contents()->GetPrimaryMainFrame());
  }

  void CreateSheetController(
      security_state::SecurityLevel security_level = security_state::SECURE) {
    auto access_loss_bridge =
        std::make_unique<MockPasswordAccessLossWarningBridge>();
    mock_access_loss_warning_bridge_ = access_loss_bridge.get();
    PasswordAccessoryControllerImpl::CreateForWebContentsForTesting(
        web_contents(), cache(), mock_manual_filling_controller_.AsWeakPtr(),
        mock_pwd_manager_client_.get(),
        base::BindRepeating(&PasswordAccessoryControllerTest::GetBaseDriver,
                            base::Unretained(this)),
        show_migration_warning_callback_.Get(), std::move(access_loss_bridge));

    controller()->RegisterFillingSourceObserver(filling_source_observer_.Get());
    controller()->SetSecurityLevelForTesting(security_level);
  }

  PasswordAccessoryControllerImpl* controller() {
    return PasswordAccessoryControllerImpl::FromWebContents(web_contents());
  }

  password_manager::CredentialCache* cache() { return &credential_cache_; }

  MockPasswordManagerClient* password_client() {
    return mock_pwd_manager_client_.get();
  }

  MockPasswordManagerDriver* driver() { return &mock_driver_; }

  MockPasswordGenerationFrameHelper& frame_helper() {
    return *mock_frame_helper_;
  }

  MockPasswordManager& password_manager() { return mock_password_manager_; }

  password_manager::MockWebAuthnCredentialsDelegate*
  webauthn_credentials_delegate() {
    return webauthn_credentials_delegate_.get();
  }

  MockAutofillClient& autofill_client() {
    return *autofill_client_injector_[web_contents()];
  }

  FakePlusAddressService& plus_address_service() {
    return *static_cast<FakePlusAddressService*>(
        PlusAddressServiceFactory::GetForBrowserContext(
            web_contents()->GetBrowserContext()));
  }

 protected:
  virtual PasswordStoreInterface* CreateInternalAccountPasswordStore() {
    mock_account_password_store_ =
        base::MakeRefCounted<NiceMock<MockPasswordStoreInterface>>();
    return mock_account_password_store_.get();
  }

  virtual PasswordStoreInterface* CreateInternalProfilePasswordStore() {
    mock_profile_password_store_ =
        base::MakeRefCounted<NiceMock<MockPasswordStoreInterface>>();
    return mock_profile_password_store_.get();
  }

  base::test::ScopedFeatureList features_;
  StrictMock<MockManualFillingController> mock_manual_filling_controller_;
  base::MockCallback<AccessoryController::FillingSourceObserver>
      filling_source_observer_;
  base::MockCallback<
      PasswordAccessoryControllerImpl::ShowMigrationWarningCallback>
      show_migration_warning_callback_;
  raw_ptr<MockPasswordAccessLossWarningBridge> mock_access_loss_warning_bridge_;
  scoped_refptr<MockPasswordStoreInterface> mock_account_password_store_;
  scoped_refptr<MockPasswordStoreInterface> mock_profile_password_store_;

 private:
  password_manager::PasswordManagerDriver* GetBaseDriver(
      content::WebContents*) {
    return driver();
  }

  password_manager::CredentialCache credential_cache_;
  std::unique_ptr<MockPasswordManagerClient> mock_pwd_manager_client_;
  NiceMock<MockPasswordManagerDriver> mock_driver_;
  NiceMock<MockPasswordManager> mock_password_manager_;
  std::unique_ptr<MockPasswordGenerationFrameHelper> mock_frame_helper_;
  std::unique_ptr<password_manager::MockWebAuthnCredentialsDelegate>
      webauthn_credentials_delegate_;
  autofill::TestAutofillClientInjector<NiceMock<MockAutofillClient>>
      autofill_client_injector_;
};

TEST_F(PasswordAccessoryControllerTest, IsNotRecreatedForSameWebContents) {
  CreateSheetController();
  PasswordAccessoryControllerImpl* initial_controller =
      PasswordAccessoryControllerImpl::FromWebContents(web_contents());
  EXPECT_NE(nullptr, initial_controller);
  PasswordAccessoryControllerImpl::CreateForWebContents(web_contents(),
                                                        cache());
  EXPECT_EQ(PasswordAccessoryControllerImpl::FromWebContents(web_contents()),
            initial_controller);
}

TEST_F(PasswordAccessoryControllerTest, TransformsMatchesToSuggestions) {
  CreateSheetController();
  std::vector<PasswordForm> matches = {CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  EXPECT_EQ(controller()->GetSheetData(),
            PasswordAccessorySheetDataBuilderEmptyTitle()
                .AddUserInfo(kExampleSite)
                .AppendField(u"Ben", u"Ben", false, true)
                .AppendField(u"S3cur3", password_for_str(u"Ben"), true, false)
                .Build());
}

TEST_F(PasswordAccessoryControllerTest, HintsToEmptyUserNames) {
  CreateSheetController();
  std::vector<PasswordForm> matches = {CreateEntry(
      "", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  EXPECT_EQ(
      controller()->GetSheetData(),
      PasswordAccessorySheetDataBuilderEmptyTitle()
          .AddUserInfo(kExampleSite)
          .AppendField(no_user_str(), no_user_str(), false, false)
          .AppendField(u"S3cur3", password_for_str(no_user_str()), true, false)
          .Build());
}

TEST_F(PasswordAccessoryControllerTest, SortsAlphabeticalDuringTransform) {
  CreateSheetController();
  std::vector<PasswordForm> matches = {
      CreateEntry("Ben", "S3cur3", GURL(kExampleSite),
                  PasswordForm::MatchType::kExact),
      CreateEntry("Zebra", "M3h", GURL(kExampleSite),
                  PasswordForm::MatchType::kExact),
      CreateEntry("Alf", "PWD", GURL(kExampleSite),
                  PasswordForm::MatchType::kExact),
      CreateEntry("Cat", "M1@u", GURL(kExampleSite),
                  PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  EXPECT_EQ(controller()->GetSheetData(),
            PasswordAccessorySheetDataBuilderEmptyTitle()
                .AddUserInfo(kExampleSite)
                .AppendField(u"Alf", u"Alf", false, true)
                .AppendField(u"PWD", password_for_str(u"Alf"), true, false)
                .AddUserInfo(kExampleSite)
                .AppendField(u"Ben", u"Ben", false, true)
                .AppendField(u"S3cur3", password_for_str(u"Ben"), true, false)
                .AddUserInfo(kExampleSite)
                .AppendField(u"Cat", u"Cat", false, true)
                .AppendField(u"M1@u", password_for_str(u"Cat"), true, false)
                .AddUserInfo(kExampleSite)
                .AppendField(u"Zebra", u"Zebra", false, true)
                .AppendField(u"M3h", password_for_str(u"Zebra"), true, false)
                .Build());
}

TEST_F(PasswordAccessoryControllerTest, RepeatsSuggestionsForSameFrame) {
  CreateSheetController();
  std::vector<PasswordForm> matches = {CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  // Pretend that any input in the same frame was focused.
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  EXPECT_EQ(controller()->GetSheetData(),
            PasswordAccessorySheetDataBuilderEmptyTitle()
                .AddUserInfo(kExampleSite)
                .AppendField(u"Ben", u"Ben", false, true)
                .AppendField(u"S3cur3", password_for_str(u"Ben"), true, false)
                .Build());
}

TEST_F(PasswordAccessoryControllerTest, ProvidesEmptySuggestionsMessage) {
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  EXPECT_EQ(
      controller()->GetSheetData(),
      PasswordAccessorySheetDataBuilder(passwords_empty_str(kExampleDomain))
          .Build());
}

TEST_F(PasswordAccessoryControllerTest, PasswordFieldChangesSuggestionType) {
  CreateSheetController();
  std::vector<password_manager::PasswordForm> matches = {
      CreateEntry("Ben", "S3cur3", GURL(kExampleSite),
                  PasswordForm::MatchType::kExact),
      CreateEntry("", "p455w0rd", GURL(kExampleSite),
                  PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  // Pretend a username field was focused. This should result in non-interactive
  // suggestion.
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  EXPECT_EQ(controller()->GetSheetData(),
            PasswordAccessorySheetDataBuilderEmptyTitle()
                .AddUserInfo(kExampleSite)
                .AppendField(u"No username", u"No username", false, false)
                .AppendField(u"p455w0rd", password_for_str(u"No username"),
                             true, false)
                .AddUserInfo(kExampleSite)
                .AppendField(u"Ben", u"Ben", false, true)
                .AppendField(u"S3cur3", password_for_str(u"Ben"), true, false)
                .Build());

  // Pretend that we focus a password field now: By triggering a refresh with
  // |is_password_field| set to true, all suggestions other than the empty
  // username should become interactive.
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField);

  EXPECT_EQ(controller()->GetSheetData(),
            PasswordAccessorySheetDataBuilderEmptyTitle()
                .AddUserInfo(kExampleSite)
                .AppendField(u"No username", u"No username", false, false)
                .AppendField(u"p455w0rd", password_for_str(u"No username"),
                             true, true)
                .AddUserInfo(kExampleSite)
                .AppendField(u"Ben", u"Ben", false, true)
                .AppendField(u"S3cur3", password_for_str(u"Ben"), true, true)
                .Build());
}

TEST_F(PasswordAccessoryControllerTest, CacheChangesReplacePasswords) {
  CreateSheetController();
  std::vector<PasswordForm> matches = {CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);
  EXPECT_EQ(controller()->GetSheetData(),
            PasswordAccessorySheetDataBuilderEmptyTitle()
                .AddUserInfo(kExampleSite)
                .AppendField(u"Ben", u"Ben", false, true)
                .AppendField(u"S3cur3", password_for_str(u"Ben"), true, false)
                .Build());

  std::vector<PasswordForm> changed_matches = {CreateEntry(
      "Alf", "M3lm4k", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      changed_matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);
  EXPECT_EQ(controller()->GetSheetData(),
            PasswordAccessorySheetDataBuilderEmptyTitle()
                .AddUserInfo(kExampleSite)
                .AppendField(u"Alf", u"Alf", false, true)
                .AppendField(u"M3lm4k", password_for_str(u"Alf"), true, false)
                .Build());
}

TEST_F(PasswordAccessoryControllerTest, SetsTitleForPSLMatchedOriginsInV2) {
  CreateSheetController();
  std::vector<PasswordForm> matches = {
      CreateEntry("Ben", "S3cur3", GURL(kExampleSite),
                  PasswordForm::MatchType::kExact),
      CreateEntry("Alf", "R4nd0m", GURL(kExampleSiteMobile),
                  PasswordForm::MatchType::kPSL)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  EXPECT_EQ(controller()->GetSheetData(),
            PasswordAccessorySheetDataBuilderEmptyTitle()
                .AddUserInfo(kExampleSite)
                .AppendField(u"Ben", u"Ben",
                             /*is_obfuscated=*/false, /*selectable=*/true)
                .AppendField(u"S3cur3", password_for_str(u"Ben"),
                             /*is_obfuscated=*/true, /*selectable=*/false)
                .AddUserInfo(kExampleSiteMobile, IsExactMatch(false))
                .AppendField(u"Alf", u"Alf",
                             /*is_obfuscated=*/false, /*selectable=*/true)
                .AppendField(u"R4nd0m", password_for_str(u"Alf"),
                             /*is_obfuscated=*/true, /*selectable=*/false)
                .Build());
}

TEST_F(PasswordAccessoryControllerTest, UnfillableFieldClearsSuggestions) {
  CreateSheetController();
  std::vector<PasswordForm> matches = {CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  // Pretend a username field was focused. This should result in non-emtpy
  // suggestions.
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  EXPECT_EQ(controller()->GetSheetData(),
            PasswordAccessorySheetDataBuilderEmptyTitle()
                .AddUserInfo(kExampleSite)
                .AppendField(u"Ben", u"Ben", false, true)
                .AppendField(u"S3cur3", password_for_str(u"Ben"), true, false)
                .Build());

  // Pretend that the focus was lost or moved to an unfillable field. Now, only
  // the empty state message should be sent.
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kUnfillableElement);

  EXPECT_EQ(
      controller()->GetSheetData(),
      PasswordAccessorySheetDataBuilder(passwords_empty_str(kExampleDomain))
          .Build());
}

TEST_F(PasswordAccessoryControllerTest, NavigatingMainFrameClearsSuggestions) {
  CreateSheetController();
  // Set any, non-empty password list and pretend a username field was focused.
  // This should result in non-emtpy suggestions.
  std::vector<PasswordForm> matches = {CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  EXPECT_EQ(controller()->GetSheetData(),
            PasswordAccessorySheetDataBuilderEmptyTitle()
                .AddUserInfo(kExampleSite)
                .AppendField(u"Ben", u"Ben", false, true)
                .AppendField(u"S3cur3", password_for_str(u"Ben"), true, false)
                .Build());

  // Pretend that the focus was lost or moved to an unfillable field.
  NavigateAndCommit(GURL("https://random.other-site.org/"));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kUnfillableElement);

  // Now, only the empty state message should be sent.
  EXPECT_EQ(controller()->GetSheetData(),
            PasswordAccessorySheetDataBuilder(
                passwords_empty_str(u"random.other-site.org"))
                .Build());
}

TEST_F(PasswordAccessoryControllerTest, OnAutomaticGenerationRequested) {
  CreateSheetController();
  MockPasswordGenerationController* mock_pwd_generation_controller =
      static_cast<MockPasswordGenerationController*>(
          PasswordGenerationController::GetIfExisting(web_contents()));
  EXPECT_CALL(
      *mock_pwd_generation_controller,
      OnGenerationRequested(
          autofill::password_generation::PasswordGenerationType::kAutomatic));
  controller()->OnGenerationRequested(
      autofill::password_generation::PasswordGenerationType::kAutomatic);
}

TEST_F(PasswordAccessoryControllerTest, AddsGenerationCommandWhenAvailable) {
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  ON_CALL(password_manager(), HaveFormManagersReceivedData)
      .WillByDefault(Return(true));
  ON_CALL(frame_helper(), IsGenerationEnabled).WillByDefault(Return(true));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField);

  EXPECT_EQ(
      controller()->GetSheetData(),
      AccessorySheetData::Builder(AccessoryTabType::PASSWORDS,
                                  passwords_empty_str(kExampleDomain),
                                  /*plus_address_title=*/std::u16string())
          .AppendFooterCommand(
              generate_password_str(),
              autofill::AccessoryAction::GENERATE_PASSWORD_MANUAL)
          .AppendFooterCommand(manage_passwords_str(),
                               autofill::AccessoryAction::MANAGE_PASSWORDS)
          .Build());
}

TEST_F(PasswordAccessoryControllerTest,
       NoGenerationCommandIfGenerationIsNotEnabled) {
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  ON_CALL(password_manager(), HaveFormManagersReceivedData)
      .WillByDefault(Return(false));
  ON_CALL(frame_helper(), IsGenerationEnabled).WillByDefault(Return(true));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  EXPECT_EQ(
      controller()->GetSheetData(),
      PasswordAccessorySheetDataBuilder(passwords_empty_str(kExampleDomain))
          .Build());
}

TEST_F(PasswordAccessoryControllerTest, NoGenerationCommandIfNoFormsReceived) {
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  ON_CALL(password_manager(), HaveFormManagersReceivedData)
      .WillByDefault(Return(true));
  ON_CALL(frame_helper(), IsGenerationEnabled).WillByDefault(Return(false));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  EXPECT_EQ(
      controller()->GetSheetData(),
      PasswordAccessorySheetDataBuilder(passwords_empty_str(kExampleDomain))
          .Build());
}

TEST_F(PasswordAccessoryControllerTest, OnManualGenerationRequested) {
  CreateSheetController();
  MockPasswordGenerationController* mock_pwd_generation_controller =
      static_cast<MockPasswordGenerationController*>(
          PasswordGenerationController::GetIfExisting(web_contents()));
  EXPECT_CALL(mock_manual_filling_controller_, Hide());
  EXPECT_CALL(
      *mock_pwd_generation_controller,
      OnGenerationRequested(
          autofill::password_generation::PasswordGenerationType::kManual));
  controller()->OnOptionSelected(AccessoryAction::GENERATE_PASSWORD_MANUAL);
}

TEST_F(PasswordAccessoryControllerTest, AddsSaveToggleIfIsBlocklisted) {
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(true),
      url::Origin::Create(GURL(kExampleSite)));
  ON_CALL(*password_client(), IsSavingAndFillingEnabled(GURL(kExampleSite)))
      .WillByDefault(Return(true));

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField);

  EXPECT_EQ(
      controller()->GetSheetData(),
      PasswordAccessorySheetDataBuilder(passwords_empty_str(kExampleDomain))
          .SetOptionToggle(
              l10n_util::GetStringUTF16(IDS_PASSWORD_SAVING_STATUS_TOGGLE),
              false, autofill::AccessoryAction::TOGGLE_SAVE_PASSWORDS)
          .Build());
}

TEST_F(PasswordAccessoryControllerTest,
       NoSaveToggleIfIsBlocklistedAndSavingDisabled) {
  CreateSheetController();

  // Simulate saving being disabled (e.g. being in incognito or having password
  // saving disabled from settings).
  ON_CALL(*password_client(), IsSavingAndFillingEnabled(GURL(kExampleSite)))
      .WillByDefault(Return(false));

  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(true),
      url::Origin::Create(GURL(kExampleSite)));

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(false)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField);

  EXPECT_EQ(
      controller()->GetSheetData(),
      PasswordAccessorySheetDataBuilder(passwords_empty_str(kExampleDomain))
          .Build());
}

TEST_F(PasswordAccessoryControllerTest, AddsSaveToggleIfWasBlocklisted) {
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(true),
      url::Origin::Create(GURL(kExampleSite)));
  // Simulate unblocklisting.
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));
  ON_CALL(*password_client(), IsSavingAndFillingEnabled(GURL(kExampleSite)))
      .WillByDefault(Return(true));

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField);

  EXPECT_EQ(
      controller()->GetSheetData(),
      PasswordAccessorySheetDataBuilder(passwords_empty_str(kExampleDomain))
          .SetOptionToggle(
              l10n_util::GetStringUTF16(IDS_PASSWORD_SAVING_STATUS_TOGGLE),
              true, autofill::AccessoryAction::TOGGLE_SAVE_PASSWORDS)
          .Build());
}

TEST_F(PasswordAccessoryControllerTest, AddsSaveToggleOnAnyFieldIfBlocked) {
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(true),
      url::Origin::Create(GURL(kExampleSite)));
  ON_CALL(*password_client(), IsSavingAndFillingEnabled(GURL(kExampleSite)))
      .WillByDefault(Return(true));

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableNonSearchField);

  EXPECT_EQ(
      controller()->GetSheetData(),
      PasswordAccessorySheetDataBuilder(passwords_empty_str(kExampleDomain))
          .SetOptionToggle(
              l10n_util::GetStringUTF16(IDS_PASSWORD_SAVING_STATUS_TOGGLE),
              false, autofill::AccessoryAction::TOGGLE_SAVE_PASSWORDS)
          .Build());
}

TEST_F(PasswordAccessoryControllerTest, AppendsPlusAddressSuggestions) {
  CreateSheetController();

  MockAffiliatedPlusProfilesProvider provider;
  EXPECT_CALL(provider, AddObserver(controller()));
  controller()->RegisterPlusProfilesProvider(provider.GetWeakPtr());

  // Provide 1 plus address, which is not used as a username in any credential.
  // It should appear as a standalone suggestion in the password sheet.
  std::vector<PlusProfile> profiles{plus_addresses::test::CreatePlusProfile(
      /*plus_address=*/"example@gmail", /*is_confirmed=*/true)};
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  EXPECT_CALL(provider, GetAffiliatedPlusProfiles)
      .WillRepeatedly(Return(base::span(profiles)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableNonSearchField);

  EXPECT_EQ(
      controller()->GetSheetData(),
      PasswordAccessorySheetDataBuilder(passwords_empty_str(kExampleDomain),
                                        plus_address_title(kExampleDomain))
          .AddPlusAddressInfo("https://foo.com", u"example@gmail")
          .AppendFooterCommand(
              l10n_util::GetStringUTF16(
                  IDS_PLUS_ADDRESS_MANAGE_PLUS_ADDRESSES_LINK_ANDROID),
              AccessoryAction::MANAGE_PLUS_ADDRESS_FROM_PASSWORD_SHEET)
          .Build());
}

TEST_F(PasswordAccessoryControllerTest, PlusAddressUsedAsUsername) {
  CreateSheetController();

  std::vector<PasswordForm> matches = {
      CreateEntry("example@gmail", "S3cur3", GURL(kExampleSite),
                  PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  MockAffiliatedPlusProfilesProvider provider;
  EXPECT_CALL(provider, AddObserver(controller()));
  controller()->RegisterPlusProfilesProvider(provider.GetWeakPtr());

  // Provide 1 plus address, which is used as a username in the saved
  // credential. It should not appear as a standalone suggestion in the password
  // sheet.
  std::vector<PlusProfile> profiles{plus_addresses::test::CreatePlusProfile(
      /*plus_address=*/"example@gmail", /*is_confirmed=*/true)};
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  EXPECT_CALL(provider, GetAffiliatedPlusProfiles)
      .WillRepeatedly(Return(base::make_span(profiles)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableNonSearchField);

  EXPECT_EQ(
      controller()->GetSheetData(),
      PasswordAccessorySheetDataBuilderEmptyTitle()
          .AddUserInfo(kExampleSite)
          .AppendField(
              u"example@gmail", u"example@gmail", u"example@gmail", "",
              ResourceMapper::MapToJavaDrawableId(IDR_AUTOFILL_PLUS_ADDRESS),
              false, true)
          .AppendField(u"S3cur3", password_for_str(u"example@gmail"), true,
                       false)
          .AppendFooterCommand(
              l10n_util::GetStringUTF16(
                  IDS_PLUS_ADDRESS_MANAGE_PLUS_ADDRESSES_LINK_ANDROID),
              AccessoryAction::MANAGE_PLUS_ADDRESS_FROM_PASSWORD_SHEET)
          .Build());
}

TEST_F(PasswordAccessoryControllerTest, BothPlusAddressAndCredentialShown) {
  CreateSheetController();

  std::vector<PasswordForm> matches = {
      CreateEntry("foo.bar@gmail", "S3cur3", GURL(kExampleSite),
                  PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  MockAffiliatedPlusProfilesProvider provider;
  EXPECT_CALL(provider, AddObserver(controller()));
  controller()->RegisterPlusProfilesProvider(provider.GetWeakPtr());

  // Provide 1 plus address, which is used as a username in the saved
  // credential. It should not appear as a standalone suggestion in the password
  // sheet.
  std::vector<PlusProfile> profiles{plus_addresses::test::CreatePlusProfile(
      /*plus_address=*/"example@gmail", /*is_confirmed=*/true)};
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  EXPECT_CALL(provider, GetAffiliatedPlusProfiles)
      .WillRepeatedly(Return(base::make_span(profiles)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableNonSearchField);

  EXPECT_EQ(
      controller()->GetSheetData(),
      PasswordAccessorySheetDataBuilder(passwords_title(kExampleDomain),
                                        plus_address_title(kExampleDomain))
          .AddUserInfo(kExampleSite)
          .AddPlusAddressInfo("https://foo.com", u"example@gmail")
          .AppendField(u"foo.bar@gmail", u"foo.bar@gmail",
                       /*is_obfuscated=*/false, /*selectable=*/true)
          .AppendField(u"S3cur3", password_for_str(u"foo.bar@gmail"), true,
                       false)
          .AppendFooterCommand(
              l10n_util::GetStringUTF16(
                  IDS_PLUS_ADDRESS_MANAGE_PLUS_ADDRESSES_LINK_ANDROID),
              AccessoryAction::MANAGE_PLUS_ADDRESS_FROM_PASSWORD_SHEET)
          .Build());
}

// Verify that the action to open plus address creation bottom sheet is appended
// when the corresponding feature flag is enabled.
TEST_F(PasswordAccessoryControllerTest,
       PlusAddressFillingDisabled_NoPlusAddressesActions) {
  CreateSheetController();

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  EXPECT_EQ(
      controller()->GetSheetData(),
      PasswordAccessorySheetDataBuilder(passwords_empty_str(kExampleDomain))
          .Build());
}

TEST_F(PasswordAccessoryControllerTest,
       NoPlusAddressesSaved_NoSelectPlusAddressAction) {
  CreateSheetController();

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  // Although the plus address filling is enabled, the user doesn't have any
  // saved plus addresses. The "Select plus address" should not be displayed.
  plus_address_service().set_is_plus_address_filling_enabled(true);

  EXPECT_EQ(
      controller()->GetSheetData(),
      PasswordAccessorySheetDataBuilder(passwords_empty_str(kExampleDomain))
          .Build());
}

TEST_F(PasswordAccessoryControllerTest,
       PlusAddressFillingEnabled_AppendsSelectPlusAddressAction) {
  CreateSheetController();

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  plus_address_service().add_plus_profile(
      plus_addresses::test::CreatePlusProfile());
  plus_address_service().set_is_plus_address_filling_enabled(true);

  EXPECT_EQ(
      controller()->GetSheetData(),
      PasswordAccessorySheetDataBuilder(passwords_empty_str(kExampleDomain))
          .AppendFooterCommand(select_plus_address_str(),
                               autofill::AccessoryAction::
                                   SELECT_PLUS_ADDRESS_FROM_PASSWORD_SHEET)
          .Build());
}

TEST_F(PasswordAccessoryControllerTest,
       NoAffiliatedPlusAddresses_AppendsCreatePlusAddressAction) {
  CreateSheetController();

  MockAffiliatedPlusProfilesProvider provider;
  EXPECT_CALL(provider, AddObserver(controller()));
  controller()->RegisterPlusProfilesProvider(provider.GetWeakPtr());

  EXPECT_CALL(provider, GetAffiliatedPlusProfiles)
      .WillRepeatedly(Return(base::span<const PlusProfile, 0>()));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  // Plus address creation can't be supported while plus address filling is
  // disabled.
  plus_address_service().set_is_plus_address_filling_enabled(true);
  plus_address_service().set_should_offer_plus_address_creation(true);

  EXPECT_EQ(
      controller()->GetSheetData(),
      PasswordAccessorySheetDataBuilder(passwords_empty_str(kExampleDomain))
          .AppendFooterCommand(generate_plus_address_str(),
                               autofill::AccessoryAction::
                                   CREATE_PLUS_ADDRESS_FROM_PASSWORD_SHEET)
          .Build());
}

TEST_F(PasswordAccessoryControllerTest,
       HasAffiliatedPlusAddresses_NoCreatePlusAddressAction) {
  CreateSheetController();

  MockAffiliatedPlusProfilesProvider provider;
  EXPECT_CALL(provider, AddObserver);
  controller()->RegisterPlusProfilesProvider(provider.GetWeakPtr());

  std::vector<PlusProfile> profiles{plus_addresses::test::CreatePlusProfile()};
  EXPECT_CALL(provider, GetAffiliatedPlusProfiles)
      .WillRepeatedly(Return(base::span(profiles)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  // Plus address creation can't be supported while plus address filling is
  // disabled.
  plus_address_service().set_is_plus_address_filling_enabled(true);
  plus_address_service().set_should_offer_plus_address_creation(true);

  // Although the plus address creation is supported, the user has an affiliated
  // plus address for the current domain. The "Create plus address" action
  // should not be displayed.
  EXPECT_EQ(
      controller()->GetSheetData(),
      PasswordAccessorySheetDataBuilder(passwords_empty_str(kExampleDomain),
                                        plus_address_title(kExampleDomain))
          .AddPlusAddressInfo("https://foo.com", u"plus+foo@plus.plus")
          .AppendFooterCommand(
              l10n_util::GetStringUTF16(
                  IDS_PLUS_ADDRESS_MANAGE_PLUS_ADDRESSES_LINK_ANDROID),
              AccessoryAction::MANAGE_PLUS_ADDRESS_FROM_PASSWORD_SHEET)
          .Build());
}

TEST_F(PasswordAccessoryControllerTest,
       RecordsAccessoryImpressionsForBlocklisted) {
  CreateSheetController();

  base::HistogramTester histogram_tester;

  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(true),
      url::Origin::Create(GURL(kExampleSite)));
  ON_CALL(*password_client(), IsSavingAndFillingEnabled(GURL(kExampleSite)))
      .WillByDefault(Return(true));

  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField);

  histogram_tester.ExpectUniqueSample(
      "KeyboardAccessory.DisabledSavingAccessoryImpressions", true, 1);
}

TEST_F(PasswordAccessoryControllerTest, NoAccessoryImpressionsIfUnblocklisted) {
  CreateSheetController();
  base::HistogramTester histogram_tester;

  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(true),
      url::Origin::Create(GURL(kExampleSite)));
  // Simulate unblocklisting.
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  ON_CALL(*password_client(), IsSavingAndFillingEnabled(GURL(kExampleSite)))
      .WillByDefault(Return(true));
  EXPECT_CALL(filling_source_observer_,
              Run(controller(), IsFillingSourceAvailable(true)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField);

  histogram_tester.ExpectTotalCount(
      "KeyboardAccessory.DisabledSavingAccessoryImpressions", 0);
}

TEST_F(PasswordAccessoryControllerTest, SavePasswordsToggledUpdatesCache) {
  CreateSheetController();
  url::Origin example_origin = url::Origin::Create(GURL(kExampleSite));
  EXPECT_CALL(*password_client(), UpdateFormManagers);
  controller()->OnToggleChanged(
      autofill::AccessoryAction::TOGGLE_SAVE_PASSWORDS, true);
}

TEST_F(PasswordAccessoryControllerTest,
       SavePasswordsEnabledUpdatesAccountStore) {
  ON_CALL(*password_client()->GetPasswordFeatureManager(),
          IsOptedInForAccountStorage)
      .WillByDefault(Return(true));
  ON_CALL(*password_client()->GetPasswordFeatureManager(),
          GetDefaultPasswordStore)
      .WillByDefault(Return(PasswordForm::Store::kAccountStore));
  CreateSheetController();
  password_manager::PasswordFormDigest form_digest(
      PasswordForm::Scheme::kHtml, kExampleSignonRealm, GURL(kExampleSite));
  EXPECT_CALL(*mock_account_password_store_, Unblocklist(form_digest, _));
  controller()->OnToggleChanged(
      autofill::AccessoryAction::TOGGLE_SAVE_PASSWORDS, true);
}

TEST_F(PasswordAccessoryControllerTest,
       SavePasswordsEnabledUpdatesProfileStore) {
  CreateSheetController();
  password_manager::PasswordFormDigest form_digest(
      PasswordForm::Scheme::kHtml, kExampleSignonRealm, GURL(kExampleSite));
  EXPECT_CALL(*mock_profile_password_store_, Unblocklist(form_digest, _));
  controller()->OnToggleChanged(
      autofill::AccessoryAction::TOGGLE_SAVE_PASSWORDS, true);
}

TEST_F(PasswordAccessoryControllerTest,
       SavePasswordsDisabledUpdatesAccountStore) {
  ON_CALL(*password_client()->GetPasswordFeatureManager(),
          IsOptedInForAccountStorage)
      .WillByDefault(Return(true));
  ON_CALL(*password_client()->GetPasswordFeatureManager(),
          GetDefaultPasswordStore)
      .WillByDefault(Return(PasswordForm::Store::kAccountStore));
  CreateSheetController();
  PasswordForm expected_form;
  expected_form.blocked_by_user = true;
  expected_form.scheme = PasswordForm::Scheme::kHtml;
  expected_form.signon_realm = kExampleSignonRealm;
  expected_form.url = GURL(kExampleSite);
  expected_form.date_created = base::Time::Now();
  EXPECT_CALL(*mock_account_password_store_, AddLogin(Eq(expected_form), _));
  controller()->OnToggleChanged(
      autofill::AccessoryAction::TOGGLE_SAVE_PASSWORDS, false);
}

TEST_F(PasswordAccessoryControllerTest,
       SavePasswordsDisabledUpdatesProfileStore) {
  CreateSheetController();
  PasswordForm expected_form;
  expected_form.blocked_by_user = true;
  expected_form.scheme = PasswordForm::Scheme::kHtml;
  expected_form.signon_realm = kExampleSignonRealm;
  expected_form.url = GURL(kExampleSite);
  expected_form.date_created = base::Time::Now();
  EXPECT_CALL(*mock_profile_password_store_, AddLogin(Eq(expected_form), _));
  controller()->OnToggleChanged(
      autofill::AccessoryAction::TOGGLE_SAVE_PASSWORDS, false);
}

TEST_F(PasswordAccessoryControllerTest, FillsUsername) {
  CreateSheetController();
  std::vector<PasswordForm> matches = {CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  AccessorySheetField selected_field = AccessorySheetField::Builder()
                                           .SetDisplayText(u"Ben")
                                           .SetSelectable(true)
                                           .Build();
  EXPECT_CALL(*driver(),
              FillIntoFocusedField(selected_field.is_obfuscated(),
                                   Eq(selected_field.display_text())));
  controller()->OnFillingTriggered(autofill::FieldGlobalId(), selected_field);
}

TEST_F(PasswordAccessoryControllerTest, FillsPasswordIfNoAuthAvailable) {
  // Auth is required to fill passwords in Android automotive.
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP();
  }

  CreateSheetController();
  std::vector<PasswordForm> matches = {CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  AccessorySheetField selected_field = AccessorySheetField::Builder()
                                           .SetDisplayText(u"S3cur3")
                                           .SetIsObfuscated(true)
                                           .SetSelectable(true)
                                           .Build();

  auto mock_authenticator = std::make_unique<MockDeviceAuthenticator>();

  EXPECT_CALL(*password_client(), IsReauthBeforeFillingRequired)
      .WillOnce(Return(false));
  EXPECT_CALL(*password_client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(mock_authenticator))));
  EXPECT_CALL(*driver(),
              FillIntoFocusedField(selected_field.is_obfuscated(),
                                   Eq(selected_field.display_text())));
  controller()->OnFillingTriggered(autofill::FieldGlobalId(), selected_field);
}

TEST_F(PasswordAccessoryControllerTest, FillsPasswordIfAuthSuccessful) {
  features_.Reset();
  features_.InitWithFeatures(
      {plus_addresses::features::kPlusAddressesEnabled,
       plus_addresses::features::kPlusAddressAndroidManualFallbackEnabled,
       password_manager::features::kBiometricTouchToFill},
      {});
  CreateSheetController();

  std::vector<PasswordForm> matches = {CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  AccessorySheetField selected_field = AccessorySheetField::Builder()
                                           .SetDisplayText(u"S3cur3")
                                           .SetIsObfuscated(true)
                                           .SetSelectable(true)
                                           .Build();

  auto mock_authenticator = std::make_unique<MockDeviceAuthenticator>();

  ON_CALL(*password_client(), IsReauthBeforeFillingRequired)
      .WillByDefault(Return(true));
  EXPECT_CALL(*mock_authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/true));

  EXPECT_CALL(*password_client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(mock_authenticator))))
      .RetiresOnSaturation();

  EXPECT_CALL(*driver(),
              FillIntoFocusedField(selected_field.is_obfuscated(),
                                   Eq(selected_field.display_text())));
  controller()->OnFillingTriggered(autofill::FieldGlobalId(), selected_field);
}

TEST_F(PasswordAccessoryControllerTest, DoesntFillPasswordIfAuthFails) {
  features_.Reset();
  features_.InitWithFeatures(
      {plus_addresses::features::kPlusAddressesEnabled,
       plus_addresses::features::kPlusAddressAndroidManualFallbackEnabled,
       password_manager::features::kBiometricTouchToFill},
      {});
  CreateSheetController();

  std::vector<PasswordForm> matches = {CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  AccessorySheetField selected_field = AccessorySheetField::Builder()
                                           .SetDisplayText(u"S3cur3")
                                           .SetIsObfuscated(true)
                                           .SetSelectable(true)
                                           .Build();

  auto mock_authenticator = std::make_unique<MockDeviceAuthenticator>();

  ON_CALL(*password_client(), IsReauthBeforeFillingRequired)
      .WillByDefault(Return(true));
  EXPECT_CALL(*mock_authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(/*auth_succeeded=*/false));

  EXPECT_CALL(*password_client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(mock_authenticator))))
      .RetiresOnSaturation();

  EXPECT_CALL(*driver(),
              FillIntoFocusedField(selected_field.is_obfuscated(),
                                   Eq(selected_field.display_text())))
      .Times(0);
  controller()->OnFillingTriggered(autofill::FieldGlobalId(), selected_field);
}

TEST_F(PasswordAccessoryControllerTest, CancelsOngoingAuthIfDestroyed) {
  features_.Reset();
  features_.InitWithFeatures(
      {plus_addresses::features::kPlusAddressesEnabled,
       plus_addresses::features::kPlusAddressAndroidManualFallbackEnabled,
       password_manager::features::kBiometricTouchToFill},
      {});
  CreateSheetController();

  std::vector<PasswordForm> matches = {CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  AccessorySheetField selected_field = AccessorySheetField::Builder()
                                           .SetDisplayText(u"S3cur3")
                                           .SetIsObfuscated(true)
                                           .SetSelectable(true)
                                           .Build();

  auto mock_authenticator = std::make_unique<MockDeviceAuthenticator>();
  auto* mock_authenticator_ptr = mock_authenticator.get();

  ON_CALL(*password_client(), IsReauthBeforeFillingRequired)
      .WillByDefault(Return(true));
  EXPECT_CALL(*mock_authenticator_ptr, AuthenticateWithMessage);

  EXPECT_CALL(*password_client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(mock_authenticator))))
      .RetiresOnSaturation();

  EXPECT_CALL(*driver(),
              FillIntoFocusedField(selected_field.is_obfuscated(),
                                   Eq(selected_field.display_text())))
      .Times(0);
  controller()->OnFillingTriggered(autofill::FieldGlobalId(), selected_field);

  EXPECT_CALL(*mock_authenticator_ptr, Cancel());
}

TEST_F(PasswordAccessoryControllerTest, ShowCredManReentry) {
  WebAuthnCredManDelegate::override_cred_man_support_for_testing(
      CredManSupport::FULL_UNLESS_INAPPLICABLE);
  CreateSheetController();
  cred_man_delegate()->OnCredManConditionalRequestPending(
      /*has_results=*/true, base::RepeatingCallback<void(bool)>());

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  ShouldShowAction(true),
                  autofill::AccessoryAction::CREDMAN_CONDITIONAL_UI_REENTRY));

  controller()->UpdateCredManReentryUi(
      autofill::mojom::FocusedFieldType::kFillableUsernameField);
}

TEST_F(PasswordAccessoryControllerTest, HideCredManReentryWithoutResult) {
  WebAuthnCredManDelegate::override_cred_man_support_for_testing(
      CredManSupport::FULL_UNLESS_INAPPLICABLE);
  CreateSheetController();
  cred_man_delegate()->OnCredManConditionalRequestPending(
      /*has_results=*/false, base::RepeatingCallback<void(bool)>());

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  ShouldShowAction(false),
                  autofill::AccessoryAction::CREDMAN_CONDITIONAL_UI_REENTRY));

  controller()->UpdateCredManReentryUi(
      autofill::mojom::FocusedFieldType::kFillableUsernameField);
}

TEST_F(PasswordAccessoryControllerTest, HideCredManReentryOnNonSignInField) {
  WebAuthnCredManDelegate::override_cred_man_support_for_testing(
      CredManSupport::FULL_UNLESS_INAPPLICABLE);
  CreateSheetController();
  cred_man_delegate()->OnCredManConditionalRequestPending(
      /*has_results=*/true, base::RepeatingCallback<void(bool)>());

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged(
                  ShouldShowAction(false),
                  autofill::AccessoryAction::CREDMAN_CONDITIONAL_UI_REENTRY));

  controller()->UpdateCredManReentryUi(
      autofill::mojom::FocusedFieldType::kFillableNonSearchField);
}

TEST_F(PasswordAccessoryControllerTest, SuppressCredManReentryWithoutFeature) {
  WebAuthnCredManDelegate::override_cred_man_support_for_testing(
      CredManSupport::DISABLED);
  CreateSheetController();

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAccessoryActionAvailabilityChanged)
      .Times(0);

  controller()->UpdateCredManReentryUi(
      autofill::mojom::FocusedFieldType::kFillableUsernameField);
}

TEST_F(PasswordAccessoryControllerTest, OnCredManConditionalUiRequested) {
  WebAuthnCredManDelegate::override_cred_man_support_for_testing(
      CredManSupport::FULL_UNLESS_INAPPLICABLE);
  CreateSheetController();
  base::MockCallback<base::RepeatingCallback<void(bool)>> cred_man_callback;
  cred_man_delegate()->OnCredManConditionalRequestPending(
      /*has_results=*/true, cred_man_callback.Get());

  EXPECT_CALL(cred_man_callback, Run);

  controller()->OnOptionSelected(
      autofill::AccessoryAction::CREDMAN_CONDITIONAL_UI_REENTRY);
}

TEST_F(PasswordAccessoryControllerTest, ShowAndSelectCredManReentryOption) {
  base::MockCallback<base::RepeatingCallback<void(bool)>> cred_man_callback;
  WebAuthnCredManDelegate::override_cred_man_support_for_testing(
      CredManSupport::FULL_UNLESS_INAPPLICABLE);
  CreateSheetController();
  cred_man_delegate()->OnCredManConditionalRequestPending(
      /*has_results=*/true, cred_man_callback.Get());
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);
  EXPECT_EQ(
      controller()->GetSheetData(),
      AccessorySheetData::Builder(AccessoryTabType::PASSWORDS,
                                  passwords_empty_str(kExampleDomain),
                                  /*plus_address_title=*/std::u16string())
          .AppendFooterCommand(
              select_passkey_str(),
              autofill::AccessoryAction::CREDMAN_CONDITIONAL_UI_REENTRY)
          .AppendFooterCommand(manage_passwords_and_passkeys_str(),
                               autofill::AccessoryAction::MANAGE_PASSWORDS)
          .Build());

  EXPECT_CALL(cred_man_callback, Run);
  controller()->OnOptionSelected(
      autofill::AccessoryAction::CREDMAN_CONDITIONAL_UI_REENTRY);
}

// Verify that when
// WebAuthnCredentialsDelegate::IsSecurityKeyOrHybridFlowAvailable returns true,
// the hybrid passkey option shows on the sheet, and selecting it triggers
// hybrid passkey sign-in invocation.
TEST_F(PasswordAccessoryControllerTest, ShowAndSelectHybridPasskeyOption) {
  ON_CALL(*webauthn_credentials_delegate(), IsSecurityKeyOrHybridFlowAvailable)
      .WillByDefault(Return(true));
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);
  EXPECT_EQ(
      controller()->GetSheetData(),
      AccessorySheetData::Builder(AccessoryTabType::PASSWORDS,
                                  passwords_empty_str(kExampleDomain),
                                  /*plus_address_title=*/std::u16string())
          .AppendFooterCommand(cross_device_passkeys_str(),
                               autofill::AccessoryAction::CROSS_DEVICE_PASSKEY)
          .AppendFooterCommand(manage_passwords_str(),
                               autofill::AccessoryAction::MANAGE_PASSWORDS)
          .Build());

  EXPECT_CALL(*webauthn_credentials_delegate(), LaunchSecurityKeyOrHybridFlow);

  controller()->OnOptionSelected(
      autofill::AccessoryAction::CROSS_DEVICE_PASSKEY);
}

// Verify that the plus address creation bottom sheet is opened when the
// corresponding action is triggered.
TEST_F(PasswordAccessoryControllerTest,
       TriggersPlusAddressCreationBottomSheet) {
  CreateSheetController();
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  const std::string plus_address = "example@gmail.com";
  EXPECT_CALL(autofill_client(), OfferPlusAddressCreation)
      .WillOnce([&plus_address](const url::Origin&,
                                autofill::PlusAddressCallback callback) {
        std::move(callback).Run(plus_address);
      });
  EXPECT_CALL(*driver(), FillIntoFocusedField(/*is_password=*/false,
                                              base::UTF8ToUTF16(plus_address)));
  // Manual filling sheet is expected to be hidden when the plus address
  // creation bottom sheet is opened.
  EXPECT_CALL(mock_manual_filling_controller_, Hide());

  controller()->OnOptionSelected(
      autofill::AccessoryAction::CREATE_PLUS_ADDRESS_FROM_PASSWORD_SHEET);
}

// Verify that when WebAuthnCredentialsDelegate::SelectPasskey can be invoked
// with a passkey shown in the fallback sheet.
TEST_F(PasswordAccessoryControllerTest, ShowAndSelectPasskey) {
  const PasskeyCredential kTestPasskey(
      PasskeyCredential::Source::kAndroidPhone,
      PasskeyCredential::RpId("rpid.com"),
      PasskeyCredential::CredentialId({21, 22, 23, 24}),
      PasskeyCredential::UserId({81, 28, 83, 84}),
      PasskeyCredential::Username("someone@example.com"),
      PasskeyCredential::DisplayName("someone"));
  const std::optional<std::vector<PasskeyCredential>> kTestPasskeys(
      {kTestPasskey});
  ON_CALL(*webauthn_credentials_delegate(), GetPasskeys)
      .WillByDefault(ReturnRef(kTestPasskeys));
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  EXPECT_EQ(
      controller()->GetSheetData(),
      AccessorySheetData::Builder(AccessoryTabType::PASSWORDS,
                                  /*user_info_title=*/std::u16string(),
                                  /*plus_address_title=*/std::u16string())
          .AddPasskeySection(kTestPasskey.display_name(),
                             kTestPasskey.credential_id())
          .AppendFooterCommand(manage_passwords_and_passkeys_str(),
                               autofill::AccessoryAction::MANAGE_PASSWORDS)
          .Build());

  EXPECT_CALL(
      *webauthn_credentials_delegate(),
      SelectPasskey(Eq(base::Base64Encode(kTestPasskey.credential_id())), _));
  controller()->OnPasskeySelected(kTestPasskey.credential_id());
}

// Verify that when
// WebAuthnCredentialsDelegate::IsSecurityKeyOrHybridFlowAvailable returns
// false, the hybrid passkey option is not shown on the sheet.
TEST_F(PasswordAccessoryControllerTest,
       HybridPasskeyOptionNotShownWhenUnavailable) {
  CreateSheetController();
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      {}, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  EXPECT_EQ(
      controller()->GetSheetData(),
      AccessorySheetData::Builder(AccessoryTabType::PASSWORDS,
                                  passwords_empty_str(kExampleDomain),
                                  /*plus_address_title=*/std::u16string())
          .AppendFooterCommand(manage_passwords_str(),
                               autofill::AccessoryAction::MANAGE_PASSWORDS)
          .Build());
}

TEST_F(PasswordAccessoryControllerTest,
       ShowMigrationSheetOnFillingCredentialIfEnabled) {
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    auto mock_authenticator = std::make_unique<MockDeviceAuthenticator>();
    ON_CALL(*mock_authenticator, AuthenticateWithMessage)
        .WillByDefault(
            base::test::RunOnceCallbackRepeatedly<1>(/*auth_succeeded=*/true));
    EXPECT_CALL(*password_client(), GetDeviceAuthenticator)
        .WillOnce(Return(testing::ByMove(std::move(mock_authenticator))))
        .RetiresOnSaturation();
  }

  features_.Reset();
  features_.InitWithFeatures(
      {plus_addresses::features::kPlusAddressesEnabled,
       plus_addresses::features::kPlusAddressAndroidManualFallbackEnabled,
       password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsMigrationWarning},
      {});
  CreateSheetController();

  // Set up credentials for filling.
  std::vector<PasswordForm> matches = {CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);
  AccessorySheetField selected_field = AccessorySheetField::Builder()
                                           .SetDisplayText(u"S3cur3")
                                           .SetIsObfuscated(true)
                                           .SetSelectable(true)
                                           .Build();
  EXPECT_CALL(
      show_migration_warning_callback_,
      Run(_, _,
          password_manager::metrics_util::PasswordMigrationWarningTriggers::
              kKeyboardAcessorySheet));
  controller()->OnFillingTriggered(autofill::FieldGlobalId(), selected_field);
}

TEST_F(PasswordAccessoryControllerTest, DontShowMigrationSheetlIfDisabled) {
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    auto mock_authenticator = std::make_unique<MockDeviceAuthenticator>();
    ON_CALL(*mock_authenticator, AuthenticateWithMessage)
        .WillByDefault(
            base::test::RunOnceCallbackRepeatedly<1>(/*auth_succeeded=*/true));
    EXPECT_CALL(*password_client(), GetDeviceAuthenticator)
        .WillOnce(Return(testing::ByMove(std::move(mock_authenticator))))
        .RetiresOnSaturation();
  }

  features_.Reset();
  features_.InitWithFeatures(
      {plus_addresses::features::kPlusAddressesEnabled,
       plus_addresses::features::kPlusAddressAndroidManualFallbackEnabled},
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsMigrationWarning});
  // Set up credentials for filling.
  CreateSheetController();
  std::vector<PasswordForm> matches = {CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  AccessorySheetField selected_field = AccessorySheetField::Builder()
                                           .SetDisplayText(u"S3cur3")
                                           .SetIsObfuscated(true)
                                           .SetSelectable(true)
                                           .Build();
  EXPECT_CALL(show_migration_warning_callback_, Run).Times(0);
  controller()->OnFillingTriggered(autofill::FieldGlobalId(), selected_field);
}

TEST_F(PasswordAccessoryControllerTest,
       ShowAccessLossWarningSheetOnFillingCredentialIfEnabled) {
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    auto mock_authenticator = std::make_unique<MockDeviceAuthenticator>();
    ON_CALL(*mock_authenticator, AuthenticateWithMessage)
        .WillByDefault(
            base::test::RunOnceCallbackRepeatedly<1>(/*auth_succeeded=*/true));
    EXPECT_CALL(*password_client(), GetDeviceAuthenticator)
        .WillOnce(Return(testing::ByMove(std::move(mock_authenticator))))
        .RetiresOnSaturation();
  }

  features_.Reset();
  features_.InitWithFeatures(
      {plus_addresses::features::kPlusAddressesEnabled,
       plus_addresses::features::kPlusAddressAndroidManualFallbackEnabled,
       password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning},
      {});
  CreateSheetController();

  // Set up credentials for filling.
  std::vector<PasswordForm> matches = {CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);
  AccessorySheetField selected_field = AccessorySheetField::Builder()
                                           .SetDisplayText(u"S3cur3")
                                           .SetIsObfuscated(true)
                                           .SetSelectable(true)
                                           .Build();
  EXPECT_CALL(*mock_access_loss_warning_bridge_,
              ShouldShowAccessLossNoticeSheet(profile()->GetPrefs(),
                                              /*called_at_startup=*/false))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(
      *mock_access_loss_warning_bridge_,
      MaybeShowAccessLossNoticeSheet(
          profile()->GetPrefs(), _, profile(),
          /*called_at_startup=*/false,
          password_manager_android_util::PasswordAccessLossWarningTriggers::
              kKeyboardAcessorySheet));
  controller()->OnFillingTriggered(autofill::FieldGlobalId(), selected_field);
}

TEST_F(PasswordAccessoryControllerTest,
       DontShowAccessLossWarningSheetlIfDisabled) {
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    auto mock_authenticator = std::make_unique<MockDeviceAuthenticator>();
    ON_CALL(*mock_authenticator, AuthenticateWithMessage)
        .WillByDefault(
            base::test::RunOnceCallbackRepeatedly<1>(/*auth_succeeded=*/true));
    EXPECT_CALL(*password_client(), GetDeviceAuthenticator)
        .WillOnce(Return(testing::ByMove(std::move(mock_authenticator))))
        .RetiresOnSaturation();
  }

  features_.Reset();
  features_.InitWithFeatures(
      {plus_addresses::features::kPlusAddressesEnabled,
       plus_addresses::features::kPlusAddressAndroidManualFallbackEnabled},
      {password_manager::features::
           kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning});
  // Set up credentials for filling.
  CreateSheetController();
  std::vector<PasswordForm> matches = {CreateEntry(
      "Ben", "S3cur3", GURL(kExampleSite), PasswordForm::MatchType::kExact)};
  cache()->SaveCredentialsAndBlocklistedForOrigin(
      matches, CredentialCache::IsOriginBlocklisted(false),
      url::Origin::Create(GURL(kExampleSite)));

  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  AccessorySheetField selected_field = AccessorySheetField::Builder()
                                           .SetDisplayText(u"S3cur3")
                                           .SetIsObfuscated(true)
                                           .SetSelectable(true)
                                           .Build();
  EXPECT_CALL(*mock_access_loss_warning_bridge_,
              ShouldShowAccessLossNoticeSheet(profile()->GetPrefs(),
                                              /*called_at_startup=*/false))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_access_loss_warning_bridge_, MaybeShowAccessLossNoticeSheet)
      .Times(0);
  controller()->OnFillingTriggered(autofill::FieldGlobalId(), selected_field);
}

class PasswordAccessoryControllerWithTestStoreTest
    : public PasswordAccessoryControllerTest,
      public testing::WithParamInterface<bool> {
 public:
  TestPasswordStore& test_account_store() { return *test_account_store_; }
  TestPasswordStore& test_profile_store() { return *test_profile_store_; }

  void SetUp() override {
    PasswordAccessoryControllerTest::SetUp();
    test_account_store_->Init(/*prefs=*/nullptr,
                              /*affiliated_match_helper=*/nullptr);
    test_profile_store_->Init(/*prefs=*/nullptr,
                              /*affiliated_match_helper=*/nullptr);
  }

  void TearDown() override {
    test_account_store_->ShutdownOnUIThread();
    test_profile_store_->ShutdownOnUIThread();
    task_environment()->RunUntilIdle();
    PasswordAccessoryControllerTest::TearDown();
  }

 protected:
  PasswordStoreInterface* CreateInternalAccountPasswordStore() override {
    test_account_store_ = CreateAndUseTestAccountPasswordStore(profile());
    return test_account_store_.get();
  }

  PasswordStoreInterface* CreateInternalProfilePasswordStore() override {
    test_profile_store_ = CreateAndUseTestPasswordStore(profile());
    return test_profile_store_.get();
  }

 private:
  scoped_refptr<TestPasswordStore> test_account_store_;
  scoped_refptr<TestPasswordStore> test_profile_store_;
};

TEST_P(PasswordAccessoryControllerWithTestStoreTest,
       AddsShowOtherPasswordsForPasswordField) {
  if (GetParam()) {
    test_account_store().AddLogin(MakeSavedPassword());
  } else {
    test_profile_store().AddLogin(MakeSavedPassword());
  }
  task_environment()->RunUntilIdle();
  CreateSheetController();

  // Trigger suggestion refresh(es) and store the latest refresh only.
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField);

  task_environment()->RunUntilIdle();  // Wait for store to trigger update.

  EXPECT_EQ(
      controller()->GetSheetData(),
      AccessorySheetData::Builder(AccessoryTabType::PASSWORDS,
                                  passwords_empty_str(kExampleDomain),
                                  /*plus_address_title=*/std::u16string())
          .AppendFooterCommand(show_other_passwords_str(),
                               autofill::AccessoryAction::USE_OTHER_PASSWORD)
          .AppendFooterCommand(manage_passwords_str(),
                               autofill::AccessoryAction::MANAGE_PASSWORDS)
          .Build());
}

TEST_P(PasswordAccessoryControllerWithTestStoreTest,
       AddsShowOtherPasswordsForUsernameField) {
  if (GetParam()) {
    test_account_store().AddLogin(MakeSavedPassword());
  } else {
    test_profile_store().AddLogin(MakeSavedPassword());
  }
  task_environment()->RunUntilIdle();
  CreateSheetController();

  // Trigger suggestion refresh(es) and store the latest refresh only.
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillableUsernameField);

  task_environment()->RunUntilIdle();  // Wait for store to trigger update.

  EXPECT_EQ(
      controller()->GetSheetData(),
      AccessorySheetData::Builder(AccessoryTabType::PASSWORDS,
                                  passwords_empty_str(kExampleDomain),
                                  /*plus_address_title=*/std::u16string())
          .AppendFooterCommand(show_other_passwords_str(),
                               autofill::AccessoryAction::USE_OTHER_PASSWORD)
          .AppendFooterCommand(manage_passwords_str(),
                               autofill::AccessoryAction::MANAGE_PASSWORDS)
          .Build());
}

TEST_P(PasswordAccessoryControllerWithTestStoreTest,
       AddsShowOtherPasswordForOnlyCryptographicSchemeSites) {
  if (GetParam()) {
    test_account_store().AddLogin(MakeSavedPassword());
  } else {
    test_profile_store().AddLogin(MakeSavedPassword());
  }
  task_environment()->RunUntilIdle();
  CreateSheetController();
  // `Setup` method sets the URL to https but http is required for this method.
  NavigateAndCommit(GURL(kExampleHttpSite));
  FocusWebContentsOnMainFrame();

  // Trigger suggestion refresh(es).
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField);
  task_environment()->RunUntilIdle();  // Wait for store to trigger update.
  EXPECT_EQ(
      controller()->GetSheetData(),
      AccessorySheetData::Builder(AccessoryTabType::PASSWORDS,
                                  passwords_empty_str(kExampleHttpSite16),
                                  /*plus_address_title=*/std::u16string())
          .AppendFooterCommand(manage_passwords_str(),
                               autofill::AccessoryAction::MANAGE_PASSWORDS)
          .Build());
}

TEST_P(PasswordAccessoryControllerWithTestStoreTest,
       HideShowOtherPasswordForLowSecurityLevelSites) {
  if (GetParam()) {
    test_account_store().AddLogin(MakeSavedPassword());
  } else {
    test_profile_store().AddLogin(MakeSavedPassword());
  }
  task_environment()->RunUntilIdle();
  CreateSheetController(security_state::WARNING);

  // Trigger suggestion refresh(es) and store the latest refresh only.
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField);

  task_environment()->RunUntilIdle();  // Wait for store to trigger update.
  EXPECT_EQ(
      controller()->GetSheetData(),
      AccessorySheetData::Builder(AccessoryTabType::PASSWORDS,
                                  passwords_empty_str(kExampleDomain),
                                  /*plus_address_title=*/std::u16string())
          .AppendFooterCommand(manage_passwords_str(),
                               autofill::AccessoryAction::MANAGE_PASSWORDS)
          .Build());
}

TEST_P(PasswordAccessoryControllerWithTestStoreTest,
       HidesUseOtherPasswordsIfPasswordStoreIsEmpty) {
  CreateSheetController();

  // Trigger suggestion refresh(es).
  controller()->RefreshSuggestionsForField(
      FocusedFieldType::kFillablePasswordField);

  task_environment()->RunUntilIdle();  // Wait for store to trigger update.
  EXPECT_EQ(
      controller()->GetSheetData(),
      AccessorySheetData::Builder(AccessoryTabType::PASSWORDS,
                                  passwords_empty_str(kExampleDomain),
                                  /*plus_address_title=*/std::u16string())
          .AppendFooterCommand(manage_passwords_str(),
                               autofill::AccessoryAction::MANAGE_PASSWORDS)
          .Build());
}

INSTANTIATE_TEST_SUITE_P(,
                         PasswordAccessoryControllerWithTestStoreTest,
                         ::testing::Bool());

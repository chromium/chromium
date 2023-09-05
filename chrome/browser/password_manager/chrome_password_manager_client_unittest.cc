// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_manager_client.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/mock_address_accessory_controller.h"
#include "chrome/browser/autofill/mock_manual_filling_view.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/password_manager/password_manager_settings_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/logging/log_receiver.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/password_manager_log_router_factory.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/mock_password_manager_settings_service.h"
#include "components/password_manager/core/browser/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/common/password_manager_feature_variations_android.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/sessions/content/content_record_password_state.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "components/safe_browsing/content/browser/password_protection/mock_password_protection_service.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "chrome/browser/autofill/manual_filling_controller_impl.h"
#include "chrome/browser/autofill/mock_address_accessory_controller.h"
#include "chrome/browser/autofill/mock_credit_card_accessory_controller.h"
#include "chrome/browser/autofill/mock_manual_filling_controller.h"
#include "chrome/browser/autofill/mock_manual_filling_view.h"
#include "chrome/browser/autofill/mock_password_accessory_controller.h"
#include "chrome/browser/password_manager/android/password_accessory_controller_impl.h"
#include "chrome/browser/password_manager/android/password_generation_controller.h"
#endif  // BUILDFLAG(IS_ANDROID)

using autofill::CalculateFormSignature;
using autofill::ContentAutofillClient;
using autofill::ContentAutofillDriver;
using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::mojom::FocusedFieldType;
using autofill::test::CreateTestFormField;
using content::BrowserContext;
using content::WebContents;

using password_manager::PasswordForm;
using password_manager::PasswordManagerClient;
using password_manager::PasswordManagerSetting;
using password_manager::PasswordStoreConsumer;
using sessions::GetPasswordStateFromNavigation;
using sessions::SerializedNavigationEntry;
using testing::_;
using testing::Key;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using testing::UnorderedElementsAre;

#if BUILDFLAG(IS_ANDROID)
using base::android::BuildInfo;
using password_manager::CredentialCache;
using password_manager::MockPasswordStoreInterface;
#endif

namespace {

#if BUILDFLAG(IS_ANDROID)
FormData MakePasswordFormData() {
  FormData form_data;
  form_data.url = GURL("https://www.example.com/");
  form_data.action = GURL("https://www.example.com/");
  form_data.name = u"form-name";

  FormFieldData field;
  field.name = u"password-element";
  field.id_attribute = field.name;
  field.name_attribute = field.name;
  field.form_control_type = "password";
  field.unique_renderer_id = FieldRendererId(123);
  form_data.fields.push_back(field);

  return form_data;
}

std::unique_ptr<PasswordForm> MakePasswordForm() {
  std::unique_ptr<PasswordForm> form = std::make_unique<PasswordForm>();
  form->url = GURL("https://www.example.com/");
  form->action = GURL("https://www.example.com/");
  form->password_element = u"password-element";
  form->submit_element = u"signIn";
  form->signon_realm = "https://www.example.com/";
  form->in_store = PasswordForm::Store::kProfileStore;
  form->match_type = PasswordForm::MatchType::kExact;
  return form;
}
#endif

// Creates a FormData with `fields` whose `url` and `host_frame` match `rfh`.
FormData CreateFormForRenderHost(content::RenderFrameHost& rfh,
                                 std::vector<FormFieldData> fields) {
  FormData form;
  form.url = rfh.GetLastCommittedURL();
  form.action = form.url;
  form.host_frame = autofill::LocalFrameToken(rfh.GetFrameToken().value());
  form.unique_renderer_id = autofill::test::MakeFormRendererId();
  form.fields = std::move(fields);
  for (FormFieldData& field : form.fields) {
    field.host_frame = form.host_frame;
  }
  return form;
}

// TODO(crbug.com/474577): Get rid of the mocked client in the client's own
// test.
class MockChromePasswordManagerClient : public ChromePasswordManagerClient {
 public:
  static MockChromePasswordManagerClient* CreateForWebContentsAndGet(
      content::WebContents* contents) {
    auto* client = new MockChromePasswordManagerClient(contents);
    contents->SetUserData(UserDataKey(), base::WrapUnique(client));
    return client;
  }

  MOCK_METHOD(net::CertStatus, GetMainFrameCertStatus, (), (const override));

  MockChromePasswordManagerClient(const MockChromePasswordManagerClient&) =
      delete;
  MockChromePasswordManagerClient& operator=(
      const MockChromePasswordManagerClient&) = delete;

#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::PasswordProtectionService* GetPasswordProtectionService()
      const override {
    return password_protection_service_.get();
  }

  safe_browsing::MockPasswordProtectionService* password_protection_service() {
    return password_protection_service_.get();
  }
#endif

 private:
  explicit MockChromePasswordManagerClient(content::WebContents* web_contents)
      : ChromePasswordManagerClient(web_contents) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
    password_protection_service_ =
        std::make_unique<safe_browsing::MockPasswordProtectionService>();
#endif
  }

#if BUILDFLAG(FULL_SAFE_BROWSING)
  std::unique_ptr<safe_browsing::MockPasswordProtectionService>
      password_protection_service_;
#endif
};

class DummyLogReceiver : public autofill::LogReceiver {
 public:
  DummyLogReceiver() = default;

  DummyLogReceiver(const DummyLogReceiver&) = delete;
  DummyLogReceiver& operator=(const DummyLogReceiver&) = delete;

  void LogEntry(const base::Value::Dict& entry) override {}
};

class FakePasswordAutofillAgent
    : public autofill::mojom::PasswordAutofillAgent {
 public:
  void BindReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<autofill::mojom::PasswordAutofillAgent>(
            std::move(handle)));
  }

  bool called_set_logging_state() { return called_set_logging_state_; }

  bool logging_state_active() { return logging_state_active_; }

  void reset_data() {
    called_set_logging_state_ = false;
    logging_state_active_ = false;
  }

 private:
  // autofill::mojom::PasswordAutofillAgent:
  void SetPasswordFillData(
      const autofill::PasswordFormFillData& form_data) override {}

  void FillPasswordSuggestion(const std::u16string& username,
                              const std::u16string& password) override {}

  void InformNoSavedCredentials(
      bool should_show_popup_without_passwords) override {}

  void FillIntoFocusedField(bool is_password,
                            const std::u16string& credential) override {}
  void AnnotateFieldsWithParsingResult(
      const autofill::ParsingResult& parsing_result) override {}

  void SetLoggingState(bool active) override {
    called_set_logging_state_ = true;
    logging_state_active_ = active;
  }

#if BUILDFLAG(IS_ANDROID)
  void KeyboardReplacingSurfaceClosed(bool show_virtual_keyboard) override {}

  void TriggerFormSubmission() override {}
#endif

  // Records whether SetLoggingState() gets called.
  bool called_set_logging_state_ = false;
  // Records data received via SetLoggingState() call.
  bool logging_state_active_ = false;

  mojo::AssociatedReceiver<autofill::mojom::PasswordAutofillAgent> receiver_{
      this};
};

#if BUILDFLAG(IS_ANDROID)

class MockPasswordAccessoryControllerImpl
    : public PasswordAccessoryControllerImpl {
 public:
  MockPasswordAccessoryControllerImpl(
      content::WebContents* web_contents,
      password_manager::CredentialCache* credential_cache,
      base::WeakPtr<ManualFillingController> mf_controller,
      password_manager::PasswordManagerClient* password_client,
      PasswordDriverSupplierForFocusedFrame driver_supplier)
      : PasswordAccessoryControllerImpl(web_contents,
                                        credential_cache,
                                        mf_controller,
                                        password_client,
                                        driver_supplier,
                                        base::DoNothing()) {}

  MOCK_METHOD(void,
              RefreshSuggestionsForField,
              (autofill::mojom::FocusedFieldType, bool),
              (override));
  MOCK_METHOD(void,
              UpdateCredManReentryUi,
              (autofill::mojom::FocusedFieldType),
              (override));
};

#endif

std::unique_ptr<KeyedService> CreateTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

}  // namespace

class ChromePasswordManagerClientTest : public ChromeRenderViewHostTestHarness {
 public:
  ChromePasswordManagerClientTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(safe_browsing::kDelayedWarnings);
  }
  ~ChromePasswordManagerClientTest() override = default;

  void SetUp() override;
  void TearDown() override;

  void SetupBasicTestSync() {
    SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), base::BindRepeating(&CreateTestSyncService));
  }

  void SetupSettingsServiceFactory() {
    PasswordManagerSettingsServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              NiceMock<MockPasswordManagerSettingsService>>();
        }));
  }

  // Make a navigation entry that will accept an annotation.
  void SetupNavigationForAnnotation() {
    sync_service()->SetIsUsingExplicitPassphrase(false);
    metrics_enabled_ = true;
    NavigateAndCommit(GURL("about:blank"));
  }

 protected:
  ChromePasswordManagerClient* GetClient();
  MockPasswordManagerSettingsService& settings_service() {
    return static_cast<MockPasswordManagerSettingsService&>(
        *PasswordManagerSettingsServiceFactory::GetForProfile(profile()));
  }
  syncer::TestSyncService* sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetInstance()->GetForProfile(profile()));
  }

  // If autofill::mojom::PasswordAutofillAgent::SetLoggingState() got called,
  // copies its argument into |activation_flag| and returns true. Otherwise
  // returns false.
  bool WasLoggingActivationMessageSent(bool* activation_flag);

  FakePasswordAutofillAgent fake_agent_;

  bool metrics_enabled_ = false;

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
  base::test::ScopedFeatureList scoped_feature_list_;
};

void ChromePasswordManagerClientTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  PasswordStoreFactory::GetInstance()->SetTestingFactory(
      GetBrowserContext(),
      base::BindRepeating(&password_manager::BuildPasswordStoreInterface<
                          content::BrowserContext,
                          password_manager::MockPasswordStoreInterface>));

  blink::AssociatedInterfaceProvider* remote_interfaces =
      web_contents()->GetPrimaryMainFrame()->GetRemoteAssociatedInterfaces();
  remote_interfaces->OverrideBinderForTesting(
      autofill::mojom::PasswordAutofillAgent::Name_,
      base::BindRepeating(&FakePasswordAutofillAgent::BindReceiver,
                          base::Unretained(&fake_agent_)));

  // In order for the `PasswordFeatureManager` to be initialized correctly
  // the testing sync service must be set up before the client.
  SetupBasicTestSync();
  // `ChromePasswordManagerClient` observes `AutofillManager`s, so
  // `ChromeAutofillClient` needs to be set up, too.
  autofill::ChromeAutofillClient::CreateForWebContents(web_contents());
  ChromePasswordManagerClient::CreateForWebContents(web_contents());

  SetupSettingsServiceFactory();

  // Connect our bool for testing.
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &metrics_enabled_);
}

void ChromePasswordManagerClientTest::TearDown() {
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

ChromePasswordManagerClient* ChromePasswordManagerClientTest::GetClient() {
  return ChromePasswordManagerClient::FromWebContents(web_contents());
}

bool ChromePasswordManagerClientTest::WasLoggingActivationMessageSent(
    bool* activation_flag) {
  base::RunLoop().RunUntilIdle();
  if (!fake_agent_.called_set_logging_state())
    return false;

  if (activation_flag)
    *activation_flag = fake_agent_.logging_state_active();
  fake_agent_.reset_data();
  return true;
}

TEST_F(ChromePasswordManagerClientTest, LogEntryNotifyRenderer) {
  bool logging_active = true;
  // Ensure the existence of a driver, which will send the IPCs we listen for
  // below.
  NavigateAndCommit(GURL("about:blank"));

  // Initially, the logging should be off, so no IPC messages.
  EXPECT_TRUE(!WasLoggingActivationMessageSent(&logging_active) ||
              !logging_active)
      << "logging_active=" << logging_active;

  DummyLogReceiver log_receiver;
  autofill::LogRouter* log_router =
      password_manager::PasswordManagerLogRouterFactory::GetForBrowserContext(
          profile());
  log_router->RegisterReceiver(&log_receiver);
  EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_active));
  EXPECT_TRUE(logging_active);

  log_router->UnregisterReceiver(&log_receiver);
  EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_active));
  EXPECT_FALSE(logging_active);
}

TEST_F(ChromePasswordManagerClientTest, GetPasswordSyncState) {
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPasswords});
  sync_service()->SetIsUsingExplicitPassphrase(false);

  ChromePasswordManagerClient* client = GetClient();

  // Passwords are syncing and custom passphrase isn't used.
  EXPECT_EQ(password_manager::SyncState::kSyncingNormalEncryption,
            client->GetPasswordSyncState());

  // Sync paused due to a persistent auth error.
  sync_service()->SetPersistentAuthError();
  EXPECT_EQ(password_manager::SyncState::kNotSyncing,
            client->GetPasswordSyncState());

  // Again, using a custom passphrase.
  sync_service()->ClearAuthError();
  sync_service()->SetIsUsingExplicitPassphrase(true);

  EXPECT_EQ(password_manager::SyncState::kSyncingWithCustomPassphrase,
            client->GetPasswordSyncState());

  // Report correctly if we aren't syncing passwords.
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kBookmarks});

  EXPECT_EQ(password_manager::SyncState::kNotSyncing,
            client->GetPasswordSyncState());

  // Again, without a custom passphrase.
  sync_service()->SetIsUsingExplicitPassphrase(false);

  EXPECT_EQ(password_manager::SyncState::kNotSyncing,
            client->GetPasswordSyncState());
}

TEST_F(ChromePasswordManagerClientTest,
       SavingPasswordsTrueDeterminedByService) {
  // Test that saving passwords depends on querying the settings service.
  ChromePasswordManagerClient* client = GetClient();
  ON_CALL(settings_service(),
          IsSettingEnabled(PasswordManagerSetting::kOfferToSavePasswords))
      .WillByDefault(Return(true));
  const GURL kUrlOn("https://accounts.google.com");
  EXPECT_TRUE(client->IsSavingAndFillingEnabled(kUrlOn));
}

TEST_F(ChromePasswordManagerClientTest,
       SavingPasswordsFalseDeterminedByService) {
  // Test that saving passwords depends on querying the settings service.
  ChromePasswordManagerClient* client = GetClient();
  EXPECT_CALL(settings_service(),
              IsSettingEnabled(PasswordManagerSetting::kOfferToSavePasswords))
      .WillOnce(Return(false));
  const GURL kUrlOn("https://accounts.google.com");
  EXPECT_FALSE(client->IsSavingAndFillingEnabled(kUrlOn));
}

TEST_F(ChromePasswordManagerClientTest, SavingAndFillingEnabledConditionsTest) {
  std::unique_ptr<WebContents> test_web_contents(
      content::WebContentsTester::CreateTestWebContents(
          web_contents()->GetBrowserContext(), nullptr));
  autofill::ChromeAutofillClient::CreateForWebContents(test_web_contents.get());
  MockChromePasswordManagerClient* client =
      MockChromePasswordManagerClient::CreateForWebContentsAndGet(
          test_web_contents.get());
  ON_CALL(*client, GetMainFrameCertStatus()).WillByDefault(Return(0));
  // Functionality disabled if there is an SSL error.
  EXPECT_CALL(*client, GetMainFrameCertStatus())
      .WillRepeatedly(Return(net::CERT_STATUS_AUTHORITY_INVALID));
  const GURL kUrlOn("https://accounts.google.com");
  EXPECT_FALSE(client->IsSavingAndFillingEnabled(kUrlOn));
  EXPECT_FALSE(client->IsFillingEnabled(kUrlOn));

  // Disable password saving.
  ON_CALL(settings_service(),
          IsSettingEnabled(PasswordManagerSetting::kOfferToSavePasswords))
      .WillByDefault(Return(false));

  // Functionality disabled if there are SSL errors and the manager itself is
  // disabled.
  EXPECT_FALSE(client->IsSavingAndFillingEnabled(kUrlOn));
  EXPECT_FALSE(client->IsFillingEnabled(kUrlOn));

  // Saving disabled if there are no SSL errors, but the manager itself is
  // disabled.
  EXPECT_CALL(*client, GetMainFrameCertStatus()).WillRepeatedly(Return(0));
  EXPECT_FALSE(client->IsSavingAndFillingEnabled(kUrlOn));
  EXPECT_TRUE(client->IsFillingEnabled(kUrlOn));

  // Enable password saving.
  ON_CALL(settings_service(),
          IsSettingEnabled(PasswordManagerSetting::kOfferToSavePasswords))
      .WillByDefault(Return(true));

  // Functionality enabled if there are no SSL errors and the manager is
  // enabled.
  EXPECT_CALL(*client, GetMainFrameCertStatus()).WillRepeatedly(Return(0));
  EXPECT_TRUE(client->IsSavingAndFillingEnabled(kUrlOn));
  EXPECT_TRUE(client->IsFillingEnabled(kUrlOn));
}

TEST_F(ChromePasswordManagerClientTest,
       SavingAndFillingDisabledConditionsInGuestAndIncognitoProfiles) {
  ON_CALL(settings_service(),
          IsSettingEnabled(PasswordManagerSetting::kOfferToSavePasswords))
      .WillByDefault(Return(true));
  std::unique_ptr<WebContents> incognito_web_contents(
      content::WebContentsTester::CreateTestWebContents(
          profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true), nullptr));
  autofill::ChromeAutofillClient::CreateForWebContents(
      incognito_web_contents.get());
  MockChromePasswordManagerClient* client =
      MockChromePasswordManagerClient::CreateForWebContentsAndGet(
          incognito_web_contents.get());
  EXPECT_CALL(*client, GetMainFrameCertStatus()).WillRepeatedly(Return(0));

  // Saving disabled in Incognito mode.
  const GURL kUrlOn("https://accounts.google.com");
  EXPECT_FALSE(client->IsSavingAndFillingEnabled(kUrlOn));
  EXPECT_TRUE(client->IsFillingEnabled(kUrlOn));

  // In guest mode saving, filling and manual filling are disabled.
  profile()->SetGuestSession(true);
  profile()
      ->GetPrimaryOTRProfile(/*create_if_needed=*/true)
      ->AsTestingProfile()
      ->SetGuestSession(true);
  EXPECT_FALSE(client->IsSavingAndFillingEnabled(kUrlOn));
  EXPECT_FALSE(client->IsFillingEnabled(kUrlOn));
}

TEST_F(ChromePasswordManagerClientTest, ReceivesAutofillPredictions) {
  constexpr char kUrl[] = "https://www.foo.com/login.html";

  NavigateAndCommit(GURL(kUrl));
  ContentAutofillDriver* autofill_driver =
      ContentAutofillClient::FromWebContents(web_contents())
          ->GetAutofillDriverFactory()
          ->DriverForFrame(main_rfh());
  ASSERT_TRUE(autofill_driver);

  FormData form = CreateFormForRenderHost(
      *main_rfh(),
      {CreateTestFormField("Username", "username", "", "text"),
       CreateTestFormField("Password", "password", "", "password")});
  form.name = u"login";

  {
    autofill::TestAutofillManagerWaiter waiter(
        autofill_driver->GetAutofillManager(),
        {autofill::AutofillManagerEvent::kFormsSeen});
    autofill_driver->renderer_events().FormsSeen(/*updated_forms=*/{form},
                                                 /*removed_forms=*/{});
    waiter.Wait(/*num_awaiting_calls=*/1);
  }

  // Simulate that the field types have been determined, since server
  // communication is turned off.
  using Observer = autofill::AutofillManager::Observer;
  autofill_driver->GetAutofillManager().NotifyObservers(
      &Observer::OnFieldTypesDetermined, form.global_id(),
      Observer::FieldTypeSource::kAutofillServer);

  EXPECT_THAT(GetClient()->GetPasswordManager()->GetFormPredictionsForTesting(),
              UnorderedElementsAre(Key(CalculateFormSignature(form))));
}

TEST_F(ChromePasswordManagerClientTest,
       ReceivesAutofillPredictionsFromMultipleFrames) {
  constexpr char kUrl1[] = "https://www.foo.com/login.html";
  constexpr char kUrl2[] = "https://www.foo.com/otp.html";

  NavigateAndCommit(GURL(kUrl1));
  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh())
          ->AppendChild(std::string("child"));
  child_rfh = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kUrl2), child_rfh);
  ContentAutofillClient* autofill_client =
      ContentAutofillClient::FromWebContents(web_contents());
  ASSERT_TRUE(autofill_client);
  ContentAutofillDriver* main_driver =
      autofill_client->GetAutofillDriverFactory()->DriverForFrame(main_rfh());
  ContentAutofillDriver* child_driver =
      autofill_client->GetAutofillDriverFactory()->DriverForFrame(child_rfh);
  ASSERT_TRUE(main_driver);
  ASSERT_TRUE(child_driver);

  FormData main_form = CreateFormForRenderHost(
      *main_rfh(),
      {CreateTestFormField("Username", "username", "", "text"),
       CreateTestFormField("Password", "password", "", "password")});
  FormData child_form = CreateFormForRenderHost(
      *child_rfh, {CreateTestFormField("OTP", "OTP", "", "text")});

  // Ensure that the child frame is picked up as a child frame of `main_form`.
  {
    autofill::FrameTokenWithPredecessor child_frame_information;
    child_frame_information.token = child_form.host_frame;
    main_form.child_frames = {child_frame_information};
  }

  {
    autofill::TestAutofillManagerWaiter waiter(
        main_driver->GetAutofillManager(),
        {autofill::AutofillManagerEvent::kFormsSeen});
    main_driver->renderer_events().FormsSeen(/*updated_forms=*/{main_form},
                                             /*removed_forms=*/{});
    child_driver->renderer_events().FormsSeen(/*updated_forms=*/{child_form},
                                              /*removed_forms=*/{});
    waiter.Wait(/*num_awaiting_calls=*/2);
  }

  // Simulate that the field types have been determined, since server
  // communication is turned off.
  using Observer = autofill::AutofillManager::Observer;
  main_driver->GetAutofillManager().NotifyObservers(
      &Observer::OnFieldTypesDetermined, main_form.global_id(),
      Observer::FieldTypeSource::kAutofillServer);

  // Even though `OnFieldTypesDetermined` was only called for a single form (the
  // browser form that is the result of merging both forms), password manager
  // receives predictions for both the main and the child form.
  EXPECT_THAT(GetClient()->GetPasswordManager()->GetFormPredictionsForTesting(),
              UnorderedElementsAre(Key(CalculateFormSignature(main_form)),
                                   Key(CalculateFormSignature(child_form))));
}

TEST_F(ChromePasswordManagerClientTest, AutoSignInEnabledDeterminedByService) {
#if BUILDFLAG(IS_ANDROID)
  if (BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif

  // Test that auto sign in being allowed depends on querying the settings
  // service.
  ChromePasswordManagerClient* client = GetClient();
  ON_CALL(settings_service(),
          IsSettingEnabled(PasswordManagerSetting::kAutoSignIn))
      .WillByDefault(Return(true));
  EXPECT_TRUE(client->IsAutoSignInEnabled());
}

TEST_F(ChromePasswordManagerClientTest,
       AutoSignInDisableddDeterminedByService) {
#if BUILDFLAG(IS_ANDROID)
  if (BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif
  // Test that auto sign in being disallowed depends on querying the settings
  // service.
  ChromePasswordManagerClient* client = GetClient();
  ON_CALL(settings_service(),
          IsSettingEnabled(PasswordManagerSetting::kAutoSignIn))
      .WillByDefault(Return(false));
  EXPECT_FALSE(client->IsAutoSignInEnabled());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(ChromePasswordManagerClientTest, AutoSignInDisabledOnAutomotive) {
  if (!BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should only run on automotive.";
  }
  EXPECT_FALSE(GetClient()->IsAutoSignInEnabled());
}
#endif

class ChromePasswordManagerClientAutomatedTest
    : public ChromePasswordManagerClientTest,
      public testing::WithParamInterface<bool> {
 public:
  ChromePasswordManagerClientAutomatedTest() {
    if (GetParam()) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kEnableAutomation);
    }
  }
};

INSTANTIATE_TEST_SUITE_P(AutomatedTestPasswordHandling,
                         ChromePasswordManagerClientAutomatedTest,
                         testing::Bool());

TEST_P(ChromePasswordManagerClientAutomatedTest, SavingDependsOnAutomation) {
  // Test that saving passwords UI is disabled for automated tests,
  // and enabled for non-automated tests.
  ChromePasswordManagerClient* client = GetClient();
  // If saving isn't allowed it shouldn't be due to the setting, so make
  // sure that is enabled.
  ON_CALL(settings_service(),
          IsSettingEnabled(PasswordManagerSetting::kOfferToSavePasswords))
      .WillByDefault(Return(true));
  const GURL kUrlOn("https://accounts.google.com");
  EXPECT_NE(client->IsSavingAndFillingEnabled(kUrlOn), GetParam());
}

// Check that password manager is disabled on about:blank pages.
// See https://crbug.com/756587.
TEST_F(ChromePasswordManagerClientTest, SavingAndFillingDisabledForAboutBlank) {
  const GURL kUrl(url::kAboutBlankURL);
  NavigateAndCommit(kUrl);
  EXPECT_TRUE(GetClient()->GetLastCommittedOrigin().opaque());
  EXPECT_FALSE(GetClient()->IsSavingAndFillingEnabled(kUrl));
  EXPECT_FALSE(GetClient()->IsFillingEnabled(kUrl));
}

TEST_F(ChromePasswordManagerClientTest,
       IsFillingAndSavingOnGooglePasswordPage) {
  PasswordManagerClient* client = GetClient();
  EXPECT_FALSE(client->IsSavingAndFillingEnabled(
      GURL("https://passwords.google.com/path?query=1")));
  EXPECT_FALSE(client->IsFillingEnabled(
      GURL("https://passwords.google.com/path?query=1")));
}

namespace {

struct SchemeTestCase {
  const char* scheme;
  bool password_manager_works;
};
const SchemeTestCase kSchemeTestCases[] = {
    {url::kHttpScheme, true},
    {url::kHttpsScheme, true},
    {url::kDataScheme, true},

    {"invalid-scheme-i-just-made-up", false},
    {content::kChromeDevToolsScheme, false},
    {content::kChromeUIScheme, false},
    {url::kMailToScheme, false},
};

// Parameterized test that takes a URL scheme as a parameter. Every scheme
// requires a separate test because NavigateAndCommit can be called only once.
class ChromePasswordManagerClientSchemeTest
    : public ChromePasswordManagerClientTest,
      public ::testing::WithParamInterface<const char*> {
 public:
  void SetUp() override {
    ChromePasswordManagerClientTest::SetUp();
    PasswordStoreFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(),
        base::BindRepeating(&password_manager::BuildPasswordStoreInterface<
                            content::BrowserContext,
                            password_manager::MockPasswordStoreInterface>));
  }

  static std::vector<const char*> GetSchemes() {
    std::vector<const char*> result;
    for (const SchemeTestCase& test_case : kSchemeTestCases) {
      result.push_back(test_case.scheme);
    }
    return result;
  }
};

TEST_P(ChromePasswordManagerClientSchemeTest,
       SavingAndFillingOnDifferentSchemes) {
  const GURL url(base::StringPrintf("%s://example.org", GetParam()));
  VLOG(0) << url.possibly_invalid_spec();
  NavigateAndCommit(url);
  EXPECT_EQ(url::Origin::Create(url).GetURL(),
            GetClient()->GetLastCommittedOrigin().GetURL());

  auto* it = base::ranges::find_if(kSchemeTestCases, [](auto test_case) {
    return strcmp(test_case.scheme, GetParam()) == 0;
  });
  // If saving isn't allowed it shouldn't be due to the setting, so make
  // sure that is enabled.
  ON_CALL(settings_service(),
          IsSettingEnabled(PasswordManagerSetting::kOfferToSavePasswords))
      .WillByDefault(Return(true));
  ASSERT_FALSE(it == std::end(kSchemeTestCases));
  EXPECT_EQ(it->password_manager_works,
            GetClient()->IsSavingAndFillingEnabled(url));
  EXPECT_EQ(it->password_manager_works, GetClient()->IsFillingEnabled(url));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromePasswordManagerClientSchemeTest,
    ::testing::ValuesIn(ChromePasswordManagerClientSchemeTest::GetSchemes()));

}  // namespace

TEST_F(ChromePasswordManagerClientTest, GetLastCommittedEntryURL_Empty) {
  EXPECT_TRUE(GetClient()->GetLastCommittedOrigin().opaque());
}

TEST_F(ChromePasswordManagerClientTest, GetLastCommittedEntryURL) {
  GURL kUrl(
      "https://accounts.google.com/ServiceLogin?continue="
      "https://passwords.google.com/settings");
  NavigateAndCommit(kUrl);
  EXPECT_EQ(url::Origin::Create(kUrl), GetClient()->GetLastCommittedOrigin());
}

TEST_F(ChromePasswordManagerClientTest, WebUINoLogging) {
  // Make sure that logging is active.
  autofill::LogRouter* log_router =
      password_manager::PasswordManagerLogRouterFactory::GetForBrowserContext(
          profile());
  DummyLogReceiver log_receiver;
  log_router->RegisterReceiver(&log_receiver);

  // But then navigate to a WebUI, there the logging should not be active.
  NavigateAndCommit(GURL("chrome://password-manager-internals/"));
  EXPECT_FALSE(GetClient()->GetLogManager()->IsLoggingActive());

  log_router->UnregisterReceiver(&log_receiver);
}

// Metrics enabled, syncing with non-custom passphrase: Do not annotate.
TEST_F(ChromePasswordManagerClientTest,
       AnnotateNavigationEntryWithMetricsNoCustom) {
  sync_service()->SetIsUsingExplicitPassphrase(false);
  metrics_enabled_ = true;

  NavigateAndCommit(GURL("about:blank"));
  GetClient()->AnnotateNavigationEntry(true);

  EXPECT_EQ(
      SerializedNavigationEntry::HAS_PASSWORD_FIELD,
      GetPasswordStateFromNavigation(controller().GetLastCommittedEntry()));
}

// Metrics disabled, syncing with non-custom passphrase: Do not annotate.
TEST_F(ChromePasswordManagerClientTest,
       AnnotateNavigationEntryNoMetricsNoCustom) {
  sync_service()->SetIsUsingExplicitPassphrase(false);
  metrics_enabled_ = false;

  NavigateAndCommit(GURL("about:blank"));
  GetClient()->AnnotateNavigationEntry(true);

  EXPECT_EQ(
      SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN,
      GetPasswordStateFromNavigation(controller().GetLastCommittedEntry()));
}

// Metrics enabled, syncing with custom passphrase: Do not annotate.
TEST_F(ChromePasswordManagerClientTest,
       AnnotateNavigationEntryWithMetricsWithCustom) {
  sync_service()->SetIsUsingExplicitPassphrase(true);
  metrics_enabled_ = true;

  NavigateAndCommit(GURL("about:blank"));
  GetClient()->AnnotateNavigationEntry(true);

  EXPECT_EQ(
      SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN,
      GetPasswordStateFromNavigation(controller().GetLastCommittedEntry()));
}

// Metrics disabled, syncing with custom passphrase: Do not annotate.
TEST_F(ChromePasswordManagerClientTest,
       AnnotateNavigationEntryNoMetricsWithCustom) {
  sync_service()->SetIsUsingExplicitPassphrase(true);
  metrics_enabled_ = false;

  NavigateAndCommit(GURL("about:blank"));
  GetClient()->AnnotateNavigationEntry(true);

  EXPECT_EQ(
      SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN,
      GetPasswordStateFromNavigation(controller().GetLastCommittedEntry()));
}

// State transition: Unannotated
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryUnannotated) {
  SetupNavigationForAnnotation();

  EXPECT_EQ(
      SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN,
      GetPasswordStateFromNavigation(controller().GetLastCommittedEntry()));
}

// State transition: unknown->false
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryToFalse) {
  SetupNavigationForAnnotation();

  GetClient()->AnnotateNavigationEntry(false);
  EXPECT_EQ(
      SerializedNavigationEntry::NO_PASSWORD_FIELD,
      GetPasswordStateFromNavigation(controller().GetLastCommittedEntry()));
}

// State transition: false->true
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryToTrue) {
  SetupNavigationForAnnotation();

  GetClient()->AnnotateNavigationEntry(false);
  GetClient()->AnnotateNavigationEntry(true);
  EXPECT_EQ(
      SerializedNavigationEntry::HAS_PASSWORD_FIELD,
      GetPasswordStateFromNavigation(controller().GetLastCommittedEntry()));
}

// State transition: true->false (retains true)
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryTrueToFalse) {
  SetupNavigationForAnnotation();

  GetClient()->AnnotateNavigationEntry(true);
  GetClient()->AnnotateNavigationEntry(false);
  EXPECT_EQ(
      SerializedNavigationEntry::HAS_PASSWORD_FIELD,
      GetPasswordStateFromNavigation(controller().GetLastCommittedEntry()));
}

// Handle missing ChromePasswordManagerClient instance in BindCredentialManager
// gracefully.
TEST_F(ChromePasswordManagerClientTest, BindCredentialManager_MissingInstance) {
  // Create a WebContent without tab helpers.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile()));
  // In particular, this WebContent should not have the
  // ChromePasswordManagerClient.
  ASSERT_FALSE(
      ChromePasswordManagerClient::FromWebContents(web_contents.get()));

  // This call should not crash.
  ChromePasswordManagerClient::BindCredentialManager(
      web_contents->GetPrimaryMainFrame(), mojo::NullReceiver());
}

TEST_F(ChromePasswordManagerClientTest, CanShowBubbleOnURL) {
  struct TestCase {
    const char* scheme;
    bool can_show_bubble;
  } kTestCases[] = {
    {url::kHttpScheme, true},
    {url::kHttpsScheme, true},
    {url::kDataScheme, true},
    {url::kBlobScheme, true},
    {url::kFileSystemScheme, true},

    {"invalid-scheme-i-just-made-up", false},
#if BUILDFLAG(ENABLE_EXTENSIONS)
    {extensions::kExtensionScheme, false},
#endif
    {url::kAboutScheme, false},
    {content::kChromeDevToolsScheme, false},
    {content::kChromeUIScheme, false},
    {url::kJavaScriptScheme, false},
    {url::kMailToScheme, false},
    {content::kViewSourceScheme, false},
  };

  for (const TestCase& test_case : kTestCases) {
    // CanShowBubbleOnURL currently only depends on the scheme.
    GURL url(base::StringPrintf("%s://example.org", test_case.scheme));
    SCOPED_TRACE(url.possibly_invalid_spec());
    EXPECT_EQ(test_case.can_show_bubble,
              ChromePasswordManagerClient::CanShowBubbleOnURL(url));
  }
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
TEST_F(ChromePasswordManagerClientTest,
       VerifyMaybeStartPasswordFieldOnFocusRequestCalled) {
  std::unique_ptr<WebContents> test_web_contents(
      content::WebContentsTester::CreateTestWebContents(
          web_contents()->GetBrowserContext(), nullptr));
  autofill::ChromeAutofillClient::CreateForWebContents(test_web_contents.get());
  MockChromePasswordManagerClient* client =
      MockChromePasswordManagerClient::CreateForWebContentsAndGet(
          test_web_contents.get());
  ON_CALL(*client, GetMainFrameCertStatus()).WillByDefault(Return(0));
  EXPECT_CALL(*client->password_protection_service(),
              MaybeStartPasswordFieldOnFocusRequest(_, _, _, _, _))
      .Times(1);
  PasswordManagerClient* mojom_client = client;
  mojom_client->CheckSafeBrowsingReputation(GURL("http://foo.com/submit"),
                                            GURL("http://foo.com/iframe.html"));
}

// SafeBrowsing Delayed Warnings experiment can delay certain SafeBrowsing
// warnings until user interaction. This test checks that when a SafeBrowsing
// warning is delayed, password saving and filling is disabled on the page.
TEST_F(ChromePasswordManagerClientTest,
       SavingAndFillingDisabledConditionsDelayedWarnings) {
  std::unique_ptr<WebContents> test_web_contents(
      content::WebContentsTester::CreateTestWebContents(
          web_contents()->GetBrowserContext(), nullptr));

  // Warnings are delayed until the user interacts with the page. This is
  // achieved by attaching an observer (SafeBrowsingUserInteractionObserver) to
  // the current WebContents. Create an observer and attach it to simulate a
  // delayed warning.
  auto ui_manager =
      base::MakeRefCounted<safe_browsing::TestSafeBrowsingUIManager>();
  security_interstitials::UnsafeResource resource;
  safe_browsing::SafeBrowsingUserInteractionObserver::CreateForWebContents(
      test_web_contents.get(), resource, /* is_main_frame= */ true, ui_manager);
  autofill::ChromeAutofillClient::CreateForWebContents(test_web_contents.get());
  MockChromePasswordManagerClient* client =
      MockChromePasswordManagerClient::CreateForWebContentsAndGet(
          test_web_contents.get());
  ON_CALL(*client, GetMainFrameCertStatus()).WillByDefault(Return(0));
  // Saving is disabled when the page has a delayed SafeBrowsing warning.
  const GURL kUrlOn("https://accounts.google.com");
  EXPECT_FALSE(client->IsSavingAndFillingEnabled(kUrlOn));
  EXPECT_FALSE(client->IsFillingEnabled(kUrlOn));
}
#endif

TEST_F(ChromePasswordManagerClientTest, MissingUIDelegate) {
  // Checks that the saving fallback methods don't crash if there is no UI
  // delegate. It can happen on ChromeOS login form, for example.
  GURL kUrl("https://example.com/");
  NavigateAndCommit(kUrl);
  PasswordManagerClient* client = GetClient();
  client->ShowManualFallbackForSaving(nullptr, false, false);
  client->HideManualFallbackForSaving();
}

TEST_F(ChromePasswordManagerClientTest,
       RefreshPasswordManagerSettingsIfNeededUPMDisabled) {
#if BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kUnifiedPasswordManagerAndroid);
#endif
  EXPECT_CALL(settings_service(), RequestSettingsFromBackend).Times(0);
  GetClient()->RefreshPasswordManagerSettingsIfNeeded();
}

#if BUILDFLAG(IS_ANDROID)
class ChromePasswordManagerClientAndroidTest
    : public ChromePasswordManagerClientTest {
 protected:
  std::unique_ptr<password_manager::ContentPasswordManagerDriver>
  CreateContentPasswordManagerDriver(content::RenderFrameHost* rfh);

  void SetUp() override;

  void CreateManualFillingController(content::WebContents* web_contents);
  void SetUpGenerationPreconditions(const GURL& url);
  MockPasswordAccessoryControllerImpl* SetUpMockPwdAccessoryForClientUse(
      password_manager::PasswordManagerDriver* driver);

  ManualFillingControllerImpl* controller() {
    return ManualFillingControllerImpl::FromWebContents(web_contents());
  }

  MockManualFillingView* view() {
    return static_cast<MockManualFillingView*>(controller()->view());
  }

  void AdvanceClock(const base::TimeDelta& delta) {
    task_environment()->AdvanceClock(delta);
  }

 private:
  NiceMock<MockPasswordAccessoryController> mock_pwd_controller_;
  NiceMock<MockAddressAccessoryController> mock_address_controller_;
  NiceMock<MockCreditCardAccessoryController> mock_cc_controller_;
};

void ChromePasswordManagerClientAndroidTest::SetUp() {
  ChromePasswordManagerClientTest::SetUp();
  PasswordStoreFactory::GetInstance()->SetTestingFactory(
      GetBrowserContext(),
      base::BindRepeating(
          &password_manager::BuildPasswordStoreInterface<
              content::BrowserContext, MockPasswordStoreInterface>));
  PasswordManagerSettingsServiceFactory::GetInstance()->SetTestingFactory(
      GetBrowserContext(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<NiceMock<MockPasswordManagerSettingsService>>();
      }));
}

std::unique_ptr<password_manager::ContentPasswordManagerDriver>
ChromePasswordManagerClientAndroidTest::CreateContentPasswordManagerDriver(
    content::RenderFrameHost* rfh) {
  return std::make_unique<password_manager::ContentPasswordManagerDriver>(
      rfh, GetClient());
}

void ChromePasswordManagerClientAndroidTest::CreateManualFillingController(
    content::WebContents* web_contents) {
  ManualFillingControllerImpl::CreateForWebContentsForTesting(
      web_contents, mock_pwd_controller_.AsWeakPtr(),
      mock_address_controller_.AsWeakPtr(), mock_cc_controller_.AsWeakPtr(),
      std::make_unique<NiceMock<MockManualFillingView>>());
}

void ChromePasswordManagerClientAndroidTest::SetUpGenerationPreconditions(
    const GURL& url) {
  // Navigate to the url of the form.
  NavigateAndCommit(url);

  // Make sure saving passwords is enabled.
  ON_CALL(settings_service(),
          IsSettingEnabled(PasswordManagerSetting::kOfferToSavePasswords))
      .WillByDefault(Return(true));

  // Password sync needs to be enabled for generation
  sync_service()->SetIsUsingExplicitPassphrase(false);
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPasswords});

  // Make sure the main frame is focused, so that a focus event on the password
  // field later is considered valid.
  FocusWebContentsOnMainFrame();
}

MockPasswordAccessoryControllerImpl*
ChromePasswordManagerClientAndroidTest::SetUpMockPwdAccessoryForClientUse(
    password_manager::PasswordManagerDriver* driver) {
  // Make a driver supplier, since the password accessory needs one
  base::RepeatingCallback<password_manager::PasswordManagerDriver*(
      content::WebContents*)>
      driver_supplier =
          base::BindLambdaForTesting([=](WebContents*) { return driver; });

  NiceMock<MockManualFillingController> mock_mf_controller;
  std::unique_ptr<MockPasswordAccessoryControllerImpl> mock_pwd_controller =
      std::make_unique<MockPasswordAccessoryControllerImpl>(
          web_contents(), GetClient()->GetCredentialCacheForTesting(),
          mock_mf_controller.AsWeakPtr(), GetClient(),
          std::move(driver_supplier));
  MockPasswordAccessoryControllerImpl* weak_mock_pwd_controller =
      mock_pwd_controller.get();

  // Tie the mock accessory to the |WebContents| so that the client uses is.
  web_contents()->SetUserData(weak_mock_pwd_controller->UserDataKey(),
                              std::move(mock_pwd_controller));

  return weak_mock_pwd_controller;
}

TEST_F(ChromePasswordManagerClientAndroidTest,
       FocusedInputChangedNoFrameFillableField) {
  CreateManualFillingController(web_contents());
  ASSERT_FALSE(web_contents()->GetFocusedFrame());

  ChromePasswordManagerClient* client = GetClient();

  std::unique_ptr<password_manager::ContentPasswordManagerDriver> driver =
      CreateContentPasswordManagerDriver(main_rfh());
  client->FocusedInputChanged(driver.get(), FieldRendererId(123),
                              FocusedFieldType::kFillablePasswordField);

  PasswordGenerationController* pwd_generation_controller =
      PasswordGenerationController::GetIfExisting(web_contents());
  EXPECT_FALSE(pwd_generation_controller);
}

TEST_F(ChromePasswordManagerClientAndroidTest,
       FocusedInputChangedNoFrameNoField) {
  CreateManualFillingController(web_contents());
  std::unique_ptr<password_manager::ContentPasswordManagerDriver> driver =
      CreateContentPasswordManagerDriver(main_rfh());

  // Simulate that an element was focused before as far as the generation
  // controller is concerned.
  PasswordGenerationController* pwd_generation_controller =
      PasswordGenerationController::GetOrCreate(web_contents());
  pwd_generation_controller->FocusedInputChanged(
      FocusedFieldType::kFillablePasswordField, base::AsWeakPtr(driver.get()));

  ChromePasswordManagerClient* client = GetClient();

  ASSERT_FALSE(web_contents()->GetFocusedFrame());
  ASSERT_TRUE(pwd_generation_controller->GetActiveFrameDriver());
  client->FocusedInputChanged(driver.get(), FieldRendererId(123),
                              FocusedFieldType::kUnknown);

  // Check that the event was processed by the generation controller and that
  // the active frame driver was unset.
  EXPECT_FALSE(pwd_generation_controller->GetActiveFrameDriver());
}

TEST_F(ChromePasswordManagerClientAndroidTest, FocusedInputChangedWrongFrame) {
  ChromePasswordManagerClient* client = GetClient();

  // Set up the main frame.
  NavigateAndCommit(GURL("https://example.com"));
  FocusWebContentsOnMainFrame();
  CreateManualFillingController(web_contents());

  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  std::unique_ptr<password_manager::ContentPasswordManagerDriver> driver =
      CreateContentPasswordManagerDriver(subframe);
  client->FocusedInputChanged(driver.get(), FieldRendererId(123),
                              FocusedFieldType::kFillablePasswordField);

  PasswordGenerationController* pwd_generation_controller =
      PasswordGenerationController::GetIfExisting(web_contents());

  // Check that no generation controller was created, since this event should be
  // rejected and not passed on to a generation controller.
  EXPECT_FALSE(pwd_generation_controller);
}

TEST_F(ChromePasswordManagerClientAndroidTest, FocusedInputChangedGoodFrame) {
  ChromePasswordManagerClient* client = GetClient();
  CreateManualFillingController(web_contents());

  std::unique_ptr<password_manager::ContentPasswordManagerDriver> driver =
      CreateContentPasswordManagerDriver(main_rfh());
  FocusWebContentsOnMainFrame();
  client->FocusedInputChanged(driver.get(), FieldRendererId(123),
                              FocusedFieldType::kFillablePasswordField);

  PasswordGenerationController* pwd_generation_controller =
      PasswordGenerationController::GetIfExisting(web_contents());
  EXPECT_TRUE(pwd_generation_controller);
}

TEST_F(ChromePasswordManagerClientAndroidTest,
       FocusedInputChangedFormsNotFetchedMessagesFeature) {
  FormData observed_form_data = MakePasswordFormData();
  SetUpGenerationPreconditions(observed_form_data.url);

  std::unique_ptr<password_manager::ContentPasswordManagerDriver> driver =
      CreateContentPasswordManagerDriver(main_rfh());

  // Since the test uses a mock store, the consumer won't be called
  // back with results, which simulates the password forms not being fetched
  // before the field is focused.
  driver->GetPasswordManager()->OnPasswordFormsParsed(driver.get(),
                                                      {observed_form_data});

  MockPasswordAccessoryControllerImpl* weak_mock_pwd_controller =
      SetUpMockPwdAccessoryForClientUse(driver.get());

  EXPECT_CALL(
      *weak_mock_pwd_controller,
      RefreshSuggestionsForField(FocusedFieldType::kFillablePasswordField,
                                 /*is_manual_generation_available=*/false));
  GetClient()->FocusedInputChanged(
      driver.get(), observed_form_data.fields[0].unique_renderer_id,
      FocusedFieldType::kFillablePasswordField);
}

TEST_F(ChromePasswordManagerClientAndroidTest,
       FocusedInputChangedFormsFetched) {
  FormData observed_form_data = MakePasswordFormData();
  SetUpGenerationPreconditions(observed_form_data.url);

  std::unique_ptr<password_manager::ContentPasswordManagerDriver> driver =
      CreateContentPasswordManagerDriver(main_rfh());

  // Simulate credential fetching from the store.
  MockPasswordStoreInterface* mock_store =
      static_cast<MockPasswordStoreInterface*>(
          GetClient()->GetProfilePasswordStore());
  base::WeakPtr<PasswordStoreConsumer> store_consumer;
  EXPECT_CALL(*mock_store, GetLogins(_, _))
      .WillOnce(SaveArg<1>(&store_consumer));
  driver->GetPasswordManager()->OnPasswordFormsParsed(driver.get(),
                                                      {observed_form_data});

  std::vector<std::unique_ptr<PasswordForm>> forms;
  forms.push_back(MakePasswordForm());
  store_consumer->OnGetPasswordStoreResultsOrErrorFrom(mock_store,
                                                       std::move(forms));

  MockPasswordAccessoryControllerImpl* weak_mock_pwd_controller =
      SetUpMockPwdAccessoryForClientUse(driver.get());
  EXPECT_CALL(
      *weak_mock_pwd_controller,
      RefreshSuggestionsForField(FocusedFieldType::kFillablePasswordField,
                                 /*is_manual_generation_available=*/true));
  GetClient()->FocusedInputChanged(
      driver.get(), observed_form_data.fields[0].unique_renderer_id,
      FocusedFieldType::kFillablePasswordField);
}

TEST_F(ChromePasswordManagerClientAndroidTest,
       FocusedInputChangeUpdatesCredManReentryUi) {
  std::unique_ptr<password_manager::ContentPasswordManagerDriver> driver =
      CreateContentPasswordManagerDriver(main_rfh());
  MockPasswordAccessoryControllerImpl* weak_mock_pwd_controller =
      SetUpMockPwdAccessoryForClientUse(driver.get());

  EXPECT_CALL(*weak_mock_pwd_controller, UpdateCredManReentryUi);

  GetClient()->FocusedInputChanged(driver.get(), FieldRendererId(123),
                                   FocusedFieldType::kFillablePasswordField);
}

TEST_F(ChromePasswordManagerClientAndroidTest,
       SameDocumentNavigationDoesNotClearCache) {
  auto origin = url::Origin::Create(GURL("https://example.com"));
  auto form = MakePasswordForm();
  GetClient()
      ->GetCredentialCacheForTesting()
      ->SaveCredentialsAndBlocklistedForOrigin(
          {form.get()}, CredentialCache::IsOriginBlocklisted(false), origin);

  // Check that a navigation within the same document does not clear the cache.
  content::MockNavigationHandle handle(web_contents());
  handle.set_is_same_document(true);
  handle.set_has_committed(true);
  static_cast<content::WebContentsObserver*>(GetClient())
      ->DidFinishNavigation(&handle);

  EXPECT_FALSE(GetClient()
                   ->GetCredentialCacheForTesting()
                   ->GetCredentialStore(origin)
                   .GetCredentials()
                   .empty());

  // Check that a navigation to a different origin clears the cache.
  NavigateAndCommit(GURL("https://example.org"));
  EXPECT_TRUE(GetClient()
                  ->GetCredentialCacheForTesting()
                  ->GetCredentialStore(origin)
                  .GetCredentials()
                  .empty());
}

TEST_F(ChromePasswordManagerClientAndroidTest, HideFillingUIOnNavigatingAway) {
  CreateManualFillingController(web_contents());
  // Navigate to a URL with a bubble/popup.
  GURL kUrl1("https://example.com/");
  NavigateAndCommit(kUrl1);
  EXPECT_TRUE(ChromePasswordManagerClient::CanShowBubbleOnURL(kUrl1));

  // Navigating away should call Hide.
  EXPECT_CALL(*view(), Hide());
  GURL kUrl2("https://accounts.google.com");
  NavigateAndCommit(kUrl2);
}

TEST_F(ChromePasswordManagerClientAndroidTest,
       FormSubmissionTrackingAfterTouchToLogin_NotStarted) {
  // As tracking is not started yet, no metric reports are expected.
  base::HistogramTester uma_recorder;
  GetClient()->NotifyOnSuccessfulLogin(u"username");
  uma_recorder.ExpectTotalCount(
      "PasswordManager.TouchToFill.TimeToSuccessfulLogin", 0);
  uma_recorder.ExpectTotalCount(
      "PasswordManager.TouchToFill.SuccessfulSubmissionWasObserved", 0);
}

TEST_F(ChromePasswordManagerClientAndroidTest,
       FormSubmissionTrackingAfterTouchToLogin_Resetted) {
  // Tracking started, but was reset before a successful login (e.g. a user
  // manually edited a field).
  base::HistogramTester uma_recorder;
  GetClient()->StartSubmissionTrackingAfterTouchToFill(u"username");
  GetClient()->ResetSubmissionTrackingAfterTouchToFill();
  GetClient()->NotifyOnSuccessfulLogin(u"username");
  uma_recorder.ExpectTotalCount(
      "PasswordManager.TouchToFill.TimeToSuccessfulLogin", 0);
  uma_recorder.ExpectUniqueSample(
      "PasswordManager.TouchToFill.SuccessfulSubmissionWasObserved", false, 1);
}

TEST_F(ChromePasswordManagerClientAndroidTest,
       FormSubmissionTrackingAfterTouchToLogin_AnotherUsername) {
  // Tracking started but a successful login was observed for a wrong
  // username.
  base::HistogramTester uma_recorder;
  GetClient()->StartSubmissionTrackingAfterTouchToFill(u"username");
  GetClient()->NotifyOnSuccessfulLogin(u"another_username");
  uma_recorder.ExpectTotalCount(
      "PasswordManager.TouchToFill.TimeToSuccessfulLogin", 0);
  uma_recorder.ExpectUniqueSample(
      "PasswordManager.TouchToFill.SuccessfulSubmissionWasObserved", false, 1);
}

TEST_F(ChromePasswordManagerClientAndroidTest,
       FormSubmissionTrackingAfterTouchToLogin_StaleTracking) {
  // Tracking started too long ago, ignore a successful login.
  base::HistogramTester uma_recorder;
  GetClient()->StartSubmissionTrackingAfterTouchToFill(u"username");
  AdvanceClock(base::Minutes(2));
  GetClient()->NotifyOnSuccessfulLogin(u"username");
  uma_recorder.ExpectTotalCount(
      "PasswordManager.TouchToFill.TimeToSuccessfulLogin", 0);
  uma_recorder.ExpectUniqueSample(
      "PasswordManager.TouchToFill.SuccessfulSubmissionWasObserved", false, 1);
}

TEST_F(ChromePasswordManagerClientAndroidTest,
       FormSubmissionTrackingAfterTouchToLogin_Succeeded) {
  // Tracking started and a successful login was observed for the correct
  // username recently, expect a metric report.
  base::HistogramTester uma_recorder;
  GetClient()->StartSubmissionTrackingAfterTouchToFill(u"username");
  GetClient()->NotifyOnSuccessfulLogin(u"username");
  uma_recorder.ExpectTotalCount(
      "PasswordManager.TouchToFill.TimeToSuccessfulLogin", 1);
  uma_recorder.ExpectUniqueSample(
      "PasswordManager.TouchToFill.SuccessfulSubmissionWasObserved", true, 1);
  // Should be reported only once.
  GetClient()->NotifyOnSuccessfulLogin(u"username");
  uma_recorder.ExpectTotalCount(
      "PasswordManager.TouchToFill.TimeToSuccessfulLogin", 1);
  uma_recorder.ExpectUniqueSample(
      "PasswordManager.TouchToFill.SuccessfulSubmissionWasObserved", true, 1);
}

TEST_F(ChromePasswordManagerClientAndroidTest,
       RefreshPasswordManagerSettingsIfNeededUPMFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  const std::map<std::string, std::string> params = {
      {"stage", base::NumberToString(static_cast<int>(
                    password_manager::features::UpmExperimentVariation::
                        kEnableForSyncingUsers))}};
  feature_list.InitAndEnableFeatureWithParameters(
      password_manager::features::kUnifiedPasswordManagerAndroid, params);

  EXPECT_CALL(settings_service(), RequestSettingsFromBackend);
  GetClient()->RefreshPasswordManagerSettingsIfNeeded();
}
#endif  //  BUILDFLAG(IS_ANDROID)

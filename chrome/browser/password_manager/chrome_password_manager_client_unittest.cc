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
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/mock_manual_filling_view.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_address_accessory_controller.h"
#include "chrome/browser/password_manager/password_manager_settings_service_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/passwords/password_cross_domain_confirmation_popup_controller_impl.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/autofill_test_utils.h"
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
#include "components/device_reauth/device_authenticator.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/password_manager_log_router_factory.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/mock_password_manager_settings_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/sessions/content/content_record_password_state.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
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
#include "chrome/browser/autofill/mock_manual_filling_view.h"
#include "chrome/browser/keyboard_accessory/android/manual_filling_controller_impl.h"
#include "chrome/browser/keyboard_accessory/android/password_accessory_controller_impl.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_address_accessory_controller.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_manual_filling_controller.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_password_accessory_controller.h"
#include "chrome/browser/keyboard_accessory/test_utils/android/mock_payment_method_accessory_controller.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/android/password_generation_controller.h"
#else
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#endif  // BUILDFLAG(IS_ANDROID)

using autofill::CalculateFormSignature;
using autofill::ContentAutofillClient;
using autofill::ContentAutofillDriver;
using autofill::FieldRendererId;
using autofill::FormControlType;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::mojom::FocusedFieldType;
using autofill::test::CreateFormDataForRenderFrameHost;
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
using device_reauth::BiometricStatus;
using password_manager::CredentialCache;
using password_manager::MockPasswordStoreInterface;
#endif

namespace {

#if BUILDFLAG(IS_ANDROID)
FormData MakePasswordFormData() {
  FormData form_data;
  form_data.set_url(GURL("https://www.example.com/"));
  form_data.set_action(GURL("https://www.example.com/"));
  form_data.set_name(u"form-name");

  FormFieldData field;
  field.set_name(u"password-element");
  field.set_id_attribute(field.name());
  field.set_name_attribute(field.name());
  field.set_form_control_type(autofill::FormControlType::kInputPassword);
  field.set_renderer_id(FieldRendererId(123));
  form_data.set_fields({field});

  return form_data;
}

PasswordForm MakePasswordForm() {
  PasswordForm form;
  form.url = GURL("https://www.example.com/");
  form.action = GURL("https://www.example.com/");
  form.password_element = u"password-element";
  form.submit_element = u"signIn";
  form.signon_realm = "https://www.example.com/";
  form.in_store = PasswordForm::Store::kProfileStore;
  form.match_type = PasswordForm::MatchType::kExact;
  return form;
}
#endif

// TODO(crbug.com/40412780): Get rid of the mocked client in the client's own
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

  void FillPasswordSuggestionById(autofill::FieldRendererId username_element_id,
                                  autofill::FieldRendererId password_element_id,
                                  const std::u16string& username,
                                  const std::u16string& password) override {}

  void PreviewPasswordSuggestionById(
      autofill::FieldRendererId username_element_id,
      autofill::FieldRendererId password_element_id,
      const std::u16string& username,
      const std::u16string& password) override {}

  void InformNoSavedCredentials(
      bool should_show_popup_without_passwords) override {}

  void FillIntoFocusedField(bool is_password,
                            const std::u16string& credential) override {}
  void PreviewField(autofill::FieldRendererId field_id,
                    const std::u16string& value) override {}
  void FillField(autofill::FieldRendererId field_id,
                 const std::u16string& value) override {}
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
                                        base::DoNothing(),
                                        nullptr) {}

  MOCK_METHOD(void,
              RefreshSuggestionsForField,
              (autofill::mojom::FocusedFieldType),
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
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        local_state_(TestingBrowserProcess::GetGlobal()) {
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
              NiceMock<password_manager::MockPasswordManagerSettingsService>>();
        }));
  }

 protected:
  ChromePasswordManagerClient* GetClient();
  password_manager::MockPasswordManagerSettingsService& settings_service() {
    return static_cast<password_manager::MockPasswordManagerSettingsService&>(
        *PasswordManagerSettingsServiceFactory::GetForProfile(profile()));
  }
  syncer::TestSyncService* sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetInstance()->GetForProfile(profile()));
  }

  // If autofill::mojom::PasswordAutofillAgent::SetLoggingState() got called,
  // copies its argument into `activation_flag` and returns true. Otherwise
  // returns false.
  bool WasLoggingActivationMessageSent(bool* activation_flag);

  FakePasswordAutofillAgent fake_agent_;
  ScopedTestingLocalState local_state_;

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
  base::test::ScopedFeatureList scoped_feature_list_;
};

void ChromePasswordManagerClientTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  ProfilePasswordStoreFactory::GetInstance()->SetTestingFactory(
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
}

void ChromePasswordManagerClientTest::TearDown() {
  ChromeRenderViewHostTestHarness::TearDown();
}

ChromePasswordManagerClient* ChromePasswordManagerClientTest::GetClient() {
  return ChromePasswordManagerClient::FromWebContents(web_contents());
}

bool ChromePasswordManagerClientTest::WasLoggingActivationMessageSent(
    bool* activation_flag) {
  base::RunLoop().RunUntilIdle();
  if (!fake_agent_.called_set_logging_state()) {
    return false;
  }

  if (activation_flag) {
    *activation_flag = fake_agent_.logging_state_active();
  }
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
      ContentAutofillDriver::GetForRenderFrameHost(main_rfh());
  ASSERT_TRUE(autofill_driver);

  FormData form = CreateFormDataForRenderFrameHost(
      *main_rfh(), {CreateTestFormField("Username", "username", "",
                                        FormControlType::kInputText),
                    CreateTestFormField("Password", "password", "",
                                        FormControlType::kInputPassword)});
  form.set_name(u"login");

  {
    autofill::TestAutofillManagerWaiter waiter(
        autofill_driver->GetAutofillManager(),
        {autofill::AutofillManagerEvent::kFormsSeen});
    autofill_driver->renderer_events().FormsSeen(/*updated_forms=*/{form},
                                                 /*removed_forms=*/{});
    ASSERT_TRUE(waiter.Wait(/*num_awaiting_calls=*/1));
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
      ContentAutofillDriver::GetForRenderFrameHost(main_rfh());
  ContentAutofillDriver* child_driver =
      ContentAutofillDriver::GetForRenderFrameHost(child_rfh);
  ASSERT_TRUE(main_driver);
  ASSERT_TRUE(child_driver);

  FormData main_form = CreateFormDataForRenderFrameHost(
      *main_rfh(), {CreateTestFormField("Username", "username", "",
                                        FormControlType::kInputText),
                    CreateTestFormField("Password", "password", "",
                                        FormControlType::kInputPassword)});
  FormData child_form = CreateFormDataForRenderFrameHost(
      *child_rfh,
      {CreateTestFormField("OTP", "OTP", "", FormControlType::kInputText)});

  // Ensure that the child frame is picked up as a child frame of `main_form`.
  {
    autofill::FrameTokenWithPredecessor child_frame_information;
    child_frame_information.token = child_form.host_frame();
    main_form.set_child_frames({child_frame_information});
  }

  {
    autofill::TestAutofillManagerWaiter waiter(
        main_driver->GetAutofillManager(),
        {autofill::AutofillManagerEvent::kFormsSeen});
    main_driver->renderer_events().FormsSeen(/*updated_forms=*/{main_form},
                                             /*removed_forms=*/{});
    child_driver->renderer_events().FormsSeen(/*updated_forms=*/{child_form},
                                              /*removed_forms=*/{});
    ASSERT_TRUE(waiter.Wait(/*num_awaiting_calls=*/2));
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
  EXPECT_THAT(static_cast<const password_manager::PasswordManager*>(
                  GetClient()->GetPasswordManager())
                  ->GetFormPredictionsForTesting(),
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

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
// Test that authentication is not possible if the `authenticator` is `nullptr`.
TEST_F(ChromePasswordManagerClientTest, CanUseBiometricAuthNoAuthenticator) {
  EXPECT_FALSE(GetClient()->IsReauthBeforeFillingRequired(
      /*authenticator=*/nullptr));
}

// Test that authentication is not possible if the device doesn't have
// necessary hardware for biometric authentication.
TEST_F(ChromePasswordManagerClientTest, CanUseBiometricAuthNoBiometrics) {
  device_reauth::MockDeviceAuthenticator authenticator;
  // Both prefs are registered by the `PasswordManager`.
  local_state_.Get()->SetBoolean(
      password_manager::prefs::kHadBiometricsAvailable, false);
  profile()->GetTestingPrefService()->SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling, true);
  EXPECT_FALSE(GetClient()->IsReauthBeforeFillingRequired(&authenticator));
}

// Test that authentication is not possible if the user didn't configure the
// corresponding setting.
TEST_F(ChromePasswordManagerClientTest, CanUseBiometricAuthSettingDisabled) {
  device_reauth::MockDeviceAuthenticator authenticator;
  // Both prefs are registered by the `PasswordManager`.
  local_state_.Get()->SetBoolean(
      password_manager::prefs::kHadBiometricsAvailable, true);
  profile()->GetTestingPrefService()->SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling, false);
  EXPECT_FALSE(GetClient()->IsReauthBeforeFillingRequired(&authenticator));
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Test that authentication is possible if both the biometric authentication
// hardware is available and the user configured the corresponding setting.
TEST_F(ChromePasswordManagerClientTest, CanUseBiometricAuthSettingEnabled) {
  device_reauth::MockDeviceAuthenticator authenticator;
  // Both prefs are registered by the `PasswordManager`.
  local_state_.Get()->SetBoolean(
      password_manager::prefs::kHadBiometricsAvailable, true);
  profile()->GetTestingPrefService()->SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling, true);
  EXPECT_TRUE(GetClient()->IsReauthBeforeFillingRequired(&authenticator));
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Test that authentication is possible if biometric authentication
// hardware is available, the user configured the corresponding setting and the
// feature flag is enabled.
TEST_F(ChromePasswordManagerClientTest,
       CanUseBiometricAuthSettingEnabledKillFlagEnabled) {
  device_reauth::MockDeviceAuthenticator authenticator;
  // Both prefs are registered by the `PasswordManager`.
  local_state_.Get()->SetBoolean(
      password_manager::prefs::kHadBiometricsAvailable, true);
  profile()->GetTestingPrefService()->SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling, true);
  base::test::ScopedFeatureList enabled_features(
      password_manager::features::kBiometricsAuthForPwdFill);
  EXPECT_TRUE(GetClient()->IsReauthBeforeFillingRequired(&authenticator));
}

// Tests that reauth is not required if the feature flag is disabled even if the
// user has the required hardware and enabled the setting in the past.
TEST_F(ChromePasswordManagerClientTest,
       CanUseBiometricAuthSettingEnabledKillFlagDisabled) {
  device_reauth::MockDeviceAuthenticator authenticator;
  // Both prefs are registered by the `PasswordManager`.
  local_state_.Get()->SetBoolean(
      password_manager::prefs::kHadBiometricsAvailable, true);
  profile()->GetTestingPrefService()->SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling, true);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{
          password_manager::features::kBiometricsAuthForPwdFill});
  EXPECT_FALSE(GetClient()->IsReauthBeforeFillingRequired(&authenticator));
}
#endif

#if BUILDFLAG(IS_ANDROID)
// Test that authentication is not possible if the `authenticator` is `nullptr`.
TEST_F(ChromePasswordManagerClientTest, CanUseBiometricAuthAndroid) {
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    // Authentication is always available for automotive and the `authenticator`
    // is always available.
    device_reauth::MockDeviceAuthenticator authenticator;
    EXPECT_TRUE(GetClient()->IsReauthBeforeFillingRequired(&authenticator));
  } else {
    EXPECT_FALSE(GetClient()->IsReauthBeforeFillingRequired(
        /*authenticator=*/nullptr));
  }
}

// Test that authentication is not possible if the `kBiometricTouchToFill`
// feature is not enabled.
TEST_F(ChromePasswordManagerClientTest,
       CanUseBiometricAuthAndroidFeatureIsDisabled) {
  // Authentication is always available for automotive.
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP();
  }
  device_reauth::MockDeviceAuthenticator authenticator;
  ON_CALL(authenticator, GetBiometricAvailabilityStatus)
      .WillByDefault(Return(BiometricStatus::kBiometricsAvailable));
  EXPECT_FALSE(GetClient()->IsReauthBeforeFillingRequired(&authenticator));
}

// Test that authentication is not possible if the
// `GetBiometricAvailabilityStatus` returns `kUnavailable` when
// `kBiometricTouchToFill` is enabled.
TEST_F(ChromePasswordManagerClientTest,
       CanUseBiometricAuthAndroidAuthDisabled) {
  // Authentication is always available for automotive.
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP();
  }
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList enabled_features(
      password_manager::features::kBiometricTouchToFill);
  device_reauth::MockDeviceAuthenticator authenticator;
  ON_CALL(authenticator, GetBiometricAvailabilityStatus)
      .WillByDefault(Return(BiometricStatus::kUnavailable));
  EXPECT_FALSE(GetClient()->IsReauthBeforeFillingRequired(&authenticator));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.BiometricAuthPwdFillAndroid."
      "CanAuthenticateWithBiometricOrScreenLock",
      false, 1);
}

// Test that authentication is not possible if the
// `GetBiometricAvailabilityStatus` returns `kBiometricsAvailable`, but the
// `kBiometricReauthBeforePwdFilling` pref is set to false when
// `kBiometricTouchToFill` is enabled.
TEST_F(ChromePasswordManagerClientTest,
       CanUseBiometricAuthAndroidPrefDisabled) {
  // Authentication is always available for automotive.
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP();
  }
  base::test::ScopedFeatureList enabled_features(
      password_manager::features::kBiometricTouchToFill);
  device_reauth::MockDeviceAuthenticator authenticator;
  ON_CALL(authenticator, GetBiometricAvailabilityStatus)
      .WillByDefault(Return(BiometricStatus::kBiometricsAvailable));
  EXPECT_FALSE(GetClient()->IsReauthBeforeFillingRequired(&authenticator));
}

// Test that authentication is possible if the `GetBiometricAvailabilityStatus`
// returns `kOnlyLskfAvailable` and the `kBiometricReauthBeforePwdFilling`
// pref is set to true when `kBiometricTouchToFill` is enabled.
TEST_F(ChromePasswordManagerClientTest, CanUseBiometricAuthAndroidAuthEnabled) {
  // Authentication is always available for automotive.
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP();
  }

  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList enabled_features(
      password_manager::features::kBiometricTouchToFill);
  device_reauth::MockDeviceAuthenticator authenticator;
  profile()->GetTestingPrefService()->SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling, true);
  ON_CALL(authenticator, GetBiometricAvailabilityStatus)
      .WillByDefault(Return(BiometricStatus::kOnlyLskfAvailable));
  EXPECT_TRUE(GetClient()->IsReauthBeforeFillingRequired(&authenticator));
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.BiometricAuthPwdFillAndroid."
      "CanAuthenticateWithBiometricOrScreenLock",
      true, 1);
}

// Test that authentication is possible if the `GetBiometricAvailabilityStatus`
// returns `kBiometricsAvailable` on auto regardless of the pref and flag value.
TEST_F(ChromePasswordManagerClientTest,
       CanUseBiometricAuthAndroidAlwaysTrueOnAutomotive) {
  // Authentication is always available for automotive.
  if (!base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP();
  }
  device_reauth::MockDeviceAuthenticator authenticator;
  ON_CALL(authenticator, GetBiometricAvailabilityStatus)
      .WillByDefault(Return(BiometricStatus::kBiometricsAvailable));
  EXPECT_TRUE(GetClient()->IsReauthBeforeFillingRequired(&authenticator));
}

// Test that `IsReauthBeforeFillingRequired` always returns true for mandatory
// biometric auth.
TEST_F(ChromePasswordManagerClientTest, MandatoryBiometricEnabled) {
  // Authentication is always available for automotive.
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP();
  }
  base::test::ScopedFeatureList enabled_features(
      password_manager::features::kBiometricAuthIdentityCheck);
  device_reauth::MockDeviceAuthenticator authenticator;
  ON_CALL(authenticator, GetBiometricAvailabilityStatus)
      .WillByDefault(Return(BiometricStatus::kRequired));
  EXPECT_TRUE(GetClient()->IsReauthBeforeFillingRequired(&authenticator));
}

#else
// Test that authentication is not possible on other platforms.
TEST_F(ChromePasswordManagerClientTest, CanUseBiometricAuth) {
  device_reauth::MockDeviceAuthenticator authenticator;
  EXPECT_FALSE(GetClient()->IsReauthBeforeFillingRequired(&authenticator));
}
#endif  // BUILDFLAG(IS_ANDROID)

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
    ProfilePasswordStoreFactory::GetInstance()->SetTestingFactory(
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

// State transition: Unannotated
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryUnannotated) {
  NavigateAndCommit(GURL("about:blank"));

  EXPECT_EQ(
      SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN,
      GetPasswordStateFromNavigation(controller().GetLastCommittedEntry()));
}

// State transition: unknown->false
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryToFalse) {
  NavigateAndCommit(GURL("about:blank"));

  GetClient()->AnnotateNavigationEntry(false);
  EXPECT_EQ(
      SerializedNavigationEntry::NO_PASSWORD_FIELD,
      GetPasswordStateFromNavigation(controller().GetLastCommittedEntry()));
}

// State transition: false->true
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryToTrue) {
  NavigateAndCommit(GURL("about:blank"));

  GetClient()->AnnotateNavigationEntry(false);
  GetClient()->AnnotateNavigationEntry(true);
  EXPECT_EQ(
      SerializedNavigationEntry::HAS_PASSWORD_FIELD,
      GetPasswordStateFromNavigation(controller().GetLastCommittedEntry()));
}

// State transition: true->false (retains true)
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryTrueToFalse) {
  NavigateAndCommit(GURL("about:blank"));

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

#if !BUILDFLAG(IS_ANDROID)
// Test that the hats service is called with the expected params.
TEST_F(ChromePasswordManagerClientTest,
       TriggerUserPerceptionOfAutofillPasswordSurvey) {
  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));
  EXPECT_CALL(*mock_hats_service, CanShowAnySurvey)
      .WillRepeatedly(Return(true));

  const std::string filling_assistane = "Automatically filled";
  const SurveyStringData filling_assistance_in_product_data = {
      {"Filling assistance", filling_assistane}};
  EXPECT_CALL(*mock_hats_service,
              LaunchDelayedSurveyForWebContents(
                  kHatsSurveyTriggerAutofillPasswordUserPerception, _, _, _,
                  filling_assistance_in_product_data, _, _, _, _, _));

  GetClient()->TriggerUserPerceptionOfPasswordManagerSurvey(filling_assistane);
}

TEST_F(
    ChromePasswordManagerClientTest,
    TriggerUserPerceptionOfAutofillPasswordSurvey_EmptyFillingAssistanceString_DoNotCallHats) {
  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));

  EXPECT_CALL(*mock_hats_service, LaunchDelayedSurveyForWebContents).Times(0);

  GetClient()->TriggerUserPerceptionOfPasswordManagerSurvey("");
}
#endif

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
      test_web_contents.get(), resource, ui_manager);
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
  NiceMock<MockPaymentMethodAccessoryController>
      mock_payment_method_controller_;
};

void ChromePasswordManagerClientAndroidTest::SetUp() {
  ChromePasswordManagerClientTest::SetUp();
  ProfilePasswordStoreFactory::GetInstance()->SetTestingFactory(
      GetBrowserContext(),
      base::BindRepeating(
          &password_manager::BuildPasswordStoreInterface<
              content::BrowserContext, MockPasswordStoreInterface>));
  AccountPasswordStoreFactory::GetInstance()->SetTestingFactory(
      GetBrowserContext(),
      base::BindRepeating(
          &password_manager::BuildPasswordStoreInterface<
              content::BrowserContext, MockPasswordStoreInterface>));
  PasswordManagerSettingsServiceFactory::GetInstance()->SetTestingFactory(
      GetBrowserContext(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<
            NiceMock<password_manager::MockPasswordManagerSettingsService>>();
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
      mock_address_controller_.AsWeakPtr(),
      mock_payment_method_controller_.AsWeakPtr(),
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

  // Tie the mock accessory to the `WebContents` so that the client uses is.
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
      FocusedFieldType::kFillablePasswordField, driver->AsWeakPtrImpl());

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
  SetUpGenerationPreconditions(observed_form_data.url());

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
      RefreshSuggestionsForField(FocusedFieldType::kFillablePasswordField));
  GetClient()->FocusedInputChanged(driver.get(),
                                   observed_form_data.fields()[0].renderer_id(),
                                   FocusedFieldType::kFillablePasswordField);
}

// https://crbug.com/346331137: Broken after M4 rollout.
TEST_F(ChromePasswordManagerClientAndroidTest,
       DISABLED_FocusedInputChangedFormsFetchedSplitStores) {
  profile()->GetTestingPrefService()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));
  FormData observed_form_data = MakePasswordFormData();
  SetUpGenerationPreconditions(observed_form_data.url());

  std::unique_ptr<password_manager::ContentPasswordManagerDriver> driver =
      CreateContentPasswordManagerDriver(main_rfh());

  // Simulate credential fetching from the stores.
  MockPasswordStoreInterface* mock_account_store =
      static_cast<MockPasswordStoreInterface*>(
          GetClient()->GetAccountPasswordStore());
  base::WeakPtr<PasswordStoreConsumer> store_consumer;
  EXPECT_CALL(*mock_account_store, IsAbleToSavePasswords)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_account_store, GetLogins(_, _))
      .WillOnce(SaveArg<1>(&store_consumer));

  MockPasswordStoreInterface* mock_profile_store =
      static_cast<MockPasswordStoreInterface*>(
          GetClient()->GetProfilePasswordStore());
  // The consumer for both stores is the same.
  EXPECT_CALL(*mock_profile_store, GetLogins(_, _));

  driver->GetPasswordManager()->OnPasswordFormsParsed(driver.get(),
                                                      {observed_form_data});

  std::vector<PasswordForm> account_store_forms = {MakePasswordForm()};
  store_consumer->OnGetPasswordStoreResultsOrErrorFrom(
      mock_account_store, std::move(account_store_forms));

  std::vector<PasswordForm> profile_store_forms = {MakePasswordForm()};
  store_consumer->OnGetPasswordStoreResultsOrErrorFrom(
      mock_profile_store, std::move(profile_store_forms));

  MockPasswordAccessoryControllerImpl* weak_mock_pwd_controller =
      SetUpMockPwdAccessoryForClientUse(driver.get());
  EXPECT_CALL(
      *weak_mock_pwd_controller,
      RefreshSuggestionsForField(FocusedFieldType::kFillablePasswordField));
  GetClient()->FocusedInputChanged(driver.get(),
                                   observed_form_data.fields()[0].renderer_id(),
                                   FocusedFieldType::kFillablePasswordField);
}

// https://crbug.com/346331137: Broken after M4 rollout.
TEST_F(ChromePasswordManagerClientAndroidTest,
       DISABLED_FocusedInputChangedFormsFetchedSingleStore) {
  profile()->GetTestingPrefService()->SetInteger(
      password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));
  FormData observed_form_data = MakePasswordFormData();
  SetUpGenerationPreconditions(observed_form_data.url());

  std::unique_ptr<password_manager::ContentPasswordManagerDriver> driver =
      CreateContentPasswordManagerDriver(main_rfh());

  // Simulate credential fetching from the store.
  base::WeakPtr<PasswordStoreConsumer> store_consumer;
  MockPasswordStoreInterface* mock_profile_store =
      static_cast<MockPasswordStoreInterface*>(
          GetClient()->GetProfilePasswordStore());

  EXPECT_CALL(*mock_profile_store, IsAbleToSavePasswords)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_profile_store, GetLogins(_, _))
      .WillOnce(SaveArg<1>(&store_consumer));
  driver->GetPasswordManager()->OnPasswordFormsParsed(driver.get(),
                                                      {observed_form_data});

  std::vector<PasswordForm> forms = {MakePasswordForm()};
  store_consumer->OnGetPasswordStoreResultsOrErrorFrom(mock_profile_store,
                                                       std::move(forms));

  MockPasswordAccessoryControllerImpl* weak_mock_pwd_controller =
      SetUpMockPwdAccessoryForClientUse(driver.get());
  EXPECT_CALL(
      *weak_mock_pwd_controller,
      RefreshSuggestionsForField(FocusedFieldType::kFillablePasswordField));
  GetClient()->FocusedInputChanged(driver.get(),
                                   observed_form_data.fields()[0].renderer_id(),
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
  std::vector<PasswordForm> forms = {MakePasswordForm()};
  GetClient()
      ->GetCredentialCacheForTesting()
      ->SaveCredentialsAndBlocklistedForOrigin(
          forms, CredentialCache::IsOriginBlocklisted(false), origin);

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
  EXPECT_CALL(settings_service(), RequestSettingsFromBackend);
  GetClient()->RefreshPasswordManagerSettingsIfNeeded();
}

class ChromePasswordManagerClientWithAccountStoreAndroidTest
    : public ChromePasswordManagerClientAndroidTest {
  void SetUp() override {
    // Override the GMS version to be big enough for local UPM support, so these
    // tests still pass in bots with an outdated version.
    base::android::BuildInfo::GetInstance()->set_gms_version_code_for_test(
        base::NumberToString(password_manager::GetLocalUpmMinGmsVersion()));

    ChromePasswordManagerClientAndroidTest::SetUp();

    AccountPasswordStoreFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(),
        base::BindRepeating(
            &password_manager::BuildPasswordStoreInterface<
                content::BrowserContext, MockPasswordStoreInterface>));
  }
};

TEST_F(ChromePasswordManagerClientWithAccountStoreAndroidTest,
       MarkSharedCredentialsAsNotified) {
  GURL kURL = GURL("https://example.com");
  auto origin = url::Origin::Create(kURL);
  auto not_shared = MakePasswordForm();
  not_shared.username_value = u"not_shared";

  auto shared_and_notified = MakePasswordForm();
  shared_and_notified.username_value = u"shared_and_notified";
  shared_and_notified.type = PasswordForm::Type::kReceivedViaSharing;
  shared_and_notified.sharing_notification_displayed = true;

  auto shared_not_notified_profile = MakePasswordForm();
  shared_not_notified_profile.username_value = u"shared_not_notified_profile";
  shared_not_notified_profile.type = PasswordForm::Type::kReceivedViaSharing;
  shared_not_notified_profile.sharing_notification_displayed = false;
  shared_not_notified_profile.in_store = PasswordForm::Store::kProfileStore;

  auto shared_not_notified_account = MakePasswordForm();
  shared_not_notified_account.username_value = u"shared_not_notified_account";
  shared_not_notified_account.type = PasswordForm::Type::kReceivedViaSharing;
  shared_not_notified_account.sharing_notification_displayed = false;
  shared_not_notified_account.in_store = PasswordForm::Store::kAccountStore;

  std::vector<PasswordForm> forms = {not_shared, shared_and_notified,
                                     shared_not_notified_profile,
                                     shared_not_notified_account};
  GetClient()
      ->GetCredentialCacheForTesting()
      ->SaveCredentialsAndBlocklistedForOrigin(
          forms, CredentialCache::IsOriginBlocklisted(false), origin);

  MockPasswordStoreInterface* profile_store =
      static_cast<MockPasswordStoreInterface*>(
          GetClient()->GetProfilePasswordStore());

  MockPasswordStoreInterface* account_store =
      static_cast<MockPasswordStoreInterface*>(
          GetClient()->GetAccountPasswordStore());

  shared_not_notified_profile.sharing_notification_displayed = true;
  shared_not_notified_account.sharing_notification_displayed = true;
  EXPECT_CALL(*profile_store, UpdateLogin(shared_not_notified_profile, _));
  EXPECT_CALL(*account_store, UpdateLogin(shared_not_notified_account, _));
  GetClient()->MarkSharedCredentialsAsNotified(kURL);
}

#endif  //  BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)

class MockPasswordCrossDomainConfirmationPopupController
    : public password_manager::PasswordCrossDomainConfirmationPopupController {
 public:
  MOCK_METHOD(void, Hide, (autofill::SuggestionHidingReason), (override));
  MOCK_METHOD(void,
              Show,
              (const gfx::RectF&,
               base::i18n::TextDirection,
               const GURL&,
               const std::u16string&,
               base::OnceClosure),
              (override));
};

TEST_F(ChromePasswordManagerClientTest, ShowCrossDomainConfirmationPopup) {
// This simple method of the web contents repositioning doesn't work on Mac.
// The screen coordinates calculation testing is skipped on this platform, but
// the logic is completely platform independent and is being tested on others.
#if !BUILDFLAG(IS_MAC)
  web_contents()->GetNativeView()->SetBounds(gfx::Rect(100, 100, 1000, 1000));
#endif  // !BUILDFLAG(IS_MAC)

  base::MockRepeatingCallback<std::unique_ptr<
      password_manager::PasswordCrossDomainConfirmationPopupController>()>
      popup_factory;
  EXPECT_CALL(popup_factory, Run).WillOnce([&]() {
    auto mock_controller =
        std::make_unique<MockPasswordCrossDomainConfirmationPopupController>();
    EXPECT_CALL(
        *mock_controller,
        Show(gfx::RectF(
                 gfx::PointF(web_contents()->GetContainerBounds().origin()),
                 gfx::SizeF(100, 100)),
             base::i18n::TextDirection::LEFT_TO_RIGHT,
             GURL("https://google.com"), std::u16string(u"google.de"), _));
    return mock_controller;
  });
  GetClient()->set_cross_domain_confirmation_popup_factory_for_testing(
      popup_factory.Get());

  GetClient()->ShowCrossDomainConfirmationPopup(
      gfx::RectF(100, 100), base::i18n::TextDirection::LEFT_TO_RIGHT,
      GURL("https://google.com"), u"google.de", base::DoNothing());
}

#endif  //  !BUILDFLAG(IS_ANDROID)

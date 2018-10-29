// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_manager_client.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/common/autofill_agent.mojom.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/content/browser/password_manager_internals_service_factory.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/log_receiver.h"
#include "components/password_manager/core/browser/log_router.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_internals_service.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sessions/content/content_record_password_state.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

#if defined(SAFE_BROWSING_DB_LOCAL)
#include "components/safe_browsing/password_protection/mock_password_protection_service.h"
#endif

using autofill::PasswordForm;
using browser_sync::ProfileSyncServiceMock;
using content::BrowserContext;
using content::WebContents;
using password_manager::PasswordManagerClient;
using sessions::GetPasswordStateFromNavigation;
using sessions::SerializedNavigationEntry;
using testing::Return;
using testing::_;

namespace {
// TODO(vabr): Get rid of the mocked client in the client's own test, see
// http://crbug.com/474577.
class MockChromePasswordManagerClient : public ChromePasswordManagerClient {
 public:
  MOCK_CONST_METHOD0(GetMainFrameCertStatus, net::CertStatus());

  explicit MockChromePasswordManagerClient(content::WebContents* web_contents)
      : ChromePasswordManagerClient(web_contents, nullptr) {
    ON_CALL(*this, GetMainFrameCertStatus()).WillByDefault(testing::Return(0));
#if defined(SAFE_BROWSING_DB_LOCAL)
    password_protection_service_ =
        std::make_unique<safe_browsing::MockPasswordProtectionService>();
#endif
  }
  ~MockChromePasswordManagerClient() override {}

#if defined(SAFE_BROWSING_DB_LOCAL)
  safe_browsing::PasswordProtectionService* GetPasswordProtectionService()
      const override {
    return password_protection_service_.get();
  }

  safe_browsing::MockPasswordProtectionService* password_protection_service() {
    return password_protection_service_.get();
  }
#endif

 private:
#if defined(SAFE_BROWSING_DB_LOCAL)
  std::unique_ptr<safe_browsing::MockPasswordProtectionService>
      password_protection_service_;
#endif
  DISALLOW_COPY_AND_ASSIGN(MockChromePasswordManagerClient);
};

class DummyLogReceiver : public password_manager::LogReceiver {
 public:
  DummyLogReceiver() = default;

  void LogSavePasswordProgress(const std::string& text) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyLogReceiver);
};

class FakePasswordAutofillAgent
    : public autofill::mojom::PasswordAutofillAgent {
 public:
  FakePasswordAutofillAgent()
      : called_set_logging_state_(false),
        logging_state_active_(false),
        binding_(this) {}

  ~FakePasswordAutofillAgent() override {}

  void BindRequest(mojo::ScopedInterfaceEndpointHandle handle) {
    binding_.Bind(autofill::mojom::PasswordAutofillAgentAssociatedRequest(
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
  void FillPasswordForm(
      const autofill::PasswordFormFillData& form_data) override {}

  void FillIntoFocusedField(bool is_password,
                            const base::string16& credential,
                            FillIntoFocusedFieldCallback callback) override {}

  void SetLoggingState(bool active) override {
    called_set_logging_state_ = true;
    logging_state_active_ = active;
  }

  void AutofillUsernameAndPasswordDataReceived(
      const autofill::FormsPredictionsMap& predictions) override {}

  // Records whether SetLoggingState() gets called.
  bool called_set_logging_state_;
  // Records data received via SetLoggingState() call.
  bool logging_state_active_;

  mojo::AssociatedBinding<autofill::mojom::PasswordAutofillAgent> binding_;
};

}  // namespace

class ChromePasswordManagerClientTest : public ChromeRenderViewHostTestHarness {
 public:
  ChromePasswordManagerClientTest()
      : field_trial_list_(nullptr), metrics_enabled_(false) {}
  void SetUp() override;
  void TearDown() override;

  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile()->GetTestingPrefService();
  }

  // Caller does not own the returned pointer.
  ProfileSyncServiceMock* SetupBasicMockSync() {
    ProfileSyncServiceMock* mock_sync_service =
        static_cast<ProfileSyncServiceMock*>(
            ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
                profile(), base::BindRepeating(&BuildMockProfileSyncService)));

    EXPECT_CALL(*mock_sync_service, IsFirstSetupComplete())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_sync_service, GetTransportState())
        .WillRepeatedly(Return(syncer::SyncService::TransportState::ACTIVE));
    return mock_sync_service;
  }

  // Make a navigation entry that will accept an annotation.
  void SetupNavigationForAnnotation() {
    ProfileSyncServiceMock* mock_sync_service = SetupBasicMockSync();
    EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
        .WillRepeatedly(Return(false));
    metrics_enabled_ = true;
    NavigateAndCommit(GURL("about:blank"));
  }

 protected:
  ChromePasswordManagerClient* GetClient();

  // If autofill::mojom::PasswordAutofillAgent::SetLoggingState() got called,
  // copies its argument into |activation_flag| and returns true. Otherwise
  // returns false.
  bool WasLoggingActivationMessageSent(bool* activation_flag);

  FakePasswordAutofillAgent fake_agent_;

  TestingPrefServiceSimple prefs_;
  base::FieldTrialList field_trial_list_;
  bool metrics_enabled_;
};

void ChromePasswordManagerClientTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  blink::AssociatedInterfaceProvider* remote_interfaces =
      web_contents()->GetMainFrame()->GetRemoteAssociatedInterfaces();
  remote_interfaces->OverrideBinderForTesting(
      autofill::mojom::PasswordAutofillAgent::Name_,
      base::BindRepeating(&FakePasswordAutofillAgent::BindRequest,
                          base::Unretained(&fake_agent_)));

  prefs_.registry()->RegisterBooleanPref(
      password_manager::prefs::kCredentialsEnableService, true);
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents(), nullptr);

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

TEST_F(ChromePasswordManagerClientTest, LogSavePasswordProgressNotifyRenderer) {
  bool logging_active = true;
  // Ensure the existence of a driver, which will send the IPCs we listen for
  // below.
  NavigateAndCommit(GURL("about:blank"));

  // Initially, the logging should be off, so no IPC messages.
  EXPECT_TRUE(!WasLoggingActivationMessageSent(&logging_active) ||
              !logging_active)
      << "logging_active=" << logging_active;

  DummyLogReceiver log_receiver;
  password_manager::LogRouter* log_router = password_manager::
      PasswordManagerInternalsServiceFactory::GetForBrowserContext(profile());
  EXPECT_EQ(std::string(), log_router->RegisterReceiver(&log_receiver));
  EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_active));
  EXPECT_TRUE(logging_active);

  log_router->UnregisterReceiver(&log_receiver);
  EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_active));
  EXPECT_FALSE(logging_active);
}

TEST_F(ChromePasswordManagerClientTest, GetPasswordSyncState) {
  ProfileSyncServiceMock* mock_sync_service = SetupBasicMockSync();

  syncer::ModelTypeSet active_types;
  active_types.Put(syncer::PASSWORDS);
  EXPECT_CALL(*mock_sync_service, GetActiveDataTypes())
      .WillRepeatedly(Return(active_types));
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));

  ChromePasswordManagerClient* client = GetClient();

  // Passwords are syncing and custom passphrase isn't used.
  EXPECT_EQ(password_manager::SYNCING_NORMAL_ENCRYPTION,
            client->GetPasswordSyncState());

  // Again, using a custom passphrase.
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(true));

  EXPECT_EQ(password_manager::SYNCING_WITH_CUSTOM_PASSPHRASE,
            client->GetPasswordSyncState());

  // Report correctly if we aren't syncing passwords.
  active_types.Remove(syncer::PASSWORDS);
  active_types.Put(syncer::BOOKMARKS);
  EXPECT_CALL(*mock_sync_service, GetActiveDataTypes())
      .WillRepeatedly(Return(active_types));

  EXPECT_EQ(password_manager::NOT_SYNCING, client->GetPasswordSyncState());

  // Again, without a custom passphrase.
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));

  EXPECT_EQ(password_manager::NOT_SYNCING, client->GetPasswordSyncState());
}

TEST_F(ChromePasswordManagerClientTest, IsIncognitoTest) {
  ChromePasswordManagerClient* client = GetClient();

  profile()->ForceIncognito(true);
  EXPECT_TRUE(client->IsIncognito());

  profile()->ForceIncognito(false);
  EXPECT_FALSE(client->IsIncognito());
}

TEST_F(ChromePasswordManagerClientTest,
       SavingDependsOnManagerEnabledPreference) {
  // Test that saving passwords depends on the password manager enabled
  // preference.
  ChromePasswordManagerClient* client = GetClient();
  prefs()->SetUserPref(password_manager::prefs::kCredentialsEnableService,
                       std::make_unique<base::Value>(true));
  EXPECT_TRUE(client->IsSavingAndFillingEnabledForCurrentPage());
  prefs()->SetUserPref(password_manager::prefs::kCredentialsEnableService,
                       std::make_unique<base::Value>(false));
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
}

TEST_F(ChromePasswordManagerClientTest, SavingAndFillingEnabledConditionsTest) {
  std::unique_ptr<WebContents> test_web_contents(
      content::WebContentsTester::CreateTestWebContents(
          web_contents()->GetBrowserContext(), nullptr));
  std::unique_ptr<MockChromePasswordManagerClient> client(
      new MockChromePasswordManagerClient(test_web_contents.get()));
  // Functionality disabled if there is an SSL error.
  EXPECT_CALL(*client, GetMainFrameCertStatus())
      .WillRepeatedly(Return(net::CERT_STATUS_AUTHORITY_INVALID));
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_FALSE(client->IsFillingEnabledForCurrentPage());
  EXPECT_FALSE(client->IsFillingFallbackEnabledForCurrentPage());

  // Functionality disabled if there are SSL errors and the manager itself is
  // disabled.
  prefs()->SetUserPref(password_manager::prefs::kCredentialsEnableService,
                       std::make_unique<base::Value>(false));
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_FALSE(client->IsFillingEnabledForCurrentPage());
  EXPECT_FALSE(client->IsFillingFallbackEnabledForCurrentPage());

  // Saving disabled if there are no SSL errors, but the manager itself is
  // disabled.
  EXPECT_CALL(*client, GetMainFrameCertStatus()).WillRepeatedly(Return(0));
  prefs()->SetUserPref(password_manager::prefs::kCredentialsEnableService,
                       std::make_unique<base::Value>(false));
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_TRUE(client->IsFillingEnabledForCurrentPage());
  EXPECT_TRUE(client->IsFillingFallbackEnabledForCurrentPage());

  // Functionality enabled if there are no SSL errors and the manager is
  // enabled.
  EXPECT_CALL(*client, GetMainFrameCertStatus()).WillRepeatedly(Return(0));
  prefs()->SetUserPref(password_manager::prefs::kCredentialsEnableService,
                       std::make_unique<base::Value>(true));
  EXPECT_TRUE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_TRUE(client->IsFillingEnabledForCurrentPage());
  EXPECT_TRUE(client->IsFillingFallbackEnabledForCurrentPage());

  // Saving disabled in Incognito mode.
  profile()->ForceIncognito(true);
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_TRUE(client->IsFillingEnabledForCurrentPage());
  EXPECT_TRUE(client->IsFillingFallbackEnabledForCurrentPage());

  // Saving disabled in Incognito mode also when manager itself is
  // enabled.
  prefs()->SetUserPref(password_manager::prefs::kCredentialsEnableService,
                       std::make_unique<base::Value>(true));
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_TRUE(client->IsFillingEnabledForCurrentPage());
  EXPECT_TRUE(client->IsFillingFallbackEnabledForCurrentPage());

  // In guest mode saving is disabled, filling is enabled but there is in fact
  // nothing to fill, manual filling is disabled.
  profile()->SetGuestSession(true);
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_TRUE(client->IsFillingEnabledForCurrentPage());
  EXPECT_FALSE(client->IsFillingFallbackEnabledForCurrentPage());
}

TEST_F(ChromePasswordManagerClientTest, SavingDependsOnAutomation) {
  // Test that saving passwords UI is disabled for automated tests.
  ChromePasswordManagerClient* client = GetClient();
  EXPECT_TRUE(client->IsSavingAndFillingEnabledForCurrentPage());
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAutomation);
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
}

// Check that password manager is disabled on about:blank pages.
// See https://crbug.com/756587.
TEST_F(ChromePasswordManagerClientTest, SavingAndFillingDisabledForAboutBlank) {
  const GURL kUrl(url::kAboutBlankURL);
  NavigateAndCommit(kUrl);
  EXPECT_EQ(kUrl, GetClient()->GetLastCommittedEntryURL());
  EXPECT_FALSE(GetClient()->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_FALSE(GetClient()->IsFillingEnabledForCurrentPage());
  EXPECT_FALSE(GetClient()->IsFillingFallbackEnabledForCurrentPage());
}

namespace {

struct SchemeTestCase {
  const char* scheme;
  bool password_manager_works;
};
const SchemeTestCase kTestCases[] = {
    {url::kHttpScheme, true},
    {url::kHttpsScheme, true},
    {url::kFtpScheme, true},
    {url::kDataScheme, true},
    {"feed", true},

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
  static std::vector<const char*> GetSchemes() {
    std::vector<const char*> result;
    for (const SchemeTestCase& test_case : kTestCases) {
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
  EXPECT_EQ(url, GetClient()->GetLastCommittedEntryURL());

  auto* it = std::find_if(std::begin(kTestCases), std::end(kTestCases),
                          [this](auto test_case) {
                            return strcmp(test_case.scheme, GetParam()) == 0;
                          });
  ASSERT_FALSE(it == std::end(kTestCases));
  EXPECT_EQ(it->password_manager_works,
            GetClient()->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_EQ(it->password_manager_works,
            GetClient()->IsFillingEnabledForCurrentPage());
  EXPECT_EQ(it->password_manager_works,
            GetClient()->IsFillingFallbackEnabledForCurrentPage());
}

INSTANTIATE_TEST_CASE_P(
    ,
    ChromePasswordManagerClientSchemeTest,
    ::testing::ValuesIn(ChromePasswordManagerClientSchemeTest::GetSchemes()));

}  // namespace

// Verify the filling check behaves accordingly to the passed type of navigation
// entry to check.
TEST_F(ChromePasswordManagerClientTest,
       IsFillingEnabledForCurrentPage_NavigationEntry) {
  // PasswordStore is needed for processing forms in PasswordManager later in
  // the test.
  PasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(),
      base::BindRepeating(
          &password_manager::BuildPasswordStore<
              content::BrowserContext,
              testing::NiceMock<password_manager::MockPasswordStore>>));

  // about:blank is one of the pages where password manager should not work.
  const GURL kUrlOff(url::kAboutBlankURL);
  // accounts.google.com is one of the pages where password manager should work.
  const GURL kUrlOn("https://accounts.google.com");

  // Ensure that the committed entry is one where password manager should work.
  NavigateAndCommit(kUrlOn);
  // Start a navigation to where password manager should not work, but do not
  // commit the navigation. The target URL should be associated with the
  // visible entry.
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateBrowserInitiated(kUrlOff,
                                                           web_contents());
  navigation->Start();
  EXPECT_EQ(kUrlOn,
            web_contents()->GetController().GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(kUrlOff,
            web_contents()->GetController().GetVisibleEntry()->GetURL());

  // Let the PasswordManager see some HTML forms. Because those can only be
  // parsed after navigation is committed, the client should check the committed
  // navigation entry and decide that password manager is enabled.
  PasswordForm html_form;
  html_form.scheme = PasswordForm::SCHEME_HTML;
  html_form.origin = GURL("http://accounts.google.com/");
  html_form.signon_realm = "http://accounts.google.com/";
  // TODO(crbug.com/777861): Get rid of the upcast.
  password_manager::PasswordManager* manager =
      static_cast<PasswordManagerClient*>(GetClient())->GetPasswordManager();
  manager->OnPasswordFormsParsed(nullptr, {html_form});
  EXPECT_EQ(
      password_manager::PasswordManager::NavigationEntryToCheck::LAST_COMMITTED,
      manager->entry_to_check());
  EXPECT_TRUE(GetClient()->IsFillingEnabledForCurrentPage());

  // Let the PasswordManager see some HTTP auth forms. Those appear before the
  // navigation is committed, so the client should check the visible navigation
  // entry and decide that password manager is not enabled.
  PasswordForm http_auth_form(html_form);
  http_auth_form.scheme = PasswordForm::SCHEME_BASIC;
  manager->OnPasswordFormsParsed(nullptr, {http_auth_form});
  EXPECT_EQ(password_manager::PasswordManager::NavigationEntryToCheck::VISIBLE,
            manager->entry_to_check());
  EXPECT_FALSE(GetClient()->IsFillingEnabledForCurrentPage());
}

TEST_F(ChromePasswordManagerClientTest, GetLastCommittedEntryURL_Empty) {
  EXPECT_EQ(GURL::EmptyGURL(), GetClient()->GetLastCommittedEntryURL());
}

TEST_F(ChromePasswordManagerClientTest, GetLastCommittedEntryURL) {
  GURL kUrl(
      "https://accounts.google.com/ServiceLogin?continue="
      "https://passwords.google.com/settings");
  NavigateAndCommit(kUrl);
  EXPECT_EQ(kUrl, GetClient()->GetLastCommittedEntryURL());
}

TEST_F(ChromePasswordManagerClientTest, WebUINoLogging) {
  // Make sure that logging is active.
  password_manager::LogRouter* log_router = password_manager::
      PasswordManagerInternalsServiceFactory::GetForBrowserContext(profile());
  DummyLogReceiver log_receiver;
  EXPECT_EQ(std::string(), log_router->RegisterReceiver(&log_receiver));

  // But then navigate to a WebUI, there the logging should not be active.
  NavigateAndCommit(GURL("about:password-manager-internals"));
  EXPECT_FALSE(GetClient()->GetLogManager()->IsLoggingActive());

  log_router->UnregisterReceiver(&log_receiver);
}

// Metrics enabled, syncing with non-custom passphrase: Do not annotate.
TEST_F(ChromePasswordManagerClientTest,
       AnnotateNavigationEntryWithMetricsNoCustom) {
  ProfileSyncServiceMock* mock_sync_service = SetupBasicMockSync();
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  metrics_enabled_ = true;

  NavigateAndCommit(GURL("about:blank"));
  GetClient()->AnnotateNavigationEntry(true);

  EXPECT_EQ(
      SerializedNavigationEntry::HAS_PASSWORD_FIELD,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// Metrics disabled, syncing with non-custom passphrase: Do not annotate.
TEST_F(ChromePasswordManagerClientTest,
       AnnotateNavigationEntryNoMetricsNoCustom) {
  ProfileSyncServiceMock* mock_sync_service = SetupBasicMockSync();
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  metrics_enabled_ = false;

  NavigateAndCommit(GURL("about:blank"));
  GetClient()->AnnotateNavigationEntry(true);

  EXPECT_EQ(
      SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// Metrics enabled, syncing with custom passphrase: Do not annotate.
TEST_F(ChromePasswordManagerClientTest,
       AnnotateNavigationEntryWithMetricsWithCustom) {
  ProfileSyncServiceMock* mock_sync_service = SetupBasicMockSync();
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(true));
  metrics_enabled_ = true;

  NavigateAndCommit(GURL("about:blank"));
  GetClient()->AnnotateNavigationEntry(true);

  EXPECT_EQ(
      SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// Metrics disabled, syncing with custom passphrase: Do not annotate.
TEST_F(ChromePasswordManagerClientTest,
       AnnotateNavigationEntryNoMetricsWithCustom) {
  ProfileSyncServiceMock* mock_sync_service = SetupBasicMockSync();
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(true));
  metrics_enabled_ = false;

  NavigateAndCommit(GURL("about:blank"));
  GetClient()->AnnotateNavigationEntry(true);

  EXPECT_EQ(
      SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// State transition: Unannotated
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryUnannotated) {
  SetupNavigationForAnnotation();

  EXPECT_EQ(
      SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// State transition: unknown->false
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryToFalse) {
  SetupNavigationForAnnotation();

  GetClient()->AnnotateNavigationEntry(false);
  EXPECT_EQ(
      SerializedNavigationEntry::NO_PASSWORD_FIELD,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// State transition: false->true
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryToTrue) {
  SetupNavigationForAnnotation();

  GetClient()->AnnotateNavigationEntry(false);
  GetClient()->AnnotateNavigationEntry(true);
  EXPECT_EQ(
      SerializedNavigationEntry::HAS_PASSWORD_FIELD,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// State transition: true->false (retains true)
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryTrueToFalse) {
  SetupNavigationForAnnotation();

  GetClient()->AnnotateNavigationEntry(true);
  GetClient()->AnnotateNavigationEntry(false);
  EXPECT_EQ(
      SerializedNavigationEntry::HAS_PASSWORD_FIELD,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
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
      blink::mojom::CredentialManagerRequest(), web_contents->GetMainFrame());
}

TEST_F(ChromePasswordManagerClientTest, CanShowBubbleOnURL) {
  struct TestCase {
    const char* scheme;
    bool can_show_bubble;
  } kTestCases[] = {
      {url::kHttpScheme, true},
      {url::kHttpsScheme, true},
      {url::kFtpScheme, true},
      {url::kDataScheme, true},
      {"feed", true},
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

#if defined(SAFE_BROWSING_DB_LOCAL)
TEST_F(ChromePasswordManagerClientTest,
       VerifyMaybeStartPasswordFieldOnFocusRequestCalled) {
  std::unique_ptr<WebContents> test_web_contents(
      content::WebContentsTester::CreateTestWebContents(
          web_contents()->GetBrowserContext(), nullptr));
  std::unique_ptr<MockChromePasswordManagerClient> client(
      new MockChromePasswordManagerClient(test_web_contents.get()));
  EXPECT_CALL(*client->password_protection_service(),
              MaybeStartPasswordFieldOnFocusRequest(_, _, _, _))
      .Times(1);
  PasswordManagerClient* mojom_client = client.get();
  mojom_client->CheckSafeBrowsingReputation(GURL("http://foo.com/submit"),
                                            GURL("http://foo.com/iframe.html"));
}

TEST_F(ChromePasswordManagerClientTest,
       VerifyMaybeProtectedPasswordEntryRequestCalled) {
  std::unique_ptr<WebContents> test_web_contents(
      content::WebContentsTester::CreateTestWebContents(
          web_contents()->GetBrowserContext(), nullptr));
  std::unique_ptr<MockChromePasswordManagerClient> client(
      new MockChromePasswordManagerClient(test_web_contents.get()));

  EXPECT_CALL(*client->password_protection_service(),
              MaybeStartProtectedPasswordEntryRequest(_, _, _, _, true))
      .Times(4);
  client->CheckProtectedPasswordEntry(
      password_manager::metrics_util::PasswordType::SAVED_PASSWORD,
      std::vector<std::string>({"saved_domain.com"}), true);
  client->CheckProtectedPasswordEntry(
      password_manager::metrics_util::PasswordType::SYNC_PASSWORD,
      std::vector<std::string>({"saved_domain.com"}), true);
  client->CheckProtectedPasswordEntry(
      password_manager::metrics_util::PasswordType::OTHER_GAIA_PASSWORD,
      std::vector<std::string>({"saved_domain.com"}), true);
  client->CheckProtectedPasswordEntry(
      password_manager::metrics_util::PasswordType::ENTERPRISE_PASSWORD,
      std::vector<std::string>({"saved_domain.com"}), true);
}

TEST_F(ChromePasswordManagerClientTest, VerifyLogPasswordReuseDetectedEvent) {
  std::unique_ptr<WebContents> test_web_contents(
      content::WebContentsTester::CreateTestWebContents(
          web_contents()->GetBrowserContext(), nullptr));
  std::unique_ptr<MockChromePasswordManagerClient> client(
      new MockChromePasswordManagerClient(test_web_contents.get()));
  EXPECT_CALL(*client->password_protection_service(),
              MaybeLogPasswordReuseDetectedEvent(test_web_contents.get()))
      .Times(1);
  client->LogPasswordReuseDetectedEvent();
}
#endif

TEST_F(ChromePasswordManagerClientTest, MissingUIDelegate) {
  // Checks that the saving fallback methods don't crash if there is no UI
  // delegate. It can happen on ChromeOS login form, for example.
  GURL kUrl("https://example.com/");
  NavigateAndCommit(kUrl);
  std::unique_ptr<password_manager::PasswordFormManager> form_manager;
  PasswordManagerClient* client = GetClient();
  client->ShowManualFallbackForSaving(std::move(form_manager), false, false);
  client->HideManualFallbackForSaving();
}

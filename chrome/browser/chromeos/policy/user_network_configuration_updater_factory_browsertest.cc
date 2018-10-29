// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/screens/gaia_view.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/policy/login_policy_test_base.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/chromeos_test_utils.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/onc/onc_certificate_importer.h"
#include "chromeos/network/onc/onc_certificate_importer_impl.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "chromeos/policy_certificate_provider.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "crypto/scoped_test_nss_db.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Test data file storing an ONC blob with an Authority certificate.
constexpr char kRootCertOnc[] = "root-ca-cert.onc";
constexpr char kClientCertOnc[] = "certificate-client.onc";
constexpr char kClientCertSubjectCommonName[] = "lmao";
constexpr char kNetworkComponentDirectory[] = "network";
// A PEM-encoded certificate which was signed by the Authority specified in
// |kRootCertOnc|.
constexpr char kGoodCert[] = "ok_cert.pem";
// The PEM-encoded Authority certificate specified by |kRootCertOnc|.
constexpr char kRootCert[] = "root_ca_cert.pem";
constexpr char kDeviceLocalAccountId[] = "dla1@example.com";

// Allows waiting until the list of policy-pushed web-trusted certificates
// changes.
class WebTrustedCertsChangedObserver
    : public chromeos::PolicyCertificateProvider::Observer {
 public:
  WebTrustedCertsChangedObserver() = default;

  // chromeos::PolicyCertificateProvider::Observer
  void OnPolicyProvidedCertsChanged(
      const net::CertificateList& all_server_and_authority_certs,
      const net::CertificateList& trust_anchors) override {
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WebTrustedCertsChangedObserver);
};

// Allows waiting until |NetworkCertLoader| updates its list of certificates.
class NetworkCertLoaderTestObserver
    : public chromeos::NetworkCertLoader::Observer {
 public:
  explicit NetworkCertLoaderTestObserver(
      chromeos::NetworkCertLoader* network_cert_loader)
      : network_cert_loader_(network_cert_loader) {
    network_cert_loader_->AddObserver(this);
  }

  ~NetworkCertLoaderTestObserver() override {
    network_cert_loader_->RemoveObserver(this);
  }

  // chromeos::NetworkCertLoader::Observer
  void OnCertificatesLoaded(
      const net::ScopedCERTCertificateList& all_certs) override {
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  chromeos::NetworkCertLoader* network_cert_loader_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(NetworkCertLoaderTestObserver);
};

// Allows waiting until the |CertDatabase| notifies its observers that it has
// changd.
class CertDatabaseChangedObserver : public net::CertDatabase::Observer {
 public:
  CertDatabaseChangedObserver() {}

  void OnCertDBChanged() override { run_loop_.Quit(); }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(CertDatabaseChangedObserver);
};

// Verifies |certificate| with |profile|'s CertVerifier and returns the result.
int VerifyTestServerCert(
    Profile* profile,
    const scoped_refptr<net::X509Certificate>& certificate) {
  mojo::ScopedAllowSyncCallForTesting allow_sync_call;
  int result = net::OK;
  content::BrowserContext::GetDefaultStoragePartition(profile)
      ->GetNetworkContext()
      ->VerifyCertificateForTesting(certificate, "127.0.0.1", std::string(),
                                    &result);
  return result;
}

bool IsSessionStarted() {
  return session_manager::SessionManager::Get()->IsSessionStarted();
}

// Returns true if |cert_handle| refers to a certificate that has a subject
// CommonName equal to |subject_common_name|.
bool HasSubjectCommonName(CERTCertificate* cert_handle,
                          const std::string& subject_common_name) {
  char* nss_text = CERT_GetCommonName(&cert_handle->subject);
  if (!nss_text)
    return false;

  const bool result = subject_common_name == nss_text;
  PORT_Free(nss_text);

  return result;
}

void IsCertInNSSDatabaseOnIOThreadWithCertDb(
    const std::string& subject_common_name,
    bool* out_system_slot_available,
    base::OnceClosure done_closure,
    net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::ScopedAllowBlockingForTesting scoped_allow_blocking_for_testing;
  net::ScopedCERTCertificateList certs = cert_db->ListCertsSync();
  for (const net::ScopedCERTCertificate& cert : certs) {
    if (HasSubjectCommonName(cert.get(), subject_common_name)) {
      *out_system_slot_available = true;
      break;
    }
  }
  std::move(done_closure).Run();
}

void IsCertInNSSDatabaseOnIOThread(content::ResourceContext* resource_context,
                                   const std::string& subject_common_name,
                                   bool* out_cert_found,
                                   base::OnceClosure done_closure) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto did_get_cert_db_callback = base::BindRepeating(
      &IsCertInNSSDatabaseOnIOThreadWithCertDb, subject_common_name,
      out_cert_found, base::AdaptCallbackForRepeating(std::move(done_closure)));

  net::NSSCertDatabase* cert_db = GetNSSCertDatabaseForResourceContext(
      resource_context, did_get_cert_db_callback);
  if (cert_db)
    did_get_cert_db_callback.Run(cert_db);
}

// Returns true if a certificate with subject CommonName |common_name| is
// present in the |NSSCertDatbase| for |profile|.
bool IsCertInNSSDatabase(Profile* profile,
                         const std::string& subject_common_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop run_loop;
  bool cert_found = false;
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(IsCertInNSSDatabaseOnIOThread,
                     profile->GetResourceContext(), subject_common_name,
                     &cert_found, run_loop.QuitClosure()));
  run_loop.Run();
  return cert_found;
}

bool IsCertInCertificateList(const net::X509Certificate* cert,
                             const net::ScopedCERTCertificateList& cert_list) {
  for (const auto& cert_list_element : cert_list) {
    if (net::x509_util::IsSameCertificate(cert_list_element.get(), cert))
      return true;
  }
  return false;
}

}  // namespace

// Base class for testing if policy-provided trust roots take effect.
class PolicyProvidedTrustAnchorsTestBase : public DevicePolicyCrosBrowserTest {
 protected:
  PolicyProvidedTrustAnchorsTestBase() {
    // Use the same testing slot as private and public slot for testing.
    test_nss_cert_db_ = std::make_unique<net::NSSCertDatabase>(
        crypto::ScopedPK11Slot(
            PK11_ReferenceSlot(test_nssdb_.slot())) /* public slot */,
        crypto::ScopedPK11Slot(
            PK11_ReferenceSlot(test_nssdb_.slot())) /* private slot */);
  }

  // InProcessBrowserTest:
  ~PolicyProvidedTrustAnchorsTestBase() override {}

  void SetUpInProcessBrowserTestFixture() override {
    // Load the certificate which is only OK if the policy-provided authority is
    // actually trusted.
    base::FilePath server_cert_pem_file_path;
    chromeos::test_utils::GetTestDataPath(kNetworkComponentDirectory, kGoodCert,
                                          &server_cert_pem_file_path);
    test_server_cert_ =
        net::ImportCertFromFile(server_cert_pem_file_path.DirName(),
                                server_cert_pem_file_path.BaseName().value());

    base::FilePath root_cert_pem_file_path;
    chromeos::test_utils::GetTestDataPath(kNetworkComponentDirectory, kRootCert,
                                          &root_cert_pem_file_path);
    test_root_cert_ =
        net::ImportCertFromFile(root_cert_pem_file_path.DirName(),
                                root_cert_pem_file_path.BaseName().value());

    // Set up the mock policy provider.
    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  }

  // Sets the ONC-policy to the blob defined by |kRootCertOnc| and waits until
  // the notification that policy-provided trust roots have changed is sent from
  // |profile|'s UserNetworkConfigurationUpdater.
  void SetRootCertONCPolicy(Profile* profile) {
    UserNetworkConfigurationUpdater* user_network_configuration_updater =
        UserNetworkConfigurationUpdaterFactory::GetForBrowserContext(profile);
    WebTrustedCertsChangedObserver trust_roots_changed_observer;
    user_network_configuration_updater->AddPolicyProvidedCertsObserver(
        &trust_roots_changed_observer);

    const std::string& user_policy_blob =
        chromeos::onc::test_utils::ReadTestData(kRootCertOnc);
    policy::PolicyMap policy;
    policy.Set(key::kOpenNetworkConfiguration, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(user_policy_blob), nullptr);
    provider_.UpdateChromePolicy(policy);
    // Note that this relies on the implementation detail that the notification
    // is sent even if the trust roots effectively remain the same.
    trust_roots_changed_observer.Wait();
    user_network_configuration_updater->RemovePolicyProvidedCertsObserver(
        &trust_roots_changed_observer);
  }

 private:
  MockConfigurationPolicyProvider provider_;

 protected:
  // Certificate which is signed by authority specified in |kRootCertOnc|.
  scoped_refptr<net::X509Certificate> test_server_cert_;
  scoped_refptr<net::X509Certificate> test_root_cert_;

  crypto::ScopedTestNSSDB test_nssdb_;
  std::unique_ptr<net::NSSCertDatabase> test_nss_cert_db_;
};

class PolicyProvidedTrustAnchorsRegularUserTest
    : public PolicyProvidedTrustAnchorsTestBase {};

IN_PROC_BROWSER_TEST_F(PolicyProvidedTrustAnchorsRegularUserTest,
                       AllowedForRegularUser) {
  SetRootCertONCPolicy(browser()->profile());
  EXPECT_EQ(net::OK,
            VerifyTestServerCert(browser()->profile(), test_server_cert_));
}

IN_PROC_BROWSER_TEST_F(PolicyProvidedTrustAnchorsRegularUserTest,
                       AuthorityAvailableThroughNetworkCertLoader) {
  // Set |NetworkCertLoader| to use a test NSS database - otherwise, it is not
  // properly initialized because |UserSessionManager| only sets the primary
  // user's NSS Database in |NetworkCertLoader| if running on ChromeOS according
  // to |base::SysInfo|.
  ASSERT_TRUE(chromeos::NetworkCertLoader::IsInitialized());
  chromeos::NetworkCertLoader::Get()->SetUserNSSDB(test_nss_cert_db_.get());

  EXPECT_FALSE(IsCertInCertificateList(
      test_root_cert_.get(), chromeos::NetworkCertLoader::Get()->all_certs()));

  NetworkCertLoaderTestObserver network_cert_loader_observer(
      chromeos::NetworkCertLoader::Get());
  SetRootCertONCPolicy(browser()->profile());
  network_cert_loader_observer.Wait();

  // Check that |NetworkCertLoader| is aware of the authority certificate.
  // (Web Trust does not matter for the NetworkCertLoader, but we currently only
  // set a policy with a certificate requesting Web Trust here).
  EXPECT_TRUE(IsCertInCertificateList(
      test_root_cert_.get(), chromeos::NetworkCertLoader::Get()->all_certs()));
}

// Base class for testing policy-provided trust roots with device-local
// accounts. Needs device policy.
class PolicyProvidedTrustAnchorsDeviceLocalAccountTest
    : public PolicyProvidedTrustAnchorsTestBase {
 protected:
  void SetUp() override {
    // Configure and start the test server.
    std::unique_ptr<crypto::RSAPrivateKey> signing_key(
        PolicyBuilder::CreateTestSigningKey());
    ASSERT_TRUE(policy_server_.SetSigningKeyAndSignature(
        signing_key.get(), PolicyBuilder::GetTestSigningKeySignature()));
    signing_key.reset();
    policy_server_.RegisterClient(PolicyBuilder::kFakeToken,
                                  PolicyBuilder::kFakeDeviceId);
    ASSERT_TRUE(policy_server_.Start());

    PolicyProvidedTrustAnchorsTestBase::SetUp();
  }

  virtual void SetupDevicePolicy() = 0;

  void SetUpInProcessBrowserTestFixture() override {
    PolicyProvidedTrustAnchorsTestBase::SetUpInProcessBrowserTestFixture();

    InstallOwnerKey();
    MarkAsEnterpriseOwned();

    device_policy()->policy_data().set_public_key_version(1);
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_show_user_names()->set_show_user_names(true);

    SetupDevicePolicy();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyProvidedTrustAnchorsTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
    command_line->AppendSwitch(chromeos::switches::kOobeSkipPostLogin);
    command_line->AppendSwitchASCII(switches::kDeviceManagementUrl,
                                    policy_server_.GetServiceURL().spec());
  }

  void WaitForSessionStart() {
    if (IsSessionStarted())
      return;
    content::WindowedNotificationObserver(chrome::NOTIFICATION_SESSION_STARTED,
                                          base::BindRepeating(IsSessionStarted))
        .Wait();
  }

  LocalPolicyTestServer policy_server_;

  const AccountId device_local_account_id_ =
      AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
          kDeviceLocalAccountId,
          DeviceLocalAccount::TYPE_PUBLIC_SESSION));
};

// Sets up device policy for public session and provides functions to sing into
// it.
class PolicyProvidedTrustAnchorsPublicSessionTest
    : public PolicyProvidedTrustAnchorsDeviceLocalAccountTest {
 protected:
  // PolicyProvidedTrustAnchorsDeviceLocalAccountTest:
  void SetupDevicePolicy() override {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    em::DeviceLocalAccountInfoProto* account =
        proto.mutable_device_local_accounts()->add_account();
    account->set_account_id(kDeviceLocalAccountId);
    account->set_type(
        em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);
    RefreshDevicePolicy();
    ASSERT_TRUE(
        policy_server_.UpdatePolicy(dm_protocol::kChromeDevicePolicyType,
                                    std::string(), proto.SerializeAsString()));
  }

  void StartLogin() {
    chromeos::WizardController::SkipPostLoginScreensForTesting();
    chromeos::WizardController* const wizard_controller =
        chromeos::WizardController::default_controller();
    ASSERT_TRUE(wizard_controller);
    wizard_controller->SkipToLoginForTesting(chromeos::LoginScreenContext());

    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources())
        .Wait();

    // Login into the public session.
    chromeos::ExistingUserController* controller =
        chromeos::ExistingUserController::current_controller();
    ASSERT_TRUE(controller);
    chromeos::UserContext user_context(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                                       device_local_account_id_);
    controller->Login(user_context, chromeos::SigninSpecifics());
  }
};

// TODO(https://crbug.com/874831): Re-enable this after the source of the
// flakiness has been identified.
IN_PROC_BROWSER_TEST_F(PolicyProvidedTrustAnchorsPublicSessionTest,
                       DISABLED_AllowedInPublicSession) {
  StartLogin();
  WaitForSessionStart();

  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());
  Browser* browser = browser_list->get(0);
  ASSERT_TRUE(browser);

  SetRootCertONCPolicy(browser->profile());
  EXPECT_EQ(net::OK,
            VerifyTestServerCert(browser->profile(), test_server_cert_));
}

class PolicyProvidedTrustAnchorsOnUserSessionInitTest
    : public LoginPolicyTestBase,
      content::NotificationObserver {
 protected:
  PolicyProvidedTrustAnchorsOnUserSessionInitTest() {}

  void SetUpOnMainThread() override {
    LoginPolicyTestBase::SetUpOnMainThread();

    session_started_notification_registrar_ =
        std::make_unique<content::NotificationRegistrar>();
    session_started_notification_registrar_->Add(
        this, chrome::NOTIFICATION_SESSION_STARTED,
        content::NotificationService::AllSources());
  }

  void TearDownOnMainThread() override {
    session_started_notification_registrar_.reset();

    LoginPolicyTestBase::TearDownOnMainThread();
  }

  void GetMandatoryPoliciesValue(base::DictionaryValue* policy) const override {
    const std::string& user_policy_blob =
        chromeos::onc::test_utils::ReadTestData(kRootCertOnc);
    policy->SetKey(key::kOpenNetworkConfiguration,
                   base::Value(user_policy_blob));
  }

  bool user_session_started() { return user_session_started_; }

  void WaitSessionStart() {
    if (user_session_started())
      return;

    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_SESSION_STARTED,
        content::NotificationService::AllSources())
        .Wait();
  }

  void TriggerLogIn() {
    chromeos::LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetGaiaScreenView()
        ->ShowSigninScreenForTest(kAccountId, kAccountPassword, kEmptyServices);
  }

  Profile* active_user_profile() {
    const user_manager::User* const user =
        user_manager::UserManager::Get()->GetActiveUser();
    Profile* const profile =
        chromeos::ProfileHelper::Get()->GetProfileByUser(user);
    return profile;
  }

 private:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    EXPECT_EQ(chrome::NOTIFICATION_SESSION_STARTED, type);
    user_session_started_ = true;
  }

  bool user_session_started_ = false;

  std::unique_ptr<content::NotificationRegistrar>
      session_started_notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(PolicyProvidedTrustAnchorsOnUserSessionInitTest);
};

// Verifies that the policy-provided trust root is active as soon as the user
// session starts.
IN_PROC_BROWSER_TEST_F(PolicyProvidedTrustAnchorsOnUserSessionInitTest,
                       TrustAnchorsAvailableImmediatelyAfterSessionStart) {
  // Load the certificate which is only OK if the policy-provided authority is
  // actually trusted.
  base::FilePath cert_pem_file_path;
  chromeos::test_utils::GetTestDataPath(kNetworkComponentDirectory, kGoodCert,
                                        &cert_pem_file_path);
  scoped_refptr<net::X509Certificate> test_server_cert =
      net::ImportCertFromFile(cert_pem_file_path.DirName(),
                              cert_pem_file_path.BaseName().value());

  SkipToLoginScreen();
  TriggerLogIn();

  EXPECT_FALSE(user_session_started());

  WaitSessionStart();
  EXPECT_EQ(net::OK,
            VerifyTestServerCert(active_user_profile(), test_server_cert));
}

// Testing policy-provided client cert import.
class PolicyProvidedClientCertsTest : public DevicePolicyCrosBrowserTest {
 protected:
  PolicyProvidedClientCertsTest() {}
  ~PolicyProvidedClientCertsTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    // Set up the mock policy provider.
    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  // Sets the ONC-policy to the blob defined by |kClientCertOnc|. Then waits
  // until |CertDatabase| notifies its observers that the database has changed.
  void SetClientCertONCPolicy(Profile* profile) {
    net::CertDatabase* cert_database = net::CertDatabase::GetInstance();
    CertDatabaseChangedObserver cert_database_changed_observer;
    cert_database->AddObserver(&cert_database_changed_observer);

    const std::string& user_policy_blob =
        chromeos::onc::test_utils::ReadTestData(kClientCertOnc);
    policy::PolicyMap policy;
    policy.Set(key::kOpenNetworkConfiguration, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(user_policy_blob), nullptr);
    provider_.UpdateChromePolicy(policy);

    cert_database_changed_observer.Wait();
    cert_database->RemoveObserver(&cert_database_changed_observer);
  }

 private:
  MockConfigurationPolicyProvider provider_;
};

IN_PROC_BROWSER_TEST_F(PolicyProvidedClientCertsTest, ClientCertsImported) {
  // Sanity check: we don't expect the client certificate to be present before
  // setting the user ONC policy.
  EXPECT_FALSE(
      IsCertInNSSDatabase(browser()->profile(), kClientCertSubjectCommonName));

  SetClientCertONCPolicy(browser()->profile());
  EXPECT_TRUE(
      IsCertInNSSDatabase(browser()->profile(), kClientCertSubjectCommonName));
}

}  // namespace policy

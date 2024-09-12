// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/login_or_lock_screen_visible_waiter.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/login/login_policy_test_base.h"
#include "chrome/browser/ash/policy/login/signin_profile_extensions_policy_test_base.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater_ash.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater_factory.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/ash/components/network/onc/onc_certificate_importer.h"
#include "chromeos/ash/components/network/onc/onc_certificate_importer_impl.h"
#include "chromeos/ash/components/network/policy_certificate_provider.h"
#include "chromeos/components/onc/onc_test_utils.h"
#include "chromeos/test/chromeos_test_utils.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "crypto/scoped_test_nss_db.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

namespace em = ::enterprise_management;

// Test data file storing an ONC blob with an Authority certificate.
constexpr char kRootCaCertOnc[] = "root-ca-cert.onc";
constexpr char kClientCertOnc[] = "certificate-client.onc";
constexpr char kRootAndIntermediateCaCertsOnc[] =
    "root-and-intermediate-ca-certs.onc";
constexpr char kClientCertSubjectCommonName[] = "lmao";
// A PEM-encoded certificate which was signed by the Authority specified in
// |kRootCaCertOnc|.
constexpr char kServerCert[] = "ok_cert.pem";
// A PEM-encoded certificate which was signed by the intermediate Authority
// specified in |kRootAndIntermediateCaCertsOnc|.
constexpr char kServerCertByIntermediate[] = "ok_cert_by_intermediate.pem";
// The PEM-encoded Authority certificate specified by |kRootCaCertOnc|.
constexpr char kRootCaCert[] = "root_ca_cert.pem";
constexpr char kDeviceLocalAccountId[] = "dla1@example.com";

constexpr char kSigninScreenExtension1[] = "ngjobkbdodapjbbncmagbccommkggmnj";
constexpr char kSigninScreenExtension1UpdateManifestPath[] =
    "/extensions/signin_screen_manual_test_extension/update_manifest.xml";

const char kSigninScreenExtension2[] = "oclffehlkdgibkainkilopaalpdobkan";
const char kSigninScreenExtension2UpdateManifestPath[] =
    "/extensions/api_test/login_screen_apis/update_manifest.xml";

// Allows waiting until the list of policy-pushed web-trusted certificates
// changes.
class WebTrustedCertsChangedObserver
    : public ash::PolicyCertificateProvider::Observer {
 public:
  WebTrustedCertsChangedObserver() = default;

  WebTrustedCertsChangedObserver(const WebTrustedCertsChangedObserver&) =
      delete;
  WebTrustedCertsChangedObserver& operator=(
      const WebTrustedCertsChangedObserver&) = delete;

  // ash::PolicyCertificateProvider::Observer
  void OnPolicyProvidedCertsChanged() override { run_loop_.Quit(); }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

// Allows waiting until the |CertDatabase| notifies its observers that a client
// cert change has occurred.
class CertDatabaseChangedObserver : public net::CertDatabase::Observer {
 public:
  CertDatabaseChangedObserver() {}

  CertDatabaseChangedObserver(const CertDatabaseChangedObserver&) = delete;
  CertDatabaseChangedObserver& operator=(const CertDatabaseChangedObserver&) =
      delete;

  void OnClientCertStoreChanged() override { run_loop_.Quit(); }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

// Retrieves the path to the directory containing certificates designated for
// testing of policy-provided certificates into *|out_test_certs_path|.
base::FilePath GetTestCertsPath() {
  base::FilePath test_data_dir;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));

  base::FilePath test_certs_path =
      test_data_dir.AppendASCII("policy").AppendASCII("test_certs");
  base::ScopedAllowBlockingForTesting allow_io;
  EXPECT_TRUE(base::DirectoryExists(test_certs_path));
  return test_certs_path;
}

// Reads the contents of the file with name |file_name| in the directory
// returned by GetTestCertsPath into *|out_file_contents|.
std::string GetTestCertsFileContents(const std::string& file_name) {
  base::FilePath test_certs_path = GetTestCertsPath();

  base::ScopedAllowBlockingForTesting allow_io;
  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(test_certs_path.AppendASCII(file_name),
                                     &file_contents));
  return file_contents;
}

// Allows setting user policy to assign trust to the CA certificate specified by
// |kRootCaCert|.
class UserPolicyCertsHelper {
 public:
  UserPolicyCertsHelper() = default;
  ~UserPolicyCertsHelper() = default;

  UserPolicyCertsHelper(const UserPolicyCertsHelper& other) = delete;
  UserPolicyCertsHelper& operator=(const UserPolicyCertsHelper& other) = delete;

  // Reads in testing certificates.
  // Run in ASSERT_NO_FATAL_FAILURE.
  void Initialize() {
    base::ScopedAllowBlockingForTesting allow_io;
    base::FilePath test_certs_path = GetTestCertsPath();

    base::FilePath server_cert_path = test_certs_path.AppendASCII(kServerCert);
    server_cert_ = net::ImportCertFromFile(server_cert_path.DirName(),
                                           server_cert_path.BaseName().value());
    ASSERT_TRUE(server_cert_);

    base::FilePath root_cert_path = test_certs_path.AppendASCII(kRootCaCert);
    root_cert_ = net::ImportCertFromFile(root_cert_path.DirName(),
                                         root_cert_path.BaseName().value());
    ASSERT_TRUE(root_cert_);

    base::FilePath server_cert_by_intermediate_path =
        test_certs_path.AppendASCII(kServerCertByIntermediate);
    server_cert_by_intermediate_ = net::ImportCertFromFile(
        server_cert_path.DirName(),
        server_cert_by_intermediate_path.BaseName().value());
    ASSERT_TRUE(server_cert_by_intermediate_);
  }

  // Sets the ONC-policy to the blob defined by |kRootCaCertOnc| and waits until
  // the notification that policy-provided trust roots have changed is sent from
  // |profile|'s UserNetworkConfigurationUpdater.
  void SetRootCertONCUserPolicy(
      Profile* profile,
      MockConfigurationPolicyProvider* mock_policy_provider) {
    std::string onc_policy_data = GetTestCertsFileContents(kRootCaCertOnc);
    SetONCUserPolicy(profile, mock_policy_provider, onc_policy_data);
  }

  // Sets the ONC-policy to the blob defined by |kRootCaCertOnc| and waits until
  // the notification that policy-provided trust roots have changed is sent from
  // |profile|'s UserNetworkConfigurationUpdater.
  void SetRootAndIntermediateCertsONCUserPolicy(
      Profile* profile,
      MockConfigurationPolicyProvider* mock_policy_provider) {
    std::string onc_policy_data =
        GetTestCertsFileContents(kRootAndIntermediateCaCertsOnc);
    SetONCUserPolicy(profile, mock_policy_provider, onc_policy_data);
  }

  const scoped_refptr<net::X509Certificate>& server_cert() {
    return server_cert_;
  }

  const scoped_refptr<net::X509Certificate>& root_cert() { return root_cert_; }

  const scoped_refptr<net::X509Certificate>& server_cert_by_intermediate() {
    return server_cert_by_intermediate_;
  }

 private:
  void SetONCUserPolicy(Profile* profile,
                        MockConfigurationPolicyProvider* mock_policy_provider,
                        const std::string& onc_policy_data) {
    NetworkConfigurationUpdater* user_network_configuration_updater =
        UserNetworkConfigurationUpdaterFactory::GetForBrowserContext(profile);

    WebTrustedCertsChangedObserver trust_roots_changed_observer;
    user_network_configuration_updater->AddPolicyProvidedCertsObserver(
        &trust_roots_changed_observer);

    PolicyMap policy;
    policy.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(onc_policy_data), nullptr);
    mock_policy_provider->UpdateChromePolicy(policy);
    // Note that this relies on the implementation detail that the notification
    // is sent even if the trust roots effectively remain the same.
    trust_roots_changed_observer.Wait();
    user_network_configuration_updater->RemovePolicyProvidedCertsObserver(
        &trust_roots_changed_observer);
  }

  // Server Certificate which is signed by authority specified in |kRootCaCert|.
  scoped_refptr<net::X509Certificate> server_cert_;
  // Authority Certificate specified in |kRootCaCert|.
  scoped_refptr<net::X509Certificate> root_cert_;
  // Server Certificate which is signed by an intermediate authority, which
  // itself is signed by the authority specified in |kRootCaCert|.
  // |kRootCaCertOnc| does not know this intermediate authority.
  // |kRootCaAndIntermediateCertsOnc| does know this intermediate authority, but
  // does not request web trust for it. Instead, trust should be delegate from
  // the root authrotiy.
  scoped_refptr<net::X509Certificate> server_cert_by_intermediate_;
};

// Verifies |certificate| with |storage_partition|'s CertVerifier and returns
// the result.
int VerifyTestServerCertInStoragePartition(
    content::StoragePartition* storage_partition,
    const scoped_refptr<net::X509Certificate>& certificate) {
  base::test::TestFuture<int> future;
  storage_partition->GetNetworkContext()->VerifyCertificateForTesting(
      certificate, "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), future.GetCallback());
  return future.Get();
}

// Verifies |certificate| with the CertVerifier for |profile|'s default
// StoragePartition and returns the result.
int VerifyTestServerCert(
    Profile* profile,
    const scoped_refptr<net::X509Certificate>& certificate) {
  return VerifyTestServerCertInStoragePartition(
      profile->GetDefaultStoragePartition(), certificate);
}

// Returns true if |cert_handle| refers to a certificate that has a subject
// CommonName equal to |subject_common_name|.
bool HasSubjectCommonName(CERTCertificate* cert_handle,
                          const std::string& subject_common_name) {
  char* nss_text = CERT_GetCommonName(&cert_handle->subject);
  if (!nss_text) {
    return false;
  }

  const bool result = subject_common_name == nss_text;
  PORT_Free(nss_text);

  return result;
}

void IsCertInNSSDatabaseOnIOThreadWithCertList(
    const std::string& subject_common_name,
    bool* out_system_slot_available,
    base::OnceClosure done_closure,
    net::ScopedCERTCertificateList certs) {
  for (const net::ScopedCERTCertificate& cert : certs) {
    if (HasSubjectCommonName(cert.get(), subject_common_name)) {
      *out_system_slot_available = true;
      break;
    }
  }
  std::move(done_closure).Run();
}

void IsCertInNSSDatabaseOnIOThreadWithCertDb(
    const std::string& subject_common_name,
    bool* out_system_slot_available,
    base::OnceClosure done_closure,
    net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  cert_db->ListCerts(base::BindOnce(
      &IsCertInNSSDatabaseOnIOThreadWithCertList, subject_common_name,
      out_system_slot_available, std::move(done_closure)));
}

void IsCertInNSSDatabaseOnIOThread(NssCertDatabaseGetter database_getter,
                                   const std::string& subject_common_name,
                                   bool* out_cert_found,
                                   base::OnceClosure done_closure) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto did_get_cert_db_split_callback = base::SplitOnceCallback(base::BindOnce(
      &IsCertInNSSDatabaseOnIOThreadWithCertDb, subject_common_name,
      out_cert_found, std::move(done_closure)));

  net::NSSCertDatabase* cert_db =
      std::move(database_getter)
          .Run(std::move(did_get_cert_db_split_callback.first));
  if (cert_db) {
    std::move(did_get_cert_db_split_callback.second).Run(cert_db);
  }
}

// Returns true if a certificate with subject CommonName |common_name| is
// present in the |NSSCertDatbase| for |profile|.
bool IsCertInNSSDatabase(Profile* profile,
                         const std::string& subject_common_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop run_loop;
  bool cert_found = false;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(IsCertInNSSDatabaseOnIOThread,
                     NssServiceFactory::GetForContext(profile)
                         ->CreateNSSCertDatabaseGetterForIOThread(),
                     subject_common_name, &cert_found, run_loop.QuitClosure()));
  run_loop.Run();
  return cert_found;
}

}  // namespace

// Base class for testing policy-provided trust roots with device-local
// accounts. Needs device policy.
class PolicyProvidedCertsDeviceLocalAccountTest
    : public DevicePolicyCrosBrowserTest {
 public:
  PolicyProvidedCertsDeviceLocalAccountTest() {
    // Use the same testing slot as private and public slot for testing.
    test_nss_cert_db_ = std::make_unique<net::NSSCertDatabase>(
        crypto::ScopedPK11Slot(
            PK11_ReferenceSlot(test_nssdb_.slot())) /* public slot */,
        crypto::ScopedPK11Slot(
            PK11_ReferenceSlot(test_nssdb_.slot())) /* private slot */);
  }

 protected:
  virtual void SetupDevicePolicy() = 0;

  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    ASSERT_NO_FATAL_FAILURE(user_policy_certs_helper_.Initialize());

    // Set up the mock policy provider.
    EXPECT_CALL(user_policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(user_policy_provider_, IsFirstPolicyLoadComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    BrowserPolicyConnector::SetPolicyProviderForTesting(&user_policy_provider_);

    device_policy()->policy_data().set_public_key_version(1);
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_show_user_names()->set_show_user_names(true);

    SetupDevicePolicy();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile, "user");
    command_line->AppendSwitch(ash::switches::kOobeSkipPostLogin);
  }

  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};

  const AccountId device_local_account_id_ = AccountId::FromUserEmail(
      GenerateDeviceLocalAccountUserId(kDeviceLocalAccountId,
                                       DeviceLocalAccountType::kPublicSession));

  MockConfigurationPolicyProvider user_policy_provider_;
  UserPolicyCertsHelper user_policy_certs_helper_;

  crypto::ScopedTestNSSDB test_nssdb_;
  std::unique_ptr<net::NSSCertDatabase> test_nss_cert_db_;
};

// Sets up device policy for public session and provides functions to sing into
// it.
class PolicyProvidedCertsPublicSessionTest
    : public PolicyProvidedCertsDeviceLocalAccountTest {
 protected:
  // PolicyProvidedCertsDeviceLocalAccountTest:
  void SetupDevicePolicy() override {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    em::DeviceLocalAccountInfoProto* account =
        proto.mutable_device_local_accounts()->add_account();
    account->set_account_id(kDeviceLocalAccountId);
    account->set_type(
        em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_PUBLIC_SESSION);
    RefreshDevicePolicy();
    policy_test_server_mixin_.UpdateDevicePolicy(proto);
  }

  // TODO(crbug/874831): Consider migrating to LoggedInMixin and deprecating
  // this function.
  void StartLogin() {
    auto* const wizard_controller = ash::WizardController::default_controller();
    ASSERT_TRUE(wizard_controller);
    wizard_controller->SkipToLoginForTesting();

    ash::LoginOrLockScreenVisibleWaiter().Wait();

    // Login into the public session.
    auto* controller = ash::ExistingUserController::current_controller();
    ASSERT_TRUE(controller);
    ash::UserContext user_context(user_manager::UserType::kPublicAccount,
                                  device_local_account_id_);
    controller->Login(user_context, ash::SigninSpecifics());
  }
};

// TODO(https://crbug.com/874831): Re-enable this after the source of the
// flakiness has been identified.
IN_PROC_BROWSER_TEST_F(PolicyProvidedCertsPublicSessionTest,
                       DISABLED_AllowedInPublicSession) {
  StartLogin();
  ash::test::WaitForPrimaryUserSessionStart();

  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());
  Browser* browser = browser_list->get(0);
  ASSERT_TRUE(browser);

  user_policy_certs_helper_.SetRootCertONCUserPolicy(browser->profile(),
                                                     &user_policy_provider_);
  EXPECT_EQ(net::OK,
            VerifyTestServerCert(browser->profile(),
                                 user_policy_certs_helper_.server_cert()));
}

class PolicyProvidedCertsOnUserSessionInitTest : public LoginPolicyTestBase {
 public:
  PolicyProvidedCertsOnUserSessionInitTest(
      const PolicyProvidedCertsOnUserSessionInitTest&) = delete;
  PolicyProvidedCertsOnUserSessionInitTest& operator=(
      const PolicyProvidedCertsOnUserSessionInitTest&) = delete;

 protected:
  PolicyProvidedCertsOnUserSessionInitTest() {}

  void GetPolicySettings(em::CloudPolicySettings* policy) const override {
    std::string user_policy_blob = GetTestCertsFileContents(kRootCaCertOnc);
    policy->mutable_opennetworkconfiguration()->set_value(user_policy_blob);
  }

  Profile* active_user_profile() {
    const user_manager::User* const user =
        user_manager::UserManager::Get()->GetActiveUser();
    Profile* const profile = ash::ProfileHelper::Get()->GetProfileByUser(user);
    return profile;
  }
};

// Verifies that the policy-provided trust root is active as soon as the user
// session starts.
IN_PROC_BROWSER_TEST_F(PolicyProvidedCertsOnUserSessionInitTest,
                       TrustAnchorsAvailableImmediatelyAfterSessionStart) {
  // Load the certificate which is only OK if the policy-provided authority is
  // actually trusted.
  base::FilePath test_certs_path = GetTestCertsPath();

  base::FilePath server_cert_path = test_certs_path.AppendASCII(kServerCert);
  scoped_refptr<net::X509Certificate> server_cert = net::ImportCertFromFile(
      server_cert_path.DirName(), server_cert_path.BaseName().value());

  OobeBaseTest::WaitForSigninScreen();
  TriggerLogIn();

  ash::test::WaitForPrimaryUserSessionStart();
  EXPECT_EQ(net::OK, VerifyTestServerCert(active_user_profile(), server_cert));
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
    EXPECT_CALL(provider_, IsFirstPolicyLoadComplete(testing::_))
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
    PolicyMap policy;
    policy.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(user_policy_blob), nullptr);
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

// TODO(crbug.com/40589684): Add a test case for a kiosk session.

// Class for testing policy-provided extensions in the sign-in profile.
// Sets a device policy which applies the |kRootCaCert| for
// |kSigninScreenExtension1|. Force-installs |kSigninScreenExtension1| and
// |kSigninScreenExtension2| into the sign-in profile.
class PolicyProvidedCertsForSigninExtensionTest
    : public SigninProfileExtensionsPolicyTestBase {
 public:
  PolicyProvidedCertsForSigninExtensionTest(
      const PolicyProvidedCertsForSigninExtensionTest&) = delete;
  PolicyProvidedCertsForSigninExtensionTest& operator=(
      const PolicyProvidedCertsForSigninExtensionTest&) = delete;

 protected:
  // Use DEV channel as sign-in screen extensions are currently usable there.
  PolicyProvidedCertsForSigninExtensionTest()
      : SigninProfileExtensionsPolicyTestBase(version_info::Channel::DEV) {}
  ~PolicyProvidedCertsForSigninExtensionTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    // Apply |kRootCaCert| for |kSigninScreenExtension1| in Device ONC policy.
    base::FilePath test_certs_path = GetTestCertsPath();
    std::string x509_contents;
    {
      base::ScopedAllowBlockingForTesting allow_io;
      ASSERT_TRUE(base::ReadFileToString(
          test_certs_path.AppendASCII(kRootCaCert), &x509_contents));
    }

    base::Value::Dict onc_dict = BuildONCForExtensionScopedCertificate(
        x509_contents, kSigninScreenExtension1);
    ASSERT_TRUE(base::JSONWriter::Write(
        onc_dict, device_policy()
                      ->payload()
                      .mutable_open_network_configuration()
                      ->mutable_open_network_configuration()));

    // Load the certificate which is only OK if the policy-provided authority is
    // actually trusted.
    base::FilePath server_cert_path = test_certs_path.AppendASCII(kServerCert);
    server_cert_ = net::ImportCertFromFile(server_cert_path.DirName(),
                                           server_cert_path.BaseName().value());
    ASSERT_TRUE(server_cert_);

    SigninProfileExtensionsPolicyTestBase::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    ash::StartupUtils::MarkOobeCompleted();  // Pretend that OOBE was complete.

    SigninProfileExtensionsPolicyTestBase::SetUpOnMainThread();

    signin_profile_ = GetInitialProfile();
    ASSERT_TRUE(ash::ProfileHelper::IsSigninProfile(signin_profile_));

    extensions::ExtensionRegistry* extension_registry =
        extensions::ExtensionRegistry::Get(signin_profile_);
    extensions::TestExtensionRegistryObserver extension_1_observer(
        extension_registry, kSigninScreenExtension1);
    extensions::TestExtensionRegistryObserver extension_2_observer(
        extension_registry, kSigninScreenExtension2);

    AddExtensionForForceInstallation(kSigninScreenExtension1,
                                     kSigninScreenExtension1UpdateManifestPath);
    AddExtensionForForceInstallation(kSigninScreenExtension2,
                                     kSigninScreenExtension2UpdateManifestPath);

    extension_1_observer.WaitForExtensionLoaded();
    extension_2_observer.WaitForExtensionLoaded();
  }

  content::StoragePartition* GetStoragePartitionForSigninExtension(
      const std::string& extension_id) {
    return extensions::util::GetStoragePartitionForExtensionId(
        extension_id, signin_profile_, /*can_create=*/false);
  }

  raw_ptr<Profile, DanglingUntriaged> signin_profile_ = nullptr;
  scoped_refptr<net::X509Certificate> server_cert_;

 private:
  // Builds an ONC policy value that specifies exactly one certificate described
  // by |x509_contents| with Web trust to be used for |extension_id|.
  base::Value::Dict BuildONCForExtensionScopedCertificate(
      const std::string& x509_contents,
      const std::string& extension_id) {
    auto onc_cert_scope = base::Value::Dict()
                              .Set(onc::scope::kType, onc::scope::kExtension)
                              .Set(onc::scope::kId, extension_id);

    auto onc_cert_trust_bits =
        base::Value::List().Append(onc::certificate::kWeb);

    auto onc_certificate =
        base::Value::Dict()
            .Set(onc::certificate::kGUID, base::Value("guid"))
            .Set(onc::certificate::kType, onc::certificate::kAuthority)
            .Set(onc::certificate::kX509, x509_contents)
            .Set(onc::certificate::kScope, std::move(onc_cert_scope))
            .Set(onc::certificate::kTrustBits, std::move(onc_cert_trust_bits));

    auto onc_certificates =
        base::Value::List().Append(std::move(onc_certificate));

    auto onc_dict = base::Value::Dict()
                        .Set(onc::toplevel_config::kCertificates,
                             std::move(onc_certificates))
                        .Set(onc::toplevel_config::kType,
                             onc::toplevel_config::kUnencryptedConfiguration);

    return onc_dict;
  }
};  // namespace policy

// Verifies that a device-policy-provided, extension-scoped trust anchor is
// active only in the sign-in profile extension for which it was specified.
// Additionally verifies that it is not active
// (*) in the default StoragePartition of the sign-in profile,
// (*) in the StoragePartition used for the webview hosting GAIA and
// (*) in a different sign-in profile extension than the one for which it was
//     specified.
// Verification of all these aspects has been intentionally put into one test,
// so if the verification result leaks (e.g. due to accidentally reusing
// caches), the test is able to catch that.
IN_PROC_BROWSER_TEST_F(PolicyProvidedCertsForSigninExtensionTest,
                       ActiveOnlyInSelectedExtension) {
  ash::OobeScreenWaiter(ash::OobeBaseTest::GetFirstSigninScreen()).Wait();
  content::StoragePartition* signin_profile_default_partition =
      signin_profile_->GetDefaultStoragePartition();

  // Active in the StoragePartition of the extension for which the certificate
  // has been specified in policy.
  content::StoragePartition* extension_1_partition =
      GetStoragePartitionForSigninExtension(kSigninScreenExtension1);
  ASSERT_TRUE(extension_1_partition);
  EXPECT_NE(signin_profile_default_partition, extension_1_partition);
  EXPECT_EQ(net::OK, VerifyTestServerCertInStoragePartition(
                         extension_1_partition, server_cert_));

  // Not active in default StoragePartition.
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            VerifyTestServerCertInStoragePartition(
                signin_profile_default_partition, server_cert_));

  // Not active in the StoragePartition used for the webview hosting GAIA.
  content::StoragePartition* signin_frame_partition =
      ash::login::GetSigninPartition();
  EXPECT_NE(signin_profile_default_partition, signin_frame_partition);

  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            VerifyTestServerCertInStoragePartition(signin_frame_partition,
                                                   server_cert_));

  // Not active in the StoragePartition of another extension.
  content::StoragePartition* extension_2_partition =
      GetStoragePartitionForSigninExtension(kSigninScreenExtension2);
  ASSERT_TRUE(extension_2_partition);
  EXPECT_NE(signin_profile_default_partition, extension_2_partition);

  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            VerifyTestServerCertInStoragePartition(extension_2_partition,
                                                   server_cert_));
}

}  // namespace policy

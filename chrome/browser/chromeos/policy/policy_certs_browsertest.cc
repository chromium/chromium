// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/task/current_thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/policy/login_policy_test_base.h"
#include "chrome/browser/chromeos/policy/signin_profile_extensions_policy_test_base.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/onc/onc_certificate_importer.h"
#include "chromeos/network/onc/onc_certificate_importer_impl.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "chromeos/network/policy_certificate_provider.h"
#include "chromeos/test/chromeos_test_utils.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "crypto/scoped_test_nss_db.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/test/test_background_page_ready_observer.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_switches.h"
#endif

namespace em = enterprise_management;

namespace policy {

namespace {

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
    : public chromeos::PolicyCertificateProvider::Observer {
 public:
  WebTrustedCertsChangedObserver() = default;

  // chromeos::PolicyCertificateProvider::Observer
  void OnPolicyProvidedCertsChanged() override { run_loop_.Quit(); }

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
  void OnCertificatesLoaded() override { run_loop_.Quit(); }

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
    UserNetworkConfigurationUpdater* user_network_configuration_updater =
        UserNetworkConfigurationUpdaterFactory::GetForBrowserContext(profile);

    WebTrustedCertsChangedObserver trust_roots_changed_observer;
    user_network_configuration_updater->AddPolicyProvidedCertsObserver(
        &trust_roots_changed_observer);

    policy::PolicyMap policy;
    policy.Set(key::kOpenNetworkConfiguration, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
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

// A class that allows testing multiple profiles in a browsertest, each having
// its own MockConfigurationPolicyProvider.
// TODO(https://crbug.com/1127263): Transform this into a general-purpose mixin.
class MultiProfilePolicyProviderHelper {
 public:
  MultiProfilePolicyProviderHelper() = default;
  ~MultiProfilePolicyProviderHelper() = default;

  MultiProfilePolicyProviderHelper(
      const MultiProfilePolicyProviderHelper& other) = delete;
  MultiProfilePolicyProviderHelper& operator=(
      const MultiProfilePolicyProviderHelper& other) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) {
#if defined(OS_CHROMEOS)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  // The test should call this before the initial profile is created by chrome.
  void BeforeInitialProfileCreated() {
    // Set the overridden policy provider for the first Profile (|profile_1_|).
    // Note that the first ptofile will be created automatically by the
    // browser initialization.
    EXPECT_CALL(policy_for_profile_1_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::PushProfilePolicyConnectorProviderForTesting(
        &policy_for_profile_1_);
  }

  // The test should call this after the initial profile is created by chrome.
  void AfterInitialProfileCreated() {
    // Mimics what InProcessBrowserTest does to get the first created Profile.
    const BrowserList* browser_list = BrowserList::GetInstance();
    ASSERT_FALSE(browser_list->empty());
    Browser* first_browser = browser_list->get(0);
    profile_1_ = first_browser->profile();
    ASSERT_TRUE(profile_1_);
  }

  // Creates a additional profile. The Profile can then be accessed by
  // profile_2() and its policy by policy_for_profile_2(). Should be wrapped in
  // ASSERT_NO_FATAL_FAILURE.
  void CreateSecondProfile() {
    ASSERT_FALSE(profile_2_);

    // Prepare policy provider for second profile.
    EXPECT_CALL(policy_for_profile_2_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::PushProfilePolicyConnectorProviderForTesting(
        &policy_for_profile_2_);

    ProfileManager* profile_manager = g_browser_process->profile_manager();

    // Create an additional profile.
    base::FilePath path_profile =
        profile_manager->GenerateNextProfileDirectoryPath();
    base::RunLoop run_loop;
    profile_manager->CreateProfileAsync(
        path_profile,
        base::Bind(&OnProfileInitialized, &profile_2_, run_loop.QuitClosure()),
        base::string16(), std::string());

    // Run the message loop to allow profile creation to take place; the loop is
    // terminated by OnProfileInitialized calling the loop's QuitClosure when
    // the profile is created.
    run_loop.Run();

    // Make sure second profile creation does what we think it does.
    ASSERT_TRUE(profile_1() != profile_2());
  }

  Profile* profile_1() { return profile_1_; }
  Profile* profile_2() { return profile_2_; }

  // Returns the MockConfigurationPolicyProvider for profile_1.
  MockConfigurationPolicyProvider* policy_for_profile_1() {
    return &policy_for_profile_1_;
  }

  // Returns the MockConfigurationPolicyProvider for profile_2.
  MockConfigurationPolicyProvider* policy_for_profile_2() {
    return &policy_for_profile_2_;
  }

 private:
  // Called when an additional profile has been created.
  // The created profile is stored in *|out_created_profile|.
  static void OnProfileInitialized(Profile** out_created_profile,
                                   const base::Closure& closure,
                                   Profile* profile,
                                   Profile::CreateStatus status) {
    if (status == Profile::CREATE_STATUS_INITIALIZED) {
      *out_created_profile = profile;
      closure.Run();
    }
  }
  Profile* profile_1_ = nullptr;
  Profile* profile_2_ = nullptr;

  MockConfigurationPolicyProvider policy_for_profile_1_;
  MockConfigurationPolicyProvider policy_for_profile_2_;
};

// Verifies |certificate| with |storage_partition|'s CertVerifier and returns
// the result.
int VerifyTestServerCertInStoragePartition(
    content::StoragePartition* storage_partition,
    const scoped_refptr<net::X509Certificate>& certificate) {
  mojo::ScopedAllowSyncCallForTesting allow_sync_call;
  int result = net::OK;
  storage_partition->GetNetworkContext()->VerifyCertificateForTesting(
      certificate, "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), &result);
  return result;
}

// Verifies |certificate| with the CertVerifier for |profile|'s default
// StoragePartition and returns the result.
int VerifyTestServerCert(
    Profile* profile,
    const scoped_refptr<net::X509Certificate>& certificate) {
  return VerifyTestServerCertInStoragePartition(
      content::BrowserContext::GetDefaultStoragePartition(profile),
      certificate);
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
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(IsCertInNSSDatabaseOnIOThread,
                     profile->GetResourceContext(), subject_common_name,
                     &cert_found, run_loop.QuitClosure()));
  run_loop.Run();
  return cert_found;
}

bool IsCertInCertificateList(
    const net::X509Certificate* cert,
    const chromeos::NetworkCertLoader::NetworkCertList& network_cert_list) {
  for (const auto& network_cert : network_cert_list) {
    if (net::x509_util::IsSameCertificate(network_cert.cert(), cert))
      return true;
  }
  return false;
}

}  // namespace

// Allows testing if user policy provided trust roots take effect, without
// having device policy.
// The parameter specifies whether the CertVerifierService is enabled.
class PolicyProvidedCertsRegularUserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  PolicyProvidedCertsRegularUserTest() = default;
  ~PolicyProvidedCertsRegularUserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    multi_profile_policy_helper_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          network::features::kCertVerifierService);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          network::features::kCertVerifierService);
    }

    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    ASSERT_NO_FATAL_FAILURE(
        multi_profile_policy_helper_.BeforeInitialProfileCreated());
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ASSERT_NO_FATAL_FAILURE(
        multi_profile_policy_helper_.AfterInitialProfileCreated());

    ASSERT_NO_FATAL_FAILURE(user_policy_certs_helper_.Initialize());

    // Use the same testing slot as private and public slot for testing.
    test_nss_cert_db_ = std::make_unique<net::NSSCertDatabase>(
        crypto::ScopedPK11Slot(
            PK11_ReferenceSlot(test_nssdb_.slot())) /* public slot */,
        crypto::ScopedPK11Slot(
            PK11_ReferenceSlot(test_nssdb_.slot())) /* private slot */);
  }

  MultiProfilePolicyProviderHelper multi_profile_policy_helper_;

  base::test::ScopedFeatureList scoped_feature_list_;

  UserPolicyCertsHelper user_policy_certs_helper_;

  // A NSSCertDatabase is needed for the tests that do something with
  // NetworkCertLoader.
  crypto::ScopedTestNSSDB test_nssdb_;
  std::unique_ptr<net::NSSCertDatabase> test_nss_cert_db_;
};

IN_PROC_BROWSER_TEST_P(PolicyProvidedCertsRegularUserTest, TrustAnchorApplied) {
  user_policy_certs_helper_.SetRootCertONCUserPolicy(
      multi_profile_policy_helper_.profile_1(),
      multi_profile_policy_helper_.policy_for_profile_1());
  EXPECT_EQ(net::OK,
            VerifyTestServerCert(multi_profile_policy_helper_.profile_1(),
                                 user_policy_certs_helper_.server_cert()));
}

IN_PROC_BROWSER_TEST_P(PolicyProvidedCertsRegularUserTest,
                       PrimaryProfileTrustAnchorDoesNotLeak) {
  ASSERT_NO_FATAL_FAILURE(multi_profile_policy_helper_.CreateSecondProfile());

  user_policy_certs_helper_.SetRootCertONCUserPolicy(
      multi_profile_policy_helper_.profile_1(),
      multi_profile_policy_helper_.policy_for_profile_1());
  EXPECT_EQ(net::OK,
            VerifyTestServerCert(multi_profile_policy_helper_.profile_1(),
                                 user_policy_certs_helper_.server_cert()));
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            VerifyTestServerCert(multi_profile_policy_helper_.profile_2(),
                                 user_policy_certs_helper_.server_cert()));
}

IN_PROC_BROWSER_TEST_P(PolicyProvidedCertsRegularUserTest,
                       SecondaryProfileTrustAnchorDoesNotLeak) {
  ASSERT_NO_FATAL_FAILURE(multi_profile_policy_helper_.CreateSecondProfile());

  user_policy_certs_helper_.SetRootCertONCUserPolicy(
      multi_profile_policy_helper_.profile_2(),
      multi_profile_policy_helper_.policy_for_profile_2());
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            VerifyTestServerCert(multi_profile_policy_helper_.profile_1(),
                                 user_policy_certs_helper_.server_cert()));
  // TODO(https://crbug.com/1127263): That the cert from a secondary user's
  // policy is used at all is currently an artifact of the test, which reuses
  // the primary user_manager::User for the secondary Profile.
  // Fix that and then expect ERR_CERT_AUTHORITY_INVALID here too, and rename
  // the test to SecondaryProfileTrustAnchorIgnored.
  // Or, allow secondary Profile ONC policy to set trust anchors for the
  // secondary Profile.
  EXPECT_EQ(net::OK,
            VerifyTestServerCert(multi_profile_policy_helper_.profile_2(),
                                 user_policy_certs_helper_.server_cert()));
}

IN_PROC_BROWSER_TEST_P(PolicyProvidedCertsRegularUserTest,
                       UntrustedIntermediateAuthorityApplied) {
  // Sanity check: Apply ONC policy which does not mention the intermediate
  // authority.
  user_policy_certs_helper_.SetRootCertONCUserPolicy(
      multi_profile_policy_helper_.profile_1(),
      multi_profile_policy_helper_.policy_for_profile_1());
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            VerifyTestServerCert(
                multi_profile_policy_helper_.profile_1(),
                user_policy_certs_helper_.server_cert_by_intermediate()));

  // Now apply ONC policy which mentions the intermediate authority (but does
  // not assign trust to it).
  user_policy_certs_helper_.SetRootAndIntermediateCertsONCUserPolicy(
      multi_profile_policy_helper_.profile_1(),
      multi_profile_policy_helper_.policy_for_profile_1());
  EXPECT_EQ(net::OK,
            VerifyTestServerCert(
                multi_profile_policy_helper_.profile_1(),
                user_policy_certs_helper_.server_cert_by_intermediate()));
}

IN_PROC_BROWSER_TEST_P(PolicyProvidedCertsRegularUserTest,
                       AuthorityAvailableThroughNetworkCertLoader) {
  // Set |NetworkCertLoader| to use a test NSS database - otherwise, it is not
  // properly initialized because |UserSessionManager| only sets the primary
  // user's NSS Database in |NetworkCertLoader| if running on ChromeOS according
  // to |base::SysInfo|.
  ASSERT_TRUE(chromeos::NetworkCertLoader::IsInitialized());
  chromeos::NetworkCertLoader::Get()->SetUserNSSDB(test_nss_cert_db_.get());

  EXPECT_FALSE(IsCertInCertificateList(
      user_policy_certs_helper_.root_cert().get(),
      chromeos::NetworkCertLoader::Get()->authority_certs()));
  NetworkCertLoaderTestObserver network_cert_loader_observer(
      chromeos::NetworkCertLoader::Get());
  user_policy_certs_helper_.SetRootCertONCUserPolicy(
      multi_profile_policy_helper_.profile_1(),
      multi_profile_policy_helper_.policy_for_profile_1());
  network_cert_loader_observer.Wait();

  // Check that |NetworkCertLoader| is aware of the authority certificate.
  // (Web Trust does not matter for the NetworkCertLoader, but we currently only
  // set a policy with a certificate requesting Web Trust here).
  EXPECT_TRUE(IsCertInCertificateList(
      user_policy_certs_helper_.root_cert().get(),
      chromeos::NetworkCertLoader::Get()->authority_certs()));
}

INSTANTIATE_TEST_SUITE_P(All,
                         PolicyProvidedCertsRegularUserTest,
                         ::testing::Bool());

// Base class for testing policy-provided trust roots with device-local
// accounts. Needs device policy.
class PolicyProvidedCertsDeviceLocalAccountTest
    : public DevicePolicyCrosBrowserTest,
      public ::testing::WithParamInterface<bool> {
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
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          network::features::kCertVerifierService);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          network::features::kCertVerifierService);
    }

    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    ASSERT_NO_FATAL_FAILURE(user_policy_certs_helper_.Initialize());

    // Set up the mock policy provider.
    EXPECT_CALL(user_policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    BrowserPolicyConnector::SetPolicyProviderForTesting(&user_policy_provider_);

    device_policy()->policy_data().set_public_key_version(1);
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_show_user_names()->set_show_user_names(true);

    SetupDevicePolicy();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
    command_line->AppendSwitch(chromeos::switches::kOobeSkipPostLogin);
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  chromeos::LocalPolicyTestServerMixin local_policy_mixin_{&mixin_host_};

  const AccountId device_local_account_id_ =
      AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
          kDeviceLocalAccountId,
          DeviceLocalAccount::TYPE_PUBLIC_SESSION));

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
    ASSERT_TRUE(local_policy_mixin_.UpdateDevicePolicy(proto));
  }

  void StartLogin() {
    chromeos::WizardController::SkipPostLoginScreensForTesting();
    chromeos::WizardController* const wizard_controller =
        chromeos::WizardController::default_controller();
    ASSERT_TRUE(wizard_controller);
    wizard_controller->SkipToLoginForTesting();

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
IN_PROC_BROWSER_TEST_P(PolicyProvidedCertsPublicSessionTest,
                       DISABLED_AllowedInPublicSession) {
  StartLogin();
  chromeos::test::WaitForPrimaryUserSessionStart();

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

INSTANTIATE_TEST_SUITE_P(All,
                         PolicyProvidedCertsPublicSessionTest,
                         ::testing::Bool());

class PolicyProvidedCertsOnUserSessionInitTest
    : public LoginPolicyTestBase,
      public ::testing::WithParamInterface<bool> {
 protected:
  PolicyProvidedCertsOnUserSessionInitTest() {}

  void SetUpInProcessBrowserTestFixture() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          network::features::kCertVerifierService);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          network::features::kCertVerifierService);
    }

    LoginPolicyTestBase::SetUpInProcessBrowserTestFixture();
  }

  void GetMandatoryPoliciesValue(base::DictionaryValue* policy) const override {
    std::string user_policy_blob = GetTestCertsFileContents(kRootCaCertOnc);
    policy->SetKey(key::kOpenNetworkConfiguration,
                   base::Value(user_policy_blob));
  }

  void TriggerLogIn() {
    chromeos::LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<chromeos::GaiaScreenHandler>()
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
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(PolicyProvidedCertsOnUserSessionInitTest);
};

// Verifies that the policy-provided trust root is active as soon as the user
// session starts.
IN_PROC_BROWSER_TEST_P(PolicyProvidedCertsOnUserSessionInitTest,
                       TrustAnchorsAvailableImmediatelyAfterSessionStart) {
  // Load the certificate which is only OK if the policy-provided authority is
  // actually trusted.
  base::FilePath test_certs_path = GetTestCertsPath();

  base::FilePath server_cert_path = test_certs_path.AppendASCII(kServerCert);
  scoped_refptr<net::X509Certificate> server_cert = net::ImportCertFromFile(
      server_cert_path.DirName(), server_cert_path.BaseName().value());

  SkipToLoginScreen();
  TriggerLogIn();

  EXPECT_FALSE(session_manager::SessionManager::Get()->IsSessionStarted());

  chromeos::test::WaitForPrimaryUserSessionStart();
  EXPECT_EQ(net::OK, VerifyTestServerCert(active_user_profile(), server_cert));
}

INSTANTIATE_TEST_SUITE_P(All,
                         PolicyProvidedCertsOnUserSessionInitTest,
                         ::testing::Bool());

// Testing policy-provided client cert import.
class PolicyProvidedClientCertsTest
    : public DevicePolicyCrosBrowserTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  PolicyProvidedClientCertsTest() {}
  ~PolicyProvidedClientCertsTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          network::features::kCertVerifierService);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          network::features::kCertVerifierService);
    }

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
               base::Value(user_policy_blob), nullptr);
    provider_.UpdateChromePolicy(policy);

    cert_database_changed_observer.Wait();
    cert_database->RemoveObserver(&cert_database_changed_observer);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  MockConfigurationPolicyProvider provider_;
};

IN_PROC_BROWSER_TEST_P(PolicyProvidedClientCertsTest, ClientCertsImported) {
  // Sanity check: we don't expect the client certificate to be present before
  // setting the user ONC policy.
  EXPECT_FALSE(
      IsCertInNSSDatabase(browser()->profile(), kClientCertSubjectCommonName));

  SetClientCertONCPolicy(browser()->profile());
  EXPECT_TRUE(
      IsCertInNSSDatabase(browser()->profile(), kClientCertSubjectCommonName));
}

INSTANTIATE_TEST_SUITE_P(All, PolicyProvidedClientCertsTest, ::testing::Bool());

// TODO(https://crbug.com/874937): Add a test case for a kiosk session.

// Class for testing policy-provided extensions in the sign-in profile.
// Sets a device policy which applies the |kRootCaCert| for
// |kSigninScreenExtension1|. Force-installs |kSigninScreenExtension1| and
// |kSigninScreenExtension2| into the sign-in profile.
class PolicyProvidedCertsForSigninExtensionTest
    : public SigninProfileExtensionsPolicyTestBase,
      public ::testing::WithParamInterface<bool> {
 protected:
  // Use DEV channel as sign-in screen extensions are currently usable there.
  PolicyProvidedCertsForSigninExtensionTest()
      : SigninProfileExtensionsPolicyTestBase(version_info::Channel::DEV) {}
  ~PolicyProvidedCertsForSigninExtensionTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          network::features::kCertVerifierService);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          network::features::kCertVerifierService);
    }

    // Apply |kRootCaCert| for |kSigninScreenExtension1| in Device ONC policy.
    base::FilePath test_certs_path = GetTestCertsPath();
    std::string x509_contents;
    {
      base::ScopedAllowBlockingForTesting allow_io;
      ASSERT_TRUE(base::ReadFileToString(
          test_certs_path.AppendASCII(kRootCaCert), &x509_contents));
    }

    base::Value onc_dict = BuildONCForExtensionScopedCertificate(
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
    chromeos::StartupUtils::MarkOobeCompleted();  // Pretend that OOBE was
                                                  // complete.

    SigninProfileExtensionsPolicyTestBase::SetUpOnMainThread();

    signin_profile_ = GetInitialProfile();
    ASSERT_TRUE(chromeos::ProfileHelper::IsSigninProfile(signin_profile_));

    extensions::ExtensionBackgroundPageReadyObserver extension_1_observer(
        signin_profile_, kSigninScreenExtension1);
    extensions::ExtensionBackgroundPageReadyObserver extension_2_observer(
        signin_profile_, kSigninScreenExtension2);

    AddExtensionForForceInstallation(kSigninScreenExtension1,
                                     kSigninScreenExtension1UpdateManifestPath);
    AddExtensionForForceInstallation(kSigninScreenExtension2,
                                     kSigninScreenExtension2UpdateManifestPath);

    extension_1_observer.Wait();
    extension_2_observer.Wait();
  }

  content::StoragePartition* GetStoragePartitionForSigninExtension(
      const std::string& extension_id) {
    return extensions::util::GetStoragePartitionForExtensionId(
        extension_id, signin_profile_, /*can_create=*/false);
  }

  Profile* signin_profile_ = nullptr;
  scoped_refptr<net::X509Certificate> server_cert_;

 private:
  // Builds an ONC policy value that specifies exactly one certificate described
  // by |x509_contents| with Web trust to be used for |extension_id|.
  base::Value BuildONCForExtensionScopedCertificate(
      const std::string& x509_contents,
      const std::string& extension_id) {
    base::Value onc_cert_scope(base::Value::Type::DICTIONARY);
    onc_cert_scope.SetKey(onc::scope::kType,
                          base::Value(onc::scope::kExtension));
    onc_cert_scope.SetKey(onc::scope::kId, base::Value(extension_id));

    base::Value onc_cert_trust_bits(base::Value::Type::LIST);
    onc_cert_trust_bits.Append(base::Value(onc::certificate::kWeb));

    base::Value onc_certificate(base::Value::Type::DICTIONARY);
    onc_certificate.SetKey(onc::certificate::kGUID, base::Value("guid"));
    onc_certificate.SetKey(onc::certificate::kType,
                           base::Value(onc::certificate::kAuthority));
    onc_certificate.SetKey(onc::certificate::kX509, base::Value(x509_contents));
    onc_certificate.SetKey(onc::certificate::kScope, std::move(onc_cert_scope));
    onc_certificate.SetKey(onc::certificate::kTrustBits,
                           std::move(onc_cert_trust_bits));

    base::Value onc_certificates(base::Value::Type::LIST);
    onc_certificates.Append(std::move(onc_certificate));

    base::Value onc_dict(base::Value::Type::DICTIONARY);
    onc_dict.SetKey(onc::toplevel_config::kCertificates,
                    std::move(onc_certificates));
    onc_dict.SetKey(
        onc::toplevel_config::kType,
        base::Value(onc::toplevel_config::kUnencryptedConfiguration));

    return onc_dict;
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(PolicyProvidedCertsForSigninExtensionTest);
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
IN_PROC_BROWSER_TEST_P(PolicyProvidedCertsForSigninExtensionTest,
                       ActiveOnlyInSelectedExtension) {
  chromeos::OobeScreenWaiter(chromeos::OobeBaseTest::GetFirstSigninScreen())
      .Wait();
  content::StoragePartition* signin_profile_default_partition =
      content::BrowserContext::GetDefaultStoragePartition(signin_profile_);

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
      chromeos::login::GetSigninPartition();
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

INSTANTIATE_TEST_SUITE_P(All,
                         PolicyProvidedCertsForSigninExtensionTest,
                         ::testing::Bool());

}  // namespace policy

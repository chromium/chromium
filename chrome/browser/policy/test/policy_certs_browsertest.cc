// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/policy/networking/policy_cert_service.h"
#include "chrome/browser/policy/networking/policy_cert_service_factory.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater_ash.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater_factory.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/ash/components/network/onc/onc_certificate_importer.h"
#include "chromeos/ash/components/network/onc/onc_certificate_importer_impl.h"
#include "chromeos/components/onc/onc_test_utils.h"
#include "chromeos/test/chromeos_test_utils.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/saml/lockscreen_reauth_dialog_test_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"

using ::testing::NotNull;
#endif

namespace em = enterprise_management;

namespace policy {
namespace {

// Test data file storing an ONC blob with an Authority certificate.
constexpr char kRootCaCertOnc[] = "root-ca-cert.onc";
constexpr char kRootAndIntermediateCaCertsOnc[] =
    "root-and-intermediate-ca-certs.onc";
// A PEM-encoded certificate which was signed by the Authority specified in
// |kRootCaCertOnc|.
constexpr char kServerCert[] = "ok_cert.pem";
// A PEM-encoded certificate which was signed by the intermediate Authority
// specified in |kRootAndIntermediateCaCertsOnc|.
constexpr char kServerCertByIntermediate[] = "ok_cert_by_intermediate.pem";
// The PEM-encoded Authority certificate specified by |kRootCaCertOnc|.
constexpr char kRootCaCert[] = "root_ca_cert.pem";

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

// Allows waiting until |NetworkCertLoader| updates its list of certificates.
class NetworkCertLoaderTestObserver : public ash::NetworkCertLoader::Observer {
 public:
  explicit NetworkCertLoaderTestObserver(
      ash::NetworkCertLoader* network_cert_loader)
      : network_cert_loader_(network_cert_loader) {
    network_cert_loader_->AddObserver(this);
  }

  NetworkCertLoaderTestObserver(const NetworkCertLoaderTestObserver&) = delete;
  NetworkCertLoaderTestObserver& operator=(
      const NetworkCertLoaderTestObserver&) = delete;

  ~NetworkCertLoaderTestObserver() override {
    network_cert_loader_->RemoveObserver(this);
  }

  // ash::NetworkCertLoader::Observer
  void OnCertificatesLoaded() override { run_loop_.Quit(); }

  void Wait() { run_loop_.Run(); }

 private:
  raw_ptr<ash::NetworkCertLoader> network_cert_loader_;
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
    if (!user_network_configuration_updater) {
      // In Lacros-Chrome the ONC policy is only handled by the main profile.
      // Secondary profiles ignore it and UserNetworkConfigurationUpdater is not
      // created.s
      return;
    }

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

    // The above `trust_roots_changed_observer` only ensures that the
    // UpdateAdditionalCertificates message has been sent, but not that the
    // CertVerifierService has received it. Do a FlushForTesting on the loaded
    // CertVerifierServiceUpdaters for `profile`, to ensure any earlier
    // messages on the mojo pipes have been processed.
    profile->ForEachLoadedStoragePartition(
        &content::StoragePartition::FlushCertVerifierInterfaceForTesting);
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
// TODO(crbug.com/40718963): Transform this into a general-purpose mixin.
class MultiProfilePolicyProviderHelper {
 public:
  MultiProfilePolicyProviderHelper() = default;
  ~MultiProfilePolicyProviderHelper() = default;

  MultiProfilePolicyProviderHelper(
      const MultiProfilePolicyProviderHelper& other) = delete;
  MultiProfilePolicyProviderHelper& operator=(
      const MultiProfilePolicyProviderHelper& other) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line->AppendSwitch(
        ash::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  // The test should call this before the initial profile is created by chrome.
  void BeforeInitialProfileCreated() {
    // Set the overridden policy provider for the first Profile (|profile_1_|).
    // Note that the first ptofile will be created automatically by the
    // browser initialization.
    policy_for_profile_1_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
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
    policy_for_profile_2_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::PushProfilePolicyConnectorProviderForTesting(
        &policy_for_profile_2_);

    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath path_profile =
        profile_manager->GenerateNextProfileDirectoryPath();
    // Create an additional profile.
    profile_2_ =
        &profiles::testing::CreateProfileSync(profile_manager, path_profile);

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
  raw_ptr<Profile, DanglingUntriaged> profile_1_ = nullptr;
  raw_ptr<Profile, DanglingUntriaged> profile_2_ = nullptr;

  testing::NiceMock<MockConfigurationPolicyProvider> policy_for_profile_1_;
  testing::NiceMock<MockConfigurationPolicyProvider> policy_for_profile_2_;
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

// Allows testing if user policy provided trust roots take effect, without
// having device policy.
// The parameter specifies whether the CertVerifierService is enabled.
class PolicyProvidedCertsRegularUserTest : public InProcessBrowserTest {
 protected:
  PolicyProvidedCertsRegularUserTest() = default;
  ~PolicyProvidedCertsRegularUserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    multi_profile_policy_helper_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
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

  UserPolicyCertsHelper user_policy_certs_helper_;

  // A NSSCertDatabase is needed for the tests that do something with
  // NetworkCertLoader.
  crypto::ScopedTestNSSDB test_nssdb_;
  std::unique_ptr<net::NSSCertDatabase> test_nss_cert_db_;
};

IN_PROC_BROWSER_TEST_F(PolicyProvidedCertsRegularUserTest, NoTrustAnchor) {
  ASSERT_NO_FATAL_FAILURE(multi_profile_policy_helper_.CreateSecondProfile());

  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            VerifyTestServerCert(multi_profile_policy_helper_.profile_1(),
                                 user_policy_certs_helper_.server_cert()));
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            VerifyTestServerCert(multi_profile_policy_helper_.profile_2(),
                                 user_policy_certs_helper_.server_cert()));

  if (PolicyCertServiceFactory::GetForProfile(
          multi_profile_policy_helper_.profile_1())) {
    EXPECT_FALSE(PolicyCertServiceFactory::GetForProfile(
                     multi_profile_policy_helper_.profile_1())
                     ->UsedPolicyCertificates());
  }
  if (PolicyCertServiceFactory::GetForProfile(
          multi_profile_policy_helper_.profile_2())) {
    EXPECT_FALSE(PolicyCertServiceFactory::GetForProfile(
                     multi_profile_policy_helper_.profile_2())
                     ->UsedPolicyCertificates());
  }
}

IN_PROC_BROWSER_TEST_F(PolicyProvidedCertsRegularUserTest, TrustAnchorApplied) {
  user_policy_certs_helper_.SetRootCertONCUserPolicy(
      multi_profile_policy_helper_.profile_1(),
      multi_profile_policy_helper_.policy_for_profile_1());
  EXPECT_EQ(net::OK,
            VerifyTestServerCert(multi_profile_policy_helper_.profile_1(),
                                 user_policy_certs_helper_.server_cert()));

  EXPECT_TRUE(PolicyCertServiceFactory::GetForProfile(
                  multi_profile_policy_helper_.profile_1())
                  ->UsedPolicyCertificates());
}

// Test that policy provided trust anchors are available in Incognito mode.
IN_PROC_BROWSER_TEST_F(PolicyProvidedCertsRegularUserTest,
                       TrustAnchorAppliedInIncognito) {
  user_policy_certs_helper_.SetRootCertONCUserPolicy(
      multi_profile_policy_helper_.profile_1(),
      multi_profile_policy_helper_.policy_for_profile_1());

  Profile* otr_profile =
      multi_profile_policy_helper_.profile_1()->GetPrimaryOTRProfile(
          /*create_if_needed=*/true);

  EXPECT_EQ(net::OK, VerifyTestServerCert(
                         otr_profile, user_policy_certs_helper_.server_cert()));
}

IN_PROC_BROWSER_TEST_F(PolicyProvidedCertsRegularUserTest,
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

  EXPECT_TRUE(PolicyCertServiceFactory::GetForProfile(
                  multi_profile_policy_helper_.profile_1())
                  ->UsedPolicyCertificates());
  if (PolicyCertServiceFactory::GetForProfile(
          multi_profile_policy_helper_.profile_2())) {
    EXPECT_FALSE(PolicyCertServiceFactory::GetForProfile(
                     multi_profile_policy_helper_.profile_2())
                     ->UsedPolicyCertificates());
  }
}

IN_PROC_BROWSER_TEST_F(PolicyProvidedCertsRegularUserTest,
                       SecondaryProfileTrustAnchorDoesNotLeakOrIgnored) {
  ASSERT_NO_FATAL_FAILURE(multi_profile_policy_helper_.CreateSecondProfile());

  user_policy_certs_helper_.SetRootCertONCUserPolicy(
      multi_profile_policy_helper_.profile_2(),
      multi_profile_policy_helper_.policy_for_profile_2());
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            VerifyTestServerCert(multi_profile_policy_helper_.profile_1(),
                                 user_policy_certs_helper_.server_cert()));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/40718963): That the cert from a secondary user's
  // policy is used at all is currently an artifact of the test, which reuses
  // the primary user_manager::User for the secondary Profile.
  // Fix that and then expect ERR_CERT_AUTHORITY_INVALID here too, and rename
  // the test to SecondaryProfileTrustAnchorIgnored.
  // Or, allow secondary Profile ONC policy to set trust anchors for the
  // secondary Profile.
  EXPECT_EQ(net::OK,
            VerifyTestServerCert(multi_profile_policy_helper_.profile_2(),
                                 user_policy_certs_helper_.server_cert()));
#else  // Implies #if BUILDFLAG(IS_CHROMEOS_LACROS), but this is a generally
       // correct behavior according to the comment above.
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            VerifyTestServerCert(multi_profile_policy_helper_.profile_2(),
                                 user_policy_certs_helper_.server_cert()));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

IN_PROC_BROWSER_TEST_F(PolicyProvidedCertsRegularUserTest,
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

// NetworkCertLoader is only relevant for Ash-Chrome.
#if BUILDFLAG(IS_CHROMEOS_ASH)

bool IsCertInCertificateList(
    const net::X509Certificate* cert,
    const ash::NetworkCertLoader::NetworkCertList& network_cert_list) {
  for (const auto& network_cert : network_cert_list) {
    if (net::x509_util::IsSameCertificate(network_cert.cert(), cert))
      return true;
  }
  return false;
}

IN_PROC_BROWSER_TEST_F(PolicyProvidedCertsRegularUserTest,
                       AuthorityAvailableThroughNetworkCertLoader) {
  // Set |NetworkCertLoader| to use a test NSS database - otherwise, it is not
  // properly initialized because |UserSessionManager| only sets the primary
  // user's NSS Database in |NetworkCertLoader| if running on ChromeOS according
  // to |base::SysInfo|.
  ASSERT_TRUE(ash::NetworkCertLoader::IsInitialized());
  ash::NetworkCertLoader::Get()->SetUserNSSDB(test_nss_cert_db_.get());

  EXPECT_FALSE(IsCertInCertificateList(
      user_policy_certs_helper_.root_cert().get(),
      ash::NetworkCertLoader::Get()->authority_certs()));
  NetworkCertLoaderTestObserver network_cert_loader_observer(
      ash::NetworkCertLoader::Get());
  user_policy_certs_helper_.SetRootCertONCUserPolicy(
      multi_profile_policy_helper_.profile_1(),
      multi_profile_policy_helper_.policy_for_profile_1());
  network_cert_loader_observer.Wait();

  // Check that |NetworkCertLoader| is aware of the authority certificate.
  // (Web Trust does not matter for the NetworkCertLoader, but we currently only
  // set a policy with a certificate requesting Web Trust here).
  EXPECT_TRUE(IsCertInCertificateList(
      user_policy_certs_helper_.root_cert().get(),
      ash::NetworkCertLoader::Get()->authority_certs()));
}

// Test that the lock screen profile uses the policy provided custom trusted
// anchors of the primary profile .
IN_PROC_BROWSER_TEST_F(PolicyProvidedCertsRegularUserTest,
                       LockScreenPrimaryProfileCerts) {
  ash::ScreenLockerTester locker;
  locker.Lock();
  // Showing the reauth dialog will create the lock screen profile.
  ash::LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_THAT(ash::ProfileHelper::GetLockScreenProfile(), NotNull());

  // Set policy provided trusted anchors on the primary profile.
  user_policy_certs_helper_.SetRootCertONCUserPolicy(
      browser()->profile(),
      multi_profile_policy_helper_.policy_for_profile_1());

  EXPECT_EQ(net::OK,
            VerifyTestServerCert(browser()->profile(),
                                 user_policy_certs_helper_.server_cert()));
  // Verify that the lock screen can access the policy provided certs.
  EXPECT_EQ(net::OK,
            VerifyTestServerCert(ash::ProfileHelper::GetLockScreenProfile(),
                                 user_policy_certs_helper_.server_cert()));

  EXPECT_TRUE(PolicyCertServiceFactory::GetForProfile(browser()->profile())
                  ->UsedPolicyCertificates());
  EXPECT_TRUE(PolicyCertServiceFactory::GetForProfile(
                  ash::ProfileHelper::GetLockScreenProfile())
                  ->UsedPolicyCertificates());
}

// Test that the lock screen profile doesn't use the policy provided custom
// trusted anchors of a secondary profile.
IN_PROC_BROWSER_TEST_F(PolicyProvidedCertsRegularUserTest,
                       LockScreenSecondaryProfileCerts) {
  ash::ScreenLockerTester locker;
  locker.Lock();
  // Showing the reauth dialog will create the lock screen profile.
  ash::LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_THAT(ash::ProfileHelper::GetLockScreenProfile(), NotNull());

  ASSERT_NO_FATAL_FAILURE(multi_profile_policy_helper_.CreateSecondProfile());
  user_policy_certs_helper_.SetRootCertONCUserPolicy(
      multi_profile_policy_helper_.profile_2(),
      multi_profile_policy_helper_.policy_for_profile_2());

  EXPECT_EQ(net::OK,
            VerifyTestServerCert(multi_profile_policy_helper_.profile_2(),
                                 user_policy_certs_helper_.server_cert()));
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            VerifyTestServerCert(ash::ProfileHelper::GetLockScreenProfile(),
                                 user_policy_certs_helper_.server_cert()));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
}  // namespace policy

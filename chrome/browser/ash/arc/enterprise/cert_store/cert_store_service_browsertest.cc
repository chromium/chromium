// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/arc/enterprise/cert_store/cert_store_service.h"

#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/constants/ash_switches.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/arc/enterprise/cert_store/cert_store_service_factory.h"
#include "chrome/browser/ash/arc/keymaster/arc_keymaster_bridge.h"
#include "chrome/browser/ash/arc/keymint/arc_keymint_bridge.h"
#include "chrome/browser/ash/arc/session/arc_service_launcher.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service_factory.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/platform_keys/extension_key_permissions_service.h"
#include "chrome/browser/chromeos/platform_keys/extension_key_permissions_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "chrome/common/pref_names.h"
#include "chrome/services/keymaster/public/mojom/cert_store.mojom.h"
#include "chrome/services/keymint/public/mojom/cert_store.mojom.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "extensions/browser/extension_system.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

constexpr char kFileName1[] = "client_1";
constexpr char kFileName2[] = "client_2";
constexpr char kFileName3[] = "client_3";
constexpr char kFileName4[] = "client_4";

constexpr char kFakeExtensionId[] = "fakeextensionid";

// Contains information needed for test cert parameters.
struct TestCertData {
  TestCertData(const std::string& file_name,
               bool is_corporate_usage,
               keymanagement::mojom::ChapsSlot slot)
      : file_name(file_name),
        is_corporate_usage(is_corporate_usage),
        slot(slot) {
    // Keys in system slot must be corporate usage.
    DCHECK(slot != keymanagement::mojom::ChapsSlot::kSystem ||
           is_corporate_usage);
  }
  TestCertData(const TestCertData&) = default;
  bool operator==(const TestCertData& other) const {
    return std::tie(file_name, is_corporate_usage, slot) ==
           std::tie(other.file_name, other.is_corporate_usage, other.slot);
  }

  std::string file_name;
  bool is_corporate_usage;
  keymanagement::mojom::ChapsSlot slot;
};

struct CertStoreServiceTestData {
  std::vector<TestCertData> certs;
  bool should_use_arc_keymint;
};

// Associates a |test_data| to its |nss_cert| once installed.
struct InstalledTestCert {
  InstalledTestCert(TestCertData test_data, net::ScopedCERTCertificate nss_cert)
      : test_data(test_data), nss_cert(std::move(nss_cert)) {}
  TestCertData test_data;
  net::ScopedCERTCertificate nss_cert;
};

std::string GetDerCert64(CERTCertificate* cert) {
  std::string der_cert;
  EXPECT_TRUE(net::x509_util::GetDEREncoded(cert, &der_cert));
  return base::Base64Encode(der_cert);
}

class FakeArcCertInstaller : public ArcCertInstaller {
 public:
  FakeArcCertInstaller(Profile* profile,
                       std::unique_ptr<policy::RemoteCommandsQueue> queue)
      : ArcCertInstaller(profile, std::move(queue)) {}

  // Returns map from nicknames to real der cert64 to identify certificates.
  std::map<std::string, std::string> InstallArcCerts(
      std::vector<CertDescription> certs,
      InstallArcCertsCallback callback) override {
    certs_.clear();
    cert_ids_.clear();
    for (const auto& cert : certs) {
      std::string cert_name =
          x509_certificate_model::GetCertNameOrNickname(cert.nss_cert.get());
      certs_[cert_name] = GetDerCert64(cert.nss_cert.get());
      cert_ids_[cert_name] = cert.id;
    }

    callback_ = std::move(callback);
    Stop();
    return certs_;
  }

  void RunCompletionCallback(bool success) {
    std::move(callback_).Run(success);
  }

  void Wait() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  void Stop() {
    if (run_loop_)
      run_loop_->QuitWhenIdle();
  }

  std::map<std::string, std::string> certs() const { return certs_; }

  std::map<std::string, std::string> cert_ids() const { return cert_ids_; }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  std::map<std::string, std::string> certs_;
  std::map<std::string, std::string> cert_ids_;
  InstallArcCertsCallback callback_;
};

class FakeArcKeymasterBridge : public ArcKeymasterBridge {
 public:
  explicit FakeArcKeymasterBridge(content::BrowserContext* context)
      : ArcKeymasterBridge(context, nullptr) {}
  FakeArcKeymasterBridge(const FakeArcKeymasterBridge& other) = delete;
  FakeArcKeymasterBridge& operator=(const FakeArcKeymasterBridge&) = delete;

  void UpdatePlaceholderKeys(std::vector<keymaster::mojom::ChromeOsKeyPtr> keys,
                             UpdatePlaceholderKeysCallback callback) override {
    keys_ = std::move(keys);
    std::move(callback).Run(/*success=*/true);
  }

  const std::vector<keymaster::mojom::ChromeOsKeyPtr>& placeholder_keys()
      const {
    return keys_;
  }

 private:
  std::vector<keymaster::mojom::ChromeOsKeyPtr> keys_;
};

std::unique_ptr<FakeArcKeymasterBridge> BuildFakeArcKeymasterBridge(
    content::BrowserContext* profile) {
  return std::make_unique<FakeArcKeymasterBridge>(profile);
}

class FakeArcKeyMintBridge : public ArcKeyMintBridge {
 public:
  explicit FakeArcKeyMintBridge(content::BrowserContext* context)
      : ArcKeyMintBridge(context, nullptr) {}
  FakeArcKeyMintBridge(const FakeArcKeyMintBridge& other) = delete;
  FakeArcKeyMintBridge& operator=(const FakeArcKeyMintBridge&) = delete;

  void UpdatePlaceholderKeys(std::vector<keymint::mojom::ChromeOsKeyPtr> keys,
                             UpdatePlaceholderKeysCallback callback) override {
    keys_ = std::move(keys);
    std::move(callback).Run(/*success=*/true);
  }

  const std::vector<keymint::mojom::ChromeOsKeyPtr>& placeholder_keys() const {
    return keys_;
  }

 private:
  std::vector<keymint::mojom::ChromeOsKeyPtr> keys_;
};

std::unique_ptr<FakeArcKeyMintBridge> BuildFakeArcKeyMintBridge(
    content::BrowserContext* profile) {
  return std::make_unique<FakeArcKeyMintBridge>(profile);
}

// The following series of functions related to IsSystemSlotAvailable use the
// NSSCertDatabase. The cert database is accessed through a raw pointer with
// limited lifetime guarantees and is not thread safe. Namely, the cert database
// is guaranteed valid for the single IO thread task where it was received.
//
// Furthermore, creating an NssCertDatabaseGetter requires a BrowserContext,
// which can only be accessed on the UI thread.
//
// ListCerts and related functions are implemented to make sure the above
// requirements are respected. Here's a diagram of the interaction between UI
// and IO threads.
//
//             UI Thread                        IO Thread
//
//       IsSystemSlotAvailable
//                 |
//       run_loop.QuitClosure
//                 |
//   NssService::CreateNSSCertDatabaseGetterForIOThread
//                 |
//                 \--------------------------------v
//                                 IsSystemSlotAvailableWithDbGetterOnIO
//                                                  |
//                                         database_getter.Run
//                                                  |
//                                       IsSystemSlotAvailableOnIO
//                                                  |
//                                            GetSystemSlot
//                                                  |
//                                           quit_closure.Run

void IsSystemSlotAvailableOnIO(bool* out_system_slot_available,
                               base::OnceClosure done_closure,
                               net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  *out_system_slot_available = cert_db->GetSystemSlot() != nullptr;
  std::move(done_closure).Run();
}

void IsSystemSlotAvailableWithDbGetterOnIO(
    NssCertDatabaseGetter database_getter,
    bool* out_system_slot_available,
    base::OnceClosure done_closure) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto did_get_cert_db_split_callback = base::SplitOnceCallback(
      base::BindOnce(&IsSystemSlotAvailableOnIO, out_system_slot_available,
                     std::move(done_closure)));
  net::NSSCertDatabase* cert_db =
      std::move(database_getter)
          .Run(std::move(did_get_cert_db_split_callback.first));
  if (cert_db) {
    std::move(did_get_cert_db_split_callback.second).Run(cert_db);
  }
}

// Returns trus if the test system slot was setup correctly and is available.
bool IsSystemSlotAvailable(Profile* profile) {
  // |profile| must be accessed on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop run_loop;
  bool system_slot_available = false;
  // The NssCertDatabaseGetter must be posted to the IO thread immediately.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(IsSystemSlotAvailableWithDbGetterOnIO,
                     NssServiceFactory::GetForContext(profile)
                         ->CreateNSSCertDatabaseGetterForIOThread(),
                     &system_slot_available, run_loop.QuitClosure()));
  run_loop.Run();
  return system_slot_available;
}

// Returns the number of corporate usage certs in |test_certs|.
size_t CountCorporateUsage(const std::vector<TestCertData>& test_certs) {
  return base::ranges::count_if(test_certs, &TestCertData::is_corporate_usage);
}

// Deletes the given |cert| from |cert_db|.
void DeleteCertAndKey(CERTCertificate* cert,
                      base::OnceClosure done_callback,
                      net::NSSCertDatabase* cert_db) {
  base::ScopedAllowBlockingForTesting allow_io;
  EXPECT_TRUE(cert_db->DeleteCertAndKey(cert));
  std::move(done_callback).Run();
}

// Called once a key has been registered as corporate usage.
void OnKeyRegisteredForCorporateUsage(base::OnceClosure done_callback,
                                      bool is_error,
                                      crosapi::mojom::KeystoreError error) {
  ASSERT_FALSE(is_error) << static_cast<int>(error);
  std::move(done_callback).Run();
}

// Uses |service| to register |cert| as corporate usage.
void RegisterCorporateKeyWithService(
    CERTCertificate* cert,
    base::OnceClosure done_callback,
    std::unique_ptr<chromeos::platform_keys::ExtensionKeyPermissionsService>
        service) {
  std::vector<uint8_t> client_cert_spki(
      cert->derPublicKey.data,
      cert->derPublicKey.data + cert->derPublicKey.len);
  service->RegisterKeyForCorporateUsage(
      std::move(client_cert_spki),
      base::BindOnce(&OnKeyRegisteredForCorporateUsage,
                     std::move(done_callback)));
}

}  // namespace

class CertStoreServiceTest
    : public MixinBasedInProcessBrowserTest,
      public ::testing::WithParamInterface<CertStoreServiceTestData> {
 public:
  CertStoreServiceTest();
  CertStoreServiceTest(const CertStoreServiceTest& other) = delete;
  CertStoreServiceTest& operator=(const CertStoreServiceTest&) = delete;

 protected:
  void SetUp() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  void SetUpInProcessBrowserTestFixture() override;

  void SetUpOnMainThread() override;

  void TearDown() override;

  void TearDownOnMainThread() override;

  void TearDownInProcessBrowserTestFixture() override;

  // Installs the given |certs_to_setup| in the NSS database. Will block until
  // all install events are processed by CertStoreService.
  void SetUpCerts(const std::vector<TestCertData>& certs_to_setup);

  // Registers the given |cert| as corporate usage through platform keys.
  void RegisterCorporateKey(CERTCertificate* cert);

  // Deletes the given |cert| from the NSS cert database. Will block until this
  // delete event is processed by CertStoreService.
  void DeleteCert(CERTCertificate* cert);

  // Verifies the expected |test_certs| are installed correctly.
  void CheckInstalledCerts(std::vector<TestCertData> test_certs,
                           CertStoreService* service);

  // Returns the profile for the |affiliation_mixin_| account.
  Profile* profile();

  // Owned by the CertStoreService instance.
  raw_ptr<FakeArcCertInstaller, DanglingUntriaged> installer_;

  std::vector<InstalledTestCert> installed_certs_;

  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  policy::DevicePolicyCrosTestHelper device_policy_helper_;
  policy::AffiliationMixin affiliation_mixin_{&mixin_host_,
                                              &device_policy_helper_};
  CertStoreServiceTestData test_data_;

 private:
  // Creates ScopedCERTCertificates for each |certs_to_setup| and appends them
  // to |installed_certs_|.
  void SetUpTestClientCerts(const std::vector<TestCertData>& certs_to_setup,
                            base::OnceClosure done_callback,
                            net::NSSCertDatabase* cert_db);

  // Imports the given |nss_cert| into the NSS |cert_db|.
  void ImportCert(CERTCertificate* const nss_cert,
                  base::OnceClosure done_callback,
                  net::NSSCertDatabase* cert_db);

  // Checks that |keymaster_bridge_->placeholder_keys()| contains a key with
  // given |id| and |slot|.
  bool PlaceholdersContainIdAndSlot(const std::string& id,
                                    keymanagement::mojom::ChapsSlot slot);

  // Initializes |test_system_slot_|.
  void SetUpTestSystemSlot();

  // Destroys |test_system_slot_|.
  void TearDownTestSystemSlot();

  void TearDownTestSystemSlotOnIO();

  std::unique_ptr<crypto::ScopedTestSystemNSSKeySlot> test_system_slot_;

  // Owned by the CertStoreService instance.
  raw_ptr<FakeArcKeymasterBridge, DanglingUntriaged> keymaster_bridge_;
  raw_ptr<FakeArcKeyMintBridge, DanglingUntriaged> keymint_bridge_;

  ash::CryptohomeMixin cryptohome_mixin_{&mixin_host_};

  base::test::ScopedFeatureList feature_list_;
};

CertStoreServiceTest::CertStoreServiceTest() : test_data_(GetParam()) {
  cryptohome_mixin_.MarkUserAsExisting(affiliation_mixin_.account_id());
  cryptohome_mixin_.ApplyAuthConfig(
      affiliation_mixin_.account_id(),
      ash::test::UserAuthConfig::Create(ash::test::kDefaultAuthSetup));
}

void CertStoreServiceTest::SetUp() {
  if (test_data_.should_use_arc_keymint) {
    base::SysInfo::SetChromeOSVersionInfoForTest(
        "CHROMEOS_ARC_ANDROID_SDK_VERSION=33",  // TM
        base::SysInfo::GetLsbReleaseTime());
    feature_list_.InitAndEnableFeature(arc::kSwitchToKeyMintOnT);
  } else {
    base::SysInfo::SetChromeOSVersionInfoForTest(
        "CHROMEOS_ARC_ANDROID_SDK_VERSION=30",  // RVC
        base::SysInfo::GetLsbReleaseTime());
  }

  MixinBasedInProcessBrowserTest::SetUp();
}

void CertStoreServiceTest::SetUpCommandLine(base::CommandLine* command_line) {
  MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  arc::SetArcAvailableCommandLineForTesting(command_line);
  policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
      command_line);
}

void CertStoreServiceTest::SetUpInProcessBrowserTestFixture() {
  MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  ash::platform_keys::PlatformKeysServiceFactory::GetInstance()->SetTestingMode(
      true);

  // Set up a system slot so tests can access device certs.
  ASSERT_NO_FATAL_FAILURE(SetUpTestSystemSlot());
}

void CertStoreServiceTest::SetUpOnMainThread() {
  MixinBasedInProcessBrowserTest::SetUpOnMainThread();

  // Pre tests need no further setup.
  if (content::IsPreTest())
    return;

  policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());

  if (test_data_.should_use_arc_keymint) {
    // Use fake ArcKeyMintBridge.
    keymint_bridge_ =
        ArcKeyMintBridge::GetFactory()->SetTestingSubclassFactoryAndUse(
            profile(), base::BindOnce(&BuildFakeArcKeyMintBridge));
  } else {
    // Use fake ArcKeymasterBridge.
    keymaster_bridge_ =
        ArcKeymasterBridge::GetFactory()->SetTestingSubclassFactoryAndUse(
            profile(), base::BindOnce(&BuildFakeArcKeymasterBridge));
  }

  // Use fake ArcCertInstaller in CertStoreService.
  CertStoreServiceFactory::GetInstance()->SetTestingSubclassFactoryAndUse(
      profile(),
      base::BindOnce(
          [](raw_ptr<FakeArcCertInstaller, DanglingUntriaged>* out_installer,
             content::BrowserContext* context) {
            Profile* profile = Profile::FromBrowserContext(context);
            auto installer = std::make_unique<FakeArcCertInstaller>(
                profile, std::make_unique<policy::RemoteCommandsQueue>());
            CHECK(out_installer);
            CHECK(!*out_installer);
            *out_installer = installer.get();
            return std::make_unique<CertStoreService>(profile,
                                                      std::move(installer));
          },
          base::Unretained(&installer_)));

  ASSERT_TRUE(IsSystemSlotAvailable(profile()));
}

void CertStoreServiceTest::TearDown() {
  MixinBasedInProcessBrowserTest::TearDown();

  base::SysInfo::ResetChromeOSVersionInfoForTest();
}

void CertStoreServiceTest::TearDownOnMainThread() {
  TearDownTestSystemSlot();
  MixinBasedInProcessBrowserTest::TearDownOnMainThread();
}

void CertStoreServiceTest::TearDownInProcessBrowserTestFixture() {
  ash::platform_keys::PlatformKeysServiceFactory::GetInstance()->SetTestingMode(
      false);
  MixinBasedInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
}

void CertStoreServiceTest::SetUpCerts(
    const std::vector<TestCertData>& certs_to_setup) {
  // Remember current size of |installed_certs_| before new certs.
  size_t initial_size = installed_certs_.size();

  {
    // Read certs from files.
    base::RunLoop loop;
    NssServiceFactory::GetForContext(profile())
        ->UnsafelyGetNSSCertDatabaseForTesting(base::BindOnce(
            &CertStoreServiceTest::SetUpTestClientCerts, base::Unretained(this),
            certs_to_setup, loop.QuitClosure()));
    loop.Run();
  }

  // Verify |certs_to_setup.size()| new certs have been installed.
  ASSERT_EQ(installed_certs_.size(), certs_to_setup.size() + initial_size);

  // Process all new certs.
  for (size_t i = initial_size; i < installed_certs_.size(); ++i) {
    const InstalledTestCert& cert = installed_certs_[i];
    // Register cert for corporate usage if needed.
    if (cert.test_data.is_corporate_usage)
      RegisterCorporateKey(cert.nss_cert.get());
    // Import cert to NSS cert database.
    base::RunLoop loop;
    NssServiceFactory::GetForContext(profile())
        ->UnsafelyGetNSSCertDatabaseForTesting(base::BindOnce(
            &CertStoreServiceTest::ImportCert, base::Unretained(this),
            cert.nss_cert.get(), loop.QuitClosure()));
    loop.Run();
    // Wait till new cert event is processed by CertStoreService.
    installer_->Wait();
    installer_->RunCompletionCallback(true /* success */);
  }
}

void CertStoreServiceTest::RegisterCorporateKey(CERTCertificate* cert) {
  base::RunLoop run_loop;
  chromeos::platform_keys::ExtensionKeyPermissionsServiceFactory::
      GetForBrowserContextAndExtension(
          base::BindOnce(&RegisterCorporateKeyWithService, cert,
                         run_loop.QuitClosure()),
          profile(), kFakeExtensionId);
  run_loop.Run();
}

void CertStoreServiceTest::DeleteCert(CERTCertificate* cert) {
  base::RunLoop loop;
  NssServiceFactory::GetForContext(profile())
      ->UnsafelyGetNSSCertDatabaseForTesting(
          base::BindOnce(&DeleteCertAndKey, cert, loop.QuitClosure()));
  loop.Run();
  installed_certs_.pop_back();
  // Wait till deleted cert event is processed by CertStoreService.
  installer_->Wait();
  installer_->RunCompletionCallback(true /* success */);
}

void CertStoreServiceTest::CheckInstalledCerts(
    std::vector<TestCertData> test_certs,
    CertStoreService* service) {
  // Verify the number of corporate usage certs reported is correct.
  EXPECT_EQ(CountCorporateUsage(test_certs), installer_->certs().size());
  EXPECT_EQ(CountCorporateUsage(test_certs),
            service->get_required_cert_names().size());
  if (test_data_.should_use_arc_keymint) {
    EXPECT_EQ(CountCorporateUsage(test_certs),
              keymint_bridge_->placeholder_keys().size());

  } else {
    EXPECT_EQ(CountCorporateUsage(test_certs),
              keymaster_bridge_->placeholder_keys().size());
  }

  // Verify |test_certs| and |installed_certs_| have matching elements.
  ASSERT_EQ(test_certs.size(), installed_certs_.size());
  for (size_t i = 0; i < installed_certs_.size(); ++i)
    EXPECT_EQ(test_certs[i], installed_certs_[i].test_data);

  for (const auto& cert_name : service->get_required_cert_names()) {
    bool found = false;
    // Check the required cert is installed.
    ASSERT_TRUE(installer_->certs().count(cert_name));
    for (const auto& cert : installed_certs_) {
      // Check the required cert is one of the installed test certificates.
      const net::ScopedCERTCertificate& nss_cert = cert.nss_cert;

      // Skip until |cert| corresponds to the current |cert_name|.
      if (GetDerCert64(nss_cert.get()) != installer_->certs()[cert_name])
        continue;

      // Check nickname.
      EXPECT_EQ(x509_certificate_model::GetCertNameOrNickname(nss_cert.get()),
                cert_name);
      found = true;
      std::string cert_id = installer_->cert_ids()[cert_name];
      // Check CKA_ID and slot.
      int slot_id;
      std::string hex_encoded_id = base::HexEncode(cert_id);
      EXPECT_EQ(hex_encoded_id,
                ash::NetworkCertLoader::GetPkcs11IdAndSlotForCert(
                    nss_cert.get(), &slot_id));
      EXPECT_TRUE(PlaceholdersContainIdAndSlot(cert_id, cert.test_data.slot));
      break;
    }
    // Check the required cert was found.
    EXPECT_TRUE(found);
  }
}

Profile* CertStoreServiceTest::profile() {
  return ash::ProfileHelper::Get()->GetProfileByAccountId(
      affiliation_mixin_.account_id());
}

void CertStoreServiceTest::SetUpTestClientCerts(
    const std::vector<TestCertData>& certs_to_setup,
    base::OnceClosure done_callback,
    net::NSSCertDatabase* cert_db) {
  for (const auto& test_data : certs_to_setup) {
    base::ScopedAllowBlockingForTesting allow_io;
    net::ImportSensitiveKeyFromFile(
        net::GetTestCertsDirectory(), test_data.file_name + ".pk8",
        test_data.slot == keymanagement::mojom::ChapsSlot::kUser
            ? cert_db->GetPrivateSlot().get()
            : cert_db->GetSystemSlot().get());
    net::ScopedCERTCertificateList certs =
        net::CreateCERTCertificateListFromFile(
            net::GetTestCertsDirectory(), test_data.file_name + ".pem",
            net::X509Certificate::FORMAT_AUTO);
    ASSERT_EQ(1U, certs.size());

    installed_certs_.emplace_back(
        test_data, net::x509_util::DupCERTCertificate(certs[0].get()));
  }
  std::move(done_callback).Run();
}

void CertStoreServiceTest::ImportCert(CERTCertificate* const nss_cert,
                                      base::OnceClosure done_callback,
                                      net::NSSCertDatabase* cert_db) {
  // Import user certificate properly how it's done in PlatformKeys.
  cert_db->ImportUserCert(nss_cert);
  std::move(done_callback).Run();
}

bool CertStoreServiceTest::PlaceholdersContainIdAndSlot(
    const std::string& id,
    keymanagement::mojom::ChapsSlot slot) {
  if (test_data_.should_use_arc_keymint) {
    for (const auto& key : keymint_bridge_->placeholder_keys()) {
      if (key->key_data->is_chaps_key_data() &&
          key->key_data->get_chaps_key_data()->id == id &&
          key->key_data->get_chaps_key_data()->slot == slot) {
        return true;
      }
    }
  } else {
    for (const auto& key : keymaster_bridge_->placeholder_keys()) {
      if (key->key_data->is_chaps_key_data() &&
          key->key_data->get_chaps_key_data()->id == id &&
          key->key_data->get_chaps_key_data()->slot == slot) {
        return true;
      }
    }
  }
  return false;
}

void CertStoreServiceTest::SetUpTestSystemSlot() {
  test_system_slot_ = std::make_unique<crypto::ScopedTestSystemNSSKeySlot>(
      /*simulate_token_loader=*/false);
  ASSERT_TRUE(test_system_slot_->ConstructedSuccessfully());
}

void CertStoreServiceTest::TearDownTestSystemSlot() {
  if (!test_system_slot_)
    return;

  base::RunLoop loop;
  content::GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&CertStoreServiceTest::TearDownTestSystemSlotOnIO,
                     base::Unretained(this)),
      loop.QuitClosure());
  loop.Run();
}

void CertStoreServiceTest::TearDownTestSystemSlotOnIO() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  test_system_slot_.reset();
}

IN_PROC_BROWSER_TEST_P(CertStoreServiceTest, PRE_HandlesCorporateUsageCerts) {
  policy::AffiliationTestHelper::PreLoginUser(affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_P(CertStoreServiceTest, HandlesCorporateUsageCerts) {
  CertStoreService* service =
      CertStoreServiceFactory::GetForBrowserContext(profile());
  ASSERT_TRUE(service);

  // Install all certs from parameter at once.
  ASSERT_NO_FATAL_FAILURE(SetUpCerts(test_data_.certs));

  // Verify all certs are installed correctly.
  ASSERT_NO_FATAL_FAILURE(CheckInstalledCerts(test_data_.certs, service));
}

IN_PROC_BROWSER_TEST_P(CertStoreServiceTest,
                       PRE_InstallsAndDeletesCorporateUsageCerts) {
  policy::AffiliationTestHelper::PreLoginUser(affiliation_mixin_.account_id());
}

IN_PROC_BROWSER_TEST_P(CertStoreServiceTest,
                       InstallsAndDeletesCorporateUsageCerts) {
  CertStoreService* service =
      CertStoreServiceFactory::GetForBrowserContext(profile());
  ASSERT_TRUE(service);

  // Install certs from parameter one by one.
  for (size_t i = 0; i < test_data_.certs.size(); ++i) {
    ASSERT_NO_FATAL_FAILURE(SetUpCerts({test_data_.certs[i]}));

    // Verify only the first (i+1) certs are installed so far.
    ASSERT_NO_FATAL_FAILURE(CheckInstalledCerts(
        std::vector<TestCertData>(test_data_.certs.begin(),
                                  test_data_.certs.begin() + i + 1),
        service));
  }

  // Uninstall certs from parameter one by one, from last to first.
  for (size_t i = test_data_.certs.size(); i--;) {
    DeleteCert(installed_certs_.back().nss_cert.get());

    // Verify only the first i certs are left after the uninstall.
    ASSERT_NO_FATAL_FAILURE(CheckInstalledCerts(
        std::vector<TestCertData>(test_data_.certs.begin(),
                                  test_data_.certs.begin() + i),
        service));
  }
}

INSTANTIATE_TEST_SUITE_P(
    CertStoreTests,
    CertStoreServiceTest,
    ::testing::Values(
        // No corporate usage keys.
        CertStoreServiceTestData{
            std::vector<TestCertData>{
                TestCertData(kFileName1,
                             false /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kUser),
                TestCertData(kFileName2,
                             false /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kUser)},
            true /* should_use_arc_keymint */},
        CertStoreServiceTestData{
            std::vector<TestCertData>{
                TestCertData(kFileName1,
                             false /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kUser),
                TestCertData(kFileName2,
                             false /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kUser)},
            false /* should_use_arc_keymint */},
        // Corporate usage keys in user slot.
        CertStoreServiceTestData{
            std::vector<TestCertData>{
                TestCertData(kFileName1,
                             true /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kUser),
                TestCertData(kFileName2,
                             true /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kUser)},
            true /* should_use_arc_keymint */},
        CertStoreServiceTestData{
            std::vector<TestCertData>{
                TestCertData(kFileName1,
                             true /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kUser),
                TestCertData(kFileName2,
                             true /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kUser)},
            false /* should_use_arc_keymint */},
        // Corporate usage keys in system slot.
        CertStoreServiceTestData{
            std::vector<TestCertData>{
                TestCertData(kFileName1,
                             true /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kSystem),
                TestCertData(kFileName2,
                             true /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kSystem)},
            true /* should_use_arc_keymint */},
        CertStoreServiceTestData{
            std::vector<TestCertData>{
                TestCertData(kFileName1,
                             true /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kSystem),
                TestCertData(kFileName2,
                             true /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kSystem)},
            false /* should_use_arc_keymint */},
        // Corporate usage keys in both slots.
        CertStoreServiceTestData{
            std::vector<TestCertData>{
                TestCertData(kFileName1,
                             true /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kUser),
                TestCertData(kFileName2,
                             true /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kSystem),
                TestCertData(kFileName3,
                             false /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kUser),
                TestCertData(kFileName4,
                             true /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kSystem)},
            true /* should_use_arc_keymint */},
        CertStoreServiceTestData{
            std::vector<TestCertData>{
                TestCertData(kFileName1,
                             true /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kUser),
                TestCertData(kFileName2,
                             true /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kSystem),
                TestCertData(kFileName3,
                             false /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kUser),
                TestCertData(kFileName4,
                             true /* is_corporate_usage */,
                             keymanagement::mojom::ChapsSlot::kSystem)},
            false /* should_use_arc_keymint */}));

}  // namespace arc

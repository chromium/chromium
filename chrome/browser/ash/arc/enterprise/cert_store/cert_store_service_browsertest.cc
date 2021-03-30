// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/arc/enterprise/cert_store/cert_store_service.h"
#include "chrome/browser/ash/arc/keymaster/arc_keymaster_bridge.h"
#include "chrome/browser/ash/arc/session/arc_service_launcher.h"
#include "chrome/browser/ash/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/extension_key_permissions_service.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/extension_key_permissions_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/chromeos/policy/user_policy_test_helper.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "chrome/common/pref_names.h"
#include "chrome/services/keymaster/public/mojom/cert_store.mojom.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/network/network_cert_loader.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "extensions/browser/extension_system.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

constexpr char kFakeUserName[] = "test@example.com";
constexpr char kFakeExtensionId[] = "fakeextensionid";

const std::vector<std::string> kCertFiles = {"client_1", "client_2"};

std::string GetDerCert64(CERTCertificate* cert) {
  std::string der_cert;
  EXPECT_TRUE(net::x509_util::GetDEREncoded(cert, &der_cert));
  std::string der_cert64;
  base::Base64Encode(der_cert, &der_cert64);
  return der_cert64;
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
    for (const auto& cert : certs) {
      certs_[x509_certificate_model::GetCertNameOrNickname(
          cert.nss_cert.get())] = GetDerCert64(cert.nss_cert.get());
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

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  std::map<std::string, std::string> certs_;
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

std::unique_ptr<KeyedService> BuildFakeArcKeymasterBridge(
    content::BrowserContext* profile) {
  return std::make_unique<FakeArcKeymasterBridge>(profile);
}

std::unique_ptr<KeyedService> BuildCertStoreService(
    std::unique_ptr<FakeArcCertInstaller> installer,
    content::BrowserContext* profile) {
  return std::make_unique<CertStoreService>(profile, std::move(installer));
}

}  // namespace

class CertStoreServiceTest : public MixinBasedInProcessBrowserTest {
 protected:
  // MixinBasedInProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);

    arc::SetArcAvailableCommandLineForTesting(command_line);

    policy_helper_ = std::make_unique<policy::UserPolicyTestHelper>(
        kFakeUserName, &local_policy_server_);
    policy_helper_->SetPolicy(
        base::DictionaryValue() /* empty mandatory policy */,
        base::DictionaryValue() /* empty recommended policy */);

    command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                    kFakeUserName);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
    // Don't require policy for our sessions - this is required because
    // this test creates a secondary profile synchronously, so we need to
    // let the policy code know not to expect cached policy.
    command_line->AppendSwitchASCII(chromeos::switches::kProfileRequiresPolicy,
                                    "false");

    // Tell the policy subsystem to wait for an initial policy load, even
    // though we are using a synchronously loaded profile.
    // TODO(edmanp): `Update this test to properly use an asynchronously loaded
    // user profile and remove the use of this flag (crbug.com/795737).
    command_line->AppendSwitchASCII(
        chromeos::switches::kWaitForInitialPolicyFetchForTest, "true");
  }

  void SetUp() override {
    chromeos::platform_keys::PlatformKeysServiceFactory::GetInstance()
        ->SetTestingMode(true);

    MixinBasedInProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    policy_helper_->WaitForInitialPolicy(browser()->profile());

    // Init ArcSessionManager for testing.
    ArcServiceLauncher::Get()->ResetForTesting();

    chromeos::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(true);

    browser()->profile()->GetPrefs()->SetBoolean(prefs::kArcSignedIn, true);
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted,
                                                 true);

    ArcKeymasterBridge::GetFactory()->SetTestingFactoryAndUse(
        browser()->profile(),
        base::BindRepeating(&BuildFakeArcKeymasterBridge));
    auto* keymaster_bridge =
        ArcKeymasterBridge::GetForBrowserContext(browser()->profile());
    keymaster_bridge_ = static_cast<FakeArcKeymasterBridge*>(keymaster_bridge);

    auto installer = std::make_unique<FakeArcCertInstaller>(
        browser()->profile(), std::make_unique<policy::RemoteCommandsQueue>());
    installer_ = installer.get();
    CertStoreService::GetFactory()->SetTestingFactoryAndUse(
        browser()->profile(),
        base::BindRepeating(&BuildCertStoreService,
                            base::Passed(std::move(installer))));

    ArcServiceLauncher::Get()->OnPrimaryUserProfilePrepared(
        browser()->profile());
  }

  void TearDownOnMainThread() override {
    // Since ArcServiceLauncher is (re-)set up with profile() in
    // SetUpOnMainThread() it is necessary to Shutdown() before the profile()
    // is destroyed. ArcServiceLauncher::Shutdown() will be called again on
    // fixture destruction (because it is initialized with the original Profile
    // instance in fixture, once), but it should be no op.
    ArcServiceLauncher::Get()->Shutdown();
    chromeos::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(false);
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  void TearDown() override {
    MixinBasedInProcessBrowserTest::TearDown();

    chromeos::platform_keys::PlatformKeysServiceFactory::GetInstance()
        ->SetTestingMode(false);
  }

  void RegisterCorporateKey(CERTCertificate* cert) {
    chromeos::platform_keys::KeyPermissionsService* const
        key_permissions_service =
            chromeos::platform_keys::KeyPermissionsServiceFactory::
                GetForBrowserContext(browser()->profile());

    ASSERT_TRUE(key_permissions_service);

    {
      base::RunLoop run_loop;
      chromeos::platform_keys::ExtensionKeyPermissionsServiceFactory::
          GetForBrowserContextAndExtension(
              base::BindOnce(&CertStoreServiceTest::GotPermissionsForExtension,
                             base::Unretained(this), cert,
                             run_loop.QuitClosure()),
              browser()->profile(), kFakeExtensionId, key_permissions_service);
      run_loop.Run();
    }
  }

  void SetUpCerts(const std::vector<std::string>& keys_file_names,
                  bool is_corporate_usage_key) {
    // Read certs from files.
    base::RunLoop loop;
    GetNSSCertDatabaseForProfile(
        browser()->profile(),
        base::BindOnce(&CertStoreServiceTest::SetUpTestClientCerts,
                       base::Unretained(this), keys_file_names,
                       loop.QuitClosure()));
    loop.Run();
    // Register certs for corporate usage if needed.
    for (auto& cert : client_certs_) {
      // Certificates must be imported.
      ASSERT_TRUE(cert);
      if (is_corporate_usage_key)
        RegisterCorporateKey(cert.get());
    }

    // Import certs into database.
    {
      base::RunLoop loop;
      GetNSSCertDatabaseForProfile(
          browser()->profile(),
          base::BindOnce(&CertStoreServiceTest::ImportTestClientCerts,
                         base::Unretained(this), loop.QuitClosure()));
      loop.Run();
    }
  }

  void DeleteCert(CERTCertificate* cert) {
    base::RunLoop loop;
    GetNSSCertDatabaseForProfile(
        browser()->profile(),
        base::BindOnce(&CertStoreServiceTest::DeleteCertAndKey,
                       base::Unretained(this), cert, loop.QuitClosure()));
    loop.Run();
  }

  bool PlaceholdersContainId(const std::string& id) {
    for (const auto& key : keymaster_bridge()->placeholder_keys()) {
      if (key->key_data->is_chaps_key_data() &&
          key->key_data->get_chaps_key_data()->id == id) {
        return true;
      }
    }
    return false;
  }

  void CheckInstalledCerts(size_t installed_cert_num,
                           CertStoreService* service) {
    EXPECT_EQ(installed_cert_num, client_certs_.size());
    EXPECT_EQ(installed_cert_num, installer()->certs().size());
    EXPECT_EQ(installed_cert_num, service->get_required_cert_names().size());
    EXPECT_EQ(installed_cert_num,
              keymaster_bridge()->placeholder_keys().size());

    for (const auto& cert_name : service->get_required_cert_names()) {
      bool found = false;
      // Check the required cert is installed.
      ASSERT_TRUE(installer()->certs().count(cert_name));
      for (const auto& cert : client_certs_) {
        // Check the required cert is one of the imported for the test
        // certificates.
        if (GetDerCert64(cert.get()) == installer()->certs()[cert_name]) {
          // Check nickname.
          EXPECT_EQ(x509_certificate_model::GetCertNameOrNickname(cert.get()),
                    cert_name);
          found = true;
          // Check KeyInfo.
          auto key_info =
              service->GetKeyInfoForDummySpki(installer()->certs()[cert_name]);
          EXPECT_TRUE(key_info.has_value());
          EXPECT_EQ(key_info.value().nickname, cert_name);
          int slot_id;
          // Check CKA_ID.
          std::string hex_encoded_id = base::HexEncode(
              key_info.value().id.data(), key_info.value().id.size());
          EXPECT_EQ(hex_encoded_id,
                    chromeos::NetworkCertLoader::GetPkcs11IdAndSlotForCert(
                        cert.get(), &slot_id));
          EXPECT_TRUE(PlaceholdersContainId(key_info.value().id));
          break;
        }
      }
      // Check the required cert was found.
      EXPECT_TRUE(found);
    }
  }

  FakeArcCertInstaller* installer() { return installer_; }

  FakeArcKeymasterBridge* keymaster_bridge() { return keymaster_bridge_; }

  net::ScopedCERTCertificateList client_certs_;

 private:
  void OnKeyRegisteredForCorporateUsage(
      std::unique_ptr<chromeos::platform_keys::ExtensionKeyPermissionsService>
          extension_key_permissions_service,
      base::OnceClosure done_callback,
      chromeos::platform_keys::Status status) {
    ASSERT_EQ(status, chromeos::platform_keys::Status::kSuccess);
    std::move(done_callback).Run();
  }

  // Register only client_cert1_ for corporate usage to test that
  // client_cert2_ is not allowed.
  void GotPermissionsForExtension(
      CERTCertificate* cert,
      base::OnceClosure done_callback,
      std::unique_ptr<chromeos::platform_keys::ExtensionKeyPermissionsService>
          extension_key_permissions_service) {
    auto* extension_key_permissions_service_unowned =
        extension_key_permissions_service.get();
    std::string client_cert_spki(
        cert->derPublicKey.data,
        cert->derPublicKey.data + cert->derPublicKey.len);
    extension_key_permissions_service_unowned->RegisterKeyForCorporateUsage(
        client_cert_spki,
        base::BindOnce(&CertStoreServiceTest::OnKeyRegisteredForCorporateUsage,
                       base::Unretained(this),
                       std::move(extension_key_permissions_service),
                       std::move(done_callback)));
  }

  void SetUpTestClientCerts(const std::vector<std::string>& key_file_names,
                            base::OnceClosure done_callback,
                            net::NSSCertDatabase* cert_db) {
    for (const auto& file_name : key_file_names) {
      base::ScopedAllowBlockingForTesting allow_io;
      net::ImportSensitiveKeyFromFile(net::GetTestCertsDirectory(),
                                      file_name + ".pk8",
                                      cert_db->GetPrivateSlot().get());
      net::ScopedCERTCertificateList certs =
          net::CreateCERTCertificateListFromFile(
              net::GetTestCertsDirectory(), file_name + ".pem",
              net::X509Certificate::FORMAT_AUTO);
      EXPECT_EQ(1U, certs.size());
      if (certs.size() != 1U) {
        std::move(done_callback).Run();
        return;
      }

      client_certs_.emplace_back(
          net::x509_util::DupCERTCertificate(certs[0].get()));
    }
    std::move(done_callback).Run();
  }

  void ImportTestClientCerts(base::OnceClosure done_callback,
                             net::NSSCertDatabase* cert_db) {
    for (const auto& cert : client_certs_) {
      // Import user certificate properly how it's done in PlatfromKeys.
      cert_db->ImportUserCert(cert.get());
    }
    std::move(done_callback).Run();
  }

  void DeleteCertAndKey(CERTCertificate* cert,
                        base::OnceClosure done_callback,
                        net::NSSCertDatabase* cert_db) {
    base::ScopedAllowBlockingForTesting allow_io;
    EXPECT_TRUE(cert_db->DeleteCertAndKey(cert));
    std::move(done_callback).Run();
  }

  std::unique_ptr<policy::UserPolicyTestHelper> policy_helper_;
  chromeos::LocalPolicyTestServerMixin local_policy_server_{&mixin_host_};

  // Owned by service.
  FakeArcCertInstaller* installer_;
  FakeArcKeymasterBridge* keymaster_bridge_;
};

// Test no corporate usage keys.
IN_PROC_BROWSER_TEST_F(CertStoreServiceTest, Basic) {
  CertStoreService* service =
      CertStoreService::GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(service);

  // Import 2 certs into DB. No corporate usage keys.
  ASSERT_NO_FATAL_FAILURE(
      SetUpCerts(kCertFiles, false /* is_corporate_usage */));
  installer()->Wait();
  installer()->RunCompletionCallback(true /* success */);

  EXPECT_EQ(kCertFiles.size(), client_certs_.size());
  // No corporate usage keys installed.
  EXPECT_TRUE(installer()->certs().empty());
  EXPECT_TRUE(service->get_required_cert_names().empty());
  EXPECT_TRUE(keymaster_bridge()->placeholder_keys().empty());
}

// Test installation of 2 corporate usage keys.
IN_PROC_BROWSER_TEST_F(CertStoreServiceTest, InstalledCorporateUsageKeys) {
  CertStoreService* service =
      CertStoreService::GetForBrowserContext(browser()->profile());
  EXPECT_EQ(browser()->profile(),
            Profile::FromBrowserContext(browser()->profile()));
  ASSERT_TRUE(service);

  size_t installed_cert_num = 0;

  // Import and register corporate certificates.
  for (const auto& file : kCertFiles) {
    ASSERT_NO_FATAL_FAILURE(
        SetUpCerts({file}, true /* is_corporate_usage_key */));
    installer()->Wait();
    installer()->RunCompletionCallback(true /* success */);

    installed_cert_num++;
    CheckInstalledCerts(installed_cert_num, service);
  }
}

// Test uninstallation of 2 corporate usage keys.
IN_PROC_BROWSER_TEST_F(CertStoreServiceTest, UninstalledCorporateUsageKeys) {
  CertStoreService* service =
      CertStoreService::GetForBrowserContext(browser()->profile());
  EXPECT_EQ(browser()->profile(),
            Profile::FromBrowserContext(browser()->profile()));
  ASSERT_TRUE(service);

  installer()->Wait();
  installer()->RunCompletionCallback(true /* success */);

  CheckInstalledCerts(0, service);
  ASSERT_NO_FATAL_FAILURE(
      SetUpCerts({kCertFiles}, true /* is_corporate_usage_key */));
  installer()->Wait();
  installer()->RunCompletionCallback(true /* success */);
  CheckInstalledCerts(kCertFiles.size(), service);

  size_t installed_cert_num = kCertFiles.size();
  while (!client_certs_.empty()) {
    DeleteCert(client_certs_.back().get());
    installer()->Wait();
    installer()->RunCompletionCallback(true /* success */);

    client_certs_.pop_back();
    installed_cert_num--;
    CheckInstalledCerts(installed_cert_num, service);
  }
}

}  // namespace arc

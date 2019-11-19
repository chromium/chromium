// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/arc/enterprise/cert_store/arc_cert_store_bridge.h"
#include "chrome/browser/chromeos/arc/session/arc_service_launcher.h"
#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/chromeos/policy/user_policy_test_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/mojom/cert_store.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "extensions/browser/extension_system.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kFakeUserName[] = "test@example.com";
constexpr char kFakePackageName[] = "fake.package.name";
constexpr char kFakeExtensionId[] = "fakeextensionid";

// Returns true if cert1 < cert2.
bool CertificatePtrLess(const arc::mojom::CertificatePtr& cert1,
                        const arc::mojom::CertificatePtr& cert2) {
  if (!cert1 || !cert2)
    return !cert2.is_null();
  return std::tie(cert1->alias, cert1->cert) <
         std::tie(cert2->alias, cert2->cert);
}

}  // namespace

namespace arc {

class FakeArcCertStoreInstance : public mojom::CertStoreInstance {
 public:
  // mojom::CertStoreInstance:
  void InitDeprecated(mojom::CertStoreHostPtr host) override {
    Init(std::move(host), base::DoNothing());
  }

  void Init(mojom::CertStoreHostPtr host, InitCallback callback) override {
    host_ = std::move(host);
    std::move(callback).Run();
  }

  void OnKeyPermissionsChanged(
      const std::vector<std::string>& permissions) override {
    permissions_ = permissions;
  }

  void OnCertificatesChanged() override { is_on_certs_changed_called_ = true; }

  const std::vector<std::string>& permissions() const { return permissions_; }
  bool is_on_certs_changed_called() const {
    return is_on_certs_changed_called_;
  }
  void clear_on_certs_changed() { is_on_certs_changed_called_ = false; }

 private:
  mojom::CertStoreHostPtr host_;
  std::vector<std::string> permissions_;
  bool is_on_certs_changed_called_ = false;
};

class ArcCertStoreBridgeTest : public MixinBasedInProcessBrowserTest {
 protected:
  ArcCertStoreBridgeTest() = default;

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
    // TODO(edmanp): Update this test to properly use an asynchronously loaded
    // user profile and remove the use of this flag (crbug.com/795737).
    command_line->AppendSwitchASCII(
        chromeos::switches::kWaitForInitialPolicyFetchForTest, "true");
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

    ArcServiceLauncher::Get()->OnPrimaryUserProfilePrepared(
        browser()->profile());

    instance_ = std::make_unique<FakeArcCertStoreInstance>();
    ASSERT_TRUE(arc_bridge());
    arc_bridge()->cert_store()->SetInstance(instance_.get());
    WaitForInstanceReady(arc_bridge()->cert_store());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(arc_bridge());
    arc_bridge()->cert_store()->CloseInstance(instance_.get());
    instance_.reset();

    // Since ArcServiceLauncher is (re-)set up with profile() in
    // SetUpOnMainThread() it is necessary to Shutdown() before the profile()
    // is destroyed. ArcServiceLauncher::Shutdown() will be called again on
    // fixture destruction (because it is initialized with the original Profile
    // instance in fixture, once), but it should be no op.
    ArcServiceLauncher::Get()->Shutdown();
    chromeos::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(false);
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  ArcBridgeService* arc_bridge() {
    return ArcServiceManager::Get()->arc_bridge_service();
  }

  FakeArcCertStoreInstance* instance() { return instance_.get(); }

  // Set up the test policy that gives app the permission to access
  // corporate keys.
  void SetCorporateKeyUsagePolicy(const std::string& app_id) {
    base::DictionaryValue key_permissions_policy;
    {
      std::unique_ptr<base::DictionaryValue> cert_key_permission =
          std::make_unique<base::DictionaryValue>();
      cert_key_permission->SetKey("allowCorporateKeyUsage", base::Value(true));
      key_permissions_policy.SetWithoutPathExpansion(
          app_id, std::move(cert_key_permission));
    }

    std::string key_permissions_policy_str;
    base::JSONWriter::WriteWithOptions(key_permissions_policy,
                                       base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                       &key_permissions_policy_str);

    base::DictionaryValue user_policy;
    user_policy.SetKey(policy::key::kKeyPermissions,
                       base::Value(key_permissions_policy_str));

    policy_helper_->SetPolicyAndWait(
        user_policy, base::DictionaryValue() /* empty recommended policy */,
        browser()->profile());
  }

  void RegisterCorporateKeys() {
    ASSERT_NO_FATAL_FAILURE(ImportCerts());

    policy::ProfilePolicyConnector* const policy_connector =
        browser()->profile()->GetProfilePolicyConnector();

    extensions::StateStore* const state_store =
        extensions::ExtensionSystem::Get(browser()->profile())->state_store();

    chromeos::KeyPermissions permissions(
        policy_connector->IsManaged(), browser()->profile()->GetPrefs(),
        policy_connector->policy_service(), state_store);

    {
      base::RunLoop run_loop;
      permissions.GetPermissionsForExtension(
          kFakeExtensionId,
          base::Bind(&ArcCertStoreBridgeTest::GotPermissionsForExtension,
                     base::Unretained(this), run_loop.QuitClosure()));
      run_loop.Run();
    }
  }

  std::vector<mojom::CertificatePtr> ListCertificates(
      ArcCertStoreBridge* cert_store_bridge) {
    std::vector<mojom::CertificatePtr> result;
    base::RunLoop loop;
    cert_store_bridge->ListCertificates(base::BindOnce(
        [](base::RunLoop* loop, std::vector<mojom::CertificatePtr>* result,
           std::vector<mojom::CertificatePtr> certs) {
          std::sort(certs.begin(), certs.end(), CertificatePtrLess);
          *result = std::move(certs);
          loop->Quit();
        },
        &loop, &result));
    loop.Run();

    return result;
  }

  // Imports certificates to NSS database.
  // FATAL ERROR: if the certificates were not imported properly.
  void ImportCerts() {
    base::RunLoop loop;
    GetNSSCertDatabaseForProfile(
        browser()->profile(),
        base::Bind(&ArcCertStoreBridgeTest::SetUpTestClientCerts,
                   base::Unretained(this), loop.QuitClosure()));
    loop.Run();
    // Certificates must be imported.
    ASSERT_NE(nullptr, client_cert1_);
  }

  net::ScopedCERTCertificate client_cert1_;
  net::ScopedCERTCertificate client_cert2_;

 private:
  // Register only client_cert1_ for corporate usage to test that
  // client_cert2_ is not allowed.
  void GotPermissionsForExtension(
      const base::Closure& done_callback,
      std::unique_ptr<chromeos::KeyPermissions::PermissionsForExtension>
          permissions_for_ext) {
    std::string client_cert1_spki(
        client_cert1_->derPublicKey.data,
        client_cert1_->derPublicKey.data + client_cert1_->derPublicKey.len);
    permissions_for_ext->RegisterKeyForCorporateUsage(
        client_cert1_spki, {chromeos::KeyPermissions::KeyLocation::kUserSlot});
    done_callback.Run();
  }

  void SetUpTestClientCerts(const base::Closure& done_callback,
                            net::NSSCertDatabase* cert_db) {
    base::ScopedAllowBlockingForTesting allow_io;
    net::ImportSensitiveKeyFromFile(net::GetTestCertsDirectory(),
                                    "client_1.pk8",
                                    cert_db->GetPrivateSlot().get());
    net::ScopedCERTCertificateList certs =
        net::CreateCERTCertificateListFromFile(
            net::GetTestCertsDirectory(), "client_1.pem",
            net::X509Certificate::FORMAT_AUTO);
    EXPECT_EQ(1U, certs.size());
    if (certs.size() != 1U) {
      done_callback.Run();
      return;
    }

    client_cert1_ = net::x509_util::DupCERTCertificate(certs[0].get());

    // Import user certificate properly how it's done in PlatfromKeys.
    cert_db->ImportUserCert(client_cert1_.get());

    net::ImportClientCertAndKeyFromFile(
        net::GetTestCertsDirectory(), "client_2.pem", "client_2.pk8",
        cert_db->GetPrivateSlot().get(), &client_cert2_);

    done_callback.Run();
  }

  std::unique_ptr<policy::UserPolicyTestHelper> policy_helper_;
  std::unique_ptr<FakeArcCertStoreInstance> instance_;
  chromeos::LocalPolicyTestServerMixin local_policy_server_{&mixin_host_};

  DISALLOW_COPY_AND_ASSIGN(ArcCertStoreBridgeTest);
};

// Test OnKeyPermissionsChanged().
IN_PROC_BROWSER_TEST_F(ArcCertStoreBridgeTest, KeyPermissionsTest) {
  ArcCertStoreBridge* cert_store_bridge =
      ArcCertStoreBridge::GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(cert_store_bridge);
  // Corporate usage keys are not allowed to any app/extension:
  EXPECT_EQ(0U, instance()->permissions().size());
  EXPECT_FALSE(instance()->is_on_certs_changed_called());

  // Allow corporate usage keys to ARC app.
  SetCorporateKeyUsagePolicy(kFakePackageName);

  ASSERT_EQ(1U, instance()->permissions().size());
  EXPECT_EQ(kFakePackageName, instance()->permissions()[0]);

  EXPECT_TRUE(instance()->is_on_certs_changed_called());
  instance()->clear_on_certs_changed();

  // Allow corporate usage keys only to Chrome extensions.
  SetCorporateKeyUsagePolicy(kFakeExtensionId);

  EXPECT_EQ(0U, instance()->permissions().size());
  EXPECT_FALSE(instance()->is_on_certs_changed_called());
}

// Test ListCertificates() with no corporate usage keys.
IN_PROC_BROWSER_TEST_F(ArcCertStoreBridgeTest, ListCertificatesBasicTest) {
  ArcCertStoreBridge* cert_store_bridge =
      ArcCertStoreBridge::GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(cert_store_bridge);

  // Import certificates.
  ASSERT_NO_FATAL_FAILURE(ImportCerts());

  // Allow corporate usage keys to ARC app.
  SetCorporateKeyUsagePolicy(kFakePackageName);

  EXPECT_TRUE(ListCertificates(cert_store_bridge).empty());
}

// Test ListCertificates() with 2 corporate usage keys.
IN_PROC_BROWSER_TEST_F(ArcCertStoreBridgeTest, ListCertificatesTest) {
  ArcCertStoreBridge* cert_store_bridge =
      ArcCertStoreBridge::GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(cert_store_bridge);

  // Import and register corporate certificates.
  ASSERT_NO_FATAL_FAILURE(RegisterCorporateKeys());
  EXPECT_FALSE(instance()->is_on_certs_changed_called());

  // No ARC app is allowed to use corporate usage keys.
  EXPECT_TRUE(ListCertificates(cert_store_bridge).empty());

  // Allow corporate usage keys to ARC app.
  SetCorporateKeyUsagePolicy(kFakePackageName);
  auto mojom_cert1 = mojom::Certificate::New();
  mojom_cert1->alias = client_cert1_->nickname;
  auto x509_cert = net::x509_util::CreateX509CertificateFromCERTCertificate(
      client_cert1_.get());
  net::X509Certificate::GetPEMEncoded(x509_cert->cert_buffer(),
                                      &mojom_cert1->cert);

  std::vector<mojom::CertificatePtr> expected_certs;
  expected_certs.emplace_back(std::move(mojom_cert1));

  const std::vector<mojom::CertificatePtr>& certs =
      ListCertificates(cert_store_bridge);

  ASSERT_EQ(expected_certs.size(), certs.size());
  for (size_t i = 0; i < certs.size(); ++i) {
    EXPECT_EQ(expected_certs[i]->alias, certs[i]->alias);
    EXPECT_EQ(expected_certs[i]->cert, certs[i]->cert);
  }
}

IN_PROC_BROWSER_TEST_F(ArcCertStoreBridgeTest, OnCertificatesChangedTest) {
  ArcCertStoreBridge* cert_store_bridge =
      ArcCertStoreBridge::GetForBrowserContext(browser()->profile());
  ASSERT_TRUE(cert_store_bridge);

  // Allow corporate usage keys to ARC app.
  SetCorporateKeyUsagePolicy(kFakePackageName);
  instance()->clear_on_certs_changed();

  // Import and register corporate certificates.
  ASSERT_NO_FATAL_FAILURE(RegisterCorporateKeys());

  EXPECT_TRUE(instance()->is_on_certs_changed_called());
}

}  // namespace arc

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cryptohi.h>
#include <pk11pub.h>

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_tags.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/extension_key_permissions_service.h"
#include "chrome/browser/chromeos/platform_keys/extension_key_permissions_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/extensions/api/platform_keys/platform_keys_api.h"
#include "chrome/browser/extensions/api/platform_keys/platform_keys_test_base.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"

namespace {

constexpr char kExtensionId[] = "knldjmfmopnpolahpmmgbagdohdnhkik";

using ContextType = extensions::ExtensionBrowserTest::ContextType;

class PlatformKeysTest : public PlatformKeysTestBase {
 public:
  enum class UserClientCertSlot { kPrivateSlot, kPublicSlot };

  PlatformKeysTest(EnrollmentStatus enrollment_status,
                   UserStatus user_status,
                   bool key_permission_policy,
                   UserClientCertSlot user_client_cert_slot,
                   ContextType context_type)
      : PlatformKeysTestBase(SystemTokenStatus::EXISTS,
                             enrollment_status,
                             user_status),
        key_permission_policy_(key_permission_policy),
        user_client_cert_slot_(user_client_cert_slot),
        context_type_(context_type) {
    // Most tests require this to be true. Those that don't can reset
    // it to false if necessary. This is always reset in the destructor.
    extensions::PlatformKeysInternalSelectClientCertificatesFunction::
        SetSkipInteractiveCheckForTest(true);
  }

  ~PlatformKeysTest() override {
    extensions::PlatformKeysInternalSelectClientCertificatesFunction::
        SetSkipInteractiveCheckForTest(false);
  }

  PlatformKeysTest(const PlatformKeysTest&) = delete;
  PlatformKeysTest& operator=(const PlatformKeysTest&) = delete;

  void SetUpOnMainThread() override {
    if (ash::features::IsCopyClientKeysCertsToChapsEnabled() &&
        (user_client_cert_slot_ == UserClientCertSlot::kPublicSlot)) {
      // There's an active effort to deprecate the public slot. Some components
      // (e.g. Kcer) don't take it into account, which breaks tests, but they
      // also don't have to consider it because with the
      // CopyClientKeysCertsToChaps feature enabled all the necessary data is
      // automatically copied from the public slot into the private slot.
      GTEST_SKIP();
    }

    base::AddTagToTestResult("feature_id",
                             "screenplay-63f95a00-bff8-4d81-9cf9-ccf5fdacbef0");
    if (!IsPreTest()) {
      // Set up the private slot before
      // |PlatformKeysTestBase::SetUpOnMainThread| triggers the user sign-in.
      ASSERT_TRUE(user_private_slot_db_.is_open());
      base::RunLoop loop;
      content::GetIOThreadTaskRunner({})->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&PlatformKeysTest::SetPrivateSoftwareSlotOnIO,
                         base::Unretained(this),
                         crypto::ScopedPK11Slot(
                             PK11_ReferenceSlot(user_private_slot_db_.slot()))),
          loop.QuitClosure());
      loop.Run();
    }

    PlatformKeysTestBase::SetUpOnMainThread();

    if (IsPreTest())
      return;

    {
      base::RunLoop loop;
      NssServiceFactory::GetForContext(profile())
          ->UnsafelyGetNSSCertDatabaseForTesting(
              base::BindOnce(&PlatformKeysTest::SetupTestCerts,
                             base::Unretained(this), loop.QuitClosure()));
      loop.Run();
    }

    if (user_status() != UserStatus::UNMANAGED && key_permission_policy_)
      SetupKeyPermissionUserPolicy();
  }

  void SetupKeyPermissionUserPolicy() {
    policy::PolicyMap policy;

    // Set up the test policy that gives |extension_| the permission to access
    // corporate keys.
    base::Value::Dict key_permissions_policy;
    {
      base::Value::Dict cert1_key_permission;
      cert1_key_permission.Set("allowCorporateKeyUsage", true);
      key_permissions_policy.Set(kExtensionId, std::move(cert1_key_permission));
    }

    policy.Set(policy::key::kKeyPermissions, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(key_permissions_policy)), nullptr);

    mock_policy_provider()->UpdateChromePolicy(policy);
  }

  chromeos::ExtensionPlatformKeysService* GetExtensionPlatformKeysService() {
    return chromeos::ExtensionPlatformKeysServiceFactory::GetForBrowserContext(
        profile());
  }

  bool RunPlatformKeysTest(const char* test_suite_name) {
    // By default, the system token is not available.
    const char* system_token_availability = "false";

    // Only if the current user is of the same domain as the device is enrolled
    // to, the system token is available to the extension.
    if (system_token_status() == SystemTokenStatus::EXISTS &&
        enrollment_status() == EnrollmentStatus::ENROLLED &&
        user_status() == UserStatus::MANAGED_AFFILIATED_DOMAIN) {
      system_token_availability = "true";
    }

    // The test gets configuration values from the custom arg.
    const std::string custom_arg = base::StringPrintf(
        R"({ "testSuiteName": "%s", "systemTokenEnabled": %s })",
        test_suite_name, system_token_availability);
    return RunExtensionTest("platform_keys", {.custom_arg = custom_arg.c_str()},
                            {.context_type = context_type_});
  }

  void RegisterClient1AsCorporateKey() {
    const extensions::Extension* const fake_gen_extension =
        LoadExtension(test_data_dir_.AppendASCII("platform_keys_genkey"));

    base::RunLoop run_loop;
    chromeos::platform_keys::ExtensionKeyPermissionsServiceFactory::
        GetForBrowserContextAndExtension(
            base::BindOnce(&PlatformKeysTest::GotPermissionsForExtension,
                           base::Unretained(this), run_loop.QuitClosure()),
            profile(), fake_gen_extension->id());
    run_loop.Run();
  }

 protected:
  // Imported into user's private or public slot, depending on the value of
  // |user_client_cert_slot_|.
  scoped_refptr<net::X509Certificate> client_cert1_;
  // Imported into system slot.
  scoped_refptr<net::X509Certificate> client_cert2_;
  // Signed using an elliptic curve (ECDSA) algorithm.
  // Imported in the same slot as |client_cert1_|.
  scoped_refptr<net::X509Certificate> client_cert3_;

 private:
  base::FilePath extension_path() const {
    return test_data_dir_.AppendASCII("platform_keys");
  }

  void SetPrivateSoftwareSlotOnIO(crypto::ScopedPK11Slot slot) {
    crypto::SetPrivateSoftwareSlotForChromeOSUserForTesting(std::move(slot));
  }

  void OnKeyRegisteredForCorporateUsage(
      std::unique_ptr<chromeos::platform_keys::ExtensionKeyPermissionsService>
          extension_key_permissions_service,
      base::OnceClosure done_callback,
      bool is_error,
      crosapi::mojom::KeystoreError error) {
    ASSERT_FALSE(is_error) << static_cast<int>(error);
    std::move(done_callback).Run();
  }

  void GotPermissionsForExtension(
      base::OnceClosure done_callback,
      std::unique_ptr<chromeos::platform_keys::ExtensionKeyPermissionsService>
          extension_key_permissions_service) {
    auto* extension_key_permissions_service_unowned =
        extension_key_permissions_service.get();
    extension_key_permissions_service_unowned->RegisterKeyForCorporateUsage(
        chromeos::platform_keys::GetSubjectPublicKeyInfoBlob(client_cert1_),
        base::BindOnce(&PlatformKeysTest::OnKeyRegisteredForCorporateUsage,
                       base::Unretained(this),
                       std::move(extension_key_permissions_service),
                       std::move(done_callback)));
  }

  void SetupTestCerts(base::OnceClosure done_callback,
                      net::NSSCertDatabase* cert_db) {
    SetupTestClientCerts(cert_db);
    SetupTestCACerts();
    std::move(done_callback).Run();
  }

  void SetupTestClientCerts(net::NSSCertDatabase* cert_db) {
    // Sanity check to ensure that
    // SetPrivateSoftwareSlotForChromeOSUserForTesting took effect.
    EXPECT_EQ(user_private_slot_db_.slot(), cert_db->GetPrivateSlot().get());
    EXPECT_NE(cert_db->GetPrivateSlot().get(), cert_db->GetPublicSlot().get());

    crypto::ScopedPK11Slot slot =
        user_client_cert_slot_ == UserClientCertSlot::kPrivateSlot
            ? cert_db->GetPrivateSlot()
            : cert_db->GetPublicSlot();
    client_cert1_ = net::ImportClientCertAndKeyFromFile(
        extension_path(), "client_1.pem", "client_1.pk8", slot.get());
    ASSERT_TRUE(client_cert1_.get());

    // Import a second client cert signed by another CA than client_1 into the
    // system wide key slot.
    client_cert2_ = net::ImportClientCertAndKeyFromFile(
        extension_path(), "client_2.pem", "client_2.pk8",
        test_system_slot()->slot());
    ASSERT_TRUE(client_cert2_.get());

    client_cert3_ = net::ImportClientCertAndKeyFromFile(
        extension_path(), "client_3.pem", "client_3.pk8", slot.get());
    ASSERT_TRUE(client_cert3_.get());

    // The main important observer for these tests is Kcer.
    net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
  }

  void SetupTestCACerts() {
    // "root.pem" is the issuer of the "l1_leaf.pem" and (transitively)
    // "l1_leaf.pem" certs which are loaded on the JS side. Generated by
    // create_test_certs.sh .
    scoped_refptr<net::X509Certificate> root =
        net::ImportCertFromFile(extension_path().AppendASCII("root.pem"));
    ASSERT_TRUE(root);
    scoped_test_root_.Reset({root});
  }

  const bool key_permission_policy_;
  const UserClientCertSlot user_client_cert_slot_;
  crypto::ScopedTestNSSDB user_private_slot_db_;
  const ContextType context_type_;
  net::ScopedTestRoot scoped_test_root_;
};

class TestSelectDelegate
    : public chromeos::ExtensionPlatformKeysService::SelectDelegate {
 public:
  // On each Select call, selects the next entry in |certs_to_select| from back
  // to front. Once the first entry is reached, that one will be selected
  // repeatedly.
  // Entries of |certs_to_select| can be null in which case no certificate will
  // be selected.
  // If |certs_to_select| is empty, any invocation of |Select| will fail.
  explicit TestSelectDelegate(net::CertificateList certs_to_select)
      : certs_to_select_(certs_to_select) {}

  ~TestSelectDelegate() override {}

  void Select(const std::string& extension_id,
              const net::CertificateList& certs,
              CertificateSelectedCallback callback,
              content::WebContents* web_contents,
              content::BrowserContext* context) override {
    ASSERT_TRUE(context);
    ASSERT_FALSE(certs_to_select_.empty());
    scoped_refptr<net::X509Certificate> selection;
    if (certs_to_select_.back()) {
      for (scoped_refptr<net::X509Certificate> cert : certs) {
        if (cert->EqualsExcludingChain(certs_to_select_.back().get())) {
          selection = cert;
          break;
        }
      }
    }
    if (certs_to_select_.size() > 1)
      certs_to_select_.pop_back();
    std::move(callback).Run(selection);
  }

 private:
  net::CertificateList certs_to_select_;
};

struct UnmanagedPlatformKeysTestParams {
  UnmanagedPlatformKeysTestParams(
      PlatformKeysTestBase::EnrollmentStatus enrollment_status,
      PlatformKeysTest::UserClientCertSlot user_client_cert_slot,
      ContextType context_type)
      : enrollment_status_(enrollment_status),
        user_client_cert_slot_(user_client_cert_slot),
        context_type_(context_type) {}

  PlatformKeysTestBase::EnrollmentStatus enrollment_status_;
  PlatformKeysTest::UserClientCertSlot user_client_cert_slot_;
  ContextType context_type_;
};

class UnmanagedPlatformKeysTest
    : public PlatformKeysTest,
      public ::testing::WithParamInterface<UnmanagedPlatformKeysTestParams> {
 public:
  UnmanagedPlatformKeysTest()
      : PlatformKeysTest(GetParam().enrollment_status_,
                         UserStatus::UNMANAGED,
                         false /* unused */,
                         GetParam().user_client_cert_slot_,
                         GetParam().context_type_) {}
};

struct ManagedPlatformKeysTestParams {
  ManagedPlatformKeysTestParams(
      PlatformKeysTestBase::EnrollmentStatus enrollment_status,
      PlatformKeysTestBase::UserStatus user_status,
      ContextType context_type)
      : enrollment_status_(enrollment_status),
        user_status_(user_status),
        context_type_(context_type) {}

  PlatformKeysTestBase::EnrollmentStatus enrollment_status_;
  PlatformKeysTestBase::UserStatus user_status_;
  ContextType context_type_;
};

class ManagedWithPermissionPlatformKeysTest
    : public PlatformKeysTest,
      public ::testing::WithParamInterface<ManagedPlatformKeysTestParams> {
 public:
  ManagedWithPermissionPlatformKeysTest()
      : PlatformKeysTest(GetParam().enrollment_status_,
                         GetParam().user_status_,
                         true /* grant the extension key permission */,
                         UserClientCertSlot::kPrivateSlot,
                         GetParam().context_type_) {}
};

class ManagedWithoutPermissionPlatformKeysTest
    : public PlatformKeysTest,
      public ::testing::WithParamInterface<ManagedPlatformKeysTestParams> {
 public:
  ManagedWithoutPermissionPlatformKeysTest()
      : PlatformKeysTest(GetParam().enrollment_status_,
                         GetParam().user_status_,
                         false /* do not grant key permission */,
                         UserClientCertSlot::kPrivateSlot,
                         GetParam().context_type_) {}
};

}  // namespace

IN_PROC_BROWSER_TEST_P(UnmanagedPlatformKeysTest, PRE_Basic) {
  RunPreTest();
}

// At first interactively selects |client_cert1_|, |client_cert2_| and
// |client_cert3_| to grant permissions and afterwards runs more basic tests.
// After the initial two interactive calls, the simulated user does not select
// any cert.
IN_PROC_BROWSER_TEST_P(UnmanagedPlatformKeysTest, Basic) {
  net::CertificateList certs;
  certs.push_back(nullptr);
  certs.push_back(client_cert3_);
  certs.push_back(client_cert2_);
  certs.push_back(client_cert1_);

  GetExtensionPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(certs));

  ASSERT_TRUE(RunPlatformKeysTest("basicTests")) << message_;
}

IN_PROC_BROWSER_TEST_P(UnmanagedPlatformKeysTest,
                       PRE_BackgroundInteractiveTest) {
  RunPreTest();
}

// Tests that interactive calls are not allowed from the extension's
// background page. This test is simple and requires no certs or any
// particular setup.
IN_PROC_BROWSER_TEST_P(UnmanagedPlatformKeysTest, BackgroundInteractiveTest) {
  // This needs to be set to false, since we're testing the actual error.
  extensions::PlatformKeysInternalSelectClientCertificatesFunction::
      SetSkipInteractiveCheckForTest(false);
  net::CertificateList certs;
  certs.push_back(nullptr);

  GetExtensionPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(certs));
  ASSERT_TRUE(RunPlatformKeysTest("backgroundInteractiveTest")) << message_;
}

IN_PROC_BROWSER_TEST_P(UnmanagedPlatformKeysTest, PRE_Permissions) {
  RunPreTest();
}

// On interactive calls, the simulated user always selects |client_cert1_| if
// matching.
IN_PROC_BROWSER_TEST_P(UnmanagedPlatformKeysTest, Permissions) {
  net::CertificateList certs;
  certs.push_back(client_cert1_);

  GetExtensionPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(certs));

  ASSERT_TRUE(RunPlatformKeysTest("permissionTests")) << message_;
}

INSTANTIATE_TEST_SUITE_P(
    PersistentBackground,
    UnmanagedPlatformKeysTest,
    ::testing::Values(UnmanagedPlatformKeysTestParams(
                          PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
                          PlatformKeysTest::UserClientCertSlot::kPrivateSlot,
                          ContextType::kPersistentBackground),
                      UnmanagedPlatformKeysTestParams(
                          PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
                          PlatformKeysTest::UserClientCertSlot::kPrivateSlot,
                          ContextType::kPersistentBackground),
                      UnmanagedPlatformKeysTestParams(
                          PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
                          PlatformKeysTest::UserClientCertSlot::kPublicSlot,
                          ContextType::kPersistentBackground),
                      UnmanagedPlatformKeysTestParams(
                          PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
                          PlatformKeysTest::UserClientCertSlot::kPublicSlot,
                          ContextType::kPersistentBackground)));

INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    UnmanagedPlatformKeysTest,
    ::testing::Values(UnmanagedPlatformKeysTestParams(
                          PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
                          PlatformKeysTest::UserClientCertSlot::kPrivateSlot,
                          ContextType::kServiceWorker),
                      UnmanagedPlatformKeysTestParams(
                          PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
                          PlatformKeysTest::UserClientCertSlot::kPrivateSlot,
                          ContextType::kServiceWorker),
                      UnmanagedPlatformKeysTestParams(
                          PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
                          PlatformKeysTest::UserClientCertSlot::kPublicSlot,
                          ContextType::kServiceWorker),
                      UnmanagedPlatformKeysTestParams(
                          PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
                          PlatformKeysTest::UserClientCertSlot::kPublicSlot,
                          ContextType::kServiceWorker)));

IN_PROC_BROWSER_TEST_P(ManagedWithoutPermissionPlatformKeysTest,
                       PRE_UserPermissionsBlocked) {
  RunPreTest();
}

IN_PROC_BROWSER_TEST_P(ManagedWithoutPermissionPlatformKeysTest,
                       UserPermissionsBlocked) {
  // To verify that the user is not prompted for any certificate selection,
  // set up a delegate that fails on any invocation.
  GetExtensionPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(net::CertificateList()));

  ASSERT_TRUE(RunPlatformKeysTest("managedProfile")) << message_;
}

IN_PROC_BROWSER_TEST_P(ManagedWithoutPermissionPlatformKeysTest,
                       PRE_CorporateKeyAccessBlocked) {
  RunPreTest();
}

// A corporate key must not be useable if there is no policy permitting it.
IN_PROC_BROWSER_TEST_P(ManagedWithoutPermissionPlatformKeysTest,
                       CorporateKeyAccessBlocked) {
  RegisterClient1AsCorporateKey();

  // To verify that the user is not prompted for any certificate selection,
  // set up a delegate that fails on any invocation.
  GetExtensionPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(net::CertificateList()));

  ASSERT_TRUE(RunPlatformKeysTest("corporateKeyWithoutPermissionTests"))
      << message_;
}

INSTANTIATE_TEST_SUITE_P(
    PersistentBackground,
    ManagedWithoutPermissionPlatformKeysTest,
    ::testing::Values(
        ManagedPlatformKeysTestParams(
            PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
            PlatformKeysTestBase::UserStatus::MANAGED_AFFILIATED_DOMAIN,
            ContextType::kPersistentBackground),
        ManagedPlatformKeysTestParams(
            PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
            PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN,
            ContextType::kPersistentBackground),
        ManagedPlatformKeysTestParams(
            PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
            PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN,
            ContextType::kPersistentBackground)));

INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    ManagedWithoutPermissionPlatformKeysTest,
    ::testing::Values(
        ManagedPlatformKeysTestParams(
            PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
            PlatformKeysTestBase::UserStatus::MANAGED_AFFILIATED_DOMAIN,
            ContextType::kServiceWorker),
        ManagedPlatformKeysTestParams(
            PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
            PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN,
            ContextType::kServiceWorker),
        ManagedPlatformKeysTestParams(
            PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
            PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN,
            ContextType::kServiceWorker)));

IN_PROC_BROWSER_TEST_P(ManagedWithPermissionPlatformKeysTest,
                       PRE_PolicyGrantsAccessToCorporateKey) {
  RunPreTest();
}

IN_PROC_BROWSER_TEST_P(ManagedWithPermissionPlatformKeysTest,
                       PolicyGrantsAccessToCorporateKey) {
  RegisterClient1AsCorporateKey();

  // Set up the test SelectDelegate to select |client_cert1_| if available for
  // selection.
  net::CertificateList certs;
  certs.push_back(client_cert1_);

  GetExtensionPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(certs));

  ASSERT_TRUE(RunPlatformKeysTest("corporateKeyWithPermissionTests"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ManagedWithPermissionPlatformKeysTest,
                       PRE_PolicyDoesGrantAccessToNonCorporateKey) {
  RunPreTest();
}

IN_PROC_BROWSER_TEST_P(ManagedWithPermissionPlatformKeysTest,
                       PolicyDoesGrantAccessToNonCorporateKey) {
  // The policy grants access to corporate keys.
  // As the profile is managed, the user must not be able to grant any
  // certificate permission.
  // If the user is not affilited, no corporate keys are available. Set up a
  // delegate that fails on any invocation. If the user is affiliated, client_2
  // on the system token will be avialable for selection, as it is implicitly
  // corporate.
  net::CertificateList certs;
  if (user_status() == UserStatus::MANAGED_AFFILIATED_DOMAIN)
    certs.push_back(nullptr);

  GetExtensionPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(certs));

  ASSERT_TRUE(RunPlatformKeysTest("policyDoesGrantAccessToNonCorporateKey"))
      << message_;
}

INSTANTIATE_TEST_SUITE_P(
    PersistentBackground,
    ManagedWithPermissionPlatformKeysTest,
    ::testing::Values(
        ManagedPlatformKeysTestParams(
            PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
            PlatformKeysTestBase::UserStatus::MANAGED_AFFILIATED_DOMAIN,
            ContextType::kPersistentBackground),
        ManagedPlatformKeysTestParams(
            PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
            PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN,
            ContextType::kPersistentBackground),
        ManagedPlatformKeysTestParams(
            PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
            PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN,
            ContextType::kPersistentBackground)));

INSTANTIATE_TEST_SUITE_P(
    ServiceWorker,
    ManagedWithPermissionPlatformKeysTest,
    ::testing::Values(
        ManagedPlatformKeysTestParams(
            PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
            PlatformKeysTestBase::UserStatus::MANAGED_AFFILIATED_DOMAIN,
            ContextType::kServiceWorker),
        ManagedPlatformKeysTestParams(
            PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
            PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN,
            ContextType::kServiceWorker),
        ManagedPlatformKeysTestParams(
            PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
            PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN,
            ContextType::kServiceWorker)));

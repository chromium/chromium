// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cryptohi.h>
#include <pk11pub.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/extensions/api/platform_keys/platform_keys_test_base.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"

namespace {

class PlatformKeysTest : public PlatformKeysTestBase {
 public:
  enum class UserClientCertSlot { kPrivateSlot, kPublicSlot };

  PlatformKeysTest(EnrollmentStatus enrollment_status,
                   UserStatus user_status,
                   bool key_permission_policy,
                   UserClientCertSlot user_client_cert_slot)
      : PlatformKeysTestBase(SystemTokenStatus::EXISTS,
                             enrollment_status,
                             user_status),
        key_permission_policy_(key_permission_policy),
        user_client_cert_slot_(user_client_cert_slot) {}

  void SetUpOnMainThread() override {
    if (!IsPreTest()) {
      // Set up the private slot before
      // |PlatformKeysTestBase::SetUpOnMainThread| triggers the user sign-in.
      ASSERT_TRUE(user_private_slot_db_.is_open());
      base::RunLoop loop;
      base::PostTaskAndReply(
          FROM_HERE, {content::BrowserThread::IO},
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
      GetNSSCertDatabaseForProfile(
          profile(),
          base::BindRepeating(&PlatformKeysTest::SetupTestCerts,
                              base::Unretained(this), loop.QuitClosure()));
      loop.Run();
    }

    base::FilePath extension_path = test_data_dir_.AppendASCII("platform_keys");
    extension_ = LoadExtension(extension_path);

    if (user_status() != UserStatus::UNMANAGED && key_permission_policy_)
      SetupKeyPermissionUserPolicy();
  }

  void SetupKeyPermissionUserPolicy() {
    policy::PolicyMap policy;

    // Set up the test policy that gives |extension_| the permission to access
    // corporate keys.
    std::unique_ptr<base::DictionaryValue> key_permissions_policy =
        std::make_unique<base::DictionaryValue>();
    {
      std::unique_ptr<base::DictionaryValue> cert1_key_permission(
          new base::DictionaryValue);
      cert1_key_permission->SetKey("allowCorporateKeyUsage", base::Value(true));
      key_permissions_policy->SetWithoutPathExpansion(
          extension_->id(), std::move(cert1_key_permission));
    }

    policy.Set(policy::key::kKeyPermissions, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               std::move(key_permissions_policy), nullptr);

    mock_policy_provider()->UpdateChromePolicy(policy);
  }

  chromeos::PlatformKeysService* GetPlatformKeysService() {
    return chromeos::PlatformKeysServiceFactory::GetForBrowserContext(
        profile());
  }

  bool RunExtensionTest(const std::string& test_suite_name) {
    // By default, the system token is not available.
    std::string system_token_availability;

    // Only if the current user is of the same domain as the device is enrolled
    // to, the system token is available to the extension.
    if (system_token_status() == SystemTokenStatus::EXISTS &&
        enrollment_status() == EnrollmentStatus::ENROLLED &&
        user_status() == UserStatus::MANAGED_AFFILIATED_DOMAIN) {
      system_token_availability = "systemTokenEnabled";
    }

    GURL url = extension_->GetResourceURL(base::StringPrintf(
        "basic.html?%s#%s", system_token_availability.c_str(),
        test_suite_name.c_str()));
    return TestExtension(url.spec());
  }

  void RegisterClient1AsCorporateKey() {
    const extensions::Extension* const fake_gen_extension =
        LoadExtension(test_data_dir_.AppendASCII("platform_keys_genkey"));

    policy::ProfilePolicyConnector* const policy_connector =
        profile()->GetProfilePolicyConnector();

    extensions::StateStore* const state_store =
        extensions::ExtensionSystem::Get(profile())->state_store();

    chromeos::KeyPermissions permissions(
        policy_connector->IsManaged(), profile()->GetPrefs(),
        policy_connector->policy_service(), state_store);

    base::RunLoop run_loop;
    permissions.GetPermissionsForExtension(
        fake_gen_extension->id(),
        base::Bind(&PlatformKeysTest::GotPermissionsForExtension,
                   base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

 protected:
  // Imported into user's private or public slot, depending on the value of
  // |user_client_cert_slot_|.
  scoped_refptr<net::X509Certificate> client_cert1_;
  // Imported into system slot.
  scoped_refptr<net::X509Certificate> client_cert2_;
  const extensions::Extension* extension_;

 private:
  base::FilePath extension_path() const {
    return test_data_dir_.AppendASCII("platform_keys");
  }

  void SetPrivateSoftwareSlotOnIO(crypto::ScopedPK11Slot slot) {
    crypto::SetPrivateSoftwareSlotForChromeOSUserForTesting(std::move(slot));
  }

  void GotPermissionsForExtension(
      const base::Closure& done_callback,
      std::unique_ptr<chromeos::KeyPermissions::PermissionsForExtension>
          permissions_for_ext) {
    std::string client_cert1_spki =
        chromeos::platform_keys::GetSubjectPublicKeyInfo(client_cert1_);
    permissions_for_ext->RegisterKeyForCorporateUsage(
        client_cert1_spki, {chromeos::KeyPermissions::KeyLocation::kUserSlot});
    done_callback.Run();
  }

  void SetupTestCerts(const base::Closure& done_callback,
                      net::NSSCertDatabase* cert_db) {
    SetupTestClientCerts(cert_db);
    SetupTestCACerts();
    done_callback.Run();
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
  }

  void SetupTestCACerts() {
    net::TestRootCerts* root_certs = net::TestRootCerts::GetInstance();
    // "root.pem" is the issuer of the "l1_leaf.pem" and (transitively)
    // "l1_leaf.pem" certs which are loaded on the JS side. Generated by
    // create_test_certs.sh .
    root_certs->AddFromFile(extension_path().AppendASCII("root.pem"));
  }

  const bool key_permission_policy_;
  const UserClientCertSlot user_client_cert_slot_;
  crypto::ScopedTestNSSDB user_private_slot_db_;

  DISALLOW_COPY_AND_ASSIGN(PlatformKeysTest);
};

class TestSelectDelegate
    : public chromeos::PlatformKeysService::SelectDelegate {
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
              const CertificateSelectedCallback& callback,
              content::WebContents* web_contents,
              content::BrowserContext* context) override {
    ASSERT_TRUE(web_contents);
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
    callback.Run(selection);
  }

 private:
  net::CertificateList certs_to_select_;
};

struct UnmanagedPlatformKeysTestParams {
  UnmanagedPlatformKeysTestParams(
      PlatformKeysTestBase::EnrollmentStatus enrollment_status,
      PlatformKeysTest::UserClientCertSlot user_client_cert_slot)
      : enrollment_status_(enrollment_status),
        user_client_cert_slot_(user_client_cert_slot) {}

  PlatformKeysTestBase::EnrollmentStatus enrollment_status_;
  PlatformKeysTest::UserClientCertSlot user_client_cert_slot_;
};

class UnmanagedPlatformKeysTest
    : public PlatformKeysTest,
      public ::testing::WithParamInterface<UnmanagedPlatformKeysTestParams> {
 public:
  UnmanagedPlatformKeysTest()
      : PlatformKeysTest(GetParam().enrollment_status_,
                         UserStatus::UNMANAGED,
                         false /* unused */,
                         GetParam().user_client_cert_slot_) {}
};

struct ManagedPlatformKeysTestParams {
  ManagedPlatformKeysTestParams(
      PlatformKeysTestBase::EnrollmentStatus enrollment_status,
      PlatformKeysTestBase::UserStatus user_status)
      : enrollment_status_(enrollment_status), user_status_(user_status) {}

  PlatformKeysTestBase::EnrollmentStatus enrollment_status_;
  PlatformKeysTestBase::UserStatus user_status_;
};

class ManagedWithPermissionPlatformKeysTest
    : public PlatformKeysTest,
      public ::testing::WithParamInterface<ManagedPlatformKeysTestParams> {
 public:
  ManagedWithPermissionPlatformKeysTest()
      : PlatformKeysTest(GetParam().enrollment_status_,
                         GetParam().user_status_,
                         true /* grant the extension key permission */,
                         UserClientCertSlot::kPrivateSlot) {}
};

class ManagedWithoutPermissionPlatformKeysTest
    : public PlatformKeysTest,
      public ::testing::WithParamInterface<ManagedPlatformKeysTestParams> {
 public:
  ManagedWithoutPermissionPlatformKeysTest()
      : PlatformKeysTest(GetParam().enrollment_status_,
                         GetParam().user_status_,
                         false /* do not grant key permission */,
                         UserClientCertSlot::kPrivateSlot) {}
};

}  // namespace

IN_PROC_BROWSER_TEST_P(UnmanagedPlatformKeysTest, PRE_Basic) {
  RunPreTest();
}

// At first interactively selects |client_cert1_| and |client_cert2_| to grant
// permissions and afterwards runs more basic tests.
// After the initial two interactive calls, the simulated user does not select
// any cert.
IN_PROC_BROWSER_TEST_P(UnmanagedPlatformKeysTest, Basic) {
  net::CertificateList certs;
  certs.push_back(nullptr);
  certs.push_back(client_cert2_);
  certs.push_back(client_cert1_);

  GetPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(certs));

  ASSERT_TRUE(RunExtensionTest("basicTests")) << message_;
}

IN_PROC_BROWSER_TEST_P(UnmanagedPlatformKeysTest, PRE_Permissions) {
  RunPreTest();
}

// On interactive calls, the simulated user always selects |client_cert1_| if
// matching.
IN_PROC_BROWSER_TEST_P(UnmanagedPlatformKeysTest, Permissions) {
  net::CertificateList certs;
  certs.push_back(client_cert1_);

  GetPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(certs));

  ASSERT_TRUE(RunExtensionTest("permissionTests")) << message_;
}

INSTANTIATE_TEST_SUITE_P(
    Unmanaged,
    UnmanagedPlatformKeysTest,
    ::testing::Values(UnmanagedPlatformKeysTestParams(
                          PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
                          PlatformKeysTest::UserClientCertSlot::kPrivateSlot),
                      UnmanagedPlatformKeysTestParams(
                          PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
                          PlatformKeysTest::UserClientCertSlot::kPrivateSlot),
                      UnmanagedPlatformKeysTestParams(
                          PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
                          PlatformKeysTest::UserClientCertSlot::kPublicSlot),
                      UnmanagedPlatformKeysTestParams(
                          PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
                          PlatformKeysTest::UserClientCertSlot::kPublicSlot)));

IN_PROC_BROWSER_TEST_P(ManagedWithoutPermissionPlatformKeysTest,
                       PRE_UserPermissionsBlocked) {
  RunPreTest();
}

IN_PROC_BROWSER_TEST_P(ManagedWithoutPermissionPlatformKeysTest,
                       UserPermissionsBlocked) {
  // To verify that the user is not prompted for any certificate selection,
  // set up a delegate that fails on any invocation.
  GetPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(net::CertificateList()));

  ASSERT_TRUE(RunExtensionTest("managedProfile")) << message_;
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
  GetPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(net::CertificateList()));

  ASSERT_TRUE(RunExtensionTest("corporateKeyWithoutPermissionTests"))
      << message_;
}

INSTANTIATE_TEST_SUITE_P(
    ManagedWithoutPermission,
    ManagedWithoutPermissionPlatformKeysTest,
    ::testing::Values(
        ManagedPlatformKeysTestParams(
            PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
            PlatformKeysTestBase::UserStatus::MANAGED_AFFILIATED_DOMAIN),
        ManagedPlatformKeysTestParams(
            PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
            PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN),
        ManagedPlatformKeysTestParams(
            PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
            PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN)));

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

  GetPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(certs));

  ASSERT_TRUE(RunExtensionTest("corporateKeyWithPermissionTests")) << message_;
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

  GetPlatformKeysService()->SetSelectDelegate(
      std::make_unique<TestSelectDelegate>(certs));

  ASSERT_TRUE(RunExtensionTest("policyDoesGrantAccessToNonCorporateKey"))
      << message_;
}

INSTANTIATE_TEST_SUITE_P(
    ManagedWithPermission,
    ManagedWithPermissionPlatformKeysTest,
    ::testing::Values(
        ManagedPlatformKeysTestParams(
            PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
            PlatformKeysTestBase::UserStatus::MANAGED_AFFILIATED_DOMAIN),
        ManagedPlatformKeysTestParams(
            PlatformKeysTestBase::EnrollmentStatus::ENROLLED,
            PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN),
        ManagedPlatformKeysTestParams(
            PlatformKeysTestBase::EnrollmentStatus::NOT_ENROLLED,
            PlatformKeysTestBase::UserStatus::MANAGED_OTHER_DOMAIN)));

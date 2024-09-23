// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_manager_model.h"

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "build/chromeos_buildflags.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_util_nss.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/certificate_provider/certificate_provider.h"
#include "chromeos/ash/components/network/policy_certificate_provider.h"
#include "chromeos/components/onc/certificate_scope.h"
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/kcer/extra_instances.h"
#endif

namespace {

// A fake CertificateManagerModel::Observer that has the ability to execute a
// OnceClosure passed to it when |CertificatesRefreshed| is called.
class FakeObserver : public CertificateManagerModel::Observer {
 public:
  void CertificatesRefreshed() override {
    if (!run_on_refresh_.is_null())
      std::move(run_on_refresh_).Run();
  }

  // Execute |closure| on the next |CertificatesRefreshed| invocation.
  void RunOnNextRefresh(base::OnceClosure closure) {
    run_on_refresh_ = std::move(closure);
  }

 private:
  base::OnceClosure run_on_refresh_;
};

// Looks up a |CertInfo| in |org_grouping_map| corresponding to |cert|. Returns
// nullptr if no such |CertInfo| was found.
CertificateManagerModel::CertInfo* GetCertInfoFromOrgGroupingMap(
    const CertificateManagerModel::OrgGroupingMap& org_grouping_map,
    CERTCertificate* cert) {
  for (const auto& org_and_cert_info_list : org_grouping_map) {
    for (const auto& cert_info : org_and_cert_info_list.second) {
      if (net::x509_util::IsSameCertificate(cert_info->cert(), cert))
        return cert_info.get();
    }
  }
  return nullptr;
}

}  // namespace

class CertificateManagerModelTest : public testing::Test {
 public:
  CertificateManagerModelTest() {}

  CertificateManagerModelTest(const CertificateManagerModelTest&) = delete;
  CertificateManagerModelTest& operator=(const CertificateManagerModelTest&) =
      delete;

 protected:
  void SetUp() override {
    ASSERT_TRUE(test_nssdb_.is_open());

    nss_cert_db_ = std::make_unique<net::NSSCertDatabase>(
        crypto::ScopedPK11Slot(
            PK11_ReferenceSlot(test_nssdb_.slot())) /* public slot */,
        crypto::ScopedPK11Slot(
            PK11_ReferenceSlot(test_nssdb_.slot())) /* private slot */);

    fake_observer_ = std::make_unique<FakeObserver>();
    certificate_manager_model_ = std::make_unique<CertificateManagerModel>(
        GetCertificateManagerModelParams(), fake_observer_.get(),
        nss_cert_db_.get());
  }

  void TearDown() override {
    certificate_manager_model_.reset();
    nss_cert_db_.reset();
  }

  // Provides the platform-specific |Params| (containing policy/extension
  // certificate provides on Chrome OS).
  virtual std::unique_ptr<CertificateManagerModel::Params>
  GetCertificateManagerModelParams() {
    return std::make_unique<CertificateManagerModel::Params>();
  }

 protected:
  // Invoke an explicit Refresh if the refresh is triggered and wait until the
  // observer has been notified.
  void WaitForRefresh(bool trigger_refresh) {
    base::RunLoop run_loop;
    fake_observer_->RunOnNextRefresh(run_loop.QuitClosure());
    if (trigger_refresh) {
      certificate_manager_model_->Refresh();
    }
    run_loop.Run();
  }

  content::BrowserTaskEnvironment task_environment_;
  crypto::ScopedTestNSSDB test_nssdb_;
  std::unique_ptr<net::NSSCertDatabase> nss_cert_db_;
  std::unique_ptr<FakeObserver> fake_observer_;
  std::unique_ptr<CertificateManagerModel> certificate_manager_model_;
};

// CertificateManagerModel correctly lists CA certificates from the platform NSS
// Database.
TEST_F(CertificateManagerModelTest, ListsCertsFromPlatform) {
  net::CertificateList orig_certs = CreateCertificateListFromFile(
      net::GetTestCertsDirectory(), "websocket_cacert.pem",
      net::X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, orig_certs.size());

  net::CertBuilder cert_builder(orig_certs[0]->cert_buffer(), nullptr);
  scoped_refptr<net::X509Certificate> x509_cert =
      cert_builder.GetX509Certificate();

  net::ScopedCERTCertificate cert =
      net::x509_util::CreateCERTCertificateFromX509Certificate(x509_cert.get());
  std::string cert_subject_name = x509_cert->subject().GetDisplayName();

  ASSERT_EQ(SECSuccess,
            PK11_ImportCert(test_nssdb_.slot(), cert.get(), CK_INVALID_HANDLE,
                            "cert", PR_FALSE /* includeTrust (unused) */));
  WaitForRefresh(true /*tigger_for_refresh*/);

  {
    CertificateManagerModel::OrgGroupingMap org_grouping_map;
    certificate_manager_model_->FilterAndBuildOrgGroupingMap(
        net::CertType::CA_CERT, &org_grouping_map);
    CertificateManagerModel::CertInfo* cert_info =
        GetCertInfoFromOrgGroupingMap(org_grouping_map, cert.get());
    ASSERT_TRUE(cert_info);

    EXPECT_EQ(net::CertType::CA_CERT, cert_info->type());
    EXPECT_EQ(base::UTF8ToUTF16(cert_subject_name), cert_info->name());
    EXPECT_TRUE(cert_info->can_be_deleted());
    // This platform cert is untrusted because it is self-signed and has no
    // trust bits.
    EXPECT_TRUE(cert_info->untrusted());
    EXPECT_EQ(CertificateManagerModel::CertInfo::Source::kPlatform,
              cert_info->source());
    EXPECT_FALSE(cert_info->web_trust_anchor());
    EXPECT_FALSE(cert_info->hardware_backed());
  }

  certificate_manager_model_->SetCertTrust(cert.get(), net::CertType::CA_CERT,
                                           net::NSSCertDatabase::TRUSTED_SSL);
  // Wait for refresh without triggering because observer should be notified by
  // net::CertDatabase and refresh automatically.
  WaitForRefresh(false /*tigger_for_refresh*/);
  {
    CertificateManagerModel::OrgGroupingMap org_grouping_map;
    certificate_manager_model_->FilterAndBuildOrgGroupingMap(
        net::CertType::CA_CERT, &org_grouping_map);
    CertificateManagerModel::CertInfo* cert_info =
        GetCertInfoFromOrgGroupingMap(org_grouping_map, cert.get());
    ASSERT_TRUE(cert_info);

    EXPECT_FALSE(cert_info->untrusted());
    EXPECT_TRUE(cert_info->web_trust_anchor());
  }
}

// CertificateManagerModel correctly lists client certificates from the platform
// NSS Database.
TEST_F(CertificateManagerModelTest, ListsClientCertsFromPlatform) {
  net::ScopedCERTCertificate platform_client_cert;
  net::ImportClientCertAndKeyFromFile(
      net::GetTestCertsDirectory(), "client_1.pem", "client_1.pk8",
      test_nssdb_.slot(), &platform_client_cert);

  WaitForRefresh(true /*tigger_for_refresh*/);

  CertificateManagerModel::OrgGroupingMap org_grouping_map;
  certificate_manager_model_->FilterAndBuildOrgGroupingMap(
      net::CertType::USER_CERT, &org_grouping_map);
  CertificateManagerModel::CertInfo* platform_cert_info =
      GetCertInfoFromOrgGroupingMap(org_grouping_map,
                                    platform_client_cert.get());
  ASSERT_TRUE(platform_cert_info);

  EXPECT_EQ(net::CertType::USER_CERT, platform_cert_info->type());
  EXPECT_EQ(u"Client Cert A", platform_cert_info->name());
  EXPECT_TRUE(platform_cert_info->can_be_deleted());
  EXPECT_EQ(CertificateManagerModel::CertInfo::Source::kPlatform,
            platform_cert_info->source());
  EXPECT_FALSE(platform_cert_info->web_trust_anchor());
  EXPECT_FALSE(platform_cert_info->hardware_backed());
}

#if BUILDFLAG(IS_CHROMEOS)
namespace {

class FakePolicyCertificateProvider : public ash::PolicyCertificateProvider {
 public:
  void AddPolicyProvidedCertsObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemovePolicyProvidedCertsObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  net::CertificateList GetAllServerAndAuthorityCertificates(
      const chromeos::onc::CertificateScope& scope) const override {
    // The CertificateManagerModel only retrieves profile-wide certificates.
    EXPECT_EQ(chromeos::onc::CertificateScope::Default(), scope);

    net::CertificateList merged;
    merged.insert(merged.end(), web_trusted_certs_.begin(),
                  web_trusted_certs_.end());
    merged.insert(merged.end(), not_web_trusted_certs_.begin(),
                  not_web_trusted_certs_.end());
    return merged;
  }

  net::CertificateList GetAllAuthorityCertificates(
      const chromeos::onc::CertificateScope& scope) const override {
    // This function is not called by CertificateManagerModel.
    NOTREACHED_IN_MIGRATION();
    return net::CertificateList();
  }

  net::CertificateList GetWebTrustedCertificates(
      const chromeos::onc::CertificateScope& scope) const override {
    // The CertificateManagerModel only retrieves profile-wide certificates.
    EXPECT_EQ(chromeos::onc::CertificateScope::Default(), scope);

    return web_trusted_certs_;
  }

  net::CertificateList GetCertificatesWithoutWebTrust(
      const chromeos::onc::CertificateScope& scope) const override {
    // The CertificateManagerModel only retrieves profile-wide certificates.
    EXPECT_EQ(chromeos::onc::CertificateScope::Default(), scope);

    return not_web_trusted_certs_;
  }

  const std::set<std::string>& GetExtensionIdsWithPolicyCertificates()
      const override {
    // This function is not called by CertificateManagerModel.
    NOTREACHED_IN_MIGRATION();
    return kNoExtensions;
  }

  void SetPolicyProvidedCertificates(
      const net::CertificateList& web_trusted_certs,
      const net::CertificateList& not_web_trusted_certs) {
    web_trusted_certs_ = web_trusted_certs;
    not_web_trusted_certs_ = not_web_trusted_certs;
  }

  void NotifyObservers() {
    for (auto& observer : observer_list_)
      observer.OnPolicyProvidedCertsChanged();
  }

 private:
  base::ObserverList<PolicyCertificateProvider::Observer,
                     true /* check_empty */>::Unchecked observer_list_;
  net::CertificateList web_trusted_certs_;
  net::CertificateList not_web_trusted_certs_;
  const std::set<std::string> kNoExtensions = {};
};

class FakeExtensionCertificateProvider : public chromeos::CertificateProvider {
 public:
  FakeExtensionCertificateProvider(
      const net::CertificateList* extension_client_certificates,
      const bool* extensions_hang)
      : extension_client_certificates_(extension_client_certificates),
        extensions_hang_(extensions_hang) {}

  void GetCertificates(
      base::OnceCallback<void(net::ClientCertIdentityList)> callback) override {
    if (*extensions_hang_)
      return;

    std::move(callback).Run(FakeClientCertIdentityListFromCertificateList(
        *extension_client_certificates_));
  }

 private:
  raw_ptr<const net::CertificateList> extension_client_certificates_;

  // If *|extensions_hang| is true, the |FakeExtensionCertificateProvider| hangs
  // - it never calls the callbacks passed to |GetCertificates|.
  raw_ptr<const bool> extensions_hang_;
};

// Looks up a |CertInfo| in |org_grouping_map| corresponding to |cert|. Returns
// nullptr if no such |CertInfo| was found.
CertificateManagerModel::CertInfo* GetCertInfoFromOrgGroupingMap(
    const CertificateManagerModel::OrgGroupingMap& org_grouping_map,
    const net::X509Certificate* cert) {
  for (const auto& org_and_cert_info_list : org_grouping_map) {
    for (const auto& cert_info : org_and_cert_info_list.second) {
      if (net::x509_util::IsSameCertificate(cert_info->cert(), cert))
        return cert_info.get();
    }
  }
  return nullptr;
}

}  // namespace

class CertificateManagerModelChromeOSTest : public CertificateManagerModelTest {
 protected:
  std::unique_ptr<CertificateManagerModel::Params>
  GetCertificateManagerModelParams() override {
    auto params = std::make_unique<CertificateManagerModel::Params>();
    params->policy_certs_provider = &policy_certs_provider_;
    params->extension_certificate_provider =
        std::make_unique<FakeExtensionCertificateProvider>(
            &extension_client_certs_, &extensions_hang_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    params->kcer = kcer::ExtraInstances::GetEmptyKcer();
#endif
    return params;
  }

  void NotifyPolicyObserversAndWaitForRefresh() {
    base::RunLoop run_loop;
    fake_observer_->RunOnNextRefresh(run_loop.QuitClosure());
    policy_certs_provider_.NotifyObservers();
    run_loop.Run();
  }

  // Provider for policy certificates. In a non-test environment, this would
  // usually be the UserNetworkConfigurationUpdater.
  FakePolicyCertificateProvider policy_certs_provider_;

  // List of certificates that will be returned from the
  // FakeExtensionCertificateProvider.
  net::CertificateList extension_client_certs_;
  // If true, the FakeExtensionCertificateProvider hangs.
  bool extensions_hang_ = false;
};

// CertificateManagerModel correctly lists policy-provided certificates with web
// trust.
TEST_F(CertificateManagerModelChromeOSTest, ListsWebTrustedCertsFromPolicy) {
  scoped_refptr<net::X509Certificate> cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "websocket_cacert.pem");
  ASSERT_TRUE(cert.get());
  policy_certs_provider_.SetPolicyProvidedCertificates({cert}, {});

  NotifyPolicyObserversAndWaitForRefresh();

  CertificateManagerModel::OrgGroupingMap org_grouping_map;
  certificate_manager_model_->FilterAndBuildOrgGroupingMap(
      net::CertType::CA_CERT, &org_grouping_map);
  CertificateManagerModel::CertInfo* cert_info =
      GetCertInfoFromOrgGroupingMap(org_grouping_map, cert.get());
  ASSERT_TRUE(cert_info);

  EXPECT_EQ(net::CertType::CA_CERT, cert_info->type());
  EXPECT_EQ(u"pywebsocket", cert_info->name());
  EXPECT_FALSE(cert_info->can_be_deleted());
  EXPECT_FALSE(cert_info->untrusted());
  EXPECT_EQ(CertificateManagerModel::CertInfo::Source::kPolicy,
            cert_info->source());
  EXPECT_TRUE(cert_info->web_trust_anchor());
  EXPECT_FALSE(cert_info->hardware_backed());
}

// CertificateManagerModel correctly lists policy-provided certificates without
// web trust.
TEST_F(CertificateManagerModelChromeOSTest, ListsNotWebTrustedCertsFromPolicy) {
  scoped_refptr<net::X509Certificate> cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "websocket_cacert.pem");
  ASSERT_TRUE(cert.get());
  policy_certs_provider_.SetPolicyProvidedCertificates({}, {cert});

  NotifyPolicyObserversAndWaitForRefresh();

  CertificateManagerModel::OrgGroupingMap org_grouping_map;
  certificate_manager_model_->FilterAndBuildOrgGroupingMap(
      net::CertType::CA_CERT, &org_grouping_map);
  CertificateManagerModel::CertInfo* cert_info =
      GetCertInfoFromOrgGroupingMap(org_grouping_map, cert.get());
  ASSERT_TRUE(cert_info);

  EXPECT_EQ(net::CertType::CA_CERT, cert_info->type());
  EXPECT_EQ(u"pywebsocket", cert_info->name());
  EXPECT_FALSE(cert_info->can_be_deleted());
  EXPECT_FALSE(cert_info->untrusted());
  EXPECT_EQ(CertificateManagerModel::CertInfo::Source::kPolicy,
            cert_info->source());
  EXPECT_FALSE(cert_info->web_trust_anchor());
  EXPECT_FALSE(cert_info->hardware_backed());
}

// CertificateManagerModel correctly lists CA certificates that are in the
// platform NSS database and provided by policy with web trust. The
// policy-provided certificate hides the platform certificate in this case.
TEST_F(CertificateManagerModelChromeOSTest,
       WebTrustedPolicyCertsWinOverPlatformCerts) {
  net::ScopedCERTCertificateList certs = CreateCERTCertificateListFromFile(
      net::GetTestCertsDirectory(), "websocket_cacert.pem",
      net::X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, certs.size());
  CERTCertificate* platform_cert = certs[0].get();
  ASSERT_EQ(SECSuccess, PK11_ImportCert(test_nssdb_.slot(), platform_cert,
                                        CK_INVALID_HANDLE, "cert",
                                        PR_FALSE /* includeTrust (unused) */));

  scoped_refptr<net::X509Certificate> policy_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "websocket_cacert.pem");
  ASSERT_TRUE(policy_cert.get());
  policy_certs_provider_.SetPolicyProvidedCertificates({policy_cert}, {});

  WaitForRefresh(true /*tigger_for_refresh*/);

  {
    CertificateManagerModel::OrgGroupingMap org_grouping_map;
    certificate_manager_model_->FilterAndBuildOrgGroupingMap(
        net::CertType::CA_CERT, &org_grouping_map);
    CertificateManagerModel::CertInfo* platform_cert_info =
        GetCertInfoFromOrgGroupingMap(org_grouping_map, platform_cert);
    ASSERT_TRUE(platform_cert_info);
    CertificateManagerModel::CertInfo* policy_cert_info =
        GetCertInfoFromOrgGroupingMap(org_grouping_map, policy_cert.get());
    ASSERT_TRUE(policy_cert_info);

    EXPECT_EQ(platform_cert_info, policy_cert_info);

    EXPECT_EQ(net::CertType::CA_CERT, policy_cert_info->type());
    EXPECT_EQ(u"pywebsocket", policy_cert_info->name());
    EXPECT_FALSE(policy_cert_info->can_be_deleted());
    EXPECT_FALSE(policy_cert_info->untrusted());
    EXPECT_EQ(CertificateManagerModel::CertInfo::Source::kPolicy,
              policy_cert_info->source());
    EXPECT_TRUE(policy_cert_info->web_trust_anchor());
    EXPECT_FALSE(policy_cert_info->hardware_backed());
  }

  // Remove the cert from policy-provided certs again. The platform certificate
  // should be visible afterwards.
  policy_certs_provider_.SetPolicyProvidedCertificates({}, {});
  NotifyPolicyObserversAndWaitForRefresh();

  {
    CertificateManagerModel::OrgGroupingMap org_grouping_map;
    certificate_manager_model_->FilterAndBuildOrgGroupingMap(
        net::CertType::CA_CERT, &org_grouping_map);
    CertificateManagerModel::CertInfo* platform_cert_info =
        GetCertInfoFromOrgGroupingMap(org_grouping_map, platform_cert);
    ASSERT_TRUE(platform_cert_info);

    EXPECT_EQ(net::CertType::CA_CERT, platform_cert_info->type());
    EXPECT_EQ(u"pywebsocket", platform_cert_info->name());
    EXPECT_TRUE(platform_cert_info->can_be_deleted());
    EXPECT_TRUE(platform_cert_info->untrusted());
    EXPECT_EQ(CertificateManagerModel::CertInfo::Source::kPlatform,
              platform_cert_info->source());
    EXPECT_FALSE(platform_cert_info->web_trust_anchor());
    EXPECT_FALSE(platform_cert_info->hardware_backed());
  }
}

// CertificateManagerModel correctly lists CA certificates that are in the
// platform NSS database and provided by policy without web trust. The platform
// certificate hides the policy-provided certificate in this case.
TEST_F(CertificateManagerModelChromeOSTest,
       PlatformCertsWinOverNotWebTrustedCerts) {
  net::ScopedCERTCertificateList certs = CreateCERTCertificateListFromFile(
      net::GetTestCertsDirectory(), "websocket_cacert.pem",
      net::X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, certs.size());
  CERTCertificate* platform_cert = certs[0].get();
  ASSERT_EQ(SECSuccess, PK11_ImportCert(test_nssdb_.slot(), platform_cert,
                                        CK_INVALID_HANDLE, "cert",
                                        PR_FALSE /* includeTrust (unused) */));

  scoped_refptr<net::X509Certificate> policy_cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "websocket_cacert.pem");
  ASSERT_TRUE(policy_cert.get());
  policy_certs_provider_.SetPolicyProvidedCertificates({}, {policy_cert});

  WaitForRefresh(true /*tigger_for_refresh*/);

  {
    CertificateManagerModel::OrgGroupingMap org_grouping_map;
    certificate_manager_model_->FilterAndBuildOrgGroupingMap(
        net::CertType::CA_CERT, &org_grouping_map);
    CertificateManagerModel::CertInfo* platform_cert_info =
        GetCertInfoFromOrgGroupingMap(org_grouping_map, platform_cert);
    ASSERT_TRUE(platform_cert_info);
    CertificateManagerModel::CertInfo* policy_cert_info =
        GetCertInfoFromOrgGroupingMap(org_grouping_map, policy_cert.get());
    ASSERT_TRUE(policy_cert_info);

    EXPECT_EQ(platform_cert_info, policy_cert_info);

    EXPECT_EQ(net::CertType::CA_CERT, platform_cert_info->type());
    EXPECT_EQ(u"pywebsocket", platform_cert_info->name());
    EXPECT_TRUE(platform_cert_info->can_be_deleted());
    EXPECT_TRUE(platform_cert_info->untrusted());
    EXPECT_EQ(CertificateManagerModel::CertInfo::Source::kPlatform,
              platform_cert_info->source());
    EXPECT_FALSE(platform_cert_info->web_trust_anchor());
    EXPECT_FALSE(platform_cert_info->hardware_backed());
  }

  // Remove the certificate from the platform NSS database. The policy-provided
  // certificate should be visible afterwards.
  base::RunLoop run_loop;
  fake_observer_->RunOnNextRefresh(run_loop.QuitClosure());
  base::test::TestFuture<bool> remove_result;
  certificate_manager_model_->RemoveFromDatabase(
      net::x509_util::DupCERTCertificate(platform_cert),
      remove_result.GetCallback());
  EXPECT_TRUE(remove_result.Get());
  run_loop.Run();

  {
    CertificateManagerModel::OrgGroupingMap org_grouping_map;
    certificate_manager_model_->FilterAndBuildOrgGroupingMap(
        net::CertType::CA_CERT, &org_grouping_map);
    CertificateManagerModel::CertInfo* policy_cert_info =
        GetCertInfoFromOrgGroupingMap(org_grouping_map, policy_cert.get());
    ASSERT_TRUE(policy_cert_info);

    EXPECT_EQ(net::CertType::CA_CERT, policy_cert_info->type());
    EXPECT_EQ(u"pywebsocket", policy_cert_info->name());
    EXPECT_FALSE(policy_cert_info->can_be_deleted());
    EXPECT_FALSE(policy_cert_info->untrusted());
    EXPECT_EQ(CertificateManagerModel::CertInfo::Source::kPolicy,
              policy_cert_info->source());
    EXPECT_FALSE(policy_cert_info->web_trust_anchor());
    EXPECT_FALSE(policy_cert_info->hardware_backed());
  }
}

// When the Extension CertificateProvider hangs (e.g. because an extension is
// not responding), policy and platform certificates are still listed.
TEST_F(CertificateManagerModelChromeOSTest,
       PlatformAndPolicyCertsListedWhenExtensionsHang) {
  extensions_hang_ = true;

  net::ScopedCERTCertificateList certs = CreateCERTCertificateListFromFile(
      net::GetTestCertsDirectory(), "websocket_cacert.pem",
      net::X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, certs.size());
  CERTCertificate* platform_cert = certs[0].get();
  ASSERT_EQ(SECSuccess, PK11_ImportCert(test_nssdb_.slot(), platform_cert,
                                        CK_INVALID_HANDLE, "cert",
                                        PR_FALSE /* includeTrust (unused) */));

  scoped_refptr<net::X509Certificate> policy_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(policy_cert.get());
  policy_certs_provider_.SetPolicyProvidedCertificates({policy_cert}, {});

  WaitForRefresh(true /*tigger_for_refresh*/);

  CertificateManagerModel::OrgGroupingMap org_grouping_map;
  certificate_manager_model_->FilterAndBuildOrgGroupingMap(
      net::CertType::CA_CERT, &org_grouping_map);
  CertificateManagerModel::CertInfo* platform_cert_info =
      GetCertInfoFromOrgGroupingMap(org_grouping_map, platform_cert);
  ASSERT_TRUE(platform_cert_info);
  CertificateManagerModel::CertInfo* policy_cert_info =
      GetCertInfoFromOrgGroupingMap(org_grouping_map, policy_cert.get());
  ASSERT_TRUE(policy_cert_info);

  EXPECT_NE(platform_cert_info, policy_cert_info);
}

// CertificateManagerModel lists client certificates provided by extensions.
TEST_F(CertificateManagerModelChromeOSTest, ListsExtensionCerts) {
  scoped_refptr<net::X509Certificate> extension_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_1.pem");
  ASSERT_TRUE(extension_cert.get());
  extension_client_certs_.push_back(extension_cert);

  WaitForRefresh(true /*tigger_for_refresh*/);

  CertificateManagerModel::OrgGroupingMap org_grouping_map;
  certificate_manager_model_->FilterAndBuildOrgGroupingMap(
      net::CertType::USER_CERT, &org_grouping_map);
  CertificateManagerModel::CertInfo* extension_cert_info =
      GetCertInfoFromOrgGroupingMap(org_grouping_map, extension_cert.get());
  ASSERT_TRUE(extension_cert_info);

  EXPECT_EQ(net::CertType::USER_CERT, extension_cert_info->type());
  EXPECT_EQ(u"Client Cert A (extension provided)", extension_cert_info->name());
  EXPECT_FALSE(extension_cert_info->can_be_deleted());
  EXPECT_EQ(CertificateManagerModel::CertInfo::Source::kExtension,
            extension_cert_info->source());
  EXPECT_FALSE(extension_cert_info->web_trust_anchor());
  EXPECT_FALSE(extension_cert_info->hardware_backed());
}

TEST_F(CertificateManagerModelChromeOSTest,
       PlatformCertsWinOverExtensionCerts) {
  net::ScopedCERTCertificate platform_client_cert;
  net::ImportClientCertAndKeyFromFile(
      net::GetTestCertsDirectory(), "client_1.pem", "client_1.pk8",
      test_nssdb_.slot(), &platform_client_cert);

  scoped_refptr<net::X509Certificate> extension_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_1.pem");
  ASSERT_TRUE(extension_cert.get());
  extension_client_certs_.push_back(extension_cert);

  WaitForRefresh(true /*tigger_for_refresh*/);

  {
    CertificateManagerModel::OrgGroupingMap org_grouping_map;
    certificate_manager_model_->FilterAndBuildOrgGroupingMap(
        net::CertType::USER_CERT, &org_grouping_map);
    CertificateManagerModel::CertInfo* platform_cert_info =
        GetCertInfoFromOrgGroupingMap(org_grouping_map,
                                      platform_client_cert.get());
    ASSERT_TRUE(platform_cert_info);
    CertificateManagerModel::CertInfo* extension_cert_info =
        GetCertInfoFromOrgGroupingMap(org_grouping_map, extension_cert.get());
    ASSERT_TRUE(extension_cert_info);

    EXPECT_EQ(platform_cert_info, extension_cert_info);

    EXPECT_EQ(net::CertType::USER_CERT, platform_cert_info->type());
    EXPECT_EQ(u"Client Cert A", platform_cert_info->name());
    EXPECT_TRUE(platform_cert_info->can_be_deleted());
    EXPECT_EQ(CertificateManagerModel::CertInfo::Source::kPlatform,
              platform_cert_info->source());
    EXPECT_FALSE(platform_cert_info->web_trust_anchor());
    EXPECT_FALSE(platform_cert_info->hardware_backed());
  }

  // Remove the platform client certificate. The extension-provided client
  // certificate should be visible afterwards.
  base::RunLoop run_loop;
  fake_observer_->RunOnNextRefresh(run_loop.QuitClosure());
  base::test::TestFuture<bool> remove_result;
  certificate_manager_model_->RemoveFromDatabase(
      std::move(platform_client_cert), remove_result.GetCallback());
  EXPECT_TRUE(remove_result.Get());
  run_loop.Run();

  {
    CertificateManagerModel::OrgGroupingMap org_grouping_map;
    certificate_manager_model_->FilterAndBuildOrgGroupingMap(
        net::CertType::USER_CERT, &org_grouping_map);
    CertificateManagerModel::CertInfo* extension_cert_info =
        GetCertInfoFromOrgGroupingMap(org_grouping_map, extension_cert.get());
    ASSERT_TRUE(extension_cert_info);

    EXPECT_EQ(net::CertType::USER_CERT, extension_cert_info->type());
    EXPECT_EQ(u"Client Cert A (extension provided)",
              extension_cert_info->name());
    EXPECT_FALSE(extension_cert_info->can_be_deleted());
    EXPECT_EQ(CertificateManagerModel::CertInfo::Source::kExtension,
              extension_cert_info->source());
    EXPECT_FALSE(extension_cert_info->web_trust_anchor());
    EXPECT_FALSE(extension_cert_info->hardware_backed());
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Test that CertificateManagerModel handles PKCS#12 import correctly.
// The test doesn't simulate a valid certificate, actual handling of PKCS#12
// data is covered by the tests for NSSCertDatabase and/or Kcer, but it tests
// that a meaningful result is returned to the caller.
// TODO(miersh): When kEnablePkcs12ToChapsDualWrite is enabled and
// is_extractable is true, PKCS#12 data is imported both into NSS and Kcer. That
// is difficult to verify at the moment. Soon UMA counters should be added and
// can be both tested here and used for the verification. And much later the
// import into NSS will be removed and the result code will come from Kcer.
TEST_F(CertificateManagerModelChromeOSTest, ImportFromPKCS12) {
  std::string kInvalidPkcs12Data = "111";
  std::u16string kPassword = u"222";

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      chromeos::features::kEnablePkcs12ToChapsDualWrite);

  {
    base::test::TestFuture<int> import_waiter;
    certificate_manager_model_->ImportFromPKCS12(
        test_nssdb_.slot(), kInvalidPkcs12Data, kPassword,
        /*is_extractable=*/false, import_waiter.GetCallback());
    EXPECT_EQ(import_waiter.Get(), net::ERR_PKCS12_IMPORT_INVALID_FILE);
  }

  {
    base::test::TestFuture<int> import_waiter;
    certificate_manager_model_->ImportFromPKCS12(
        test_nssdb_.slot(), kInvalidPkcs12Data, kPassword,
        /*is_extractable=*/true, import_waiter.GetCallback());
    EXPECT_EQ(import_waiter.Get(), net::ERR_PKCS12_IMPORT_INVALID_FILE);
  }

  feature_list.Reset();
  feature_list.InitAndEnableFeature(
      chromeos::features::kEnablePkcs12ToChapsDualWrite);

  {
    base::test::TestFuture<int> import_waiter;
    certificate_manager_model_->ImportFromPKCS12(
        test_nssdb_.slot(), kInvalidPkcs12Data, kPassword,
        /*is_extractable=*/false, import_waiter.GetCallback());
    EXPECT_EQ(import_waiter.Get(), net::ERR_PKCS12_IMPORT_INVALID_FILE);
  }

  {
    base::test::TestFuture<int> import_waiter;
    certificate_manager_model_->ImportFromPKCS12(
        test_nssdb_.slot(), kInvalidPkcs12Data, kPassword,
        /*is_extractable=*/true, import_waiter.GetCallback());
    EXPECT_EQ(import_waiter.Get(), net::ERR_PKCS12_IMPORT_INVALID_FILE);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#endif  // BUILDFLAG(IS_CHROMEOS)

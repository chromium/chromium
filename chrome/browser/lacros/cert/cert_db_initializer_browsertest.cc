
// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/lacros/cert/cert_db_initializer.h"
#include "chrome/browser/lacros/cert/cert_db_initializer_factory.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/cpp/keystore_service_util.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace {

constexpr char kRootCaCert[] = "root_ca_cert.pem";
// A PEM-encoded certificate which was signed by the Authority specified in
// |kRootCaCert|.
constexpr char kServerCert[] = "ok_cert.pem";

base::FilePath GetTestCertsPath() {
  base::FilePath test_data_dir;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));

  base::FilePath test_certs_path =
      test_data_dir.AppendASCII("policy").AppendASCII("test_certs");
  base::ScopedAllowBlockingForTesting allow_io;
  EXPECT_TRUE(base::DirectoryExists(test_certs_path));
  return test_certs_path;
}

void WaitUnitCertDbReady(Profile* profile) {
  base::RunLoop run_loop;
  CertDbInitializer* initializer =
      CertDbInitializerFactory::GetInstance()->GetForBrowserContext(profile);
  ASSERT_TRUE(initializer);
  auto subscription = initializer->WaitUntilReady(run_loop.QuitClosure());
  run_loop.Run();
}

void ImportCaCertWithDb(base::OnceClosure done_callback,
                        net::NSSCertDatabase* nss_db) {
  ASSERT_TRUE(nss_db);

  std::string cert_bytes;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(
        GetTestCertsPath().AppendASCII(kRootCaCert), &cert_bytes));
  }

  auto cert = net::x509_util::CreateCERTCertificateListFromBytes(
      cert_bytes.data(), cert_bytes.size(), net::X509Certificate::FORMAT_AUTO);

  net::NSSCertDatabase::ImportCertFailureList failure_list;
  nss_db->ImportCACerts(std::move(cert),
                        /*trust_bits=*/net::NSSCertDatabase::TRUSTED_SSL,
                        &failure_list);

  if (!failure_list.empty()) {
    for (const auto& failure : failure_list) {
      LOG(ERROR) << "Failed to import CA cert: " << failure.net_error;
    }
    ASSERT_TRUE(false);
  }

  std::move(done_callback).Run();
}

void GetNssDatabase(base::OnceClosure done_callback,
                    NssCertDatabaseGetter nss_getter) {
  auto on_got_db = base::SplitOnceCallback(
      base::BindOnce(&ImportCaCertWithDb, std::move(done_callback)));
  net::NSSCertDatabase* nss_db =
      std::move(nss_getter).Run(std::move(on_got_db.first));
  if (nss_db) {
    std::move(on_got_db.second).Run(nss_db);
  }
}

void ImportCaCert(Profile* profile) {
  base::RunLoop run_loop;

  NssService* nss_service =
      NssServiceFactory::GetInstance()->GetForContext(profile);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetNssDatabase, run_loop.QuitClosure(),
                     nss_service->CreateNSSCertDatabaseGetterForIOThread()));

  run_loop.Run();
}

[[nodiscard]] int VerifyServerCert(Profile* profile) {
  base::FilePath server_cert_path = GetTestCertsPath().AppendASCII(kServerCert);
  scoped_refptr<net::X509Certificate> server_cert = net::ImportCertFromFile(
      server_cert_path.DirName(), server_cert_path.BaseName().value());
  EXPECT_TRUE(server_cert);

  base::test::TestFuture<int> future;
  profile->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->VerifyCertificateForTesting(
          server_cert, "127.0.0.1", /*ocsp_response=*/std::string(),
          /*sct_list=*/std::string(), future.GetCallback());
  return future.Get();
}

// Generates an x509 client certificate for the `public_key_spki` and returns it
// as a DER-encoded certificate.
[[nodiscard]] std::vector<uint8_t> GenerateClientCertForPublicKey(
    const std::vector<uint8_t>& public_key_spki) {
  net::CertBuilder issuer(/*orig_cert=*/nullptr, /*issuer=*/nullptr);
  auto cert_builder =
      net::CertBuilder::FromSubjectPublicKeyInfo(public_key_spki, &issuer);
  cert_builder->SetSignatureAlgorithmRsaPkca1(net::DigestAlgorithm::Sha256);
  cert_builder->SetValidity(base::Time::Now(),
                            base::Time::Now() + base::Days(30));

  auto cert_span =
      net::x509_util::CryptoBufferAsSpan(cert_builder->GetCertBuffer());
  return std::vector<uint8_t>(cert_span.begin(), cert_span.end());
}

// Observes notifications about cert database changes during its lifetime.
class ScopedCertDatabaseObserver : public net::CertDatabase::Observer {
 public:
  ScopedCertDatabaseObserver() {
    net::CertDatabase::GetInstance()->AddObserver(this);
  }
  ~ScopedCertDatabaseObserver() override {
    net::CertDatabase::GetInstance()->RemoveObserver(this);
  }

  void OnCertDBChanged() override {
    notifications_received_++;
    run_loop_.Quit();
  }

  // Waits for the next CertDBChanged notification if none were observed so far.
  // Returns the amount of notifications received since creation. The counter is
  // mostly used to detect unexpected notifications that could cause flakiness /
  // false positives.
  size_t Wait() {
    // Noop if Quit() was ever called.
    run_loop_.Run();
    return notifications_received_;
  }

 private:
  size_t notifications_received_ = 0;
  base::RunLoop run_loop_;
};

class CertDbInitializerTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    CertDbInitializerFactory::GetInstance()
        ->SetCreateWithBrowserContextForTesting(
            /*should_create=*/true);
    InProcessBrowserTest::SetUp();
  }
};

// TODO(b/219968355): At the moment Lacros browser tests don't clear user
// directory in Ash in between tests. Because of that, these tests interfere
// with each other (because they import certs into the software nss database
// that is stored there) and fail when run all in the same batch. They can be
// run one a time for semi-manual testing and should be re-enabled when the bug
// is fixed.

// Tests that CertDbInitializer eventually reports that cert database is ready
// for the main profile.
IN_PROC_BROWSER_TEST_F(CertDbInitializerTest, DISABLED_EventuallyReady) {
  EXPECT_TRUE(browser()->profile()->IsMainProfile());
  WaitUnitCertDbReady(browser()->profile());
}

// Tests that a CA certificate can be imported and used for cert verification.
IN_PROC_BROWSER_TEST_F(CertDbInitializerTest, DISABLED_CanImportCaCert) {
  WaitUnitCertDbReady(browser()->profile());
  ImportCaCert(browser()->profile());
  EXPECT_EQ(net::OK, VerifyServerCert(browser()->profile()));
}

// Tests that without importing a CA certificate, Chrome rejects unknown server
// certs.
IN_PROC_BROWSER_TEST_F(CertDbInitializerTest, DISABLED_CertRejectedByDefault) {
  WaitUnitCertDbReady(browser()->profile());
  EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID,
            VerifyServerCert(browser()->profile()));
}

// Imports a CA certs that will be available in ImmediatelyAfterLaunch test.
IN_PROC_BROWSER_TEST_F(CertDbInitializerTest,
                       DISABLED_PRE_ImmediatelyAfterLaunch) {
  WaitUnitCertDbReady(browser()->profile());
  ImportCaCert(browser()->profile());
}

// Tests that Chrome waits until certs are initialized and the imported CA cert
// from the PRE_ test is available before verifying server certs.
IN_PROC_BROWSER_TEST_F(CertDbInitializerTest, DISABLED_ImmediatelyAfterLaunch) {
  EXPECT_EQ(net::OK, VerifyServerCert(browser()->profile()));
}

// Tests that when Ash imports a new certificate, Lacros receives a
// notification about it.
IN_PROC_BROWSER_TEST_F(CertDbInitializerTest,
                       DISABLED_CertsChangedNotificationFromAsh) {
  auto& keystore_crosapi = chromeos::LacrosService::Get()
                               ->GetRemote<crosapi::mojom::KeystoreService>();

  // This test uses the Keystore mojo API to make Ash import a cert. Ash will
  // only successfully import a cert if it owns a key pair associated with it.
  // This call generates a new key pair.
  base::test::TestFuture<crosapi::mojom::KeystoreBinaryResultPtr>
      generate_key_result;
  keystore_crosapi->GenerateKey(
      crosapi::mojom::KeystoreType::kUser,
      crosapi::keystore_service_util::MakeRsaKeystoreSigningAlgorithm(
          /*modulus_length=*/2048, /*sw_backed=*/false),
      generate_key_result.GetCallback());
  ASSERT_FALSE(generate_key_result.Get()->is_error());

  std::vector<uint8_t> client_cert =
      GenerateClientCertForPublicKey(generate_key_result.Get()->get_blob());

  ScopedCertDatabaseObserver observer;

  // Generate and import a certificate.
  base::test::TestFuture<bool /*is_error*/, crosapi::mojom::KeystoreError>
      add_cert_result;
  keystore_crosapi->AddCertificate(crosapi::mojom::KeystoreType::kUser,
                                   client_cert, add_cert_result.GetCallback());
  ASSERT_FALSE(add_cert_result.Get<0>())
      << "Error: " << add_cert_result.Get<1>();

  // Wait for the notification from Ash about cert database changes.
  // If there are more than one, most likely there are other sources of changes
  // in the background and the test should be rewritten somehow.
  EXPECT_EQ(1u, observer.Wait());

  // Check that the cert was actually imported.
  base::test::TestFuture<crosapi::mojom::GetCertificatesResultPtr>
      get_certs_result;
  keystore_crosapi->GetCertificates(crosapi::mojom::KeystoreType::kUser,
                                    get_certs_result.GetCallback());
  ASSERT_FALSE(get_certs_result.Get()->is_error());
  EXPECT_TRUE(
      base::Contains(get_certs_result.Get()->get_certificates(), client_cert));
}

// TODO(b/191336682): Add a test similar to CertsChangedNotificationFromAsh, but
// about system keystore. Right now system slot is not available/emulated in
// lacros browser tests.

// For a test that covers notifications in Ash when Lacros changes the database,
// see network.CertSettingsPage tast test. Such a browser test could be written
// by adding new methods into crosapi.TestController, but their implementation
// would have a similar complexity to the notification mechanism itself.

}  // namespace

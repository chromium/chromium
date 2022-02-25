
// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/lacros/cert_db_initializer.h"
#include "chrome/browser/lacros/cert_db_initializer_factory.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
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

}  // namespace

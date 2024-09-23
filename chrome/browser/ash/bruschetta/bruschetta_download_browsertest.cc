// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_download.h"

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bruschetta {
namespace {

class BruschettaDownloadBrowserTest : public InProcessBrowserTest {};

class BruschettaHttpsDownloadBrowserTest
    : public BruschettaDownloadBrowserTest {
 public:
  BruschettaHttpsDownloadBrowserTest() = default;
  ~BruschettaHttpsDownloadBrowserTest() override = default;

  BruschettaHttpsDownloadBrowserTest(
      const BruschettaHttpsDownloadBrowserTest&) = delete;
  BruschettaHttpsDownloadBrowserTest& operator=(
      const BruschettaHttpsDownloadBrowserTest&) = delete;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    BruschettaDownloadBrowserTest::SetUpInProcessBrowserTestFixture();

    // Set up the test server, with the download URL and to require SSL cert.
    net::SSLServerConfig ssl_config;
    ssl_config.client_cert_type =
        net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
    server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
    server_.ServeFilesFromSourceDirectory("chrome/test/data/bruschetta");
    server_handle_ = server_.StartAndReturnHandle();
    ASSERT_TRUE(server_handle_);
    url_ = server_.GetURL("/download_file.img");
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add an entry into AutoSelectCertificateForUrls policy for automatic
    // client cert selection.
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
    DCHECK(profile);
    base::Value::List filters;
    filters.Append(base::Value::Dict());
    base::Value::Dict setting;
    setting.Set("filters", std::move(filters));
    HostContentSettingsMapFactory::GetForProfile(profile)
        ->SetWebsiteSettingDefaultScope(
            url_, GURL(), ContentSettingsType::AUTO_SELECT_CERTIFICATE,
            base::Value(std::move(setting)));
  }

  net::EmbeddedTestServer server_{net::EmbeddedTestServer::TYPE_HTTPS};
  net::test_server::EmbeddedTestServerHandle server_handle_;
  GURL url_;
};

bool PathExistsBlockingAllowed(const base::FilePath& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  return base::PathExists(path);
}

// A stub ClientCertStore which returns the list of client certs it was
// initialised with.
class StubClientCertStore : public net::ClientCertStore {
 public:
  explicit StubClientCertStore(net::ClientCertIdentityList list)
      : list_(std::move(list)) {}

  ~StubClientCertStore() override = default;

  // net::ClientCertStore override.
  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override {
    std::move(callback).Run(std::move(list_));
  }

 private:
  net::ClientCertIdentityList list_;
};

// Creates a stub client cert store which returns a single test certificate.
std::unique_ptr<net::ClientCertStore> CreateStubClientCertStore() {
  base::FilePath certs_dir = net::GetTestCertsDirectory();

  net::ClientCertIdentityList cert_identity_list;
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::unique_ptr<net::FakeClientCertIdentity> cert_identity =
      net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
          certs_dir, "client_1.pem", "client_1.pk8");
  EXPECT_TRUE(cert_identity.get());
  if (cert_identity) {
    cert_identity_list.push_back(std::move(cert_identity));
  }

  return std::unique_ptr<net::ClientCertStore>(
      new StubClientCertStore(std::move(cert_identity_list)));
}

std::unique_ptr<net::ClientCertStore> CreateEmptyClientCertStore() {
  return std::unique_ptr<net::ClientCertStore>(new StubClientCertStore({}));
}

// Tests that we get an empty path and hash for a network error (in this case
// a bad URL).
IN_PROC_BROWSER_TEST_F(BruschettaHttpsDownloadBrowserTest,
                       TestDownloadUrlNotFound) {
  base::test::TestFuture<base::FilePath, std::string> future;
  auto download = std::make_unique<SimpleURLLoaderDownload>();
  download->StartDownload(browser()->profile(), GURL("bad url"),
                          future.GetCallback());

  auto path = future.Get<base::FilePath>();
  auto hash = future.Get<std::string>();

  ASSERT_TRUE(path.empty());
  EXPECT_EQ(hash, "");
}

// Tests that the `url_` is downloaded successfully.
// Because the `server_` is configured to require a client certificate
// this involves selecting a client certificate using the auto-selection
// setting configured in `SetUpOnMainThread`
IN_PROC_BROWSER_TEST_F(BruschettaHttpsDownloadBrowserTest, TestHappyPath) {
  const auto kExpectedHash = base::ToUpperASCII(
      "f54d00e6d24844ee3b1d0d8c2b9d2ed80b967e94eb1055bb1fd43eb9522908cc");

  // Make the browser use the ClientCertStoreStub instead of the regular one.
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->set_client_cert_store_factory_for_testing(
          base::BindRepeating(&CreateStubClientCertStore));

  base::test::TestFuture<base::FilePath, std::string> future;
  auto download = std::make_unique<SimpleURLLoaderDownload>();
  download->StartDownload(browser()->profile(), url_, future.GetCallback());

  auto path = future.Get<base::FilePath>();
  auto hash = future.Get<std::string>();

  ASSERT_FALSE(path.empty());
  EXPECT_EQ(hash, kExpectedHash);

  // Deleting the download should clean up downloaded files. I found
  // RunUntilIdle to be flaky hence we have an explicit callback for after
  // deletion completes.
  base::RunLoop run_loop;
  download->SetPostDeletionCallbackForTesting(run_loop.QuitClosure());
  download.reset();
  run_loop.Run();
  EXPECT_FALSE(PathExistsBlockingAllowed(path));
}

// Tests downloading from url_, which requires an SSL cert for auth, when the
// cert isn't available. This should fail and signal that by returning an empty
// path.
IN_PROC_BROWSER_TEST_F(BruschettaHttpsDownloadBrowserTest,
                       TestDownloadNoMatchingCert) {
  // Make the browser use an empty client cert store
  ProfileNetworkContextServiceFactory::GetForContext(browser()->profile())
      ->set_client_cert_store_factory_for_testing(
          base::BindRepeating(&CreateEmptyClientCertStore));

  base::test::TestFuture<base::FilePath, std::string> future;
  auto download = std::make_unique<SimpleURLLoaderDownload>();
  download->StartDownload(browser()->profile(), url_, future.GetCallback());

  auto path = future.Get<base::FilePath>();
  auto hash = future.Get<std::string>();

  ASSERT_TRUE(path.empty());
}

}  // namespace
}  // namespace bruschetta

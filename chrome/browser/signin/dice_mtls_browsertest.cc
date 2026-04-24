// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_config.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cert/test_root_certs.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DummyClientCertStore : public net::ClientCertStore {
 public:
  explicit DummyClientCertStore(
      std::unique_ptr<net::FakeClientCertIdentity> identity)
      : identity_(std::move(identity)) {}
  ~DummyClientCertStore() override = default;

  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override {
    net::ClientCertIdentityList cert_identity_list;
    if (identity_) {
      cert_identity_list.push_back(identity_->Copy());
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(cert_identity_list)));
  }

 private:
  std::unique_ptr<net::FakeClientCertIdentity> identity_;
};

class RefreshTokenObserver : public signin::IdentityManager::Observer {
 public:
  RefreshTokenObserver(signin::IdentityManager* identity_manager,
                       const CoreAccountId& account_id,
                       base::OnceClosure quit_closure)
      : account_id_(account_id), quit_closure_(std::move(quit_closure)) {
    identity_manager_observation_.Observe(identity_manager);
  }
  ~RefreshTokenObserver() override = default;

  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override {
    if (account_info.account_id == account_id_) {
      if (quit_closure_) {
        std::move(quit_closure_).Run();
      }
    }
  }

 private:
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  CoreAccountId account_id_;
  base::OnceClosure quit_closure_;
};

}  // namespace

class DiceMtlsBrowserTest : public InProcessBrowserTest {
 public:
  DiceMtlsBrowserTest() {
    feature_list_.InitWithFeatures({switches::kEnableMtlsTokenBinding,
                                    switches::kEnableChromeRefreshTokenBinding},
                                   {});
  }

  void SetUp() override {
    embedded_https_test_server().SetCertHostnames(
        {"google.com", "accounts.google.com"});
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        GetChromeTestDataDir());
    CHECK(embedded_https_test_server().InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII("allow-browser-signin", "true");
    command_line->AppendSwitch("ignore-certificate-errors");

    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        "MAP oauthaccountmanager.mtls.googleapis.com 127.0.0.1, MAP "
        "oauth2.mtls.googleapis.com 127.0.0.1, MAP * " +
            embedded_https_test_server().host_port_pair().ToString());
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    second_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);

    // Setup certificates for the second server.
    base::FilePath certs_dir = net::GetTestCertsDirectory();
    scoped_refptr<net::X509Certificate> ca_cert =
        net::ImportCertFromFile(certs_dir, "root_ca_cert.pem");
    CHECK(ca_cert);
    scoped_test_root_ = std::make_unique<net::ScopedTestRoot>(ca_cert);

    net::SSLServerConfig ssl_config;
    ssl_config.client_cert_type = net::SSLServerConfig::REQUIRE_CLIENT_CERT;

    net::EmbeddedTestServer::ServerCertificateConfig server_cert_config;
    server_cert_config.dns_names = {"oauthaccountmanager.mtls.googleapis.com",
                                    "oauth2.mtls.googleapis.com"};

    second_server_->SetSSLConfig(server_cert_config, ssl_config);

    // LST and AccessToken fetch handler.
    auto handler = [](DiceMtlsBrowserTest* test,
                      const net::test_server::HttpRequest& request)
        -> std::unique_ptr<net::test_server::HttpResponse> {
      if (request.GetURL().path() == "/token") {
        CHECK(request.ssl_info.has_value() && request.ssl_info->cert)
            << "Missing client certificate.";
        test->lst_fetched_with_cert_ = true;
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_content(
            R"({
              "access_token": "dummy_access_token",
              "refresh_token": "test_refresh_token",
              "expires_in": 3600,
              "refresh_token_type": "bound_to_key",
              "id_token": "test_id_token"
            })");
        response->set_content_type("application/json");
        response->set_code(net::HTTP_OK);
        return response;
      }
      if (request.GetURL().path() == "/v1/issuetoken") {
        CHECK(request.ssl_info.has_value() && request.ssl_info->cert)
            << "Missing client certificate.";
        test->access_token_fetched_with_cert_ = true;
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_content(
            R"({
              "issueAdvice": "auto",
              "token": "test_access_token",
              "expiresIn": "3600",
              "grantedScopes": "email"
            })");
        response->set_content_type("application/json");
        response->set_code(net::HTTP_OK);
        return response;
      }
      return nullptr;
    };

    second_server_->RegisterRequestHandler(
        base::BindRepeating(handler, base::Unretained(this)));

    CHECK(second_server_->InitializeAndListen());
    second_server_->StartAcceptingConnections();

    // Handle requests on embedded_https_test_server
    embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
        [](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url.find("/Signin") != std::string::npos) {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->AddCustomHeader(
                "X-Chrome-ID-Consistency-Response",
                base::StringPrintf(
                    "action=SIGNIN,authuser=1,id=%s,email=test@gmail.com,"
                    "authorization_code=test_auth_code,mtls_token_binding=true,"
                    "eligible_for_token_binding=ES256 RS256",
                    signin::GetTestGaiaIdForEmail("test@gmail.com")
                        .ToString()
                        .c_str()));
            return response;
          }
          return nullptr;
        }));
    embedded_https_test_server().StartAcceptingConnections();
    Profile* profile = browser()->profile();
    SetupMtlsEnvironment(profile);
  }

  void TearDownOnMainThread() override {
    second_server_.reset();
    GaiaUrls::SetInstanceForTesting(nullptr);
    InProcessBrowserTest::TearDownOnMainThread();
  }

 public:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> second_server_;
  std::unique_ptr<net::ScopedTestRoot> scoped_test_root_;
  std::atomic<bool> access_token_fetched_with_cert_ = false;
  std::atomic<bool> lst_fetched_with_cert_ = false;

  void SetupMtlsEnvironment(Profile* profile);

  base::ScopedClosureRunner scoped_config_;
  std::unique_ptr<GaiaUrls> test_gaia_urls_;
};

void DiceMtlsBrowserTest::SetupMtlsEnvironment(Profile* profile) {
  int https_port = second_server_->port();

  // Override GaiaUrls to include the test server port for both
  // accounts.google.com and oauth2.mtls.googleapis.com.
  std::string gaia_url = base::StringPrintf(
      "https://accounts.google.com:%d/", embedded_https_test_server().port());
  std::string mtls_url = base::StringPrintf(
      "https://oauth2.mtls.googleapis.com:%d/token", https_port);
  std::string mtls_issue_token_url = base::StringPrintf(
      "https://oauthaccountmanager.mtls.googleapis.com:%d/v1/issuetoken",
      https_port);

  base::DictValue urls_dict;
  urls_dict.Set("gaia_url", base::DictValue().Set("url", gaia_url));
  urls_dict.Set("mtls_oauth2_token_url",
                base::DictValue().Set("url", mtls_url));
  urls_dict.Set("mtls_oauth2_issue_token_url",
                base::DictValue().Set("url", mtls_issue_token_url));

  base::DictValue config_dict;
  config_dict.Set("urls", std::move(urls_dict));

  scoped_config_ = GaiaConfig::SetScopedConfigForTesting(
      std::make_unique<GaiaConfig>(std::move(config_dict)));

  test_gaia_urls_ = std::make_unique<GaiaUrls>();
  GaiaUrls::SetInstanceForTesting(test_gaia_urls_.get());

  // Setup client cert store using static files.
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  auto create_cert_store = base::BindRepeating(
      [](const base::FilePath& dir, const std::string& cert_file,
         const std::string& key_file) -> std::unique_ptr<net::ClientCertStore> {
        base::ScopedAllowBlockingForTesting allow_blocking;
        auto identity = net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
            dir, cert_file, key_file);
        CHECK(identity);
        return std::make_unique<DummyClientCertStore>(std::move(identity));
      },
      certs_dir, "client_1.pem", "client_1.pk8");

  ProfileNetworkContextServiceFactory::GetForContext(profile)
      ->set_client_cert_store_factory_for_testing(create_cert_store);

  // Set content settings for auto selection.
  std::vector<GURL> urls = {
      second_server_->GetURL("oauthaccountmanager.mtls.googleapis.com", "/"),
      second_server_->GetURL("oauth2.mtls.googleapis.com", "/")};

  for (const auto& url : urls) {
    base::ListValue filters;
    filters.Append(base::DictValue());
    base::DictValue setting;
    setting.Set("filters", std::move(filters));

    HostContentSettingsMapFactory::GetForProfile(profile)
        ->SetWebsiteSettingDefaultScope(
            url, GURL(), ContentSettingsType::AUTO_SELECT_CERTIFICATE,
            base::Value(std::move(setting)));
  }
}

IN_PROC_BROWSER_TEST_F(DiceMtlsBrowserTest, DiceSigninWithMtlsTokenBinding) {
  Profile* profile = browser()->profile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  CoreAccountId account_id = identity_manager->PickAccountIdForAccount(
      signin::GetTestGaiaIdForEmail("test@gmail.com"), "test@gmail.com");

  base::RunLoop token_update_loop;
  RefreshTokenObserver observer(identity_manager, account_id,
                                token_update_loop.QuitClosure());

  EXPECT_FALSE(lst_fetched_with_cert_);

  // Navigate to /Signin to trigger registration and DICE header.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL("accounts.google.com", "/Signin")));

  token_update_loop.Run();

  EXPECT_TRUE(lst_fetched_with_cert_);

  base::test::TestFuture<GoogleServiceAuthError, signin::AccessTokenInfo>
      future;

  EXPECT_FALSE(access_token_fetched_with_cert_);
  // Trigger an access token fetch to verify that the fetch uses mTLS.
  std::unique_ptr<signin::AccessTokenFetcher> fetcher =
      identity_manager->CreateAccessTokenFetcherForAccount(
          account_id, signin::OAuthConsumerId::kSyncDeviceStatisticsMetrics,
          future.GetCallback(), signin::AccessTokenFetcher::Mode::kImmediate);

  ASSERT_TRUE(future.Wait());
  ASSERT_EQ(future.Get<signin::AccessTokenInfo>().token, "test_access_token");

  EXPECT_TRUE(access_token_fetched_with_cert_);
}

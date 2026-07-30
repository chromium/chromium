// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/enterprise_skills_provider.h"

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_service.h"
#include "components/skills/public/skills_prefs.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "crypto/sha2.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/key_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

const char kValidYamlFrontmatter[] =
    "---\n"
    "name: \"Enterprise Test\"\n"
    "description: \"Test description for EnterprisePublishedSkills\"\n"
    "---\n"
    "Test prompt content";

// A stub ClientCertStore that returns a FakeClientCertIdentity.
class ClientCertStoreStub : public net::ClientCertStore {
 public:
  explicit ClientCertStoreStub(net::ClientCertIdentityList list)
      : list_(std::move(list)) {}

  ~ClientCertStoreStub() override = default;

  // net::ClientCertStore:
  void GetClientCerts(
      scoped_refptr<const net::SSLCertRequestInfo> cert_request_info,
      ClientCertListCallback callback) override {
    std::move(callback).Run(std::move(list_));
  }

 private:
  net::ClientCertIdentityList list_;
};

std::unique_ptr<net::ClientCertStore> CreateCertStore(bool provide_cert) {
  net::ClientCertIdentityList cert_identity_list;
  if (provide_cert) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath certs_dir = net::GetTestCertsDirectory();
    std::unique_ptr<net::FakeClientCertIdentity> cert_identity =
        net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
            certs_dir, "client_1.pem", "client_1.pk8");
    if (cert_identity) {
      cert_identity_list.emplace_back(std::move(cert_identity));
    }
  }
  return std::make_unique<ClientCertStoreStub>(std::move(cert_identity_list));
}

}  // namespace

class EnterpriseSkillsProviderBrowserTest : public InProcessBrowserTest {
 public:
  EnterpriseSkillsProviderBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    https_server_.RegisterRequestHandler(
        base::BindRepeating(&EnterpriseSkillsProviderBrowserTest::HandleRequest,
                            base::Unretained(this)));

    net::SSLServerConfig ssl_config;
    ssl_config.client_cert_type =
        net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
    ASSERT_TRUE(https_server_.Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(kValidYamlFrontmatter);
    response->set_content_type("text/yaml");
    return response;
  }

  void SetCertStore(bool provide_cert) {
    ProfileNetworkContextServiceFactory::GetForContext(browser()->GetProfile())
        ->set_client_cert_store_factory_for_testing(
            base::BindRepeating(&CreateCertStore, provide_cert));
  }

  void SetAutoSelectCertificateForUrlsPolicy(const GURL& url) {
    base::ListValue filters;
    filters.Append(base::DictValue());
    base::DictValue setting;
    setting.Set("filters", std::move(filters));
    HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile())
        ->SetWebsiteSettingDefaultScope(
            url, GURL(), ContentSettingsType::AUTO_SELECT_CERTIFICATE,
            base::Value(std::move(setting)));
    browser()
        ->GetProfile()
        ->GetDefaultStoragePartition()
        ->FlushNetworkInterfaceForTesting();
  }

  void SetEnterprisePublishedSkillsPolicy(const GURL& url) {
    std::string hash =
        base::HexEncode(crypto::SHA256HashString(kValidYamlFrontmatter));
    base::ListValue list;
    base::DictValue dict;
    dict.Set("url", url.spec());
    dict.Set("hash", hash);
    list.Append(std::move(dict));
    browser()->GetProfile()->GetPrefs()->SetList(
        skills::prefs::kEnterprisePublishedSkills, std::move(list));
  }

  void SetPolicyPref(
      const std::vector<std::pair<std::string, std::string>>& urls_and_hashes) {
    base::ListValue list;
    for (const auto& pair : urls_and_hashes) {
      base::DictValue dict;
      dict.Set("url", pair.first);
      dict.Set("hash", pair.second);
      list.Append(std::move(dict));
    }
    browser()->GetProfile()->GetPrefs()->SetList(
        skills::prefs::kEnterprisePublishedSkills, std::move(list));
  }

 protected:
  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(EnterpriseSkillsProviderBrowserTest,
                       FetchWithCertAndAutoSelectCertificateForUrlsPolicy) {
  // AutoSelectCertificateForUrls policy set with correct cert available.
  SetCertStore(true);
  GURL https_url = https_server_.GetURL("/skill.yaml");
  SetAutoSelectCertificateForUrlsPolicy(https_url);

  auto provider = std::make_unique<skills::EnterpriseSkillsProvider>(
      browser()->GetProfile()->GetPrefs(),
      browser()->GetProfile()->GetURLLoaderFactory());

  base::RunLoop run_loop;
  auto sub = provider->RegisterSkillsChangedCallback(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

  SetEnterprisePublishedSkillsPolicy(https_url);
  run_loop.Run();

  // The fetch should succeed because the cert was automatically selected.
  EXPECT_EQ(1u, provider->GetSkills().size());
}

IN_PROC_BROWSER_TEST_F(
    EnterpriseSkillsProviderBrowserTest,
    FetchWithoutCertButAutoSelectCertificateForUrlsPolicySet) {
  // AutoSelectCertificateForUrls policy set but NO cert available.
  SetCertStore(false);
  GURL https_url = https_server_.GetURL("/skill.yaml");
  SetAutoSelectCertificateForUrlsPolicy(https_url);

  auto provider = std::make_unique<skills::EnterpriseSkillsProvider>(
      browser()->GetProfile()->GetPrefs(),
      browser()->GetProfile()->GetURLLoaderFactory());

  base::RunLoop run_loop;
  auto sub = provider->RegisterSkillsChangedCallback(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

  SetEnterprisePublishedSkillsPolicy(https_url);
  run_loop.Run();

  // The fetch should fail because the server requires a cert, but none was
  // provided.
  EXPECT_EQ(0u, provider->GetSkills().size());
}

IN_PROC_BROWSER_TEST_F(EnterpriseSkillsProviderBrowserTest,
                       FetchWithCertButNoAutoSelectCertificateForUrlsPolicy) {
  // Cert is available but AutoSelectCertificateForUrls policy is NOT set.
  SetCertStore(true);
  GURL https_url = https_server_.GetURL("/skill.yaml");
  // Do NOT set AutoSelectCertificateForUrls policy.

  auto provider = std::make_unique<skills::EnterpriseSkillsProvider>(
      browser()->GetProfile()->GetPrefs(),
      browser()->GetProfile()->GetURLLoaderFactory());

  base::RunLoop run_loop;
  auto sub = provider->RegisterSkillsChangedCallback(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

  SetEnterprisePublishedSkillsPolicy(https_url);
  run_loop.Run();

  // The fetch should fail (or hang/timeout depending on if the UI prompt
  // blocks). Because it's a headless SimpleURLLoader, it will fail to provide
  // the cert.
  EXPECT_EQ(0u, provider->GetSkills().size());
}

IN_PROC_BROWSER_TEST_F(EnterpriseSkillsProviderBrowserTest,
                       StressTestWithRealServer) {
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        if (request.relative_url.find("/fail") != std::string::npos) {
          response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
        } else if (request.relative_url.find("/slow") != std::string::npos) {
          base::PlatformThread::Sleep(base::Milliseconds(500));
          response->set_code(net::HTTP_OK);
          response->set_content(kValidYamlFrontmatter);
          response->set_content_type("text/yaml");
        } else if (request.relative_url.find("/hang") != std::string::npos) {
          return std::make_unique<net::test_server::HungResponse>();
        } else {
          response->set_code(net::HTTP_OK);
          response->set_content(kValidYamlFrontmatter);
          response->set_content_type("text/yaml");
        }
        return response;
      }));
  ASSERT_TRUE(test_server.Start());

  std::string expected_hash =
      base::HexEncode(crypto::SHA256HashString(kValidYamlFrontmatter));

  constexpr size_t kNumFast = 10;
  constexpr size_t kNumFail = 2;
  constexpr size_t kNumSlow = 2;
  constexpr size_t kNumHang = 2;

  std::vector<std::pair<std::string, std::string>> policy_entries;
  for (size_t i = 0; i < kNumFast; ++i) {
    policy_entries.emplace_back(
        test_server.GetURL("/fast_" + base::NumberToString(i)).spec(),
        expected_hash);
  }
  for (size_t i = 0; i < kNumFail; ++i) {
    policy_entries.emplace_back(
        test_server.GetURL("/fail_" + base::NumberToString(i)).spec(),
        expected_hash);
  }
  for (size_t i = 0; i < kNumSlow; ++i) {
    policy_entries.emplace_back(
        test_server.GetURL("/slow_" + base::NumberToString(i)).spec(),
        expected_hash);
  }
  for (size_t i = 0; i < kNumHang; ++i) {
    policy_entries.emplace_back(
        test_server.GetURL("/hang_" + base::NumberToString(i)).spec(),
        expected_hash);
  }

  auto provider = std::make_unique<skills::EnterpriseSkillsProvider>(
      browser()->GetProfile()->GetPrefs(),
      browser()->GetProfile()->GetURLLoaderFactory());

  base::RunLoop run_loop;
  auto sub = provider->RegisterSkillsChangedCallback(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

  SetPolicyPref(policy_entries);
  run_loop.Run();

  EXPECT_EQ(kNumFast + kNumSlow, provider->GetSkills().size());
}

// Verifies that when a policy specifies a hash for a URL, but the HTTP cache
// (or server) returns content whose SHA-256 hash does not match the policy
// hash, the provider rejects the mismatched content and clears the skill.
IN_PROC_BROWSER_TEST_F(EnterpriseSkillsProviderBrowserTest,
                       HashMismatchWithCachedURL) {
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->AddCustomHeader("Cache-Control", "max-age=3600");
        response->set_code(net::HTTP_OK);
        response->set_content(kValidYamlFrontmatter);
        response->set_content_type("text/yaml");
        return response;
      }));
  ASSERT_TRUE(test_server.Start());

  std::string valid_hash =
      base::HexEncode(crypto::SHA256HashString(kValidYamlFrontmatter));
  std::string fake_new_hash =
      base::HexEncode(crypto::SHA256HashString("completely_different_content"));
  std::string test_url = test_server.GetURL("/test_skill.yaml").spec();

  auto provider = std::make_unique<skills::EnterpriseSkillsProvider>(
      browser()->GetProfile()->GetPrefs(),
      browser()->GetProfile()->GetURLLoaderFactory());

  {
    // First fetch with the correct matching hash should succeed.
    base::RunLoop run_loop;
    auto sub = provider->RegisterSkillsChangedCallback(
        base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

    SetPolicyPref({{test_url, valid_hash}});
    run_loop.Run();
    EXPECT_EQ(1u, provider->GetSkills().size());
  }

  {
    // Update the policy with a mismatched hash. Even if the URL is cached,
    // the downloaded content hash will not match the new policy hash, so the
    // provider must reject it and clear the skill.
    base::RunLoop run_loop;
    auto sub = provider->RegisterSkillsChangedCallback(
        base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

    SetPolicyPref({{test_url, fake_new_hash}});
    run_loop.Run();

    EXPECT_EQ(0u, provider->GetSkills().size());
  }
}

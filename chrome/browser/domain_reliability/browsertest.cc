// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/domain_reliability/domain_reliability_prefs.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace domain_reliability {

class DomainReliabilityBrowserTest : public InProcessBrowserTest {
 public:
  DomainReliabilityBrowserTest()
      : test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    net::URLRequestFailedJob::AddUrlHandler();
  }

  DomainReliabilityBrowserTest(const DomainReliabilityBrowserTest&) = delete;
  DomainReliabilityBrowserTest& operator=(const DomainReliabilityBrowserTest&) =
      delete;

  ~DomainReliabilityBrowserTest() override {}

  // Note: In an ideal world, instead of appending the command-line switch and
  // manually setting discard_uploads to false, Domain Reliability would
  // continuously monitor the metrics reporting pref, and the test could just
  // set the pref.

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableDomainReliability);
  }

  void SetUp() override {
    ProfileNetworkContextService::SetDiscardDomainReliabilityUploadsForTesting(
        false);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    test_server()->AddDefaultHandlers(GetChromeTestDataDir());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  network::mojom::NetworkContext* GetNetworkContext() {
    return browser()
        ->profile()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext();
  }

  net::EmbeddedTestServer* test_server() { return &test_server_; }

 private:
  net::EmbeddedTestServer test_server_;
};

class DomainReliabilityDisabledBrowserTest
    : public DomainReliabilityBrowserTest {
 public:
  DomainReliabilityDisabledBrowserTest(
      const DomainReliabilityDisabledBrowserTest&) = delete;
  DomainReliabilityDisabledBrowserTest& operator=(
      const DomainReliabilityDisabledBrowserTest&) = delete;

 protected:
  DomainReliabilityDisabledBrowserTest() {}

  ~DomainReliabilityDisabledBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableDomainReliability);
  }
};

class DomainReliabilityPolicyTest : public policy::PolicyTest {
 protected:
  void SetAndUpdateDomainReliabilityAllowedPolicy(bool value) {
    policy::PolicyMap policies;
    policies.Set(policy::key::kDomainReliabilityAllowed,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(value), nullptr);
    provider_.UpdateChromePolicy(policies);
  }

  void SetAndUpdateIsMetricsReporting(bool value) {
    is_metrics_reporting_enabled_ = value;
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &is_metrics_reporting_enabled_);
  }

 private:
  bool is_metrics_reporting_enabled_ = false;
};

IN_PROC_BROWSER_TEST_F(DomainReliabilityPolicyTest, PolicyOverride) {
  // Confirm that DomainReliabilityAllowed enterprise policy overrides existing
  // prefs
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kDomainReliabilityAllowedByPolicy, true);

  // Set policy to false and check pref
  SetAndUpdateDomainReliabilityAllowedPolicy(false);
  EXPECT_FALSE(prefs->GetBoolean(prefs::kDomainReliabilityAllowedByPolicy));

  // Set policy to true and check pref
  SetAndUpdateDomainReliabilityAllowedPolicy(true);
  EXPECT_TRUE(prefs->GetBoolean(prefs::kDomainReliabilityAllowedByPolicy));
}

IN_PROC_BROWSER_TEST_F(DomainReliabilityPolicyTest,
                       PolicyEnabledMetricsEnabled) {
  // Confirm behavior with policy true and metrics enabled
  SetAndUpdateDomainReliabilityAllowedPolicy(true);
  SetAndUpdateIsMetricsReporting(true);
  EXPECT_TRUE(domain_reliability::ShouldCreateService());
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(DomainReliabilityPolicyTest,
                       PolicyEnabledMetricsDisabled) {
  // Confirm behavior with policy true and metrics disabled
  SetAndUpdateDomainReliabilityAllowedPolicy(true);
  SetAndUpdateIsMetricsReporting(false);
  EXPECT_FALSE(domain_reliability::ShouldCreateService());
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(DomainReliabilityPolicyTest,
                       PolicyDisabledMetricsEnabled) {
  // Confirm behavior with policy false and metrics enabled
  SetAndUpdateDomainReliabilityAllowedPolicy(false);
  SetAndUpdateIsMetricsReporting(true);
  EXPECT_FALSE(domain_reliability::ShouldCreateService());
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(DomainReliabilityPolicyTest,
                       PolicyDisabledMetricsDisabled) {
  // Confirm behavior with policy false and metrics disabled
  SetAndUpdateDomainReliabilityAllowedPolicy(false);
  SetAndUpdateIsMetricsReporting(false);
  EXPECT_FALSE(domain_reliability::ShouldCreateService());
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(DomainReliabilityDisabledBrowserTest,
                       ServiceNotCreated) {
  EXPECT_FALSE(domain_reliability::ShouldCreateService());
}

IN_PROC_BROWSER_TEST_F(DomainReliabilityBrowserTest, ServiceCreated) {
  EXPECT_TRUE(domain_reliability::ShouldCreateService());
}

static const char kUploadPath[] = "/domainreliability/upload";

std::unique_ptr<net::test_server::HttpResponse> TestRequestHandler(
    int* request_count_out,
    std::string* last_request_content_out,
    const base::RepeatingClosure& quit_closure,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kUploadPath)
    return nullptr;

  ++*request_count_out;
  *last_request_content_out = request.has_content ? request.content : "";

  quit_closure.Run();

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content("");
  response->set_content_type("text/plain");
  return std::move(response);
}

IN_PROC_BROWSER_TEST_F(DomainReliabilityBrowserTest, Upload) {
  base::RunLoop run_loop;

  // Register a same-origin collector to receive report uploads so we can check
  // the full path. (Domain Reliability elides the path for privacy reasons when
  // uploading to non-same-origin collectors.)
  int request_count = 0;
  std::string last_request_content;
  test_server()->RegisterRequestHandler(
      base::BindRepeating(&TestRequestHandler, &request_count,
                          &last_request_content, run_loop.QuitClosure()));

  ASSERT_TRUE(test_server()->Start());

  GURL error_url = test_server()->GetURL("/close-socket");
  GURL upload_url = test_server()->GetURL(kUploadPath);

  {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    GetNetworkContext()->AddDomainReliabilityContextForTesting(  // IN-TEST
        test_server()->GetOrigin(), upload_url);
  }

  // Trigger an error.

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), error_url));

  {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    GetNetworkContext()->ForceDomainReliabilityUploadsForTesting();
  }

  run_loop.Run();

  EXPECT_EQ(1, request_count);
  EXPECT_NE("", last_request_content);

  auto body = base::JSONReader::Read(last_request_content);
  ASSERT_TRUE(body);
  ASSERT_TRUE(body->is_dict());

  const base::Value::Dict& dict = body->GetDict();

  const base::Value::List* entries = dict.FindList("entries");
  ASSERT_TRUE(entries);
  ASSERT_EQ(1u, entries->size());

  const base::Value& entry = (*entries)[0u];
  ASSERT_TRUE(entry.is_dict());
  ASSERT_TRUE(entry.GetDict().FindString("url"));
  EXPECT_EQ(*(entry.GetDict().FindString("url")), error_url);
}

IN_PROC_BROWSER_TEST_F(DomainReliabilityBrowserTest, UploadAtShutdown) {
  ASSERT_TRUE(test_server()->Start());

  GURL upload_url = test_server()->GetURL("/hung");
  {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    GetNetworkContext()->AddDomainReliabilityContextForTesting(  // IN-TEST
        url::Origin::Create(GURL("https://localhost/")), upload_url);
  }

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://localhost/")));

  {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    GetNetworkContext()->ForceDomainReliabilityUploadsForTesting();
  }

  // At this point, there is an upload pending. If everything goes well, the
  // test will finish, destroy the profile, and Domain Reliability will shut
  // down properly. If things go awry, it may crash as terminating the pending
  // upload calls into already-destroyed parts of the component.
}

// Ensures that there's no crash at NetworkContext shutdown if there are
// outstanding URLLoaders.
IN_PROC_BROWSER_TEST_F(DomainReliabilityBrowserTest, RequestAtShutdown) {
  ASSERT_TRUE(test_server()->Start());

  GURL hung_url = test_server()->GetURL("/hung");
  {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    GetNetworkContext()->AddDomainReliabilityContextForTesting(  // IN-TEST
        url::Origin::Create(hung_url), hung_url);
  }

  // Use a SimpleURLLoader so we can leak the mojo pipe, ensuring that URLLoader
  // doesn't see a connection error before NetworkContext does.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = hung_url;
  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), TRAFFIC_ANNOTATION_FOR_TESTS);
  auto* storage_partition = browser()->profile()->GetDefaultStoragePartition();
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      base::BindOnce([](std::unique_ptr<std::string> body) {}));

  simple_loader.release();
}

}  // namespace domain_reliability

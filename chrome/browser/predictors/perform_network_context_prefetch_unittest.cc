// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/perform_network_context_prefetch.h"

#include <stdint.h>

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "base/test/mock_log.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_host_resolver.h"
#include "net/base/schemeful_site.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace predictors {

namespace {

using net::test_server::EmbeddedTestServer;
using net::test_server::EmbeddedTestServerHandle;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using network::mojom::RequestDestination;
using ::testing::_;
using ::testing::A;
using ::testing::AnyOf;
using ::testing::ExplainMatchResult;
using ::testing::HasSubstr;
using ::testing::IsSupersetOf;
using ::testing::Pair;
using ::testing::StartsWith;
using ::testing::StrCaseEq;
using ::testing::StrictMock;

constexpr std::string_view kPagePath = "/page";
constexpr std::string_view kResourcePath = "/nocontent";
constexpr std::string_view kHostname = "a.test";

class PerformNetworkContextPrefetchRecorderTest : public ::testing::Test {
 public:
  PerformNetworkContextPrefetchRecorderTest() {
    features_.InitWithFeatures(
        /*enabled_features=*/
        {
            network::features::kNetworkContextPrefetch,
            features::kLoadingPredictorPrefetch,
            features::kPrefetchManagerUseNetworkContextPrefetch,
        },
        /*disabled_features=*/{});
    profile_ = std::make_unique<TestingProfile>();
  }

  void SetUp() override {
    host_resolver_.host_resolver()->AddRule("*", "127.0.0.1");
    test_server_.AddDefaultHandlers();
    test_server_.RegisterRequestMonitor(GetFutureCallback());
    // This SSL config is only needed for tests that test cross-origin behavior,
    // but it does no harm for the other tests.
    test_server_.SetSSLConfig(EmbeddedTestServer::CERT_TEST_NAMES);
    test_server_handle_ = test_server_.StartAndReturnHandle();
    ASSERT_TRUE(test_server_handle_);
    // Treat 127.0.0.1 as "public" to avoid being blocked by local network
    // access.
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        network::switches::kIpAddressSpaceOverrides,
        base::StringPrintf("127.0.0.1:%d=public", test_server_.port()));
  }

  GURL PageURL(std::string_view hostname = kHostname) const {
    return test_server_.GetURL(hostname, kPagePath);
  }

  GURL ResourceURL(std::string_view hostname = kHostname) const {
    return test_server_.GetURL(hostname, kResourcePath);
  }

  void DoPrefetch(RequestDestination destination,
                  std::string_view page_path = kPagePath,
                  std::string_view resource_path = kResourcePath) {
    DoPrefetch(destination, test_server_.GetURL(kHostname, page_path),
               test_server_.GetURL(kHostname, resource_path));
  }

  void DoPrefetch(RequestDestination destination,
                  const GURL& page_url,
                  const GURL& resource_url) {
    DoPrefetches(destination, page_url, {resource_url});
  }

  void DoPrefetches(RequestDestination destination,
                    const GURL& page_url,
                    const std::vector<GURL>& resources) {
    const net::SchemefulSite site(page_url);
    const auto nak = net::NetworkAnonymizationKey::CreateSameSite(site);
    auto requests = base::ToVector(resources, [&](const GURL& resource_url) {
      return PrefetchRequest(resource_url, nak, destination);
    });
    PerformNetworkContextPrefetch(profile_.get(), page_url,
                                  std::move(requests));
  }

  HttpRequest GetRequest() { return request_future_.Get(); }

  void ExpectNoRequest() {
    EXPECT_FALSE(request_future_.IsReady());
    // Unfortunately the only way to wait for nothing to happen is to use a
    // delay.
    static constexpr auto kWaitFor = base::Milliseconds(100);
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), kWaitFor);
    run_loop.Run();
    EXPECT_FALSE(request_future_.IsReady());
  }

  EmbeddedTestServer::MonitorRequestCallback GetFutureCallback() {
    return request_future_.GetSequenceBoundRepeatingCallback();
  }

 private:
  base::test::ScopedCommandLine command_line_;
  base::test::ScopedFeatureList features_;
  // IO_MAINLOOP is needed for the EmbeddedTestServer.
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::IO_MAINLOOP};
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  std::unique_ptr<TestingProfile> profile_;
  base::test::TestFuture<const HttpRequest&> request_future_;
  content::TestHostResolver host_resolver_;
  EmbeddedTestServer test_server_{EmbeddedTestServer::TYPE_HTTPS};
  EmbeddedTestServerHandle test_server_handle_;
};

// The combination of matchers required to match a header value is long and
// hard-to-read, so provide this short-cut. `value_matcher` can either be a
// literal string to compare equality, or any matcher.
template <typename ValueMatcher>
auto HasHeader(std::string_view name, ValueMatcher value_matcher) {
  return Contains(Pair(StrCaseEq(name), value_matcher));
}

TEST_F(PerformNetworkContextPrefetchRecorderTest, Script) {
  DoPrefetch(RequestDestination::kScript);
  const auto request = GetRequest();
  EXPECT_EQ(request.relative_url, "/nocontent");
  EXPECT_EQ(request.method, net::test_server::METHOD_GET);
  EXPECT_EQ(request.method_string, "GET");

  // Intentionally don't test headers that are added by the network layer, as
  // changes there shouldn't require changing this test.
  EXPECT_THAT(request.headers, HasHeader("Accept", "*/*"));
  EXPECT_THAT(request.headers, HasHeader("Accept-Language", "en"));
  EXPECT_THAT(request.headers, HasHeader("Purpose", "prefetch"));
  EXPECT_THAT(request.headers, HasHeader("Referer", PageURL().spec()));
  EXPECT_THAT(request.headers, HasHeader("sec-ch-ua", HasSubstr("v=")));
  EXPECT_THAT(request.headers,
              HasHeader("sec-ch-ua-mobile", AnyOf("?0", "?1")));
  EXPECT_THAT(request.headers,
              HasHeader("sec-ch-ua-platform", A<std::string>()));
  EXPECT_THAT(request.headers, HasHeader("Sec-Fetch-Dest", "script"));
  EXPECT_THAT(request.headers, HasHeader("Sec-Fetch-Mode", "no-cors"));
  EXPECT_THAT(request.headers, HasHeader("Sec-Fetch-Site", "same-origin"));
  EXPECT_THAT(request.headers, HasHeader("Sec-Purpose", "prefetch"));
  EXPECT_THAT(request.headers,
              HasHeader("User-Agent", StartsWith("Mozilla/5.0 ")));

  EXPECT_TRUE(request.content.empty());
}

TEST_F(PerformNetworkContextPrefetchRecorderTest, Style) {
  DoPrefetch(RequestDestination::kStyle);
  const auto request = GetRequest();

  // Only test things that differ from the previous test.
  EXPECT_THAT(request.headers, HasHeader("Accept", "text/css,*/*;q=0.1"));
  EXPECT_THAT(request.headers, HasHeader("Sec-Fetch-Dest", "style"));
}

class InsecureTestServer final {
 public:
  explicit InsecureTestServer(const EmbeddedTestServer::MonitorRequestCallback&
                                  monitor_request_callback) {
    server_.AddDefaultHandlers();
    server_.RegisterRequestMonitor(monitor_request_callback);
    handle_ = server_.StartAndReturnHandle();
    EXPECT_TRUE(handle_);
  }

  GURL GetURL(std::string_view path) const { return server_.GetURL(path); }

 private:
  EmbeddedTestServer server_{EmbeddedTestServer::TYPE_HTTP};
  EmbeddedTestServerHandle handle_;
};

constexpr auto kERROR = ::logging::LOGGING_ERROR;

TEST_F(PerformNetworkContextPrefetchRecorderTest, NonSSLPage) {
  InsecureTestServer insecure(GetFutureCallback());
  {
    StrictMock<base::test::MockLog> log;
    if (DLOG_IS_ON(ERROR)) {
      EXPECT_CALL(log, Log(kERROR, _, _, _, HasSubstr("SSL")));
    }
    log.StartCapturingLogs();

    DoPrefetch(RequestDestination::kStyle, insecure.GetURL("/"),
               insecure.GetURL("/style.css"));
  }
  ExpectNoRequest();
}

TEST_F(PerformNetworkContextPrefetchRecorderTest, NonSSLResource) {
  InsecureTestServer insecure(GetFutureCallback());
  {
    StrictMock<base::test::MockLog> log;
    if (DLOG_IS_ON(ERROR)) {
      EXPECT_CALL(log, Log(kERROR, _, _, _, HasSubstr("SSL")));
    }
    log.StartCapturingLogs();

    // Do two fetches, so that the second one succeeds.
    DoPrefetches(RequestDestination::kStyle, PageURL(),
                 {insecure.GetURL("/style.css"), ResourceURL()});
  }

  const auto request = GetRequest();

  // The first request should have been skipped and only the second one issued.
  EXPECT_TRUE(request.base_url.SchemeIs("https"));
  EXPECT_EQ(request.relative_url, kResourcePath);
}

TEST_F(PerformNetworkContextPrefetchRecorderTest, ReferrerSameOrigin) {
  DoPrefetch(RequestDestination::kStyle);
  const auto request = GetRequest();

  EXPECT_THAT(request.headers, HasHeader("Referer", PageURL().spec()));
}

TEST_F(PerformNetworkContextPrefetchRecorderTest, ReferrerCrossOrigin) {
  // These are both included in CERT_TEST_NAMES
  constexpr char kPageHostname[] = "a.test";
  constexpr char kResourceHostname[] = "b.test";

  DoPrefetch(RequestDestination::kStyle, PageURL(kPageHostname),
             ResourceURL(kResourceHostname));

  const auto request = GetRequest();

  const auto page_origin = url::Origin::Create(PageURL(kPageHostname));
  const auto expected_referrer = page_origin.Serialize() + "/";

  EXPECT_THAT(request.headers, HasHeader("Referer", expected_referrer));
  EXPECT_THAT(request.headers, HasHeader("Sec-Fetch-Site", "cross-site"));
}

}  // namespace

}  // namespace predictors

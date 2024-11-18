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

#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/schemeful_site.h"
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

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using network::mojom::RequestDestination;
using ::testing::A;
using ::testing::AnyOf;
using ::testing::ExplainMatchResult;
using ::testing::HasSubstr;
using ::testing::IsSupersetOf;
using ::testing::Pair;
using ::testing::StartsWith;
using ::testing::StrCaseEq;

constexpr std::string_view kPagePath = "/";
constexpr std::string_view kResourcePath = "/nocontent";

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
    test_server_.AddDefaultHandlers();
    test_server_.RegisterRequestMonitor(
        request_future_.GetSequenceBoundRepeatingCallback());
    test_server_handle_ = test_server_.StartAndReturnHandle();
    ASSERT_TRUE(test_server_handle_);
    // Treat 127.0.0.1 as "public" to avoid being blocked by local network
    // access.
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        network::switches::kIpAddressSpaceOverrides,
        base::StringPrintf("127.0.0.1:%d=public", test_server_.port()));
  }

  GURL GetURL() const { return test_server_.GetURL(kResourcePath); }

  void DoPrefetch(RequestDestination destination) {
    const GURL page_url = test_server_.GetURL(kPagePath);
    const net::SchemefulSite site(page_url);
    const auto nak = net::NetworkAnonymizationKey::CreateSameSite(site);
    std::vector<PrefetchRequest> requests = {
        PrefetchRequest(GetURL(), nak, destination)};
    PerformNetworkContextPrefetch(profile_.get(), page_url,
                                  std::move(requests));
  }

  HttpRequest GetRequest() { return request_future_.Get(); }

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
  net::test_server::EmbeddedTestServer test_server_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
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
  auto request = GetRequest();
  EXPECT_EQ(request.relative_url, "/nocontent");
  EXPECT_EQ(request.method, net::test_server::METHOD_GET);
  EXPECT_EQ(request.method_string, "GET");

  // Intentionally don't test headers that are added by the network layer, as
  // changes there shouldn't require changing this test.
  EXPECT_THAT(request.headers, HasHeader("Accept", "*/*"));
  EXPECT_THAT(request.headers, HasHeader("Accept-Language", "en"));
  EXPECT_THAT(request.headers, HasHeader("Purpose", "prefetch"));
  EXPECT_THAT(request.headers,
              HasHeader("Referer", StartsWith("http://127.0.0.1:")));
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
  auto request = GetRequest();

  // Only test things that differ from the previous test.
  EXPECT_THAT(request.headers, HasHeader("Accept", "text/css,*/*;q=0.1"));
  EXPECT_THAT(request.headers, HasHeader("Sec-Fetch-Dest", "style"));
}

}  // namespace

}  // namespace predictors

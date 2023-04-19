// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cctype>
#include <cstddef>
#include <memory>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/client_hints/common/client_hints.h"
#include "components/client_hints/common/switches.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/embedder_support/switches.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_hints.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/nqe/effective_connection_type.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/url_request/url_request.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/origin.h"

namespace {

using ::content::URLLoaderInterceptor;
using ::net::test_server::EmbeddedTestServer;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Key;
using ::testing::Not;
using ::testing::Optional;

constexpr unsigned expected_client_hints_number = 19u;
constexpr unsigned expected_default_third_party_client_hints_number = 3u;
constexpr unsigned expected_requested_third_party_client_hints_number = 22u;
constexpr unsigned expected_pre_merge_third_party_client_hints_number = 14u;

// All of the status codes from HttpResponseHeaders::IsRedirectResponseCode.
const net::HttpStatusCode kRedirectStatusCodes[] = {
    net::HTTP_MOVED_PERMANENTLY,  net::HTTP_FOUND,
    net::HTTP_SEE_OTHER,          net::HTTP_TEMPORARY_REDIRECT,
    net::HTTP_PERMANENT_REDIRECT,
};

// An interceptor that records count of fetches and client hint headers for
// requests to https://{foo|bar}.com/non-existing-{image.jpg|iframe.html}.
class ThirdPartyURLLoaderInterceptor {
 public:
  explicit ThirdPartyURLLoaderInterceptor(const std::set<GURL> intercepted_urls)
      : intercepted_urls_(intercepted_urls),
        interceptor_(base::BindRepeating(
            &ThirdPartyURLLoaderInterceptor::InterceptURLRequest,
            base::Unretained(this))) {}

  ThirdPartyURLLoaderInterceptor(const ThirdPartyURLLoaderInterceptor&) =
      delete;
  ThirdPartyURLLoaderInterceptor& operator=(
      const ThirdPartyURLLoaderInterceptor&) = delete;

  ~ThirdPartyURLLoaderInterceptor() = default;

  size_t request_count_seen() const { return request_count_seen_; }

  size_t client_hints_count_seen() const { return client_hints_count_seen_; }

  size_t unique_request_count_seen() const {
    return unique_request_count_seen_;
  }

  size_t client_hints_count_seen_on_unique_request() const {
    return client_hints_count_seen_on_unique_request_;
  }

 private:
  bool InterceptURLRequest(URLLoaderInterceptor::RequestParams* params) {
    if (intercepted_urls_.find(params->url_request.url) ==
        intercepted_urls_.end()) {
      return false;
    }

    bool url_has_not_visited =
        visited_urls_.insert(params->url_request.url).second;

    request_count_seen_++;

    if (url_has_not_visited) {
      unique_request_count_seen_++;
    }

    for (const auto& elem : network::GetClientHintToNameMap()) {
      const auto& header = elem.second;
      if (params->url_request.headers.HasHeader(header)) {
        client_hints_count_seen_++;

        if (url_has_not_visited) {
          client_hints_count_seen_on_unique_request_++;
        }
      }
    }
    return false;
  }

  std::set<GURL> intercepted_urls_;

  size_t request_count_seen_ = 0u;

  size_t client_hints_count_seen_ = 0u;

  URLLoaderInterceptor interceptor_;

  // Count to deduplicate third-party requests since the total number of third
  // party request can be flaky on JS injected requests.
  std::set<GURL> visited_urls_;
  size_t unique_request_count_seen_ = 0u;
  size_t client_hints_count_seen_on_unique_request_ = 0u;
};

// Returns true only if `header_value` satisfies ABNF: 1*DIGIT [ "." 1*DIGIT ]
bool IsSimilarToDoubleABNF(const std::string& header_value) {
  if (header_value.empty())
    return false;
  char first_char = header_value.at(0);
  if (!isdigit(first_char))
    return false;

  bool period_found = false;
  bool digit_found_after_period = false;
  for (char ch : header_value) {
    if (isdigit(ch)) {
      if (period_found) {
        digit_found_after_period = true;
      }
      continue;
    }
    if (ch == '.') {
      if (period_found)
        return false;
      period_found = true;
      continue;
    }
    return false;
  }
  if (period_found)
    return digit_found_after_period;
  return true;
}

// Returns true only if `header_value` satisfies ABNF: 1*DIGIT
bool IsSimilarToIntABNF(const std::string& header_value) {
  if (header_value.empty())
    return false;

  for (char ch : header_value) {
    if (!isdigit(ch))
      return false;
  }
  return true;
}

// Return |true| in the following conditions: If we expect reduced user agent,
// user agent minor version matches "0.0.0" if reduced UA through UAReduction
// origin trial. or user agent minor version matches "0.X.0" if reduced UA
// through kReduceUserAgentMinorVersion experiment. Otherwise, return |false|.
// We should not always expect reduced UA when kReduceUserAgentMinorVersion
// feature turns on, it would give false positive test results when the feature
// turns on as default. For example, if we expect full UA in the UADeprecation
// origin trial with kReduceUserAgentMinorVersion turned on, the actual value
// gives reduced UA, and the validation will succeed in this case which causes
// us to ignore actual bugs in code.
void CheckUserAgentMinorVersion(
    const std::string& user_agent_value,
    const bool expected_user_agent_reduced,
    const bool expected_reduced_ua_through_experiment) {
  // A regular expression that matches Chrome/{major_version}.{minor_version}
  // in the User-Agent string, where the {minor_version} is captured.
  static constexpr char kChromeVersionRegex[] =
      "Chrome/[0-9]+\\.([0-9]+\\.[0-9]+\\.[0-9]+)";
  // The minor version in the reduced UA string is always "0.0.0".
  static constexpr char kReducedMinorVersion[] = "0.0.0";
  // The minor version in the ReduceUserAgentMinorVersion experiment is always
  // "0.X.0", where X is the frozen build version.
  const std::string kReduceUserAgentMinorVersion =
      "0." +
      std::string(blink::features::kUserAgentFrozenBuildVersion.Get().data()) +
      ".0";

  std::string minor_version;
  EXPECT_TRUE(re2::RE2::PartialMatch(user_agent_value, kChromeVersionRegex,
                                     &minor_version));

  if (expected_user_agent_reduced) {
    EXPECT_EQ(minor_version, expected_reduced_ua_through_experiment
                                 ? kReduceUserAgentMinorVersion
                                 : kReducedMinorVersion);
  } else {
    EXPECT_NE(minor_version, kReducedMinorVersion);
  }
}

// A helper function that returns true when the legacy GREASE implementation is
// seen. It relies on the old algorithm having only 3 possible permutations due
// to a very limited set of allowed special characters. This may be removed once
// the legacy algorithm is no longer supported for emergency situations.
bool SawOldGrease(const std::string& ua_ch_result) {
  bool seen_legacy = false;
  // The legacy GREASE algorithm had only semicolon and space, and thus had one
  // of these three permutations.
  const std::string old_grease_permutations[]{";Not A Brand", " Not;A Brand",
                                              " Not A;Brand"};
  for (auto i : old_grease_permutations) {
    seen_legacy = seen_legacy || (ua_ch_result.find(i) != std::string::npos);
  }
  return seen_legacy;
}

// A helper function to determine whether the GREASE algorithm per the spec:
// https://wicg.github.io/ua-client-hints/#create-arbitrary-brands-section
// was observed in the client hint user agent header.
bool SawUpdatedGrease(const std::string& ua_ch_result) {
  // The updated GREASE algorithm would contain at least two of these
  // characters.
  static constexpr char kUpdatedGreaseRegex[] =
      "Not[ ()\\-.\\/:;=?_]A[ ()\\-.\\/:;=?_]Brand";
  return re2::RE2::PartialMatch(ua_ch_result, kUpdatedGreaseRegex);
}

enum class UserAgentOriginTrialTestType {
  UAReduction,
  UADeprecation,
  UAReductionAndDeprecation
};

struct OriginTrialTestOptions {
  bool has_ot_token = true;
  bool valid_ot_token = true;
  bool has_accept_ch_header = true;
  bool has_critical_ch_header = false;
};

class AlternatingCriticalCHRequestHandler {
 public:
  AlternatingCriticalCHRequestHandler() = default;
  net::test_server::EmbeddedTestServer::HandleRequestCallback
  GetRequestHandler() {
    return base::BindRepeating(
        &AlternatingCriticalCHRequestHandler::DifferentCriticalCH,
        base::Unretained(this));
  }

  int request_count() { return request_count_; }

  void SetRedirectLocation(const GURL& redirect_location) {
    redirect_location_ = redirect_location;
  }

  void SetStatusCode(net::HttpStatusCode status_code) {
    status_code_ = status_code;
  }

  static constexpr char kCriticalCH[] = "/critical-ch";

 private:
  // A response that flips between two critical-ch headers
  std::unique_ptr<net::test_server::HttpResponse> DifferentCriticalCH(
      const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(request.relative_url, kCriticalCH))
      return nullptr;

    request_count_++;

    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();

    if (redirect_location_) {
      response->set_code(status_code_);
      response->AddCustomHeader("Location", redirect_location_->spec());
    }

    // Always send client hints different from what were received.
    if (request.headers.find(GetCHToken()) != request.headers.end())
      critical_ch_state_ = !critical_ch_state_;

    response->AddCustomHeader("Accept-CH", GetCHToken());
    response->AddCustomHeader("Critical-CH", GetCHToken());

    return std::move(response);
  }

  std::string GetCHToken() {
    return critical_ch_state_ ? "sec-ch-ua-arch" : "sec-ch-ua-bitness";
  }

  bool critical_ch_state_ = true;
  int request_count_ = 0;
  absl::optional<GURL> redirect_location_;
  net::HttpStatusCode status_code_ = net::HTTP_TEMPORARY_REDIRECT;
};

void ExpectUKMSeen(const ukm::TestAutoSetUkmRecorder& ukm_recorder,
                   const std::vector<network::mojom::WebClientHintsType>& hints,
                   size_t loads,
                   const base::StringPiece metric_name,
                   const base::StringPiece type_name) {
  auto ukm_entries = ukm_recorder.GetEntriesByName(metric_name);
  // We expect the same series of `hints` to appear `loads` times.
  ASSERT_EQ(ukm_entries.size(), hints.size() * loads);
  for (size_t hint = 0; hint < hints.size(); ++hint) {
    for (size_t load = 0; load < loads; ++load) {
      EXPECT_EQ(*ukm_recorder.GetEntryMetric(
                    ukm_entries[hint + load * hints.size()], type_name),
                static_cast<int64_t>(hints[hint]));
    }
  }
}

void ExpectAcceptCHHeaderUKMSeen(
    const ukm::TestAutoSetUkmRecorder& ukm_recorder,
    const std::vector<network::mojom::WebClientHintsType>& hints,
    size_t loads) {
  ExpectUKMSeen(ukm_recorder, hints, loads,
                ukm::builders::ClientHints_AcceptCHHeaderUsage::kEntryName,
                ukm::builders::ClientHints_AcceptCHHeaderUsage::kTypeName);
}

void ExpectCriticalCHHeaderUKMSeen(
    const ukm::TestAutoSetUkmRecorder& ukm_recorder,
    const std::vector<network::mojom::WebClientHintsType>& hints,
    size_t loads) {
  ExpectUKMSeen(ukm_recorder, hints, loads,
                ukm::builders::ClientHints_CriticalCHHeaderUsage::kEntryName,
                ukm::builders::ClientHints_CriticalCHHeaderUsage::kTypeName);
}

void ExpectAcceptCHMetaUKMSeen(
    const ukm::TestAutoSetUkmRecorder& ukm_recorder,
    const std::vector<network::mojom::WebClientHintsType>& hints,
    size_t loads) {
  ExpectUKMSeen(ukm_recorder, hints, loads,
                ukm::builders::ClientHints_AcceptCHMetaUsage::kEntryName,
                ukm::builders::ClientHints_AcceptCHMetaUsage::kTypeName);
}

void ExpectDelegateCHMetaUKMSeen(
    const ukm::TestAutoSetUkmRecorder& ukm_recorder,
    const std::vector<network::mojom::WebClientHintsType>& hints,
    size_t loads) {
  ExpectUKMSeen(ukm_recorder, hints, loads,
                ukm::builders::ClientHints_DelegateCHMetaUsage::kEntryName,
                ukm::builders::ClientHints_DelegateCHMetaUsage::kTypeName);
}

const std::vector<network::mojom::WebClientHintsType> kStandardHTTPHeaderHints(
    {network::mojom::WebClientHintsType::kDpr_DEPRECATED,
     network::mojom::WebClientHintsType::kDpr,
     network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
     network::mojom::WebClientHintsType::kDeviceMemory,
     network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED,
     network::mojom::WebClientHintsType::kViewportWidth,
     network::mojom::WebClientHintsType::kRtt_DEPRECATED,
     network::mojom::WebClientHintsType::kDownlink_DEPRECATED,
     network::mojom::WebClientHintsType::kEct_DEPRECATED,
     network::mojom::WebClientHintsType::kUAArch,
     network::mojom::WebClientHintsType::kUAPlatformVersion,
     network::mojom::WebClientHintsType::kUAModel,
     network::mojom::WebClientHintsType::kUAFullVersion,
     network::mojom::WebClientHintsType::kPrefersColorScheme,
     network::mojom::WebClientHintsType::kPrefersReducedMotion,
     network::mojom::WebClientHintsType::kUABitness,
     network::mojom::WebClientHintsType::kViewportHeight,
     network::mojom::WebClientHintsType::kUAFullVersionList,
     network::mojom::WebClientHintsType::kUAWoW64});

const std::vector<network::mojom::WebClientHintsType>
    kStandardAcceptCHMetaHints(
        {network::mojom::WebClientHintsType::kDpr_DEPRECATED,
         network::mojom::WebClientHintsType::kDpr,
         network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
         network::mojom::WebClientHintsType::kDeviceMemory,
         network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED,
         network::mojom::WebClientHintsType::kViewportWidth,
         network::mojom::WebClientHintsType::kRtt_DEPRECATED,
         network::mojom::WebClientHintsType::kDownlink_DEPRECATED,
         network::mojom::WebClientHintsType::kEct_DEPRECATED,
         network::mojom::WebClientHintsType::kUAArch,
         network::mojom::WebClientHintsType::kUAPlatform,
         network::mojom::WebClientHintsType::kUAModel,
         network::mojom::WebClientHintsType::kUAFullVersion,
         network::mojom::WebClientHintsType::kUABitness,
         network::mojom::WebClientHintsType::kUAFullVersionList,
         network::mojom::WebClientHintsType::kUAWoW64});

const std::vector<network::mojom::WebClientHintsType>
    kStandardDelegateCHMetaHints(
        {network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
         network::mojom::WebClientHintsType::kDpr_DEPRECATED,
         network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED,
         network::mojom::WebClientHintsType::kRtt_DEPRECATED,
         network::mojom::WebClientHintsType::kDownlink_DEPRECATED,
         network::mojom::WebClientHintsType::kEct_DEPRECATED,
         network::mojom::WebClientHintsType::kUAArch,
         network::mojom::WebClientHintsType::kUAPlatform,
         network::mojom::WebClientHintsType::kUAModel,
         network::mojom::WebClientHintsType::kUAFullVersion,
         network::mojom::WebClientHintsType::kUABitness,
         network::mojom::WebClientHintsType::kDeviceMemory,
         network::mojom::WebClientHintsType::kDpr,
         network::mojom::WebClientHintsType::kViewportWidth,
         network::mojom::WebClientHintsType::kUAFullVersionList,
         network::mojom::WebClientHintsType::kUAWoW64});

const std::vector<network::mojom::WebClientHintsType>
    kExtendedAcceptCHMetaHints(
        {network::mojom::WebClientHintsType::kDpr_DEPRECATED,
         network::mojom::WebClientHintsType::kDpr,
         network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
         network::mojom::WebClientHintsType::kDeviceMemory,
         network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED,
         network::mojom::WebClientHintsType::kViewportWidth,
         network::mojom::WebClientHintsType::kRtt_DEPRECATED,
         network::mojom::WebClientHintsType::kDownlink_DEPRECATED,
         network::mojom::WebClientHintsType::kEct_DEPRECATED,
         network::mojom::WebClientHintsType::kUAArch,
         network::mojom::WebClientHintsType::kUAPlatform,
         network::mojom::WebClientHintsType::kUAPlatformVersion,
         network::mojom::WebClientHintsType::kUAModel,
         network::mojom::WebClientHintsType::kUAFullVersion,
         network::mojom::WebClientHintsType::kPrefersColorScheme,
         network::mojom::WebClientHintsType::kPrefersReducedMotion,
         network::mojom::WebClientHintsType::kUABitness,
         network::mojom::WebClientHintsType::kViewportHeight,
         network::mojom::WebClientHintsType::kUAFullVersionList,
         network::mojom::WebClientHintsType::kUAWoW64});

const std::vector<network::mojom::WebClientHintsType>
    kExtendedDelegateCHMetaHints(
        {network::mojom::WebClientHintsType::kDeviceMemory_DEPRECATED,
         network::mojom::WebClientHintsType::kDpr_DEPRECATED,
         network::mojom::WebClientHintsType::kViewportWidth_DEPRECATED,
         network::mojom::WebClientHintsType::kRtt_DEPRECATED,
         network::mojom::WebClientHintsType::kDownlink_DEPRECATED,
         network::mojom::WebClientHintsType::kEct_DEPRECATED,
         network::mojom::WebClientHintsType::kUAArch,
         network::mojom::WebClientHintsType::kUAPlatform,
         network::mojom::WebClientHintsType::kUAModel,
         network::mojom::WebClientHintsType::kUAFullVersion,
         network::mojom::WebClientHintsType::kUAPlatformVersion,
         network::mojom::WebClientHintsType::kPrefersColorScheme,
         network::mojom::WebClientHintsType::kUABitness,
         network::mojom::WebClientHintsType::kViewportHeight,
         network::mojom::WebClientHintsType::kDeviceMemory,
         network::mojom::WebClientHintsType::kDpr,
         network::mojom::WebClientHintsType::kViewportWidth,
         network::mojom::WebClientHintsType::kUAFullVersionList,
         network::mojom::WebClientHintsType::kUAWoW64,
         network::mojom::WebClientHintsType::kPrefersReducedMotion});
}  // namespace

class ClientHintsBrowserTest : public policy::PolicyTest {
 public:
  ClientHintsBrowserTest()
      : http_server_(net::EmbeddedTestServer::TYPE_HTTP),
        https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        https_cross_origin_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        http2_server_(net::EmbeddedTestServer::TYPE_HTTPS,
                      net::test_server::HttpConnection::Protocol::kHttp2),
        expect_client_hints_on_subresources_(false) {
    http_server_.ServeFilesFromSourceDirectory("chrome/test/data/client_hints");
    https_server_.ServeFilesFromSourceDirectory(
        "chrome/test/data/client_hints");
    https_cross_origin_server_.ServeFilesFromSourceDirectory(
        "chrome/test/data/client_hints");
    http2_server_.ServeFilesFromSourceDirectory("chrome/test/data");

    http_server_.RegisterRequestMonitor(
        base::BindRepeating(&ClientHintsBrowserTest::MonitorResourceRequest,
                            base::Unretained(this)));
    https_server_.RegisterRequestMonitor(
        base::BindRepeating(&ClientHintsBrowserTest::MonitorResourceRequest,
                            base::Unretained(this)));
    http2_server_.RegisterRequestMonitor(
        base::BindRepeating(&ClientHintsBrowserTest::MonitorResourceRequest,
                            base::Unretained(this)));
    https_cross_origin_server_.RegisterRequestMonitor(
        base::BindRepeating(&ClientHintsBrowserTest::MonitorResourceRequest,
                            base::Unretained(this)));
    https_cross_origin_server_.RegisterRequestHandler(
        base::BindRepeating(&ClientHintsBrowserTest::RequestHandlerToRedirect,
                            base::Unretained(this)));
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &ClientHintsBrowserTest::RequestHandlerToFetchCrossOriginIframe,
        base::Unretained(this)));

    http2_server_.AddDefaultHandlers();

    std::vector<std::string> accept_ch_tokens;
    for (const auto& pair : network::GetClientHintToNameMap())
      accept_ch_tokens.push_back(pair.second);

    http2_server_.SetAlpsAcceptCH("", base::JoinString(accept_ch_tokens, ","));

    EXPECT_TRUE(http_server_.Start());
    EXPECT_TRUE(https_server_.Start());
    EXPECT_TRUE(http2_server_.Start());
    EXPECT_TRUE(https_cross_origin_server_.Start());

    EXPECT_NE(https_server_.base_url(), https_cross_origin_server_.base_url());

    accept_ch_url_ = https_server_.GetURL("/accept_ch.html");
    http_equiv_accept_ch_url_ =
        https_server_.GetURL("/http_equiv_accept_ch.html");
    meta_equiv_delegate_ch_url_ =
        https_server_.GetURL("/meta_equiv_delegate_ch.html");

    without_accept_ch_url_ = https_server_.GetURL("/without_accept_ch.html");
    EXPECT_TRUE(without_accept_ch_url_.SchemeIsHTTPOrHTTPS());
    EXPECT_TRUE(without_accept_ch_url_.SchemeIsCryptographic());

    without_accept_ch_local_url_ =
        http_server_.GetURL("/without_accept_ch.html");
    EXPECT_TRUE(without_accept_ch_local_url_.SchemeIsHTTPOrHTTPS());
    EXPECT_FALSE(without_accept_ch_local_url_.SchemeIsCryptographic());

    without_accept_ch_img_localhost_ =
        https_server_.GetURL("/without_accept_ch_img_localhost.html");
    without_accept_ch_img_foo_com_ =
        https_server_.GetURL("/without_accept_ch_img_foo_com.html");
    accept_ch_with_iframe_url_ =
        https_server_.GetURL("/accept_ch_with_iframe.html");
    http_equiv_accept_ch_with_iframe_url_ =
        https_server_.GetURL("/http_equiv_accept_ch_with_iframe.html");
    meta_equiv_delegate_ch_with_iframe_url_ =
        https_server_.GetURL("/meta_equiv_delegate_ch_with_iframe.html");
    accept_ch_with_subresource_url_ =
        https_server_.GetURL("/accept_ch_with_subresource.html");
    http_equiv_accept_ch_with_subresource_url_ =
        https_server_.GetURL("/http_equiv_accept_ch_with_subresource.html");
    meta_equiv_delegate_ch_with_subresource_url_ =
        https_server_.GetURL("/meta_equiv_delegate_ch_with_subresource.html");
    accept_ch_with_subresource_iframe_url_ =
        https_server_.GetURL("/accept_ch_with_subresource_iframe.html");
    http_equiv_accept_ch_with_subresource_iframe_url_ = https_server_.GetURL(
        "/http_equiv_accept_ch_with_subresource_iframe."
        "html");
    meta_equiv_delegate_ch_with_subresource_iframe_url_ = https_server_.GetURL(
        "/meta_equiv_delegate_ch_with_subresource_iframe."
        "html");
    accept_ch_img_localhost_ =
        https_server_.GetURL("/accept_ch_img_localhost.html");
    http_equiv_accept_ch_img_localhost_ =
        https_server_.GetURL("/http_equiv_accept_ch_img_localhost.html");
    meta_equiv_delegate_ch_img_localhost_ =
        https_server_.GetURL("/meta_equiv_delegate_ch_img_localhost.html");

    redirect_url_ = https_cross_origin_server_.GetURL("/redirect.html");

    accept_ch_empty_ = https_server_.GetURL("/accept_ch_empty.html");
    http_equiv_accept_ch_injection_ =
        https_server_.GetURL("/http_equiv_accept_ch_injection.html");
    meta_equiv_delegate_ch_injection_ =
        https_server_.GetURL("/meta_equiv_delegate_ch_injection.html");
    http_equiv_accept_ch_delegation_foo_ =
        https_server_.GetURL("/http_equiv_accept_ch_delegation_foo.html");
    meta_equiv_delegate_ch_delegation_foo_ =
        https_server_.GetURL("/meta_equiv_delegate_ch_delegation_foo.html");
    http_equiv_accept_ch_delegation_bar_ =
        https_server_.GetURL("/http_equiv_accept_ch_delegation_bar.html");
    meta_equiv_delegate_ch_delegation_bar_ =
        https_server_.GetURL("/meta_equiv_delegate_ch_delegation_bar.html");
    http_equiv_accept_ch_delegation_merge_ =
        https_server_.GetURL("/http_equiv_accept_ch_delegation_merge.html");
    meta_equiv_delegate_ch_delegation_merge_ =
        https_server_.GetURL("/meta_equiv_delegate_ch_delegation_merge.html");
    http_equiv_accept_ch_merge_ =
        https_server_.GetURL("/http_equiv_accept_ch_merge.html");
    meta_equiv_delegate_ch_merge_ =
        https_server_.GetURL("/meta_equiv_delegate_ch_merge.html");

    without_accept_ch_cross_origin_ =
        https_cross_origin_server_.GetURL("/without_accept_ch.html");
  }

  ClientHintsBrowserTest(const ClientHintsBrowserTest&) = delete;
  ClientHintsBrowserTest& operator=(const ClientHintsBrowserTest&) = delete;

  ~ClientHintsBrowserTest() override {}

  virtual std::unique_ptr<base::FeatureList> EnabledFeatures() {
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine(
        "UserAgentClientHint,CriticalClientHint,AcceptCHFrame", "");
    return feature_list;
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatureList(EnabledFeatures());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    request_interceptor_ = std::make_unique<ThirdPartyURLLoaderInterceptor>(
        (std::set<GURL>){GURL("https://foo.com/non-existing-image.jpg"),
                         GURL("https://foo.com/non-existing-iframe.html"),
                         GURL("https://bar.com/non-existing-image.jpg"),
                         GURL("https://bar.com/non-existing-iframe.html")});
    base::RunLoop().RunUntilIdle();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void TearDownOnMainThread() override { request_interceptor_.reset(); }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitchASCII(network::switches::kForceEffectiveConnectionType,
                           net::kEffectiveConnectionType2G);
  }

  void SetClientHintExpectationsOnMainFrame(bool expect_client_hints) {
    expect_client_hints_on_main_frame_ = expect_client_hints;
  }

  void SetClientHintExpectationsOnSubresources(bool expect_client_hints) {
    base::AutoLock lock(expect_client_hints_on_subresources_lock_);
    expect_client_hints_on_subresources_ = expect_client_hints;
  }

  bool expect_client_hints_on_subresources() {
    base::AutoLock lock(expect_client_hints_on_subresources_lock_);
    return expect_client_hints_on_subresources_;
  }

  // Verify that the user is not notified that cookies or JavaScript were
  // blocked on the webpage due to the checks done by client hints.
  void VerifyContentSettingsNotNotified() const {
    auto* pscs = content_settings::PageSpecificContentSettings::GetForFrame(
        browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetPrimaryMainFrame());
    EXPECT_FALSE(pscs->IsContentBlocked(ContentSettingsType::COOKIES));
    EXPECT_FALSE(pscs->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
  }

  void SetExpectedEffectiveConnectionType(
      net::EffectiveConnectionType effective_connection_type) {
    expected_ect = effective_connection_type;
  }

  void SetJsEnabledForActiveView(bool enabled) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    blink::web_pref::WebPreferences prefs =
        web_contents->GetOrCreateWebPreferences();
    prefs.javascript_enabled = enabled;
    web_contents->SetWebPreferences(prefs);
  }

  void TestProfilesIndependent(Browser* browser_a, Browser* browser_b);
  void TestSwitchWithNewProfile(const std::string& switch_value,
                                size_t origins_stored);

  // A URL whose response headers include only Accept-CH header.
  const GURL& accept_ch_url() const { return accept_ch_url_; }
  const GURL& http_equiv_accept_ch_url() const {
    return http_equiv_accept_ch_url_;
  }
  const GURL& meta_equiv_delegate_ch_url() const {
    return meta_equiv_delegate_ch_url_;
  }

  // A URL whose response headers do not include the Accept-CH header.
  // Navigating to this URL also fetches an image.
  const GURL& without_accept_ch_url() const { return without_accept_ch_url_; }

  // A URL whose response headers do not include the Accept-CH header.
  // Navigating to this URL also fetches an image.
  const GURL& without_accept_ch_local_url() const {
    return without_accept_ch_local_url_;
  }

  // A URL whose response headers do not include the Accept-CH header.
  // Navigating to this URL also fetches an image from localhost.
  const GURL& without_accept_ch_img_localhost() const {
    return without_accept_ch_img_localhost_;
  }

  // A URL whose response headers do not include the Accept-CH header.
  // Navigating to this URL also fetches an image from foo.com.
  const GURL& without_accept_ch_img_foo_com() const {
    return without_accept_ch_img_foo_com_;
  }

  // A URL whose response does not include the Accept-CH header. The response
  // loads accept_ch_url() in an iframe.
  const GURL& accept_ch_with_iframe_url() const {
    return accept_ch_with_iframe_url_;
  }
  const GURL& http_equiv_accept_ch_with_iframe_url() const {
    return http_equiv_accept_ch_with_iframe_url_;
  }
  const GURL& meta_equiv_delegate_ch_with_iframe_url() const {
    return meta_equiv_delegate_ch_with_iframe_url_;
  }

  // A URL whose response does not include the Accept-CH header. The response
  // loads accept_ch_url() as a subresource in the main frame.
  const GURL& accept_ch_with_subresource_url() const {
    return accept_ch_with_subresource_url_;
  }
  const GURL& http_equiv_accept_ch_with_subresource_url() const {
    return http_equiv_accept_ch_with_subresource_url_;
  }
  const GURL& meta_equiv_delegate_ch_with_subresource_url() const {
    return meta_equiv_delegate_ch_with_subresource_url_;
  }

  // A URL whose response does not include the Accept-CH header. The response
  // loads accept_ch_url() or {http_equiv|meta_name}_accept_ch_url() as a
  // subresource in the iframe.
  const GURL& accept_ch_with_subresource_iframe_url() const {
    return accept_ch_with_subresource_iframe_url_;
  }
  const GURL& http_equiv_accept_ch_with_subresource_iframe_url() const {
    return http_equiv_accept_ch_with_subresource_iframe_url_;
  }
  const GURL& meta_equiv_delegate_ch_with_subresource_iframe_url() const {
    return meta_equiv_delegate_ch_with_subresource_iframe_url_;
  }

  // A URL whose response includes only Accept-CH header. Navigating to
  // this URL also fetches two images: One from the localhost, and one from
  // foo.com.
  const GURL& accept_ch_img_localhost() const {
    return accept_ch_img_localhost_;
  }
  const GURL& http_equiv_accept_ch_img_localhost() const {
    return http_equiv_accept_ch_img_localhost_;
  }
  const GURL& meta_equiv_delegate_ch_img_localhost() const {
    return meta_equiv_delegate_ch_img_localhost_;
  }

  const GURL& redirect_url() const { return redirect_url_; }

  // A URL to a page with a response containing an empty accept_ch header.
  const GURL& accept_ch_empty() const { return accept_ch_empty_; }

  // A page where hints are injected via javascript into an http-equiv meta tag.
  const GURL& http_equiv_accept_ch_injection() const {
    return http_equiv_accept_ch_injection_;
  }
  const GURL& meta_equiv_delegate_ch_injection() const {
    return meta_equiv_delegate_ch_injection_;
  }

  // A page where hints are delegated to the third-party site `foo.com`.
  const GURL& http_equiv_accept_ch_delegation_foo() const {
    return http_equiv_accept_ch_delegation_foo_;
  }
  const GURL& meta_equiv_delegate_ch_delegation_foo() const {
    return meta_equiv_delegate_ch_delegation_foo_;
  }

  // A page where hints are delegated to the third-party site `bar.com`.
  const GURL& http_equiv_accept_ch_delegation_bar() const {
    return http_equiv_accept_ch_delegation_bar_;
  }
  const GURL& meta_equiv_delegate_ch_delegation_bar() const {
    return meta_equiv_delegate_ch_delegation_bar_;
  }

  // A page where hints are delegated to the third-party sites in HTTP and HTML.
  const GURL& http_equiv_accept_ch_delegation_merge() const {
    return http_equiv_accept_ch_delegation_merge_;
  }
  const GURL& meta_equiv_delegate_ch_delegation_merge() const {
    return meta_equiv_delegate_ch_delegation_merge_;
  }

  // A page where some hints are in accept-ch header, some in http-equiv.
  const GURL& http_equiv_accept_ch_merge() const {
    return http_equiv_accept_ch_merge_;
  }
  const GURL& meta_equiv_delegate_ch_merge() const {
    return meta_equiv_delegate_ch_merge_;
  }

  const GURL& without_accept_ch_cross_origin() {
    return without_accept_ch_cross_origin_;
  }

  GURL GetHttp2Url(const std::string& relative_url) const {
    return http2_server_.GetURL(relative_url);
  }

  size_t count_user_agent_hint_headers_seen() const {
    base::AutoLock lock(count_headers_lock_);
    return count_user_agent_hint_headers_seen_;
  }

  size_t count_ua_mobile_client_hints_headers_seen() const {
    base::AutoLock lock(count_headers_lock_);
    return count_ua_mobile_client_hints_headers_seen_;
  }

  size_t count_ua_platform_client_hints_headers_seen() const {
    base::AutoLock lock(count_headers_lock_);
    return count_ua_platform_client_hints_headers_seen_;
  }

  size_t count_save_data_client_hints_headers_seen() const {
    base::AutoLock lock(count_headers_lock_);
    return count_save_data_client_hints_headers_seen_;
  }

  size_t count_client_hints_headers_seen() const {
    base::AutoLock lock(count_headers_lock_);
    return count_client_hints_headers_seen_;
  }

  size_t third_party_request_count_seen() const {
    return request_interceptor_->request_count_seen();
  }

  size_t third_party_client_hints_count_seen() const {
    return request_interceptor_->client_hints_count_seen();
  }

  size_t third_party_unique_request_count_seen() const {
    return request_interceptor_->unique_request_count_seen();
  }

  size_t third_party_client_hints_count_seen_on_unique_request() const {
    return request_interceptor_->client_hints_count_seen_on_unique_request();
  }

  const std::string& main_frame_ua_observed() const {
    return main_frame_ua_observed_;
  }

  const std::string& main_frame_ua_full_version_observed() const {
    return main_frame_ua_full_version_observed_;
  }

  const std::string& main_frame_ua_full_version_list_observed() const {
    return main_frame_ua_full_version_list_observed_;
  }

  const std::string& main_frame_ua_mobile_observed() const {
    return main_frame_ua_mobile_observed_;
  }

  const std::string& main_frame_ua_platform_observed() const {
    return main_frame_ua_platform_observed_;
  }

  const std::string& main_frame_save_data_observed() const {
    return main_frame_save_data_observed_;
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  std::string intercept_iframe_resource_;
  bool intercept_to_http_equiv_iframe_ = false;
  bool intercept_to_meta_delegate_iframe_ = false;
  mutable base::Lock count_headers_lock_;

  Profile* GenerateNewProfile() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath current_profile_path = browser()->profile()->GetPath();

    // Create an additional profile.
    base::FilePath new_path =
        profile_manager->GenerateNextProfileDirectoryPath();

    return &profiles::testing::CreateProfileSync(profile_manager, new_path);
  }

 protected:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

 private:
  // Intercepts only the main frame requests that contain
  // "redirect" in the resource path. The intercepted requests
  // are served an HTML file that fetches an iframe from a cross-origin HTTPS
  // server.
  std::unique_ptr<net::test_server::HttpResponse> RequestHandlerToRedirect(
      const net::test_server::HttpRequest& request) {
    // Check if it's a main frame request.
    if (request.relative_url.find(".html") == std::string::npos)
      return nullptr;

    if (request.GetURL().spec().find("redirect") == std::string::npos)
      return nullptr;

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_FOUND);
    response->AddCustomHeader("Location", without_accept_ch_url().spec());
    return std::move(response);
  }

  // Intercepts only the main frame requests that contain
  // `intercept_iframe_resource_` in the resource path. The intercepted requests
  // are served an HTML file that fetches an iframe from a cross-origin HTTPS
  // server.
  std::unique_ptr<net::test_server::HttpResponse>
  RequestHandlerToFetchCrossOriginIframe(
      const net::test_server::HttpRequest& request) {
    if (intercept_iframe_resource_.empty())
      return nullptr;

    // Check if it's a main frame request.
    if (request.relative_url.find(".html") == std::string::npos)
      return nullptr;

    if (request.relative_url.find(intercept_iframe_resource_) ==
        std::string::npos) {
      return nullptr;
    }

    const std::string iframe_url =
        intercept_to_meta_delegate_iframe_
            ? https_cross_origin_server_.GetURL("/meta_equiv_delegate_ch.html")
                  .spec()
        : intercept_to_http_equiv_iframe_
            ? https_cross_origin_server_.GetURL("/http_equiv_accept_ch.html")
                  .spec()
            : https_cross_origin_server_.GetURL("/accept_ch.html").spec();

    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/html");
    http_response->set_content(
        "<html>"
        "<link rel='icon' href='data:;base64,='><head></head>"
        "Empty file which uses link-rel to disable favicon fetches. "
        "<iframe src='" +
        iframe_url + "'></iframe></html>");

    return std::move(http_response);
  }

  static std::string UpdateHeaderObservation(
      const net::test_server::HttpRequest& request,
      const std::string& header) {
    if (request.headers.find(header) != request.headers.end())
      return request.headers.find(header)->second;
    else
      return "";
  }

  // Called by `https_server_`.
  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    bool is_main_frame_navigation =
        (request.GetURL().spec().find(".html") != std::string::npos ||
         request.GetURL().spec().find("echoheader") != std::string::npos);

    if (is_main_frame_navigation &&
        request.GetURL().spec().find("redirect") != std::string::npos) {
      return;
    }

    if (is_main_frame_navigation) {
      main_frame_ua_observed_ = UpdateHeaderObservation(request, "sec-ch-ua");
      main_frame_ua_full_version_observed_ =
          UpdateHeaderObservation(request, "sec-ch-ua-full-version");
      main_frame_ua_full_version_list_observed_ =
          UpdateHeaderObservation(request, "sec-ch-ua-full-version-list");
      main_frame_ua_mobile_observed_ =
          UpdateHeaderObservation(request, "sec-ch-ua-mobile");
      main_frame_ua_platform_observed_ =
          UpdateHeaderObservation(request, "sec-ch-ua-platform");
      main_frame_save_data_observed_ =
          UpdateHeaderObservation(request, "save-data");

      VerifyClientHintsReceived(expect_client_hints_on_main_frame_, request);
      if (expect_client_hints_on_main_frame_) {
        double value = 0.0;

        EXPECT_TRUE(base::StringToDouble(
            request.headers.find("device-memory")->second, &value));
        EXPECT_LT(0.0, value);
        EXPECT_TRUE(IsSimilarToDoubleABNF(
            request.headers.find("device-memory")->second));
        main_frame_device_memory_observed_deprecated_ = value;

        EXPECT_TRUE(base::StringToDouble(
            request.headers.find("sec-ch-device-memory")->second, &value));
        EXPECT_LT(0.0, value);
        EXPECT_TRUE(IsSimilarToDoubleABNF(
            request.headers.find("sec-ch-device-memory")->second));
        main_frame_device_memory_observed_ = value;

        EXPECT_TRUE(
            base::StringToDouble(request.headers.find("dpr")->second, &value));
        EXPECT_LT(0.0, value);
        EXPECT_TRUE(IsSimilarToDoubleABNF(request.headers.find("dpr")->second));
        main_frame_dpr_observed_deprecated_ = value;

        EXPECT_TRUE(base::StringToDouble(
            request.headers.find("sec-ch-dpr")->second, &value));
        EXPECT_LT(0.0, value);
        EXPECT_TRUE(
            IsSimilarToDoubleABNF(request.headers.find("sec-ch-dpr")->second));
        main_frame_dpr_observed_ = value;

        EXPECT_TRUE(base::StringToDouble(
            request.headers.find("viewport-width")->second, &value));
        EXPECT_TRUE(
            IsSimilarToIntABNF(request.headers.find("viewport-width")->second));
#if !BUILDFLAG(IS_ANDROID)
        EXPECT_LT(0.0, value);
#else
        EXPECT_EQ(980, value);
#endif
        main_frame_viewport_width_observed_deprecated_ = value;

        EXPECT_TRUE(base::StringToDouble(
            request.headers.find("sec-ch-viewport-width")->second, &value));
        EXPECT_TRUE(IsSimilarToIntABNF(
            request.headers.find("sec-ch-viewport-width")->second));
#if !BUILDFLAG(IS_ANDROID)
        EXPECT_LT(0.0, value);
#else
        EXPECT_EQ(980, value);
#endif
        main_frame_viewport_width_observed_ = value;

        EXPECT_TRUE(base::StringToDouble(
            request.headers.find("sec-ch-viewport-height")->second, &value));
        EXPECT_TRUE(IsSimilarToIntABNF(
            request.headers.find("sec-ch-viewport-height")->second));
        EXPECT_LT(0.0, value);

        VerifyNetworkQualityClientHints(request);
      }
    }

    if (!is_main_frame_navigation) {
      VerifyClientHintsReceived(expect_client_hints_on_subresources(), request);

      if (expect_client_hints_on_subresources()) {
        double value = 0.0;

        EXPECT_TRUE(base::StringToDouble(
            request.headers.find("device-memory")->second, &value));
        EXPECT_LT(0.0, value);
        EXPECT_TRUE(IsSimilarToDoubleABNF(
            request.headers.find("device-memory")->second));
        if (main_frame_device_memory_observed_deprecated_ > 0) {
          EXPECT_EQ(main_frame_device_memory_observed_deprecated_, value);
        }

        EXPECT_TRUE(base::StringToDouble(
            request.headers.find("sec-ch-device-memory")->second, &value));
        EXPECT_LT(0.0, value);
        EXPECT_TRUE(IsSimilarToDoubleABNF(
            request.headers.find("sec-ch-device-memory")->second));
        if (main_frame_device_memory_observed_ > 0) {
          EXPECT_EQ(main_frame_device_memory_observed_, value);
        }

        EXPECT_TRUE(
            base::StringToDouble(request.headers.find("dpr")->second, &value));
        EXPECT_LT(0.0, value);
        EXPECT_TRUE(IsSimilarToDoubleABNF(request.headers.find("dpr")->second));
        if (main_frame_dpr_observed_deprecated_ > 0) {
          EXPECT_EQ(main_frame_dpr_observed_deprecated_, value);
        }

        EXPECT_TRUE(base::StringToDouble(
            request.headers.find("sec-ch-dpr")->second, &value));
        EXPECT_LT(0.0, value);
        EXPECT_TRUE(
            IsSimilarToDoubleABNF(request.headers.find("sec-ch-dpr")->second));
        if (main_frame_dpr_observed_ > 0) {
          EXPECT_EQ(main_frame_dpr_observed_, value);
        }

        EXPECT_TRUE(base::StringToDouble(
            request.headers.find("viewport-width")->second, &value));
        EXPECT_TRUE(
            IsSimilarToIntABNF(request.headers.find("viewport-width")->second));
#if !BUILDFLAG(IS_ANDROID)
        EXPECT_LT(0.0, value);
#else
        EXPECT_EQ(980, value);
#endif
#if BUILDFLAG(IS_ANDROID)
        // TODO(tbansal): https://crbug.com/825892: Viewport width on main
        // frame requests may be incorrect when the Chrome window is not
        // maximized.
        if (main_frame_viewport_width_observed_deprecated_ > 0) {
          EXPECT_EQ(main_frame_viewport_width_observed_deprecated_, value);
        }
#endif

        EXPECT_TRUE(base::StringToDouble(
            request.headers.find("sec-ch-viewport-width")->second, &value));
        EXPECT_TRUE(IsSimilarToIntABNF(
            request.headers.find("sec-ch-viewport-width")->second));
#if !BUILDFLAG(IS_ANDROID)
        EXPECT_LT(0.0, value);
#else
        EXPECT_EQ(980, value);
#endif
#if BUILDFLAG(IS_ANDROID)
        // TODO(tbansal): https://crbug.com/825892: Viewport width on main
        // frame requests may be incorrect when the Chrome window is not
        // maximized.
        if (main_frame_viewport_width_observed_ > 0) {
          EXPECT_EQ(main_frame_viewport_width_observed_, value);
        }
#endif

        EXPECT_TRUE(base::StringToDouble(
            request.headers.find("sec-ch-viewport-height")->second, &value));
        EXPECT_TRUE(IsSimilarToIntABNF(
            request.headers.find("sec-ch-viewport-height")->second));
        EXPECT_LT(0.0, value);

        VerifyNetworkQualityClientHints(request);
      }
    }

    for (const auto& elem : network::GetClientHintToNameMap()) {
      const auto& header = elem.second;
      if (base::Contains(request.headers, header)) {
        base::AutoLock lock(count_headers_lock_);
        // The user agent hint is special:
        if (header == "sec-ch-ua") {
          count_user_agent_hint_headers_seen_++;
        } else if (header == "sec-ch-ua-mobile") {
          count_ua_mobile_client_hints_headers_seen_++;
        } else if (header == "sec-ch-ua-platform") {
          count_ua_platform_client_hints_headers_seen_++;
        } else if (header == "save-data") {
          count_save_data_client_hints_headers_seen_++;
        } else {
          count_client_hints_headers_seen_++;
        }
      }
    }
  }

  void VerifyClientHintsReceived(bool expect_client_hints,
                                 const net::test_server::HttpRequest& request) {
    for (const auto& elem : network::GetClientHintToNameMap()) {
      const auto& header = elem.second;
      SCOPED_TRACE(testing::Message() << header);
      SCOPED_TRACE(testing::Message() << request.GetURL().spec());
      // Resource width client hint is only attached on image subresources.
      if (header == "width" || header == "sec-ch-width") {
        continue;
      }

      // `Sec-CH-UA`, `Sec-CH-UA-Mobile`, and `Sec-CH-UA-Platform` is attached
      // on all requests. `Save-Data` is included by default when on.
      if (header == "sec-ch-ua" || header == "sec-ch-ua-mobile" ||
          header == "sec-ch-ua-platform" || header == "save-data") {
        continue;
      }

      // Skip over the `Sec-CH-UA-Reduced` client hint because it is only added
      // in the presence of a valid "UserAgentReduction" Origin Trial token.
      // `Sec-CH-UA-Reduced` is tested via UaReducedOriginTrialBrowserTest
      // below.
      if (header == "sec-ch-ua-reduced") {
        continue;
      }

      // TODO(crbug.com/1286857): Skip over the `Sec-CH-UA-Full` client hint
      // because it is only added in the presence of a valid
      // "UserAgentDeprecation" Origin Trial token. Need to add `Sec-CH-UA-Full`
      // corresponding tests.
      if (header == "sec-ch-ua-full") {
        continue;
      }

      EXPECT_EQ(expect_client_hints, base::Contains(request.headers, header));
    }
  }

  void VerifyNetworkQualityClientHints(
      const net::test_server::HttpRequest& request) const {
    // Effective connection type is forced to 2G using command line in these
    // tests.
    int rtt_value = 0.0;
    EXPECT_TRUE(
        base::StringToInt(request.headers.find("rtt")->second, &rtt_value));
    EXPECT_LE(0, rtt_value);
    EXPECT_TRUE(IsSimilarToIntABNF(request.headers.find("rtt")->second));
    // Verify that RTT value is a multiple of 50 milliseconds.
    EXPECT_EQ(0, rtt_value % 50);
    EXPECT_GE(expected_ect == net::EFFECTIVE_CONNECTION_TYPE_2G ? 3000 : 500,
              rtt_value);

    double mbps_value = 0.0;
    EXPECT_TRUE(base::StringToDouble(request.headers.find("downlink")->second,
                                     &mbps_value));
    EXPECT_LE(0, mbps_value);
    EXPECT_TRUE(
        IsSimilarToDoubleABNF(request.headers.find("downlink")->second));
    // Verify that the mbps value is a multiple of 0.050 mbps.
    // Allow for small amount of noise due to double to integer conversions.
    EXPECT_NEAR(0, (static_cast<int>(mbps_value * 1000)) % 50, 1);
    EXPECT_GE(10.0, mbps_value);

    EXPECT_FALSE(request.headers.find("ect")->second.empty());

    // TODO(tbansal): https://crbug.com/819244: When network servicification is
    // enabled, the renderer processes do not receive notifications on
    // change in the network quality. Hence, the network quality client hints
    // are not set to the correct value on subresources.
    bool is_main_frame_navigation =
        request.GetURL().spec().find(".html") != std::string::npos;
    if (is_main_frame_navigation) {
      // Effective connection type is forced to 2G using command line in these
      // tests. RTT is expected to be 1800 msec but leave some gap to account
      // for added noise and randomization.
      if (expected_ect == net::EFFECTIVE_CONNECTION_TYPE_2G) {
        EXPECT_NEAR(1800, rtt_value, 360);
      } else if (expected_ect == net::EFFECTIVE_CONNECTION_TYPE_3G) {
        EXPECT_NEAR(450, rtt_value, 90);
      } else {
        NOTREACHED();
      }

      // Effective connection type is forced to 2G using command line in these
      // tests. downlink is expected to be 0.075 Mbps but leave some gap to
      // account for added noise and randomization.
      if (expected_ect == net::EFFECTIVE_CONNECTION_TYPE_2G) {
        EXPECT_NEAR(0.075, mbps_value, 0.05);
      } else if (expected_ect == net::EFFECTIVE_CONNECTION_TYPE_3G) {
        EXPECT_NEAR(0.4, mbps_value, 0.1);
      } else {
        NOTREACHED();
      }

      EXPECT_EQ(expected_ect == net::EFFECTIVE_CONNECTION_TYPE_2G ? "2g" : "3g",
                request.headers.find("ect")->second);
    }
  }

  net::EmbeddedTestServer http_server_;
  net::EmbeddedTestServer https_server_;
  net::EmbeddedTestServer https_cross_origin_server_;
  net::EmbeddedTestServer http2_server_;
  GURL accept_ch_url_;
  GURL http_equiv_accept_ch_url_;
  GURL meta_equiv_delegate_ch_url_;
  GURL without_accept_ch_url_;
  GURL without_accept_ch_local_url_;
  GURL accept_ch_with_iframe_url_;
  GURL http_equiv_accept_ch_with_iframe_url_;
  GURL meta_equiv_delegate_ch_with_iframe_url_;
  GURL accept_ch_with_subresource_url_;
  GURL http_equiv_accept_ch_with_subresource_url_;
  GURL meta_equiv_delegate_ch_with_subresource_url_;
  GURL accept_ch_with_subresource_iframe_url_;
  GURL http_equiv_accept_ch_with_subresource_iframe_url_;
  GURL meta_equiv_delegate_ch_with_subresource_iframe_url_;
  GURL without_accept_ch_img_foo_com_;
  GURL without_accept_ch_img_localhost_;
  GURL accept_ch_img_localhost_;
  GURL http_equiv_accept_ch_img_localhost_;
  GURL meta_equiv_delegate_ch_img_localhost_;
  GURL redirect_url_;
  GURL accept_ch_empty_;
  GURL http_equiv_accept_ch_injection_;
  GURL meta_equiv_delegate_ch_injection_;
  GURL http_equiv_accept_ch_delegation_foo_;
  GURL meta_equiv_delegate_ch_delegation_foo_;
  GURL http_equiv_accept_ch_delegation_bar_;
  GURL meta_equiv_delegate_ch_delegation_bar_;
  GURL http_equiv_accept_ch_delegation_merge_;
  GURL meta_equiv_delegate_ch_delegation_merge_;
  GURL http_equiv_accept_ch_merge_;
  GURL meta_equiv_delegate_ch_merge_;
  GURL without_accept_ch_cross_origin_;

  std::string main_frame_ua_observed_;
  std::string main_frame_ua_full_version_observed_;
  std::string main_frame_ua_full_version_list_observed_;
  std::string main_frame_ua_mobile_observed_;
  std::string main_frame_ua_platform_observed_;
  std::string main_frame_save_data_observed_;

  double main_frame_dpr_observed_deprecated_ = -1;
  double main_frame_dpr_observed_ = -1;
  double main_frame_viewport_width_observed_deprecated_ = -1;
  double main_frame_viewport_width_observed_ = -1;
  double main_frame_device_memory_observed_deprecated_ = -1;
  double main_frame_device_memory_observed_ = -1;

  // Expect client hints on all the main frame request.
  bool expect_client_hints_on_main_frame_{false};
  // Expect client hints on all the subresource requests.
  bool expect_client_hints_on_subresources_
      GUARDED_BY(expect_client_hints_on_subresources_lock_);

  base::Lock expect_client_hints_on_subresources_lock_;

  size_t count_user_agent_hint_headers_seen_{0};
  size_t count_ua_mobile_client_hints_headers_seen_{0};
  size_t count_ua_platform_client_hints_headers_seen_{0};
  size_t count_save_data_client_hints_headers_seen_{0};
  size_t count_client_hints_headers_seen_{0};

  std::unique_ptr<ThirdPartyURLLoaderInterceptor> request_interceptor_{nullptr};

  // Set to 2G in SetUpCommandLine().
  net::EffectiveConnectionType expected_ect = net::EFFECTIVE_CONNECTION_TYPE_2G;
};

// When a test needs to verify all three types of meta tags, they can use this
// test and read the param to verify behavior of all three.
class ClientHintsBrowserTestForMetaTagTypes
    : public ClientHintsBrowserTest,
      public testing::WithParamInterface<network::MetaCHType> {};
INSTANTIATE_TEST_SUITE_P(
    All,
    ClientHintsBrowserTestForMetaTagTypes,
    testing::Values(network::MetaCHType::HttpEquivAcceptCH,
                    network::MetaCHType::HttpEquivDelegateCH));

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, CorsChecks) {
  for (const auto& elem : network::GetClientHintToNameMap()) {
    const auto& header = elem.second;
    // Do not test for headers that have not been enabled on the blink "stable"
    // yet.
    if (header == "rtt" || header == "downlink" || header == "ect") {
      continue;
    }
    // Save-Data can only have the 'on' value so it's tested below.
    if (header == "save-data")
      continue;
    EXPECT_TRUE(
        network::cors::IsCorsSafelistedHeader(header, "42" /* value */));
  }
  EXPECT_FALSE(network::cors::IsCorsSafelistedHeader("not-a-client-hint-header",
                                                     "" /* value */));
  EXPECT_TRUE(
      network::cors::IsCorsSafelistedHeader("save-data", "on" /* value */));
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, HttpEquivWorks) {
  const GURL gurl = http_equiv_accept_ch_img_localhost();
  base::HistogramTester histogram_tester;

  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(true);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);
}
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, MetaDelegateWorks) {
  const GURL gurl = meta_equiv_delegate_ch_img_localhost();
  base::HistogramTester histogram_tester;

  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(true);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);
}

// Loads a webpage that requests persisting of client hints. Verifies that
// the browser receives the mojo notification from the renderer and persists the
// client hints to the disk --- unless it's using http-equiv which shouldn't
// persist.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, ClientHintsHttps) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), accept_ch_url()));

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize",
                                      expected_client_hints_number, 1);
  ExpectAcceptCHHeaderUKMSeen(*ukm_recorder_, kStandardHTTPHeaderHints,
                              /*loads=*/1);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);
}
IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTestForMetaTagTypes,
                       ClientHintsHttps) {
  base::HistogramTester histogram_tester;
  GURL gurl;
  switch (GetParam()) {
    case network::MetaCHType::HttpEquivAcceptCH:
      gurl = http_equiv_accept_ch_url();
      break;
    case network::MetaCHType::HttpEquivDelegateCH:
      gurl = meta_equiv_delegate_ch_url();
      break;
  }
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));

  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectTotalCount("ClientHints.UpdateSize", 0);
  switch (GetParam()) {
    case network::MetaCHType::HttpEquivAcceptCH:
      ExpectAcceptCHMetaUKMSeen(*ukm_recorder_, kStandardAcceptCHMetaHints,
                                /*loads=*/1);
      ExpectDelegateCHMetaUKMSeen(*ukm_recorder_, {},
                                  /*loads=*/1);
      break;
    case network::MetaCHType::HttpEquivDelegateCH:
      ExpectAcceptCHMetaUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);
      ExpectDelegateCHMetaUKMSeen(*ukm_recorder_, kStandardDelegateCHMetaHints,
                                  /*loads=*/1);
      break;
  }
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, ClientHintsAlps) {
  base::HistogramTester histogram_tester;
  SetClientHintExpectationsOnMainFrame(true);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetHttp2Url("/blank.html")));
  histogram_tester.ExpectBucketCount(
      "ClientHints.AcceptCHFrame",
      content::AcceptCHFrameRestart::kNavigationRestarted, 1);
}

// Ensure that Critical-CH doesn't restart if headers added via ALPS are already
// present.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       NoCriticalRestartIfHeadersPresentViaAlps) {
  base::HistogramTester histogram_tester;
  SetClientHintExpectationsOnMainFrame(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GetHttp2Url("/client_hints/critical_ch_ua_full_version_list.html")));
  histogram_tester.ExpectBucketCount(
      "ClientHints.AcceptCHFrame",
      content::AcceptCHFrameRestart::kNavigationRestarted, 1);
  histogram_tester.ExpectBucketCount("ClientHints.CriticalCHRestart",
                                     2 /*=kNavigationRestarted*/, 0);
  ExpectAcceptCHHeaderUKMSeen(
      *ukm_recorder_,
      {network::mojom::WebClientHintsType::kUAFullVersionList,
       network::mojom::WebClientHintsType::kDpr},
      /*loads=*/1);
  ExpectCriticalCHHeaderUKMSeen(
      *ukm_recorder_, {network::mojom::WebClientHintsType::kUAFullVersionList},
      /*loads=*/1);
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, ClientHintsAlpsRestartLimit) {
  net::test_server::EmbeddedTestServer server_1(
      net::EmbeddedTestServer::TYPE_HTTPS,
      net::test_server::HttpConnection::Protocol::kHttp2);
  net::test_server::EmbeddedTestServer server_2(
      net::EmbeddedTestServer::TYPE_HTTPS,
      net::test_server::HttpConnection::Protocol::kHttp2);

  server_1.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto http_response =
            std::make_unique<net::test_server::BasicHttpResponse>();
        http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        http_response->AddCustomHeader("Location", server_2.GetURL("/").spec());
        return http_response;
      }));
  server_2.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto http_response =
            std::make_unique<net::test_server::BasicHttpResponse>();
        http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        http_response->AddCustomHeader("Location", server_1.GetURL("/").spec());
        return http_response;
      }));

  server_1.SetAlpsAcceptCH("", "sec-ch-ua-arch");
  server_2.SetAlpsAcceptCH("", "sec-ch-ua-arch");

  ASSERT_TRUE(server_1.Start());
  ASSERT_TRUE(server_2.Start());

  base::HistogramTester histogram_tester;
  content::TestNavigationObserver nav_observer(
      browser()->tab_strip_model()->GetActiveWebContents(), 1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), server_1.GetURL("/")));
  histogram_tester.ExpectBucketCount(
      "ClientHints.AcceptCHFrame",
      content::AcceptCHFrameRestart::kNavigationRestarted,
      net::URLRequest::kMaxRedirects);
  histogram_tester.ExpectBucketCount(
      "ClientHints.AcceptCHFrame",
      content::AcceptCHFrameRestart::kRedirectOverflow, 1);
  EXPECT_EQ(net::ERR_TOO_MANY_ACCEPT_CH_RESTARTS,
            nav_observer.last_net_error_code());
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsAlpsNavigationPreload) {
  SetClientHintExpectationsOnMainFrame(true);
  const GURL kCreateServiceWorker =
      GetHttp2Url("/service_worker/create_service_worker.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kCreateServiceWorker));
  EXPECT_EQ(
      "DONE",
      EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
             "register('/service_worker/navigation_preload_worker.js', '/');"));

  const GURL kEchoHeader =
      GetHttp2Url("/echoheader?Service-Worker-Navigation-Preload");
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kEchoHeader));
  histogram_tester.ExpectBucketCount(
      "ClientHints.AcceptCHFrame",
      content::AcceptCHFrameRestart::kNavigationRestarted, 1);
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, PRE_ClientHintsClearSession) {
  const GURL gurl = accept_ch_url();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Fetching `gurl` should persist the request for client hints iff using
  // headers and not http-equiv.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize",
                                      expected_client_hints_number, 1);

  // Clients hints preferences for one origin should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());

  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), without_accept_ch_url()));

  // The user agent hint is attached to all three requests:
  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(3u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, ClientHintsClearSession) {
  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);

  EXPECT_EQ(
      base::FeatureList::IsEnabled(blink::features::kDurableClientHintsCache)
          ? 1u
          : 0u,
      host_settings.size());

  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), without_accept_ch_url()));

  // The user agent hint is attached to all three requests:
  EXPECT_EQ(2u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(2u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(2u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(0u, count_client_hints_headers_seen());
  ExpectAcceptCHHeaderUKMSeen(*ukm_recorder_, {},
                              /*loads=*/1);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);
}

// Test that client hints are attached to subresources only if they belong
// to the same host as document host.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsHttpsSubresourceDifferentOrigin) {
  const GURL gurl = accept_ch_url();

  base::HistogramTester histogram_tester;

  // Add client hints for the embedded test server.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);

  // Verify that the client hints settings for localhost have been saved.
  ContentSettingsForOneType client_hints_settings;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, &client_hints_settings);
  ASSERT_EQ(1U, client_hints_settings.size());

  // Copy the client hints setting for localhost to foo.com.
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      GURL("https://foo.com/"), GURL(), ContentSettingsType::CLIENT_HINTS,
      client_hints_settings.at(0).setting_value.Clone());

  // Verify that client hints for the two hosts has been saved.
  host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, &client_hints_settings);
  ASSERT_EQ(2U, client_hints_settings.size());

  // Navigating to without_accept_ch_img_localhost() should
  // attach client hints to the image subresouce contained in that page since
  // the image is located on the same server as the document origin.
  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           without_accept_ch_img_localhost()));
  base::RunLoop().RunUntilIdle();
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // The user agent hint is attached to all three requests, as is UA-mobile:
  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(3u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());

  // Navigating to without_accept_ch_img_foo_com() should not
  // attach client hints to the image subresouce contained in that page since
  // the image is located on a different server as the document origin.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), without_accept_ch_img_foo_com()));
  base::RunLoop().RunUntilIdle();
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // The device-memory and dprheader is attached to the main frame request.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(expected_client_hints_number, count_client_hints_headers_seen());
#else
  EXPECT_EQ(expected_client_hints_number * 3,
            count_client_hints_headers_seen());
#endif

  // Requests to third party servers should have three (3) client hints attached
  // (`Sec-CH-UA`, `Sec-CH-UA-Mobile`, `Sec-CH-UA-Platform`).
  EXPECT_EQ(1u, third_party_request_count_seen());
  EXPECT_EQ(3u, third_party_client_hints_count_seen());
}

// Test that client hints are attached to subresources checks the right setting
// for OTR profile.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsHttpsSubresourceOffTheRecord) {
  const GURL gurl = accept_ch_url();

  base::HistogramTester histogram_tester;

  // Add client hints for the embedded test server.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);

  // Main profile should get hints for both page and subresources.
  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           without_accept_ch_img_localhost()));
  base::RunLoop().RunUntilIdle();
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // The user agent hint is attached to all three requests:
  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(3u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());

  // OTR profile should get neither.
  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());
  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(otr_browser,
                                           without_accept_ch_img_localhost()));
}

// Verify that we send only major version information in the `Sec-CH-UA` header
// by default, regardless of opt-in.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, UserAgentVersion) {
  const GURL gurl = accept_ch_url();

  blink::UserAgentMetadata ua = embedder_support::GetUserAgentMetadata();

  // Navigate to a page that opts-into the header: the value should end with
  // the major version, and not contain the full version.
  SetClientHintExpectationsOnMainFrame(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  std::string expected_ua = ua.SerializeBrandMajorVersionList();
  EXPECT_EQ(main_frame_ua_observed(), expected_ua);
  EXPECT_TRUE(main_frame_ua_full_version_observed().empty());

  // Navigate again, after the opt-in: the value should stay the major
  // version.
  SetClientHintExpectationsOnMainFrame(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  std::string expected_full_version = "\"" + ua.full_version + "\"";
  EXPECT_EQ(main_frame_ua_observed(), expected_ua);
  EXPECT_EQ(main_frame_ua_full_version_observed(), expected_full_version);

  std::string expected_full_version_list = ua.SerializeBrandFullVersionList();
  EXPECT_EQ(main_frame_ua_observed(), expected_ua);
  EXPECT_EQ(main_frame_ua_full_version_list_observed(),
            expected_full_version_list);
  // Two navigations occurred.
  ExpectAcceptCHHeaderUKMSeen(*ukm_recorder_, kStandardHTTPHeaderHints,
                              /*loads=*/2);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_, {},
                                /*loads=*/2);
}

// Verify that client hints send when we restart the browser.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, RestartBrowser) {
  const GURL gurl = accept_ch_url();

  // First request: no high-entropy hints send in the request header because we
  // don't know server preferences.
  SetClientHintExpectationsOnMainFrame(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  EXPECT_TRUE(main_frame_ua_full_version_observed().empty());

  // Send request: we should expect the high-entropy client hints send in the
  // request header.
  SetClientHintExpectationsOnMainFrame(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  blink::UserAgentMetadata ua = embedder_support::GetUserAgentMetadata();
  std::string expected_full_version_list = ua.SerializeBrandFullVersionList();
  EXPECT_EQ(main_frame_ua_full_version_list_observed(),
            expected_full_version_list);

  // Restart the browser, create a new browser to mock the restart process.
  Browser* new_browser = CreateBrowser(browser()->profile());
  CloseBrowserSynchronously(browser());
  SelectFirstBrowser();
  ASSERT_EQ(browser(), new_browser);

  // First request with new browser should expect the high-entropy client hints
  // send in the request header.
  SetClientHintExpectationsOnMainFrame(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  EXPECT_EQ(main_frame_ua_full_version_list_observed(),
            expected_full_version_list);
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, UAHintsTabletMode) {
  const GURL gurl = accept_ch_url();

  blink::UserAgentMetadata ua = embedder_support::GetUserAgentMetadata();

  // First request: only minimal hints, no tablet override.
  SetClientHintExpectationsOnMainFrame(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  std::string expected_ua = ua.SerializeBrandMajorVersionList();
  EXPECT_EQ(main_frame_ua_observed(), expected_ua);
  EXPECT_EQ(main_frame_ua_full_version_observed(), "");
  EXPECT_EQ(main_frame_ua_mobile_observed(), "?0");
  EXPECT_EQ(main_frame_ua_platform_observed(), "\"" + ua.platform + "\"");
  EXPECT_EQ(main_frame_save_data_observed(), "");

  // Second request: table override, all hints.
  chrome::ToggleRequestTabletSite(browser());
  SetClientHintExpectationsOnMainFrame(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  EXPECT_EQ(main_frame_ua_observed(), expected_ua);
  std::string expected_full_version = "\"" + ua.full_version + "\"";
  EXPECT_EQ(main_frame_ua_full_version_observed(), expected_full_version);
  std::string expected_full_version_list = ua.SerializeBrandFullVersionList();
  EXPECT_EQ(main_frame_ua_full_version_list_observed(),
            expected_full_version_list);
  EXPECT_EQ(main_frame_ua_mobile_observed(), "?1");
  EXPECT_EQ(main_frame_ua_platform_observed(), "\"Android\"");
  EXPECT_EQ(main_frame_save_data_observed(), "");
}

// TODO(morlovich): Move this into WebContentsImplBrowserTest once things are
// refactored enough that UA client hints actually work in content/
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, UserAgentOverrideClientHints) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(embedded_test_server()->Start());
  const std::string kHeaderPath = std::string("/echoheader?") +
                                  net::HttpRequestHeaders::kUserAgent +
                                  "&sec-ch-ua&sec-ch-ua-mobile";
  const GURL kUrl(embedded_test_server()->GetURL(kHeaderPath));

  web_contents->SetUserAgentOverride(
      blink::UserAgentOverride::UserAgentOnly("foo"), false);
  // Not enabled first.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));
  std::string header_value =
      EvalJs(web_contents, "document.body.textContent;").ExtractString();
  EXPECT_EQ(std::string::npos, header_value.find("foo")) << header_value;

  // Actually turn it on.
  web_contents->GetController()
      .GetLastCommittedEntry()
      ->SetIsOverridingUserAgent(true);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));
  header_value =
      EvalJs(web_contents, "document.body.textContent;").ExtractString();
  EXPECT_EQ("foo\nNone\nNone", header_value);
  header_value =
      EvalJs(web_contents, "JSON.stringify(navigator.userAgentData);")
          .ExtractString();
  EXPECT_EQ(R"({"brands":[],"mobile":false,"platform":""})", header_value);

  // Now actually provide values for the hints.
  blink::UserAgentOverride ua_override;
  ua_override.ua_string_override = "foobar";
  ua_override.ua_metadata_override.emplace();
  ua_override.ua_metadata_override->mobile = true;
  ua_override.ua_metadata_override->brand_version_list.emplace_back(
      "Foobarnator", "3.14");
  web_contents->SetUserAgentOverride(ua_override, false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));
  header_value =
      EvalJs(web_contents, "document.body.textContent;").ExtractString();
  EXPECT_EQ("foobar\n\"Foobarnator\";v=\"3.14\"\n?1", header_value);
  header_value =
      EvalJs(web_contents, "JSON.stringify(navigator.userAgentData);")
          .ExtractString();
  const std::string kExpected =
      "{\"brands\":[{\"brand\":\"Foobarnator\",\"version\":\"3.14\"}],"
      "\"mobile\":true,\"platform\":\"\"}";
  EXPECT_EQ(kExpected, header_value);
}

class ClientHintsUAOverrideBrowserTest : public ClientHintsBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kUACHOverrideBlank);
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ClientHintsUAOverrideBrowserTest,
                       UserAgentOverrideClientHints) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(embedded_test_server()->Start());
  const std::string kHeaderPath = std::string("/echoheader?") +
                                  net::HttpRequestHeaders::kUserAgent +
                                  "&sec-ch-ua&sec-ch-ua-mobile";
  const GURL kUrl(embedded_test_server()->GetURL(kHeaderPath));

  web_contents->SetUserAgentOverride(
      blink::UserAgentOverride::UserAgentOnly("foo"), false);
  web_contents->GetController()
      .GetLastCommittedEntry()
      ->SetIsOverridingUserAgent(true);

  // Since no value was provided for client hints, they are sent with blank or
  // false values.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));

  std::string header_value =
      EvalJs(web_contents, "document.body.textContent;").ExtractString();
  EXPECT_EQ("foo\n\n?0", header_value);
  header_value =
      EvalJs(web_contents, "JSON.stringify(navigator.userAgentData);")
          .ExtractString();
  EXPECT_EQ(R"({"brands":[],"mobile":false,"platform":""})", header_value);
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, EmptyAcceptCH) {
  // First navigate to a page that enables hints. No CH for it yet, since
  // nothing opted in.
  GURL gurl = accept_ch_url();
  SetClientHintExpectationsOnMainFrame(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));

  // Now go to a page with blank Accept-CH. Should get hints from previous
  // visit.
  gurl = accept_ch_empty();
  SetClientHintExpectationsOnMainFrame(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));

  // Visiting again should not expect them since we opted out again.
  SetClientHintExpectationsOnMainFrame(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  ExpectAcceptCHHeaderUKMSeen(*ukm_recorder_, kStandardHTTPHeaderHints,
                              /*loads=*/1);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, InjectAcceptCH_HttpEquiv) {
  // Go to page where hints are injected via javascript into an http-equiv meta
  // tag. It shouldn't get hints itself (due to first visit),
  // but subresources should get all the client hints.
  GURL gurl = http_equiv_accept_ch_injection();
  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  EXPECT_EQ(expected_client_hints_number, count_client_hints_headers_seen());
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, InjectAcceptCH_MetaDelegate) {
  // Go to page where hints are injected via javascript into an named meta
  // tag. It shouldn't get hints itself (due to first visit),
  // but subresources should get all the client hints.
  GURL gurl = meta_equiv_delegate_ch_injection();
  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  EXPECT_EQ(0u, count_client_hints_headers_seen());
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, DelegateToFoo_HttpEquiv) {
  // Go to a page which delegates hints to `foo.com`.
  GURL gurl = http_equiv_accept_ch_delegation_foo();
  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  EXPECT_EQ(0u, count_client_hints_headers_seen());
  // Four unique requests are request to the following URLs:
  // "https://foo.com/non-existing-image.jpg",
  // "https://foo.com/non-existing-iframe.html",
  // "https://bar.com/non-existing-image.jpg",
  // "https://bar.com/non-existing-iframe.html"
  EXPECT_EQ(4u, third_party_unique_request_count_seen());
  EXPECT_EQ(expected_default_third_party_client_hints_number * 4,
            third_party_client_hints_count_seen_on_unique_request());
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, DelegateToFoo_MetaDelegate) {
  // Go to a page which delegates hints to `foo.com`.
  GURL gurl = meta_equiv_delegate_ch_delegation_foo();
  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());
  EXPECT_EQ(4u, third_party_unique_request_count_seen());
  EXPECT_EQ(expected_requested_third_party_client_hints_number * 2 +
                expected_default_third_party_client_hints_number * 2,
            third_party_client_hints_count_seen_on_unique_request());
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, DelegateToBar_HttpEquiv) {
  // Go to a page which delegates hints to `bar.com`.
  GURL gurl = http_equiv_accept_ch_delegation_bar();
  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  EXPECT_EQ(0u, count_client_hints_headers_seen());
  EXPECT_EQ(4u, third_party_unique_request_count_seen());
  EXPECT_EQ(expected_default_third_party_client_hints_number * 4,
            third_party_client_hints_count_seen_on_unique_request());
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, DelegateToBar_MetaDelegate) {
  // Go to a page which delegates hints to `bar.com`.
  GURL gurl = meta_equiv_delegate_ch_delegation_bar();
  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());
  EXPECT_EQ(4u, third_party_unique_request_count_seen());
  EXPECT_EQ(expected_requested_third_party_client_hints_number * 2 +
                expected_default_third_party_client_hints_number * 2,
            third_party_client_hints_count_seen_on_unique_request());
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, DelegateAndMerge_HttpEquiv) {
  // Go to a page which delegates hints in HTTP and HTML.
  GURL gurl = http_equiv_accept_ch_delegation_merge();
  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());
  EXPECT_EQ(4u, third_party_unique_request_count_seen());
  EXPECT_EQ(expected_pre_merge_third_party_client_hints_number * 2 +
                expected_requested_third_party_client_hints_number * 2,
            third_party_client_hints_count_seen_on_unique_request());
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, DelegateAndMerge_MetaDelegate) {
  // Go to a page which delegates hints in HTTP and HTML.
  GURL gurl = meta_equiv_delegate_ch_delegation_merge();
  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());
  EXPECT_EQ(4u, third_party_unique_request_count_seen());
  EXPECT_EQ(expected_requested_third_party_client_hints_number * 4,
            third_party_client_hints_count_seen_on_unique_request());
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, MergeAcceptCH_HttpEquiv) {
  // Go to page where some hints are enabled by headers, some by
  // http-equiv. It shouldn't get hints itself (due to first visit),
  // but subresources should get all the client hints.
  GURL gurl = http_equiv_accept_ch_merge();
  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  EXPECT_EQ(expected_client_hints_number, count_client_hints_headers_seen());
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, MergeAcceptCH_MetaDelegate) {
  // Go to page where some hints are enabled by headers, some by
  // http-equiv. It shouldn't get hints itself (due to first visit),
  // but subresources should get all the client hints.
  GURL gurl = meta_equiv_delegate_ch_merge();
  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  EXPECT_EQ(expected_client_hints_number, count_client_hints_headers_seen());
}

void ClientHintsBrowserTest::TestProfilesIndependent(Browser* browser_a,
                                                     Browser* browser_b) {
  const GURL gurl = accept_ch_url();

  blink::UserAgentMetadata ua = embedder_support::GetUserAgentMetadata();

  // Navigate `browser_a` to a page that opts-into the header: the value should
  // end with the major version, and not contain the full version.
  SetClientHintExpectationsOnMainFrame(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser_a, gurl));
  std::string expected_ua = ua.SerializeBrandMajorVersionList();
  EXPECT_EQ(main_frame_ua_observed(), expected_ua);
  EXPECT_TRUE(main_frame_ua_full_version_observed().empty());

  // Try again on `browser_a`, the header should have an effect there.
  SetClientHintExpectationsOnMainFrame(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser_a, gurl));
  std::string expected_full_version = "\"" + ua.full_version + "\"";
  EXPECT_EQ(main_frame_ua_observed(), expected_ua);
  EXPECT_EQ(main_frame_ua_full_version_observed(), expected_full_version);
  // verify full version list
  std::string expected_full_version_list = ua.SerializeBrandFullVersionList();
  EXPECT_EQ(main_frame_ua_observed(), expected_ua);
  EXPECT_EQ(main_frame_ua_full_version_list_observed(),
            expected_full_version_list);

  // Navigate on `browser_b`. That should still only have the major
  // version.
  SetClientHintExpectationsOnMainFrame(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser_b, gurl));
  EXPECT_EQ(main_frame_ua_observed(), expected_ua);
  EXPECT_TRUE(main_frame_ua_full_version_observed().empty());
}

// Check that client hints attached to navigation inside OTR profiles
// use the right settings, regular -> OTR direction.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, OffTheRecordIndependent) {
  TestProfilesIndependent(browser(),
                          CreateIncognitoBrowser(browser()->profile()));
}

// Check that client hints attached to navigation inside OTR profiles
// use the right settings, OTR -> regular direction.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, OffTheRecordIndependent2) {
  TestProfilesIndependent(CreateIncognitoBrowser(browser()->profile()),
                          browser());
}

// Only default client hints should be delegated to third party subresources.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, Default) {
  GURL gurl = accept_ch_img_localhost();
  unsigned update_event_count = 1;
  gurl = accept_ch_img_localhost();

  base::HistogramTester histogram_tester;

  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(true);

  // Add client hints for the embedded test server.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount",
                                    update_event_count);

  // One fetch when initially add the client hints head, one fetch for look up
  // commit client hints when navigation commits.
  histogram_tester.ExpectTotalCount("ClientHints.FetchLatency_Total", 2);
  histogram_tester.ExpectTotalCount("ClientHints.FetchLatency_PrefRead", 2);
  histogram_tester.ExpectTotalCount("ClientHints.FetchLatency_PrerenderHost",
                                    2);
  histogram_tester.ExpectTotalCount("ClientHints.FetchLatency_OriginTrialCheck",
                                    2);

  EXPECT_EQ(2u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(2u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(2u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());
  EXPECT_EQ(expected_client_hints_number, count_client_hints_headers_seen());

  // Requests to third party servers should not have client hints attached.
  EXPECT_EQ(1u, third_party_request_count_seen());

  // Client hints should not be sent to the third-party with the exception of
  // the `Sec-CH-UA/-Platform/-Mobile))` hints sent every request.
  EXPECT_EQ(3u, third_party_client_hints_count_seen());
  ExpectAcceptCHHeaderUKMSeen(*ukm_recorder_, kStandardHTTPHeaderHints,
                              /*loads=*/1);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);
}
IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTestForMetaTagTypes, Default) {
  GURL gurl;
  unsigned update_event_count = 0;
  switch (GetParam()) {
    case network::MetaCHType::HttpEquivAcceptCH:
      gurl = http_equiv_accept_ch_img_localhost();
      break;
    case network::MetaCHType::HttpEquivDelegateCH:
      gurl = meta_equiv_delegate_ch_img_localhost();
      break;
  }

  base::HistogramTester histogram_tester;

  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(true);

  // Add client hints for the embedded test server.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount",
                                    update_event_count);

  EXPECT_EQ(2u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(2u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(2u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());
  EXPECT_EQ(expected_client_hints_number, count_client_hints_headers_seen());

  // Requests to third party servers should not have client hints attached.
  EXPECT_EQ(1u, third_party_request_count_seen());

  // Client hints should not be sent to the third-party with the exception of
  // the `Sec-CH-UA/-Platform/-Mobile))` hints sent every request.
  EXPECT_EQ(3u, third_party_client_hints_count_seen());
  switch (GetParam()) {
    case network::MetaCHType::HttpEquivAcceptCH:
      ExpectAcceptCHMetaUKMSeen(*ukm_recorder_, kExtendedAcceptCHMetaHints,
                                /*loads=*/1);
      ExpectDelegateCHMetaUKMSeen(*ukm_recorder_, {},
                                  /*loads=*/1);
      break;
    case network::MetaCHType::HttpEquivDelegateCH:
      ExpectAcceptCHMetaUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);
      ExpectDelegateCHMetaUKMSeen(*ukm_recorder_, kExtendedDelegateCHMetaHints,
                                  /*loads=*/1);
      break;
  }
}

// Loads a HTTPS webpage that does not request persisting of client hints.
// A same-origin iframe loaded by the webpage requests persistence of client
// hints. Since that's not a main frame, persistence should not happen.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       PersistenceRequestIframe_SameOrigin) {
  const GURL gurl = accept_ch_with_iframe_url();
  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));

  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // accept_ch_with_iframe_url() loads
  // accept_ch() in an iframe. The request to persist client
  // hints from accept_ch() should not be persisted.
  histogram_tester.ExpectTotalCount("ClientHints.UpdateSize", 0);
  ExpectAcceptCHHeaderUKMSeen(*ukm_recorder_, {},
                              /*loads=*/1);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);
}

// Loads a HTTPS webpage that does not request persisting of client hints.
// An iframe loaded by the webpage from an cross origin server requests
// persistence of client hints.
// Verify that the request from the cross origin iframe is not honored, and
// client hints preference is not persisted.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       DisregardPersistenceRequestIframe_CrossOrigin) {
  const GURL gurl = accept_ch_with_iframe_url();

  intercept_iframe_resource_ = gurl.path();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));

  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // accept_ch_with_iframe_url() loads
  // accept_ch() in a cross origin iframe. The request to
  // persist client hints from accept_ch() should be
  // disregarded.
  histogram_tester.ExpectTotalCount("ClientHints.UpdateSize", 0);
  ExpectAcceptCHHeaderUKMSeen(*ukm_recorder_, {},
                              /*loads=*/1);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);
}
IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTestForMetaTagTypes,
                       DisregardPersistenceRequestIframe) {
  GURL gurl;
  switch (GetParam()) {
    case network::MetaCHType::HttpEquivAcceptCH:
      gurl = http_equiv_accept_ch_with_iframe_url();
      intercept_to_http_equiv_iframe_ = true;
      break;
    case network::MetaCHType::HttpEquivDelegateCH:
      gurl = meta_equiv_delegate_ch_with_iframe_url();
      intercept_to_meta_delegate_iframe_ = true;
      break;
  }
  intercept_iframe_resource_ = gurl.path();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));

  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // accept_ch_with_iframe_url() loads
  // accept_ch() in a cross origin iframe. The request to
  // persist client hints from accept_ch() should be
  // disregarded.
  histogram_tester.ExpectTotalCount("ClientHints.UpdateSize", 0);
  ExpectAcceptCHMetaUKMSeen(*ukm_recorder_, {},
                            /*loads=*/1);
  ExpectDelegateCHMetaUKMSeen(*ukm_recorder_, {},
                              /*loads=*/1);
}

// Loads a HTTPS webpage that does not request persisting of client hints.
// A subresource loaded by the webpage requests persistence of client hints.
// Verify that the request from the subresource is not honored, and client hints
// preference is not persisted.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       DisregardPersistenceRequestSubresource) {
  const GURL gurl = accept_ch_with_subresource_url();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));

  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // accept_ch_with_subresource_url() loads
  // accept_ch() as a subresource. The request to persist
  // client hints from accept_ch() should be disregarded.
  histogram_tester.ExpectTotalCount("ClientHints.UpdateSize", 0);
  ExpectAcceptCHHeaderUKMSeen(*ukm_recorder_, {},
                              /*loads=*/1);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);
}
IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTestForMetaTagTypes,
                       DisregardPersistenceRequestSubresource) {
  GURL gurl;
  switch (GetParam()) {
    case network::MetaCHType::HttpEquivAcceptCH:
      gurl = http_equiv_accept_ch_with_subresource_url();
      break;
    case network::MetaCHType::HttpEquivDelegateCH:
      gurl = meta_equiv_delegate_ch_with_subresource_url();
      break;
  }

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));

  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // accept_ch_with_subresource_url() loads
  // accept_ch() as a subresource. The request to persist
  // client hints from accept_ch() should be disregarded.
  histogram_tester.ExpectTotalCount("ClientHints.UpdateSize", 0);
  ExpectAcceptCHMetaUKMSeen(*ukm_recorder_, {},
                            /*loads=*/1);
  ExpectDelegateCHMetaUKMSeen(*ukm_recorder_, {},
                              /*loads=*/1);
}

// Loads a HTTPS webpage that does not request persisting of client hints.
// A subresource loaded by the webpage in an iframe requests persistence of
// client hints. Verify that the request from the subresource in the iframe
// is not honored, and client hints preference is not persisted.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       DisregardPersistenceRequestSubresourceIframe) {
  const GURL gurl = accept_ch_with_subresource_iframe_url();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));

  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // `gurl` loads accept_ch() or
  // http_equiv_accept_ch_url() as a subresource in an iframe.
  // The request to persist client hints from accept_ch() or
  // http_equiv_accept_ch_url() should be disregarded.
  histogram_tester.ExpectTotalCount("ClientHints.UpdateSize", 0);
  ExpectAcceptCHHeaderUKMSeen(*ukm_recorder_, {},
                              /*loads=*/1);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);
}
IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTestForMetaTagTypes,
                       DisregardPersistenceRequestSubresourceIframe) {
  GURL gurl;
  switch (GetParam()) {
    case network::MetaCHType::HttpEquivAcceptCH:
      gurl = http_equiv_accept_ch_with_subresource_iframe_url();
      break;
    case network::MetaCHType::HttpEquivDelegateCH:
      gurl = meta_equiv_delegate_ch_with_subresource_iframe_url();
      break;
  }

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));

  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // `gurl` loads accept_ch() or meta_equiv_delegate_ch_url() as a subresource
  // in an iframe. The request to persist client hints  should be disregarded.
  histogram_tester.ExpectTotalCount("ClientHints.UpdateSize", 0);
  ExpectAcceptCHMetaUKMSeen(*ukm_recorder_, {},
                            /*loads=*/1);
  ExpectDelegateCHMetaUKMSeen(*ukm_recorder_, {},
                              /*loads=*/1);
}

// Loads a webpage that does not request persisting of client hints.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, NoClientHintsHttps) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), without_accept_ch_url()));

  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // no_client_hints_url() does not sets the client hints.
  histogram_tester.ExpectTotalCount("ClientHints.UpdateSize", 0);
  ExpectAcceptCHHeaderUKMSeen(*ukm_recorder_, {},
                              /*loads=*/1);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsFollowedByNoClientHint) {
  const GURL gurl = accept_ch_url();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Fetching `gurl` should persist the request for client hints iff using
  // headers and not http-equiv.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize",
                                      expected_client_hints_number, 1);

  // Clients hints preferences for one origin should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());

  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), without_accept_ch_url()));

  // The user agent hint is attached to all three requests:
  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(3u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());
  ExpectAcceptCHHeaderUKMSeen(*ukm_recorder_, kStandardHTTPHeaderHints,
                              /*loads=*/1);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);
}
IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTestForMetaTagTypes,
                       ClientHintsFollowedByNoClientHint) {
  GURL gurl;
  switch (GetParam()) {
    case network::MetaCHType::HttpEquivAcceptCH:
      gurl = http_equiv_accept_ch_url();
      break;
    case network::MetaCHType::HttpEquivDelegateCH:
      gurl = meta_equiv_delegate_ch_url();
      break;
  }

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Fetching `gurl` should persist the request for client hints iff using
  // headers and not http-equiv.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));

  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 0);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount("ClientHints.UpdateSize", 0);

  // Clients hints preferences for one origin should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), without_accept_ch_url()));

  // The user agent hint is attached to all three requests:
  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(3u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(0u, count_client_hints_headers_seen());
  switch (GetParam()) {
    case network::MetaCHType::HttpEquivAcceptCH:
      ExpectAcceptCHMetaUKMSeen(*ukm_recorder_, kStandardAcceptCHMetaHints,
                                /*loads=*/1);
      ExpectDelegateCHMetaUKMSeen(*ukm_recorder_, {},
                                  /*loads=*/1);
      break;
    case network::MetaCHType::HttpEquivDelegateCH:
      ExpectAcceptCHMetaUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);
      ExpectDelegateCHMetaUKMSeen(*ukm_recorder_, kStandardDelegateCHMetaHints,
                                  /*loads=*/1);
      break;
  }
}

// The test first fetches a page that sets Accept-CH. Next, it fetches a URL
// from a different origin. However, that URL response redirects to the same
// origin from where the first page was fetched. The test verifies that on
// receiving redirect to an origin for which the browser has persisted client
// hints prefs, the browser attaches the client hints headers when fetching the
// redirected URL.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsFollowedByRedirectToNoClientHint) {
  const GURL gurl = accept_ch_url();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Fetching `gurl` should persist the request for client hints.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize",
                                      expected_client_hints_number, 1);
  base::RunLoop().RunUntilIdle();

  // Clients hints preferences for one origin should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());

  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), redirect_url()));

  // The user agent hint is attached to all three requests:
  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(3u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());
  ExpectAcceptCHHeaderUKMSeen(*ukm_recorder_, kStandardHTTPHeaderHints,
                              /*loads=*/1);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);
}

// Ensure that even when cookies are blocked, client hint preferences are
// persisted.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsPersistedCookiesBlocked) {
  const GURL gurl_with = accept_ch_url();

  scoped_refptr<content_settings::CookieSettings> cookie_settings_ =
      CookieSettingsFactory::GetForProfile(browser()->profile());
  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  // Block cookies.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(gurl_with, GURL(),
                                      ContentSettingsType::COOKIES,
                                      CONTENT_SETTING_BLOCK);

  // Fetching `gurl_with` should persist the request for client hints.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl_with));
  histogram_tester.ExpectTotalCount("ClientHints.UpdateEventCount", 1);
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  VerifyContentSettingsNotNotified();
  ExpectAcceptCHHeaderUKMSeen(*ukm_recorder_, kStandardHTTPHeaderHints,
                              /*loads=*/1);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsAttachedCookiesBlocked) {
  const GURL gurl_with = accept_ch_url();
  const GURL gurl_without = accept_ch_url();
  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Fetching `gurl_with` should persist the request for client hints.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl_with));
  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize",
                                      expected_client_hints_number, 1);

  // Clients hints preferences for one origin should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());

  // Block the cookies: Client hints should be attached.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(gurl_without, GURL(),
                                      ContentSettingsType::COOKIES,
                                      CONTENT_SETTING_BLOCK);

  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), without_accept_ch_url()));

  // The user agent hint is attached to all three requests:
  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(3u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());
  ExpectAcceptCHHeaderUKMSeen(*ukm_recorder_, kStandardHTTPHeaderHints,
                              /*loads=*/1);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);

  // Clear settings.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::COOKIES);
}

// Ensure that when JavaScript is blocked, client hint preferences are not
// persisted.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsNotPersistedJavaScriptBlocked) {
  ContentSettingsForOneType host_settings;

  // Start a navigation. This navigation makes it possible to block JavaScript
  // later.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), without_accept_ch_url()));

  const GURL gurl = accept_ch_url();

  // Block JavaScript: Client hint preferences should not be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(
          gurl, GURL(), ContentSettingsType::JAVASCRIPT, CONTENT_SETTING_BLOCK);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), accept_ch_url()));
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());
  VerifyContentSettingsNotNotified();

  // Allow JavaScript: Client hint preferences should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(
          gurl, GURL(), ContentSettingsType::JAVASCRIPT, CONTENT_SETTING_ALLOW);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), accept_ch_url()));
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  // Three navigations occurred but only two had an Accept-CH header.
  ExpectAcceptCHHeaderUKMSeen(*ukm_recorder_, kStandardHTTPHeaderHints,
                              /*loads=*/2);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_, {},
                                /*loads=*/2);

  // Clear settings.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::JAVASCRIPT);
}
IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTestForMetaTagTypes,
                       ClientHintsNotPersistedJavaScriptBlocked) {
  ContentSettingsForOneType host_settings;

  // Start a navigation. This navigation makes it possible to block JavaScript
  // later.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), without_accept_ch_url()));

  GURL gurl;
  switch (GetParam()) {
    case network::MetaCHType::HttpEquivAcceptCH:
      gurl = http_equiv_accept_ch_with_iframe_url();
      break;
    case network::MetaCHType::HttpEquivDelegateCH:
      gurl = meta_equiv_delegate_ch_with_iframe_url();
      break;
  }

  // Block JavaScript: Client hint preferences should not be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(
          gurl, GURL(), ContentSettingsType::JAVASCRIPT, CONTENT_SETTING_BLOCK);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), accept_ch_url()));
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());
  VerifyContentSettingsNotNotified();

  // Allow JavaScript: Client hint preferences should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(
          gurl, GURL(), ContentSettingsType::JAVASCRIPT, CONTENT_SETTING_ALLOW);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), accept_ch_url()));
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());
  ExpectAcceptCHMetaUKMSeen(*ukm_recorder_, {},
                            /*loads=*/1);
  ExpectDelegateCHMetaUKMSeen(*ukm_recorder_, {},
                              /*loads=*/1);

  // Clear settings.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::JAVASCRIPT);
}

// Ensure that when JavaScript is blocked, persisted client hints are not
// attached to the request headers.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsNotAttachedJavaScriptBlocked) {
  const GURL gurl = accept_ch_url();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Fetching accept_ch_url() should persist the request for
  // client hints.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_EQ(1u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(1u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(1u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize",
                                      expected_client_hints_number, 1);
  base::RunLoop().RunUntilIdle();

  // Clients hints preferences for one origin should be persisted.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());

  // Block JavaScript via WebPreferences: Client hints should not be attached.
  SetJsEnabledForActiveView(false);

  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), without_accept_ch_url()));

  EXPECT_EQ(0u, count_client_hints_headers_seen());
  VerifyContentSettingsNotNotified();
  EXPECT_EQ(1u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(1u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(1u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());

  SetJsEnabledForActiveView(true);

  // Block JavaScript via ContentSetting: Client hints should not be attached.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(without_accept_ch_url(), GURL(),
                                      ContentSettingsType::JAVASCRIPT,
                                      CONTENT_SETTING_BLOCK);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), without_accept_ch_url()));
  EXPECT_EQ(0u, count_client_hints_headers_seen());
  VerifyContentSettingsNotNotified();
  EXPECT_EQ(1u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(1u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(1u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());

  // Allow JavaScript: Client hints should now be attached.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(without_accept_ch_url(), GURL(),
                                      ContentSettingsType::JAVASCRIPT,
                                      CONTENT_SETTING_ALLOW);

  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), without_accept_ch_url()));

  // The user agent hint is attached to all three requests:
  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(3u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());
  ExpectAcceptCHHeaderUKMSeen(*ukm_recorder_, kStandardHTTPHeaderHints,
                              /*loads=*/1);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);

  // Clear settings.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::JAVASCRIPT);
}

// Test that if the content settings are malformed, then the browser does not
// crash.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsMalformedContentSettings) {
  ContentSettingsForOneType client_hints_settings;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());

  // Add setting for the host.
  base::Value::List client_hints_list;
  client_hints_list.Append(42 /* client hint value */);
  base::Value::Dict client_hints_dictionary;
  client_hints_dictionary.Set(client_hints::kClientHintsSettingKey,
                              base::Value(std::move(client_hints_list)));
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      without_accept_ch_url(), GURL(), ContentSettingsType::CLIENT_HINTS,
      base::Value(std::move(client_hints_dictionary)));

  // Reading the settings should now return one setting.
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, &client_hints_settings);
  EXPECT_EQ(1U, client_hints_settings.size());

  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), without_accept_ch_url()));
}

// Ensure that when JavaScript is blocked, client hints requested using
// Accept-CH are not attached to the request headers for subresources.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, ClientHintsScriptNotAllowed) {
  const GURL gurl = accept_ch_url();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Block Javascript: Client hints should not be attached.
  SetClientHintExpectationsOnSubresources(false);
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(accept_ch_img_localhost(), GURL(),
                                      ContentSettingsType::JAVASCRIPT,
                                      CONTENT_SETTING_BLOCK);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), accept_ch_img_localhost()));
  EXPECT_EQ(0u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(0u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(0u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());
  EXPECT_EQ(0u, count_client_hints_headers_seen());
  EXPECT_EQ(1u, third_party_request_count_seen());
  EXPECT_EQ(0u, third_party_client_hints_count_seen());

  // Allow Javascript: Client hints should now be attached.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(accept_ch_img_localhost(), GURL(),
                                      ContentSettingsType::JAVASCRIPT,
                                      CONTENT_SETTING_ALLOW);

  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), accept_ch_img_localhost()));

  EXPECT_EQ(2u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(2u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(2u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());
  EXPECT_EQ(expected_client_hints_number, count_client_hints_headers_seen());
  EXPECT_EQ(2u, third_party_request_count_seen());
  EXPECT_EQ(3u, third_party_client_hints_count_seen());
  VerifyContentSettingsNotNotified();

  // Clear settings.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::JAVASCRIPT);

  // Block Javascript again: Client hints should not be attached.
  SetClientHintExpectationsOnSubresources(false);
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(accept_ch_img_localhost(), GURL(),
                                      ContentSettingsType::JAVASCRIPT,
                                      CONTENT_SETTING_BLOCK);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), accept_ch_img_localhost()));
  EXPECT_EQ(2u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(2u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(2u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());
  EXPECT_EQ(expected_client_hints_number, count_client_hints_headers_seen());
  EXPECT_EQ(3u, third_party_request_count_seen());
  EXPECT_EQ(3u, third_party_client_hints_count_seen());

  // Clear settings.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::JAVASCRIPT);
  // Three navigations occurred.
  ExpectAcceptCHHeaderUKMSeen(*ukm_recorder_, kStandardHTTPHeaderHints,
                              /*loads=*/3);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_, {},
                                /*loads=*/3);
}
IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTestForMetaTagTypes,
                       ClientHintsScriptNotAllowed) {
  GURL gurl;
  switch (GetParam()) {
    case network::MetaCHType::HttpEquivAcceptCH:
      gurl = http_equiv_accept_ch_url();
      break;
    case network::MetaCHType::HttpEquivDelegateCH:
      gurl = meta_equiv_delegate_ch_url();
      break;
  }

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Block Javascript: Client hints should not be attached.
  SetClientHintExpectationsOnSubresources(false);
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(accept_ch_img_localhost(), GURL(),
                                      ContentSettingsType::JAVASCRIPT,
                                      CONTENT_SETTING_BLOCK);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), accept_ch_img_localhost()));
  EXPECT_EQ(0u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(0u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(0u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());
  EXPECT_EQ(0u, count_client_hints_headers_seen());
  EXPECT_EQ(1u, third_party_request_count_seen());
  EXPECT_EQ(0u, third_party_client_hints_count_seen());

  // Allow Javascript: Client hints should now be attached.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(accept_ch_img_localhost(), GURL(),
                                      ContentSettingsType::JAVASCRIPT,
                                      CONTENT_SETTING_ALLOW);

  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), accept_ch_img_localhost()));

  EXPECT_EQ(2u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(2u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(2u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());
  EXPECT_EQ(expected_client_hints_number, count_client_hints_headers_seen());
  EXPECT_EQ(2u, third_party_request_count_seen());
  EXPECT_EQ(3u, third_party_client_hints_count_seen());
  VerifyContentSettingsNotNotified();

  // Clear settings.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::JAVASCRIPT);

  // Block Javascript again: Client hints should not be attached.
  SetClientHintExpectationsOnSubresources(false);
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(accept_ch_img_localhost(), GURL(),
                                      ContentSettingsType::JAVASCRIPT,
                                      CONTENT_SETTING_BLOCK);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), accept_ch_img_localhost()));
  EXPECT_EQ(2u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(2u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(2u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());
  EXPECT_EQ(expected_client_hints_number, count_client_hints_headers_seen());
  EXPECT_EQ(3u, third_party_request_count_seen());
  EXPECT_EQ(3u, third_party_client_hints_count_seen());
  ExpectAcceptCHMetaUKMSeen(*ukm_recorder_, {},
                            /*loads=*/1);
  ExpectDelegateCHMetaUKMSeen(*ukm_recorder_, {},
                              /*loads=*/1);

  // Clear settings.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::JAVASCRIPT);
}

// Ensure that when the cookies is blocked, client hints are attached to the
// request headers.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, ClientHintsCookiesNotAllowed) {
  const GURL gurl = accept_ch_img_localhost();

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_ =
      CookieSettingsFactory::GetForProfile(browser()->profile());

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Block cookies.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(
          gurl, GURL(), ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  base::RunLoop().RunUntilIdle();

  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  EXPECT_EQ(2u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(2u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(2u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());
  EXPECT_EQ(expected_client_hints_number, count_client_hints_headers_seen());
  EXPECT_EQ(1u, third_party_request_count_seen());
  EXPECT_EQ(3u, third_party_client_hints_count_seen());
  ExpectAcceptCHHeaderUKMSeen(*ukm_recorder_, kStandardHTTPHeaderHints,
                              /*loads=*/1);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);

  // Clear settings.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::COOKIES);
}
IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTestForMetaTagTypes,
                       ClientHintsCookiesNotAllowed) {
  GURL gurl;
  switch (GetParam()) {
    case network::MetaCHType::HttpEquivAcceptCH:
      gurl = http_equiv_accept_ch_img_localhost();
      break;
    case network::MetaCHType::HttpEquivDelegateCH:
      gurl = meta_equiv_delegate_ch_img_localhost();
      break;
  }

  base::HistogramTester histogram_tester;
  ContentSettingsForOneType host_settings;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_ =
      CookieSettingsFactory::GetForProfile(browser()->profile());

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Block cookies.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(
          gurl, GURL(), ContentSettingsType::COOKIES, CONTENT_SETTING_BLOCK);
  base::RunLoop().RunUntilIdle();

  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  EXPECT_EQ(2u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(2u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(2u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());
  EXPECT_EQ(expected_client_hints_number, count_client_hints_headers_seen());
  EXPECT_EQ(1u, third_party_request_count_seen());
  EXPECT_EQ(3u, third_party_client_hints_count_seen());
  switch (GetParam()) {
    case network::MetaCHType::HttpEquivAcceptCH:
      ExpectAcceptCHMetaUKMSeen(*ukm_recorder_, kExtendedAcceptCHMetaHints,
                                /*loads=*/1);
      ExpectDelegateCHMetaUKMSeen(*ukm_recorder_, {},
                                  /*loads=*/1);
      break;
    case network::MetaCHType::HttpEquivDelegateCH:
      ExpectAcceptCHMetaUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);
      ExpectDelegateCHMetaUKMSeen(*ukm_recorder_, kExtendedDelegateCHMetaHints,
                                  /*loads=*/1);
      break;
  }

  // Clear settings.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->ClearSettingsForOneType(ContentSettingsType::COOKIES);
}

// Verify that client hints are sent in the incognito profiles, and server
// client hint opt-ins are honored within the incognito profile.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       ClientHintsFollowedByNoClientHintIncognito) {
  const GURL gurl = accept_ch_url();

  base::HistogramTester histogram_tester;
  Browser* incognito = CreateIncognitoBrowser();
  ContentSettingsForOneType host_settings;

  HostContentSettingsMapFactory::GetForProfile(incognito->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(0u, host_settings.size());

  // Fetching `gurl` should persist the request for client hints.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito, gurl));

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateEventCount", 1, 1);

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  histogram_tester.ExpectUniqueSample("ClientHints.UpdateSize",
                                      expected_client_hints_number, 1);
  base::RunLoop().RunUntilIdle();

  // Clients hints preferences for one origin should be persisted.
  HostContentSettingsMapFactory::GetForProfile(incognito->profile())
      ->GetSettingsForOneType(ContentSettingsType::CLIENT_HINTS,
                              &host_settings);
  EXPECT_EQ(1u, host_settings.size());

  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito, without_accept_ch_url()));

  // The user agent hint is attached to all three requests:
  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(3u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());

  // Expected number of hints attached to the image request, and the same number
  // to the main frame request.
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());

  // Navigate using regular profile. Client hints should not be send.
  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), without_accept_ch_url()));

  // The user agent hint is attached to the two new requests.
  EXPECT_EQ(5u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(5u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(5u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());

  // No additional hints are sent.
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());
  ExpectAcceptCHHeaderUKMSeen(*ukm_recorder_, kStandardHTTPHeaderHints,
                              /*loads=*/1);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_, {},
                                /*loads=*/1);
}

class ClientHintsWebHoldbackBrowserTest : public ClientHintsBrowserTest {
 public:
  ClientHintsWebHoldbackBrowserTest() : ClientHintsBrowserTest() {
    ConfigureHoldbackExperiment();
  }

  net::EffectiveConnectionType web_effective_connection_type_override() const {
    return web_effective_connection_type_override_;
  }

  std::unique_ptr<base::FeatureList> EnabledFeatures() override {
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
    const std::string kTrialName = "TrialFoo";
    const std::string kGroupName = "GroupFoo";  // Value not used

    scoped_refptr<base::FieldTrial> trial =
        base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);

    std::map<std::string, std::string> params;

    params["web_effective_connection_type_override"] =
        net::GetNameForEffectiveConnectionType(
            web_effective_connection_type_override_);
    EXPECT_TRUE(
        base::FieldTrialParamAssociator::GetInstance()
            ->AssociateFieldTrialParams(kTrialName, kGroupName, params));

    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine("UserAgentClientHint", "");
    feature_list->RegisterFieldTrialOverride(
        features::kNetworkQualityEstimatorWebHoldback.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
    return feature_list;
  }

 private:
  void ConfigureHoldbackExperiment() {}

  const net::EffectiveConnectionType web_effective_connection_type_override_ =
      net::EFFECTIVE_CONNECTION_TYPE_3G;
};

// Make sure that when NetInfo holdback experiment is enabled, the NetInfo APIs
// and client hints return the overridden values. Verify that the client hints
// are overridden on both main frame and subresource requests.
IN_PROC_BROWSER_TEST_F(ClientHintsWebHoldbackBrowserTest,
                       EffectiveConnectionTypeChangeNotified) {
  SetExpectedEffectiveConnectionType(web_effective_connection_type_override());

  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(true);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), accept_ch_url()));
  EXPECT_EQ(0u, count_client_hints_headers_seen());
  EXPECT_EQ(0u, third_party_request_count_seen());
  EXPECT_EQ(0u, third_party_client_hints_count_seen());

  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           accept_ch_with_subresource_url()));
  base::RunLoop().RunUntilIdle();
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  EXPECT_EQ(3u, count_user_agent_hint_headers_seen());
  EXPECT_EQ(3u, count_ua_mobile_client_hints_headers_seen());
  EXPECT_EQ(3u, count_ua_platform_client_hints_headers_seen());
  EXPECT_EQ(0u, count_save_data_client_hints_headers_seen());
  EXPECT_EQ(expected_client_hints_number * 2,
            count_client_hints_headers_seen());
  EXPECT_EQ(0u, third_party_request_count_seen());
  EXPECT_EQ(0u, third_party_client_hints_count_seen());
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, UseCounter) {
  auto web_feature_waiter =
      std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
          chrome_test_utils::GetActiveWebContents(this));

  web_feature_waiter->AddWebFeatureExpectation(
      blink::mojom::WebFeature::kClientHintsUAFullVersion);
  const GURL gurl = accept_ch_url();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));

  web_feature_waiter->Wait();
}
IN_PROC_BROWSER_TEST_P(ClientHintsBrowserTestForMetaTagTypes, UseCounter) {
  auto web_feature_waiter =
      std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
          chrome_test_utils::GetActiveWebContents(this));

  web_feature_waiter->AddWebFeatureExpectation(
      blink::mojom::WebFeature::kClientHintsUAFullVersion);
  GURL gurl;
  switch (GetParam()) {
    case network::MetaCHType::HttpEquivAcceptCH:
      gurl = http_equiv_accept_ch_url();
      break;
    case network::MetaCHType::HttpEquivDelegateCH:
      gurl = meta_equiv_delegate_ch_url();
      break;
  }

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  web_feature_waiter->Wait();
}

class CriticalClientHintsBrowserTest : public InProcessBrowserTest {
 public:
  CriticalClientHintsBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.ServeFilesFromSourceDirectory(
        "chrome/test/data/client_hints");
    https_server_.RegisterRequestMonitor(base::BindRepeating(
        &CriticalClientHintsBrowserTest::MonitorResourceRequest,
        base::Unretained(this)));
    https_server_.RegisterRequestHandler(
        base::BindRepeating(&CriticalClientHintsBrowserTest::CriticalCHRedirect,
                            base::Unretained(this)));

    EXPECT_TRUE(https_server_.Start());
  }

  void SetUp() override {
    std::unique_ptr<base::FeatureList> feature_list =
        std::make_unique<base::FeatureList>();
    // Don't include ClientHintsDPR in the enabled features; we will verify that
    // sec-ch-dpr is not included.
    feature_list->InitializeFromCommandLine(
        "UserAgentClientHint,CriticalClientHint,AcceptCHFrame",
        "ClientHintsDPR");
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    InProcessBrowserTest::SetUpOnMainThread();
  }

  GURL critical_ch_ua_full_version_url() const {
    return https_server_.GetURL("/critical_ch_ua_full_version.html");
  }

  GURL critical_ch_dpr_url() const {
    return https_server_.GetURL("/critical_ch_dpr.html");
  }

  GURL critical_ch_ua_full_version_list_url() const {
    return https_server_.GetURL("/critical_ch_ua_full_version_list.html");
  }

  GURL critical_ch_redirect(GURL target,
                            int status = net::HTTP_TEMPORARY_REDIRECT) const {
    return https_server_.GetURL(
        "/redirect-criticl-ch"
        "?url=" +
        target.spec() + "&status=" + base::NumberToString(status));
  }

  GURL blank_url() { return https_server_.GetURL("/blank.html"); }

  GURL accept_ch_empty() {
    return https_server_.GetURL("/accept_ch_empty.html");
  }

  const absl::optional<std::string>& observed_ch_ua_full_version() {
    base::AutoLock lock(ch_ua_full_version_lock_);
    return ch_ua_full_version_;
  }

  const absl::optional<std::string>& observed_ch_ua_full_version_list() {
    base::AutoLock lock(ch_ua_full_version_list_lock_);
    return ch_ua_full_version_list_;
  }

  const absl::optional<std::string>& observed_ch_dpr() {
    base::AutoLock lock(ch_dpr_lock_);
    return ch_dpr_;
  }

  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    if (request.headers.find("sec-ch-ua-full-version") !=
        request.headers.end()) {
      SetChUaFullVersion(request.headers.at("sec-ch-ua-full-version"));
    }
    if (request.headers.find("sec-ch-ua-full-version-list") !=
        request.headers.end()) {
      SetChUaFullVersionList(request.headers.at("sec-ch-ua-full-version-list"));
    }
    if (request.headers.find("sec-ch-dpr") != request.headers.end()) {
      SetChDpr(request.headers.at("sec-ch-dpr"));
    }
  }

  std::unique_ptr<net::test_server::HttpResponse> CriticalCHRedirect(
      const net::test_server::HttpRequest& request) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();

    if (!base::StartsWith(request.relative_url, "/redirect-criticl-ch"))
      return nullptr;

    net::test_server::RequestQuery query =
        net::test_server::ParseQuery(request.GetURL());

    std::string location = base::UnescapeBinaryURLComponent(query["url"][0]);

    net::HttpStatusCode status_code = net::HTTP_TEMPORARY_REDIRECT;
    auto query_code = query.find("status");
    int query_code_int;
    if (query_code != query.end() &&
        base::StringToInt(query_code->second[0], &query_code_int))
      status_code = static_cast<net::HttpStatusCode>(query_code_int);

    http_response->set_code(status_code);
    http_response->AddCustomHeader("Location", location);
    http_response->AddCustomHeader("Accept-CH", "sec-ch-ua-full-version");
    http_response->AddCustomHeader("Critical-CH", "sec-ch-ua-full-version");

    return http_response;
  }

 protected:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

 private:
  void SetChUaFullVersion(const std::string& ch_ua_full_version) {
    base::AutoLock lock(ch_ua_full_version_lock_);
    ch_ua_full_version_ = ch_ua_full_version;
  }

  void SetChUaFullVersionList(const std::string& ch_ua_full_version_list) {
    base::AutoLock lock(ch_ua_full_version_list_lock_);
    ch_ua_full_version_list_ = ch_ua_full_version_list;
  }

  void SetChDpr(const std::string& ch_dpr) {
    base::AutoLock lock(ch_dpr_lock_);
    ch_dpr_ = ch_dpr;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
  base::Lock ch_ua_full_version_lock_;
  absl::optional<std::string> ch_ua_full_version_
      GUARDED_BY(ch_ua_full_version_lock_);
  base::Lock ch_ua_full_version_list_lock_;
  absl::optional<std::string> ch_ua_full_version_list_
      GUARDED_BY(ch_ua_full_version_list_lock_);
  base::Lock ch_dpr_lock_;
  absl::optional<std::string> ch_dpr_ GUARDED_BY(ch_dpr_lock_);
};

// Verify that setting Critical-CH in the response header causes the request to
// be resent with the client hint included.
IN_PROC_BROWSER_TEST_F(CriticalClientHintsBrowserTest,
                       CriticalClientHintInRequestHeader) {
  blink::UserAgentMetadata ua = embedder_support::GetUserAgentMetadata();
  // On the first navigation request, the client hints in the Critical-CH
  // should be set on the request header.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           critical_ch_ua_full_version_url()));
  const std::string expected_ch_ua_full_version = "\"" + ua.full_version + "\"";
  EXPECT_THAT(observed_ch_ua_full_version(),
              Optional(Eq(expected_ch_ua_full_version)));
  EXPECT_EQ(observed_ch_dpr(), absl::nullopt);
  EXPECT_EQ(observed_ch_ua_full_version_list(), absl::nullopt);
  // One navigation occurred but it was restarted.
  ExpectAcceptCHHeaderUKMSeen(
      *ukm_recorder_, {network::mojom::WebClientHintsType::kUAFullVersion},
      /*loads=*/2);
  ExpectCriticalCHHeaderUKMSeen(
      *ukm_recorder_, {network::mojom::WebClientHintsType::kUAFullVersion},
      /*loads=*/2);
}

// Verify that setting Critical-CH in the response header causes the request to
// be resent with the client hint included. Adding a separate test case for
// Sec-CH-UA-Full-Version-List since Sec-CH-UA-Full-Version will be deprecated.
IN_PROC_BROWSER_TEST_F(CriticalClientHintsBrowserTest,
                       CriticalClientHintFullVersionListInRequestHeader) {
  blink::UserAgentMetadata ua = embedder_support::GetUserAgentMetadata();
  // On the first navigation request, the client hints in the Critical-CH
  // should be set on the request header.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), critical_ch_ua_full_version_list_url()));
  const std::string expected_ch_ua_full_version_list =
      ua.SerializeBrandFullVersionList();
  EXPECT_THAT(observed_ch_ua_full_version_list(),
              Optional(Eq(expected_ch_ua_full_version_list)));
  // The request should not have been resent, so ch-ua-full-version and dpr
  // should also not be present.
  EXPECT_EQ(observed_ch_ua_full_version(), absl::nullopt);
  EXPECT_EQ(observed_ch_dpr(), absl::nullopt);
  // One navigation occurred but it was restarted.
  ExpectAcceptCHHeaderUKMSeen(
      *ukm_recorder_,
      {network::mojom::WebClientHintsType::kUAFullVersionList,
       network::mojom::WebClientHintsType::kDpr},
      /*loads=*/2);
  ExpectCriticalCHHeaderUKMSeen(
      *ukm_recorder_, {network::mojom::WebClientHintsType::kUAFullVersionList},
      /*loads=*/2);
}

// Verify that setting Critical-CH in the response header with a client hint
// that is filtered out of Accept-CH causes the request to *not* be resent and
// the critical client hint is not included.
IN_PROC_BROWSER_TEST_F(
    CriticalClientHintsBrowserTest,
    CriticalClientHintFilteredOutOfAcceptChNotInRequestHeader) {
  // On the first navigation request, the client hints in the Critical-CH
  // should be set on the request header, but in this case, the kClientHintsDPR
  // is not enabled, so the critical client hint won't be set in the request
  // header.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), critical_ch_dpr_url()));
  EXPECT_EQ(observed_ch_dpr(), absl::nullopt);
  // The request should not have been resent, so ch-ua-full-version should also
  // not be present.
  EXPECT_EQ(observed_ch_ua_full_version(), absl::nullopt);
  ExpectAcceptCHHeaderUKMSeen(
      *ukm_recorder_,
      {network::mojom::WebClientHintsType::kDpr,
       network::mojom::WebClientHintsType::kUAFullVersion},
      /*loads=*/1);
  ExpectCriticalCHHeaderUKMSeen(*ukm_recorder_,
                                {network::mojom::WebClientHintsType::kDpr},
                                /*loads=*/1);
}

IN_PROC_BROWSER_TEST_F(CriticalClientHintsBrowserTest, OneRestartSingleOrigin) {
  AlternatingCriticalCHRequestHandler handler;
  net::test_server::EmbeddedTestServer https_server =
      net::test_server::EmbeddedTestServer(
          net::test_server::EmbeddedTestServer::TYPE_HTTPS);

  https_server.RegisterRequestHandler(handler.GetRequestHandler());

  ASSERT_TRUE(https_server.Start());

  base::HistogramTester histogram;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server.GetURL(AlternatingCriticalCHRequestHandler::kCriticalCH)));
  histogram.ExpectBucketCount("ClientHints.CriticalCHRestart",
                              2 /*=kNavigationRestarted*/, 1);
  EXPECT_EQ(2, handler.request_count());
}

IN_PROC_BROWSER_TEST_F(CriticalClientHintsBrowserTest,
                       OneRestartPerNavigation) {
  AlternatingCriticalCHRequestHandler handler;
  net::test_server::EmbeddedTestServer https_server =
      net::test_server::EmbeddedTestServer(
          net::test_server::EmbeddedTestServer::TYPE_HTTPS);

  https_server.RegisterRequestHandler(handler.GetRequestHandler());

  ASSERT_TRUE(https_server.Start());

  // Two navigations, two separate restarts
  base::HistogramTester histogram;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server.GetURL(AlternatingCriticalCHRequestHandler::kCriticalCH)));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server.GetURL(AlternatingCriticalCHRequestHandler::kCriticalCH)));
  histogram.ExpectBucketCount("ClientHints.CriticalCHRestart",
                              2 /*=kNavigationRestarted*/, 2);
  EXPECT_EQ(4, handler.request_count());
}

IN_PROC_BROWSER_TEST_F(CriticalClientHintsBrowserTest,
                       NoRestartIfHintsAlreadyPresent) {
  base::HistogramTester histogram;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), critical_ch_ua_full_version_list_url()));
  histogram.ExpectBucketCount("ClientHints.CriticalCHRestart",
                              2 /*=kNavigationRestarted*/, 1);

  // Ensure that hints are now in storage.
  ContentSettingsForOneType client_hints_settings;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, &client_hints_settings);
  ASSERT_EQ(1U, client_hints_settings.size());

  // Because hints are already in storage, there should be no restart.
  base::HistogramTester histogram_after;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), critical_ch_ua_full_version_list_url()));
  histogram_after.ExpectBucketCount("ClientHints.CriticalCHRestart",
                                    1 /*=kHeaderPresent*/, 1);
  histogram_after.ExpectBucketCount("ClientHints.CriticalCHRestart",
                                    2 /*=kNavigationRestarted*/, 0);
  // Two navigation occurred but one was restarted.
  ExpectAcceptCHHeaderUKMSeen(
      *ukm_recorder_,
      {network::mojom::WebClientHintsType::kUAFullVersionList,
       network::mojom::WebClientHintsType::kDpr},
      /*loads=*/3);
  ExpectCriticalCHHeaderUKMSeen(
      *ukm_recorder_, {network::mojom::WebClientHintsType::kUAFullVersionList},
      /*loads=*/3);
}

IN_PROC_BROWSER_TEST_F(CriticalClientHintsBrowserTest,
                       HintsPersistAfterRestart) {
  base::HistogramTester histogram;
  // Critical-CH on a redirect to a page with no headers.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), critical_ch_ua_full_version_list_url()));
  histogram.ExpectBucketCount("ClientHints.CriticalCHRestart",
                              2 /*=kNavigationRestarted*/, 1);

  // Ensure that hints are now in storage.
  ContentSettingsForOneType client_hints_settings;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, &client_hints_settings);
  ASSERT_EQ(1U, client_hints_settings.size());
  // One navigation occurred but it was restarted.
  ExpectAcceptCHHeaderUKMSeen(
      *ukm_recorder_,
      {network::mojom::WebClientHintsType::kUAFullVersionList,
       network::mojom::WebClientHintsType::kDpr},
      /*loads=*/2);
  ExpectCriticalCHHeaderUKMSeen(
      *ukm_recorder_, {network::mojom::WebClientHintsType::kUAFullVersionList},
      /*loads=*/2);
}

class CriticalClientHintsRedirectBrowserTest
    : public CriticalClientHintsBrowserTest,
      public testing::WithParamInterface<net::HttpStatusCode> {};

INSTANTIATE_TEST_SUITE_P(AllRedirectCodes,
                         CriticalClientHintsRedirectBrowserTest,
                         testing::ValuesIn(kRedirectStatusCodes));

IN_PROC_BROWSER_TEST_P(CriticalClientHintsRedirectBrowserTest,
                       RestartDuringRedirect) {
  blink::UserAgentMetadata ua = embedder_support::GetUserAgentMetadata();
  base::HistogramTester histogram;

  // Critical-CH on a redirect to a page with no headers.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), critical_ch_redirect(blank_url(), GetParam())));
  const std::string expected_ch_ua_full_version = "\"" + ua.full_version + "\"";

  EXPECT_THAT(observed_ch_ua_full_version(),
              Optional(Eq(expected_ch_ua_full_version)));
  histogram.ExpectBucketCount("ClientHints.CriticalCHRestart",
                              2 /*=kNavigationRestarted*/, 1);
}

IN_PROC_BROWSER_TEST_P(CriticalClientHintsRedirectBrowserTest,
                       InsecureRedirectToSecureRedirect) {
  blink::UserAgentMetadata ua = embedder_support::GetUserAgentMetadata();
  base::HistogramTester histogram;

  net::test_server::EmbeddedTestServer http_server;
  http_server.AddDefaultHandlers();
  ASSERT_TRUE(http_server.Start());

  // http -> https + Critical-CH -> https blank
  GURL url =
      http_server.GetURL("/server-redirect?" +
                         critical_ch_redirect(blank_url(), GetParam()).spec());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  const std::string expected_ch_ua_full_version = "\"" + ua.full_version + "\"";

  EXPECT_THAT(observed_ch_ua_full_version(),
              Optional(Eq(expected_ch_ua_full_version)));
  histogram.ExpectBucketCount("ClientHints.CriticalCHRestart",
                              2 /*=kNavigationRestarted*/, 1);
}

IN_PROC_BROWSER_TEST_P(CriticalClientHintsRedirectBrowserTest,
                       SecureRedirectToInsecureRedirect) {
  blink::UserAgentMetadata ua = embedder_support::GetUserAgentMetadata();
  base::HistogramTester histogram;

  net::test_server::EmbeddedTestServer http_server;
  http_server.AddDefaultHandlers();
  ASSERT_TRUE(http_server.Start());

  // https + Critical-CH -> http -> https blank
  GURL redirect_url =
      http_server.GetURL("/server-redirect?" + blank_url().spec());
  GURL url = critical_ch_redirect(redirect_url, GetParam());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  const std::string expected_ch_ua_full_version = "\"" + ua.full_version + "\"";

  EXPECT_THAT(observed_ch_ua_full_version(),
              Optional(Eq(expected_ch_ua_full_version)));
  histogram.ExpectBucketCount("ClientHints.CriticalCHRestart",
                              2 /*=kNavigationRestarted*/, 1);
}

IN_PROC_BROWSER_TEST_P(CriticalClientHintsRedirectBrowserTest,
                       OneRestartSingleOriginRedirect) {
  // "Permanent" redirects are cached and don't actually send a second request
  // before redirecting
  if (GetParam() == net::HTTP_PERMANENT_REDIRECT ||
      GetParam() == net::HTTP_MOVED_PERMANENTLY) {
    return;
  }

  AlternatingCriticalCHRequestHandler handler;
  net::test_server::EmbeddedTestServer https_server =
      net::test_server::EmbeddedTestServer(
          net::test_server::EmbeddedTestServer::TYPE_HTTPS);

  https_server.RegisterRequestHandler(handler.GetRequestHandler());

  ASSERT_TRUE(https_server.Start());

  handler.SetRedirectLocation(https_server.GetURL("/"));
  handler.SetStatusCode(GetParam());

  base::HistogramTester histogram;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server.GetURL(AlternatingCriticalCHRequestHandler::kCriticalCH)));
  histogram.ExpectBucketCount("ClientHints.CriticalCHRestart",
                              2 /*=kNavigationRestarted*/, 1);
  EXPECT_EQ(2, handler.request_count());
}

IN_PROC_BROWSER_TEST_P(CriticalClientHintsRedirectBrowserTest,
                       OneRestartMultipleOriginRedirect) {
  // "Permanent" redirects are cached and don't actually send a second request
  // before redirecting
  if (GetParam() == net::HTTP_PERMANENT_REDIRECT ||
      GetParam() == net::HTTP_MOVED_PERMANENTLY) {
    return;
  }

  AlternatingCriticalCHRequestHandler handler_1, handler_2;

  net::test_server::EmbeddedTestServer https_server_1 =
      net::test_server::EmbeddedTestServer(
          net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  net::test_server::EmbeddedTestServer https_server_2 =
      net::test_server::EmbeddedTestServer(
          net::test_server::EmbeddedTestServer::TYPE_HTTPS);

  https_server_1.RegisterRequestHandler(handler_1.GetRequestHandler());
  https_server_2.RegisterRequestHandler(handler_2.GetRequestHandler());

  ASSERT_TRUE(https_server_1.Start());
  ASSERT_TRUE(https_server_2.Start());

  // This will send the two servers redirecting to each other in a loop until
  // the navigation redirect break is tripped.
  handler_1.SetRedirectLocation(
      https_server_2.GetURL(AlternatingCriticalCHRequestHandler::kCriticalCH));
  handler_2.SetRedirectLocation(
      https_server_1.GetURL(AlternatingCriticalCHRequestHandler::kCriticalCH));

  handler_1.SetStatusCode(GetParam());
  handler_2.SetStatusCode(GetParam());

  base::HistogramTester histogram;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server_1.GetURL(AlternatingCriticalCHRequestHandler::kCriticalCH)));
  histogram.ExpectBucketCount("ClientHints.CriticalCHRestart",
                              2 /*=kNavigationRestarted*/, 2);
  EXPECT_EQ(net::URLRequest::kMaxRedirects,
            handler_1.request_count() + handler_2.request_count());
}

class ClientHintsBrowserTestWithEmulatedMedia
    : public DevToolsProtocolTestBase {
 public:
  ClientHintsBrowserTestWithEmulatedMedia()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    scoped_feature_list_.InitFromCommandLine(
        "UserAgentClientHint,AcceptCHFrame", "");

    https_server_.ServeFilesFromSourceDirectory(
        "chrome/test/data/client_hints");
    https_server_.RegisterRequestMonitor(base::BindRepeating(
        &ClientHintsBrowserTestWithEmulatedMedia::MonitorResourceRequest,
        base::Unretained(this)));
    EXPECT_TRUE(https_server_.Start());

    test_url_ = https_server_.GetURL("/accept_ch.html");
  }

  ClientHintsBrowserTestWithEmulatedMedia(
      const ClientHintsBrowserTestWithEmulatedMedia&) = delete;
  ClientHintsBrowserTestWithEmulatedMedia& operator=(
      const ClientHintsBrowserTestWithEmulatedMedia&) = delete;

  ~ClientHintsBrowserTestWithEmulatedMedia() override = default;

  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    if (base::Contains(request.headers, "sec-ch-prefers-color-scheme")) {
      prefers_color_scheme_observed_ =
          request.headers.at("sec-ch-prefers-color-scheme");
    }
    if (base::Contains(request.headers, "sec-ch-prefers-reduced-motion")) {
      prefers_reduced_motion_observed_ =
          request.headers.at("sec-ch-prefers-reduced-motion");
    }
  }

  const GURL& test_url() const { return test_url_; }

  const std::string& prefers_color_scheme_observed() const {
    return prefers_color_scheme_observed_;
  }

  const std::string& prefers_reduced_motion_observed() const {
    return prefers_reduced_motion_observed_;
  }

  void EmulateMedia(base::StringPiece string) {
    base::Value features = base::test::ParseJson(string);
    DCHECK(features.is_list());
    base::Value::Dict params;
    params.Set("features", std::move(features));
    SendCommandSync("Emulation.setEmulatedMedia", std::move(params));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
  GURL test_url_;
  std::string prefers_color_scheme_observed_;
  std::string prefers_reduced_motion_observed_;
};

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTestWithEmulatedMedia,
                       PrefersColorScheme) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));
  EXPECT_EQ(prefers_color_scheme_observed(), "");
  Attach();

  EmulateMedia(R"([{"name": "prefers-color-scheme", "value": "light"}])");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));
  EXPECT_EQ(prefers_color_scheme_observed(), "light");

  EmulateMedia(R"([{"name": "prefers-color-scheme", "value": "dark"}])");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));
  EXPECT_EQ(prefers_color_scheme_observed(), "dark");
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTestWithEmulatedMedia,
                       PrefersReducedMotion) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));
  EXPECT_EQ(prefers_reduced_motion_observed(), "");
  Attach();

  EmulateMedia(R"([{"name": "prefers-reduced-motion", "value": "reduce"}])");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));
  EXPECT_EQ(prefers_reduced_motion_observed(), "reduce");

  EmulateMedia(R"([{"name": "prefers-reduced-motion", "value": ""}])");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));
  EXPECT_EQ(prefers_reduced_motion_observed(), "no-preference");
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTestWithEmulatedMedia,
                       MultipleMediaFeatures) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));
  EXPECT_EQ(prefers_color_scheme_observed(), "");
  EXPECT_EQ(prefers_reduced_motion_observed(), "");
  Attach();

  EmulateMedia(
      R"([{"name": "prefers-color-scheme", "value": "light"},
          {"name": "prefers-reduced-motion", "value": "reduce"}])");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));
  EXPECT_EQ(prefers_color_scheme_observed(), "light");
  EXPECT_EQ(prefers_reduced_motion_observed(), "reduce");

  EmulateMedia(R"([{"name": "prefers-color-scheme", "value": "dark"}])");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));
  EXPECT_EQ(prefers_color_scheme_observed(), "dark");
  EXPECT_EQ(prefers_reduced_motion_observed(), "no-preference");
}

// Base class for the User-Agent reduction or deprecation Origin Trial browser
// tests.  Common functionality shared between the various UA browser
// tests should go in this class.
class UaOriginTrialBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // The public key for the default privatey key used by the
    // tools/origin_trials/generate_token.py tool.
    static constexpr char kOriginTrialTestPublicKey[] =
        "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";
    command_line->AppendSwitchASCII(embedder_support::kOriginTrialPublicKey,
                                    kOriginTrialTestPublicKey);
  }

  void SetUp() override {
    std::unique_ptr<base::FeatureList> feature_list =
        std::make_unique<base::FeatureList>();
    feature_list->InitializeFromCommandLine(
        "CriticalClientHint,UACHOverrideBlank", "");
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));

    InProcessBrowserTest::SetUp();
  }

  void SetUserAgentOverride(const std::string& ua_override) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    web_contents->SetUserAgentOverride(
        blink::UserAgentOverride::UserAgentOnly(ua_override), false);
    web_contents->GetController()
        .GetLastCommittedEntry()
        ->SetIsOverridingUserAgent(true);
  }

  void CheckUaOriginTrialClientHint(const bool ch_ua_expected) {
    const absl::optional<std::string>& ua_client_hint =
        GetLastUaOriginTrialClientHintValue();

    if (ch_ua_expected) {
      EXPECT_THAT(ua_client_hint, Optional(Eq("?1")));
    } else {
      EXPECT_THAT(ua_client_hint, Eq(absl::nullopt));
    }
  }

  void CheckUserAgentString(const std::string& expected_ua_header_value) {
    EXPECT_THAT(GetLastUserAgentHeaderValue(),
                Optional(expected_ua_header_value));
  }

  void CheckUserAgentReduced(
      const bool expected_user_agent_reduced,
      const bool expected_reduced_ua_through_experiment) {
    const absl::optional<std::string>& user_agent_header_value =
        GetLastUserAgentHeaderValue();
    EXPECT_TRUE(user_agent_header_value.has_value());
    CheckUserAgentMinorVersion(*user_agent_header_value,
                               expected_user_agent_reduced,
                               expected_reduced_ua_through_experiment);
  }

  // |ch_ua_reduced_expected| indicates whether expects a reduce UA string.
  // |ch_ua_exist_expected| indicates whether the corresponding
  // Sec-CH-UA-Reduced or Sec-CH-UA-Full  exist in header.
  void NavigateAndCheckHeaders(const GURL& url,
                               const bool ch_ua_reduced_expected,
                               const bool ch_ua_exist_expected) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    CheckUaOriginTrialClientHint(ch_ua_exist_expected);

    // If we expect reduced user agent, but there is no valid origin trial
    // header, it means reduced UA depends on feature
    // kReduceUserAgentMinorVersion experiment.
    const bool expected_reduced_ua_through_experiment =
        ch_ua_reduced_expected && !ch_ua_exist_expected;
    CheckUserAgentReduced(ch_ua_reduced_expected,
                          expected_reduced_ua_through_experiment);
  }

  bool UAReductionEnabled() {
    return base::FeatureList::IsEnabled(
        blink::features::kReduceUserAgentMinorVersion);
  }

 protected:
  // Returns the value of the User-Agent request header from the last sent
  // request, or nullopt if the header could not be read.
  virtual const absl::optional<std::string>& GetLastUserAgentHeaderValue() = 0;
  // Returns the value of the Sec-CH-UA-Reduced or Sec-CH-UA-Full request header
  // from the last sent request, or nullopt if the header could not be read.
  virtual const absl::optional<std::string>&
  GetLastUaOriginTrialClientHintValue() = 0;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Common tests that verify
// 1. Sec-CH-UA-Reduced client hint is sent if and only if the
// UserAgentReduction Origin Trial token is present and valid in the response
// headers.
// 2. Sec-CH-UA-Full client hint is sent if and only if the
// SendFullUserAgentAfterReduction Origin Trial token is present and valid in
// the response headers.
//
// The test Origin Trial token found in the test files was generated by running
// (in tools/origin_trials):
// generate_token.py https://127.0.0.1:44444 UserAgentReduction
// --expire-timestamp=2000000000
//
// generate_token.py https://127.0.0.1:44444 SendFullUserAgentAfterReduction
// --expire-timestamp=2000000000
//
// The Origin Trial token expires in 2033.  Generate a new token by then, or
// find a better way to re-generate a test trial token.
class SameOriginUaOriginTrialBrowserTest
    : public UaOriginTrialBrowserTest,
      public testing::WithParamInterface<UserAgentOriginTrialTestType> {
 public:
  SameOriginUaOriginTrialBrowserTest() = default;

  // The URL that was used to register the Origin Trial token.
  static constexpr const char kOriginUrl[] = "https://127.0.0.1:44444";

  // According to the low entropy hint table:
  // https://wicg.github.io/client-hints-infrastructure/#low-entropy-hint-table,
  // only 3 hints are low entropy hints
  static constexpr const int kSecChUaLowEntropyCount = 3;

  void SetUpOnMainThread() override {
    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, since
    // the origin trial token in the response is associated with a fixed
    // origin, whereas EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindRepeating(
            &SameOriginUaOriginTrialBrowserTest::InterceptRequest,
            base::Unretained(this)));

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetTestOptions(const OriginTrialTestOptions& test_setting,
                      const std::set<GURL>& expected_request_urls) {
    test_options_ = test_setting;
    expected_request_urls_ = expected_request_urls;
  }

  const absl::optional<std::string>& GetLastUserAgentHeaderValue() override {
    std::string user_agent;
    CHECK(url_loader_interceptor_->GetLastRequestHeaders().GetHeader(
        "user-agent", &user_agent));
    last_user_agent_ = user_agent;
    return last_user_agent_;
  }

  const absl::optional<std::string>& GetLastUaOriginTrialClientHintValue()
      override {
    std::string ch_ua_header_value;
    if (url_loader_interceptor_->GetLastRequestHeaders().GetHeader(
            base::StrCat(
                {GetParam() == UserAgentOriginTrialTestType::UAReduction
                     ? "sec-ch-ua-reduced"
                     : "sec-ch-ua-full"}),
            &ch_ua_header_value)) {
      last_ua_ch_val_ = ch_ua_header_value;
    } else {
      last_ua_ch_val_ = absl::nullopt;
    }
    return last_ua_ch_val_;
  }

  void CheckSecClientHintUaCount() {
    net::HttpRequestHeaders::Iterator header_iterator(
        url_loader_interceptor_->GetLastRequestHeaders());
    int sec_ch_ua_count = 0;
    while (header_iterator.GetNext()) {
      if (base::StartsWith(header_iterator.name(), "sec-ch-ua",
                           base::CompareCase::SENSITIVE)) {
        ++sec_ch_ua_count;
      }
    }

    if (GetParam() == UserAgentOriginTrialTestType::UAReductionAndDeprecation) {
      // Two Accept-CH client hints in header: sec-ch-ua-reduced and
      // sec-ch-ua-full.
      EXPECT_EQ(sec_ch_ua_count, kSecChUaLowEntropyCount + 2);
    } else {
      // One Accept-CH client hint in header: sec-ch-ua-reduced or
      // sec-ch-ua-full.
      EXPECT_EQ(sec_ch_ua_count, kSecChUaLowEntropyCount + 1);
    }
  }

  void VerifyNonAcceptCHNotAddedToHeader(const std::string& client_hint) {
    ASSERT_FALSE(url_loader_interceptor_->GetLastRequestHeaders().HasHeader(
        client_hint));
  }

  GURL ua_with_valid_origin_trial_token_url() const {
    return GURL(base::StrCat(
        {kOriginUrl, "/accept_ch_ua_with_valid_origin_trial.html"}));
  }

  GURL ua_with_invalid_origin_trial_token_url() const {
    return GURL(base::StrCat(
        {kOriginUrl, "/accept_ch_ua_with_invalid_origin_trial.html"}));
  }

  GURL ua_with_no_origin_trial_token_url() const {
    return GURL(
        base::StrCat({kOriginUrl, "/accept_ch_ua_with_no_origin_trial.html"}));
  }

  GURL ua_missing_with_valid_origin_trial_token_url() const {
    return GURL(base::StrCat(
        {kOriginUrl, "/accept_ch_ua_missing_valid_origin_trial.html"}));
  }

  GURL critical_ch_ua_with_valid_origin_trial_token_url() const {
    return GURL(base::StrCat(
        {kOriginUrl, "/critical_ch_ua_with_valid_origin_trial.html"}));
  }

  GURL critical_ch_ua_with_invalid_origin_trial_token_url() const {
    return GURL(base::StrCat(
        {kOriginUrl, "/critical_ch_ua_with_invalid_origin_trial.html"}));
  }

  GURL accept_ch_ua_subresource_request_url() const {
    return GURL(
        base::StrCat({kOriginUrl, "/accept_ch_ua_subresource_request.html"}));
  }

  GURL accept_ch_ua_iframe_request_url() const {
    return GURL(
        base::StrCat({kOriginUrl, "/accept_ch_ua_iframe_request.html"}));
  }

  GURL accept_ch_ua_iframe_sandbox_request_url() const {
    return GURL(base::StrCat(
        {kOriginUrl, "/accept_ch_ua_iframe_sandbox_request.html"}));
  }

  GURL critical_ch_ua_subresource_request_url() const {
    return GURL(
        base::StrCat({kOriginUrl, "/critical_ch_ua_subresource_request.html"}));
  }

  GURL critical_ch_ua_iframe_request_url() const {
    return GURL(
        base::StrCat({kOriginUrl, "/critical_ch_ua_iframe_request.html"}));
  }

  GURL simple_request_url() const {
    return GURL(base::StrCat({kOriginUrl, "/simple.html"}));
  }

  GURL style_css_request_url() const {
    return GURL(base::StrCat({kOriginUrl, "/style.css"}));
  }

  GURL last_request_url() const {
    return url_loader_interceptor_->GetLastRequestURL();
  }

  void NavigateTwiceAndCheckHeaderReduced(
      const GURL& url,
      const bool ch_ua_reduced_expected,
      const bool critical_ch_ua_reduced_expected) {
    base::HistogramTester histograms;
    int reduced_count = 0;
    int full_count = 0;

    // If Critical-CH is set, we expect Sec-CH-UA-Reduced in the first
    // navigation request header.  If Critical-CH is not set, we don't expect
    // Sec-CH-UA-Reduced in the first navigation request.
    // If Sec-CH-UA-Reduced in the first request, UA string should reduced,
    // otherwise UA string depends on whether kReduceUserAgentMinorVersion has
    // turns up.
    const bool first_navigation_has_sec_reduced_ua =
        critical_ch_ua_reduced_expected && ch_ua_reduced_expected;
    bool first_navigation_expected_reduced_ua = true;
    if (first_navigation_has_sec_reduced_ua) {
      first_navigation_expected_reduced_ua = true;
    } else {
      first_navigation_expected_reduced_ua = UAReductionEnabled();
    }
    NavigateAndCheckHeaders(url, first_navigation_expected_reduced_ua,
                            first_navigation_has_sec_reduced_ua);
    if (first_navigation_has_sec_reduced_ua) {
      ++reduced_count;
      if (critical_ch_ua_reduced_expected) {
        // If Critical-CH was set, there will also be the initial navigation
        // that does not send the reduced UA string.
        ++full_count;
      }
    } else {
      ++full_count;
    }
    // The UserAgentStringType enum is not accessible in //chrome/browser, so
    // we just use the enum's integer value.
    histograms.ExpectBucketCount("Navigation.UserAgentStringType",
                                 /*NavigationRequest::kFullVersion*/ 0,
                                 full_count);
    histograms.ExpectBucketCount("Navigation.UserAgentStringType",
                                 /*NavigationRequest::kReducedVersion*/ 1,
                                 reduced_count);

    // Regardless of the Critical-CH setting, we expect the Sec-CH-UA-Reduced
    // client hint sent on the second request, if Sec-CH-UA-Reduced is set and
    // the Origin Trial token is valid.
    // If Sec-CH-UA-Reduced in the second request, UA string should reduced,
    // otherwise UA string depends on whether kReduceUserAgentMinorVersion has
    // turns up.
    bool second_navigation_has_sec_reduced_ua = ch_ua_reduced_expected;
    bool second_navigation_expected_reduced_ua = true;
    if (second_navigation_has_sec_reduced_ua) {
      second_navigation_expected_reduced_ua = true;
    } else {
      second_navigation_expected_reduced_ua = UAReductionEnabled();
    }
    NavigateAndCheckHeaders(url, second_navigation_expected_reduced_ua,
                            second_navigation_has_sec_reduced_ua);
    // Make sure non-default client hints are not added to the request headers
    // of subresource requests. Here, we just use Sec-CH-UA-Bitness as a high
    // entropy hint to check against.
    VerifyNonAcceptCHNotAddedToHeader("sec-ch-ua-bitness");
    if (ch_ua_reduced_expected) {
      ++reduced_count;
    } else {
      ++full_count;
    }
    histograms.ExpectBucketCount("Navigation.UserAgentStringType",
                                 /*NavigationRequest::kFullVersion*/ 0,
                                 full_count);
    histograms.ExpectBucketCount("Navigation.UserAgentStringType",
                                 /*NavigationRequest::kReducedVersion*/ 1,
                                 reduced_count);
  }

  void NavigateTwiceAndCheckHeaderFull(
      const GURL& url,
      const bool ch_ua_full_expected,
      const bool critical_ch_ua_full_expected) {
    base::HistogramTester histograms;
    int full_count = 0;

    // If Critical-CH is set, we expect Sec-CH-UA-Full in the first
    // navigation request header.  If Critical-CH is not set, we don't expect
    // Sec-CH-UA-Full in the first navigation request.
    const bool first_navigation_has_sec_full_ua =
        critical_ch_ua_full_expected && ch_ua_full_expected;
    // If Sec-CH-UA-Full in the first request, UA string should not reduced,
    // otherwise UA string depends on whether kReduceUserAgentMinorVersion has
    // turns up.
    bool first_navigation_expected_reduced_ua = false;
    if (first_navigation_has_sec_full_ua) {
      first_navigation_expected_reduced_ua = false;
    } else {
      first_navigation_expected_reduced_ua = UAReductionEnabled();
    }
    NavigateAndCheckHeaders(url, first_navigation_expected_reduced_ua,
                            first_navigation_has_sec_full_ua);

    // TODO: Currently no matter whether it's a first navigation request or not,
    // we always sent the full user agent string. We need to update the count
    // logic once we fully migrate to the reduced user agent string.

    // If Critical-CH was set, there will also be the initial navigation
    // that send full UA string.
    if (critical_ch_ua_full_expected) {
      ++full_count;
    }
    ++full_count;

    // The UserAgentStringType enum is not accessible in //chrome/browser, so
    // we just use the enum's integer value.
    histograms.ExpectBucketCount("Navigation.UserAgentStringType",
                                 /*NavigationRequest::kFullVersion*/ 0,
                                 full_count);

    // Regardless of the Critical-CH setting, we expect the Sec-CH-UA-Full
    // client hint sent on the second request, if Sec-CH-UA-Full is set and
    // the Origin Trial token is valid.
    // If Sec-CH-UA-Full in the second request, UA string should not reduced,
    // otherwise UA string depends on whether kReduceUserAgentMinorVersion has
    // turns up.
    bool second_navigation_has_sec_full_ua = ch_ua_full_expected;
    bool second_navigation_expected_reduced_ua = false;
    if (second_navigation_has_sec_full_ua) {
      second_navigation_expected_reduced_ua = false;
    } else {
      second_navigation_expected_reduced_ua = UAReductionEnabled();
    }
    NavigateAndCheckHeaders(url, second_navigation_expected_reduced_ua,
                            ch_ua_full_expected);
    // Make sure non-default client hints are not added to the request headers
    // of subresource requests. Here, we just use Sec-CH-UA-Bitness as a high
    // entropy hint to check against.
    VerifyNonAcceptCHNotAddedToHeader("sec-ch-ua-bitness");

    ++full_count;
    histograms.ExpectBucketCount("Navigation.UserAgentStringType",
                                 /*NavigationRequest::kFullVersion*/ 0,
                                 full_count);
  }

  void NavigateTwiceAndCheckHeader(const GURL& url,
                                   const bool ch_ua_exist_expected,
                                   const bool critical_ch_ua_exist_expected) {
    if (GetParam() == UserAgentOriginTrialTestType::UAReduction) {
      NavigateTwiceAndCheckHeaderReduced(url, ch_ua_exist_expected,
                                         critical_ch_ua_exist_expected);
    } else {
      NavigateTwiceAndCheckHeaderFull(url, ch_ua_exist_expected,
                                      critical_ch_ua_exist_expected);
    }
  }

 private:
  // URLLoaderInterceptor callback
  bool InterceptRequest(URLLoaderInterceptor::RequestParams* params) {
    if (expected_request_urls_.find(params->url_request.url) ==
        expected_request_urls_.end())
      return false;

    std::string path = "chrome/test/data/client_hints";
    path.append(static_cast<std::string>(params->url_request.url.path_piece()));

    if (params->url_request.url.path() == "/style.css" ||
        params->url_request.url.path() == "/simple.html") {
      URLLoaderInterceptor::WriteResponse(path, params->client.get());
      return true;
    }
    std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n";
    base::StrAppend(&headers, {BuildOriginTrialHeader()});
    URLLoaderInterceptor::WriteResponse(path, params->client.get(), &headers,
                                        absl::nullopt,
                                        /*url=*/params->url_request.url);
    return true;
  }

  std::string BuildOriginTrialHeader() {
    std::string headers;

    // Generated by running (in tools/origin_trials):
    // generate_token.py https://127.0.0.1:44444 UserAgentReduction
    //   --expire-timestamp=2000000000
    static constexpr char kUAReducedOriginTrialToken[] =
        "A93QtcQ0CRKf5ioPasUwNbweXQWgbI4ZEshiz+"
        "YS7dkQEWVfW9Ua2pTnA866sZwRzuElkPwsUdGdIaW0fRUP8AwAAABceyJvcm"
        "lnaW4iOiAiaH"
        "R0cHM6Ly8xMjcuMC4wLjE6NDQ0NDQiLCAiZmVhdHVyZSI6ICJVc2VyQWdlbn"
        "RSZWR1Y3Rpb2"
        "4iLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0=";

    // Generated by running (in tools/origin_trials):
    // generate_token.py https://127.0.0.1:44444
    // SendFullUserAgentAfterReduction
    //   --expire-timestamp=2000000000
    static constexpr char kUAFullOriginTrialToken[] =
        "A6+Ti/9KuXTgmFzOQwkTuO8k0QFH8vUaxmv0CllAET1/"
        "307KShF6fhskMuBqFUvqO7ViAkZ+"
        "NSeJhQI0n5aLggsAAABpeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6"
        "NDQ0NDQiLCAiZmVhdHVyZSI6ICJTZW5kRnVsbFVzZXJBZ2VudEFmdGVyUmVk"
        "dWN0aW9uIiwgImV4cGlyeSI6IDIwMDAwMDAwMDB9";

    switch (GetParam()) {
      case UserAgentOriginTrialTestType::UAReduction:
        if (test_options_.has_accept_ch_header) {
          base::StrAppend(&headers, {"Accept-CH: ", "sec-ch-ua-reduced", "\n"});
        }
        if (test_options_.has_critical_ch_header) {
          base::StrAppend(&headers,
                          {"Critical-CH: ", "sec-ch-ua-reduced", "\n"});
        }
        if (test_options_.has_ot_token) {
          base::StrAppend(&headers, {"Origin-Trial: ",
                                     test_options_.valid_ot_token
                                         ? kUAReducedOriginTrialToken
                                         : "invalid",
                                     "\n"});
        }
        break;
      case UserAgentOriginTrialTestType::UADeprecation:
        if (test_options_.has_accept_ch_header) {
          base::StrAppend(&headers, {"Accept-CH: ", "sec-ch-ua-full", "\n"});
        }
        if (test_options_.has_critical_ch_header) {
          base::StrAppend(&headers, {"Critical-CH: ", "sec-ch-ua-full", "\n"});
        }
        if (test_options_.has_ot_token) {
          base::StrAppend(
              &headers, {"Origin-Trial: ",
                         test_options_.valid_ot_token ? kUAFullOriginTrialToken
                                                      : "invalid",
                         "\n"});
        }
        break;
      case UserAgentOriginTrialTestType::UAReductionAndDeprecation:
        if (test_options_.has_accept_ch_header) {
          base::StrAppend(
              &headers,
              {"Accept-CH: ", "sec-ch-ua-reduced, sec-ch-ua-full", "\n"});
        }
        if (test_options_.has_critical_ch_header) {
          base::StrAppend(
              &headers,
              {"Critical-CH: ", "sec-ch-ua-reduced, sec-ch-ua-full", "\n"});
        }

        if (test_options_.has_ot_token) {
          base::StrAppend(
              &headers,
              {"Origin-Trial: ",
               (test_options_.valid_ot_token ? kUAReducedOriginTrialToken
                                             : "invalid"),
               ",",
               (test_options_.valid_ot_token ? kUAFullOriginTrialToken
                                             : "invalid"),
               "\n"});
        }
        break;
      default:
        break;
    }
    return headers;
  }

  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
  absl::optional<std::string> last_user_agent_;
  absl::optional<std::string> last_ua_ch_val_;
  std::set<GURL> expected_request_urls_;
  OriginTrialTestOptions test_options_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SameOriginUaOriginTrialBrowserTest,
    testing::Values(UserAgentOriginTrialTestType::UAReduction,
                    UserAgentOriginTrialTestType::UADeprecation,
                    UserAgentOriginTrialTestType::UAReductionAndDeprecation));

constexpr const char SameOriginUaOriginTrialBrowserTest::kOriginUrl[];
constexpr const int SameOriginUaOriginTrialBrowserTest::kSecChUaLowEntropyCount;

IN_PROC_BROWSER_TEST_P(SameOriginUaOriginTrialBrowserTest,
                       AcceptChUaWithValidOriginTrialToken) {
  SetTestOptions(
      {/*has_ot_token=*/true, /*valid_ot_token=*/true,
       /*has_accept_ch_header=*/true, /*has_critical_ch_header=*/false},
      {ua_with_valid_origin_trial_token_url()});

  NavigateTwiceAndCheckHeader(ua_with_valid_origin_trial_token_url(),
                              /*ch_ua_exist_expected=*/true,
                              /*critical_ch_ua_exist_expected=*/false);

  // The Origin Trial token is valid, so we expect the reduced/full UA values in
  // the Javascript getters as well.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  CheckUserAgentMinorVersion(
      content::EvalJs(web_contents, "navigator.userAgent").ExtractString(),
      /*expected_user_agent_reduced=*/GetParam() ==
          UserAgentOriginTrialTestType::UAReduction,
      false);
  CheckUserAgentMinorVersion(
      content::EvalJs(web_contents, "navigator.appVersion").ExtractString(),
      /*expected_user_agent_reduced=*/GetParam() ==
          UserAgentOriginTrialTestType::UAReduction,
      false);
  // Instead of checking all platform types, just check one that has a
  // difference between the full and reduced versions.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ("Linux x86_64",
            content::EvalJs(web_contents, "navigator.platform"));
#endif

  CheckSecClientHintUaCount();
}

IN_PROC_BROWSER_TEST_P(SameOriginUaOriginTrialBrowserTest,
                       AcceptChUaWithInvalidOriginTrialToken) {
  SetTestOptions(
      {/*has_ot_token=*/true, /*valid_ot_token=*/false,
       /*has_accept_ch_header=*/true, /*has_critical_ch_header=*/false},
      {ua_with_invalid_origin_trial_token_url()});

  // The response contained Sec-CH-UA-Reduced or  Sec-CH-UA-Full in the
  // Accept-CH header, but the origin trial token is invalid.
  NavigateTwiceAndCheckHeader(ua_with_invalid_origin_trial_token_url(),
                              /*ch_ua_exist_expected=*/false,
                              /*critical_ch_ua_exist_expected=*/false);

  // The Origin Trial token is invalid, so we expect the UA values depends on
  // the feature kReduceUserAgentMinorVersion in the Javascript getters.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  CheckUserAgentMinorVersion(
      content::EvalJs(web_contents, "navigator.userAgent").ExtractString(),
      /*expected_user_agent_reduced=*/UAReductionEnabled(), true);
  CheckUserAgentMinorVersion(
      content::EvalJs(web_contents, "navigator.appVersion").ExtractString(),
      /*expected_user_agent_reduced=*/UAReductionEnabled(), true);
  // Instead of checking all platform types, just check one that has a
  // difference between the full and reduced versions.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_NE("Linux x86_64",
            content::EvalJs(web_contents, "navigator.platform"));
#endif
}

IN_PROC_BROWSER_TEST_P(SameOriginUaOriginTrialBrowserTest,
                       AcceptChUaWithNoOriginTrialToken) {
  SetTestOptions(
      {/*has_ot_token=*/false, /*valid_ot_token=*/false,
       /*has_accept_ch_header=*/true, /*has_critical_ch_header=*/false},
      {ua_with_no_origin_trial_token_url()});

  // The response contained Sec-CH-UA-Reduced or Sec-CH-UA-Full in the
  // Accept-CH header, but the origin trial token is not present.
  NavigateTwiceAndCheckHeader(ua_with_no_origin_trial_token_url(),
                              /*ch_ua_exist_expected=*/false,
                              /*critical_ch_ua_exist_expected=*/false);
}

IN_PROC_BROWSER_TEST_P(SameOriginUaOriginTrialBrowserTest,
                       NoAcceptChUaWithValidOriginTrialToken) {
  SetTestOptions(
      {/*has_ot_token=*/true, /*valid_ot_token=*/true,
       /*has_accept_ch_header=*/false, /*has_critical_ch_header=*/false},
      {ua_missing_with_valid_origin_trial_token_url()});

  // The response contained a valid Origin Trial token, but no corresponding
  // Sec-CH-UA-Reduced or Sec-CH-UA-Full in the Accept-CH header.
  NavigateTwiceAndCheckHeader(ua_missing_with_valid_origin_trial_token_url(),
                              /*ch_ua_exist_expected=*/false,
                              /*critical_ch_ua_exist_expected=*/false);
}

IN_PROC_BROWSER_TEST_P(SameOriginUaOriginTrialBrowserTest,
                       CriticalChUaWithValidOriginTrialToken) {
  SetTestOptions(
      {/*has_ot_token=*/true, /*valid_ot_token=*/true,
       /*has_accept_ch_header=*/true, /*has_critical_ch_header=*/true},
      {critical_ch_ua_with_valid_origin_trial_token_url()});

  // The initial navigation also contains the Critical-CH header, so the
  // corresponding Sec-CH-UA-Reduced or Sec-CH-UA-Full header should be set
  // after the first navigation.
  NavigateTwiceAndCheckHeader(
      critical_ch_ua_with_valid_origin_trial_token_url(),
      /*ch_ua_exist_expected=*/true,
      /*critical_ch_ua_exist_expected=*/true);

  CheckSecClientHintUaCount();
}

IN_PROC_BROWSER_TEST_P(SameOriginUaOriginTrialBrowserTest,
                       CriticalChUaWithInvalidOriginTrialToken) {
  SetTestOptions(
      {/*has_ot_token=*/true, /*valid_ot_token=*/false,
       /*has_accept_ch_header=*/true, /*has_critical_ch_header=*/true},
      {critical_ch_ua_with_invalid_origin_trial_token_url()});

  // The Origin Trial token is invalid, so the Critical-CH should not have
  // resulted in the corresponding Sec-CH-UA-Reduced or Sec-CH-UA-Full header
  // being sent.
  NavigateTwiceAndCheckHeader(
      critical_ch_ua_with_invalid_origin_trial_token_url(),
      /*ch_ua_exist_expected=*/false,
      /*critical_ch_ua_exist_expected=*/false);
}

IN_PROC_BROWSER_TEST_P(SameOriginUaOriginTrialBrowserTest,
                       IframeRequestUaWithValidOriginTrialToken) {
  SetTestOptions(
      {/*has_ot_token=*/true, /*valid_ot_token=*/true,
       /*has_accept_ch_header=*/true, /*has_critical_ch_header=*/false},
      {accept_ch_ua_iframe_request_url(), simple_request_url()});

  // The last resource request processed for this navigation will be an embedded
  // iframe request. Since Accept-CH has either Sec-CH-UA-Reduced or
  // Sec-CH-UA-Full set on the top-level level frame's response header, along
  // with a valid origin trial token, the iframe request should send the reduced
  // UA string if Sec-CH-UA-Reduced set, or the full UA string if Sec-CH-UA-Full
  // set in the request header.
  NavigateAndCheckHeaders(accept_ch_ua_iframe_request_url(),
                          /*ch_ua_reduced_expected=*/GetParam() ==
                              UserAgentOriginTrialTestType::UAReduction,
                          /*ch_ua_exist_expected=*/true);

  CheckSecClientHintUaCount();

  // Make sure the last intercepted URL was the request for the embedded iframe.
  EXPECT_EQ(last_request_url().path(), "/simple.html");
}

IN_PROC_BROWSER_TEST_P(SameOriginUaOriginTrialBrowserTest,
                       IframeRequestUaWithValidOriginTrialTokenIgnoreSandbox) {
  SetTestOptions(
      {
          /*has_ot_token=*/true,
          /*valid_ot_token=*/true,
          /*has_accept_ch_header=*/true,
          /*has_critical_ch_header=*/false,
      },
      {accept_ch_ua_iframe_sandbox_request_url(), simple_request_url()});

  // Ensure that frames with sandbox flags don't interfere with the origin trial
  NavigateAndCheckHeaders(accept_ch_ua_iframe_sandbox_request_url(),
                          /*ch_ua_reduced_expected=*/GetParam() ==
                              UserAgentOriginTrialTestType::UAReduction,
                          /*ch_ua_exist_expected=*/true);

  CheckSecClientHintUaCount();

  // Make sure the last intercepted URL was the request for the embedded iframe.
  EXPECT_EQ(last_request_url().path(), "/simple.html");
}

IN_PROC_BROWSER_TEST_P(SameOriginUaOriginTrialBrowserTest,
                       IframeRequestUaWithValidOriginTrialTokenAndCriticalCH) {
  SetTestOptions(
      {/*has_ot_token=*/true, /*valid_ot_token=*/true,
       /*has_accept_ch_header=*/true, /*has_critical_ch_header=*/true},
      {critical_ch_ua_iframe_request_url(), simple_request_url()});

  // The last resource request processed for this navigation will be an embedded
  // iframe request. Since Accept-CH has either Sec-CH-UA-Reduced or
  // Sec-CH-UA-Full set on the top-level level frame's response header,
  // along with a valid origin trial token, the iframe request should send the
  // reduced UA string if Sec-CH-UA-Reduced set, or the full UA string if
  // Sec-CH-UA-Full set in the request header.
  NavigateAndCheckHeaders(critical_ch_ua_iframe_request_url(),
                          /*ch_ua_reduced_expected=*/GetParam() ==
                              UserAgentOriginTrialTestType::UAReduction,
                          /*ch_ua_exist_expected=*/true);

  CheckSecClientHintUaCount();

  // Make sure the last intercepted URL was the request for the embedded iframe.
  EXPECT_EQ(last_request_url().path(), "/simple.html");
}

IN_PROC_BROWSER_TEST_P(SameOriginUaOriginTrialBrowserTest,
                       SubresourceRequestUaWithValidOriginTrialToken) {
  SetTestOptions(
      {/*has_ot_token=*/true, /*valid_ot_token=*/true,
       /*has_accept_ch_header=*/true, /*has_critical_ch_header=*/false},
      {accept_ch_ua_subresource_request_url(), style_css_request_url()});

  // The last resource request processed for this navigation will be a
  // subresource request for the stylesheet.  Since Accept-CH has
  // either Sec-CH-UA-Reduced or Sec-CH-UA-Full set on the top-level
  // level frame's response header, along with a valid origin trial token, the
  // subresource request should send the reduced UA string if Sec-CH-UA-Reduced
  // set, or the full UA string if Sec-CH-UA-Full set in the request header.
  NavigateAndCheckHeaders(accept_ch_ua_subresource_request_url(),
                          /*ch_ua_reduced_expected=*/GetParam() ==
                              UserAgentOriginTrialTestType::UAReduction,
                          /*ch_ua_exist_expected=*/true);
  // Make sure the last intercepted URL was the subresource request for the
  // embedded stylesheet.
  EXPECT_EQ(last_request_url().path(), "/style.css");
}

IN_PROC_BROWSER_TEST_P(
    SameOriginUaOriginTrialBrowserTest,
    SubresourceRequestUaWithValidOriginTrialTokenAndCriticalCH) {
  SetTestOptions(
      {/*has_ot_token=*/true, /*valid_ot_token=*/true,
       /*has_accept_ch_header=*/true, /*has_critical_ch_header=*/true},
      {critical_ch_ua_subresource_request_url(), style_css_request_url()});

  // The last resource request processed for this navigation will be a
  // subresource request for the stylesheet.  Since Accept-CH has
  // either Sec-CH-UA-Reduced or Sec-CH-UA-Full set on the top-level level
  // frame's response header, along with a valid origin trial token, the
  // subresource request should send the reduced UA string Sec-CH-UA-Reduced
  // set, or the full UA string if Sec-CH-UA-Full set in the request header.
  NavigateAndCheckHeaders(critical_ch_ua_subresource_request_url(),
                          /*ch_ua_reduced_expected=*/GetParam() ==
                              UserAgentOriginTrialTestType::UAReduction,
                          /*ch_ua_exist_expected=*/true);
  // Make sure the last intercepted URL was the subresource request for the
  // embedded stylesheet.
  EXPECT_EQ(last_request_url().path(), "/style.css");
}

IN_PROC_BROWSER_TEST_P(SameOriginUaOriginTrialBrowserTest,
                       UserAgentOverrideAcceptChUa) {
  SetTestOptions(
      {/*has_ot_token=*/true, /*valid_ot_token=*/true,
       /*has_accept_ch_header=*/true, /*has_critical_ch_header=*/false},
      {ua_with_valid_origin_trial_token_url()});

  base::HistogramTester histograms;
  const std::string user_agent_override = "foo";
  SetUserAgentOverride(user_agent_override);

  const GURL url = ua_with_valid_origin_trial_token_url();
  // First navigation to set the client hints in the response.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // Second navigation has either Sec-CH-UA-Reduced or Sec-CH-UA-Full client
  // hint stored from the first navigation's response.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Since the UA override was set, the UA client hints are *not* added to the
  // request.
  CheckUaOriginTrialClientHint(/*ch_ua_expected=*/true);
  // Make sure the overridden UA string is the one sent.
  CheckUserAgentString(user_agent_override);

  // The UserAgentStringType enum is not accessible in //chrome/browser, so
  // we just use the enum's integer value.
  histograms.ExpectBucketCount("Navigation.UserAgentStringType",
                               /*NavigationRequest::kOverridden*/ 2, 2);
}

IN_PROC_BROWSER_TEST_P(SameOriginUaOriginTrialBrowserTest,
                       UserAgentOverrideSubresourceRequest) {
  SetTestOptions(
      {/*has_ot_token=*/true, /*valid_ot_token=*/true,
       /*has_accept_ch_header=*/true, /*has_critical_ch_header=*/false},
      {accept_ch_ua_subresource_request_url()});

  const std::string user_agent_override = "foo";
  SetUserAgentOverride(user_agent_override);

  const GURL url = accept_ch_ua_subresource_request_url();
  // First navigation to set the client hints in the response.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // Second navigation has either Sec-CH-UA-Reduced or Sec-CH-UA-Full client
  // hint stored from the first navigation's response.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Since the UA override was set, the UA client hints are *not* added to the
  // request.
  CheckUaOriginTrialClientHint(/*ch_ua_expected=*/true);
  // Make sure the overridden UA string is the one sent.
  CheckUserAgentString(user_agent_override);
}

IN_PROC_BROWSER_TEST_P(SameOriginUaOriginTrialBrowserTest,
                       UserAgentOverrideIframeRequest) {
  SetTestOptions(
      {/*has_ot_token=*/true, /*valid_ot_token=*/true,
       /*has_accept_ch_header=*/true, /*has_critical_ch_header=*/false},
      {accept_ch_ua_iframe_request_url()});

  const std::string user_agent_override = "foo";
  SetUserAgentOverride(user_agent_override);

  const GURL url = accept_ch_ua_iframe_request_url();
  // First navigation to set the client hints in the response.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // Second navigation has either Sec-CH-UA-Reduced or Sec-CH-UA-Full client
  // hint stored from the first navigation's response.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Since the UA override was set, the UA client hints are *not* added to the
  // request.
  CheckUaOriginTrialClientHint(/*ch_ua_expected=*/true);
  // Make sure the overridden UA string is the one sent.
  CheckUserAgentString(user_agent_override);
}

IN_PROC_BROWSER_TEST_P(SameOriginUaOriginTrialBrowserTest,
                       NoAcceptCHRemovesSecChUaFromStorage) {
  SetTestOptions(
      {/*has_ot_token=*/true, /*valid_ot_token=*/true,
       /*has_accept_ch_header=*/true, /*has_critical_ch_header=*/false},
      {ua_with_valid_origin_trial_token_url(), simple_request_url()});

  // The first navigation sets Sec-CH-UA-Reduced/Sec-CH-UA-Full in the client
  // hints storage for the origin.
  NavigateAndCheckHeaders(ua_with_valid_origin_trial_token_url(),
                          /*ch_ua_reduced_expected=*/UAReductionEnabled(),
                          /*ch_ua_exist_expected=*/false);
  // The second navigation doesn't contain an Accept-CH header in the
  // response, so Sec-CH-UA-Reduced/Sec-CH-UA-Full is removed from the storage.
  NavigateAndCheckHeaders(simple_request_url(),
                          /*ch_ua_reduced_expected=*/GetParam() ==
                              UserAgentOriginTrialTestType::UAReduction,
                          /*ch_ua_exist_expected=*/true);
  // The third navigation doesn't contain a Sec-CH-UA-Reduced/Sec-CH-UA-Full
  // in the request header because the second navigation caused it to get
  // removed.
  NavigateAndCheckHeaders(ua_with_valid_origin_trial_token_url(),
                          /*ch_ua_reduced_expected=*/UAReductionEnabled(),
                          /*ch_ua_exist_expected=*/false);
}

// Tests that the Sec-CH-UA-Reduced or Sec-CH-UA-Full client hint and the
// reduced User-Agent string are sent on request headers for third-party
// embedded resources if the Origin Trial token from the top-level frame is
// valid and the permissions policy allows it.
class ThirdPartyUaOriginTrialBrowserTest
    : public UaOriginTrialBrowserTest,
      public testing::WithParamInterface<UserAgentOriginTrialTestType> {
 public:
  ThirdPartyUaOriginTrialBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.ServeFilesFromSourceDirectory(
        "chrome/test/data/client_hints");
    https_server_.RegisterRequestMonitor(base::BindRepeating(
        &ThirdPartyUaOriginTrialBrowserTest::MonitorResourceRequest,
        base::Unretained(this)));
    EXPECT_TRUE(https_server_.Start());
  }

  // The URL that was used to register the Origin Trial token.
  static constexpr char kFirstPartyOriginUrl[] = "https://my-site.com:44444";

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer,
    // since the origin trial token in the response is associated with a fixed
    // origin, whereas EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindRepeating(
            &ThirdPartyUaOriginTrialBrowserTest::InterceptRequest,
            base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  GURL accept_ch_ua_cross_origin_iframe_request_url() const {
    return GURL(
        base::StrCat({kFirstPartyOriginUrl,
                      "/accept_ch_ua_cross_origin_iframe_request.html"}));
  }

  GURL accept_ch_ua_cross_origin_subresource_request_url() const {
    return GURL(
        base::StrCat({kFirstPartyOriginUrl,
                      "/accept_ch_ua_cross_origin_subresource_request.html"}));
  }

 protected:
  const absl::optional<std::string>& GetLastUserAgentHeaderValue() override {
    base::AutoLock lock(last_request_lock_);
    return last_user_agent_;
  }

  const absl::optional<std::string>& GetLastUaOriginTrialClientHintValue()
      override {
    base::AutoLock lock(last_request_lock_);
    return last_sec_ch_ua_value_;
  }

  const absl::optional<GURL>& GetLastRequestedURL() {
    base::AutoLock lock(last_request_lock_);
    return last_requested_url_;
  }

  void SetUaPermissionsPolicy(const std::string& value) {
    ua_permissions_policy_header_value_ = value;
  }

  void SetValidOTToken(const bool valid_ot_token) {
    valid_ot_token_ = valid_ot_token;
  }

  GURL GetServerOrigin() const { return https_server_.GetOrigin().GetURL(); }

 private:
  // URLLoaderInterceptor callback
  bool InterceptRequest(URLLoaderInterceptor::RequestParams* params) {
    if (params->url_request.url.DeprecatedGetOriginAsURL() !=
        GURL(kFirstPartyOriginUrl)) {
      return false;
    }
    if (params->url_request.url.path() !=
            base::StrCat({"/accept_ch_ua_cross_origin_iframe_request.html"}) &&
        params->url_request.url.path() !=
            base::StrCat(
                {"/accept_ch_ua_cross_origin_subresource_request.html"})) {
      return false;
    }

    // Generated by running (in tools/origin_trials):
    // generate_token.py https://my-site.com:44444 UserAgentReduction
    //   --expire-timestamp=2000000000
    //
    // The Origin Trial token expires in 2033.  Generate a new token by then, or
    // find a better way to re-generate a test trial token.
    static constexpr const char kOriginTrialTokenReduced[] =
        "AziP2Iyo74PHkJAVVXJ1NBAyZd+"
        "GZFmTqpFtug4Wazsj5rQPFeCFjjZpiEYb086vZzi48lF1ydynMj/"
        "oLqqLXgEAAABeeyJvcmlnaW4iOiAiaHR0cHM6Ly9teS1zaXRlLmNvbTo0NDQ0NCIsICJmZ"
        "WF0dXJlIjogIlVzZXJBZ2VudFJlZHVjdGlvbiIsICJleHBpcnkiOiAyMDAwMDAwMDAwfQ="
        "=";

    // Generated by running (in tools/origin_trials):
    // generate_token.py https://my-site.com:44444
    // SendFullUserAgentAfterReduction --expire-timestamp=2000000000
    //
    // The Origin Trial token expires in 2033.  Generate a new token by then,
    // or find a better way to re-generate a test trial token.
    static constexpr const char kOriginTrialTokenFull[] =
        "A/qRZSBJ/"
        "wuh1vOPO1X3x79VvjXlKiWldDIX0ra1iQf2FBB7yHPCQ3rEEHOc8S0cnWUG8as1k98sUyV"
        "xGawmmggAAABreyJvcmlnaW4iOiAiaHR0cHM6Ly9teS1zaXRlLmNvbTo0NDQ0NCIsICJmZ"
        "WF0dXJlIjogIlNlbmRGdWxsVXNlckFnZW50QWZ0ZXJSZWR1Y3Rpb24iLCAiZXhwaXJ5Ijo"
        "gMjAwMDAwMDAwMH0=";

    // Construct and send the response.
    std::string headers =
        "HTTP/1.1 200 OK\nContent-Type: text/html; charset=utf-8\n";
    base::StrAppend(&headers,
                    {"Accept-CH: ",
                     GetParam() == UserAgentOriginTrialTestType::UAReduction
                         ? "Sec-CH-UA-Reduced"
                         : "Sec-CH-UA-Full",
                     "\n"});
    base::StrAppend(&headers,
                    {"Permissions-Policy: ",
                     GetParam() == UserAgentOriginTrialTestType::UAReduction
                         ? "ch-ua-reduced="
                         : "ch-ua-full=",
                     ua_permissions_policy_header_value_, "\n"});

    base::StrAppend(
        &headers,
        {"Origin-Trial: ",
         valid_ot_token_
             ? (GetParam() == UserAgentOriginTrialTestType::UAReduction
                    ? kOriginTrialTokenReduced
                    : kOriginTrialTokenFull)
             : "invalid-origin-trial-token",
         "\n\n"});
    std::string body = "<html><head>";
    if (params->url_request.url.path() ==
        base::StrCat({"/accept_ch_ua_cross_origin_subresource_request.html"})) {
      base::StrAppend(&body, {BuildSubresourceHTML()});
    }
    base::StrAppend(&body, {"</head><body>"});
    if (params->url_request.url.path() ==
        base::StrCat({"/accept_ch_ua_cross_origin_iframe_request.html"})) {
      base::StrAppend(&body, {BuildIframeHTML()});
    }
    base::StrAppend(&body, {"</body></html>"});
    URLLoaderInterceptor::WriteResponse(headers, body, params->client.get());
    return true;
  }

  void SetLastUserAgent(const std::string* value) {
    base::AutoLock lock(last_request_lock_);
    if (value != nullptr) {
      last_user_agent_ = *value;
    } else {
      NOTREACHED();
    }
  }

  void SetLastSecChUaValue(const std::string* value) {
    base::AutoLock lock(last_request_lock_);
    if (value != nullptr) {
      last_sec_ch_ua_value_ = *value;
    } else {
      last_sec_ch_ua_value_ = absl::nullopt;
    }
  }

  void SetLastRequestedURL(const GURL& url) {
    base::AutoLock lock(last_request_lock_);
    last_requested_url_ = url;
  }

  // Called by `https_server_`.
  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    SetLastRequestedURL(request.GetURL());
    auto it = request.headers.find("user-agent");
    SetLastUserAgent(it != request.headers.end() ? &it->second : nullptr);
    it = request.headers.find(
        base::StrCat({GetParam() == UserAgentOriginTrialTestType::UAReduction
                          ? "sec-ch-ua-reduced"
                          : "sec-ch-ua-full"}));
    SetLastSecChUaValue(it != request.headers.end() ? &it->second : nullptr);
  }

  std::string BuildIframeHTML() {
    std::string html = "<iframe src=\"";
    base::StrAppend(
        &html, {https_server_.GetURL("/simple.html").spec(), "\"></iframe>"});
    return html;
  }

  std::string BuildSubresourceHTML() {
    std::string html = "<link rel=\"stylesheet\" href=\"";
    base::StrAppend(&html,
                    {https_server_.GetURL("/style.css").spec(), "\"></link>"});
    return html;
  }

  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
  net::EmbeddedTestServer https_server_;
  std::string ua_permissions_policy_header_value_;
  bool valid_ot_token_ = true;
  base::Lock last_request_lock_;
  absl::optional<std::string> last_user_agent_ GUARDED_BY(last_request_lock_);
  absl::optional<std::string> last_sec_ch_ua_value_
      GUARDED_BY(last_request_lock_);
  absl::optional<GURL> last_requested_url_ GUARDED_BY(last_request_lock_);
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ThirdPartyUaOriginTrialBrowserTest,
    testing::Values(UserAgentOriginTrialTestType::UAReduction,
                    UserAgentOriginTrialTestType::UADeprecation,
                    UserAgentOriginTrialTestType::UAReductionAndDeprecation));

constexpr const char ThirdPartyUaOriginTrialBrowserTest::kFirstPartyOriginUrl[];

IN_PROC_BROWSER_TEST_P(ThirdPartyUaOriginTrialBrowserTest,
                       ThirdPartyIframeUaWildcardPolicy) {
  SetUaPermissionsPolicy("*");  // Allow all third-party sites.

  NavigateAndCheckHeaders(accept_ch_ua_cross_origin_iframe_request_url(),
                          /*ch_ua_reduced_expected=*/GetParam() ==
                              UserAgentOriginTrialTestType::UAReduction,
                          /*ch_ua_exist_expected=*/true);

  // Make sure the last intercepted URL was the request for the embedded
  // iframe.
  EXPECT_EQ(GetLastRequestedURL()->path(), "/simple.html");
}

// Tests that headers are not sent to a third-party iframe after script is
// disabled with content settings.
IN_PROC_BROWSER_TEST_P(ThirdPartyUaOriginTrialBrowserTest, ScriptDisabled) {
  SetUaPermissionsPolicy("*");
  const GURL url = accept_ch_ua_cross_origin_iframe_request_url();
  NavigateAndCheckHeaders(url,
                          /*ch_ua_reduced_expected=*/GetParam() ==
                              UserAgentOriginTrialTestType::UAReduction,
                          /*ch_ua_exist_expected=*/true);

  // Disable script for first party origin.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingCustomScope(
          ContentSettingsPattern::FromURL(GURL(kFirstPartyOriginUrl)),
          ContentSettingsPattern::Wildcard(), ContentSettingsType::JAVASCRIPT,
          CONTENT_SETTING_BLOCK);
  // Headers should not be sent in third party iframe.
  NavigateAndCheckHeaders(url,
                          /*ch_ua_reduced_expected=*/UAReductionEnabled(),
                          /*ch_ua_exist_expected=*/false);

  // Make sure the last intercepted URL was the request for the embedded
  // iframe.
  EXPECT_EQ(GetLastRequestedURL()->path(), "/simple.html");
}

IN_PROC_BROWSER_TEST_P(ThirdPartyUaOriginTrialBrowserTest,
                       ThirdPartySubresourceUaWithWildcardPolicy) {
  SetUaPermissionsPolicy("*");  // Allow all third-party sites.

  NavigateAndCheckHeaders(accept_ch_ua_cross_origin_subresource_request_url(),
                          /*ch_ua_reduced_expected=*/GetParam() ==
                              UserAgentOriginTrialTestType::UAReduction,
                          /*ch_ua_exist_expected=*/true);

  // Make sure the last intercepted URL was the request for the embedded
  // iframe.
  EXPECT_EQ(GetLastRequestedURL()->path(), "/style.css");
}

IN_PROC_BROWSER_TEST_P(ThirdPartyUaOriginTrialBrowserTest,
                       ThirdPartyIframeUaSpecificPolicy) {
  std::string policy = "(self \"";
  base::StrAppend(&policy, {GetServerOrigin().spec(), "\")"});
  SetUaPermissionsPolicy(policy);  // Allow our third-party site only.

  NavigateAndCheckHeaders(accept_ch_ua_cross_origin_iframe_request_url(),
                          /*ch_ua_reduced_expected=*/GetParam() ==
                              UserAgentOriginTrialTestType::UAReduction,
                          /*ch_ua_exist_expected=*/true);

  // Make sure the last intercepted URL was the request for the embedded
  // iframe.
  EXPECT_EQ(GetLastRequestedURL()->path(), "/simple.html");
}

IN_PROC_BROWSER_TEST_P(ThirdPartyUaOriginTrialBrowserTest,
                       ThirdPartySubresourceUaSpecificPolicy) {
  std::string policy = "(self \"";
  base::StrAppend(&policy, {GetServerOrigin().spec(), "\")"});
  SetUaPermissionsPolicy(policy);  // Allow our third-party site only.

  NavigateAndCheckHeaders(accept_ch_ua_cross_origin_subresource_request_url(),
                          /*ch_ua_reduced_expected=*/GetParam() ==
                              UserAgentOriginTrialTestType::UAReduction,
                          /*ch_ua_exist_expected=*/true);

  // Make sure the last intercepted URL was the request for the embedded
  // iframe.
  EXPECT_EQ(GetLastRequestedURL()->path(), "/style.css");
}

IN_PROC_BROWSER_TEST_P(ThirdPartyUaOriginTrialBrowserTest,
                       ThirdPartyIframeUaInvalidOriginTrialToken) {
  SetUaPermissionsPolicy("*");  // Allow all third-party sites.
  SetValidOTToken(false);       // Origin Trial Token is invalid.

  NavigateAndCheckHeaders(accept_ch_ua_cross_origin_iframe_request_url(),
                          /*ch_ua_reduced_expected=*/UAReductionEnabled(),
                          /*ch_ua_exist_expected=*/false);

  // Make sure the last intercepted URL was the request for the embedded
  // iframe.
  EXPECT_EQ(GetLastRequestedURL()->path(), "/simple.html");
}

IN_PROC_BROWSER_TEST_P(ThirdPartyUaOriginTrialBrowserTest,
                       ThirdPartySubresourceUaInvalidOriginTrialToken) {
  SetUaPermissionsPolicy("*");  // Allow all third-party sites.
  SetValidOTToken(false);       // Origin Trial Token is invalid.

  NavigateAndCheckHeaders(accept_ch_ua_cross_origin_subresource_request_url(),
                          /*ch_ua_reduced_expected=*/UAReductionEnabled(),
                          /*ch_ua_exist_expected=*/false);

  // Make sure the last intercepted URL was the request for the embedded
  // iframe.
  EXPECT_EQ(GetLastRequestedURL()->path(), "/style.css");
}

// A test fixture for setting Accept-CH and Origin-Trial response headers for
// third-party embeds.  This suite of tests ensures that third-party embeds
// with Sec-CH-UA-Reduced or Sec-CH-UA-Full and a valid Origin Trial token send
// the reduced UA string if Sec-CH-UA-Reduced set, or the full UA string if
// Sec-CH-UA-Full set in the request header, even if the top-level page is
// not enrolled in the UA reduction origin trial.
//
// The Origin Trial token for UserAgentReduction in the header files were
// generated by running (in tools/origin_trials): generate_token.py
// https://my-site.com:44444 UserAgentReduction
//   --is-third-party --expire-timestamp=2000000000
//
// The Origin Trial token for SendFullUserAgentAfterReduction in the header
// files were generated by running (in tools/origin_trials): generate_token.py
// https://my-site.com:44444 SendFullUserAgentAfterReduction
//   --is-third-party --expire-timestamp=2000000000
class ThirdPartyAcceptChUaOriginTrialBrowserTest
    : public UaOriginTrialBrowserTest,
      public testing::WithParamInterface<UserAgentOriginTrialTestType> {
 public:
  ThirdPartyAcceptChUaOriginTrialBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.ServeFilesFromSourceDirectory(
        "chrome/test/data/client_hints");
    https_server_.RegisterRequestMonitor(base::BindRepeating(
        &ThirdPartyAcceptChUaOriginTrialBrowserTest::MonitorRequest,
        base::Unretained(this)));
    EXPECT_TRUE(https_server_.Start());
  }

  // The URL that was used to register the Origin Trial token.
  static constexpr char kThirdPartyOriginUrl[] = "https://my-site.com:44444";

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, for
    // the third-party requests, since the origin trial token in the response
    // is associated with a fixed origin, whereas EmbeddedTestServer serves
    // content on a random port.
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindRepeating(
            &ThirdPartyAcceptChUaOriginTrialBrowserTest::InterceptRequest,
            base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  GURL accept_ch_ua_cross_origin_iframe_request_url() const {
    return https_server_.GetURL(base::StrCat(
        {"/accept_ch_ua_cross_origin_iframe_with_ot_request.html"}));
  }

  GURL accept_ch_ua_cross_origin_iframe_with_subrequests_url() const {
    return https_server_.GetURL(base::StrCat(
        {"/accept_ch_ua_cross_origin_iframe_with_subrequests.html"}));
  }

  GURL top_level_with_iframe_redirect_url() const {
    return https_server_.GetURL(
        base::StrCat({"/top_level_with_iframe_redirect.html"}));
  }

 protected:
  const absl::optional<std::string>& GetLastUserAgentHeaderValue() override {
    base::AutoLock lock(last_request_lock_);
    return last_user_agent_;
  }

  const absl::optional<std::string>& GetLastUaOriginTrialClientHintValue()
      override {
    base::AutoLock lock(last_request_lock_);
    return last_sec_ch_ua_vaule_;
  }

  const absl::optional<GURL>& GetLastRequestedURL() {
    base::AutoLock lock(last_request_lock_);
    return last_requested_url_;
  }

 private:
  // URLLoaderInterceptor callback.  Handles the third-party embeds and
  // subresource requests.
  bool InterceptRequest(URLLoaderInterceptor::RequestParams* params) {
    const GURL origin = params->url_request.url.DeprecatedGetOriginAsURL();
    // The interceptor does not handle requests to the EmbeddedTestServer.
    // Requests also get sent to https://accounts.google.com/ (not sure from
    // where), so we ignore them as well.
    if (origin == https_server_.base_url() ||
        origin == GURL("https://accounts.google.com/")) {
      return false;
    }

    // Filter out unknown paths to avoid flaky tests.
    static constexpr auto kExpectedPaths =
        base::MakeFixedFlatSet<base::StringPiece>({
            "/basic.html",
            "/frame_3p_ot.html",
            "/nested_style.css",
            "/redirect_style.css",
            "/simple_3p_ot.html",
            "/style.css",
            "/subresource_redirect.html",
        });
    const std::string path = params->url_request.url.path();
    if (!base::Contains(kExpectedPaths, path)) {
      return false;
    }

    SetLastRequestedURL(params->url_request.url);
    std::string user_agent;
    params->url_request.headers.GetHeader("user-agent", &user_agent);
    SetLastUserAgent(&user_agent);
    std::string sec_ch_ua_value;
    params->url_request.headers.GetHeader(
        base::StrCat({GetParam() == UserAgentOriginTrialTestType::UAReduction
                          ? "sec-ch-ua-reduced"
                          : "sec-ch-ua-full"}),
        &sec_ch_ua_value);
    SetLastSecChUaValue(&sec_ch_ua_value);

    if (path == "/style.css" || path == "/basic.html" ||
        path == "/nested_style.css") {
      // These paths are known to be the last request (with no subrequest
      // after them), so verify that the UA string is set appropriately.
      const bool ch_ua_reduced_expected =
          GetParam() == UserAgentOriginTrialTestType::UAReduction;
      const bool ch_ua_exist_expected = true;
      CheckUaOriginTrialClientHint(ch_ua_exist_expected);
      CheckUserAgentReduced(ch_ua_reduced_expected, false);
    }

    std::string resource_path = "chrome/test/data/client_hints";
    resource_path.append(
        static_cast<std::string>(params->url_request.url.path_piece()));

    // Only build mock header with origin trial tokens for the third party
    // requests.
    if (origin != GURL(kThirdPartyOriginUrl)) {
      URLLoaderInterceptor::WriteResponse(resource_path, params->client.get());
      return true;
    }

    // Generated by running (in tools/origin_trials):
    // generate_token.py https://my-site.com:44444 UserAgentReduction
    // --is-third-party --expire-timestamp=2000000000
    //
    // The Origin Trial token expires in 2033.  Generate a new token by then, or
    // find a better way to re-generate a test trial token.
    static constexpr const char kOriginTrialTokenReduced[] =
        "Awgc/"
        "axBbE+4mDB+z2AKFEl26TUKBzCM2GBkDQmt4IephJgHpel1kcsIdCCBYKUgJ4s4+"
        "JQLXFKkOCs7lFIISAMAAAB0eyJvcmlnaW4iOiAiaHR0cHM6Ly9teS1zaXRlLmNvbTo0NDQ"
        "0NCIsICJpc1RoaXJkUGFydHkiOiB0cnVlLCAiZmVhdHVyZSI6ICJVc2VyQWdlbnRSZWR1Y"
        "3Rpb24iLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0=";

    // Generated by running (in tools/origin_trials):
    // generate_token.py https://my-site.com:44444
    // SendFullUserAgentAfterReduction --is-third-party
    // --expire-timestamp=2000000000
    //
    // The Origin Trial token expires in 2033.  Generate a new token by then,
    // or find a better way to re-generate a test trial token.
    static constexpr const char kOriginTrialTokenFull[] =
        "AznwSelRzlbEO7T3NXT68fp+"
        "k7amzJdYhxfcUEH3M7WTMES73QlwoqK8zBNVd1rGDvFuDxDbDILL4pr7Og6wJw0AAACBey"
        "JvcmlnaW4iOiAiaHR0cHM6Ly9teS1zaXRlLmNvbTo0NDQ0NCIsICJpc1RoaXJkUGFydHki"
        "OiB0cnVlLCAiZmVhdHVyZSI6ICJTZW5kRnVsbFVzZXJBZ2VudEFmdGVyUmVkdWN0aW9uIi"
        "wgImV4cGlyeSI6IDIwMDAwMDAwMDB9";

    std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n";
    switch (GetParam()) {
      case UserAgentOriginTrialTestType::UAReduction:
        base::StrAppend(&headers, {"Accept-CH: ", "Sec-CH-UA-Reduced", "\n"});
        base::StrAppend(&headers, {"Origin-Trial: ", kOriginTrialTokenReduced});
        break;
      case UserAgentOriginTrialTestType::UADeprecation:
        base::StrAppend(&headers, {"Accept-CH: ", "Sec-CH-UA-Full", "\n"});
        base::StrAppend(&headers, {"Origin-Trial: ", kOriginTrialTokenFull});
        break;
      case UserAgentOriginTrialTestType::UAReductionAndDeprecation:
        base::StrAppend(
            &headers,
            {"Accept-CH: ", "Sec-CH-UA-Reduced, Sec-CH-UA-Full", "\n"});
        base::StrAppend(&headers, {"Origin-Trial: ", kOriginTrialTokenReduced,
                                   ",", kOriginTrialTokenFull});
        break;
      default:
        break;
    }
    URLLoaderInterceptor::WriteResponse(resource_path, params->client.get(),
                                        &headers);
    return true;
  }

  // Called by `https_server_`.
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    // All first party requests don't respond with a valid Origin Trial token,
    // Reduced UA string or not is controlled by kReduceUserAgentMinorVersion.
    CheckUserAgentMinorVersion(
        request.headers.at("user-agent"),
        /*expected_user_agent_reduced=*/UAReductionEnabled(), true);
    std::string sec_ua_ch_name =
        base::StrCat({GetParam() == UserAgentOriginTrialTestType::UAReduction
                          ? "sec-ch-ua-reduced"
                          : "sec-ch-ua-full"});
    request.headers.find(sec_ua_ch_name);
    EXPECT_THAT(request.headers, Not(Contains(Key(sec_ua_ch_name))));
  }

  void SetLastUserAgent(const std::string* value) {
    base::AutoLock lock(last_request_lock_);
    if (value != nullptr) {
      last_user_agent_ = *value;
    } else {
      NOTREACHED();
    }
  }

  void SetLastSecChUaValue(const std::string* value) {
    base::AutoLock lock(last_request_lock_);
    if (value != nullptr && !value->empty()) {
      last_sec_ch_ua_vaule_ = *value;
    } else {
      last_sec_ch_ua_vaule_ = absl::nullopt;
    }
  }

  void SetLastRequestedURL(const GURL& url) {
    base::AutoLock lock(last_request_lock_);
    last_requested_url_ = url;
  }

  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
  net::EmbeddedTestServer https_server_;
  base::Lock last_request_lock_;
  absl::optional<std::string> last_user_agent_ GUARDED_BY(last_request_lock_);
  absl::optional<std::string> last_sec_ch_ua_vaule_
      GUARDED_BY(last_request_lock_);
  absl::optional<GURL> last_requested_url_ GUARDED_BY(last_request_lock_);
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ThirdPartyAcceptChUaOriginTrialBrowserTest,
    testing::Values(UserAgentOriginTrialTestType::UAReduction,
                    UserAgentOriginTrialTestType::UADeprecation,
                    UserAgentOriginTrialTestType::UAReductionAndDeprecation));

constexpr char
    ThirdPartyAcceptChUaOriginTrialBrowserTest::kThirdPartyOriginUrl[];

IN_PROC_BROWSER_TEST_P(ThirdPartyAcceptChUaOriginTrialBrowserTest,
                       ThirdPartyIframeUaWithOriginTrialToken) {
  const GURL top_level_frame_url =
      accept_ch_ua_cross_origin_iframe_request_url();
  // The first navigation is to opt-into the OT.
  NavigateAndCheckHeaders(top_level_frame_url,
                          /*ch_ua_reduced_expected=*/UAReductionEnabled(),
                          /*ch_ua_exist_expected=*/false);
  NavigateAndCheckHeaders(top_level_frame_url,
                          /*ch_ua_reduced_expected=*/GetParam() ==
                              UserAgentOriginTrialTestType::UAReduction,
                          /*ch_ua_exist_expected=*/true);
  // Make sure the last intercepted URL was the request for the embedded
  // iframe.
  EXPECT_EQ(GetLastRequestedURL()->path(),
            base::StrCat({"/simple_3p_ot.html"}));
}

IN_PROC_BROWSER_TEST_P(ThirdPartyAcceptChUaOriginTrialBrowserTest,
                       ThirdPartyIframeUaWithAllCookiesBlocked) {
  const GURL top_level_frame_url =
      accept_ch_ua_cross_origin_iframe_request_url();
  const GURL third_party_iframe_url = GURL(kThirdPartyOriginUrl);

  // Block all cookies for the third-party origin.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(third_party_iframe_url, GURL(),
                                      ContentSettingsType::COOKIES,
                                      CONTENT_SETTING_BLOCK);

  // The first navigation is to attempt to opt-into the OT for the third-party
  // embed, which shouldn't happen for this test because third-party cookies
  // are blocked.
  NavigateAndCheckHeaders(top_level_frame_url,
                          /*ch_ua_reduced_expected=*/UAReductionEnabled(),
                          /*ch_ua_exist_expected=*/false);
  NavigateAndCheckHeaders(top_level_frame_url,
                          /*ch_ua_reduced_expected=*/UAReductionEnabled(),
                          /*ch_ua_exist_expected=*/false);
  // Make sure the last intercepted URL was the request for the embedded
  // iframe.
  EXPECT_EQ(GetLastRequestedURL()->path(),
            base::StrCat({"/simple_3p_ot.html"}));
}

IN_PROC_BROWSER_TEST_P(ThirdPartyAcceptChUaOriginTrialBrowserTest,
                       ThirdPartyIframeUaWithThirdPartyCookiesBlocked) {
  // Block third-party cookies.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));

  // The first navigation is to attempt to opt-into the OT for the third-party
  // embed, which shouldn't happen for this test because cookies are blocked.
  NavigateAndCheckHeaders(accept_ch_ua_cross_origin_iframe_request_url(),
                          /*ch_ua_reduced_expected=*/UAReductionEnabled(),
                          /*ch_ua_exist_expected=*/false);
  NavigateAndCheckHeaders(accept_ch_ua_cross_origin_iframe_request_url(),
                          /*ch_ua_reduced_expected=*/UAReductionEnabled(),
                          /*ch_ua_exist_expected=*/false);
  // Make sure the last intercepted URL was the request for the embedded
  // iframe.
  EXPECT_EQ(GetLastRequestedURL()->path(),
            base::StrCat({"/simple_3p_ot.html"}));
}

IN_PROC_BROWSER_TEST_P(ThirdPartyAcceptChUaOriginTrialBrowserTest,
                       ThirdPartyIframeUaWithSubresourceRequests) {
  // The first navigation is to opt-into the OT.  Since there are subresource
  // requests, the last processed requests from the first navigation will have
  // the corresponding reduced or full UA string.
  NavigateAndCheckHeaders(
      accept_ch_ua_cross_origin_iframe_with_subrequests_url(),
      /*ch_ua_reduced_expected=*/GetParam() ==
          UserAgentOriginTrialTestType::UAReduction,
      /*ch_ua_exist_expected=*/true);
  NavigateAndCheckHeaders(
      accept_ch_ua_cross_origin_iframe_with_subrequests_url(),
      /*ch_ua_reduced_expected=*/GetParam() ==
          UserAgentOriginTrialTestType::UAReduction,
      /*ch_ua_exist_expected=*/true);
}

IN_PROC_BROWSER_TEST_P(ThirdPartyAcceptChUaOriginTrialBrowserTest,
                       ThirdPartyIframeUaWithSubresourceRedirectRequests) {
  // The first navigation is to opt-into the OT.  Since there are subresource
  // requests, the last processed requests from the first navigation will have
  // the corresponding reduced or full UA string.
  NavigateAndCheckHeaders(top_level_with_iframe_redirect_url(),
                          /*ch_ua_reduced_expected=*/GetParam() ==
                              UserAgentOriginTrialTestType::UAReduction,
                          /*ch_ua_exist_expected=*/true);
  NavigateAndCheckHeaders(top_level_with_iframe_redirect_url(),
                          /*ch_ua_reduced_expected=*/GetParam() ==
                              UserAgentOriginTrialTestType::UAReduction,
                          /*ch_ua_exist_expected=*/true);
}

// CrOS multi-profiles implementation is too different for these tests.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

void ClientHintsBrowserTest::TestSwitchWithNewProfile(
    const std::string& switch_value,
    size_t origins_stored) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      client_hints::switches::kInitializeClientHintsStorage, switch_value);

  Profile* profile = GenerateNewProfile();
  Browser* browser = CreateBrowser(profile);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, without_accept_ch_url()));

  ContentSettingsForOneType host_settings;

  // Clients hints preferences for one origin should be persisted.
  HostContentSettingsMapFactory::GetForProfile(profile)->GetSettingsForOneType(
      ContentSettingsType::CLIENT_HINTS, &host_settings);
  EXPECT_EQ(origins_stored, host_settings.size());
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, SwitchAppliesStorage) {
  TestSwitchWithNewProfile(
      "{\"https://a.test\":\"Sec-CH-UA-Full-Version\", "
      "\"https://b.test\":\"Sec-CH-UA-Full-Version\"}",
      2);
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, SwitchNotJson) {
  TestSwitchWithNewProfile("foo", 0);
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, SwitchOriginNotSecure) {
  TestSwitchWithNewProfile("{\"http://a.test\":\"Sec-CH-UA-Full-Version\"}", 0);
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, SwitchAcceptCHInvalid) {
  TestSwitchWithNewProfile("{\"https://a.test\":\"Not Valid\"}", 0);
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, SwitchAppliesStorageOneOrigin) {
  TestSwitchWithNewProfile(
      "{\"https://a.test\":\"Sec-CH-UA-Full-Version\", "
      "\"https://b.test\":\"Not Valid\"}",
      1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

class ClientHintsCommandLineSwitchBrowserTest : public ClientHintsBrowserTest {
 public:
  ClientHintsCommandLineSwitchBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* cmd) override {
    ClientHintsBrowserTest::SetUpCommandLine(cmd);
    std::string server_origin =
        url::Origin::Create(accept_ch_url()).Serialize();

    std::vector<std::string> accept_ch_tokens;
    for (const auto& pair : network::GetClientHintToNameMap())
      accept_ch_tokens.push_back(pair.second);

    cmd->AppendSwitchASCII(
        client_hints::switches::kInitializeClientHintsStorage,
        base::StringPrintf("{\"%s\":\"%s\"}", server_origin.c_str(),
                           base::JoinString(accept_ch_tokens, ",").c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(ClientHintsCommandLineSwitchBrowserTest,
                       NavigationToDifferentOrigins) {
  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), without_accept_ch_url()));

  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           without_accept_ch_cross_origin()));
}

IN_PROC_BROWSER_TEST_F(ClientHintsCommandLineSwitchBrowserTest,
                       ClearHintsWithAcceptCH) {
  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), without_accept_ch_url()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), accept_ch_empty()));

  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), without_accept_ch_url()));
}

IN_PROC_BROWSER_TEST_F(ClientHintsCommandLineSwitchBrowserTest,
                       StorageNotPresentInOffTheRecordProfile) {
  SetClientHintExpectationsOnMainFrame(true);
  SetClientHintExpectationsOnSubresources(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), without_accept_ch_url()));

  // OTR profile should get neither.
  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());
  SetClientHintExpectationsOnMainFrame(false);
  SetClientHintExpectationsOnSubresources(false);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(otr_browser, without_accept_ch_url()));
}

// Validate that the updated GREASE algorithm is used by default. The continued
// support of the old algorithm (which used only semicolon and space) is tested
// separately below. That functionality will be maintained for a period of time
// until we are confident in more permutations generated by the updated
// algorithm.
IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, UpdatedGREASEByDefault) {
  const GURL gurl = accept_ch_url();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  std::string ua_ch_result = main_frame_ua_observed();

  ASSERT_TRUE(SawUpdatedGrease(ua_ch_result) && !SawOldGrease(ua_ch_result));
}

class GreaseFeatureParamOptOutTest : public ClientHintsBrowserTest {
  // Test that feature param opt outs are able to trigger the old algorithm,
  // which we will maintain until additional confidence in the permutations of
  // the new algorithm is attained.
  std::unique_ptr<base::FeatureList> EnabledFeatures() override {
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->InitializeFromCommandLine(
        "GreaseUACH:updated_algorithm/false", "");
    return feature_list;
  }
};

// Checks that the updated GREASE algorithm is not used when explicitly
// disabled.
IN_PROC_BROWSER_TEST_F(GreaseFeatureParamOptOutTest,
                       UpdatedGreaseFeatureParamOptOutTest) {
  const GURL gurl = accept_ch_url();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  std::string ua_ch_result = main_frame_ua_observed();

  ASSERT_TRUE(SawOldGrease(ua_ch_result));
}

class GreaseEnterprisePolicyTest : public ClientHintsBrowserTest {
  void SetUpInProcessBrowserTestFixture() override {
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();
    policy::PolicyMap policies;
    SetPolicy(&policies, policy::key::kUserAgentClientHintsGREASEUpdateEnabled,
              absl::optional<base::Value>(false));
    provider_.UpdateChromePolicy(policies);
  }
};

// Makes sure that the enterprise policy is able to prevent updated GREASE.
IN_PROC_BROWSER_TEST_F(GreaseEnterprisePolicyTest, GreaseEnterprisePolicyTest) {
  const GURL gurl = accept_ch_url();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  std::string ua_ch_result = main_frame_ua_observed();

  ASSERT_TRUE(SawOldGrease(ua_ch_result));
}
IN_PROC_BROWSER_TEST_F(GreaseEnterprisePolicyTest,
                       GreaseEnterprisePolicyDynamicRefreshTest) {
  const GURL gurl = accept_ch_url();
  // Reset the policy that was already set to false in the setup, then see if
  // the change is reflected in the sec-ch-ua header without requiring a
  // browser restart.
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kUserAgentClientHintsGREASEUpdateEnabled,
            absl::optional<base::Value>(true));
  provider_.UpdateChromePolicy(policies);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  std::string ua_ch_result = main_frame_ua_observed();

  ASSERT_TRUE(SawUpdatedGrease(ua_ch_result) && !SawOldGrease(ua_ch_result));
}

// Tests that the Sec-CH-UA-Reduced client hint gets cleared on a redirect if
// the response doesn't contain the hint in the Accept-CH header.
class RedirectUaReducedOriginTrialBrowserTest : public InProcessBrowserTest {
 public:
  RedirectUaReducedOriginTrialBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_.ServeFilesFromSourceDirectory(
        "chrome/test/data/client_hints");
    https_server_.RegisterRequestMonitor(base::BindRepeating(
        &RedirectUaReducedOriginTrialBrowserTest::MonitorResourceRequest,
        base::Unretained(this)));
    EXPECT_TRUE(https_server_.Start());
  }

  static constexpr char kRedirectUrl[] = "https://my-site:44444";

  void SetUpCommandLine(base::CommandLine* cmd) override {
    InProcessBrowserTest::SetUpCommandLine(cmd);
    // Store Sec-CH-UA-Reduced in the Accept-CH cache for the server origin on
    // browser startup.
    cmd->AppendSwitchASCII(
        client_hints::switches::kInitializeClientHintsStorage,
        base::StringPrintf("{\"%s\":\"%s\"}",
                           https_server_.GetOrigin().Serialize().c_str(),
                           "Sec-CH-UA-Reduced"));
  }

  void SetUpOnMainThread() override {
    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer,
    // since the origin trial token in the response is associated with a fixed
    // origin, whereas EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindRepeating(
            &RedirectUaReducedOriginTrialBrowserTest::InterceptURLRequest,
            base::Unretained(this)));

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  GURL accept_ch_url() const { return https_server_.GetURL("/accept_ch.html"); }

  GURL redirect_url() const { return https_server_.GetURL("/redirect.html"); }

  std::string last_url() const { return last_url_; }

  std::string last_user_agent() const { return last_user_agent_; }

  absl::optional<std::string> last_ua_reduced_ch() const {
    return last_ua_reduced_ch_;
  }

 private:
  bool InterceptURLRequest(URLLoaderInterceptor::RequestParams* params) {
    if (url::Origin::Create(params->url_request.url) !=
        url::Origin::Create(GURL(kRedirectUrl))) {
      return false;
    }

    std::string resource_path = "chrome/test/data/client_hints";
    resource_path.append(
        static_cast<std::string>(params->url_request.url.path_piece()));
    URLLoaderInterceptor::WriteResponse(resource_path, params->client.get());
    return true;
  }

  void MonitorResourceRequest(const net::test_server::HttpRequest& request) {
    last_url_.clear();
    last_user_agent_.clear();
    last_ua_reduced_ch_.reset();

    last_url_ = request.GetURL().spec();
    last_user_agent_ = request.headers.at("user-agent");
    std::string ch_ua_reduced;
    if (request.headers.find("sec-ch-ua-reduced") != request.headers.end()) {
      last_ua_reduced_ch_ = request.headers.at("sec-ch-ua-reduced");
    }
  }

  net::EmbeddedTestServer https_server_;
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
  std::string last_url_;
  std::string last_user_agent_;
  absl::optional<std::string> last_ua_reduced_ch_;
};

constexpr char RedirectUaReducedOriginTrialBrowserTest::kRedirectUrl[];

IN_PROC_BROWSER_TEST_F(RedirectUaReducedOriginTrialBrowserTest,
                       AcceptChUaReducedWithValidOriginTrialToken) {
  // The first request sends SEc-CH-UA-Reduced and the reduced UA string, but
  // redirects to a different origin.  Since the response did not contain a
  // valid origin trial token, Sec-CH-UA-Reduced should be removed from the
  // Accept-CH cache.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), redirect_url()));
  EXPECT_EQ(last_url(), redirect_url());
  CheckUserAgentMinorVersion(last_user_agent(),
                             /*expected_user_agent_reduced=*/true, false);
  EXPECT_THAT(last_ua_reduced_ch(), Optional(Eq("?1")));

  // The next request to the origin should not send Sec-CH-UA-Reduced.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), accept_ch_url()));
  EXPECT_EQ(last_url(), accept_ch_url());
  CheckUserAgentMinorVersion(last_user_agent(),
                             /*expected_user_agent_reduced=*/
                             base::FeatureList::IsEnabled(
                                 blink::features::kReduceUserAgentMinorVersion),
                             true);
  EXPECT_THAT(last_ua_reduced_ch(), Eq(absl::nullopt));
}

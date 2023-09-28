// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_test_util.h"

#include <cmath>
#include <memory>
#include <utility>

#include "net/base/request_priority.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace {

bool AlmostEqual(const double x, const double y) {
  return std::fabs(x - y) <= 1e-6;  // Arbitrary but close enough.
}

}  // namespace

namespace predictors {

MockResourcePrefetchPredictor::MockResourcePrefetchPredictor(
    const LoadingPredictorConfig& config,
    Profile* profile)
    : ResourcePrefetchPredictor(config, profile) {}

MockResourcePrefetchPredictor::~MockResourcePrefetchPredictor() = default;

void InitializeRedirectStat(RedirectStat* redirect,
                            const GURL& url,
                            int number_of_hits,
                            int number_of_misses,
                            int consecutive_misses,
                            bool include_scheme,
                            bool include_port) {
  redirect->set_url(url.host());
  if (include_scheme)
    redirect->set_url_scheme(url.scheme());
  if (include_port)
    redirect->set_url_port(url.EffectiveIntPort());
  redirect->set_number_of_hits(number_of_hits);
  redirect->set_number_of_misses(number_of_misses);
  redirect->set_consecutive_misses(consecutive_misses);
}

void InitializeOriginStat(OriginStat* origin_stat,
                          const std::string& origin,
                          int number_of_hits,
                          int number_of_misses,
                          int consecutive_misses,
                          double average_position,
                          bool always_access_network,
                          bool accessed_network) {
  origin_stat->set_origin(origin);
  origin_stat->set_number_of_hits(number_of_hits);
  origin_stat->set_number_of_misses(number_of_misses);
  origin_stat->set_consecutive_misses(consecutive_misses);
  origin_stat->set_average_position(average_position);
  origin_stat->set_always_access_network(always_access_network);
  origin_stat->set_accessed_network(accessed_network);
}

RedirectData CreateRedirectData(const std::string& primary_key,
                                uint64_t last_visit_time) {
  RedirectData data;
  data.set_primary_key(primary_key);
  data.set_last_visit_time(last_visit_time);
  return data;
}

OriginData CreateOriginData(const std::string& host, uint64_t last_visit_time) {
  // |host| should not contain the scheme.
  EXPECT_EQ(std::string::npos, host.find("https://"));
  EXPECT_EQ(std::string::npos, host.find("http://"));
  OriginData data;
  data.set_host(host);
  data.set_last_visit_time(last_visit_time);
  return data;
}

LcppData CreateLcppData(const std::string& host, uint64_t last_visit_time) {
  // |host| should not contain the scheme.
  EXPECT_EQ(std::string::npos, host.find("://"));
  LcppData data;
  data.set_host(host);
  data.set_last_visit_time(last_visit_time);
  return data;
}

void InitializeLcpElementLocatorBucket(LcppData& lcpp_data,
                                       const std::string& lcp_element_locator,
                                       double frequency) {
  LcpElementLocatorBucket& bucket = *lcpp_data.mutable_lcpp_stat()
                                         ->mutable_lcp_element_locator_stat()
                                         ->add_lcp_element_locator_buckets();
  bucket.set_lcp_element_locator(lcp_element_locator);
  bucket.set_frequency(frequency);
}

void InitializeLcpInfluencerScriptUrlsBucket(LcppData& lcpp_data,
                                             const std::vector<GURL>& urls,
                                             double frequency) {
  for (auto& url : urls) {
    lcpp_data.mutable_lcpp_stat()
        ->mutable_lcp_script_url_stat()
        ->mutable_main_buckets()
        ->insert({url.spec(), frequency});
  }
}

void InitializeFontUrlsBucket(LcppData& lcpp_data,
                              const std::vector<GURL>& urls,
                              double frequency) {
  for (const auto& url : urls) {
    lcpp_data.mutable_lcpp_stat()
        ->mutable_fetched_font_url_stat()
        ->mutable_main_buckets()
        ->insert({url.spec(), frequency});
  }
}

void InitializeLcpElementLocatorOtherBucket(LcppData& lcpp_data,
                                            double frequency) {
  lcpp_data.mutable_lcpp_stat()
      ->mutable_lcp_element_locator_stat()
      ->set_other_bucket_frequency(frequency);
}

void InitializeLcpInfluencerScriptUrlsOtherBucket(LcppData& lcpp_data,
                                                  double frequency) {
  lcpp_data.mutable_lcpp_stat()
      ->mutable_lcp_script_url_stat()
      ->set_other_bucket_frequency(frequency);
}

void InitializeFontUrlsOtherBucket(LcppData& lcpp_data, double frequency) {
  lcpp_data.mutable_lcpp_stat()
      ->mutable_fetched_font_url_stat()
      ->set_other_bucket_frequency(frequency);
}

PageRequestSummary CreatePageRequestSummary(
    const std::string& main_frame_url,
    const std::string& initial_url,
    const std::vector<blink::mojom::ResourceLoadInfoPtr>& resource_load_infos,
    base::TimeTicks navigation_started) {
  PageRequestSummary summary(ukm::SourceId(), GURL(initial_url),
                             navigation_started);
  summary.main_frame_url = GURL(main_frame_url);
  for (const auto& resource_load_info : resource_load_infos)
    summary.UpdateOrAddResource(*resource_load_info);
  return summary;
}

blink::mojom::ResourceLoadInfoPtr CreateResourceLoadInfo(
    const std::string& url,
    network::mojom::RequestDestination request_destination,
    bool always_access_network) {
  auto resource_load_info = blink::mojom::ResourceLoadInfo::New();
  resource_load_info->final_url = GURL(url);
  resource_load_info->original_url = GURL(url);
  resource_load_info->method = "GET";
  resource_load_info->request_destination = request_destination;
  resource_load_info->network_info = blink::mojom::CommonNetworkInfo::New(
      true, always_access_network, absl::nullopt);
  resource_load_info->request_priority = net::HIGHEST;
  return resource_load_info;
}

blink::mojom::ResourceLoadInfoPtr CreateLowPriorityResourceLoadInfo(
    const std::string& url,
    network::mojom::RequestDestination request_destination) {
  auto resource_load_info =
      CreateResourceLoadInfo(url, request_destination, false);
  resource_load_info->request_priority = net::LOWEST;
  return resource_load_info;
}

blink::mojom::ResourceLoadInfoPtr CreateResourceLoadInfoWithRedirects(
    const std::vector<std::string>& redirect_chain,
    network::mojom::RequestDestination request_destination) {
  auto resource_load_info = blink::mojom::ResourceLoadInfo::New();
  resource_load_info->final_url = GURL(redirect_chain.back());
  resource_load_info->original_url = GURL(redirect_chain.front());
  resource_load_info->method = "GET";
  resource_load_info->request_destination = request_destination;
  resource_load_info->request_priority = net::HIGHEST;
  auto common_network_info =
      blink::mojom::CommonNetworkInfo::New(true, false, absl::nullopt);
  resource_load_info->network_info = common_network_info.Clone();
  for (size_t i = 0; i + 1 < redirect_chain.size(); ++i) {
    resource_load_info->redirect_info_chain.push_back(
        blink::mojom::RedirectInfo::New(
            url::Origin::Create(GURL(redirect_chain[i])),
            common_network_info.Clone()));
  }
  return resource_load_info;
}

PreconnectPrediction CreatePreconnectPrediction(
    std::string host,
    bool is_redirected,
    const std::vector<PreconnectRequest>& requests) {
  PreconnectPrediction prediction;
  prediction.host = host;
  prediction.is_redirected = is_redirected;
  prediction.requests = requests;
  return prediction;
}

void PopulateTestConfig(LoadingPredictorConfig* config, bool small_db) {
  if (small_db) {
    config->max_hosts_to_track = 2;
    config->max_hosts_to_track_for_lcpp = 2;
    config->max_origins_per_entry = 5;
    config->max_consecutive_misses = 2;
    config->max_redirect_consecutive_misses = 2;
    config->lcpp_histogram_sliding_window_size = 5;
    config->max_lcpp_histogram_buckets = 2;
  }
  config->flush_data_to_disk_delay_seconds = 0;
}

std::ostream& operator<<(std::ostream& os, const RedirectData& data) {
  os << "[" << data.primary_key() << "," << data.last_visit_time() << "]"
     << std::endl;
  for (const RedirectStat& redirect : data.redirect_endpoints())
    os << "\t\t" << redirect << std::endl;
  return os;
}

std::ostream& operator<<(std::ostream& os, const RedirectStat& redirect) {
  return os << "[" << redirect.url() << "," << redirect.url_scheme() << ","
            << redirect.url_port() << "," << redirect.number_of_hits() << ","
            << redirect.number_of_misses() << ","
            << redirect.consecutive_misses() << "]";
}

std::ostream& operator<<(std::ostream& os, const OriginData& data) {
  os << "[" << data.host() << "," << data.last_visit_time() << "]" << std::endl;
  for (const OriginStat& origin : data.origins())
    os << "\t\t" << origin << std::endl;
  return os;
}

std::ostream& operator<<(std::ostream& os, const OriginStat& origin) {
  return os << "[" << origin.origin() << "," << origin.number_of_hits() << ","
            << origin.number_of_misses() << "," << origin.consecutive_misses()
            << "," << origin.average_position() << ","
            << origin.always_access_network() << ","
            << origin.accessed_network() << "]";
}

std::ostream& operator<<(std::ostream& os,
                         const LcppStringFrequencyStatData& data) {
  for (const auto& [url, frequency] : data.main_buckets()) {
    os << "\t\t\t\t" << url << ":" << frequency << std::endl;
  }
  os << "\t\t\t\t"
     << "[<other_bucket>," << data.other_bucket_frequency() << "]" << std::endl;
  return os;
}

std::ostream& operator<<(std::ostream& os, const LcppData& data) {
  os << "[" << data.host() << "," << data.last_visit_time() << "]" << std::endl;
  os << "\t\t"
     << "lcp_element_locator_stat:" << std::endl;
  for (const LcpElementLocatorBucket& bucket :
       data.lcpp_stat()
           .lcp_element_locator_stat()
           .lcp_element_locator_buckets()) {
    os << "\t\t\t\t" << bucket << std::endl;
  }
  os << "\t\t\t\t"
     << "[<other_bucket>,"
     << data.lcpp_stat().lcp_element_locator_stat().other_bucket_frequency()
     << "]" << std::endl;
  os << "\t\t"
     << "lcp_script_url_stat:" << std::endl;
  os << data.lcpp_stat().lcp_script_url_stat();
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const LcpElementLocatorBucket& bucket) {
  return os << "[" << bucket.lcp_element_locator() << "," << bucket.frequency()
            << "]";
}

std::ostream& operator<<(std::ostream& os,
                         const OriginRequestSummary& summary) {
  return os << "[" << summary.origin << "," << summary.always_access_network
            << "," << summary.accessed_network << ","
            << summary.first_occurrence << "]";
}

std::ostream& operator<<(std::ostream& os, const PageRequestSummary& summary) {
  os << "[" << summary.main_frame_url << "," << summary.initial_url << "]"
     << std::endl;
  for (const auto& pair : summary.origins)
    os << "\t\t" << pair.first << ":" << pair.second << std::endl;
  return os;
}

std::ostream& operator<<(std::ostream& os, const PreconnectRequest& request) {
  return os << "[" << request.origin << "," << request.num_sockets << ","
            << request.allow_credentials << ","
            << request.network_anonymization_key.ToDebugString() << "]";
}

std::ostream& operator<<(std::ostream& os,
                         const PreconnectPrediction& prediction) {
  os << "[" << prediction.host << "," << prediction.is_redirected << "]"
     << std::endl;

  for (const auto& request : prediction.requests)
    os << "\t\t" << request << std::endl;

  return os;
}

bool operator==(const RedirectData& lhs, const RedirectData& rhs) {
  bool equal = lhs.primary_key() == rhs.primary_key() &&
               lhs.redirect_endpoints_size() == rhs.redirect_endpoints_size();

  if (!equal)
    return false;

  for (int i = 0; i < lhs.redirect_endpoints_size(); ++i)
    equal = equal && lhs.redirect_endpoints(i) == rhs.redirect_endpoints(i);

  return equal;
}

bool operator==(const RedirectStat& lhs, const RedirectStat& rhs) {
  return lhs.url() == rhs.url() && lhs.url_scheme() == rhs.url_scheme() &&
         lhs.url_port() == rhs.url_port() &&
         lhs.number_of_hits() == rhs.number_of_hits() &&
         lhs.number_of_misses() == rhs.number_of_misses() &&
         lhs.consecutive_misses() == rhs.consecutive_misses();
}

bool operator==(const PageRequestSummary& lhs, const PageRequestSummary& rhs) {
  return lhs.main_frame_url == rhs.main_frame_url &&
         lhs.initial_url == rhs.initial_url && lhs.origins == rhs.origins &&
         lhs.subresource_urls == rhs.subresource_urls;
}

bool operator==(const OriginRequestSummary& lhs,
                const OriginRequestSummary& rhs) {
  return lhs.origin == rhs.origin &&
         lhs.always_access_network == rhs.always_access_network &&
         lhs.accessed_network == rhs.accessed_network &&
         lhs.first_occurrence == rhs.first_occurrence;
}

bool operator==(const OriginData& lhs, const OriginData& rhs) {
  bool equal =
      lhs.host() == rhs.host() && lhs.origins_size() == rhs.origins_size();
  if (!equal)
    return false;

  for (int i = 0; i < lhs.origins_size(); ++i)
    equal = equal && lhs.origins(i) == rhs.origins(i);

  return equal;
}

bool operator==(const LcpElementLocatorBucket& lhs,
                const LcpElementLocatorBucket& rhs) {
  return lhs.lcp_element_locator() == rhs.lcp_element_locator() &&
         AlmostEqual(lhs.frequency(), rhs.frequency());
}

bool operator==(const LcpElementLocatorStat& lhs,
                const LcpElementLocatorStat& rhs) {
  if (lhs.lcp_element_locator_buckets_size() !=
          rhs.lcp_element_locator_buckets_size() ||
      !AlmostEqual(lhs.other_bucket_frequency(),
                   rhs.other_bucket_frequency())) {
    return false;
  }

  // lcp_element_locator_buckets don't care the order.
  std::map<std::string, double> lhs_map;
  for (const auto& it : lhs.lcp_element_locator_buckets()) {
    lhs_map.emplace(it.lcp_element_locator(), it.frequency());
  }

  for (const auto& rhs_it : rhs.lcp_element_locator_buckets()) {
    const auto& lhs_it = lhs_map.find(rhs_it.lcp_element_locator());
    if (lhs_it == lhs_map.end() ||
        !AlmostEqual(lhs_it->second, rhs_it.frequency())) {
      return false;
    }
  }

  return true;
}

bool operator==(const LcppStringFrequencyStatData& lhs,
                const LcppStringFrequencyStatData& rhs) {
  if (lhs.main_buckets_size() != rhs.main_buckets_size() ||
      !AlmostEqual(lhs.other_bucket_frequency(),
                   rhs.other_bucket_frequency())) {
    return false;
  }

  for (const auto& [rhs_entry, rhs_frequency] : rhs.main_buckets()) {
    const auto& lhs_it = lhs.main_buckets().find(rhs_entry);
    if (lhs_it == lhs.main_buckets().end() ||
        !AlmostEqual(lhs_it->second, rhs_frequency)) {
      return false;
    }
  }

  return true;
}

bool operator==(const LcppStat& lhs, const LcppStat& rhs) {
  return lhs.lcp_element_locator_stat() == rhs.lcp_element_locator_stat() &&
         lhs.lcp_script_url_stat() == rhs.lcp_script_url_stat() &&
         lhs.fetched_font_url_stat() == rhs.fetched_font_url_stat();
}

bool operator==(const LcppData& lhs, const LcppData& rhs) {
  return lhs.host() == rhs.host() && lhs.lcpp_stat() == rhs.lcpp_stat();
}

bool operator==(const OriginStat& lhs, const OriginStat& rhs) {
  return lhs.origin() == rhs.origin() &&
         lhs.number_of_hits() == rhs.number_of_hits() &&
         lhs.number_of_misses() == rhs.number_of_misses() &&
         lhs.consecutive_misses() == rhs.consecutive_misses() &&
         AlmostEqual(lhs.average_position(), rhs.average_position()) &&
         lhs.always_access_network() == rhs.always_access_network() &&
         lhs.accessed_network() == rhs.accessed_network();
}

bool operator==(const PreconnectRequest& lhs, const PreconnectRequest& rhs) {
  return lhs.origin == rhs.origin && lhs.num_sockets == rhs.num_sockets &&
         lhs.allow_credentials == rhs.allow_credentials &&
         lhs.network_anonymization_key == rhs.network_anonymization_key;
}

bool operator==(const PreconnectPrediction& lhs,
                const PreconnectPrediction& rhs) {
  return lhs.is_redirected == rhs.is_redirected && lhs.host == rhs.host &&
         lhs.requests == rhs.requests;
}

bool operator==(const OptimizationGuidePrediction& lhs,
                const OptimizationGuidePrediction& rhs) {
  return lhs.decision == rhs.decision &&
         lhs.preconnect_prediction == rhs.preconnect_prediction &&
         lhs.predicted_subresources == rhs.predicted_subresources;
}

}  // namespace predictors

namespace blink {
namespace mojom {

std::ostream& operator<<(std::ostream& os, const CommonNetworkInfo& info) {
  return os << "[" << info.network_accessed << "," << info.always_access_network
            << "]";
}

std::ostream& operator<<(std::ostream& os, const ResourceLoadInfo& info) {
  return os << "[" << info.original_url.spec() << ","
            << static_cast<int>(info.request_destination) << ","
            << info.mime_type << "," << info.method << "," << *info.network_info
            << "]";
}

bool operator==(const CommonNetworkInfo& lhs, const CommonNetworkInfo& rhs) {
  return lhs.network_accessed == rhs.network_accessed &&
         lhs.always_access_network == rhs.always_access_network;
}

bool operator==(const ResourceLoadInfo& lhs, const ResourceLoadInfo& rhs) {
  return lhs.original_url == rhs.original_url &&
         lhs.request_destination == rhs.request_destination &&
         lhs.mime_type == rhs.mime_type && lhs.method == rhs.method &&
         *lhs.network_info == *rhs.network_info;
}

}  // namespace mojom
}  // namespace blink

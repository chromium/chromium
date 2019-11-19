// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_test_util.h"

#include <cmath>
#include <memory>
#include <utility>

#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"

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

NavigationID CreateNavigationID(SessionID tab_id,
                                const std::string& main_frame_url) {
  NavigationID navigation_id;
  navigation_id.tab_id = tab_id;
  navigation_id.main_frame_url = GURL(main_frame_url);
  navigation_id.creation_time = base::TimeTicks::Now();
  return navigation_id;
}

PageRequestSummary CreatePageRequestSummary(
    const std::string& main_frame_url,
    const std::string& initial_url,
    const std::vector<content::mojom::ResourceLoadInfoPtr>&
        resource_load_infos) {
  GURL main_frame_gurl(main_frame_url);
  PageRequestSummary summary(main_frame_gurl);
  summary.initial_url = GURL(initial_url);
  for (const auto& resource_load_info : resource_load_infos)
    summary.UpdateOrAddToOrigins(*resource_load_info);
  return summary;
}

content::mojom::ResourceLoadInfoPtr CreateResourceLoadInfo(
    const std::string& url,
    content::ResourceType resource_type,
    bool always_access_network) {
  auto resource_load_info = content::mojom::ResourceLoadInfo::New();
  resource_load_info->url = GURL(url);
  resource_load_info->original_url = GURL(url);
  resource_load_info->method = "GET";
  resource_load_info->resource_type = resource_type;
  resource_load_info->network_info = content::mojom::CommonNetworkInfo::New(
      true, always_access_network, base::nullopt);
  resource_load_info->request_priority = net::HIGHEST;
  return resource_load_info;
}

content::mojom::ResourceLoadInfoPtr CreateLowPriorityResourceLoadInfo(
    const std::string& url,
    content::ResourceType resource_type) {
  auto resource_load_info = CreateResourceLoadInfo(url, resource_type, false);
  resource_load_info->request_priority = net::LOWEST;
  return resource_load_info;
}

content::mojom::ResourceLoadInfoPtr CreateResourceLoadInfoWithRedirects(
    const std::vector<std::string>& redirect_chain,
    content::ResourceType resource_type) {
  auto resource_load_info = content::mojom::ResourceLoadInfo::New();
  resource_load_info->url = GURL(redirect_chain.back());
  resource_load_info->original_url = GURL(redirect_chain.front());
  resource_load_info->method = "GET";
  resource_load_info->resource_type = resource_type;
  resource_load_info->request_priority = net::HIGHEST;
  auto common_network_info =
      content::mojom::CommonNetworkInfo::New(true, false, base::nullopt);
  resource_load_info->network_info = common_network_info.Clone();
  for (size_t i = 0; i + 1 < redirect_chain.size(); ++i) {
    resource_load_info->redirect_info_chain.push_back(
        content::mojom::RedirectInfo::New(GURL(redirect_chain[i]),
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
    config->max_origins_per_entry = 5;
    config->max_consecutive_misses = 2;
    config->max_redirect_consecutive_misses = 2;
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

std::ostream& operator<<(std::ostream& os, const NavigationID& navigation_id) {
  return os << navigation_id.tab_id << "," << navigation_id.main_frame_url;
}

std::ostream& operator<<(std::ostream& os, const PreconnectRequest& request) {
  return os << "[" << request.origin << "," << request.num_sockets << ","
            << request.allow_credentials << ","
            << request.network_isolation_key.ToDebugString() << "]";
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
         lhs.initial_url == rhs.initial_url &&
         lhs.origins == rhs.origins;
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
         lhs.network_isolation_key == rhs.network_isolation_key;
}

bool operator==(const PreconnectPrediction& lhs,
                const PreconnectPrediction& rhs) {
  return lhs.is_redirected == rhs.is_redirected && lhs.host == rhs.host &&
         lhs.requests == rhs.requests;
}

}  // namespace predictors

namespace content {
namespace mojom {

std::ostream& operator<<(std::ostream& os, const CommonNetworkInfo& info) {
  return os << "[" << info.network_accessed << "," << info.always_access_network
            << "]";
}

std::ostream& operator<<(std::ostream& os, const ResourceLoadInfo& info) {
  return os << "[" << info.url.spec() << ","
            << static_cast<int>(info.resource_type) << "," << info.mime_type
            << "," << info.method << "," << *info.network_info << "]";
}

bool operator==(const CommonNetworkInfo& lhs, const CommonNetworkInfo& rhs) {
  return lhs.network_accessed == rhs.network_accessed &&
         lhs.always_access_network == rhs.always_access_network;
}

bool operator==(const ResourceLoadInfo& lhs, const ResourceLoadInfo& rhs) {
  return lhs.url == rhs.url && lhs.resource_type == rhs.resource_type &&
         lhs.mime_type == rhs.mime_type && lhs.method == rhs.method &&
         *lhs.network_info == *rhs.network_info;
}

}  // namespace mojom
}  // namespace content

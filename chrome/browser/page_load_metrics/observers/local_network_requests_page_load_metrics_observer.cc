// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/page_load_metrics/observers/local_network_requests_page_load_metrics_observer.h"

#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace {
// TODO(uthakore): Update router regex based on further study.
// Returns true if the IP address matches the following regular expression for
// common router IP addresses:
// "^192\.168\.(0|10?|2)\.(1(00?)?)|^10\.(0|1)\.(0|10?)\.(1(00?)?|2)"
bool IsLikelyRouterIP(net::IPAddress ip_address) {
  return ip_address.IsIPv4() &&
         ((ip_address.bytes()[0] == 192 && ip_address.bytes()[1] == 168 &&
           (ip_address.bytes()[2] == 0 || ip_address.bytes()[2] == 1 ||
            ip_address.bytes()[2] == 2 || ip_address.bytes()[2] == 10) &&
           (ip_address.bytes()[3] == 1 || ip_address.bytes()[3] == 10 ||
            ip_address.bytes()[3] == 100)) ||
          (ip_address.bytes()[0] == 10 &&
           (ip_address.bytes()[1] == 0 || ip_address.bytes()[1] == 1) &&
           (ip_address.bytes()[2] == 0 || ip_address.bytes()[2] == 1 ||
            ip_address.bytes()[2] == 10) &&
           (ip_address.bytes()[3] == 1 || ip_address.bytes()[3] == 10 ||
            ip_address.bytes()[3] == 100 || ip_address.bytes()[3] == 2)));
}

// Attempts to get the IP address of a resource request from
// |extra_request_info.remote_endpoint|, trying to get it from the URL string in
// |extra_request_info.url| if that fails.
// Sets the values of |resource_ip| and |port| with the extracted IP address and
// port, respectively.
// Returns true if a valid, nonempty IP address was extracted.
bool GetIPAndPort(
    const page_load_metrics::ExtraRequestCompleteInfo& extra_request_info,
    net::IPAddress* resource_ip,
    int* resource_port) {
  // If the request was successful, then the IP address should be in
  // |extra_request_info|.
  bool ip_exists = extra_request_info.remote_endpoint.address().IsValid();
  *resource_ip = extra_request_info.remote_endpoint.address();
  *resource_port = extra_request_info.remote_endpoint.port();

  // If the request failed, it's possible we didn't receive the IP address,
  // possibly because domain resolution failed. As a backup, try getting the IP
  // from the URL. If none was returned, try matching the hostname from the URL
  // itself as it might be an IP address if it is a local network request, which
  // is what we care about.
  if (!ip_exists && extra_request_info.final_url.IsValid()) {
    // TODO(csharrison): https://crbug.com/1023042: Avoid the url::Origin->GURL
    // conversion.  Today the conversion is necessary, because net::IsLocalhost
    // and EffectiveIntPort are only available for GURL.
    GURL final_url = extra_request_info.final_url.GetURL();
    if (net::IsLocalhost(final_url)) {
      *resource_ip = net::IPAddress::IPv4Localhost();
      ip_exists = true;
    } else {
      ip_exists = net::ParseURLHostnameToAddress(final_url.host(), resource_ip);
    }
    *resource_port = final_url.EffectiveIntPort();
  }

  if (net::HostStringIsLocalhost(resource_ip->ToString())) {
    *resource_ip = net::IPAddress::IPv4Localhost();
    ip_exists = true;
  }

  return ip_exists;
}

// Getter for the list of mappings for localhost ports that belong to special
// categories that we want to track.
const std::map<uint16_t, internal::PortType>& GetLocalhostPortCategories() {
  static base::LazyInstance<std::map<uint16_t, internal::PortType>>::Leaky
      localhost_port_categories = LAZY_INSTANCE_INITIALIZER;
  if (localhost_port_categories.Get().empty()) {
    localhost_port_categories.Get() = {
        {80, internal::PORT_TYPE_WEB},     {443, internal::PORT_TYPE_WEB},
        {8000, internal::PORT_TYPE_WEB},   {8008, internal::PORT_TYPE_WEB},
        {8080, internal::PORT_TYPE_WEB},   {8081, internal::PORT_TYPE_WEB},
        {8088, internal::PORT_TYPE_WEB},   {8181, internal::PORT_TYPE_WEB},
        {8888, internal::PORT_TYPE_WEB},   {3306, internal::PORT_TYPE_DB},
        {5432, internal::PORT_TYPE_DB},    {27017, internal::PORT_TYPE_DB},
        {427, internal::PORT_TYPE_PRINT},  {515, internal::PORT_TYPE_PRINT},
        {631, internal::PORT_TYPE_PRINT},  {9100, internal::PORT_TYPE_PRINT},
        {9220, internal::PORT_TYPE_PRINT}, {9500, internal::PORT_TYPE_PRINT},
        {3000, internal::PORT_TYPE_DEV},   {5000, internal::PORT_TYPE_DEV},
        {9000, internal::PORT_TYPE_DEV},
        // TODO(uthakore): Add additional port mappings based on further study.
    };
  }

  return localhost_port_categories.Get();
}

}  // namespace

LocalNetworkRequestsPageLoadMetricsObserver::
    LocalNetworkRequestsPageLoadMetricsObserver() {}
LocalNetworkRequestsPageLoadMetricsObserver::
    ~LocalNetworkRequestsPageLoadMetricsObserver() {}

const char* LocalNetworkRequestsPageLoadMetricsObserver::GetObserverName()
    const {
  static const char kName[] = "LocalNetworkRequestsPageLoadMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
LocalNetworkRequestsPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class needs forwarding for the event OnLoadedResource.
  return FORWARD_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
LocalNetworkRequestsPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class will be deprecated in the near future. No need to support.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
LocalNetworkRequestsPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  // Upon page load, we want to determine whether the page loaded was a public
  // domain or private domain and generate an event describing the domain type.
  net::IPEndPoint remote_endpoint = navigation_handle->GetSocketAddress();
  page_ip_address_ = remote_endpoint.address();

  // In cases where the page loaded does not have a socket address or was not a
  // network resource, we don't want to track the page load. Such resources will
  // fail to parse or return an empty IP address.
  if (!page_ip_address_.IsValid()) {
    return STOP_OBSERVING;
  }

  if (page_ip_address_.IsLoopback()) {
    page_domain_type_ = internal::DOMAIN_TYPE_LOCALHOST;
  } else if (!page_ip_address_.IsPubliclyRoutable()) {
    page_domain_type_ = internal::DOMAIN_TYPE_PRIVATE;
    // Maps from first byte of an IPv4 address to the number of bits in the
    // reserved prefix. This table contains the subset of prefixes defined in
    // |IPAddress::IsReservedIPv4| from which we would expect a page load.
    // TODO: Refactor this code to remove the table and retrieve the prefix from
    // net::IPAddress as per bug 739856.
    static const uint8_t kReservedIPv4Prefixes[][2] = {
        {10, 8}, {100, 10}, {169, 16}, {172, 12}, {192, 16}, {198, 15}};

    for (const auto& entry : kReservedIPv4Prefixes) {
      // A reserved IP will always be a valid IPv4 or IPv6 address and will
      // thus have at least 4 bytes, so [0] is safe here.
      if (page_ip_address_.bytes()[0] == entry[0]) {
        page_ip_prefix_length_ = entry[1];
      }
    }
  } else {
    page_domain_type_ = internal::DOMAIN_TYPE_PUBLIC;
  }

  RecordUkmDomainType(GetDelegate().GetPageUkmSourceId());

  // If the load was localhost, we don't track it because it isn't meaningful
  // for our purposes.
  return (page_domain_type_ == internal::DOMAIN_TYPE_LOCALHOST)
             ? STOP_OBSERVING
             : CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
LocalNetworkRequestsPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // The browser may come back, but there is no guarantee. To be safe, we record
  // what we have now and treat changes to this navigation as new page loads.
  if (GetDelegate().DidCommit()) {
    RecordUkmMetrics(GetDelegate().GetPageUkmSourceId());
    ClearLocalState();
  }

  return CONTINUE_OBSERVING;
}

void LocalNetworkRequestsPageLoadMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo& extra_request_info) {
  net::IPAddress resource_ip;
  int resource_port;

  // We can't track anything if we don't have an IP address for the resource.
  // We also don't want to track any requests to the page's IP address itself.
  if (!GetIPAndPort(extra_request_info, &resource_ip, &resource_port) ||
      !resource_ip.IsValid() || resource_ip == page_ip_address_) {
    return;
  }

  // We monitor localhost resource requests for both public and private page
  // loads.
  if (resource_ip == net::IPAddress::IPv4Localhost()) {
    if (extra_request_info.net_error != net::OK) {
      localhost_request_counts_[resource_port].second++;
    } else {
      localhost_request_counts_[resource_port].first++;
    }
  // We only track public resource requests for private pages.
  } else if (!resource_ip.IsPubliclyRoutable() ||
             page_domain_type_ == internal::DOMAIN_TYPE_PRIVATE) {
    if (extra_request_info.net_error != net::OK) {
      resource_request_counts_[resource_ip].second++;
    } else {
      resource_request_counts_[resource_ip].first++;
    }
  }
}

void LocalNetworkRequestsPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (GetDelegate().DidCommit()) {
    RecordUkmMetrics(GetDelegate().GetPageUkmSourceId());
  }
}

void LocalNetworkRequestsPageLoadMetricsObserver::ClearLocalState() {
  localhost_request_counts_.clear();
  resource_request_counts_.clear();
  requested_resource_types_.reset();
}

internal::ResourceType
LocalNetworkRequestsPageLoadMetricsObserver::DetermineResourceType(
    net::IPAddress resource_ip) {
  if (page_domain_type_ == internal::DOMAIN_TYPE_PUBLIC) {
    DCHECK(!resource_ip.IsPubliclyRoutable());
    return IsLikelyRouterIP(resource_ip) ? internal::RESOURCE_TYPE_ROUTER
                                         : internal::RESOURCE_TYPE_PRIVATE;
  }

  DCHECK_EQ(internal::DOMAIN_TYPE_PRIVATE, page_domain_type_);
  if (!resource_ip.IsPubliclyRoutable()) {  // PRIVATE
    const bool is_same_subnet =
        net::CommonPrefixLength(page_ip_address_, resource_ip) >=
        page_ip_prefix_length_;
    return is_same_subnet ? internal::RESOURCE_TYPE_LOCAL_SAME_SUBNET
                          : internal::RESOURCE_TYPE_LOCAL_DIFF_SUBNET;
  }
  return internal::RESOURCE_TYPE_PUBLIC;  // PUBLIC
}

internal::PortType
LocalNetworkRequestsPageLoadMetricsObserver::DeterminePortType(int port) {
  auto lookup = GetLocalhostPortCategories().find(port);
  if (lookup == GetLocalhostPortCategories().end()) {
    return internal::PORT_TYPE_OTHER;
  } else {
    return lookup->second;
  }
}

void LocalNetworkRequestsPageLoadMetricsObserver::ResolveResourceTypes() {
  // Lazy instantiation.
  if (requested_resource_types_) {
    return;
  }

  requested_resource_types_ =
      std::make_unique<std::map<net::IPAddress, internal::ResourceType>>();
  for (const auto& entry : resource_request_counts_) {
    requested_resource_types_->insert(
        {entry.first, DetermineResourceType(entry.first)});
  }
}

void LocalNetworkRequestsPageLoadMetricsObserver::RecordUkmMetrics(
    ukm::SourceId source_id) {
  if (page_domain_type_ == internal::DOMAIN_TYPE_LOCALHOST ||
      ukm::UkmRecorder::Get() == nullptr) {
    return;
  }

  ResolveResourceTypes();

  // Log an entry for each non-localhost resource (one per IP address).
  for (const auto& entry : resource_request_counts_) {
    ukm::builders::LocalNetworkRequests(source_id)
        .SetResourceType(
            static_cast<int>(requested_resource_types_->at(entry.first)))
        .SetCount_Successful(entry.second.first)
        .SetCount_Failed(entry.second.second)
        .Record(ukm::UkmRecorder::Get());
  }

  // Log an entry for each localhost resource (one per port).
  for (const auto& entry : localhost_request_counts_) {
    ukm::builders::LocalNetworkRequests(source_id)
        .SetResourceType(static_cast<int>(internal::RESOURCE_TYPE_LOCALHOST))
        .SetPortType(static_cast<int>(DeterminePortType(entry.first)))
        .SetCount_Successful(entry.second.first)
        .SetCount_Failed(entry.second.second)
        .Record(ukm::UkmRecorder::Get());
  }
}

void LocalNetworkRequestsPageLoadMetricsObserver::RecordUkmDomainType(
    ukm::SourceId source_id) {
  ukm::builders::PageDomainInfo(source_id)
      .SetDomainType(static_cast<int>(page_domain_type_))
      .Record(ukm::UkmRecorder::Get());
}

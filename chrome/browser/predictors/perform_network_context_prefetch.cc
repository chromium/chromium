// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/perform_network_context_prefetch.h"

#include <optional>
#include <string>
#include <type_traits>

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/predictors/prefetch_manager.h"
#include "chrome/browser/predictors/prefetch_traffic_annotation.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/prefetch/prefetch_headers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/reduce_accept_language_utils.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/network_anonymization_key.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_util.h"
#include "net/http/structured_headers.h"
#include "net/url_request/referrer_policy.h"
#include "net/url_request/url_request_job.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace predictors {

namespace {

using blink::mojom::ResourceType;

// Creates a serialized string header value out of the input type, using
// structured headers as described in
// https://www.rfc-editor.org/rfc/rfc8941.html.
//
// Shameless lifted from content/browser/client_hints/client_hints.cc.
// TODO(crbug.com/342445996): Deduplicate this function.
template <typename T>
  requires(std::is_constructible_v<net::structured_headers::Item, T>)
const std::string SerializeHeaderString(const T& value) {
  return net::structured_headers::SerializeItem(
             net::structured_headers::Item(value))
      .value_or(std::string());
}

// Returns the correct referrer header for a request to `url` from `page`.
// Currently assumes the policy is
// REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN.
GURL CalculateReferrer(const GURL& page, const GURL& url) {
  return net::URLRequestJob::ComputeReferrerForPolicy(
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN, page,
      url);
}

// Heuristic to identify a favicon request.
bool LooksLikeFavicon(const GURL& url) {
  const std::string& as_string = url.possibly_invalid_spec();
  return (as_string.ends_with(".ico") ||
          as_string.find("favicon") != std::string::npos);
}

// Returns true if `url` is now allowed to perform auth on a page from
// `site_for_cookies`. Based on `IsBannedCrossSiteAuth()` from
// //third_party/blink/renderer/platform/loader/fetch/url_loader/request_conversion.cc.
// TODO(crbug.com/342445996): Reduce code duplication.
bool IsBannedCrossSiteAuth(const GURL& url,
                           const net::SiteForCookies& first_party) {
  return !first_party.IsFirstPartyWithSchemefulMode(
      url, /*compute_schemefully=*/true);
}

// Prefetches one subresource URL using NetworkContext::Prefetch. `ua_metadata`
// needs to be a mutable ref because its methods aren't marked const. This is
// similar to PrefetchManager::PrefetchUrl(), but makes more effort to precisely
// predict the exact headers and other fields that the render process will set.
void PrefetchResource(
    network::mojom::NetworkContext* network_context,
    blink::UserAgentMetadata& ua_metadata,
    const std::string& user_agent,
    const std::string& accept_language,
    ResourceType type,
    const GURL& page,
    const url::Origin& page_origin,
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    network::mojom::RequestDestination destination) {
  const auto site_for_cookies = net::SiteForCookies::FromUrl(page);
  network::ResourceRequest request;
  request.method = "GET";
  request.url = url;
  request.site_for_cookies = site_for_cookies;
  request.request_initiator = page_origin;

  // TODO(crbug.com/342445996): We need the predictor to predict the referrer
  // policy so that we can create the referrer fields correctly.
  static constexpr net::ReferrerPolicy kExpectedReferrerPolicy =
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
  request.referrer = CalculateReferrer(page, url);
  request.referrer_policy = kExpectedReferrerPolicy;

  auto& headers = request.headers;
  headers.SetHeader("Purpose", "prefetch");
  headers.SetHeader(prefetch::headers::kSecPurposeHeaderName,
                    prefetch::headers::kSecPurposePrefetchHeaderValue);

  // Client hints headers.
  //
  // TODO(crbug.com/342445996): We either need the predictor to predict client
  // hints headers, or we need the code in //content/browser/client_hints to be
  // refactored so be more general so that it is usable here.
  headers.SetHeader("sec-ch-ua", ua_metadata.SerializeBrandMajorVersionList());
  headers.SetHeader("sec-ch-ua-mobile",
                    SerializeHeaderString(ua_metadata.mobile));
  headers.SetHeader("sec-ch-ua-platform",
                    SerializeHeaderString(ua_metadata.platform));
  // We shouldn't be prefetching if data saver is enabled, so we should never
  // need to set the "save-data" header.

  headers.SetHeader("User-Agent", user_agent);

  headers.SetHeader("Accept-Language", accept_language);

  headers.SetHeader(
      "Accept",
      blink::network_utils::GetAcceptHeaderForDestination(destination));

  // Add the X-Client-Data header for requests to Google properties.
  variations::AppendVariationsHeaderUnknownSignedIn(
      url, variations::InIncognito::kNo, &request);

  request.load_flags = net::LOAD_SUPPORT_ASYNC_REVALIDATION;

  request.destination = destination;
  request.resource_type = static_cast<int>(type);
  request.mode = network::mojom::RequestMode::kNoCors;
  request.enable_load_timing = true;
  request.do_not_prompt_for_login = false;
  request.is_outermost_main_frame = true;
  request.attribution_reporting_support =
      network::mojom::AttributionSupport::kWeb;
  request.shared_dictionary_writer_enabled = true;

  // Suppress credentials for cross-origin image loads. See the comment in
  // PopulateResourceRequest() in
  // //third_party/blink/renderer/platform/loader/fetch/url_loader/request_conversion.cc
  // for more details.

  // TODO(crbug.com/342445996): Identify favicons more reliably.
  const bool is_favicon = LooksLikeFavicon(url);
  if (!is_favicon &&
      destination == network::mojom::RequestDestination::kImage &&
      IsBannedCrossSiteAuth(url, site_for_cookies)) {
    request.do_not_prompt_for_login = true;
    request.load_flags |= net::LOAD_DO_NOT_USE_EMBEDDED_IDENTITY;
  }

  // The hints are only for requests made from the top frame,
  // so frame_origin is the same as top_frame_origin.
  const auto frame_origin = page_origin;

  request.trusted_params = network::ResourceRequest::TrustedParams();
  request.trusted_params->isolation_info =
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 page_origin, frame_origin, site_for_cookies);

  // TODO(crbug.com/342445996): Set `trusted_params->client_security_state` if
  // possible, to stop prefetch being blocked on local networks. This requires a
  // lot of information we don't currently have available here.

  // TODO(crbug.com/342445996): Support interception by extensions.

  // TODO(crbug.com/342445996): Factor out the URLLoaderThrottle code from
  // chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_request.cc
  // and reuse it here.

  network_context->Prefetch(
      content::GlobalRequestID::MakeBrowserInitiated().request_id,
      network::mojom::kURLLoadOptionBlockLocalRequest, request,
      net::MutableNetworkTrafficAnnotationTag(kPrefetchTrafficAnnotation));
}

std::string ComputeAcceptLanguageHeaderValue(const url::Origin& page_origin,
                                             const GURL& request,
                                             Profile* profile,
                                             const PrefService* prefs) {
  // This first bit loads the user's languages from preferences. This is adapted
  // from ProfileNetworkContextService::ComputeAcceptLanguage().
  std::string language_pref = "en";
  if (const base::Value* value =
          prefs->GetUserPrefValue(language::prefs::kAcceptLanguages)) {
    if (const std::string* as_string = value->GetIfString()) {
      language_pref = *as_string;
    }
  }

  // TODO(crbug.com/342445996): If we decide to support incognito mode, we
  // should set `language_pref` to language::GetFirstLanguage(language_pref) in
  // incognito mode.

  // Reduce the number of languages in the header if the feature is enabled.
  //
  // This copied from one of the two versions in
  // //content/browser/renderer_host/navigation_request.cc. We don't bother to
  // check for devtools overrides or origin trials here.
  // TODO(crbug.com/342445996): Reduce the number of copies of this code in
  // the codebase.
  if (auto reduce_accept_lang_utils =
          content::ReduceAcceptLanguageUtils::Create(profile)) {
    std::optional<std::string> reduced_accept_language =
        reduce_accept_lang_utils.value().LookupReducedAcceptLanguage(
            page_origin, url::Origin::Create(request));
    if (reduced_accept_language) {
      language_pref = reduced_accept_language.value();
    }
  }

  const std::string lang = net::HttpUtil::ExpandLanguageList(language_pref);
  return net::HttpUtil::GenerateAcceptLanguageHeader(lang);
}

}  // namespace

void PerformNetworkContextPrefetch(Profile* profile,
                                   const GURL& page,
                                   std::vector<PrefetchRequest> requests) {
  DVLOG(1) << "PrefetchManager::StartNetworkContextPrefetch( page=\"" << page
           << "\", requests.size() = " << requests.size() << " )";
  if (profile->IsOffTheRecord()) {
    // TODO(crbug.com/342445996): Consider whether to support prefetch in
    // incognito or not.
    return;
  }
  // Only support secure connections.
  // TODO(crbug.com/342445996): Maybe relax this restriction if this code is
  // used by features that support insecure prefetches.
  if (!page.SchemeIsCryptographic()) {
    DLOG(ERROR) << "PerformNetworkContextPrefetch() called for non-SSL page: "
                << page << " (ignored)";
    return;
  }
  const auto page_origin = url::Origin::Create(page);
  content::StoragePartition* storage_partition =
      profile->GetStoragePartitionForUrl(page);

  CHECK(storage_partition);
  network::mojom::NetworkContext* network_context =
      storage_partition->GetNetworkContext();
  CHECK(network_context);
  // `ua_metadata` can't be const because we need to call non-const methods on
  // it.
  // TODO(crbug.com/342445996): Make it const once the blink::UserAgentMetadata
  // methods have been made const.
  blink::UserAgentMetadata ua_metadata =
      embedder_support::GetUserAgentMetadata(g_browser_process->local_state());

  // When generating the User-Agent header, we need to take into account user
  // agent reduction enterprise policy. Nothing is ever simple. This code
  // gratuitously copied from from chrome_content_browser_client.cc. This
  // doesn't take into account DevTools overrides or desktop emulation.
  const PrefService* prefs = profile->GetPrefs();
  const embedder_support::UserAgentReductionEnterprisePolicyState
      user_agent_reduction =
          embedder_support::GetUserAgentReductionFromPrefs(prefs);
  const std::string user_agent =
      embedder_support::GetUserAgent(user_agent_reduction);

  for (const auto& [url, network_anonymization_key, destination] : requests) {
    auto resource_type = GetResourceTypeForPrefetch(destination);
    if (!resource_type) {
      // TODO(crbug.com/342445996): Support more resource types.
      continue;
    }
    // Only support secure subresources.
    // TODO(crbug.com/342445996): Relax this restriction in future if needed.
    if (!url.SchemeIsCryptographic()) {
      DLOG(ERROR)
          << "PerformNetworkContextPrefetch() called for non-SSL subresource: "
          << url << " (ignored)";
      continue;
    }

    // TODO(crbug.com/342445996): Usually requests will have the same origin, so
    // maybe cache (origin, accept_langage) to avoid wasteful recalculation?
    const std::string accept_language =
        ComputeAcceptLanguageHeaderValue(page_origin, url, profile, prefs);
    PrefetchResource(network_context, ua_metadata, user_agent, accept_language,
                     resource_type.value(), page, page_origin, url,
                     network_anonymization_key, destination);
  }
}

}  // namespace predictors

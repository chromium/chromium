// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/search_prefetch/base_search_prefetch_request.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/client_hints.h"
#include "content/public/browser/frame_accept_header.h"
#include "content/public/common/content_constants.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"

BaseSearchPrefetchRequest::BaseSearchPrefetchRequest(
    const GURL& prefetch_url,
    base::OnceClosure report_error_callback)
    : prefetch_url_(prefetch_url),
      report_error_callback_(std::move(report_error_callback)) {}

BaseSearchPrefetchRequest::~BaseSearchPrefetchRequest() = default;

void BaseSearchPrefetchRequest::StartPrefetchRequest(Profile* profile) {
  net::NetworkTrafficAnnotationTag network_traffic_annotation =
      net::DefineNetworkTrafficAnnotation("search_prefetch_service", R"(
        semantics {
          sender: "Search Prefetch Service"
          description:
            "Prefetches search results page (HTML) based on omnibox hints "
            "provided by the user's default search engine. This allows the "
            "prefetched content to be served when the user navigates to the "
            "omnibox hint."
          trigger:
            "User typing in the omnibox and the default search provider "
            "indicates the provided omnibox hint entry is likely to be "
            "navigated which would result in loading a search results page for "
            "that hint."
          data: "Credentials if user is signed in."
          destination: OTHER
          destination_other: "The user's default search engine."
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Users can control this feature by opting out of 'Preload pages "
            "for faster browsing and searching'"
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
            NetworkPredictionOptions {
              NetworkPredictionOptions: 2
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->load_flags |= net::LOAD_PREFETCH;
  resource_request->url = prefetch_url_;
  // Search prefetch URL Loaders should check |report_raw_headers| on the
  // intercepted request to clear out the raw headers when |report_raw_headers|
  // is false.
  resource_request->report_raw_headers = true;
  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kInclude;
  variations::AppendVariationsHeaderUnknownSignedIn(
      prefetch_url_, variations::InIncognito::kNo, resource_request.get());
  resource_request->headers.SetHeader(content::kCorsExemptPurposeHeaderName,
                                      "prefetch");
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAccept,
      content::FrameAcceptHeaderValue(/*allow_sxg_responses=*/true, profile));

  bool js_enabled = profile->GetPrefs() && profile->GetPrefs()->GetBoolean(
                                               prefs::kWebKitJavascriptEnabled);

  AddClientHintsHeadersToPrefetchNavigation(
      resource_request->url, &(resource_request->headers), profile,
      profile->GetClientHintsControllerDelegate(),
      /*is_ua_override_on=*/false, js_enabled);

  // TODO(ryansturm): Find other headers that may need to be set.
  // https://crbug.com/1138648

  current_status_ = SearchPrefetchStatus::kInFlight;

  StartPrefetchRequestInternal(profile, std::move(resource_request),
                               network_traffic_annotation);
}

void BaseSearchPrefetchRequest::CancelPrefetch() {
  DCHECK(current_status_ == SearchPrefetchStatus::kInFlight);
  current_status_ = SearchPrefetchStatus::kRequestCancelled;
  StopPrefetch();
}

void BaseSearchPrefetchRequest::ErrorEncountered() {
  DCHECK(!report_error_callback_.is_null());
  // A streaming response can still encounter an error after the headers, so
  // both these states are possible.
  DCHECK(current_status_ == SearchPrefetchStatus::kInFlight ||
         current_status_ == SearchPrefetchStatus::kCanBeServed);
  current_status_ = SearchPrefetchStatus::kRequestFailed;
  std::move(report_error_callback_).Run();
  StopPrefetch();
}

void BaseSearchPrefetchRequest::MarkPrefetchAsServable() {
  DCHECK(current_status_ == SearchPrefetchStatus::kInFlight);
  current_status_ = SearchPrefetchStatus::kCanBeServed;
}

bool BaseSearchPrefetchRequest::CanServePrefetchRequest(
    const scoped_refptr<net::HttpResponseHeaders> headers) {
  if (!headers)
    return false;

  // Any 200 response can be served.
  if (headers->response_code() >= net::HTTP_OK &&
      headers->response_code() < net::HTTP_MULTIPLE_CHOICES) {
    return true;
  }

  return false;
}

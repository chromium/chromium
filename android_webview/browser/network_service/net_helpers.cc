// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/net_helpers.h"

#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/common/url_constants.h"
#include "base/check_op.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace android_webview {

namespace {
int UpdateCacheControlFlags(int load_flags, int cache_control_flags) {
  const int all_cache_control_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_VALIDATE_CACHE |
      net::LOAD_SKIP_CACHE_VALIDATION | net::LOAD_ONLY_FROM_CACHE;
  DCHECK_EQ((cache_control_flags & all_cache_control_flags),
            cache_control_flags);
  load_flags &= ~all_cache_control_flags;
  load_flags |= cache_control_flags;
  return load_flags;
}

// Gets the net-layer load_flags which reflect |client|'s cache mode.
int GetCacheModeForClient(AwContentsIoThreadClient* client) {
  DCHECK(client);
  AwContentsIoThreadClient::CacheMode cache_mode = client->GetCacheMode();
  switch (cache_mode) {
    case AwContentsIoThreadClient::LOAD_CACHE_ELSE_NETWORK:
      // If the resource is in the cache (even if expired), load from cache.
      // Otherwise, fall back to network.
      return net::LOAD_SKIP_CACHE_VALIDATION;
    case AwContentsIoThreadClient::LOAD_NO_CACHE:
      // Always load from the network, don't use the cache.
      return net::LOAD_BYPASS_CACHE;
    case AwContentsIoThreadClient::LOAD_CACHE_ONLY:
      // If the resource is in the cache (even if expired), load from cache. Do
      // not fall back to the network.
      return net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
    default:
      // If the resource is in the cache (and is valid), load from cache.
      // Otherwise, fall back to network. This is the usual (default) case.
      return 0;
  }
}

}  // namespace

int UpdateLoadFlags(int load_flags, AwContentsIoThreadClient* client) {
  if (!client)
    return load_flags;

  if (client->ShouldBlockNetworkLoads()) {
    return UpdateCacheControlFlags(
        load_flags,
        net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION);
  }

  int cache_mode = GetCacheModeForClient(client);
  if (!cache_mode)
    return load_flags;

  return UpdateCacheControlFlags(load_flags, cache_mode);
}

bool ShouldBlockURL(const GURL& url, AwContentsIoThreadClient* client) {
  if (!client)
    return false;

  // Part of implementation of WebSettings.allowContentAccess.
  if (url.SchemeIs(url::kContentScheme) && client->ShouldBlockContentUrls())
    return true;

  if (url.SchemeIsFile()) {
    bool is_special_file_url = IsAndroidSpecialFileUrl(url);

    if (is_special_file_url && client->ShouldBlockSpecialFileUrls()) {
      return true;
    }

    // Part of implementation of WebSettings.allowFileAccess.
    if (client->ShouldBlockFileUrls()) {
      // Application's assets and resources are always available.
      return !is_special_file_url;
    }
  }

  return client->ShouldBlockNetworkLoads() && url.SchemeIs(url::kFtpScheme);
}

int GetHttpCacheSize() {
  // This currently returns a constant value, but we may consider deciding cache
  // size dynamically, since Android provides better support on newer versions
  // (http://crbug.com/893318).
  return 20 * 1024 * 1024;  // 20M
}

void ConvertRequestHeadersToVectors(const net::HttpRequestHeaders& headers,
                                    std::vector<std::string>* header_names,
                                    std::vector<std::string>* header_values) {
  DCHECK(header_names->empty());
  DCHECK(header_values->empty());
  net::HttpRequestHeaders::Iterator headers_iterator(headers);
  while (headers_iterator.GetNext()) {
    header_names->push_back(headers_iterator.name());
    header_values->push_back(headers_iterator.value());
  }
}

}  // namespace android_webview

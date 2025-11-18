// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_NET_HELPERS_H_
#define ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_NET_HELPERS_H_

#include <string>
#include <vector>

class GURL;

namespace net {

class HttpRequestHeaders;

}  // namespace net

namespace android_webview {

class AwContentsIoThreadClient;

// Returns the updated request's |load_flags| based on the settings.
int UpdateLoadFlags(int load_flags, AwContentsIoThreadClient* client);

// Returns true if the given URL should be aborted with
// net::ERR_ACCESS_DENIED.
bool ShouldBlockURL(const GURL& url, AwContentsIoThreadClient* client);

// Determines the desired size for WebView's on-disk HttpCache, measured in
// Bytes.
int GetHttpCacheSize();

// Convert `net::HttpRequestHeaders` to a pair of vectors to be passed through
// JNI.
void ConvertRequestHeadersToVectors(const net::HttpRequestHeaders& headers,
                                    std::vector<std::string>* header_names,
                                    std::vector<std::string>* header_values);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_NET_HELPERS_H_

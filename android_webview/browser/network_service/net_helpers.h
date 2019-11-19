// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_NET_HELPERS_H_
#define ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_NET_HELPERS_H_

#include <memory>

class GURL;

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

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_NETWORK_SERVICE_NET_HELPERS_H_

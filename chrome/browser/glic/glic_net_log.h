// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_NET_LOG_H_
#define CHROME_BROWSER_GLIC_GLIC_NET_LOG_H_

class GURL;

namespace glic::net_log {

enum class GlicPage { kGlicFre, kGlic };
// Log a fake network request to NetLog with a Glic traffic annotation. This
// doesn't *send* a request, it just logs it for chrome://net-export.
//
// Unfortunately there's no way to pass `traffic_annotation` to
// LoadURLWithParams() or to tag the WebContents with an annotation, so we
// use this hacky workaround to capture the annotation at runtime.
void LogDummyNetworkRequestForTrafficAnnotation(const GURL& url,
                                                GlicPage glic_page);

}  // namespace glic::net_log

#endif  // CHROME_BROWSER_GLIC_GLIC_NET_LOG_H_

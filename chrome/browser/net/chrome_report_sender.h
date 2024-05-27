// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_REPORT_SENDER_H_
#define CHROME_BROWSER_NET_CHROME_REPORT_SENDER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

class GURL;

namespace network {
class SharedURLLoaderFactory;
}

// Similar to net::ReportSender but uses network::SimpleURLLoader under the
// hood. Reports sent with this function do not save or send credentials.
void SendReport(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    const GURL& report_uri,
    const std::string& content_type,
    const std::string& report,
    base::OnceClosure success_callback,
    base::OnceCallback<void(int /* net_error */, int /* http_response_code */)>
        error_callback);

#endif  // CHROME_BROWSER_NET_CHROME_REPORT_SENDER_H_

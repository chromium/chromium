// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_TLS_DEPRECATION_CONFIG_H_
#define CHROME_BROWSER_SSL_TLS_DEPRECATION_CONFIG_H_

#include <memory>

class GURL;

namespace chrome_browser_ssl {
class LegacyTLSExperimentConfig;
}  // namespace chrome_browser_ssl

void SetRemoteTLSDeprecationConfigProto(
    std::unique_ptr<chrome_browser_ssl::LegacyTLSExperimentConfig> proto);

bool ShouldSuppressLegacyTLSWarning(const GURL& url);

#endif  // CHROME_BROWSER_SSL_TLS_DEPRECATION_CONFIG_H_

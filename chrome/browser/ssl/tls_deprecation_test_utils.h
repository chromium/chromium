// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_TLS_DEPRECATION_TEST_UTILS_H_
#define CHROME_BROWSER_SSL_TLS_DEPRECATION_TEST_UTILS_H_

#include <memory>

namespace base {
class RunLoop;
}  // namespace base

namespace content {
class NavigationSimulator;
class WebContents;
}  // namespace content

class GURL;

const char kLegacyTLSHost[] = "example-nonsecure.test";
const char kLegacyTLSURL[] = "https://example-nonsecure.test";
// SHA-256 hash of kLegacyTLSURL for use in setting a control site in the
// LegacyTLSExperimentConfig for Legacy TLS tests. Generated with
// `echo -n "example-nonsecure.test" | openssl sha256`.
const char kLegacyTlsControlUrlHash[] =
    "aaa334d67e96314a14d5679b2309e72f96bf30f9fe9b218e5db3d57be8baa94c";

void InitializeEmptyLegacyTLSConfig();
void InitializeEmptyLegacyTLSConfigNetworkService(base::RunLoop* run_loop);

void InitializeLegacyTLSConfigWithControl();
void InitializeLegacyTLSConfigWithControlNetworkService(
    base::RunLoop* run_loop);

// Creates and starts a simulated navigation using the specified SSL protocol
// version (e.g., net::SSL_CONNECTION_VERSION_TLS1_2).
std::unique_ptr<content::NavigationSimulator> CreateTLSNavigation(
    const GURL& url,
    content::WebContents* web_contents,
    uint16_t ssl_protocol_version);

// Creates and starts a simulated navigation using TLS 1.0.
std::unique_ptr<content::NavigationSimulator> CreateLegacyTLSNavigation(
    const GURL& url,
    content::WebContents* web_contents);

// Creates and starts a simulated navigation using TLS 1.2.
std::unique_ptr<content::NavigationSimulator> CreateNonlegacyTLSNavigation(
    const GURL& url,
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_SSL_TLS_DEPRECATION_TEST_UTILS_H_

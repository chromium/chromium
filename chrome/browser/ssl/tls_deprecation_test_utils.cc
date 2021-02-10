// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/tls_deprecation_test_utils.h"

#include "base/run_loop.h"
#include "chrome/browser/ssl/tls_deprecation_config.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/navigation_simulator.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/proto/tls_deprecation_config.pb.h"
#include "url/gurl.h"

void InitializeEmptyLegacyTLSConfig() {
  auto config =
      std::make_unique<chrome_browser_ssl::LegacyTLSExperimentConfig>();
  config->set_version_id(1);
  // Set browser-side config.
  SetRemoteTLSDeprecationConfig(config->SerializeAsString());
}

void InitializeEmptyLegacyTLSConfigNetworkService(base::RunLoop* run_loop) {
  auto config =
      std::make_unique<chrome_browser_ssl::LegacyTLSExperimentConfig>();
  config->set_version_id(1);
  // Push config to network service.
  content::GetNetworkService()->UpdateLegacyTLSConfig(
      base::as_bytes(base::make_span(config->SerializeAsString())),
      run_loop->QuitClosure());
  run_loop->Run();
}

void InitializeLegacyTLSConfigWithControl() {
  auto config =
      std::make_unique<chrome_browser_ssl::LegacyTLSExperimentConfig>();
  config->set_version_id(1);
  config->add_control_site_hashes(kLegacyTlsControlUrlHash);
  SetRemoteTLSDeprecationConfig(config->SerializeAsString());
}

void InitializeLegacyTLSConfigWithControlNetworkService(
    base::RunLoop* run_loop) {
  auto config =
      std::make_unique<chrome_browser_ssl::LegacyTLSExperimentConfig>();
  config->set_version_id(1);
  config->add_control_site_hashes(kLegacyTlsControlUrlHash);
  // Push config to network service.
  content::GetNetworkService()->UpdateLegacyTLSConfig(
      base::as_bytes(base::make_span(config->SerializeAsString())),
      run_loop->QuitClosure());
  run_loop->Run();
}

std::unique_ptr<content::NavigationSimulator> CreateTLSNavigation(
    const GURL& url,
    content::WebContents* web_contents,
    uint16_t ssl_protocol_version) {
  auto navigation_simulator =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents);
  navigation_simulator->Start();

  // Setup the SSLInfo to specify the TLS version used.
  auto cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  net::SSLInfo ssl_info = net::SSLInfo();
  net::SSLConnectionStatusSetVersion(ssl_protocol_version,
                                     &ssl_info.connection_status);
  ssl_info.cert = cert;
  navigation_simulator->SetSSLInfo(ssl_info);

  return navigation_simulator;
}

std::unique_ptr<content::NavigationSimulator> CreateLegacyTLSNavigation(
    const GURL& url,
    content::WebContents* web_contents) {
  return CreateTLSNavigation(url, web_contents,
                             net::SSL_CONNECTION_VERSION_TLS1);
}

std::unique_ptr<content::NavigationSimulator> CreateNonlegacyTLSNavigation(
    const GURL& url,
    content::WebContents* web_contents) {
  return CreateTLSNavigation(url, web_contents,
                             net::SSL_CONNECTION_VERSION_TLS1_2);
}

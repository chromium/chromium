// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_ERROR_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_SSL_SSL_ERROR_CONTROLLER_CLIENT_H_

#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "net/ssl/ssl_info.h"

namespace content {
class WebContents;
}

namespace security_interstitials {
class SettingsPageHelper;
}

// Provides embedder-specific logic for the SSL error page controller.
class SSLErrorControllerClient
    : public security_interstitials::SecurityInterstitialControllerClient {
 public:
  // Actions recorded on recurrent error interstitials. This enum is
  // histogrammed, so do not add, reorder, or remove values. Exposed for
  // testing.
  enum class RecurrentErrorAction {
    kShow,
    kProceed,
    kMaxValue = kProceed,
  };

  SSLErrorControllerClient(
      content::WebContents* web_contents,
      const net::SSLInfo& ssl_info,
      int cert_error,
      const GURL& request_url,
      std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper,
      std::unique_ptr<security_interstitials::SettingsPageHelper>
          settings_page_helper);

  SSLErrorControllerClient(const SSLErrorControllerClient&) = delete;
  SSLErrorControllerClient& operator=(const SSLErrorControllerClient&) = delete;

  ~SSLErrorControllerClient() override;

  // security_interstitials::ControllerClient overrides
  void GoBack() override;
  void Proceed() override;
  bool CanLaunchDateAndTimeSettings() override;
  void LaunchDateAndTimeSettings() override;
  bool HasSeenRecurrentError() override;

 private:
  const net::SSLInfo ssl_info_;
  const GURL request_url_;
  const int cert_error_;
};

#endif  // CHROME_BROWSER_SSL_SSL_ERROR_CONTROLLER_CLIENT_H_

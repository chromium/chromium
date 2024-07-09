// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_verification_controller_client.h"

#include <memory>
#include <utility>

#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/web_contents.h"

namespace {

std::unique_ptr<security_interstitials::MetricsHelper> GetMetricsHelper(
    const GURL& url) {
  security_interstitials::MetricsHelper::ReportDetails settings;
  settings.metric_prefix = "supervised_user_verification";

  return std::make_unique<security_interstitials::MetricsHelper>(url, settings,
                                                                 nullptr);
}

}  // namespace

SupervisedUserVerificationControllerClient::
    SupervisedUserVerificationControllerClient(
        content::WebContents* web_contents,
        PrefService* prefs,
        const std::string& app_locale,
        const GURL& default_safe_page,
        const GURL& request_url)
    : SecurityInterstitialControllerClient(web_contents,
                                           GetMetricsHelper(request_url),
                                           prefs,
                                           app_locale,
                                           default_safe_page,
                                           /*settings_page_helper=*/nullptr) {}

SupervisedUserVerificationControllerClient::
    ~SupervisedUserVerificationControllerClient() = default;

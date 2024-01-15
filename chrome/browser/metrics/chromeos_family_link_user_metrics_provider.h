// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROMEOS_FAMILY_LINK_USER_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_CHROMEOS_FAMILY_LINK_USER_METRICS_PROVIDER_H_

#include <memory>
#include <optional>

#include "components/metrics/metrics_provider.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

// This metrics provider categorizes the current user into over or under the age
// of consent for UMA dashboard filtering. This metrics provider is ChromeOS
// specific.
class ChromeOSFamilyLinkUserMetricsProvider
    : public metrics::MetricsProvider,
      public session_manager::SessionManagerObserver {
 public:
  // These enum values represent the current user's supervision type for the
  // Family Experiences team's metrics.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "FamilyLinkUserLogSegment" in src/tools/metrics/histograms/enums.xml.
  enum class LogSegment {
    // User does not fall into any of the below categories. For example, this
    // bucket includes regular users.
    kOther = 0,
    // Child under age of consent.
    kUnderConsentAge = 1,
    // Regular Gaia account above the age of consent with supervision added.
    kOverConsentAge = 2,
    // Add future entries above this comment, in sync with
    // "FamilyLinkUserLogSegment" in src/tools/metrics/histograms/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kOverConsentAge
  };

  ChromeOSFamilyLinkUserMetricsProvider();
  ChromeOSFamilyLinkUserMetricsProvider(
      const ChromeOSFamilyLinkUserMetricsProvider&) = delete;
  ChromeOSFamilyLinkUserMetricsProvider& operator=(
      const ChromeOSFamilyLinkUserMetricsProvider&) = delete;
  ~ChromeOSFamilyLinkUserMetricsProvider() override;

  // metrics::MetricsProvider:
  bool ProvideHistograms() override;

  // session_manager::SessionManagerObserver:
  void OnUserSessionStarted(bool is_primary_user) override;

  static const char* GetHistogramNameForTesting();

 protected:
  // This function is protected for testing.
  virtual void SetLogSegment(LogSegment log_segment);

 private:
  void OnAccessTokenRequestCompleted(GoogleServiceAuthError error,
                                     signin::AccessTokenInfo access_token_info);

  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  // Cache the log segment because it won't change during the session once
  // assigned.
  std::optional<LogSegment> log_segment_;
};

#endif  // CHROME_BROWSER_METRICS_CHROMEOS_FAMILY_LINK_USER_METRICS_PROVIDER_H_

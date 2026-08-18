// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_IMPL_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_IMPL_H_

#include <optional>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "net/base/schemeful_site.h"

class PrefService;

namespace content_settings {
class CookieSettings;
}

class PrivacySandboxServiceImpl : public PrivacySandboxService {
 public:
  PrivacySandboxServiceImpl(
      privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
      scoped_refptr<content_settings::CookieSettings> cookie_settings,
      PrefService* pref_service,
      profile_metrics::BrowserProfileType profile_type,
      first_party_sets::FirstPartySetsPolicyService*
          first_party_sets_policy_service);

  ~PrivacySandboxServiceImpl() override;

  // KeyedService:
  void Shutdown() override;

  // PrivacySandboxService:
  void SetRelatedWebsiteSetsDataAccessEnabled(bool enabled) override;
  bool IsRelatedWebsiteSetsDataAccessEnabled() const override;
  bool IsRelatedWebsiteSetsDataAccessManaged() const override;
  std::optional<net::SchemefulSite> GetRelatedWebsiteSetOwner(
      const GURL& site_url) const override;
  std::optional<std::u16string> GetRelatedWebsiteSetOwnerForDisplay(
      const GURL& site_url) const override;
  bool IsPartOfManagedRelatedWebsiteSet(
      const net::SchemefulSite& site) const override;

 protected:
  friend class PrivacySandboxServiceTest;
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           RelatedWebsiteSetsNotRelevantMetricAllowedCookies);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           RelatedWebsiteSetsNotRelevantMetricBlockedCookies);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           RelatedWebsiteSetsEnabledMetric);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxServiceTest,
                           RelatedWebsiteSetsDisabledMetric);

  // Contains all possible states of first party sets preference.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Must be kept in sync with the FirstPartySetsState enum in
  // histograms/enums.xml.
  enum class FirstPartySetsState {
    // The user allows all cookies, or blocks all cookies.
    kFpsNotRelevant = 0,
    // The user blocks third-party cookies, and has FPS enabled.
    kFpsEnabled = 1,
    // The user blocks third-party cookies, and has FPS disabled.
    kFpsDisabled = 2,
    kMaxValue = kFpsDisabled,
  };

  // Helper function to log first party sets state.
  void RecordFirstPartySetsStateHistogram();

  // Logs the state of cookie-related settings (FPS, Tracking Protection).
  // Called once per profile startup.
  void LogPrivacySandboxState();

  // Checks to see if initialization of the user's RWS pref is required, and if
  // so, sets the default value based on the user's current cookie settings.
  void MaybeInitializeRelatedWebsiteSetsPref();

 private:
  raw_ptr<privacy_sandbox::PrivacySandboxSettings> privacy_sandbox_settings_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  raw_ptr<PrefService> pref_service_;
  profile_metrics::BrowserProfileType profile_type_;
  raw_ptr<first_party_sets::FirstPartySetsPolicyService>
      first_party_sets_policy_service_;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_IMPL_H_

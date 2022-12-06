// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_FAMILY_LINK_USER_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_FAMILY_LINK_USER_METRICS_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/metrics/metrics_provider.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Categorizes the primary account of the active user profile into a FamilyLink
// supervision type to segment the Chrome user population.
// TODO(crbug.com/1347816): Support multi-profile supervision type segmentation.
class FamilyLinkUserMetricsProvider : public metrics::MetricsProvider,
                                      public IdentityManagerFactory::Observer,
                                      public signin::IdentityManager::Observer {
 public:
  // These enum values represent the user's supervision type and how the
  // supervision has been enabled.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "FamilyLinkUserLogSegment" in src/tools/metrics/histograms/enums.xml.
  enum class LogSegment {
    // User is not supervised by FamilyLink.
    kUnsupervised = 0,
    // User that is required to be supervised by FamilyLink due to child account
    // policies (maps to Unicorn and Griffin accounts).
    kSupervisionEnabledByPolicy = 1,
    // User that has chosen to be supervised by FamilyLink (maps to Geller
    // accounts).
    kSupervisionEnabledByUser = 2,
    // Add future entries above this comment, in sync with
    // "FamilyLinkUserLogSegment" in src/tools/metrics/histograms/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kSupervisionEnabledByUser
  };

  FamilyLinkUserMetricsProvider();
  FamilyLinkUserMetricsProvider(const FamilyLinkUserMetricsProvider&) = delete;
  FamilyLinkUserMetricsProvider& operator=(
      const FamilyLinkUserMetricsProvider&) = delete;
  ~FamilyLinkUserMetricsProvider() override;

  // metrics::MetricsProvider:
  bool ProvideHistograms() override;

  // IdentityManagerFactoryObserver:
  void IdentityManagerCreated(
      signin::IdentityManager* identity_manager) override;

  // signin::IdentityManager::Observer
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  static const char* GetHistogramNameForTesting();

 private:
  void SetLogSegment(LogSegment log_segment);

  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  // Used to track the IdentityManager that this instance is observing so that
  // this instance can be removed as an observer on its destruction.
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_observation_{this};

  // Used to track the IdentityManagerFactory instance.
  base::ScopedObservation<IdentityManagerFactory,
                          IdentityManagerFactory::Observer>
      scoped_factory_observation_{this};

  // Cache the log segment because it won't change during the session once
  // assigned.
  absl::optional<LogSegment> log_segment_;
};

#endif  // CHROME_BROWSER_METRICS_FAMILY_LINK_USER_METRICS_PROVIDER_H_

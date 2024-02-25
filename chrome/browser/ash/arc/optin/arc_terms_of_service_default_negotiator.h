// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_OPTIN_ARC_TERMS_OF_SERVICE_DEFAULT_NEGOTIATOR_H_
#define CHROME_BROWSER_ASH_ARC_OPTIN_ARC_TERMS_OF_SERVICE_DEFAULT_NEGOTIATOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/arc_support_host.h"
#include "chrome/browser/ash/arc/optin/arc_optin_preference_handler_observer.h"
#include "chrome/browser/ash/arc/optin/arc_terms_of_service_negotiator.h"

class PrefService;

namespace metrics {

class MetricsService;

}

namespace arc {

class ArcOptInPreferenceHandler;

// Handles the Terms-of-service agreement user action via default OptIn UI.
class ArcTermsOfServiceDefaultNegotiator
    : public ArcTermsOfServiceNegotiator,
      public ArcSupportHost::TermsOfServiceDelegate,
      public ArcOptInPreferenceHandlerObserver {
 public:
  ArcTermsOfServiceDefaultNegotiator(PrefService* pref_service,
                                     ArcSupportHost* support_host,
                                     metrics::MetricsService* metrics_service);

  ArcTermsOfServiceDefaultNegotiator(
      const ArcTermsOfServiceDefaultNegotiator&) = delete;
  ArcTermsOfServiceDefaultNegotiator& operator=(
      const ArcTermsOfServiceDefaultNegotiator&) = delete;

  ~ArcTermsOfServiceDefaultNegotiator() override;

 private:
  // ArcSupportHost::TermsOfServiceDelegate:
  void OnTermsAgreed(bool is_metrics_enabled,
                     bool is_backup_and_restore_enabled,
                     bool is_location_service_enabled) override;
  void OnTermsRejected() override;
  void OnTermsRetryClicked() override;
  void OnTermsLoadResult(bool success) override;

  // ArcOptInPreferenceHandlerObserver:
  void OnMetricsModeChanged(bool enabled, bool managed) override;
  void OnBackupAndRestoreModeChanged(bool enabled, bool managed) override;
  void OnLocationServicesModeChanged(bool enabled, bool managed) override;

  // Callback when metrics prefs have successfully been updated by
  // |preference_handler_|.
  void OnMetricsPrefsUpdated();

  // ArcTermsOfServiceNegotiator:
  // Shows "Terms of service" page on ARC support Chrome App.
  void StartNegotiationImpl() override;

  const raw_ptr<PrefService> pref_service_;
  // Owned by ArcSessionManager.
  const raw_ptr<ArcSupportHost> support_host_;

  const raw_ptr<metrics::MetricsService> metrics_service_;

  std::unique_ptr<ArcOptInPreferenceHandler> preference_handler_;

  base::WeakPtrFactory<ArcTermsOfServiceDefaultNegotiator> weak_ptr_factory_{
      this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_OPTIN_ARC_TERMS_OF_SERVICE_DEFAULT_NEGOTIATOR_H_

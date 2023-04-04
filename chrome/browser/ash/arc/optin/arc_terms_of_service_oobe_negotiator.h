// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_OPTIN_ARC_TERMS_OF_SERVICE_OOBE_NEGOTIATOR_H_
#define CHROME_BROWSER_ASH_ARC_OPTIN_ARC_TERMS_OF_SERVICE_OOBE_NEGOTIATOR_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ash/arc/optin/arc_terms_of_service_negotiator.h"
#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"

namespace arc {

// Handles the Terms-of-service agreement user action via OOBE OptIn UI.
class ArcTermsOfServiceOobeNegotiator
    : public ArcTermsOfServiceNegotiator,
      public ash::ConsolidatedConsentScreen::Observer {
 public:
  ArcTermsOfServiceOobeNegotiator();

  ArcTermsOfServiceOobeNegotiator(const ArcTermsOfServiceOobeNegotiator&) =
      delete;
  ArcTermsOfServiceOobeNegotiator& operator=(
      const ArcTermsOfServiceOobeNegotiator&) = delete;

  ~ArcTermsOfServiceOobeNegotiator() override;

 private:
  // Helper to handle callbacks from ash::ConsolidatedConsentScreen::Observer.
  // It resets the observer and then dispatches `accepted`. It is expected that
  // this method is called exactly once for each instance of
  // ArcTermsOfServiceOobeNegotiator.
  void HandleTermsAccepted(bool accepted);

  // ash::ConsolidatedConsentScreen::Observer:
  void OnConsolidatedConsentAccept() override;
  void OnConsolidatedConsentScreenDestroyed() override;

  // ArcTermsOfServiceNegotiator:
  void StartNegotiationImpl() override;

  base::ScopedObservation<ash::ConsolidatedConsentScreen,
                          ash::ConsolidatedConsentScreen::Observer>
      consolidated_consent_observation_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_OPTIN_ARC_TERMS_OF_SERVICE_OOBE_NEGOTIATOR_H_

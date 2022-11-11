// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_OPTIN_ARC_TERMS_OF_SERVICE_OOBE_NEGOTIATOR_H_
#define CHROME_BROWSER_ASH_ARC_OPTIN_ARC_TERMS_OF_SERVICE_OOBE_NEGOTIATOR_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ash/arc/optin/arc_terms_of_service_negotiator.h"
#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"
#include "chrome/browser/ui/webui/ash/login/arc_terms_of_service_screen_handler.h"

namespace ash {
class ArcTermsOfServiceScreenView;
}

namespace arc {

// Handles the Terms-of-service agreement user action via OOBE OptIn UI.
class ArcTermsOfServiceOobeNegotiator
    : public ArcTermsOfServiceNegotiator,
      public ash::ArcTermsOfServiceScreenViewObserver,
      public ash::ConsolidatedConsentScreen::Observer {
 public:
  ArcTermsOfServiceOobeNegotiator();

  ArcTermsOfServiceOobeNegotiator(const ArcTermsOfServiceOobeNegotiator&) =
      delete;
  ArcTermsOfServiceOobeNegotiator& operator=(
      const ArcTermsOfServiceOobeNegotiator&) = delete;

  ~ArcTermsOfServiceOobeNegotiator() override;

  // Injects ARC OOBE screen handler in unit tests, where OOBE UI is not
  // available.
  static void SetArcTermsOfServiceScreenViewForTesting(
      ash::ArcTermsOfServiceScreenView* view);

 private:
  // Helper to handle callbacks from
  // ash::ArcTermsOfServiceScreenViewObserver. It removes observer from
  // |screen_view_|, resets it, and then dispatches |accepted|. It is expected
  // that this method is called exactly once for each instance of
  // ArcTermsOfServiceOobeNegotiator.
  void HandleTermsAccepted(bool accepted);

  // ash::ArcTermsOfServiceScreenViewObserver:
  void OnAccept(bool review_arc_settings) override;
  void OnViewDestroyed(ash::ArcTermsOfServiceScreenView* view) override;

  // ash::ConsolidatedConsentScreen::Observer:
  void OnConsolidatedConsentAccept() override;
  void OnConsolidatedConsentScreenDestroyed() override;

  // ArcTermsOfServiceNegotiator:
  void StartNegotiationImpl() override;

  // Unowned pointer. If a user signs out while ARC OOBE opt-in is active,
  // LoginDisplayHost is detached first then OnViewDestroyed is called.
  // It means, in OnSkip() and OnAccept(), the View needs to be obtained via
  // LoginDisplayHost, but in OnViewDestroyed(), the argument needs to be used.
  // In order to use the same way to access the View, remember the pointer in
  // StartNegotiationImpl(), and reset in HandleTermsAccepted().
  ash::ArcTermsOfServiceScreenView* screen_view_ = nullptr;

  base::ScopedObservation<ash::ConsolidatedConsentScreen,
                          ash::ConsolidatedConsentScreen::Observer>
      consolidated_consent_observation_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_OPTIN_ARC_TERMS_OF_SERVICE_OOBE_NEGOTIATOR_H_

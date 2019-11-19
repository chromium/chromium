// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_OPTIN_ARC_TERMS_OF_SERVICE_OOBE_NEGOTIATOR_H_
#define CHROME_BROWSER_CHROMEOS_ARC_OPTIN_ARC_TERMS_OF_SERVICE_OOBE_NEGOTIATOR_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/optin/arc_terms_of_service_negotiator.h"
#include "chrome/browser/ui/webui/chromeos/login/arc_terms_of_service_screen_handler.h"

namespace chromeos {
class ArcTermsOfServiceScreenView;
}

namespace arc {

// Handles the Terms-of-service agreement user action via OOBE OptIn UI.
class ArcTermsOfServiceOobeNegotiator
    : public ArcTermsOfServiceNegotiator,
      public chromeos::ArcTermsOfServiceScreenViewObserver {
 public:
  ArcTermsOfServiceOobeNegotiator();
  ~ArcTermsOfServiceOobeNegotiator() override;

  // Injects ARC OOBE screen handler in unit tests, where OOBE UI is not
  // available.
  static void SetArcTermsOfServiceScreenViewForTesting(
      chromeos::ArcTermsOfServiceScreenView* view);

 private:
  // Helper to handle callbacks from
  // chromeos::ArcTermsOfServiceScreenViewObserver. It removes observer from
  // |screen_view_|, resets it, and then dispatches |accepted|. It is expected
  // that this method is called exactly once for each instance of
  // ArcTermsOfServiceOobeNegotiator.
  void HandleTermsAccepted(bool accepted);

  // chromeos::ArcTermsOfServiceScreenViewObserver:
  void OnSkip() override;
  void OnAccept(bool review_arc_settings) override;
  void OnViewDestroyed(chromeos::ArcTermsOfServiceScreenView* view) override;

  // ArcTermsOfServiceNegotiator:
  void StartNegotiationImpl() override;

  // Unowned pointer. If a user signs out while ARC OOBE opt-in is active,
  // LoginDisplayHost is detached first then OnViewDestroyed is called.
  // It means, in OnSkip() and OnAccept(), the View needs to be obtained via
  // LoginDisplayHost, but in OnViewDestroyed(), the argument needs to be used.
  // In order to use the same way to access the View, remember the pointer in
  // StartNegotiationImpl(), and reset in HandleTermsAccepted().
  chromeos::ArcTermsOfServiceScreenView* screen_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ArcTermsOfServiceOobeNegotiator);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_OPTIN_ARC_TERMS_OF_SERVICE_OOBE_NEGOTIATOR_H_

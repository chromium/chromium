// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_DESKTOP_H_
#define CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_DESKTOP_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/digital_credentials/digital_identity_fido_handler_observer.h"
#include "chrome/browser/ui/views/digital_credentials/digital_identity_bluetooth_manual_dialog_controller.h"
#include "chrome/browser/ui/views/digital_credentials/digital_identity_multi_step_dialog.h"
#include "content/public/browser/digital_identity_provider.h"
#include "device/fido/digital_identity_request_handler.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_request_handler_base.h"

namespace content {
class WebContents;
}

// Desktop-specific implementation of `DigitalIdentityProvider`. Uses FIDO
// hybrid flow to retrieve credentials stored on a mobile device.
class DigitalIdentityProviderDesktop : public content::DigitalIdentityProvider {
 public:
  DigitalIdentityProviderDesktop();
  ~DigitalIdentityProviderDesktop() override;

  // content::DigitalIdentityProvider:
  bool IsLowRiskOrigin(const url::Origin& to_check) const override;
  DigitalIdentityInterstitialAbortCallback ShowDigitalIdentityInterstitial(
      content::WebContents& web_contents,
      const url::Origin& origin,
      content::DigitalIdentityInterstitialType interstitial_type,
      DigitalIdentityInterstitialCallback callback) override;
  void Request(content::WebContents* web_contents,
               const url::Origin& rp_origin,
               const std::string& request,
               DigitalIdentityCallback callback) override;

 private:
  void OnReadyToShowUi(
      const device::FidoRequestHandlerBase::TransportAvailabilityInfo&
          availability_info);

  // Ensures `dialog_` is initialized and returns it.
  DigitalIdentityMultiStepDialog* EnsureDialogCreated();

  // Shows dialog with QR code.
  void ShowQrCodeDialog();

  // Shows dialog which prompts user to manually turn on bluetooth.
  void ShowBluetoothManualTurnOnDialog();

  // Called once the user has turned on bluetooth and clicked "Try Again".
  void OnBluetoothTurnedOn();

  // Called when the request has failed, possibly as a result of the user
  // canceling the dialog.
  void OnCanceled();

  // The web contents to which the dialog is modal to.
  base::WeakPtr<content::WebContents> web_contents_;

  url::Origin rp_origin_;
  std::string qr_url_;

  // Whether bluetooth is powered on.
  bool is_ble_powered_ = false;

  // Shows dialog requesting that the user manually turn on bluetooth.
  std::unique_ptr<DigitalIdentityBluetoothManualDialogController>
      bluetooth_manual_dialog_controller_;

  // Dialog which supports swapping its contents when the user goes to the next
  // step.
  std::unique_ptr<DigitalIdentityMultiStepDialog> dialog_;

  std::unique_ptr<device::FidoDiscoveryFactory> discovery_factory_;

  std::unique_ptr<device::DigitalIdentityRequestHandler> request_handler_;
  std::unique_ptr<DigitalIdentityFidoHandlerObserver> request_handler_observer_;

  DigitalIdentityCallback callback_;

  base::WeakPtrFactory<DigitalIdentityProviderDesktop> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_DESKTOP_H_

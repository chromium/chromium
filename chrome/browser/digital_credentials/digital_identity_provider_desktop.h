// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_DESKTOP_H_
#define CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_DESKTOP_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/digital_credentials/digital_identity_bluetooth_manual_dialog_controller.h"
#include "chrome/browser/ui/views/digital_credentials/digital_identity_multi_step_dialog.h"
#include "content/public/browser/digital_credentials_cross_device.h"
#include "content/public/browser/digital_identity_provider.h"

namespace content {
class WebContents;
}

namespace device::cablev2 {
enum class Event;
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
               base::Value request,
               DigitalIdentityCallback callback) override;

 private:
  // Called whenever some significant event occurs during the transaction.
  void OnEvent(const std::string& qr_url,
               content::digital_credentials::cross_device::Event);

  // Called when the transaction is finished (successfully or not).
  void OnFinished(
      base::expected<content::digital_credentials::cross_device::Response,
                     content::digital_credentials::cross_device::Error>);

  // Ensures `dialog_` is initialized and returns it.
  DigitalIdentityMultiStepDialog* EnsureDialogCreated();

  // Shows dialog with QR code.
  void ShowQrCodeDialog(const std::string& qr_url);

  // Shows dialog which prompts user to manually turn on bluetooth.
  void ShowBluetoothManualTurnOnDialog();

  // Called when the user clicks a button on the dialog requesting Bluetooth
  // power.
  void OnUserRequestedBluetoothPowerOn();

  // Called when the request has failed, possibly as a result of the user
  // canceling the dialog.
  void OnCanceled();

  // Called to end the request with an error.
  void EndRequestWithError(
      content::DigitalIdentityProvider::RequestStatusForMetrics);

  // The web contents to which the dialog is modal to.
  base::WeakPtr<content::WebContents> web_contents_;

  url::Origin rp_origin_;

  std::unique_ptr<content::digital_credentials::cross_device::Transaction>
      transaction_;

  // Shows dialog requesting that the user manually turn on bluetooth.
  std::unique_ptr<DigitalIdentityBluetoothManualDialogController>
      bluetooth_manual_dialog_controller_;

  // Dialog which supports swapping its contents when the user goes to the next
  // step.
  std::unique_ptr<DigitalIdentityMultiStepDialog> dialog_;

  DigitalIdentityCallback callback_;

  base::WeakPtrFactory<DigitalIdentityProviderDesktop> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_DESKTOP_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_DESKTOP_H_
#define CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_DESKTOP_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/digital_credentials/digital_identity_bluetooth_manual_dialog_controller.h"
#include "chrome/browser/ui/views/digital_credentials/digital_identity_multi_step_dialog.h"
#include "content/public/browser/cross_device_request_info.h"
#include "content/public/browser/digital_credentials_cross_device.h"
#include "content/public/browser/digital_identity_provider.h"

namespace content {
class WebContents;
class RenderFrameHost;
}

namespace device::cablev2 {
enum class Event;
}

// Desktop-specific implementation of `DigitalIdentityProvider`. Uses FIDO
// hybrid flow to retrieve credentials stored on a mobile device.
class DigitalIdentityProviderDesktop : public content::DigitalIdentityProvider {
 public:
  using RequestInfo = content::digital_credentials::cross_device::RequestInfo;

  DigitalIdentityProviderDesktop();
  ~DigitalIdentityProviderDesktop() override;

  // content::DigitalIdentityProvider:
  bool IsLastCommittedOriginLowRisk(
      content::RenderFrameHost& render_frame_host) const override;
  DigitalIdentityInterstitialAbortCallback ShowDigitalIdentityInterstitial(
      content::WebContents& web_contents,
      const url::Origin& origin,
      content::DigitalIdentityInterstitialType interstitial_type,
      DigitalIdentityInterstitialCallback callback) override;
  void Get(content::WebContents* web_contents,
           const url::Origin& rp_origin,
           base::ValueView request,
           DigitalIdentityCallback callback) override;
  void Create(content::WebContents* web_contents,
              const url::Origin& rp_origin,
              base::ValueView request,
              DigitalIdentityCallback callback) override;

 private:
  // Shared implementation between `Request()` and `Create()` above.
  void Transact(content::WebContents* web_contents,
                RequestInfo::RequestType request_type,
                const url::Origin& rp_origin,
                base::ValueView request,
                DigitalIdentityCallback callback);

  // Called whenever some significant event occurs during the transaction.
  void OnEvent(const std::string& qr_url,
               RequestInfo::RequestType request_type,
               content::digital_credentials::cross_device::Event);

  // caBLE events notify when the user has started the transaction on their
  // phone. This method updates the desktop UI to inform about the current state
  // or to instruct the user to complete the action on the phone.
  void OnCableEvent(device::cablev2::Event event);

  // Called when the transaction is finished (successfully or not).
  void OnFinished(
      base::expected<content::digital_credentials::cross_device::Response,
                     content::digital_credentials::cross_device::Error>);

  // Ensures `dialog_` is initialized and returns it.
  DigitalIdentityMultiStepDialog* EnsureDialogCreated();

  // Shows dialog with QR code.
  void ShowQrCodeDialog(const std::string& qr_url,
                        RequestInfo::RequestType request_type);

  // Shows dialog which prompts user to manually turn on bluetooth.
  void ShowBluetoothManualTurnOnDialog();

  // Called when the user clicks a button on the dialog requesting Bluetooth
  // power.
  void OnUserRequestedBluetoothPowerOn();

  // Called upon receiving a BLE advert from the phone which starts the
  // connection between the phone and the desktop via the tunnel service.
  void ShowConnectingToPhoneDialog();

  // Called when the tunnel connection is established in which case the user
  // should follow the instruction on the phone.
  void ShowContinueStepsOnThePhoneDialog();

  // Called when `cable_connecting_dialog_timer_` completes.
  void OnCableConnectingTimerComplete();

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

  // cable_connecting_dialog_timer_ is started when we start displaying
  // the "connecting..." dialog for a caBLE connection. To avoid flashing the
  // UI, the dialog won't be automatically replaced until this timer completes.
  base::OneShotTimer cable_connecting_dialog_timer_;

  // cable_connecting_ready_to_advance_ is set to true if we are ready to
  // advance the "connecting" dialog but are waiting for
  // `cable_connecting_dialog_timer_` to complete.
  bool cable_connecting_ready_to_advance_ = false;

  base::WeakPtrFactory<DigitalIdentityProviderDesktop> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_PROVIDER_DESKTOP_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_FIDO_HANDLER_OBSERVER_H_
#define CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_FIDO_HANDLER_OBSERVER_H_

#include "base/observer_list.h"
#include "chrome/browser/digital_credentials/digital_identity_bluetooth_adapter_status_change_observer.h"
#include "chrome/browser/ui/browser_commands.h"
#include "device/fido/fido_request_handler_base.h"

// Registers observers for bluetooth powered-on state.
class DigitalIdentityFidoHandlerObserver
    : public device::FidoRequestHandlerBase::Observer {
 public:
  using ReadyToShowUiCallback = base::OnceCallback<void(
      const device::FidoRequestHandlerBase::TransportAvailabilityInfo&)>;

  explicit DigitalIdentityFidoHandlerObserver(
      ReadyToShowUiCallback ready_to_show_ui_callback);
  ~DigitalIdentityFidoHandlerObserver() override;

  DigitalIdentityFidoHandlerObserver(
      const DigitalIdentityFidoHandlerObserver&) = delete;
  DigitalIdentityFidoHandlerObserver& operator=(
      const DigitalIdentityFidoHandlerObserver&) = delete;

  void AddBluetoothAdapterStatusChangeObserver(
      DigitalIdentityBluetoothAdapterStatusChangeObserver* observer);
  void RemoveBluetoothAdapterStatusChangeObserver(
      DigitalIdentityBluetoothAdapterStatusChangeObserver* observer);

  // device::FidoRequestHandlerBase::Observer:
  void OnTransportAvailabilityEnumerated(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo data) override;
  bool EmbedderControlsAuthenticatorDispatch(
      const device::FidoAuthenticator& authenticator) override;
  void BluetoothAdapterStatusChanged(
      device::FidoRequestHandlerBase::BleStatus ble_status) override;
  void FidoAuthenticatorAdded(
      const device::FidoAuthenticator& authenticator) override;
  void FidoAuthenticatorRemoved(std::string_view device_id) override;
  bool SupportsPIN() const override;
  void CollectPIN(
      CollectPINOptions options,
      base::OnceCallback<void(std::u16string)> provide_pin_cb) override;
  void FinishCollectToken() override;
  void StartBioEnrollment(base::OnceClosure next_callback) override;
  void OnSampleCollected(int bio_samples_remaining) override;
  void OnRetryUserVerification(int attempts) override;

 private:
  ReadyToShowUiCallback ready_to_show_ui_callback_;
  base::ObserverList<DigitalIdentityBluetoothAdapterStatusChangeObserver>
      bluetooth_observers_;
};

#endif  // CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_FIDO_HANDLER_OBSERVER_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/digital_credentials/digital_identity_fido_handler_observer.h"

#include "base/notreached.h"

using BleStatus = device::FidoRequestHandlerBase::BleStatus;

DigitalIdentityFidoHandlerObserver::DigitalIdentityFidoHandlerObserver(
    ReadyToShowUiCallback ready_to_show_ui_callback)
    : ready_to_show_ui_callback_(std::move(ready_to_show_ui_callback)) {}

DigitalIdentityFidoHandlerObserver::~DigitalIdentityFidoHandlerObserver() =
    default;

void DigitalIdentityFidoHandlerObserver::
    AddBluetoothAdapterStatusChangeObserver(
        DigitalIdentityBluetoothAdapterStatusChangeObserver* observer) {
  bluetooth_observers_.AddObserver(observer);
}

void DigitalIdentityFidoHandlerObserver::
    RemoveBluetoothAdapterStatusChangeObserver(
        DigitalIdentityBluetoothAdapterStatusChangeObserver* observer) {
  bluetooth_observers_.RemoveObserver(observer);
}

void DigitalIdentityFidoHandlerObserver::OnTransportAvailabilityEnumerated(
    device::FidoRequestHandlerBase::TransportAvailabilityInfo data) {
  if (!ready_to_show_ui_callback_) {
    return;
  }
  std::move(ready_to_show_ui_callback_).Run(data);
}

bool DigitalIdentityFidoHandlerObserver::EmbedderControlsAuthenticatorDispatch(
    const device::FidoAuthenticator& authenticator) {
  return false;
}

void DigitalIdentityFidoHandlerObserver::BluetoothAdapterStatusChanged(
    BleStatus ble_status) {
  for (auto& observer : bluetooth_observers_) {
    observer.OnBluetoothAdapterStatusChanged(ble_status);
  }
}

void DigitalIdentityFidoHandlerObserver::FidoAuthenticatorAdded(
    const device::FidoAuthenticator& authenticator) {}

void DigitalIdentityFidoHandlerObserver::FidoAuthenticatorRemoved(
    std::string_view device_id) {}

bool DigitalIdentityFidoHandlerObserver::SupportsPIN() const {
  return false;
}

void DigitalIdentityFidoHandlerObserver::CollectPIN(
    CollectPINOptions options,
    base::OnceCallback<void(std::u16string)> provide_pin_cb) {
  NOTREACHED();
}

void DigitalIdentityFidoHandlerObserver::FinishCollectToken() {}

void DigitalIdentityFidoHandlerObserver::StartBioEnrollment(
    base::OnceClosure next_callback) {}

void DigitalIdentityFidoHandlerObserver::OnSampleCollected(
    int bio_samples_remaining) {}

void DigitalIdentityFidoHandlerObserver::OnRetryUserVerification(int attempts) {
}

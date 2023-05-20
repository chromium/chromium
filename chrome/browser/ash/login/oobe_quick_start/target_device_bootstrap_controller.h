// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_TARGET_DEVICE_BOOTSTRAP_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_TARGET_DEVICE_BOOTSTRAP_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace ash::quick_start {

class TargetDeviceBootstrapController
    : public TargetDeviceConnectionBroker::ConnectionLifecycleListener {
 public:
  explicit TargetDeviceBootstrapController(
      std::unique_ptr<TargetDeviceConnectionBroker>
          target_device_connection_broker);
  TargetDeviceBootstrapController(TargetDeviceBootstrapController&) = delete;
  TargetDeviceBootstrapController& operator=(TargetDeviceBootstrapController&) =
      delete;
  ~TargetDeviceBootstrapController() override;

  enum class Step {
    NONE,
    ERROR,
    ADVERTISING,
    QR_CODE_VERIFICATION,
    PIN_VERIFICATION,
    CONNECTED,
    GAIA_CREDENTIALS,
    CONNECTING_TO_WIFI,
    CONNECTED_TO_WIFI,
    TRANSFERRING_GOOGLE_ACCOUNT_DETAILS,
    TRANSFERRED_GOOGLE_ACCOUNT_DETAILS,
  };

  enum class ErrorCode {
    START_ADVERTISING_FAILED,
    CONNECTION_REJECTED,
    CONNECTION_CLOSED,
    WIFI_CREDENTIALS_NOT_RECEIVED,
    USER_VERIFICATION_FAILED,
    GAIA_ASSERTION_NOT_RECEIVED,
  };

  using QRCodePixelData = std::vector<uint8_t>;

  using Payload = absl::variant<absl::monostate, ErrorCode, QRCodePixelData>;

  struct Status {
    Status();
    ~Status();
    Step step = Step::NONE;
    Payload payload;
    std::string ssid;
    std::string password;
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnStatusChanged(const Status& status) = 0;

   protected:
    ~Observer() override = default;
  };

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  void GetFeatureSupportStatusAsync(
      TargetDeviceConnectionBroker::FeatureSupportStatusCallback callback);

  // Returns the CryptAuth Instance ID of the connected remote device. Returns
  // an empty string if no ID is available.
  std::string GetPhoneInstanceId();

  // This function would crash (if DCHECKs are on) in case there are existing
  // valid weakptrs.
  base::WeakPtr<TargetDeviceBootstrapController> GetAsWeakPtrForClient();

  // TODO: Finalize api for frontend.
  void StartAdvertising();
  void StopAdvertising();

  // A user may initiate Quick Start then have to download an update and reboot.
  // This function persists necessary data and notifies the source device so
  // Quick Start can resume where it left off after the reboot.
  void PrepareForUpdate();

  // TargetDeviceConnectionBroker::ConnectionLifecycleListener:
  void OnPinVerificationRequested(const std::string& pin) override;
  void OnQRCodeVerificationRequested(
      const std::vector<uint8_t>& qr_code_data) override;
  void OnConnectionAuthenticated(
      base::WeakPtr<TargetDeviceConnectionBroker::AuthenticatedConnection>
          authenticated_connection) override;
  void OnConnectionRejected() override;
  void OnConnectionClosed(
      TargetDeviceConnectionBroker::ConnectionClosedReason reason) override;

  void AttemptWifiCredentialTransfer();
  void AttemptGoogleAccountTransfer();

 private:
  friend class TargetDeviceBootstrapControllerTest;

  void NotifyObservers();
  void OnStartAdvertisingResult(bool success);
  void OnStopAdvertising();

  void WaitForUserVerification(base::OnceClosure on_verification);
  void OnUserVerificationResult(base::OnceClosure on_verification,
                                absl::optional<mojom::UserVerificationResponse>
                                    user_verification_response);

  // If the target device successfully receives an ack message, it prepares to
  // automatically resume Quick Start after the update and closes the
  // connection. If ack_successful is 'false', it closes the connection without
  // preparing to automatically resume Quick Start after the update.
  void OnNotifySourceOfUpdateResponse(bool ack_successful);

  void OnWifiCredentialsReceived(
      absl::optional<mojom::WifiCredentials> credentials);
  void OnFidoAssertionReceived(absl::optional<FidoAssertionInfo> assertion);

  std::unique_ptr<TargetDeviceConnectionBroker> connection_broker_;

  std::string pin_;
  // TODO: Should we enforce one observer at a time here too?
  base::ObserverList<Observer> observers_;

  Status status_;

  base::WeakPtr<TargetDeviceConnectionBroker::AuthenticatedConnection>
      authenticated_connection_;

  int32_t session_id_;

  base::WeakPtrFactory<TargetDeviceBootstrapController>
      weak_ptr_factory_for_clients_{this};

  base::WeakPtrFactory<TargetDeviceBootstrapController> weak_ptr_factory_{this};
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_TARGET_DEVICE_BOOTSTRAP_CONTROLLER_H_

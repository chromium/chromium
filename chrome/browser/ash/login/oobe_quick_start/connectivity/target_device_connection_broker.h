// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/session_context.h"
#include "chromeos/ash/components/quick_start/types.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"

namespace ash::quick_start {

struct FidoAssertionInfo;

// TargetDeviceConnectionBroker is the entrypoint for consuming the Quick Start
// connectivity component. Calling code is expected to get an instance of this
// class using the TargetDeviceConnectionBrokerFactory and interact with the
// component using the public interface of this class.
//
// All references to "target device" imply this device (Chromebook). All
// references to "source device" imply the remote Android phone, which is the
// source for Gaia and WiFi credentials.
class TargetDeviceConnectionBroker {
 public:
  using ResultCallback = base::OnceCallback<void(bool success)>;
  using SharedSecret = SessionContext::SharedSecret;

  enum class FeatureSupportStatus {
    kUndetermined = 0,
    kNotSupported,
    kSupported,
    kWaitingForAdapterToBecomePresent,  // When resuming after an update, the
                                        // bluetooth adapter may not be present
                                        // and powered immediately upon reboot
                                        // when we initiate advertising.
    kWaitingForAdapterToBecomePowered
  };

  enum class ConnectionClosedReason {
    kUserAborted,  // Based on user selections on target device, which are
                   // always informed by Chromebook UI.
    kAuthenticationFailed,
    kTargetDeviceUpdate,
    kResponseTimeout,
    kUnknownError,
    kConnectionLifecycleListenerDestroyed,
  };

  class AuthenticatedConnection {
   public:
    using RequestWifiCredentialsCallback =
        base::OnceCallback<void(std::optional<mojom::WifiCredentials>)>;
    // The ack_successful bool indicates whether the ack was successfully
    // received by the source device. If true, then the target device will
    // prepare to resume the Quick Start connection after it updates.
    using NotifySourceOfUpdateCallback =
        base::OnceCallback<void(/*ack_successful=*/bool)>;
    using RequestAccountTransferAssertionCallback =
        base::OnceCallback<void(std::optional<FidoAssertionInfo>)>;
    using AwaitUserVerificationCallback = base::OnceCallback<void(
        std::optional<mojom::UserVerificationResponse>)>;
    using RequestAccountInfoCallback =
        base::OnceCallback<void(/*account_email=*/std::string)>;

    // Close the connection.
    virtual void Close(
        TargetDeviceConnectionBroker::ConnectionClosedReason reason) = 0;

    // Request wifi credentials from target Android device.
    virtual void RequestWifiCredentials(
        RequestWifiCredentialsCallback callback) = 0;

    // Notify Android device that the Chromebook will download an update and
    // reboot.
    virtual void NotifySourceOfUpdate(
        NotifySourceOfUpdateCallback callback) = 0;

    // The first step in the account transfer process which involves retrieving
    // GAIA account info from the source device.
    virtual void RequestAccountInfo(RequestAccountInfoCallback callback) = 0;

    // Begin the account transfer process and retrieve an Assertion from the
    // source device. The caller must provide a "challenge" nonce to be sent to
    // the remote source device.
    virtual void RequestAccountTransferAssertion(
        const Base64UrlString& challenge,
        RequestAccountTransferAssertionCallback callback) = 0;

    // Wait for the user to perform verification, and return if it succeeded
    virtual void WaitForUserVerification(
        AwaitUserVerificationCallback callback) = 0;

    // Exposes SessionContext::GetPrepareForUpdateInfo() to the
    // AuthenticatedConnection caller.
    virtual base::Value::Dict GetPrepareForUpdateInfo() = 0;

    virtual void NotifyPhoneSetupComplete() = 0;

    // Retrieve Instance ID (CryptAuth device ID) from BootstrapConfigurations
    // response.
    std::string get_phone_instance_id() { return phone_instance_id_; }

    // Retrieve boolean value indicating whether the account in question is a
    // supervised account (e.g. Unicorn).
    bool is_supervised_account() { return is_supervised_account_; }

   protected:
    AuthenticatedConnection() = default;
    virtual ~AuthenticatedConnection() = default;

    std::string phone_instance_id_;
    bool is_supervised_account_;
  };

  // Clients of TargetDeviceConnectionBroker should implement this interface,
  // and provide a self-reference when calling TargetDeviceConnectionBroker::
  // StartAdvertising().
  //
  // This interface is a simplification of
  // nearby::connections::mojom::ConnectionLifecycleListener, for ease
  // of client use.
  class ConnectionLifecycleListener {
   public:
    ConnectionLifecycleListener() = default;
    virtual ~ConnectionLifecycleListener() = default;

    // A connection has been initiated between this target device and the remote
    // source device, but needs to be authenticated before messages can be
    // exchanged. The source device has requested that the pin be displayed so
    // that the user can check that the codes match, thereby authenticating the
    // connection.
    virtual void OnPinVerificationRequested(const std::string& pin) = 0;

    // Called after both sides have accepted the connection.
    //
    // This connection may be a "resumed" connection that was previously
    // "paused" before this target device performed a Critical Update and
    // rebooted.
    //
    // The AuthenticatedConnection pointer may be cached, but will become
    // invalid after OnConnectionClosed() is called.
    //
    // Use source_device_id to understand which connection
    // OnConnectionClosed() refers to.
    virtual void OnConnectionAuthenticated(
        base::WeakPtr<AuthenticatedConnection> authenticated_connection) = 0;

    // Called if the source device rejected the connection.
    virtual void OnConnectionRejected() = 0;

    // Called when the source device is disconnected or has become unreachable.
    virtual void OnConnectionClosed(ConnectionClosedReason reason) = 0;
  };

  TargetDeviceConnectionBroker();
  virtual ~TargetDeviceConnectionBroker();

  // Checks to see whether the feature can be supported on the device's
  // hardware. The feature is supported if Bluetooth is supported and an adapter
  // is present.
  virtual FeatureSupportStatus GetFeatureSupportStatus() const = 0;

  using FeatureSupportStatusCallback =
      base::RepeatingCallback<void(FeatureSupportStatus status)>;
  void GetFeatureSupportStatusAsync(FeatureSupportStatusCallback callback);

  // Will kick off Fast Pair and Nearby Connections advertising.
  // Clients can use the result of |on_start_advertising_callback| to
  // immediately understand if advertising succeeded, and can then wait for the
  // source device to connect and request authentication via
  // |ConnectionLifecycleListener::OnPinVerificationRequested()| or
  // |ConnectionLifecycleListener::OnQRCodeVerificationRequested()|.
  //
  // If the caller paused a connection previously, the connection to the
  // source device will resume via OnConnectionAuthenticated().
  // Clients should check  GetFeatureSupportStatus()  before calling
  // StartAdvertising().
  //
  // If the target device is attempting to resume a Quick Start connection after
  // an update, it skips the Fast Pair advertising step and automatically
  // begins Nearby Connections advertising. Since the source device "remembers"
  // the target device, we don't need to require manual user confirmation with
  // the Fast Pair half-sheet.
  //
  // If |use_pin_authentication| is true, then the target device will
  // advertise its preference to use pin authentication instead of QR code
  // authentication. This should be false unless the user would benefit from
  // using pin for, e.g. accessibility reasons.
  virtual void StartAdvertising(
      ConnectionLifecycleListener* listener,
      bool use_pin_authentication,
      ResultCallback on_start_advertising_callback) = 0;

  // Clients are responsible for calling this once they have accepted their
  // desired connection, or in error/edge cases, e.g., the user exits the UI.
  virtual void StopAdvertising(
      base::OnceClosure on_stop_advertising_callback) = 0;

  // Gets the 3 digits of the discoverable name. e.g.: Chromebook (123)
  virtual std::string GetAdvertisingIdDisplayCode() = 0;

 protected:
  void MaybeNotifyFeatureStatus();
  void OnConnectionAuthenticated(
      base::WeakPtr<AuthenticatedConnection> authenticated_connection);

  void OnConnectionClosed(ConnectionClosedReason reason);

  // Derive a 4-digit decimal pin code from the authentication token. This is
  // meant to match the Android implementation found here:
  // http://google3/java/com/google/android/gmscore/integ/modules/smartdevice/src/com/google/android/gms/smartdevice/d2d/nearby/advertisement/VerificationUtils.java;l=37;rcl=511361463
  // Since the PIN is derived from the auth token, this PIN cannot be calculated
  // until the connection is initiated between this target device and the remote
  // source device.
  std::string DerivePin(const std::string& authentication_token) const;

  // Determines whether the advertisement info sent to the source device will
  // request pin verification or QR code verification.
  bool use_pin_authentication_ = false;

  raw_ptr<ConnectionLifecycleListener> connection_lifecycle_listener_ = nullptr;

 private:
  std::vector<FeatureSupportStatusCallback> feature_status_callbacks_;
};

std::ostream& operator<<(
    std::ostream& stream,
    const TargetDeviceConnectionBroker::ConnectionClosedReason&
        connection_closed_reason);

std::ostream& operator<<(
    std::ostream& stream,
    const TargetDeviceConnectionBroker::FeatureSupportStatus&
        feature_support_status);

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_H_
